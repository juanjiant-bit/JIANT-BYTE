/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT FM V10.7.60, normalized) EDIT's tabs SOUND TONE FOCUS PLAY, the engine's pages by their names, MORE, a knob's
 * meaning in the header (MENU > DISPLAY > HELP), PRESETS on SEQ the step */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int nt_bad;
#define OK(c, ...) do { int ok_ = (c); printf("nav: "); printf(__VA_ARGS__); printf("    %s\n", ok_ ? "ok" : "FAIL"); nt_bad += !ok_; } while (0)
static void eng(uint32_t e) { ui_power_on(); song.sel = 0; select_engine(e); frame(); }
int main(void)
{
    eng(ENGI_WAVE); ui.home = 1; press(B_EDIT); frames(20);
    OK(str_eq(nav_page_name(), ENG_WAVE.page_title[0]), "WAVE's first page by its name (%s)", nav_page_name());
    OK(str_eq(nav_group_name(0), "SOUND") && str_eq(nav_group_name(1), "TONE") && str_eq(nav_group_name(2), "FOCUS") &&
       str_eq(nav_group_name(3), "PLAY"), "tabs SOUND TONE FOCUS PLAY");
    go_title("VOICES"); frames(20);
    OK(str_eq(nav_page_name(), "PLAY"), "VOICES shown as PLAY (%s)", nav_page_name());
    go_title("MORE"); frames(20);
    OK(str_eq(cur_page()->title, "MORE") && cur_page()->id[0] == ID_VMAX, "a synth's MORE: VOICES (UNISON VEL on PLAY)");
    eng(ENGI_LOFI); TSEL->p[P_E1] = RW_BYTE; frame(); ui.home = 1; press(B_EDIT); go_title("MORE"); frames(20);
    OK(str_eq(cur_page()->title, "MORE") && cur_page()->id[0] == P_LRTRG, "MATH BYTE's MORE: RESTART first");
    {   /* the meaning in the header */
        int any = 0, k;
        eng(ENGI_WAVE); ui.home = 1; press(B_EDIT); go_title("FILTER"); frames(20);
        for (k = 0; k < 4 && !any; k++) { ui.msg_t = 0; turn(EN_K1 + (uint32_t)k, 1); frame(); any = ui.msg_t && strstr(ui.msg, ": ") != 0; }
        OK(any, "FILTER's knob says what it does (%s)", ui.msg);
        ui_help_off = 1; ui.msg_t = 0; turn(EN_K1 + (uint32_t)(k - 1), 1); frame();
        OK(!(ui.msg_t && strstr(ui.msg, ": ")), "HELP OFF: quiet");
        ui_help_off = 0;
    }
    {   /* PRESETS on SEQ: the step */
        eng(ENGI_WAVE); ui.home = 1; press(B_SEQ); frames(20); ui.cursor = 0;
        raw_knob = 1; turn(EN_PRESET, 1); raw_knob = 0; frame();
        OK(cur_page()->fam == FAM_SEQ && ui.cursor == 1 && strstr(ui.msg, "STEP 2"), "PRESETS on SEQ: the next step (%s)", ui.msg);
    }
    printf("nav: %s\n", nt_bad ? "FAIL" : "all ok");
    return nt_bad != 0;
}
