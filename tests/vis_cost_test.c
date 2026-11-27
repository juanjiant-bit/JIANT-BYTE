/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (VIS lite) the UI's cost a frame, page by page, under valgrind's callgrind (tests/vis_cost_test.py): the heaviest GEN
 * song playing (cpu_lines' scene 101: EXPLORE s123, CLOUD, the delay and the chorus), every page of every section (EDIT's
 * for each part), HOME > VIS and MENU > SAVER's visual: WARM frames drawn, then FRAMES collected (ui_draw only: the
 * audio between them is not), their counts dumped at "PAGE <name>" (VIS: "VIS <frames drawn>", its every 2nd frame).
 * Natively (no valgrind) it checks VIS's segments a frame: never past VIS_SEG, some drawn */
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <valgrind/callgrind.h>
#define WARM 3u
#define FRAMES 6u
static int32_t vo[2 * CTL];
static uint32_t seg_max, seg_min = ~0u;
static void vframe(int on)
{
    uint32_t t = vg.t;
    fm1_ms += 16u;
    mix_block(vo, CTL);
    math_poll(); dw_poll();
    if (on) CALLGRIND_TOGGLE_COLLECT;
    ui_draw();
    if (on) CALLGRIND_TOGGLE_COLLECT;
    if (vg.t != t && (scrn.st == SCR_VIS || home_vis())) {   /* (a VIS frame drawn: its segments) */
        seg_max = vg.ns > seg_max ? vg.ns : seg_max;
        seg_min = vg.ns < seg_min ? vg.ns : seg_min;
    }
}
static void page(const char *kind, uint32_t sec)          /* (kind: VIS's, else a page) */
{
    uint32_t i, t, drawn = 0;
    char nm[96];
    ui.force = 1;
    for (i = 0; i < WARM; i++) vframe(0);
    for (i = 0; i < FRAMES; i++) {
        t = vg.t;
        vframe(1);
        drawn += vg.t != t;
    }
    if (kind) snprintf(nm, sizeof nm, "VIS %s %u", kind, drawn);
    else snprintf(nm, sizeof nm, "PAGE %u %s %s", song.sel, str_nth(SEC_NAME, sec), nav_page_name());
    CALLGRIND_DUMP_STATS_AT(nm);
}
int main(void)
{
    uint32_t k, s, j, n, t;
    uint8_t v[NAV_MAX];
    mc_host = 1;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    gp.genre = GEN_EXPLORE; gp.seed = 123; gp.energy = 85; gp.form = 0;
    gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
    for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    life_v[LIFE_AMT] = 100;
    gen_go();
    for (k = 0; k < 64u; k++) { mix_block(vo, CTL); math_poll(); dw_poll(); }
    song.g[G_RTYPE] = 4; song.g[G_DFDBK] = 70; song.g[G_DTIME] = DT_FREE + 40;
    for (k = 0; k < NTRK; k++) { trk[k].p[P_DLY] = 70; trk[k].p[P_REV] = 80; trk[k].p[P_CHOR] = 70; }
    song.master_q12 = 4096;
    transport_req = 1; ui_input();
    for (k = 0; k < 200u; k++) vframe(0);
    for (t = 0; t < trk_live; t++)                      /* every page (EDIT's for each part; the others for the first) */
        for (s = 0; s < SEC_N; s++) {
            if (t && s != SEC_EDIT) continue;
            song.sel = (uint8_t)t;
            nav_go(s, 0, 0);
            n = nav_list(s, v);
            for (j = 0; j < n; j++) {
                nav_go(s, v[j], 0);
                if (home_vis()) continue;
                page(0, s);
            }
        }
    song.sel = 0;
    nav_go(SEC_HOME, HE_VIS, 0);
    page("HOME", 0);
    for (k = 0; k < 120u; k++) vframe(0);                /* (a while: the cuts, the sound) */
    page("HOME-LATER", 0);
    nav_go(SEC_HOME, HE_MACROS, 0);
    vframe(0);
    scrn.st = SCR_VIS;
    page("SAVER", 0);
    scrn.st = SCR_ON;
    printf("vis_cost: VIS segments a frame %u .. %u (cap %u)\n", seg_min, seg_max, VIS_SEG);
    return seg_max > VIS_SEG || seg_min < 8u;
}
