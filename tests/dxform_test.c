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
        dxf_ok(proj_pack(&st, &q), "the project packed");
        memset(&r, 0, sizeof r);
        if (!proj_import(&r, st.raw, (int)sizeof st.raw)) {
            dxf_ok(0, "the project read back");
        } else {
            for (k = 0; k < 8u; k++)
                same &= r.dx_form[k] == (uint8_t)((k * 5u) % (DX_NFORM + 1u));
            dxf_ok(same, "the kit's formulas stored and read back ('D' 'F')");
        }
    }
    {   /* (JIANT BYTE) the floatbeat factory: each lane its own (DX_FORM_DEF), on its own lane: sounding, bounded, as loud as
         * DRUM-X's own oscillator there within x1/3 .. x3 (energy x1/9 .. x9) */
        uint32_t l;
        bad = 0;
        for (l = 0; l < 8u; l++) {
            double e0, e1;
            hit_lane = l;
            memset(dx_form, 0, sizeof dx_form);
            kit(); hit(ya, 0);
            dx_form[l] = DX_FORM_DEF[l];
            kit(); hit(yb, 0);
            e0 = energy(ya, 0, NS);
            e1 = energy(yb, 0, NS);
            printf("dxform: lane %u %-6s energy x%.2f of its oscillator's, peak %.3f\n", l, N_DXFORM[DX_FORM_DEF[l] - 1u], e1 / (e0 > 0 ? e0 : 1e-9), peak(yb, NS));
            if (!(e1 > e0 / 9 && e1 < e0 * 9) || peak(yb, NS) > 1.0) bad++;
        }
        hit_lane = 0;
        dxf_ok(!bad, "the floatbeat kit: every lane sounds near its oscillator's level, bounded (%u off)", bad);
    }
    {
        /* Every lane of FLOAT KIT comes from its formula, including CLAP whose classic noise mix is 127. */
        bad = 0;
        for (uint32_t l = 0; l < 8u; l++) {
            coldkit(); dx_kit_load(DX_NKIT); hit_lane = l;
            hit(ya, 0);
            if(energy(ya, 0, NS) < 1e-3 || peak(ya, NS) > 1.0) {printf("dxform: FLOAT lane %u silent or high peak %g\n",l,peak(ya,NS));bad++;}
            for (uint32_t m = 0; m < DFM_N; m++) {
                coldkit(); dx_kit_load(DX_NKIT);
                dx_fmod[l][m] = m == DFM_RATE ? 12 : m == DFM_BEND ? 90 : 12;
                hit(yb, 0);
                double delta = 0;
                for (uint32_t j = 0; j < NS; j++) delta += (ya[j]-yb[j])*(ya[j]-yb[j]);
                if (!(delta > 1e-5 && peak(yb, NS) <= 1.0)) {
                    printf("dxform: lane %u modifier %u delta %g peak %g\n", l,m,delta,peak(yb,NS)); bad++;
                }
                coldkit(); dx_kit_load(DX_NKIT); dx_fmod[l][m] = m == DFM_RATE ? 12 : m == DFM_BEND ? 90 : 12;
                hit(yc, 0);
                if(memcmp(yb,yc,sizeof yb)) {printf("dxform: lane %u modifier %u not deterministic\n",l,m);bad++;}
            }
        }
        hit_lane = 0;
        memset(dx_fmod, 0, sizeof dx_fmod);
        dxf_ok(!bad, "FLOAT KIT: all eight lanes sound; RATE/BEND/BITS each audible, bounded and deterministic");
    }
    {
        static project_t q, r;
        static project_store_t st;
        kit(); dx_kit_load(DX_NKIT);
        for (uint32_t l=0; l<8u; l++) {
            dx_fmod[l][0] = (int8_t)l-4; dx_fmod[l][1] = (int8_t)(l*15); dx_fmod[l][2] = (int8_t)l;
        }
        project_capture(&q);
        q.bal=1; q.dnz[1]=42; q.cru[2]=80; q.life[0]=-1; /* all preceding optional XM/XC/XL blocks */
        int packed=proj_pack(&st,&q), read=packed && proj_import(&r,st.raw,sizeof st.raw);
        dxf_ok(read && !memcmp(q.dx_fmod,r.dx_fmod,sizeof q.dx_fmod) && r.dnz[1]==42 && r.life[0]==-1,
               "DQ modifiers round trip after all optional effects/LIFE blocks");
        if (read) {
            uint32_t at=0;
            for(uint32_t j=68;j+2+sizeof q.dx_fmod<PROJ_STORE_SIZE;j++)
                if(st.raw[j]=='D' && st.raw[j+1]=='Q' && !memcmp(st.raw+j+2,q.dx_fmod,sizeof q.dx_fmod)) at=j;
            dxf_ok(at!=0,"DQ extension found at the tail");
            if(at) {
                memset(st.raw+at,0,2+sizeof q.dx_fmod);
                uint32_t sum=proj_hash(st.raw,PROJ_STORE_SIZE-4); memcpy(st.raw+PROJ_STORE_SIZE-4,&sum,4);
                memset(&r,0x7f,sizeof r);
                int ok=proj_import(&r,st.raw,sizeof st.raw); uint32_t nonzero=0;
                for(uint32_t j=0;j<sizeof r.dx_fmod;j++)nonzero|=((uint8_t*)r.dx_fmod)[j];
                dxf_ok(ok && !nonzero && r.cru[2]==80,"older project without DQ loads neutral modifiers, other fields intact");
            }
        }
        memset(dx_fmod,0,sizeof dx_fmod);
    }
    {
        kit(); song.sel=0; dx_ui_lane=0;
        go_title("FORMULA");
        dxf_ok(page_visible(ui.page) && dx_pagen(cur_page())==9,"FORMULA page available on DRUM");
        dx_form[0]=0; dx_knob(cur_page(),1,1);
        dxf_ok(dx_form[0]==1,"FORMULA selects the first formula directly from CLASSIC");
        go_title("FORM MOD");
        for(int j=0;j<150;j++)for(int k=1;k<4;k++)dx_knob(cur_page(),k,1);
        dxf_ok(dx_fmod[0][0]==24 && dx_fmod[0][1]==127 && dx_fmod[0][2]==12,"modifier UI upper bounds");
        for(int j=0;j<180;j++)for(int k=1;k<4;k++)dx_knob(cur_page(),k,-1);
        dxf_ok(dx_fmod[0][0]==-24 && !dx_fmod[0][1] && !dx_fmod[0][2],"modifier UI lower bounds");
        memset(dx_fmod,0,sizeof dx_fmod);
    }
    printf(dxf_fails ? "dxform: %d FAILED\n" : "dxform: all ok\n", dxf_fails);
    return dxf_fails != 0;
}
