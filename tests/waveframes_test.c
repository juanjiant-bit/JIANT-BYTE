/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (waveframes) WAVE's frames (eng_wave.c wf_*), rendered as the device does (mix_block: a step of the frames a block):
 *   at rest      a sound whose POS nothing moves: the frames point for point the old table (bit for bit)
 *   bilinear     between two points and two frames, the ramp of a frame a sample; STEP the nearest frame only
 *   smooth POS   POS on a fast LFO: no step at the blocks' edges (the table's were)
 *   UNISON       1 .. 7 voices, CLASSIC SHIMMER CHORD: the level within 1.5 dB of one voice; WIDTH: L and R apart
 *   spectral     SMEAR SKEW SHEPARD each its own spectrum, DISPERSE its own wave (the levels kept); SHEPARD climbs
 *   WSEQ         POS from the STEP MOD lane; the frames' settings saved and loaded with the project
 *   presets      every preset through the frames and through the table: bounded, at rest the same
 *   cc -O2 -Ibuild/gen -Ifirmware/src -Itests -o build/waveframes tests/waveframes_test.c -lm */
#include <stdarg.h>
static int wf_test_off;
#define WF_NOFRAMES wf_test_off
#define UI_TEST_NO_MAIN
#include "ui_test.c"

static int nfail;
static void okw(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("waveframes: %-96s %s\n", b, c ? "ok" : "FAIL");
    nfail += !c;
}

#define NB 1024u                                     /* blocks rendered */
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static int32_t L[NB * CTL], R[NB * CTL], *A0;        /* (A0: shared with the renders, each in a fresh process: fork) */
static int32_t vo[2 * CTL];
static void part(uint32_t pr)
{
    track_t *t = &trk[0];
    uint32_t k;
    ui_power_on();
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
    set_engine_of(t, ENGI_WAVE);
    t->engine = t->eng_req;
    apply_preset_to(t, pr);
    t->p[P_MUTE] = 0;
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_VOICE] = V_POLY; t->p[P_SUS] = 127;
    song.master_q12 = 4096;
    for (k = 0; k < 64u || t->xf_on || t->eng_req != t->engine; k++) mix_block(vo, CTL);
}
static void run(uint32_t note, uint32_t nb)          /* a note held, nb blocks: L R */
{
    uint32_t b, i;
    for (b = 0; b < 80u; b++) mix_block(vo, CTL);    /* (the frames drawn: a step a block) */
    if (note) trk_note_on(&trk[0], note, 100);
    for (b = 0; b < nb; b++) {
        mix_block(vo, CTL);
        for (i = 0; i < CTL; i++) L[b * CTL + i] = vo[2 * i], R[b * CTL + i] = vo[2 * i + 1];
    }
}
static double rms(const int32_t *x, uint32_t a, uint32_t b)
{
    double s = 0;
    for (uint32_t i = a; i < b; i++) s += (double)x[i] * x[i];
    return sqrt(s / (b - a));
}
static double diff_db(const int32_t *a, const int32_t *b, uint32_t n)
{
    double e = 0, s = 0;
    for (uint32_t i = 0; i < n; i++) e += ((double)a[i] - b[i]) * ((double)a[i] - b[i]), s += (double)a[i] * a[i];
    return e == 0 ? -999 : 10 * log10(e / (s + 1));
}

/* the harmonics' levels of a 128-point frame (DFT) */
static void mags(const int16_t *x, double *m)
{
    for (int h = 0; h < 64; h++) {
        double re = 0, im = 0;
        for (int i = 0; i < 128; i++) re += x[i] * cos(2 * M_PI * h * i / 128), im -= x[i] * sin(2 * M_PI * h * i / 128);
        m[h] = sqrt(re * re + im * im) / 64;
    }
}
static double spec_db(const double *a, const double *b)
{
    double e = 0, s = 0;
    for (int h = 1; h < 64; h++) e += (a[h] - b[h]) * (a[h] - b[h]), s += a[h] * a[h];
    return 10 * log10(e / s + 1e-12);
}

/* a render in a fresh process (the device's state from power-on each time): setup k, note, nb blocks into dst (L; R) */
typedef void (*setup_f)(uint32_t k);
static void fresh(setup_f f, uint32_t k, uint32_t note, uint32_t nb, int32_t *dl, int32_t *dr, int off)
{
    pid_t pid = fork();
    if (!pid) {
        f(k);
        wf_test_off = off;
        run(note, nb);
        memcpy(dl, L, nb * CTL * 4u);
        if (dr) memcpy(dr, R, nb * CTL * 4u);
        _exit(0);
    }
    waitpid(pid, 0, 0);
}
static void s_rest(uint32_t k) { part(0); trk[0].p[P_E0] = 1; trk[0].p[P_E1] = 40; (void)k; }
static void s_lfo(uint32_t k)
{
    part(0); trk[0].p[P_E0] = 14; trk[0].p[P_E1] = 64; (void)k;
    trk[0].p[P_LWAVE] = 0; trk[0].p[P_LRATE] = 120; trk[0].p[P_LD_SHP] = 63;
}
static void s_uni(uint32_t k) { part(0); trk[0].p[P_E0] = 4; trk[0].p[P_E1] = 30; trk[0].p[P_E6] = 64; trk[0].p[P_MORPH] = (int16_t)(k & 63u); trk[0].p[P_DXIMG] = (int16_t)(k >> 6); }
static void s_warp(uint32_t k) { part(0); trk[0].p[P_E0] = 4; trk[0].p[P_E1] = 40; trk[0].p[P_E2] = (int16_t)k; trk[0].p[P_E3] = 90; }
static void s_pre(uint32_t k) { part(k); }
static int32_t *B0, *B1;

int main(void)
{
    uint32_t i, b;
    A0 = mmap(0, 3u * NB * CTL * 4u, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    B0 = A0 + NB * CTL, B1 = B0 + NB * CTL;
    vpol_on = 0;
    {   /* at rest: FM2 at POS 40, nothing on POS: the frames' one frame is the table */
        s_rest(0);
        run(60, 16);
        okw(wf[0].use && wf[0].na == 1u, "the frames drawn (one: nothing moves POS) and read");
        fresh(s_rest, 0, 60, 256, A0, 0, 0);
        memcpy(B0, A0, 256u * CTL * 4u);
        fresh(s_rest, 0, 60, 256, A0, 0, 1);
        okw(!memcmp(B0, A0, 256u * CTL * 4u), "at rest: bit for bit the old table at POS (FM2, POS 40)");
    }
    {   /* bilinear: frame 0 a ramp, frame 1 its negative; a frame's ramp across, STEP the nearest */
        int16_t *F = comb_buf[5];
        uint32_t W[32], y[32], bad = 0, sbad = 0;
        for (i = 0; i < 128u; i++) F[i] = (int16_t)(i * 256), F[128 + i] = (int16_t)(-(int32_t)i * 256);
        for (i = 0; i < 32u; i++) W[i] = (i * 3u + 1u) * 512u + 256u;   /* (between two points) */
        wf_read(F, W, 32u, 0, 4096 / 32, 0u, y);
        for (i = 0; i < 32u; i++) {
            double j = (i * 3 + 1) + 0.5, a0 = j * 256, a1 = -j * 256, bb = i / 32.0, want = (a0 + (a1 - a0) * bb) * 2;
            bad += fabs((int32_t)y[i] - want) > 4.0;
        }
        wf_read(F, W, 32u, 0, 4096 / 32, 1u, y);
        for (i = 0; i < 32u; i++) {
            double j = (i * 3 + 1) + 0.5, want = (i < 16u ? j : -j) * 256 * 2;
            sbad += fabs((int32_t)y[i] - want) > 2.0;
        }
        okw(!bad, "bilinear: between two points and two frames, a frame's ramp across a run (%u off)", bad);
        okw(!sbad, "STEP: the nearest frame, no blend (%u off)", sbad);
    }
    {   /* smooth POS: SINTR on an LFO (~27 Hz): the table stepped a block at a time (sidebands between the harmonics, at
         * the blocks' rate), the frames not: the power half way between the harmonics against the harmonics' */
        double r[2], f0 = 440.0 * pow(2, (48 - 69) / 12.0);
        for (int k = 0; k < 2; k++) {
            double on = 0, off = 0;
            fresh(s_lfo, 0, 48, 1024, A0, 0, k);
            for (int h = 1; h <= 40; h++) {
                double gp[2];
                for (int q = 0; q < 2; q++) {
                    double w = 2 * M_PI * (h + 0.5 * q) * f0 / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
                    for (i = 128u * CTL; i < 1024u * CTL; i++) { double s0 = A0[i] + c * s1 - s2; s2 = s1; s1 = s0; }
                    gp[q] = s1 * s1 + s2 * s2 - c * s1 * s2;
                }
                on += gp[0], off += gp[1];
            }
            r[k] = 10 * log10(off / on);
        }
        okw(r[0] < r[1] - 10, "smooth POS: no steps (between the harmonics: frames %.1f dB, the table %.1f dB)", r[0], r[1]);
    }
    {   /* UNISON's level: SAWSQ, DETUNE 64, 1 .. 7 voices each mode */
        double r1 = 0, worst = 0, d0, d1;
        static const int16_t VC[] = {1, 2, 3, 5, 7, 9, 13, 15, 19, 0};
        for (uint32_t k = 0; k < sizeof VC / sizeof VC[0]; k++) {
            double r;
            fresh(s_uni, (uint32_t)VC[k], 60, NB, A0, 0, 0);
            r = rms(A0, 64u * CTL, NB * CTL);
            if (!k) r1 = r;
            printf("waveframes:   VOICES %-6s %+.2f dB of one\n", N_VOICES[VC[k]], 20 * log10(r / r1));
            if (fabs(20 * log10(r / r1)) > worst) worst = fabs(20 * log10(r / r1));
        }
        okw(worst < 1.5, "UNISON: 1 .. 7 voices, CLASSIC SHIMMER CHORD, AUTO: each within %.2f dB of one voice", worst);
        fresh(s_uni, 7u | 127u << 6, 48, 256, B0, B1, 0);
        d1 = diff_db(B0, B1, 256u * CTL);
        fresh(s_uni, 7u, 48, 256, B0, B1, 0);
        d0 = diff_db(B0, B1, 256u * CTL);
        okw(d1 > -20 && d0 < -200, "WIDTH: the side voices across (L - R %.1f dB), none at 0 (%.1f)", d1, d0);
    }
    {   /* the spectral warps on a saw frame */
        int16_t src[128], dst[128];
        double m0[64], m1[64];
        for (i = 0; i < 128u; i++) src[i] = (int16_t)(((int32_t)i - 64) * 400);
        mags(src, m0);
        for (uint32_t k = WW_SMEAR; k <= WW_DISP; k++) {
            double c = 0, aa = 0, bb = 0, sd;
            wf_spec(src, dst, k, 100u, 64 << 8, 3u);
            mags(dst, m1);
            sd = spec_db(m0, m1);
            for (i = 0; i < 128u; i++) c += (double)src[i] * dst[i], aa += (double)src[i] * src[i], bb += (double)dst[i] * dst[i];
            c /= sqrt(aa * bb);
            if (k == WW_DISP)
                okw(c < 0.8 && sd < -20, "DISPERSE: its own wave (correlation %.2f), the harmonics' levels kept (%.1f dB)", c, sd);
            else
                okw(sd > -10, "%s: the spectrum its own (%.1f dB from the saw's)", N_WARP[k], sd);
        }
        {   /* SHEPARD climbs: the frames' centre (log2 of the harmonic, weighted) up 0 .. 7 */
            double prev = -1;
            int up = 1;
            for (uint32_t j = 0; j < 8u; j++) {
                double sw = 0, w = 0;
                wf_spec(src, dst, WW_SHEP, 0u, 0, j);
                mags(dst, m1);
                for (int h = 1; h < 64; h++) sw += m1[h] * m1[h] * log2(h), w += m1[h] * m1[h];
                up = up && sw / w > prev;
                prev = sw / w;
            }
            okw(up, "SHEPARD: each frame higher than the one before (an octave over the eight)");
        }
        {   /* end to end: each warp its sound (SAWSQ, POS 40) */
            uint32_t same = 0;
            fresh(s_warp, 0, 48, 128, B0, 0, 0);
            for (uint32_t k = WW_SMEAR; k <= WW_DISP; k++) {
                fresh(s_warp, k, 48, 128, A0, 0, 0);
                same += diff_db(B0, A0, 128u * CTL) < -10;
            }
            okw(!same, "the four spectral warps, rendered: each another sound than none");
        }
    }
    {   /* WSEQ: POS from the lane, each step a frame; saved, loaded */
        double lo = 1e18, hi = 0;
        part(0);
        trk[0].p[P_E0] = 1; trk[0].p[P_KEY] = 3;    /* (SEQ) */
        trk[0].p[P_MSLEN] = 2; trk[0].p[P_MSDIV] = 2;
        trk[0].p[P_MS0] = 0; trk[0].p[P_MS0 + 1] = 127;
        transport_req = 1; ui_input();
        run(60, 512);
        transport_req = 2; for (b = 0; b < 8u; b++) mix_block(vo, CTL); frames(200);
        for (b = 64; b + 32u < 512u; b += 32u) {     /* (FM2: POS 0 a sine, 127 bright: the steps alternate) */
            double d = 0, e = 0;
            for (i = b * CTL + 1u; i < (b + 32u) * CTL; i++) d += ((double)L[i] - L[i - 1]) * ((double)L[i] - L[i - 1]), e += (double)L[i] * L[i];
            d = sqrt(d / (e + 1));
            if (d < lo) lo = d;
            if (d > hi) hi = d;
        }
        okw(hi > 1.5 * lo, "WSEQ: the lane's two steps two timbres (brightness %.3f .. %.3f)", lo, hi);
        trk[0].p[P_MORPH] = 12; trk[0].p[P_DXIMG] = 77; trk[0].p[P_LRTRG] = 90;
        project_save(0);
        printf("waveframes:   save: %s\n", ui.msg);
        trk[0].p[P_KEY] = 0; trk[0].p[P_MS0 + 1] = 5; trk[0].p[P_MORPH] = 0; trk[0].p[P_DXIMG] = 0; trk[0].p[P_LRTRG] = 0;
        project_load(0);
        frames(200);
        printf("waveframes:   loaded: FRAMES %d lane %d VOICES %d WIDTH %d BLEND %d\n", trk[0].p[P_KEY], trk[0].p[P_MS0 + 1],
               trk[0].p[P_MORPH], trk[0].p[P_DXIMG], trk[0].p[P_LRTRG]);
        okw(trk[0].p[P_KEY] == 3 && trk[0].p[P_MS0 + 1] == 127 && trk[0].p[P_MORPH] == 12 && trk[0].p[P_DXIMG] == 77 &&
            trk[0].p[P_LRTRG] == 90, "WSEQ, VOICES WIDTH BLEND: saved and loaded with the project");
    }
    {   /* every preset: through the frames and through the table, a chord */
        uint32_t bad = 0;
        for (uint32_t k = 0; k < NELEM(WAVE_PRESETS); k++) {
            double d, pk = 0;
            fresh(s_pre, k, 55, 256, B0, 0, 0);
            fresh(s_pre, k, 55, 256, A0, 0, 1);
            d = diff_db(A0, B0, 256u * CTL);
            for (i = 0; i < 256u * CTL; i++) if (fabs(B0[i]) > pk) pk = fabs(B0[i]);
            printf("waveframes:   %-10s frames against the table %7.1f dB, peak %.2f\n", WAVE_PRESETS[k].name, d, pk / 32768);
            bad += pk >= 32767 * 1.0 || pk < 200;
        }
        okw(!bad, "every preset (%u) through the frames: sounding, bounded", (unsigned)NELEM(WAVE_PRESETS));
    }
    {   /* the pages: UNISON FRAMES WSEQ a WAVE track's only */
        uint32_t k, seen = 0;
        part(0);
        song.sel = 0;
        for (k = 0; k < NPAGES; k++)
            if (str_eq(PAGES[k].title, "UNISON") || str_eq(PAGES[k].title, "FRAMES") || str_eq(PAGES[k].title, "WSEQ"))
                seen += page_visible(k);
        select_engine(ENGI_LOFI); frames(100);
        for (k = 0; k < NPAGES; k++)
            if (str_eq(PAGES[k].title, "UNISON") || str_eq(PAGES[k].title, "FRAMES") || str_eq(PAGES[k].title, "WSEQ"))
                seen += 10u * page_visible(k);
        okw(seen == 3u, "UNISON, FRAMES, WSEQ: WAVE's pages, not MATH's (%u)", seen);
    }
    printf(nfail ? "waveframes: %d FAILED\n" : "waveframes: all ok\n", nfail);
    return nfail != 0;
}
