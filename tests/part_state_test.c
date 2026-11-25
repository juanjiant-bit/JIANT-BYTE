/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (the owner: "la idea es poder editar on the fly las secuencias") HOME > PARTS KNOB 4 STATE (song_chain.c PST_*): a
 * song A B A B of one bar each, a track's PAN (a value the part keeps) and DELAY (its sound's) moved while A plays.
 * LOCK: each part's start its stored values (as ever); JAM: none, the values as they were left; OVER: at the end of
 * A's pass its values into A (B's start B's own; A again what A ended with; B's change B's), the marker; the STATE
 * kept with the song (an older song's byte: LOCK) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void make_song(void)
{
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    gp.genre = 1; gp.seed = 31; gp.energy = 60; gp.form = 2;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    for (unsigned k = 0; k < GP_N; ++k) gp.amt[k] = GA_AUTO;
    gen_go();
}
static int next_row(void)                               /* bars until the row changes; 0: it never did */
{
    uint32_t last = chain.row, bar;
    for (bar = 0; bar < 80u; bar++) {
        events_block(chain_bar());
        song_poll();
        if (chain.row != last)
            return 1;
    }
    return 0;
}
static int slot_put(uint32_t s, uint32_t k, int pan, int dly)   /* part s's track k: its PAN and DELAY stored */
{
    if (!proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t))) return 0;
    proj_scratch.t[k].p[P_PAN] = (int16_t)pan; proj_scratch.t[k].p[P_DLY] = (int16_t)dly;
    if (!proj_pack(&proj_wire, &proj_scratch)) return 0;
    memcpy(&proj_slot[s], &proj_wire, sizeof proj_slot[s]);
    return 1;
}
static uint32_t slot_eng(uint32_t s, uint32_t k)
{
    return proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t)) ? eng_live(proj_scratch.t[k].engine) : 255u;
}
/* the song A B A B (forever), mode m, track k found (a synth, the same engine in A and B) */
static uint32_t setup(uint32_t m)
{
    uint32_t k, r;
    make_song();
    for (k = 0; k < NTRK; k++)
        if (slot_eng(0, k) == slot_eng(1, k) && slot_eng(0, k) != ENGI_DRUM && slot_eng(0, k) != ENGI_INPUT && slot_eng(0, k) < 255u)
            break;
    if (k >= NTRK || !slot_put(0, k, -30, 10) || !slot_put(1, k, 30, 90)) return NTRK;
    chain_defaults(&chain_config);
    chain_config.count = 4;
    for (r = 0; r < 4u; r++) { chain_config.row[r].slot = (uint8_t)(r & 1u); chain_config.row[r].bars = 1; }
    SONG_END(&chain_config) = SONG_FOREVER;
    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
    memset(song_idx.tsrc[song_cur], 0, sizeof song_idx.tsrc[song_cur]);
    memset(song_idx.tvar[song_cur], 0, sizeof song_idx.tvar[song_cur]);
    if (section_load(0)) return NTRK;                   /* (the music A as stored: every mode starts from it) */
    live.pst = (uint8_t)m; live.mode = 1; pst_t = 0xDEADBEEFu;
    press(B_PLAY); events_block(CTL); song_poll(); frame();
    return chain.running && chain.row == 0u ? k : NTRK;
}

int main(void)
{
    int bad = 0, ok;
    uint32_t k;
    int16_t pan[4], dly[4];
    uint8_t mk = 0;
    static const char *const NM[3] = {"LOCK", "JAM", "OVER"};
    for (uint32_t m = PST_LOCK; m <= PST_OVER; m++) {
        uint32_t r;
        k = setup(m);
        ok = k < NTRK && trk[k].p[P_PAN] == -30 && trk[k].p[P_DLY] == 10;   /* (A as stored) */
        if (k >= NTRK) { printf("  %s: no song\n", NM[m]); bad++; continue; }
        trk[k].p[P_PAN] = 50; trk[k].p[P_DLY] = 60;     /* the player, in A */
        for (r = 0; r < 4u; r++) {
            ok &= next_row();
            pan[r] = trk[k].p[P_PAN]; dly[r] = trk[k].p[P_DLY];
            if (r == 0u) { mk = pst_t != 0xDEADBEEFu; trk[k].p[P_PAN] = 5; }   /* .. and in B */
        }
        printf("  %s: B %d/%d, A %d/%d, B %d/%d, A %d (PAN/DELAY; A was -30/10, B 30/90, moved to 50/60 in A, PAN 5 in B)\n",
               NM[m], pan[0], dly[0], pan[1], dly[1], pan[2], dly[2], pan[3]);
        if (!ok) printf("  %s: a row did not change (row %u)\n", NM[m], chain.row);
        if (m == PST_LOCK)
            bad += check("STATE LOCK: each part's start its stored values (B's PAN, A's again: as before)",
                         ok && pan[0] == 30 && pan[1] == -30 && pan[2] == 30 && pan[3] == -30);
        else if (m == PST_JAM)
            bad += check("STATE JAM: no start recalls anything (the PAN and DELAY as left, through B and A)",
                         ok && pan[0] == 50 && dly[0] == 60 && pan[1] == 5 && dly[1] == 60 && pan[2] == 5 && pan[3] == 5);
        else {
            bad += check("STATE OVER: A's end stored into A: B its own (30/90), A again what it ended with (50/60), B its own change (5)",
                         ok && pan[0] == 30 && dly[0] == 90 && pan[1] == 50 && dly[1] == 60 && pan[2] == 5 && dly[2] == 90 && pan[3] == 50);
            ok = mk && proj_import(&proj_scratch, &proj_slot[0], sizeof(project_store_t)) &&
                 proj_scratch.t[k].p[P_PAN] == 50 && proj_scratch.t[k].p[P_DLY] == 60 &&
                 proj_import(&proj_scratch, &proj_slot[1], sizeof(project_store_t)) && proj_scratch.t[k].p[P_PAN] == 5;
            bad += check("STATE OVER: the parts hold the values they ended with (RAM), KNOB 4's mark as it stored", ok);
        }
        seq_stop(); song_poll(); frame();
    }
    {   /* kept with the song: the mode byte; an older song's (0xA1, 0): LOCK */
        song_draft = 0; live.pst = PST_OVER; song_poll();
        ok = (chain_config.rsv[2] & 0xF8u) == SONG_MODE_TAG && (chain_config.rsv[2] >> 1 & 3u) == PST_OVER;
        live.pst = PST_JAM; song_mode_restore(); ok &= live.pst == PST_OVER;
        chain_config.rsv[2] = SONG_MODE_TAG | 1u; song_mode_restore(); ok &= live.pst == PST_LOCK;
        live.pst = PST_JAM; chain_config.rsv[2] = 0; song_mode_restore(); ok &= live.pst == PST_LOCK;
        if (!ok) printf("  rsv[2] %02x pst %u\n", chain_config.rsv[2], live.pst);
        bad += check("STATE: kept with the song's rows (rsv[2] bits 1, 2); older songs LOCK", ok);
    }
    {   /* the page: KNOB 4 STATE LOCK / JAM / OVER */
        char v[16];
        nav_put(SEC_HOME, "PARTS"); frames(60);
        live.pst = 0;
        turn(EN_K4, 1); frame(); ok = live.pst == PST_JAM && !strncmp(ui.col[3], "STATE|JAM", 9);
        turn(EN_K4, 4); frame(); ok &= live.pst == PST_OVER && !strncmp(ui.col[3], "STATE|OVER", 10);
        turn(EN_K4, -4); frame(); ok &= live.pst == PST_LOCK && !strncmp(ui.col[3], "STATE|LOCK", 10);
        live.pst = PST_OVER; pst_t = fm1_ms; ui.force = 1; frame(); ok &= !strncmp(ui.col[3], "STATE|OVER \x85", 11);   /* (just stored) */
        pst_t = fm1_ms - 5000u; ui.force = 1; frame(); ok &= !strcmp(ui.col[3] + 6, "OVER") || !strncmp(ui.col[3], "STATE|OVER|", 11);
        live.pst = PST_LOCK;
        (void)v;
        if (!ok) printf("  col 3 '%s'\n", ui.col[3]);
        bad += check("STATE: HOME > PARTS KNOB 4 LOCK / JAM / OVER, shown; OVER's mark as it stores", ok);
    }
    printf(bad ? "part-state: %d FAILED\n" : "part-state: all ok\n", bad);
    return bad != 0;
}
