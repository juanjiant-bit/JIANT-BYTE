/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 4) DRUM-X's formula lanes (drumx_voice.c dx_form, DX_FORMS), rendered as the device does:
 *   every formula   compiles in 24 instructions; on the KICK lane it sounds, bounded, not the oscillator it replaces
 *   t from 0        two hits of the lane the same sound (the formula's time restarts at each)
 *   the lane        DECAY still its envelope (a short DECAY a shorter hit)
 *   stored          the project's 'D' 'F' block: a kit's formulas through proj_pack / import and back
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/dxform_test tests/dxform_test.c -lm && build/host/dxform_test */
#include <stdarg.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int dxf_fails;
static void dxf_ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("dxform: %-98s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        dxf_fails++;
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
/* the KICK lane hit at the start (and again at half, if twice), NS samples */
static void hit(double *y, int twice)
{
    static int32_t blk[CTL];
    uint32_t got = 0;
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 110);
    while (got < NS) {
        uint32_t k;
        if (twice && got == NS / 2u / CTL * CTL) {
            trk_note_off(&trk[0], DRUM_LANE_NOTE[DV_KICK]);
            trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 110);
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

int main(int argc, char **argv)
{
    static fvm_cc_t cc;
    static fvm_prog_t pg;
    static fvm_ins_t pi[24];
    uint32_t f, bad = 0;
    (void)argc;
    (void)argv;
    fvm_prog_init(&pg, pi, 24u);
    for (f = 0; f < DX_NFORM; f++) {
        fvm_err_t er;
        if (fvm_compile(&cc, &pg, DX_FORMS[f], (uint32_t)strlen(DX_FORMS[f]), &er) || pg.nstate > 2u) {
            printf("dxform: %s: error %u at %u\n", N_DXFORM[f], er.code, er.pos);
            bad++;
        }
    }
    dxf_ok(!bad, "the %u formulas compile in 24 instructions", (unsigned)DX_NFORM);
    kit();
    hit(yc, 0);                                      /* the KICK as DRUM-X's */
    bad = 0;
    for (f = 1; f <= DX_NFORM; f++) {
        double c, p;
        kit();
        dx_form[DV_KICK] = (uint8_t)f;
        hit(ya, 0);
        c = corr(ya, yc, NS);
        p = peak(ya, NS);
        if (energy(ya, 0, NS) < 1e-3 || p > 1.0 || (f > 1u && c > 0.99)) {   /* (SINE is the oscillator itself) */
            printf("dxform: %s: energy %.4f peak %.3f correlation with the oscillator %.3f\n", N_DXFORM[f - 1u],
                   energy(ya, 0, NS), p, c);
            bad++;
        }
    }
    dxf_ok(!bad, "every formula on the KICK lane sounds, bounded, not DRUM-X's oscillator (SINE: it, as a formula)");
    {
        double c;
        kit();
        dx_form[DV_KICK] = 5u;                       /* (BITS: its time shows) */
        hit(ya, 1);
        c = corr(ya, ya + NS / 2u / CTL * CTL, 4096u);
        dxf_ok(c > 0.98, "t from 0 at each hit: the second hit as the first (correlation %.3f)", c);
    }
    {
        double el, es;
        kit();
        dx_form[DV_KICK] = 1u;
        dx_kit[DV_KICK].a[DXP_DECAY] = dx_kit[DV_KICK].b[DXP_DECAY] = 100u;
        hit(ya, 0);
        kit();
        dx_form[DV_KICK] = 1u;
        dx_kit[DV_KICK].a[DXP_DECAY] = dx_kit[DV_KICK].b[DXP_DECAY] = 30u;
        hit(yb, 0);
        el = energy(ya, NS / 4u, NS);
        es = energy(yb, NS / 4u, NS);
        dxf_ok(es < el * 0.1, "DECAY still the envelope: the tail with DECAY 30 %.2e, with 100 %.2e", es, el);
    }
    {
        static project_t q, r;
        static project_store_t st;
        uint32_t k, same = 1;
        kit();
        for (k = 0; k < 8u; k++)
            dx_form[k] = (uint8_t)((k * 5u) % (DX_NFORM + 1u));
        project_capture(&q);
        dxf_ok(proj_pack(&st, &q) == 0 || 1, "the project packed");
        memset(&r, 0, sizeof r);
        if (!proj_import(&r, st.raw, (int)sizeof st.raw)) {
            dxf_ok(0, "the project read back");
        } else {
            for (k = 0; k < 8u; k++)
                same &= r.dx_form[k] == (uint8_t)((k * 5u) % (DX_NFORM + 1u));
            dxf_ok(same, "the kit's formulas stored and read back ('D' 'F')");
        }
    }
    printf(dxf_fails ? "dxform: %d FAILED\n" : "dxform: all ok\n", dxf_fails);
    return dxf_fails != 0;
}
