/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DRUM-W: its tables drawn, its pads sounding, each STRUCT and modulator heard, the pads stored */
#include <stdarg.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int dw_t_fails;
static void dw_t_ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("drumw: %-98s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        dw_t_fails++;
}

#define NS (FS / 2u)
static double ya[NS], yb[NS], yc[NS];

static void kit(void)
{
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_drums(&trk[0]);
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}
static void coldkit(void) { memset(drum_kit, 0, sizeof drum_kit); kit(); }
static uint32_t hit_lane = 0;                        /* (the lane hit: DV_KICK unless set) */
/* the KICK lane hit at the start (and again at half, if twice), NS samples */
static void hit(double *y, int twice)
{
    static int32_t blk[CTL];
    uint32_t got = 0;
    trk_note_on(&trk[0], DRUM_LANE_NOTE[hit_lane], 110);
    while (got < NS) {
        uint32_t k;
        if (twice && got == NS / 2u / CTL * CTL) {
            trk_note_off(&trk[0], DRUM_LANE_NOTE[hit_lane]);
            trk_note_on(&trk[0], DRUM_LANE_NOTE[hit_lane], 110);
        }
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
static double energy(const double *a, uint32_t i0, uint32_t i1)
{
    double s = 0;
    uint32_t i;
    for (i = i0; i < i1; i++) s += a[i] * a[i];
    return s;
}
static double peak(const double *a, uint32_t n)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < n; i++) if (fabs(a[i]) > p) p = fabs(a[i]);
    return p;
}


static double dlt(const double *a, const double *b) { double d = 0, e = 0; uint32_t i; for (i = 0; i < NS; i++) d += (a[i] - b[i]) * (a[i] - b[i]), e += a[i] * a[i]; return sqrt(d / (e + 1e-12)); }
static void draw_all(void) { uint32_t k; for (k = 0; k < 16u; k++) dw_poll(); }
int main(void)
{
    uint32_t l, k;
    kit();
    draw_all();
    dw_t_ok(dw_have == 0xFFFFu, "the sixteen tables drawn in sixteen main-loop calls (%04x)", (unsigned)dw_have);
    for (l = 0; l < 8u; l++) {                       /* every factory pad: sounding, bounded */
        double e, p;
        kit(); hit_lane = l; hit(ya, 0);
        e = energy(ya, 0, NS); p = peak(ya, NS);
        dw_t_ok(e > 3.0 && p < 1.6, "pad %u sounds, bounded: energy %.2f peak %.3f", (unsigned)l, e, p);
    }
    hit_lane = 0;
    for (k = 1; k < DWS_N; k++) {                     /* each STRUCT not MIX */
        kit(); dw_kit[0].str = DWS_MIX; dw_kit[0].bamt = 110; hit(ya, 0);
        kit(); dw_kit[0].str = (uint8_t)k; dw_kit[0].bamt = 110; hit(yb, 0);
        dw_t_ok(dlt(ya, yb) > 0.1, "STRUCT %s changes the hit (%.3f)", N_DWS[k], dlt(ya, yb));
    }
    {
        static const char *const NM[5] = {"TABLE A", "MOD ENV", "MOD LFO", "TABLE B", "RATIO"};
        for (k = 0; k < 5u; k++) {
            dw_lane_t *D = &dw_kit[0];
            kit(); D->bamt = 90; D->str = DWS_RING; D->eamt = 64; D->lrate = 0; hit(ya, 0);
            kit(); D->bamt = 90; D->str = DWS_RING; D->eamt = 64; D->lrate = 0;
            if (k == 0) D->ta = (uint8_t)((D->ta + 4u) & 15u);
            else if (k == 1) { D->eamt = 127; D->edst = DWD_POS; }
            else if (k == 2) { D->lrate = 100; D->ldep = 127; }
            else if (k == 3) D->tb = (uint8_t)((D->tb + 5u) & 15u);
            else D->ratio = (uint8_t)((D->ratio + 4u) % DW_NRAT);
            hit(yb, 0);
            dw_t_ok(dlt(ya, yb) > 0.05, "%s changes the hit (%.3f)", NM[k], dlt(ya, yb));
        }
    }
    for (k = 0; k < DWD_N; k++) {                     /* each DEST heard, from the ENV and from the LFO */
        uint32_t lf;
        for (lf = 0; lf < 2u; lf++) {
            dw_lane_t *D = &dw_kit[0];
            kit(); D->bamt = 90; D->str = DWS_RING; D->eamt = 64; D->lrate = 0; hit(ya, 0);
            kit(); D->bamt = 90; D->str = DWS_RING; D->eamt = 64; D->lrate = 0;
            if (lf) { D->lrate = 90; D->ldep = 127; D->ldst = (uint8_t)k; D->lshp = DWL_SQR; }
            else { D->eamt = k == DWD_PITCH ? 0 : 127; D->edec = 90; D->edst = (uint8_t)k; }
            hit(yb, 0);
            dw_t_ok(dlt(ya, yb) > 0.03, "%s to %s changes the hit (%.3f)", lf ? "LFO" : "ENV", N_DWD[k], dlt(ya, yb));
        }
    }
    {                                                 /* a soft hit elsewhere in the table (VEL) */
        static int32_t blk[CTL];
        double e0 = 0, e1 = 0; uint32_t got;
        for (k = 0; k < 2u; k++) {
            double *y = k ? yb : ya;
            kit(); dw_kit[0].vel = k ? 127 : 64;
            trk_note_on(&trk[0], DRUM_LANE_NOTE[0], 40);
            for (got = 0; got < NS; ) { uint32_t j; track_render(&trk[0], blk, CTL); for (j = 0; j < CTL && got < NS; j++) y[got++] = blk[j] / 32768.0; }
        }
        for (l = 0; l < NS; l++) e0 += ya[l] * ya[l], e1 += yb[l] * yb[l];
        dw_t_ok(dlt(ya, yb) > 0.05 && e1 > 0, "VEL moves a soft hit (%.3f)", dlt(ya, yb));
    }
    {
        static project_t q, r;
        static project_store_t st;
        int same;
        kit();
        dw_kit[3].ta = 9; dw_kit[3].str = DWS_FOLD; dw_kit[3].lrate = 33; dw_kit[3].ratio = 7;
        project_capture(&q);
        dw_t_ok(proj_pack(&st, &q), "the project packed");
        memset(&r, 0, sizeof r);
        same = proj_import(&r, st.raw, (int)sizeof st.raw) && !memcmp(r.dw, q.dw, sizeof q.dw);
        dw_t_ok(same, "the pads' tables and motion stored and read back ('D' 'W')");
    }
    {
        dw_lane_t d;
        memset(&d, 0xFF, sizeof d);
        dw_sane(&d);
        dw_t_ok(d.ta < 16 && d.tb < 16 && d.str < DWS_N && d.ratio < DW_NRAT && d.noise <= DWN_RESON && d.tick <= DWT_CLICK, "a stored pad kept in range");
    }
    {   /* (BYTE 0.16) COLOR: ATTACK (the kit's transients: the hit's first 5 ms against its tail) and GAIN (clean, louder) */
        double e0, e1, h0, h1, t0, t1;
        uint32_t k, a5 = FS / 200u;
        hit_lane = 0;
        kit(); trk[0].p[P_E1] = 64; trk[0].p[P_E7] = 0; hit(ya, 0);
        kit(); trk[0].p[P_E1] = 127; hit(yb, 0);
        kit(); trk[0].p[P_E7] = 127; hit(yc, 0);
        for (h0 = h1 = t0 = t1 = e0 = e1 = 0, k = 0; k < NS; k++) {
            if (k < a5) h0 += ya[k] * ya[k], h1 += yb[k] * yb[k]; else t0 += ya[k] * ya[k], t1 += yb[k] * yb[k];
            e0 += ya[k] * ya[k]; e1 += yc[k] * yc[k];
        }
        dw_t_ok(h1 / (t1 + 1e-12) > 1.2 * h0 / (t0 + 1e-12), "ATTACK 127 sharpens the hit (attack / tail %.3f -> %.3f)", h0 / (t0 + 1e-12), h1 / (t1 + 1e-12));
        dw_t_ok(e1 > 1.5 * e0, "GAIN 127 louder (energy x%.2f)", e1 / (e0 + 1e-12));
    }
    printf("drumw: %s\n", dw_t_fails ? "FAIL" : "all ok");
    return dw_t_fails != 0;
}
