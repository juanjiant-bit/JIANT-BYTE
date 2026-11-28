/* SPDX-License-Identifier: GPL-3.0-only */
/* (BYTE) the matrix's names in full (ui_input.c MSRC_FULL MDST_FULL mod_full) and their card (mod_card): every SRC and
 * DEST has one on every engine, the engine's own named by the engine, F.NOISE stepped over by a SRC turn, the card
 * shown on LFO > MATRIX with HELP OFF too */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static int starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
int main(void)
{
    char b[28], w[40];
    uint32_t e, v;
    ui_power_on();
    song.sel = 0;
    {   /* (str_nth tables) a full name for every SRC and DEST, no more: the last ones' and nothing after them */
        const char *ls = str_nth(MSRC_FULL, MS_N - 1u), *ld;   /* (packed, text_z.h: one at a time, gz.h's buffer) */
        assert(!strncmp(ls, "FUNC BAR", 8) && ls + strlen(ls) + 1u == MSRC_FULL + MSRC_FULL_N);
        ld = str_nth(MDST_FULL, MD_N - 1u);
        assert(!strncmp(ld, "FUNC RANGE", 10) && ld + strlen(ld) + 1u == MDST_FULL + MDST_FULL_N);
    }
    for (v = 0; v < MS_N; v++) {
        mod_full(TSEL, 1u, (int32_t)v, b, sizeof b, w, sizeof w);
        assert(b[0] && !strchr(b, '|'));
    }
    for (e = 0; e < NENGINES; e++) {
        if (ENGINES[e]->name[0] == '-' || !strcmp(ENGINES[e]->name, "-"))
            continue;
        select_engine(e);
        frame();
        for (v = 0; v < MD_N; v++) {
            mod_full(TSEL, 0u, (int32_t)v, b, sizeof b, w, sizeof w);
            if (!b[0]) { printf("engine %s DEST %u (%s): no full name\n", ENGINES[e]->name, v, N_MDST[v]); return 1; }
        }
    }
    select_engine(ENGI_WAVE);
    frame();
    mod_full(TSEL, 0u, MD_E1 + 1, b, sizeof b, 0, 0);
    if (strcmp(b, "WAVE POS")) { printf("WAVE E2: %s\n", b); return 1; }
    mod_full(TSEL, 1u, MS_MODW, b, sizeof b, w, sizeof w);
    assert(!strcmp(b, "MOD WHEEL") && !strcmp(w, "MIDI CC1"));
    mod_full(TSEL, 1u, MS_RAND, b, sizeof b, w, sizeof w);
    assert(!strcmp(b, "RANDOM") && !strcmp(w, "NEW VALUE EACH NOTE"));
    puts("modnames: every SRC and DEST has a full name on every engine PASS");
    assert(mod_src_turn(MS_FNZ - 1, MS_FNZ) == MS_FNZ + 1 && mod_src_turn(MS_FNZ + 1, MS_FNZ) == MS_FNZ - 1 &&
           mod_src_turn(MS_FNZ, MS_FNZ) == MS_FNZ);
    ui_help_off |= 1u;                                  /* (HELP OFF: the matrix's card all the same) */
    assert(nav_put(SEC_LFO, "MATRIX") && cur_page()->graph == GR_MOD);
    frame();
    mod_ui_slot = 0;
    TSEL->p[P_M1SRC] = MS_RAND;
    TSEL->p[P_M1DST] = MD_E1 + 1;
    turn(EN_K2, 1);                                     /* SRC: RAND -> MODW */
    if (TSEL->p[P_M1SRC] != MS_MODW || strcmp(ui.msg, "SOURCE: MOD WHEEL") || strcmp(ui.msg2, "MIDI CC1")) {
        printf("SRC card: %d '%s' '%s'\n", TSEL->p[P_M1SRC], ui.msg, ui.msg2); return 1;
    }
    turn(EN_K3, -1);                                    /* DEST: E2 -> E1 (WAVE's A) */
    if (!starts(ui.msg, "DEST: WAVE ") || strcmp(ui.msg2, "MOD WHEEL > WAVE A")) {
        printf("DEST card: '%s' '%s'\n", ui.msg, ui.msg2); return 1;
    }
    TSEL->p[P_M1DST] = MD_PAN;
    mod_card(2u);                                       /* (DEST: the route in full, after) */
    if (strcmp(ui.msg, "DEST: PAN") || strcmp(ui.msg2, "MOD WHEEL > PAN")) { printf("PAN card: '%s' '%s'\n", ui.msg, ui.msg2); return 1; }
    TSEL->p[P_M1DST] = MD_DLY;
    mod_card(2u);
    if (strcmp(ui.msg, "DEST: DELAY SEND") || strcmp(ui.msg2, "MOD WHEEL > DELAY SEND")) { printf("DLY card: '%s' '%s'\n", ui.msg, ui.msg2); return 1; }
    TSEL->p[P_M1SRC] = 0;
    mod_card(2u);
    if (ui.msg2[0]) { printf("no route: '%s'\n", ui.msg2); return 1; }
    TSEL->p[P_M1SRC] = MS_OCTDN;
    turn(EN_K2, 1);                                     /* OCT- -> (F.NOISE stepped over) LIFE */
    if (TSEL->p[P_M1SRC] != MS_LIFE) { printf("F.NOISE not stepped over: %d\n", TSEL->p[P_M1SRC]); return 1; }
    turn(EN_K2, -1);
    assert(TSEL->p[P_M1SRC] == MS_OCTDN);
    puts("modnames: the card on SRC / DEST turns, HELP OFF too, F.NOISE hidden PASS");
    return 0;
}
