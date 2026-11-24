/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (FM V10.8.36) FX > CHORUS's MIX (params.c cho_mix; fx.c fx_buses, mix_part): at 0 the chorus as it was; towards 100
 * a part's dry goes down by what it sends, the ensemble becomes one voice in the middle: at 100 with the send whole,
 * a vibrato alone (the pitch moves, no dry under it). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <math.h>
static double rms(const int32_t *a, uint32_t n) { double e = 0; for (uint32_t i = 0; i < n; i++) e += (double)a[i] * a[i]; return sqrt(e / n); }
int main(void)
{
    int fails = 0, mix;
    static int32_t wl[3][CTL * 400], wr[3][CTL * 400];
    for (mix = 0; mix <= 100; mix += 50) {
        uint32_t b, i;
        host_tracks_init();
        song.g[G_CRATE] = 40; song.g[G_CDEPTH] = 60; song.g[G_CSTER] = 0; song.g[G_WIDTH] = 0;
        cho_mix = (int16_t)mix;
        for (b = 0; b < 800; b++) {
            for (i = 0; i < CTL; i++) {
                fxq_c[i] = (int32_t)(8000 * sin(2 * M_PI * 440.0 * (b * CTL + i) / 44100.0));
                fxq_d[i] = fxq_r[i] = 0;
            }
            memset(fx_sl, 0, sizeof fx_sl); memset(fx_sr, 0, sizeof fx_sr);   /* (the mixer's: summed into) */
            fx_run(CTL);
            if (b >= 400)
                for (i = 0; i < CTL; i++) {
                    wl[mix / 50][(b - 400) * CTL + i] = fx_wet[i] + fx_sl[i];
                    wr[mix / 50][(b - 400) * CTL + i] = fx_wet[i] + fx_sr[i];
                }
        }
    }
    {
        double l0 = rms(wl[0], CTL * 400), l2 = rms(wl[2], CTL * 400), d0 = 0, d = 0;
        uint32_t i;
        for (i = 0; i < CTL * 400; i++) d0 += fabs((double)wl[0][i] - wr[0][i]), d += fabs((double)wl[2][i] - wr[2][i]);
        printf("chorus mix: wet level MIX 0 %.0f, MIX 100 %.0f (%+.1f dB); left - right MIX 0 %.0f, MIX 100 %.0f\n",
               l0, l2, 20 * log10(l2 / l0), d0, d);
        if (d0 == 0) { printf("FAIL MIX 0 not stereo\n"); fails++; }
        if (d != 0) { printf("FAIL MIX 100 not one voice in the middle\n"); fails++; }
        if (l2 < l0 * 0.5 || l2 > l0 * 2.5) { printf("FAIL MIX 100 level\n"); fails++; }
    }
    {   /* the dry's share: a part's pan gains (mix_part's f) */
        int32_t f0 = 4096 - ((0 * 127 * 165) >> 9), f1 = 4096 - ((100 * 127 * 165) >> 9), f2 = 4096 - ((50 * 64 * 165) >> 9);
        printf("chorus mix: dry gain Q12: MIX 0 %d, MIX 100 send 127 %d, MIX 50 send 64 %d\n", f0, f1, f2);
        if (f0 != 4096 || f1 < 0 || f1 > 8 || f2 < 3000 || f2 > 3100) fails++;
    }
    {   /* the part's dry at MIX 100 and its CHORUS whole: gone from the mix (the chorus's return alone) */
        track_t *t = &trk[0];
        uint32_t b, i;
        double e[2];
        for (mix = 0; mix <= 100; mix += 100) {
            host_tracks_init();
            host_preset(t, 0, 0);
            t->p[P_CHOR] = 127; t->p[P_DLY] = t->p[P_REV] = 0;
            cho_mix = (int16_t)mix;
            e[mix / 100] = 0;
            for (b = 0; b < 200; b++) {
                if (b == 4) trk_note_on(t, 60, 100);
                memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
                memset(send_c, 0, sizeof send_c); memset(send_d, 0, sizeof send_d); memset(send_r, 0, sizeof send_r);
                mix_part_m(t, CTL);
                for (i = 0; i < CTL; i++) e[mix / 100] += (double)mix_l[i] * mix_l[i];
            }
        }
        printf("chorus mix: a part's dry energy MIX 0 %.3g, MIX 100 %.3g (%.1f dB)\n", e[0], e[1], 10 * log10((e[1] + 1) / (e[0] + 1)));
        if (!(e[0] > 0 && e[1] < e[0] * 1e-4)) { printf("FAIL the dry not gone at MIX 100\n"); fails++; }
    }
    {   /* kept with the project ('C' 'M'), PUNCH SEQ or not before it; one without it: 0 */
        static project_t q, r;
        static project_store_t st;
        int ok;
        cho_mix = 73;
        project_capture(&q);
        ok = proj_pack(&st, &q) && proj_import(&r, &st, sizeof st) && r.cmix == 73;
        cho_mix = 0;
        project_capture(&q);
        ok &= proj_pack(&st, &q) && proj_import(&r, &st, sizeof st) && r.cmix == 0;
        printf("chorus mix: kept with the project (73 and 0 back as they were) %s\n", ok ? "ok" : "FAIL");
        fails += !ok;
    }
    cho_mix = 0;
    printf(fails ? "chorus mix test FAILED\n" : "chorus mix test passed\n");
    return fails ? 1 : 0;
}
