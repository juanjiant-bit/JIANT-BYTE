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



int main(void){ uint32_t l,k; kit(); for(k=0;k<16;k++) dw_poll(); for(l=0;l<8;l++){kit();hit_lane=l;hit(ya,0);printf("pad %u energy %.2f peak %.3f\n",l,energy(ya,0,NS),peak(ya,NS));} return 0;}
