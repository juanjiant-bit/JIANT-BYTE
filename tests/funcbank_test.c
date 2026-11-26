/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (funcbank) FUNC's BANK (eng_lofi.c FUNC_BANK, funcplay.c func_bank_run): FORM 3's formula bank as a recipe. Every
 * formula of it compiles (a state at most, in its slot's 32 instructions: B's, wave_prg_b) and sounds over SCL >
 * HARMONY (50S, a chord a bar): not silent, never clipping, at its level (FB_LV: within 2 dB of the bank's middle); the
 * pitched ones in tune with the chord (most of their energy on its three tones); the chord follows HARMONY (its root,
 * third and fifth as it changes, LENGTH's bars; stopped, the key's tonic chord in the part's SCALE; EXTENS); MORPH
 * turned while it plays reaches the sound over blocks, never a step; a FORM's change faded; the packed texts (fvm_bank.c
 * FVM_BPE) expand to the bank as written. With "report": a line each (level, in-tune share, brightness) */
#define UI_TEST_NO_MAIN 1
static int fb_peak;                                     /* (funcplay.c func_bank_run: the formulas' mix's peak, Q16) */
#define FUNCBANK_PEAK fb_peak
#include "ui_test.c"
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static void setup(uint32_t fa, uint32_t fb, uint32_t morph, uint32_t def)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    for (uint32_t k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, FUNC_BANK - 1u); song.master_q12 = 4096;
    TSEL->p[P_DLY] = TSEL->p[P_REV] = TSEL->p[P_CHOR] = 0;
    TSEL->p[P_E2] = (int16_t)fa; TSEL->p[P_E3] = (int16_t)fb; TSEL->p[P_E4] = (int16_t)morph; TSEL->p[P_E5] = (int16_t)def;
    song.g[G_BPM] = 120; song.g[G_HPROG] = 14; song.g[G_HLEN] = 2;   /* (50S: I vi IV V, a chord a bar) */
    math_poll(); frame(); math_poll();
    memset(fp_st, 0, sizeof fp_st);
    int32_t x[CTL * 2]; uint32_t j;
    for (j = 0; j < FS / 8u / CTL; j++) mix_block(x, CTL);
    rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
    ap_rng = 0x2545F491u; pfx_seed = 0x9E3779B9u; mns_clk = 0; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0;
    memset(TSEL->v, 0, sizeof TSEL->v); TSEL->lfo_ph = 0; TSEL->lfo_val = TSEL->lfo_fade = 0; fx_trip(0, CTL); memset(&fx, 0, sizeof fx);
    memset(fn_sg, 0, sizeof fn_sg);
}
#define SEC 8u
static int32_t mono[FS * SEC];
static uint8_t cpc[FS * SEC / CTL];                     /* each block's chord: its root's pitch class, third, fifth */
static int8_t cth[FS * SEC / CTL], cfi[FS * SEC / CTL];
/* the mix of SEC seconds playing (its first half second let go): mono[], the chord each block */
static uint32_t play(void)
{
    int32_t x[CTL * 2]; uint32_t j, i, n = FS * SEC / CTL, at = 0;
    transport_req = 1;
    for (j = 0; j < FS / 2u / CTL; j++) mix_block(x, CTL);
    for (j = 0; j < n; j++) {
        int32_t th, fi, dg, r = func_bank_chord(TSEL, 60, &th, &fi, &dg);
        mix_block(x, CTL);
        cpc[j] = (uint8_t)(r % 12); cth[j] = (int8_t)th; cfi[j] = (int8_t)fi;
        for (i = 0; i < CTL; i++) mono[at++] = x[2u * i] + x[2u * i + 1u];
    }
    transport_req = 2;
    for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
    return at;
}
/* BS.1770's K-weighted loudness of mono[] (its 400 ms blocks gated, -70 and 10 dB under) */
static double lufs(uint32_t n)
{
    static const double B1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285}, A1[2] = {-1.69065929318241, 0.73248077421585},
                        A2[2] = {-1.99004745483398, 0.99007225036621};
    static double kms[FS * SEC / (FS / 10u) + 1u];
    double q[4] = {0}, acc = 0, sum = 0, g, tot = 0;
    uint32_t i, nb = 0, k, used = 0, lb = FS / 10u;
    for (i = 0; i < n; i++) {
        double v = mono[i] / 65536.0;
        double y1 = B1[0] * v + q[0]; q[0] = B1[1] * v - A1[0] * y1 + q[1]; q[1] = B1[2] * v - A1[1] * y1;
        double y2 = y1 + q[2]; q[2] = -2.0 * y1 - A2[0] * y2 + q[3]; q[3] = y1 - A2[1] * y2;
        acc += 2 * y2 * y2;
        if ((i + 1u) % lb == 0u) { kms[nb++] = acc / lb; acc = 0; }
    }
    for (k = 0; k + 3u < nb; k++) { double m = (kms[k] + kms[k + 1] + kms[k + 2] + kms[k + 3]) / 4; if (-0.691 + 10 * log10(m + 1e-12) > -70) { sum += m; used++; } }
    g = -0.691 + 10 * log10(sum / (used ? used : 1u) + 1e-12) - 10;
    for (used = 0, k = 0; k + 3u < nb; k++) {
        double m = (kms[k] + kms[k + 1] + kms[k + 2] + kms[k + 3]) / 4, l = -0.691 + 10 * log10(m + 1e-12);
        if (l > -70 && l > g) { tot += m; used++; }
    }
    return -0.691 + 10 * log10(tot / (used ? used : 1u) + 1e-12);
}
/* how a chord is in tune: over each chord's middle (FFT_N samples, Hann), of the spectrum's energy from 50 to 700 Hz
 * the share within 4 Hz of a harmonic of the chord's root, third or fifth (by chance about a third) */
#define FFT_N 16384u
static double fre[FFT_N], fim[FFT_N];
static void fft(void)
{
    uint32_t i, j = 0, len, k;
    for (i = 1; i < FFT_N; i++) {
        uint32_t bit = FFT_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = fre[i]; fre[i] = fre[j]; fre[j] = t; t = fim[i]; fim[i] = fim[j]; fim[j] = t; }
    }
    for (len = 2; len <= FFT_N; len <<= 1) {
        double a = -2 * M_PI / len, wr = cos(a), wi = sin(a);
        for (i = 0; i < FFT_N; i += len) {
            double cr = 1, ci = 0;
            for (k = 0; k < len / 2u; k++) {
                uint32_t p = i + k, q = p + len / 2u;
                double tr = fre[q] * cr - fim[q] * ci, ti = fre[q] * ci + fim[q] * cr, nr;
                fre[q] = fre[p] - tr; fim[q] = fim[p] - ti; fre[p] += tr; fim[p] += ti;
                nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
static double harm(uint32_t n)
{
    uint32_t s, e0, k, i;
    double on = 0, all = 0, df = (double)FS / FFT_N;
    for (s = 0; s < n; s = e0) {
        uint32_t b0 = s / CTL;
        for (e0 = s; e0 < n && cpc[e0 / CTL] == cpc[b0] && cth[e0 / CTL] == cth[b0]; e0 += CTL) {}
        if (e0 - s < FFT_N + FS / 8u) continue;         /* (a chord heard long enough) */
        uint32_t s0 = s + (e0 - s - FFT_N) / 2u;
        double f0[3];
        for (i = 0; i < FFT_N; i++) { fre[i] = mono[s0 + i] * (0.5 - 0.5 * cos(2 * M_PI * i / FFT_N)); fim[i] = 0; }
        fft();
        f0[0] = 440.0 * pow(2, (36 + cpc[b0] - 69) / 12.0); f0[1] = f0[0] * pow(2, cth[b0] / 12.0); f0[2] = f0[0] * pow(2, cfi[b0] / 12.0);
        for (k = (uint32_t)(50 / df); k < (uint32_t)(700 / df); k++) {
            double f = k * df, e = fre[k] * fre[k] + fim[k] * fim[k], m = 1e9;
            for (uint32_t j = 0; j < 3u; j++) { double h = f / f0[j], d = fabs(h - floor(h + 0.5)) * f0[j]; if (h > 0.5 && d < m) m = d; }
            all += e;
            if (m < 4.0) on += e;
        }
    }
    return all > 0 ? on / all : 0;
}
/* how in tune: over each chord's middle 4096 samples, the sound's period (its normalized autocorrelation's first peak
 * within 0.9 of its highest, 50 .. 700 Hz) and how clearly it repeats (that peak, 0 .. 1); *cents: the worst distance
 * of that pitch from the chord's tones (any octave), *clar the least clear; the share of chords heard within 35 cents */
#define AC_N 4096u
static double intune(uint32_t n, double *cents, double *clar)
{
    uint32_t s, e0, i, tau, t0 = FS / 700u, t1 = FS / 50u, seg = 0, ok = 0;
    static double r[FS / 50u + 1u];
    *cents = 0; *clar = 1;
    for (s = 0; s < n; s = e0) {
        uint32_t b0 = s / CTL;
        for (e0 = s; e0 < n && cpc[e0 / CTL] == cpc[b0] && cth[e0 / CTL] == cth[b0]; e0 += CTL) {}
        if (e0 - s < AC_N + t1 + FS / 8u) continue;
        uint32_t s0 = s + (e0 - s - AC_N - t1) / 2u;
        double best = 0, e1 = 0, f, c, worst = 1e9;
        for (i = 0; i < AC_N; i++) e1 += (double)mono[s0 + i] * mono[s0 + i];
        for (tau = t0; tau <= t1; tau++) {
            double xy = 0, e2 = 0;
            for (i = 0; i < AC_N; i++) { double y = mono[s0 + i + tau]; xy += mono[s0 + i] * y; e2 += y * y; }
            r[tau] = e1 > 0 && e2 > 0 ? xy / sqrt(e1 * e2) : 0;
            if (r[tau] > best) best = r[tau];
        }
        for (tau = t0 + 1u; tau < t1; tau++) if (r[tau] >= 0.9 * best && r[tau] >= r[tau - 1] && r[tau] >= r[tau + 1]) break;
        f = (double)FS / tau;
        int tone[3] = {0, cth[b0], cfi[b0]};
        for (i = 0; i < 3u; i++) {
            double ref = 440.0 * pow(2, (36 + cpc[b0] + tone[i] - 69) / 12.0);
            c = fmod(fabs(1200 * log2(f / ref)) + 600, 1200) - 600;
            c = fabs(c);
            if (c < worst) worst = c;
        }
        seg++; ok += worst <= 35;
        if (worst > *cents) *cents = worst;
        if (r[tau] < *clar) *clar = r[tau];
    }
    return seg ? (double)ok / seg : 0;
}
static int32_t peak(uint32_t n, uint32_t *clip)
{
    int32_t p = 0; uint32_t i;
    *clip = 0;
    for (i = 0; i < n; i++) { int32_t v = mono[i] < 0 ? -mono[i] : mono[i]; if (v > p) p = v; *clip += v > 64000; }
    return p;
}
/* ---- MORPH: turned while it plays, the difference from as it was grows from 0 (its first three samples a small
 * share of where it goes; funcsmooth's) */
#define NBK 24u
static int32_t *shb, base[NBK * CTL / 2];
static void render(uint32_t nb, int32_t *o)
{
    int32_t x[CTL * 2]; uint32_t j, i;
    for (j = 0; j < nb; j++) { mix_block(x, CTL); if (o) for (i = 0; i < CTL / 2u; i++) o[j * CTL / 2u + i] = x[4u * i] + x[4u * i + 1u]; }
}
static double trial(int16_t to)
{
    uint32_t i, nn = NBK * CTL / 2u;
    pid_t pid = fork();
    if (!pid) { TSEL->p[P_E4] = to; render(NBK, shb); _exit(0); }
    waitpid(pid, 0, 0);
    render(NBK, base);
    double x = 0, m = 0;
    for (i = 0; i < nn; i++) { double d = fabs((double)shb[i] - base[i]); if (i < 3u && d > x) x = d; if (d > m) m = d; }
    return m > 0 ? x / m : -1;
}
static const char *fb_name(uint32_t f) { return str_nth(FB_NAMES, f); }
/* each formula's tuning, checked as: C a chord (its partials the chord's tones' harmonics), R a line (its period a
 * chord tone's), - a drum or a noise (none) */
static const char KIND[] = "CCCCCCCCCCC" "RRRRRRRR-" "RRRRRRRRR" "CR" "---------" "RRRRR";
_Static_assert(sizeof KIND - 1u == FB_N, "funcbank: a kind each");
int main(int argc, char **argv)
{
    uint32_t f, n, k, rep = argc > 1 && !strcmp(argv[1], "report");
    double lv[FB_N], mid;
    setvbuf(stdout, 0, _IONBF, 0);
    shb = mmap(0, sizeof base, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shb != MAP_FAILED);
    if (argc > 2 && !strcmp(argv[1], "probe")) {         /* (probe F: formula F's strongest frequency, the key's C chord) */
        double best = 0, bf = 0, fq;
        f = (uint32_t)atoi(argv[2]);
        setup(f, f, 0, 64); song.g[G_HPROG] = 0; hm_load();
        n = play();
        for (fq = 30; fq < 700; fq += 0.25) {
            double c = 2 * cos(2 * M_PI * fq / FS), q0, q1 = 0, q2 = 0, e;
            for (uint32_t i = 0; i < FS * 2u; i++) { q0 = mono[i] + c * q1 - q2; q2 = q1; q1 = q0; }
            e = q1 * q1 + q2 * q2 - c * q1 * q2;
            if (e > best) { best = e; bf = fq; }
        }
        printf("%s: %.2f Hz (C2 65.41, C3 130.81), VH_SH %u\n", fb_name(f), bf, (unsigned)vh_sh_c[0]);
        return 0;
    }
    /* 1 the packed texts: every one as the generator wrote it compiles, a state at most, 32 instructions */
    {
        static gq_work_t w;
        char tx[256];
        for (f = 0; f < 2u * FVM_NBYTE + FB_N; f++) {
            n = fvm_bank_to(f, tx);
            assert(n == strlen(tx) && n > 4u && n < 256u);
            for (k = 0; k < n; k++) assert(tx[k] >= 32 && tx[k] < 127);
            if (f < 2u * FVM_NBYTE) { assert(!strcmp(tx, fvm_bank(f >= FVM_NBYTE, f % FVM_NBYTE))); continue; }
            fvm_err_t e;
            if (fvm_compile(&w.cc, FVM_FULL(&w.pg), tx, n, &e)) { printf("BANK %s: error %u at %u: %s\n", fb_name(f - 2u * FVM_NBYTE), e.code, e.pos, tx); return 1; }
            if (rep) printf("%-10s %2u instructions, %u state\n", fb_name(f - 2u * FVM_NBYTE), w.pg.p.n, w.pg.p.nstate);
            if (!rep) assert(w.pg.p.n <= 32u && w.pg.p.nstate <= 1u && w.pg.p.ty == FT_FIX);
        }
    }
    puts("funcbank: the bank's texts unpacked as written, each compiling in 32 instructions, a state at most PASS");
    /* 2 each formula over the progression: sounding, not clipping, at its level; the pitched ones in tune */
    for (f = 0; f < FB_N; f++) {
        uint32_t clip; double ce, cl, it, hm;
        setup(f, f, 0, 64);
        assert(func_form(TSEL) == FUNC_BANK && TSEL->p[P_E2] == (int)f);
        fb_peak = 0;
        n = play();
        int32_t pk = peak(n, &clip);
        lv[f] = lufs(n);
        it = intune(n, &ce, &cl);
        hm = harm(n);
        if (rep) printf("%-10s %6.1f LUFS  peak %5.1f dB  %c line %3.0f%% (worst %3.0f ct, clear %.2f) chord %3.0f%%  formula peak %.2f\n", fb_name(f), lv[f],
                        20 * log10(pk / 65536.0 + 1e-9), KIND[f], it * 100, ce, cl, hm * 100, fb_peak / 65536.0);
        if (!rep) {
            if (pk < 2000 || clip > n / 1000u) { printf("BANK %s: peak %d, %u clipped\n", fb_name(f), pk, clip); return 1; }
            if ((KIND[f] == 'C' && hm < 0.65) || (KIND[f] == 'R' && it < 0.75)) {
                printf("BANK %s: out of tune (chord %.0f%%, line %.0f%%)\n", fb_name(f), hm * 100, it * 100); return 1; }
            if (fb_peak > 65536) { printf("BANK %s: the formulas' mix past full scale (%.2f)\n", fb_name(f), fb_peak / 65536.0); return 1; }
        }
    }
    {
        double s[FB_N];
        memcpy(s, lv, sizeof s);
        for (f = 0; f < FB_N; f++) for (k = f + 1u; k < FB_N; k++) if (s[k] < s[f]) { double z = s[f]; s[f] = s[k]; s[k] = z; }
        mid = s[FB_N / 2u];
        for (f = 0; f < FB_N; f++) {
            if (rep) printf("LV %-10s %+.1f dB -> %d\n", fb_name(f), lv[f] - mid, (int)lrint(FB_LV[f] * pow(10, (mid - lv[f]) / 20)));
            else if (fabs(lv[f] - mid) > 2.0) { printf("BANK %s: %+.1f dB from the bank's middle\n", fb_name(f), lv[f] - mid); return 1; }
        }
    }
    if (rep) return 0;
    puts("funcbank: every formula sounding over the progression, none clipping, within 2 dB, the pitched in tune PASS");
    /* 3 the chord follows HARMONY: 50S in C (I vi IV V), a bar each, then 4 BAR; stopped, the key's tonic in its SCALE */
    {
        static const int8_t AX[4] = {0, 9, 5, 7}, TH[4] = {4, 3, 4, 4};
        int32_t th, fi, dg, r, x[CTL * 2];
        uint32_t j, bar;
        setup(0, 0, 0, 64);
        transport_req = 1;
        for (bar = 0; bar < 8u; bar++) {
            for (j = 0; j < FS * 2u / CTL / 2u; j++) mix_block(x, CTL);   /* (into the bar's middle) */
            r = func_bank_chord(TSEL, 60, &th, &fi, &dg);
            if (r % 12 != AX[bar % 4u] || th != TH[bar % 4u]) printf("bar %u: root %d third %d fifth %d (hm_now %u)\n", bar, r, th, fi, hm_now());
            assert(r % 12 == AX[bar % 4u] && th == TH[bar % 4u] && fi == 7 && r >= 36 && r < 48);
            for (; j < FS * 2u / CTL; j++) mix_block(x, CTL);
        }
        transport_req = 2;
        for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
        song.g[G_HLEN] = 4;                             /* 4 BAR: the owner's jam's */
        transport_req = 1;
        for (bar = 0; bar < 9u; bar++) {
            for (j = 0; j < FS * 2u / CTL / 2u; j++) mix_block(x, CTL);
            r = func_bank_chord(TSEL, 60, &th, &fi, &dg);
            assert(r % 12 == AX[bar / 4u % 4u]);
            for (; j < FS * 2u / CTL; j++) mix_block(x, CTL);
        }
        transport_req = 2;
        for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
        assert(!song.playing);
        TSEL->p[P_ROOT] = 9; TSEL->p[P_SCALE] = 2;      /* (A minor) */
        r = func_bank_chord(TSEL, 60, &th, &fi, &dg);
        assert(r == 45 && th == 3 && fi == 7);
        TSEL->p[P_ROOT] = 2; TSEL->p[P_SCALE] = 1;      /* (D major) */
        r = func_bank_chord(TSEL, 60, &th, &fi, &dg);
        assert(r == 38 && th == 4 && fi == 7);
        /* EXTENS: a sus or an add9's third, as HARMONY's own chords play it (each progression's chords, playing) */
        song.g[G_HEXT] = 127; song.g[G_HLEN] = 2; TSEL->p[P_ROOT] = 0;
        uint32_t other = 0, pr, i;
        song.playing = 1;
        for (pr = 1; pr < 40u; pr++) {
            song.g[G_HPROG] = (int16_t)pr; hm_load();
            for (i = 0; i < hm_n; i++) { bar_n = i; beat_n = 0; r = func_bank_chord(TSEL, 60, &th, &fi, &dg); other |= th != 3 && th != 4; }
        }
        song.playing = 0; bar_n = beat_n = 0;
        assert(other);
        song.g[G_HEXT] = 0;
    }
    puts("funcbank: the chord follows HARMONY (a bar, 4 BAR), stopped the key's tonic in its scale, EXTENS PASS");
    /* 4 the sound's pitch: PAD's lowest partial the chord's root at w x 2 (C3 on I) */
    {
        int32_t x[CTL * 2]; uint32_t j, i, at = 0;
        setup(0, 0, 0, 0);                              /* (PAD, DEF 0: no root below) */
        TSEL->p[P_FCUT] = 127;
        transport_req = 1;
        for (j = 0; j < FS / 2u / CTL; j++) mix_block(x, CTL);
        for (j = 0; j < FS / 2u / CTL; j++) { mix_block(x, CTL); for (i = 0; i < CTL; i++) mono[at++] = x[2u * i] + x[2u * i + 1u]; }
        transport_req = 2;
        for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
        /* (the energy at C3 E3 G3 over that a semitone off) */
        double on = 0, off = 0;
        for (k = 47; k <= 56; k++) {
            double fq = 440.0 * pow(2, ((int)k - 69) / 12.0), c = 2 * cos(2 * M_PI * fq / FS), q1 = 0, q2 = 0, q0, e;
            for (i = 0; i < at; i++) { q0 = mono[i] + c * q1 - q2; q2 = q1; q1 = q0; }
            e = q1 * q1 + q2 * q2 - c * q1 * q2;
            if (k == 48 || k == 52 || k == 55) on += e; else off += e;
        }
        assert(on > 20 * off);
    }
    puts("funcbank: PAD on I plays C3 E3 G3 PASS");
    /* 5 MORPH turned while it plays: no step; a FORM's change faded */
    {
        static const uint8_t PAIRS[][2] = {{0, 2}, {11, 13}, {20, 21}, {32, 34}};
        double worst = 0;
        for (k = 0; k < NELEM(PAIRS); k++) {
            double r[8]; uint32_t nr = 0, t;
            setup(PAIRS[k][0], PAIRS[k][1], 40, 64);
            transport_req = 1;
            render(FS / 2u / CTL, 0);
            for (t = 0; t < 8u; t++) { render(37u + t * 13u, 0); double q = trial((int16_t)(44 + (int)t % 2 * -8)); if (q >= 0) r[nr++] = q; }
            transport_req = 2;
            for (uint32_t i = 0; i < nr; i++) for (uint32_t j = i + 1u; j < nr; j++) if (r[j] < r[i]) { double z = r[i]; r[i] = r[j]; r[j] = z; }
            double med = nr >= 3u ? r[nr / 2u] : 0;
            if (med > worst) worst = med;
            if (med > 0.2) { printf("BANK %s > %s MORPH: onset %.2f\n", fb_name(PAIRS[k][0]), fb_name(PAIRS[k][1]), med); return 1; }
        }
        printf("MORPH: the worst pair's onset ratio %.2f\n", worst);
        /* the FORM's change: faded (math_fade) */
        setup(0, 2, 0, 64);
        transport_req = 1; render(FS / 2u / CTL, 0);
        TSEL->p[P_E2] = 1; math_poll(); render(1, 0);
        assert(math_fade[0] > 0u);
        render(FS / 4u / CTL, 0); assert(!math_fade[0]);
        TSEL->p[P_E4] = 64; render(FS / 4u / CTL, 0);   /* (B heard) */
        TSEL->p[P_E3] = 5; render(2, 0);               /* (FORM B: a name's change, the part's voices dipped out
                                                         * and in on it, voice.c engine_block; compiled where it plays) */
        assert(TSEL->xf_on && TSEL->xf_dip);
        for (k = 0; k < 200u && wave_id_b[0] != (0x8000u | 5u); k++) render(1, 0);
        assert(wave_id_b[0] == (0x8000u | 5u) && wave_prg_b[0].n > 0u && math_fade[0] > 0u);
        transport_req = 2; render(FS / 4u / CTL, 0);
    }
    puts("funcbank: MORPH turned while it plays, no step; a FORM's change dipped and faded PASS");
    puts("funcbank: all ok");
    return 0;
}
