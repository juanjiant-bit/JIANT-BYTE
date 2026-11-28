/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (wavefix) WAVE's knobs turned while a note is held make no click (v1.1b4, "moving any parameter of WAVE makes a click
 * on every step"), through the whole part (mix_block: the dip, the frames, the half rate), dry:
 *   POS and AMT under a spectral warp (SMEAR, SKEW, DISPRS): the frames drawn again, the frame heard kept meanwhile
 *   A with the formula compiled late (the device's main loop: one serve every 16 blocks): the dip's way in held until it
 *   is, never the old formula swapped at once
 * A click: a 4 ms window of the second difference's energy in the 40 ms after a knob's step over 4x the larger of the
 * 10 ms before and the 40 .. 80 ms after (v1.0 and v1.1b4: x6 .. x86 on these); x8 for a list value's step (A, WARP:
 * the dip's own fade out and in).
 *   cc -O2 -Ibuild/gen -Ifirmware/src -Itests -o build/host/wave_click_test tests/wave_click_test.c -lm */
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
    printf("wave_click: %-96s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

#define NS (3u * FS)
static double y[NS];
static uint32_t ny, lat;
static void block(void)
{
    static int32_t o[2 * CTL];
    uint32_t k;
    static uint32_t nb;
    mix_block(o, CTL);
    if (lat && ++nb % lat == 0u) {                    /* (the main loop's compile, now and then) */
        fvm_in_audio--;
        fvm_host_serve();
        fvm_in_audio++;
    }
    for (k = 0; k < CTL && ny < NS; k++)
        y[ny++] = (o[2 * k] + o[2 * k + 1]) / 65536.0;
}
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
    ny = 0;
    for (k = 0; k < 64u; k++) block();
    trk_note_on(t, 48, 100);
    ny = 0;
    for (k = 0; k < 400u; k++) block();               /* (~0.6 s: settled) */
}
static double e2(uint32_t a, uint32_t b)
{
    double e = 0;
    uint32_t i;
    for (i = a; i < b; i++) {
        double d = y[i] - 2 * y[i - 1] + y[i - 2];
        e += d * d;
    }
    return e / (b - a);
}
/* knob id stepped by d, n times, a step every 60 ms: the worst step's ratio */
static double sweep(uint32_t id, int d, uint32_t n)
{
    uint32_t s, ev[32], k, i;
    double worst = 0;
    for (s = 0; s < n && s < 32u; s++) {
        ev[s] = ny;
        trk[0].p[id] = (int16_t)(trk[0].p[id] + d);
        for (k = 0; k < 41u; k++) block();
    }
    for (k = 0; k < 60u; k++) block();
    for (s = 0; s < n && s < 32u; s++) {
        uint32_t e = ev[s];
        double b = e2(e - 441, e - 88), a = e2(e + 1764, e + 3528), m = 0;
        for (i = e; i < e + 1764; i += 44) {
            double q = e2(i, i + 176);
            if (q > m) m = q;
        }
        m /= fmax(a, b) + 1e-14;
        if (m > worst) worst = m;
    }
    return worst;
}

int main(void)
{
    static const int16_t SMEAR[8] = {0, 64, 6, 40, 0, 0, 0, 0}, PLAIN[8] = {0, 64, 0, 64, 0, 0, 0, 0},
                         FOLD[8] = {4, 30, 0, 0, 0, 0, 0, 0}, SKEW[8] = {4, 64, 7, 40, 0, 0, 0, 0},
                         DISP[8] = {1, 64, 9, 40, 0, 0, 0, 0};
    double r;
    setup(0, SMEAR);
    r = sweep(P_E1, 1, 12);
    ok(r < 4, "SINE SMEAR: POS up 12 steps, the worst x%.1f", r);
    setup(0, SMEAR);
    r = sweep(P_E3, 1, 12);
    ok(r < 4, "SINE SMEAR: AMT up 12 steps, the worst x%.1f", r);
    setup(0, SKEW);
    r = sweep(P_E1, 1, 12);
    ok(r < 4, "SAWSQ SKEW: POS up 12 steps, the worst x%.1f", r);
    setup(0, DISP);
    r = sweep(P_E3, -1, 12);
    ok(r < 4, "FM2 DISPRS: AMT down 12 steps, the worst x%.1f", r);
    lat = 16u;
    fvm_in_audio++;                                  /* (the compiles the main loop's only: block()) */
    setup(0, FOLD);
    r = sweep(P_E0, 1, 9);
    ok(r < 8, "SAWSQ .. A up 9 formulas, compiled 16 blocks late: the worst x%.1f", r);
    setup(0, PLAIN);
    r = sweep(P_E2, 1, 9);
    ok(r < 8, "SINE: WARP OFF .. DISPRS, compiled late: the worst x%.1f", r);
    fvm_in_audio--;
    lat = 0;
    printf("wave_click: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
