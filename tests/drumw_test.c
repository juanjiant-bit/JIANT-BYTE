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
        dw_t_ok(dlt(ya, yb) > 0.1, "STRUCT %s changes the hit (%.3f)", str_nth(N_DWS, k), dlt(ya, yb));
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
            dw_t_ok(dlt(ya, yb) > 0.03, "%s to %s changes the hit (%.3f)", lf ? "LFO" : "ENV", str_nth(N_DWD, k), dlt(ya, yb));
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
    {   /* (BYTE 0.16) COLOR: ATTACK (an ADSR's: the hit's first 5 ms against its tail, slower) and GAIN (clean, louder) */
        double e0, e1, h0, h1, t0, t1;
        uint32_t k, a5 = FS / 200u;
        hit_lane = 0;
        kit(); trk[0].p[P_E1] = 0; trk[0].p[P_E7] = 0; hit(ya, 0);
        kit(); trk[0].p[P_E1] = 127; hit(yb, 0);
        kit(); trk[0].p[P_E7] = 127; hit(yc, 0);
        for (h0 = h1 = t0 = t1 = e0 = e1 = 0, k = 0; k < NS; k++) {
            if (k < a5) h0 += ya[k] * ya[k], h1 += yb[k] * yb[k]; else t0 += ya[k] * ya[k], t1 += yb[k] * yb[k];
            e0 += ya[k] * ya[k]; e1 += yc[k] * yc[k];
        }
        dw_t_ok(h1 / (t1 + 1e-12) < 0.5 * h0 / (t0 + 1e-12), "ATTACK 127 a slow rise (attack / tail %.3f -> %.3f)", h0 / (t0 + 1e-12), h1 / (t1 + 1e-12));
        dw_t_ok(e1 > 1.5 * e0, "GAIN 127 louder (energy x%.2f)", e1 / (e0 + 1e-12));
    }
    {   /* (FM V10.8.32's check: DRUM-X's MATH jumped every block, up to 0.95 of the peak) DRUM-W draws its tables once
         * and reads them with a phase run on: no pad steps at a block's edge more than it moves inside one (its place
         * a block, the gains and the pitch a sample) */
        uint32_t k, i, ok = 1;
        double wj = 0;
        for (k = 0; k < 8u; k++) {
            double je = 0, ji = 0, pk;
            hit_lane = k;
            kit(); hit(ya, 0);
            pk = peak(ya, NS / CTL * CTL);
            for (i = 4u * CTL; i < NS / CTL * CTL; i++) {
                double d = fabs(ya[i] - ya[i - 1]);
                if (i % CTL == 0) { if (d > je) je = d; } else if (d > ji) ji = d;
            }
            ok &= je <= ji * 1.05 + 0.02 * pk;
            if (je / pk > wj) wj = je / pk;
        }
        hit_lane = 0;
        dw_t_ok(ok, "no pad steps at a block's edge (its largest jump there %.2f of the peak, none over one inside)", wj);
    }
    {   /* (FEEDBK, was WAVE) FEEDBK 0 bit-exact against WAVE as it was at the kit's TONE: each pad's render hashed, its
         * hashes those of the firmware before (v1.0) at WAVE 70 (the DRUM-W preset's), 64 and 100 (STRIKE 90, MORPH 20) */
        static const uint64_t GOLD[3][8] = {
            {0x1365ebc00013f563ull, 0x687dab091a2ffba9ull, 0xa43281b45cddb997ull, 0x3d88187bd5d59bc9ull, 0xfd137a5b21a063fdull, 0x644a273f70e66fd9ull, 0xfc8267eaaa1b2775ull, 0x5a39b9a72730a0e1ull},
            {0xf7194416eeb90dffull, 0x1f3ffc853588d785ull, 0x9c3ea76db7c8f5d5ull, 0x241b339a8e7cad87ull, 0x743b09652839fa7bull, 0x68fd0fee701a1759ull, 0x0df97f9f8d81f281ull, 0x80e49ad202ba7f93ull},
            {0x6f73f6bd36740c41ull, 0xdfcc3e0817bd9309ull, 0xd2370bab116c4d8full, 0xd6867aad72ef840full, 0xc872175910909d3bull, 0xe69bda11e1d9f98dull, 0xa7c810f14fb23ff5ull, 0x9d06d68caa807489ull}
        };
        static const int W[3] = {70, 64, 100}, S[3] = {0, 0, 90}, M[3] = {64, 64, 20};
        static int32_t blk[CTL];
        uint32_t c, l2, got, same = 0;
        for (c = 0; c < 3u; c++)
            for (l2 = 0; l2 < 8u; l2++) {
                uint64_t h = 1469598103934665603ull;
                memcpy(dw_kit, DW_DEF, sizeof dw_kit); coldkit(); trk[0].p[P_E2] = 0; dw_ktone = (int8_t)(W[c] - 64); trk[0].p[P_E5] = S[c]; trk[0].p[P_E0] = M[c];
                trk_note_on(&trk[0], DRUM_LANE_NOTE[l2], 110);
                for (got = 0; got < NS; got += CTL) {
                    uint32_t j;
                    track_render(&trk[0], blk, CTL);
                    for (j = 0; j < CTL; j++) { h ^= (uint32_t)blk[j]; h *= 1099511628211ull; }
                }
                same += h == GOLD[c][l2];
            }
        dw_ktone = DW_KTONE_DEF;
        dw_t_ok(same == 24u, "FEEDBK 0: every pad bit-exact with WAVE at the kit's TONE (70 / 64 / 100: %u of 24)", same);
    }
    {   /* FEEDBK 127: every pad bounded (the voice's own output inside full scale, the part's as at 0), each changed */
        static int32_t y[CTL];
        double p0, p1, d;
        uint32_t l2, got, j, inside = 1, chg = 0;
        for (l2 = 0; l2 < 8u; l2++) {
            dx_voice_t v;
            memset(&v, 0, sizeof v);
            kit();
            dx_trigger(&v);
            for (got = 0; got < NS; got += CTL) {
                dx_run_fx(&dx_kit[l2], &v, 64, 0, 0, dw_ktone, 0, 0, 0, y, CTL, dw_fb_gain(127, &v));
                for (j = 0; j < CTL; j++) inside &= y[j] >= -32767 && y[j] <= 32767;
            }
            hit_lane = l2;
            kit(); hit(ya, 0); p0 = peak(ya, NS);
            kit(); trk[0].p[P_E2] = 127; hit(yb, 0); p1 = peak(yb, NS);
            d = dlt(ya, yb);
            inside &= p1 == p1 && p1 < 1.6;   /* (the pads' bound above) */
            chg += d > 0.05;
            printf("drumw:   pad %u FEEDBK 127: peak %.3f (0: %.3f), change %.3f\n", (unsigned)l2, p1, p0, d);
        }
        hit_lane = 0;
        dw_t_ok(inside, "FEEDBK 127 bounded on every pad (its voice within +-32767, the part under 1.6 as every pad)");
        dw_t_ok(chg >= 6u, "FEEDBK 127 heard on the pads (%u of 8 changed)", chg);
        for (l2 = 0; l2 < 8u; l2 += 5u) {               /* the KICK, the TOM: brighter as it goes up, 0 < 40 < 80 < 127 */
            static const int F[4] = {0, 40, 80, 127};
            double h[4];
            uint32_t f, i;
            hit_lane = l2;
            for (f = 0; f < 4u; f++) {
                double dd = 0, ee = 0;
                kit(); trk[0].p[P_E2] = F[f]; hit(ya, 0);
                for (i = 1; i < NS; i++) dd += (ya[i] - ya[i - 1]) * (ya[i] - ya[i - 1]), ee += ya[i] * ya[i];
                h[f] = dd / (ee + 1e-12);
            }
            dw_t_ok(h[0] < h[1] && h[1] < h[2] && h[2] < h[3], "FEEDBK brightens the %s (its slope / its energy %.4f %.4f %.4f %.4f)",
                    l2 ? "TOM" : "KICK", h[0], h[1], h[2], h[3]);
        }
        hit_lane = 0;
    }
    {   /* a project of before FEEDBK (no 'D' 'B'): its DRUM part's WAVE the kit's TONE, FEEDBK 0; today's kept */
        static project_t q, r;
        static project_store_t st;
        uint32_t k, at = 0, ok;
        kit();
        dw_ktone = -20; trk[0].p[P_E2] = 50;
        project_capture(&q);
        ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw) && r.dwt == -20 && r.t[0].p[P_E2] == 50;
        for (k = 68u; k + 3u < PROJC_PFX_OFF; k++)          /* (the block: the last before the zeros) */
            if (st.raw[k] == 'D' && st.raw[k + 1u] == 'B') at = k;
        ok &= at != 0;
        for (k = 0; at && k < 3u; k++) st.raw[at + k] = 0;
        q.t[0].p[P_E2] = 0;
        {   /* (as an older firmware wrote it: WAVE 70 stored; then WAVE 64, its default then, not stored) */
            static project_store_t s2;
            uint32_t sum;
            sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u); memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
            ok &= proj_import(&r, st.raw, (int)sizeof st.raw) && r.dwt == 50 - 64 && r.t[0].p[P_E2] == 0;
            q.t[0].p[P_E2] = 0; q.dwt = 9;
            ok &= proj_pack(&s2, &q);
            for (k = 68u, at = 0; k + 3u < PROJC_PFX_OFF; k++)
                if (s2.raw[k] == 'D' && s2.raw[k + 1u] == 'B') at = k;
            for (k = 0; at && k < 3u; k++) s2.raw[at + k] = 0;
            sum = proj_hash(s2.raw, PROJ_STORE_SIZE - 4u); memcpy(s2.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
            ok &= at && proj_import(&r, s2.raw, (int)sizeof s2.raw) && r.dwt == 0 && r.t[0].p[P_E2] == 0;
        }
        dw_ktone = DW_KTONE_DEF;
        dw_t_ok(ok, "projects: the kit's TONE and FEEDBK stored ('D' 'B'); one of before: WAVE 50 -> TONE -14, 64 -> 0, FEEDBK 0");
    }
    {   /* a DRUM user preset of before: its WAVE gone, FEEDBK 0 */
        up_rec_t u;
        int16_t v[P_COUNT];
        uint32_t i;
        memset(&u, 0, sizeof u);
        u.used = UP_USED; u.ver = UP_VER; u.engine = ENGI_DRUM; u.np = P_COUNT;
        memcpy(u.name, "OLD KIT", 7);
        for (i = 0; i < P_COUNT; i++) up_set_value(&u, i, param_store_def(ENGI_DRUM, i));
        u.packed[UP_FM_AT] = UP_V04_MARK; u.packed[UP_VEL_AT] = UP_VEL_MARK; u.packed[UP_DT_AT] = UP_DT_MARK; u.packed[UP_WEST_AT] = UP_WEST_MARK;
        up_set_value(&u, P_E2, 70);
        *up_rec(9) = u;
        up_migrate(up_rec(9));
        up_values(up_rec(9), v);
        dw_t_ok(v[P_E2] == 0 && up_rec(9)->packed[UP_FB_AT] == UP_FB_MARK, "a DRUM user preset of before: WAVE 70 -> FEEDBK 0");
        u.packed[UP_FB_AT] = UP_FB_MARK;                 /* today's: kept */
        *up_rec(9) = u;
        up_migrate(up_rec(9));
        up_values(up_rec(9), v);
        dw_t_ok(v[P_E2] == 70, "a DRUM user preset of today: FEEDBK 70 kept");
        memset(up_rec(9), 0, sizeof u);
    }
    printf("drumw: %s\n", dw_t_fails ? "FAIL" : "all ok");
    return dw_t_fails != 0;
}
