/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (funcmotion) FUNC's formula moving its stereo and its timbre: the matrix's FN HIT FN NOTE FN BAR (what the formula
 * plays: its onsets, how high, its place in four bars), EDIT > MOTION's STEREO MOVE TIMBRE COLOR (mod.c func_mod,
 * eng_lofi.c fn_color). MOTION 0 (every project of before, MATH WAVE FUNC moved): the sources change nothing, the
 * sound as before; the presets' own; the panning moving between the sides, the timbre far from the recipe's; saved in
 * the project ('F' 'M'), one of before loading still; the page FUNC's alone */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static project_t a, b;
static project_store_t stored;
#define MN (FS * 8u)
typedef struct { int16_t L0[MN], R0[MN], L1[MN], R1[MN]; int32_t hit_mx, bar_mn, bar_mx, note_mx; uint32_t onsets; } sh_t;
static sh_t *sh;   /* (each run in a child from the same state: shared) */
#define L0 sh->L0
#define R0 sh->R0
#define L1 sh->L1
#define R1 sh->R1
#define hit_mx sh->hit_mx
#define bar_mn sh->bar_mn
#define bar_mx sh->bar_mx
#define note_mx sh->note_mx
#define onsets sh->onsets
static void setup(uint32_t form)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    for (uint32_t k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, form - 1u); math_poll(); frame(); song.master_q12 = 4096;
    TSEL->p[P_DLY] = TSEL->p[P_REV] = TSEL->p[P_CHOR] = 0;
    song.g[G_BPM] = 120;
    memset(fp_st, 0, sizeof fp_st);
    {
        int32_t x[CTL * 2]; uint32_t j;
        for (j = 0; j < FS / CTL; j++) mix_block(x, CTL);
        rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
        ap_rng = 0x2545F491u; pfx_seed = 0x9E3779B9u; mns_clk = 0; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0;
        memset(TSEL->v, 0, sizeof TSEL->v); TSEL->lfo_ph = 0; TSEL->lfo_val = TSEL->lfo_fade = 0; fx_trip(0, CTL); memset(&fx, 0, sizeof fx);
        tf_ic1[0] = tf_ic2[0] = 0; memset(fns, 0, sizeof fns); memset(fnb, 0, sizeof fnb); drift_clk[0] = 0; pl_g[0] = 0; dist_w[0] = 0; memset(l24_s, 0, sizeof l24_s); memset(ms_s, 0, sizeof ms_s); memset(dist_nlp, 0, sizeof dist_nlp);
    }
    memset(fn_sg, 0, sizeof fn_sg);
}
static void mo(int st, int mv, int tb, int co) { fp_kp[0][2] = (int16_t)st; fp_kp[0][3] = (int16_t)mv; fp_kp[0][4] = (int16_t)tb; fp_kp[0][5] = (int16_t)co; }
/* the transport playing ms, the channels into l r; the sources' ranges meanwhile */
static void run1(uint32_t ms, int16_t *l, int16_t *r)
{
    int32_t x[CTL * 2];
    uint32_t n = FS / 1000u * ms / CTL, j, i;
    hit_mx = note_mx = 0; bar_mn = 32767; bar_mx = -32767; onsets = 0;
    transport_req = 1;
    for (j = 0; j < n && (j + 1u) * CTL <= MN; j++) {
        mix_block(x, CTL);
        for (i = 0; i < CTL; i++) { l[j * CTL + i] = (int16_t)clamp(x[2u * i], -32768, 32767); r[j * CTL + i] = (int16_t)clamp(x[2u * i + 1u], -32768, 32767); }
        if (j * CTL < FS / 2u) continue;
        onsets += fn_sg[0].v[0] == 32767;           /* (an onset's block) */
        if (mod_tsrc(TSEL, MS_FNHIT, 0) > hit_mx) hit_mx = mod_tsrc(TSEL, MS_FNHIT, 0);
        if (mod_tsrc(TSEL, MS_FNNOTE, 0) > note_mx) note_mx = mod_tsrc(TSEL, MS_FNNOTE, 0);
        if (fn_sg[0].v[2] < bar_mn) bar_mn = fn_sg[0].v[2];
        if (fn_sg[0].v[2] > bar_mx) bar_mx = fn_sg[0].v[2];
    }
    transport_req = 2;
    for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
}
static void run(uint32_t ms, int16_t *l, int16_t *r)
{
    int st = 0;
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (!pid) { run1(ms, l, r); _exit(0); }
    waitpid(pid, &st, 0);
    assert(WIFEXITED(st) && !WEXITSTATUS(st));
}
static double side_swing(const int16_t *l, const int16_t *r, uint32_t n)   /* the share of the blocks clearly off the middle */
{
    uint32_t j, i, off = 0, all = 0;
    for (j = FS / 2u / CTL; j < n / CTL; j++) {
        double el = 0, er = 0;
        for (i = j * CTL; i < (j + 1u) * CTL; i++) { el += (double)l[i] * l[i]; er += (double)r[i] * r[i]; }
        if (el + er < 1e5) continue;
        all++;
        off += el > er * 2.0 || er > el * 2.0;
    }
    return all ? (double)off / all : 0;
}
static double diff_rms(const int16_t *x, const int16_t *y, uint32_t n)
{
    double e = 0; uint32_t i;
    for (i = 0; i < n; i++) e += ((double)x[i] - y[i]) * ((double)x[i] - y[i]);
    return sqrt(e / n);
}
static double rms(const int16_t *x, uint32_t n)
{
    double e = 0; uint32_t i;
    for (i = 0; i < n; i++) e += (double)x[i] * x[i];
    return sqrt(e / n);
}
int main(void)
{
    uint32_t f, n = FS * 8u;
    sh = mmap(0, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(sh != MAP_FAILED);
    /* 1 the sources follow the formula: hits, how high, the four bars' sweep */
    setup(19);                                          /* MINIMAL */
    assert(fp_kp[0][2] == 64 && fp_kp[0][3] == 1 && fp_kp[0][4] == 48 && fp_kp[0][5] == 16);   /* (its preset's) */
    mo(0, 0, 0, 0);
    run1(1500, L0, R0);                                 /* (in this process: its state looked at) */
    assert(!fn_sg[0].v[0] && !fn_sg[0].v[1] && !fn_sg[0].slow);          /* (MOTION 0, no slot reading them: not even worked out) */
    TSEL->p[P_M1SRC] = MS_FNHIT;                        /* (a slot reading them, going nowhere: worked out) */
    run(8000, L0, R0);
    printf("MINIMAL: FN HIT up to %d, %u onsets in 7.5 s; FN NOTE up to %d; FN BAR %d .. %d\n", hit_mx, onsets, note_mx, bar_mn, bar_mx);
    assert(hit_mx > 30000 && onsets >= 12u && note_mx > 1000 && bar_mn < -20000 && bar_mx > -5000);
    assert(MS_BIPOLAR(MS_FNBAR) && !MS_BIPOLAR(MS_FNHIT) && !MS_BIPOLAR(MS_FNNOTE) && MS_BIPOLAR(MS_BEING));
    /* 2 MOTION 0: the sources change nothing heard (the sound as before), left and right the same */
    for (f = 1; f < FUNC_NFORM; f += 4u) {
        setup(f); mo(0, 3, 0, 0); run(3000, L0, R0);
        fn_sg[0].v[0] = 20000; fn_sg[0].v[2] = 9000; fn_sg[0].side = 1; run(3000, L1, R1);
        assert(!memcmp(L0, L1, FS * 2u * 2u) && !memcmp(R0, R1, FS * 2u * 2u));
        assert(!memcmp(L0, R0, FS * 2u * 2u) && !mod_begin(TSEL));
    }
    puts("funcmotion: MOTION 0, the sound as before whatever the sources hold, the matrix idle PASS");
    /* 3 STEREO: the formula's hits from side to side; NOTE, BAR, ALL move it too */
    {
        static const char *const NM[4] = {"NOTE", "HIT", "BAR", "ALL"};
        for (f = 0; f < 4u; f++) {
            double s;
            setup(19); mo(127, (int)f, 0, 0); run(8000, L1, R1);
            s = side_swing(L1, R1, n);
            printf("MINIMAL STEREO 127 MOVE %s: %.0f %% of the blocks to one side\n", NM[f], s * 100.0);
            assert(s > (f == 0u ? 0.05 : 0.2) && memcmp(L1, R1, n * 2u));
        }
        setup(19); mo(127, 1, 0, 0); run(8000, L1, R1);
        {   /* (HIT: both sides, in turn) */
            uint32_t j, i, left = 0, right = 0;
            for (j = FS / 2u / CTL; j < n / CTL; j++) {
                double el = 0, er = 0;
                for (i = j * CTL; i < (j + 1u) * CTL; i++) { el += (double)L1[i] * L1[i]; er += (double)R1[i] * R1[i]; }
                left += el > er * 2.0; right += er > el * 2.0;
            }
            printf("  HIT: %u blocks left, %u right\n", left, right);
            assert(left > 50u && right > 50u);
        }
    }
    puts("funcmotion: STEREO pans by the formula, both sides, by NOTE HIT BAR ALL PASS");
    /* 4 TIMBRE and COLOR: far from the recipe's sound, moving with it */
    {
        double r0, d1, d2;
        setup(10); mo(0, 0, 0, 0); run(6000, L0, R0);  /* PLUCK */
        r0 = rms(L0 + FS, FS * 5u);
        mo(0, 0, 110, 0); run(6000, L1, R1);
        d1 = diff_rms(L0 + FS, L1 + FS, FS * 5u);
        mo(0, 0, 0, 110); run(6000, L1, R1);
        d2 = diff_rms(L0 + FS, L1 + FS, FS * 5u);
        printf("PLUCK RMS %.0f: TIMBRE 110 moves it by %.0f, COLOR 110 by %.0f\n", r0, d1, d2);
        assert(d1 > r0 * 0.3 && d2 > r0 * 0.3 && rms(L1 + FS, FS * 5u) < 32000.0);
        /* (the fold: up to ~9 times, over 1 folded back; past 64 the low bits gone) */
        assert(fn_color(-32767, 0) == -32767 && fn_color(16000, 16) == 32000 && fn_color(20000, 16) == 25536 &&
               (fn_color(12345, 127) & 0x3FFF) == 0);
    }
    puts("funcmotion: TIMBRE and COLOR take the recipe far from its sound PASS");
    /* 5 the presets' MOTION on; a FUNC from MATH WAVE FUNC, a project of before: still */
    {
        uint32_t on = 0;
        for (f = 1; f < FUNC_NFORM; f++) { setup(f); on += fp_kp[0][2] || fp_kp[0][4] || fp_kp[0][5]; }
        assert(on == FUNC_NFORM - 1u);
        setup(19); mo(70, 2, 40, 9);
        project_capture(&a);
        assert(a.fmo[0][0] == 70 && a.fmo[0][1] == 2 && a.fmo[0][2] == 40 && a.fmo[0][3] == 9);
        assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw) && !memcmp(b.fmo[0], a.fmo[0], 4));
        mo(0, 0, 0, 0);
        assert(!project_restore_runtime(&b) && fp_kp[0][2] == 70 && fp_kp[0][3] == 2 && fp_kp[0][4] == 40 && fp_kp[0][5] == 9);
        project_capture(&a); memset(a.fmo, 0, sizeof a.fmo);      /* (one of before: no 'F' 'M') */
        assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw));
        assert(!project_restore_runtime(&b) && !fp_kp[0][2] && !fp_kp[0][3] && !fp_kp[0][4] && !fp_kp[0][5]);
        project_capture(&a); a.fpx[0] = 0;                        /* (before FUNC's 'F' 'P' too) */
        assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw));
        assert(!project_restore_runtime(&b) && !fp_kp[0][2] && !fp_kp[0][4]);
    }
    puts("funcmotion: the presets move, MOTION saved in the project, an old one still PASS");
    /* 6 the page, FUNC's alone; the sources' names */
    {
        char nb[28], w[40];
        setup(19);
        assert(nav_put(SEC_EDIT, "MOTION") && !strcmp(nav_page_name(), "MOTION")); frame();
        {
            int16_t *vp; const param_desc_t *d = page_desc(cur_page(), 0, &vp);
            assert(d && !strcmp(d->label, "STEREO") && vp == &fp_kp[0][2]);
            d = page_desc(cur_page(), 1, &vp); assert(!strcmp(d->label, "MOVE") && d->max == 3);
            d = page_desc(cur_page(), 3, &vp); assert(!strcmp(d->label, "COLOR") && vp == &fp_kp[0][5]);
            turn(EN_K1, 5); assert(fp_kp[0][2] != 64);
        }
        mod_full(TSEL, 1u, MS_FNHIT, nb, sizeof nb, w, sizeof w); assert(!strcmp(nb, "FUNC HITS"));
        assert(!strcmp(N_MSRC[MS_FNBAR], "FN BAR"));
        TSEL->p[P_M1SRC] = MS_FNBAR; TSEL->p[P_M1DST] = MD_PAN; TSEL->p[P_M1AMT] = 63; fn_sg[0].v[2] = 32767;
        mo(0, 0, 0, 0);
        assert(mod_begin(TSEL) && TSEL->p[P_PAN] > 40); mod_end(TSEL);
        select_engine(ENGI_LOFI); frame();
        assert(!nav_put(SEC_EDIT, "MOTION") || strcmp(nav_page_name(), "MOTION") || cur_page()->id[0] != ID_FM0);
    }
    puts("funcmotion: EDIT > MOTION FUNC's, the matrix's FN HIT FN NOTE FN BAR PASS");
    puts("funcmotion: all ok");
    return 0;
}
