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

int main(int argc, char **argv)
{
    static fvm_cc_t cc;
    static fvm_prog_t pg;
    uint32_t f, bad = 0, pi;
    (void)argc;
    (void)argv;
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
    printf(fails ? "wave: %d FAILED\n" : "wave: all ok\n", fails);
    return fails != 0;
}
