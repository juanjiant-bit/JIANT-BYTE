/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (FM V10.8.36) DIST's NOISE (fx.c track_dist, dist_noise; params.c dnz_v): it modulates the drive (sidebands round
 * the partials), adds nothing to silence and keeps the level, on every TYPE (the formula ones too); at 0 the drive
 * is bit for bit what it was. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
static int32_t buf[2][4096];
static double run(int nz, int type, int amp, double *side)
{
    track_t *t = &trk[0];
    uint32_t b, i, n = 4096 / CTL;
    double e = 0, sre = 0, sim = 0;
    host_tracks_init();
    t->p[P_DIST] = 60; t->p[P_DTYPE] = (int16_t)type; t->p[P_DTONE] = 0;
    dnz_v[0] = (int16_t)nz;
    dist_w[0] = 0; dist_nlp[0] = 0; dist_rnd = 0;
    for (b = 0; b < n * 4u; b++) {                       /* four passes: the blend in, the filters settled */
        int32_t *o = buf[nz ? 1 : 0] + (b % n) * CTL;
        for (i = 0; i < CTL; i++)
            o[i] = (int32_t)(amp * sin(2 * M_PI * 220.0 * (b * CTL + i) / 44100.0));
        track_dist(t, o, CTL);
    }
    for (i = 0; i < 4096; i++) {
        double x = buf[nz ? 1 : 0][i], w = 2 * M_PI * i / 44100.0, hw = 0.5 - 0.5 * cos(2 * M_PI * i / 4096.0);
        e += x * x;
        x *= hw;                                         /* (a Hann window: the partial's skirt out of the way) */
        sre += x * cos(380 * w); sim += x * sin(380 * w);   /* between the partials (and RING's 306 and 746 Hz): only
                                                         * the noise's sidebands */
    }
    *side = 10 * log10((sre * sre + sim * sim + 1e-9) / (e * 4096 + 1e-9));   /* against the whole signal */
    return sqrt(e / 4096);
}
int main(void)
{
    static const int TY[] = {0, 1, 2, 3, 4, 6, 7, 11, 16};   /* SOFT HARD FOLD RECT AMP, RING STEPS SFOLD SAT */
    int fails = 0, k;
    for (k = 0; k < (int)(sizeof TY / sizeof TY[0]); k++) {
        int type = TY[k];
        double s0, s1, sz, l0 = run(0, type, 12000, &s0), l1 = run(100, type, 12000, &s1), z0 = run(0, type, 0, &sz), z = run(100, type, 0, &sz);
        double q0, q1, m0 = run(0, type, 3000, &q0), m1 = run(100, type, 3000, &q1);
        double d = 20 * log10(l1 / l0), dq = 20 * log10(m1 / m0);
        printf("dist noise: type %2d: level %.0f -> %.0f (%+.2f dB; quiet %+.2f dB), between partials %.1f -> %.1f dB "
               "(quiet %.1f -> %.1f), silence %.1f\n", type, l0, l1, d, dq, s0, s1, q0, q1, z);
        if (z > z0 + 3) { printf("FAIL NOISE adds to silence (%.2f, without %.2f)\n", z, z0); fails++; }   /* (FOLD's own
                                                         * floor ~10, its corner's roundings moved) */
        if (d < -3.0 || d > 3.0 || dq < -3.0 || dq > 3.0) { printf("FAIL level moved\n"); fails++; }
        if (s1 < s0 + 6 || q1 < q0 + 6) { printf("FAIL no sidebands (quiet %.1f -> %.1f dB)\n", q0, q1); fails++; }
    }
    {   /* NOISE 0: as before (the same samples twice) */
        double s; static int32_t keep[4096];
        run(0, 0, 12000, &s); memcpy(keep, buf[0], sizeof keep);
        run(0, 0, 12000, &s);
        if (memcmp(keep, buf[0], sizeof keep)) { printf("FAIL NOISE 0 not deterministic\n"); fails++; }
    }
    printf(fails ? "dist noise test FAILED\n" : "dist noise test passed\n");
    return fails ? 1 : 0;
}
