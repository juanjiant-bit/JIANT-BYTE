/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE, JIANT FM V10.7.66's harmony_test, ported) SCL > HARMONY: a progression every melodic source follows. A track
 * repeating C plays the progression's roots (in its scale; a scale of other than seven: the major scale's intervals),
 * chord by chord on LENGTH; SKIP's track does not move; a kit never; OFF or stopped as before; the project keeps it
 * ('H' 'M'), a project without it loads OFF; the page and its knobs; EUCLID's CHORDS on the key's own chord (moved at
 * playback); FUNC's and DRONE CLAY's root the chord's unless a root key is held; GEN's family progressions (FM
 * V10.8.34) among PROG's, thrown. Run by tests/run_byte_tests.sh */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok;
    uint32_t i, k, hits, fam;
    ui_power_on();
    trk[1].p[P_ROOT] = 0; trk[1].p[P_SCALE] = 1;      /* (C major) */
    song.g[G_HPROG] = 1 + PR_50S; song.g[G_HLEN] = 2; song.g[G_HEVO] = 0; song.g[G_HSKIP] = 0;
    hm_load();
    song.playing = 1;
    ok = hm_n == 4u;
    bar_n = 0; beat_n = 0; ok &= hm_shift(&trk[1]) == 0;   /* I */
    bar_n = 1; ok &= hm_shift(&trk[1]) == 5;                /* vi */
    bar_n = 2; ok &= hm_shift(&trk[1]) == 3;                /* IV */
    bar_n = 3; beat_n = 3; ok &= hm_shift(&trk[1]) == 4;    /* V (the bar's last beat still V) */
    bar_n = 4; beat_n = 0; ok &= hm_shift(&trk[1]) == 0;    /* round */
    ok &= hm_move(&trk[1], 60, hm_shift(&trk[1])) == 60;
    bar_n = 1; ok &= hm_move(&trk[1], 60, hm_shift(&trk[1])) == 69;   /* (C up six scale steps: A) */
    ok &= hm_move(&trk[1], 64, hm_shift(&trk[1])) == 72;   /* (E: C, the chord's third) */
    bad += check("HARMONY 50S: I vi IV V a bar each; C follows as C A F G", ok);
    trk[1].p[P_SCALE] = 0; hm_load();                   /* (CHR: the line moved by the major scale's intervals) */
    ok = hm_move(&trk[1], 61, hm_shift(&trk[1])) == 70;
    bar_n = 2; ok &= hm_move(&trk[1], 61, hm_shift(&trk[1])) == 66;
    trk[1].p[P_SCALE] = 1; hm_load();
    bad += check("HARMONY on a chromatic track: the major scale's intervals (vi +9, IV +5)", ok);
    song.g[G_HLEN] = 0; bar_n = 0; beat_n = 1;
    ok = hm_shift(&trk[1]) == 5;                        /* (1/4: a chord a beat) */
    song.g[G_HSKIP] = 2;                                /* T2 skips */
    ok &= hm_shift(&trk[1]) == 0 && hm_shift(&trk[0]) == 5;
    song.g[G_HSKIP] = 0;
    song.playing = 0; ok &= hm_shift(&trk[1]) == 0;     /* (stopped: the notes as written) */
    song.playing = 1;
    song.g[G_HPROG] = 0; hm_load();
    ok &= hm_n == 0u && hm_shift(&trk[1]) == 0;
    bad += check("HARMONY: LENGTH 1/4 a chord a beat; SKIP T2 stays; stopped or OFF nothing", ok);
    {   /* EVOLVE: now and then a chord a fourth or a third up, never on the first time round */
        uint32_t moved = 0;
        song.g[G_HPROG] = 1 + PR_50S; song.g[G_HLEN] = 2; song.g[G_HEVO] = 127; hm_load();
        for (bar_n = 0, beat_n = 0, ok = 1; bar_n < 4u; bar_n++) ok &= hm_shift(&trk[1]) == hm_dg[bar_n];
        for (bar_n = 4; bar_n < 64u; bar_n++) moved += hm_shift(&trk[1]) != hm_dg[bar_n % 4u];
        ok &= moved >= 50u;
        song.g[G_HEVO] = 0;
        bad += check("HARMONY EVOLVE: the first time round as written, then its substitutions", ok);
    }
    {   /* FUNC and DRONE CLAY's root: the chord's (vi: +9), unless a root key is held on the part selected */
        song.g[G_HPROG] = 1 + PR_50S; hm_load(); bar_n = 1; beat_n = 0;
        ok = hm_semi(&trk[1]) == 9;
        bar_n = 3; ok &= hm_semi(&trk[1]) == 7;
        song.sel = 1; ok &= !hm_rootkey(1);
        kb_on[22] = 1; ok &= hm_rootkey(1) && !hm_rootkey(2);
        kb_on[22] = 0;
        bad += check("HARMONY: FUNC's and CLAY's root the chord's (vi +9, V +7), a root key held its own", ok);
    }
    song.playing = 0; bar_n = 0;
    {   /* the sequencer: a step of C and E, every 1/16, plays C E, A C, F A, G B bar by bar; a kit's steps as written */
        track_t *t = &trk[1];
        uint8_t seen[4] = {0}, s2[4] = {0};
        set_engine_of(t, ENGI_ANALOG);
        t->p[P_VOICE] = V_POLY; t->p[P_SLEN] = 16; t->p[P_SCALE] = 1; t->p[P_ROOT] = 0; song.g[G_BPM] = 240;
        for (i = 0; i < 16u; i++) t->step[i] = (step_t){.note = {60, 64}, .n = 2, .time = ST_NOTE, .vel = 100};
        for (i = 0; i < NTRK; i++) trk[i].seq_active = i == 1u;
        t->seq_active = 1;
        seq_start();
        for (k = 0; k < 4000u && bar_n < 4u; k++) {
            events_block(CTL);
            if (bar_n < 4u && t->seq_n == 2u) { seen[bar_n] = t->seq_notes[0]; s2[bar_n] = t->seq_notes[1]; }   /* (its last step's) */
        }
        seq_stop();
        ok = seen[0] == 60 && s2[0] == 64 && seen[1] == 69 && s2[1] == 72 && seen[2] == 65 && s2[2] == 69 && seen[3] == 67 && s2[3] == 71;
        if (!ok) printf("  played: %u %u, %u %u, %u %u, %u %u\n", seen[0], s2[0], seen[1], s2[1], seen[2], s2[2], seen[3], s2[3]);
        bad += check("HARMONY 50S on the sequencer: C E plays C E, A C, F A, G B", ok);
        for (i = 0; i < 16u; i++) step_clear(&t->step[i]);
        song.g[G_BPM] = 120; bar_n = 0;
    }
    {   /* EUCLID's CHORDS with HARMONY on: the key's own triad (C E G) though a neighbour plays A minor */
        track_t *m = &trk[1], *nb = &trk[2];
        uint32_t c, n = 0, ok2 = 1;
        set_engine_of(m, ENGI_ANALOG); set_engine_of(nb, ENGI_ANALOG);
        memset(eu, 0, sizeof eu);
        for (i = 0; i < NSTEP; i++) { step_clear(&m->step[i]); step_clear(&nb->step[i]); }
        m->p[P_SLEN] = 16; nb->p[P_SLEN] = 16; nb->p[P_SCALE] = 1; nb->p[P_ROOT] = 0;
        for (i = 0; i < 16u; i += 2u) { nb->step[i].time = ST_NOTE; nb->step[i].n = 1; nb->step[i].note[0] = (uint8_t)(i & 4u ? 64 : 57); nb->step[i].vel = 100; }
        song.sel = 1; frame();
        go_title("EUCLID"); frame();
        eu_put(EK_HITS, 8); eu_put(EK_M0, 6); eu_put(EK_M0 + 2u, 0); eu_put(EK_K4, 1); frame();   /* RANDOM, CHORDS */
        for (i = 0; i < 16u; i++) {
            const step_t *s = &m->step[i];
            if (s->time != ST_NOTE) continue;
            n++;
            ok2 &= s->n >= 3u;
            for (c = 0; c < 3u; c++) ok2 &= s->note[c] % 12u == 0u || s->note[c] % 12u == 4u || s->note[c] % 12u == 7u;
        }
        bad += check("HARMONY on: EUCLID's CHORDS the key's own triad (moved onto the progression at playback)", ok2 && n == 8u);
        for (i = 0; i < NSTEP; i++) { step_clear(&m->step[i]); step_clear(&nb->step[i]); }
    }
    {   /* the project keeps it; a project without the block loads OFF */
        song.g[G_HPROG] = 1 + PR_JAZZ2; song.g[G_HLEN] = 3; song.g[G_HEVO] = 90; song.g[G_HSKIP] = 3;
        ok = !project_save(1);
        song.g[G_HPROG] = 0; song.g[G_HLEN] = 2; song.g[G_HEVO] = 0; song.g[G_HSKIP] = 0;
        project_load(1); frames(64);
        ok &= song.g[G_HPROG] == 1 + PR_JAZZ2 && song.g[G_HLEN] == 3 && song.g[G_HEVO] == 90 && song.g[G_HSKIP] == 3;
        ok &= hm_n == 4u && hm_dg[0] == 2;
        song.g[G_HPROG] = 0; song.g[G_HLEN] = 2; song.g[G_HEVO] = 0; song.g[G_HSKIP] = 0;
        ok &= !project_save(2);
        song.g[G_HPROG] = 5; song.g[G_HLEN] = 4; song.g[G_HEVO] = 9; song.g[G_HSKIP] = 1;
        project_load(2); frames(64);
        ok &= song.g[G_HPROG] == 0 && song.g[G_HLEN] == 2 && song.g[G_HEVO] == 0 && song.g[G_HSKIP] == 0 && hm_n == 0u;
        bad += check("HARMONY kept with the project; one saved without it loads OFF", ok);
    }
    {   /* SCL > HARMONY: its page and knobs */
        song.g[G_HPROG] = 0;
        go_title("HARMONY"); frames(32);
        turn(EN_K1, 3); frames(32);
        ok = str_eq(cur_page()->title, "HARMONY") && song.g[G_HPROG] == 3 && hm_n == PROG[2].n;
        turn(EN_K2, -1); turn(EN_K4, 2); frames(32);
        ok &= song.g[G_HLEN] == 1 && song.g[G_HSKIP] == 2;
        song.g[G_HPROG] = 0; song.g[G_HLEN] = 2; song.g[G_HSKIP] = 0; frames(32);
        bad += check("SCL > HARMONY: the page, KNOB 1 the progression, 2 LENGTH, 4 SKIP", ok);
    }
    {   /* (FM V10.8.34) GEN: the harmony families' own progressions (PROG's PR_FAM ..) thrown now and then */
        ok = PROG[PR_FAM].n == 4u && PROG[PR_FAM + 4].deg[1] == 4 && PROG[PR_FAM + 11].deg[0] == 1 &&
             str_eq(N_HPROG[1 + PR_FAM + 4], "4CHRD") && str_eq(N_HPROG[1 + PR_NSOUL], "NSOUL");
        for (k = 0, hits = 0; k < 96u; k++) {
            ui_power_on();
            gp.genre = 1u + (k >> 1) % NGENRE; gp.seed = 100u + (k >> 1); gp.energy = 60; gp.form = 0;
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            for (i = 0; i < GP_N; ++i) gp.amt[i] = GA_AUTO;
            song.g[G_HPROG] = k & 1u ? 3 : 0;
            gen_go();
            if (k & 1u) {                               /* (HARMONY on: kept, the parts on the tonic chord, HARMONY moves them) */
                ok &= song.g[G_HPROG] == 3 && gs.pn == 1u && gs.pdeg[0] == 0u;
                continue;
            }
            fam = HXF(gs.hx);
            for (i = 0; i < 2u && fam < HF_N; i++) {
                const gprog_t *p = &PROG[PR_FAM + 2u * fam + i];
                hits += gs.pn == p->n && gs.pdeg[0] == (uint8_t)p->deg[0] && gs.pdeg[1] == (uint8_t)p->deg[1] &&
                        gs.pdeg[2] == (uint8_t)p->deg[2] && gs.pdeg[3] == (uint8_t)p->deg[3];
            }
        }
        if (!hits) printf("  no family progression in 48 throws\n");
        bad += check("GEN: the families' progressions among PROG's (SCL > HARMONY's too), thrown; HARMONY on kept, GEN on the tonic", ok && hits);
    }
    printf(bad ? "harmony: %d FAILED\n" : "harmony: all ok\n", bad);
    return bad != 0;
}
