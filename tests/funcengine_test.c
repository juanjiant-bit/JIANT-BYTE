/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE 0.17) FUNC as an engine of its own (eng_lofi.c ENG_FUNC): its number, its place in the picker, its pages and
 * names; a project of before it (MATH WAVE FUNC) loading as FUNC, one of it stored as MATH WAVE FUNC (a firmware without
 * it loads it); every FORM's preset sounding, none clipping, at its own knobs and at their ends; the new recipes'
 * rhythms where they are written. With a directory as its argument: a WAV of each new recipe there */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
static project_t a, b;
static project_store_t stored;
static void wav(const char *path, const int16_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = n * 4u, v;
    assert(f);
    fwrite("RIFF", 1, 4, f); v = 36u + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1u | 2u << 16; fwrite(&v, 4, 1, f); v = FS; fwrite(&v, 4, 1, f);
    v = FS * 4u; fwrite(&v, 4, 1, f); v = 4u | 16u << 16; fwrite(&v, 4, 1, f);
    fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(x, 4, n, f); fclose(f);
}
static void setup(uint32_t preset)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    for (uint32_t k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, preset); math_poll(); frame(); song.master_q12 = 4096;
    int32_t o[CTL * 2];
    for (uint32_t j = 0; j < 20; j++) mix_block(o, CTL);
}
/* the mix of the transport playing for ms: its peak and RMS (and its samples into pcm, when asked) */
static int16_t pcm[FS * 9u * 2u];
static void play(uint32_t ms, int32_t *peak, double *rms, int keep)
{
    int32_t o[CTL * 2];
    double e = 0;
    uint32_t n = FS / 1000u * ms / CTL, at = 0;
    *peak = 0;
    transport_req = 1;
    for (uint32_t j = 0; j < n; j++) {
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL * 2u; i++) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > *peak) *peak = v;
            e += (double)o[i] * o[i];
            if (keep && at < NELEM(pcm)) pcm[at++] = (int16_t)clamp(o[i], -32768, 32767);
        }
    }
    *rms = sqrt(e / (n * CTL * 2.0));
    transport_req = 2;
    for (uint32_t j = 0; j < FS / 4u / CTL; j++) mix_block(o, CTL);
}
int main(int argc, char **argv)
{
    uint32_t k, f;
    /* the engine: number 18 (appended), named, picked after MATH, a preset a FORM; MATH's WAVE without FUNC */
    assert(ENGINES[ENGI_FUNC] == &ENG_FUNC && eng_idx(ENGI_FUNC) == ENGI_FUNC && eng_ok(ENGI_FUNC) && ENGI_HIVE == 17u);
    assert(!strcmp(ENG_FUNC.name, "FUNC") && !strcmp(N_ENGNAME[ENGI_FUNC], "FUNC") && NELEM(N_ENGNAME) == NENGINES);
    assert(eng_vis(eng_rank(ENGI_LOFI) + 1u) == ENGI_FUNC && eng_step(ENGI_LOFI, 1) == ENGI_FUNC);
    assert(ENGINE_PC_ORDER[NENG_SHOWN - 1u] == ENGI_FUNC);
    assert(ENG_LOFI.edit[1].max == RW_USER && ENG_FUNC.edit[1].min == RW_FUNC && ENG_FUNC.edit[1].max == RW_FUNC);
    assert(ENG_FUNC.npresets == FUNC_NFORM - 1u && FUNC_NFORM == 21u);
    for (k = 0; k < ENG_FUNC.npresets; k++) {
        const preset_t *p = &ENG_FUNC.presets[k];
        assert(p->e[1] == RW_FUNC && p->e[7] == (int)k + 1 && p->e[0] >= 4 && p->e[0] <= 6 && PR_TAG(p));
        assert(!strcmp(p->name, N_FUNC_FORM[k + 1u]));
    }
    for (k = 0; k < NELEM(LOFI_PRESETS); k++) assert(LOFI_PRESETS[k].e[1] != RW_FUNC);
    /* its stars in a row of their own (FM6's, retired), not past the table */
    memset(&favorites, 0, sizeof favorites);
    assert(favorite_set(ENGI_FUNC, 19, 1) && favorite_has(ENGI_FUNC, 19) && (favorites.factory[ENGI_FM6][2] & 8u));
    assert(!favorite_has(ENGI_LOFI, 19) && !favorite_has(ENGI_HIVE, 19));
    memset(&favorites, 0, sizeof favorites);
    puts("funcengine: engine 18, after MATH in the picker, a preset a FORM, MATH's WAVE without FUNC, its stars PASS");

    /* its pages: PLAY (FORM RUN ROOT CUT) and KNOBS (A B C D by FORM), no other engine's EDIT; MATH not theirs */
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    assert(TSEL->eng_req == ENGI_FUNC && TSEL->p[P_E1] == RW_FUNC && TSEL->p[P_E7] == 1);
    {
        uint32_t seen = 0, i;
        for (i = 0; i < NPAGES; i++) {
            if (PAGES[i].scope != SC_ENGINE || PAGES[i].fam != FAM_EDIT || !page_visible(i)) continue;
            if (PAGES[i].id[0] == P_E7) { assert(PAGES[i].id[1] == P_E0 && PAGES[i].id[2] == P_E6 && PAGES[i].id[3] == P_FCUT); seen |= 1; }
            else { assert(PAGES[i].id[0] == P_E2 && PAGES[i].id[3] == P_E5); seen |= 2; }
            assert(!strcmp(ENG_FUNC.page_title[PG_TI(&PAGES[i])], PAGES[i].id[0] == P_E7 ? "PLAY" : "KNOBS"));
        }
        assert(seen == 3u);
        assert(nav_put(SEC_EDIT, "EDIT 1") && cur_page()->id[0] == P_E7 && !strcmp(nav_page_name(), "PLAY"));
        frame();
        assert(nav_put(SEC_EDIT, "EDIT 2") && cur_page()->id[0] == P_E2 && !strcmp(nav_page_name(), "KNOBS"));
        frame();
        for (f = 1; f < FUNC_NFORM; f++) {               /* the knobs by FORM's names, the cards' and the matrix's */
            TSEL->p[P_E7] = (int16_t)f;
            for (k = 0; k < 4u; k++) assert(track_desc(TSEL, P_E2 + k) == &FUNC_KNOBS[f][k] && func_knob_tip(track_desc(TSEL, P_E2 + k)));
            assert(!strcmp(track_desc(TSEL, P_E7)->label, "FORM") && !strcmp(track_desc(TSEL, P_E0)->label, "RUN"));
        }
        TSEL->p[P_E7] = 15; frame();
        select_engine(ENGI_LOFI); frame();
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].scope == SC_ENGINE && PAGES[i].fam == FAM_EDIT && page_visible(i))
                assert(PAGES[i].id[0] == P_E0 || PAGES[i].id[0] == P_E4);
    }
    /* the new FORMs' knobs named (not A B C D), a line of help each */
    {
        static const char *const N[6][4] = {{"CHORD", "ECHO", "BASS", "SEED"}, {"HATS", "CHOP", "SNARE", "SEED"},
            {"TONE", "GATE", "BASS", "SEED"}, {"FM", "DRIFT", "SHINE", "SEED"}, {"BLIP", "HATS", "DRIVE", "SEED"},
            {"WOBBLE", "GROWL", "DRIVE", "SEED"}};
        for (f = 15; f < FUNC_NFORM; f++)
            for (k = 0; k < 4u; k++) assert(!strcmp(FUNC_KNOBS[f][k].label, N[f - 15u][k]) && strlen(FUNC_TIPS[f][k]) > 4u);
    }
    puts("funcengine: PLAY and KNOBS its own EDIT pages, by their names; MATH's its own; the new FORMs' knobs named PASS");

    /* a project of before FUNC: a MATH part of WAVE FUNC loads as FUNC (its preset its FORM's), sounding */
    ui_power_on(); song.sel = 0; select_engine(ENGI_LOFI); frame();
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    TSEL->eng_req = ENGI_LOFI; TSEL->p[P_E1] = RW_FUNC; TSEL->p[P_E0] = 5; TSEL->p[P_E7] = 7; TSEL->p[P_E2] = 50; TSEL->p[P_SUS] = 127;
    TSEL->preset = 12;
    project_capture(&a);
    assert(a.t[0].engine == ENGI_LOFI && a.t[0].p[P_E1] == RW_FUNC);
    assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw));
    assert(b.t[0].engine == ENGI_FUNC && b.t[0].preset == 6u && b.t[0].p[P_E7] == 7 && b.t[0].p[P_E2] == 50 && proj_ok(&b));
    assert(!project_restore_runtime(&b));
    assert(TSEL->eng_req == ENGI_FUNC && TSEL->p[P_E1] == RW_FUNC && TSEL->p[P_E7] == 7 && TSEL->p[P_E0] == 5);
    math_poll(); frame(); song.master_q12 = 4096;
    {
        int32_t pk; double r;
        int32_t o[CTL * 2];
        for (k = 0; k < 20u; k++) mix_block(o, CTL);
        assert(TSEL->engine == ENGI_FUNC && lofi_prg[0].func);
        play(2000, &pk, &r, 0);
        printf("old MATH FUNC HOUSE as FUNC: rms %.0f peak %d\n", r, pk);
        assert(r > 300.0);
    }
    /* .. and a FUNC part stored as MATH WAVE FUNC (MATH's FUNC SONG): what a firmware without FUNC reads as before */
    project_capture(&a);
    assert(a.t[0].engine == ENGI_LOFI && a.t[0].p[P_E1] == RW_FUNC && a.t[0].preset == 12u && a.t[0].p[P_E7] == 7);
    assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw) && b.t[0].engine == ENGI_FUNC);
    /* .. MATH WAVE FUNC reached otherwise (the editor, a test): FUNC's at the next poll */
    select_engine(ENGI_LOFI); TSEL->p[P_E1] = RW_FUNC; TSEL->p[P_E7] = 17; math_poll();
    assert(TSEL->eng_req == ENGI_FUNC && TSEL->preset == 16u);
    puts("funcengine: a MATH WAVE FUNC project loads as FUNC and sounds; FUNC is stored as MATH WAVE FUNC PASS");

    /* every FORM's preset: sounding at its own knobs, never past full scale there or at the knobs' ends */
    for (k = 0; k < ENG_FUNC.npresets; k++) {
        int32_t pk, p0, p1; double r, r0, r1;
        setup(k);
        assert(TSEL->eng_req == ENGI_FUNC && lofi_prg[0].func && lofi_prg[0].n <= FVM_MAXINS && math_music_cost(TSEL) <= 12u);
        play(3000, &pk, &r, 0);
        setup(k); for (f = 0; f < 4u; f++) TSEL->p[P_E2 + f] = 0; math_poll(); play(1500, &p0, &r0, 0);
        setup(k); for (f = 0; f < 4u; f++) TSEL->p[P_E2 + f] = 127; math_poll(); play(1500, &p1, &r1, 0);
        printf("FORM %2u %-8s rms %6.0f peak %6d | knobs 0: %6.0f %6d | 127: %6.0f %6d  (%u ins, cost %u)\n", k + 1u,
               N_FUNC_FORM[k + 1u], r, pk, r0, p0, r1, p1, lofi_prg[0].n, math_music_cost(TSEL));
        assert(r > 400.0 && pk < 32767 && p0 < 32767 && p1 < 32767 && r1 > 100.0);
    }
    puts("funcengine: every FORM's preset sounds, none past full scale at its knobs or their ends PASS");

    /* the new rhythms where they are written: AMEN's kick and snare masks at B 0 (no beat shuffled), DUB's one drop,
     * MINIMAL's four on the floor, JUNGLE's kick on 1 and the "and" of 3, snare on 2 and 4 (func_peek: the panel's) */
    {
        static const struct { uint32_t form, steps, kick, snare; } W[] = {
            {16, 32, 0x04050C05u, 0x42909290u}, {15, 16, 0x0100u, 0u}, {19, 16, 0x1111u, 0u}, {20, 32, 0x04010401u, 0x10101010u}};
        static fvm_cc_t cc; static fvm_full_t pg;
        for (uint32_t w = 0; w < NELEM(W); w++) {
            const char *s = MATH_FUNC_R[W[w].form - 5u];
            uint32_t u[FU_N] = {0}, hk = 0, hs = 0, st;
            assert(!fvm_compile(&cc, FVM_FULL(&pg), s, strlen(s), 0) && !pg.p.nstate);
            for (f = 0; f < 4u; f++) { u[FU_A + f] = f == 1u && W[w].form == 16u ? 0u : 128u; u[FU_AF + f] = u[FU_A + f] * 512u; }
            u[FU_NOTE] = 48; u[FU_BPS] = FS / 2u;
            for (st = 0; st < W[w].steps; st++) {
                fpeek_t o; uint32_t P = st * 16384u + 64u;
                func_peek(&pg.p, u, P, P * (FS / 2u) >> 16, &o);
                if ((o.has & 1u) && o.age[0] < 1024u) hk |= 1u << st;
                if ((o.has & 2u) && o.age[1] < 1024u) hs |= 1u << st;
            }
            printf("FORM %u kick %08x snare %08x\n", W[w].form, hk, hs);
            assert(hk == W[w].kick && hs == W[w].snare);
        }
    }
    puts("funcengine: AMEN DUB MINIMAL JUNGLE's drums where their patterns put them PASS");

    if (argc > 1) {                                     /* the new recipes heard: a WAV each (8 s, 120 BPM) */
        for (f = 15; f < FUNC_NFORM; f++) {
            char path[256]; int32_t pk; double r;
            setup(f - 1u);
            play(8000, &pk, &r, 1);
            snprintf(path, sizeof path, "%s/func_%02u_%s.wav", argv[1], f, N_FUNC_FORM[f]);
            wav(path, pcm, FS * 8u);
            printf("wrote %s\n", path);
        }
    }
    puts("funcengine: FUNC its own engine, old projects as FUNC, its recipes sounding PASS");
    return 0;
}
