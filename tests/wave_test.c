/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 3) WAVE (eng_wave.c), the parametric wavetable of formulas, rendered as the device does:
 *   every formula   compiles in the FVM; every preset sounds, bounded (no sample past full scale)
 *   pitch           SINE at A4: 440 Hz (zero crossings), an octave up twice
 *   POS             moves the timbre (FM2 at POS 0 and 127 apart), WARP each mode its own, DETUNE three voices
 *   SUB             adds the octave down
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/wave_test tests/wave_test.c -lm && build/host/wave_test */
#include <stdarg.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("wave: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

#define NS FS
static double ya[NS], yb[NS];

static void setup(uint32_t preset, const int16_t *e)
{
    track_t *t = &trk[0];
    uint32_t k;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(t, ENGI_WAVE, preset);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    for (k = 0; e && k < 8u; k++)
        if (e[k] >= 0)
            t->p[P_E0 + k] = e[k];
}
static void play(uint32_t note, double *y)
{
    static int32_t blk[CTL];
    uint32_t got = 0;
    trk_note_on(&trk[0], note, 100);
    while (got < NS) {
        uint32_t k;
        track_render(&trk[0], blk, CTL);
        for (k = 0; k < CTL && got < NS; k++)
            y[got++] = blk[k] / 32768.0;
    }
}
static double corr(const double *a, const double *b, uint32_t n)
{
    double ab = 0, aa = 0, bb = 0;
    uint32_t i;
    for (i = 0; i < n; i++) ab += a[i] * b[i], aa += a[i] * a[i], bb += b[i] * b[i];
    return aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 0;
}
static double peak(const double *a, uint32_t n)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < n; i++) if (fabs(a[i]) > p) p = fabs(a[i]);
    return p;
}
static double rms(const double *a, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++) s += a[i] * a[i];
    return sqrt(s / n);
}
static double goertzel(const double *a, uint32_t n, double hz)   /* the power at hz */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double s0 = a[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)n * n);
}
static double zc(const double *a, uint32_t n, uint32_t skip)
{
    uint32_t i, c = 0;
    for (i = skip + 1u; i < n; i++) c += a[i - 1] < 0 && a[i] >= 0;
    return c * (double)FS / (n - skip);
}

/* (wavepresets) the presets through the whole part (mix_block: the matrix, ENV 2, the LFO, the FILTER), dry: a held
 * note (a bass's C2, a lead's C4) or a chord (C3 G3 C4 E4), 3 s. Each: sounding, no sample at full scale, no DC; one
 * whose sound moves (a PAD, or ENV 2 / the LFO / ENV on POS AMT MIX or CUT) moves: its brightness (the RMS frequency,
 * sqrt(E[dx^2] / E[x^2]) FS / 2 pi, a 4096-sample window) over the held note at least a third of an octave apart */
#define MV_N (3u * FS)
static double ym[MV_N];
static const char *const N_PTAG[PT_N] = {"-", "BASS", "LEAD", "PAD", "KEYS", "PLUCK", "BELL", "VOX", "DRUM", "FX"};
static uint32_t pr_moves(const preset_t *pr)
{
    const int8_t *m = pr->mod;
    uint32_t k, src[4] = {0}, dst[4] = {0}, mv = PR_TAG(pr) == PT_PAD;
    for (; m && m[0] >= 0; m += 2) {
        for (k = 0; k < 4u; k++) {
            if (m[0] == P_M1SRC + 3 * (int)k) src[k] = (uint32_t)m[1];
            if (m[0] == P_M1DST + 3 * (int)k) dst[k] = (uint32_t)m[1];
        }
        mv |= (m[0] == P_ED_SHP || m[0] == P_LD_SHP) && m[1];
    }
    for (k = 0; k < 4u; k++)
        mv |= (src[k] == PS_ENV2 || src[k] == PS_LFO) &&
              (dst[k] == PD_SHP || dst[k] == PD_E(1) || dst[k] == PD_E(3) || dst[k] == PD_E(5) || dst[k] == PD_FCUT);
    return mv;
}
static void presets_moving(void)
{
    static const uint8_t CH[4] = {48, 55, 60, 64};
    static int32_t o[2 * CTL];
    uint32_t pi, i, k, bad = 0, still = 0, nmove = 0, ncat[PT_N] = {0}, seen = 0, names = 0;
    for (pi = 0; pi < NELEM(WAVE_PRESETS); pi++) {
        const preset_t *pr = &WAVE_PRESETS[pi];
        uint32_t tg = PR_TAG(pr), nn = PR_MONO(pr) || tg == PT_LEAD ? 1u : 4u, got = 0, w;
        double pk = 0, s = 0, dc = 0, lo = 1e9, hi = 0, top = 0;
        setup(pi, 0);
        for (i = 0; i < 64u; i++) mix_block(o, CTL);
        for (k = 0; k < nn; k++)
            trk_note_on(&trk[0], nn == 1u ? (tg == PT_BASS ? 36u : 60u) : CH[k], 100);
        while (got < MV_N) {
            mix_block(o, CTL);
            for (k = 0; k < CTL && got < MV_N; k++) {
                double y = (o[2 * k] + o[2 * k + 1]) / 65536.0;
                ym[got++] = y;
                if (fabs(o[2 * k]) > pk * 32768) pk = fabs(o[2 * k]) / 32768;
                if (fabs(o[2 * k + 1]) > pk * 32768) pk = fabs(o[2 * k + 1]) / 32768;
                s += y * y, dc += y;
            }
        }
        s = sqrt(s / MV_N), dc /= MV_N;
        for (w = 0; w + 4096u <= MV_N; w += 2048u) {
            double e = 0;
            for (i = w; i < w + 4096u; i++) e += ym[i] * ym[i];
            if (e > top) top = e;
        }
        for (w = 0; w + 4096u <= MV_N; w += 2048u) {   /* (a window under -30 dB of the loudest: a pluck's tail, left out) */
            double e = 0, d = 0, f;
            for (i = w + 1u; i < w + 4096u; i++) e += ym[i] * ym[i], d += (ym[i] - ym[i - 1]) * (ym[i] - ym[i - 1]);
            if (e < top / 1000.0) continue;
            f = sqrt(d / e) * FS / (2 * M_PI);
            if (f < lo) lo = f;
            if (f > hi) hi = f;
        }
        if (s < 0.02 || pk >= 0.99 || pk < 0.15 || fabs(dc) > 0.005) {
            printf("wave: preset %-10s rms %.3f peak %.3f dc %+.4f\n", pr->name, s, pk, dc);
            bad++;
        }
        if (pr_moves(pr) && (tg == PT_PAD || pi >= 8u)) {   /* (the PADs and the curated set from 8 on; FM KEYS' bark,
                                                         * BIT TABLE's xor: a timbre, not a brightness, moved) */
            nmove++;
            if (hi < lo * 1.26) {
                printf("wave: preset %-10s brightness %.0f .. %.0f Hz: still\n", pr->name, lo, hi);
                still++;
            }
        }
        if (getenv("WAVE_PRESETS"))
            printf("wave:   %-10s %-5s rms %.3f peak %.3f dc %+.4f brightness %5.0f .. %5.0f Hz (%.2f oct)%s\n", pr->name,
                   N_PTAG[tg], s, pk, dc, lo, hi, log2(hi / lo), pr_moves(pr) && (tg == PT_PAD || pi >= 8u) ? " moves" : "");
        ncat[tg]++;
        for (k = 0; pr->name[k] && (pr->name[k] == ' ' || (pr->name[k] >= 'A' && pr->name[k] <= 'Z') || (pr->name[k] >= '0' && pr->name[k] <= '9')); k++)
            ;
        names += pr->name[k] || k > 10u;
        for (k = 0; k < pi; k++)
            names += !strcmp(WAVE_PRESETS[k].name, pr->name);
    }
    for (k = 0; k < sizeof WAVE_ORDER; k++)
        seen |= 1u << WAVE_ORDER[k];
    ok(!bad, "every preset (%u) through the part, dry: rms >= 0.02, peak 0.15 .. 0.99, no DC", (unsigned)NELEM(WAVE_PRESETS));
    ok(!still, "the %u moving presets (PADs; ENV 2 / LFO / ENV on POS AMT MIX CUT) move: a third of an octave or more", nmove);
    ok(ncat[PT_BASS] >= 6u && ncat[PT_PAD] >= 6u && nmove >= 16u,
       "a palette: %u BASS, %u PAD, %u KEYS, %u PLUCK, %u LEAD, %u VOX, %u BELL, %u FX", ncat[PT_BASS], ncat[PT_PAD], ncat[PT_KEYS],
       ncat[PT_PLUCK], ncat[PT_LEAD], ncat[PT_VOX], ncat[PT_BELL], ncat[PT_FX]);
    ok(!names && seen == (1u << NELEM(WAVE_PRESETS)) - 1u, "names: up to 10 of A-Z 0-9 and space, none twice; WAVE_ORDER shows each once");
}

int main(int argc, char **argv)
{
    static fvm_cc_t cc;
    static fvm_prog_t pg;
    static fvm_ins_t pgi[FVM_MAXINS];
    uint32_t f, bad = 0, pi;
    (void)argc;
    (void)argv;
    fvm_prog_init(&pg, pgi, FVM_MAXINS);
    for (f = 0; f < WAVE_NF; f++) {
        fvm_err_t er;
        const char *s = WAVE_BANK[f];
        if (fvm_compile(&cc, &pg, s, (uint32_t)strlen(s), &er) || pg.ty != FT_FIX) {
            printf("wave: %s: error %u at %u (or not FIX)\n", N_WAVEF[f], er.code, er.pos);
            bad++;
        }
    }
    ok(!bad, "the %u formulas compile, each a FIX (floatbeat) result", (unsigned)WAVE_NF);
    bad = 0;
    for (pi = 0; pi < NELEM(WAVE_PRESETS); pi++) {
        double r, p;
        setup(pi, 0);
        play(60, ya);
        r = rms(ya, NS);
        p = peak(ya, NS);
        if (r < 0.01 || p > 1.0) {
            printf("wave: preset %s: rms %.3f peak %.3f\n", WAVE_PRESETS[pi].name, r, p);
            bad++;
        }
    }
    ok(!bad, "every preset (%u) sounds and is bounded at C4", (unsigned)NELEM(WAVE_PRESETS));
    {
        static const int16_t SINE[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        double z1, z2;
        setup(0, SINE);
        play(69, ya);
        z1 = zc(ya, NS, FS / 10u);
        setup(0, SINE);
        play(81, yb);
        z2 = zc(yb, NS, FS / 10u);
        ok(fabs(z1 - 440.0) < 4.0 && fabs(z2 - 880.0) < 8.0, "SINE in tune: A4 %.1f Hz, A5 %.1f Hz", z1, z2);
    }
    {
        static const int16_t P0[8] = {1, 0, 0, 0, 0, 0, 0, 0}, P1[8] = {1, 127, 0, 0, 0, 0, 0, 0};
        double c;
        setup(0, P0);
        play(60, ya);
        setup(0, P1);
        play(60, yb);
        c = corr(ya, yb, NS);
        ok(c < 0.9, "POS moves the timbre: FM2 at POS 0 and 127 (correlation %.3f)", c);
    }
    {
        uint32_t w, same = 0;
        static const int16_t W0[8] = {4, 64, 0, 90, 0, 0, 0, 0};
        static double base[NS];
        setup(0, W0);
        play(60, base);
        for (w = 1; w <= WW_FM; w++) {
            int16_t e[8] = {4, 64, (int16_t)w, 90, 1, 0, 0, 0};
            double c;
            setup(0, e);
            play(60, ya);
            c = corr(base, ya, NS);
            if (c > 0.97 || peak(ya, NS) > 1.0) {
                printf("wave: WARP %s: correlation with OFF %.3f, peak %.3f\n", N_WARP[w], c, peak(ya, NS));
                same++;
            }
        }
        ok(!same, "WARP: SYNC BEND MIRROR QUANT FM each bend the phase (none as OFF), bounded");
    }
    {
        static const int16_t D0[8] = {4, 0, 0, 0, 0, 0, 0, 0}, D1[8] = {4, 0, 0, 0, 0, 0, 60, 0};
        double c;
        setup(0, D0);
        play(60, ya);
        setup(0, D1);
        play(60, yb);
        c = corr(ya, yb, NS);
        ok(c < 0.9 && peak(yb, NS) <= 1.0, "DETUNE: three voices spread (correlation with one %.3f), bounded", c);
    }
    {
        static const int16_t S0[8] = {0, 0, 0, 0, 0, 0, 0, 0}, S1[8] = {0, 0, 0, 0, 0, 0, 0, 127};
        double g1, g2;
        setup(0, S0);
        play(69, ya);
        setup(0, S1);
        play(69, yb);
        g1 = goertzel(ya, NS, 220.0);
        g2 = goertzel(yb, NS, 220.0);
        ok(g2 > 100.0 * g1 + 1e-6, "SUB: the octave down added (power at 220 Hz %.2e -> %.2e)", g1, g2);
    }
    presets_moving();
    printf(fails ? "wave: %d FAILED\n" : "wave: all ok\n", fails);
    return fails != 0;
}
