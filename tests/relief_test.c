/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE relief, the owner: "the sound never cuts out for the CPU") The overload guard's relief ladder (core.h load_rl,
 * voice.c load_half, load_step): with no overload not a sample changes; under an overload emulated on the heaviest
 * scenes, the stages take the load back under the deadline before anything is cut: no voice shed (audio.c sheds one
 * only for a late half with every stage on), no part paused; the stages hold while the overload lasts (no pumping) and
 * are given back once it ends.
 * The overload: each half's render share fed to the guard as the FM-1's audio interrupt does (audio.c shed_check), a
 * half late (past 100 %) taking a stage as audio.c does. The host has no TIMER4 (and its own clock is far noisier than
 * the FM-1's), so the share is the scene's cost model: OVER x its load now (its sounding voices, of the most it had)
 * x (1 - what the stages on give back). What each stage gives back is the larger core's instructions a block, measured
 * with callgrind on these scenes (the BYTE relief pass: no stage, then each on), read off the engines' own state as it
 * is: WAVE's unison voices as faded (eng_wave.c wf_t uo1, uo3), HIVE's strings at the lower rate (eng_hive.c hv_st_t
 * rl), the delay's filters and MATH / FUNC at 11 kHz with the stage */
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int32_t vo[2 * CTL];
static uint32_t rt_hash;
static void rt_half(void)                                /* a half (two blocks), the main loop's between them */
{
    uint32_t b, q;
    for (b = 0; b < HALF_FRAMES / CTL; b++) {
        mix_block(vo, CTL);
        for (q = 0; q < 2u * CTL; q++) rt_hash = (rt_hash ^ (uint32_t)vo[q]) * 16777619u;
    }
    math_poll(); dw_poll();
}
/* ---- the scenes (as the CPU passes' profiler) */
static void quiet(void)
{
    uint32_t k;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 4096;
}
static void part(uint32_t k, uint32_t e, uint32_t pr)
{
    track_t *t = &trk[k];
    uint32_t i;
    set_engine_of(t, e);
    t->engine = t->eng_req;
    apply_preset_to(t, pr);
    t->p[P_MUTE] = 0;
    for (i = 0; i < 512u && (i < 64u || t->xf_on || t->eng_req != t->engine); i++) { mix_block(vo, CTL); math_poll(); }
}
static void notes(uint32_t k, uint32_t n)
{
    static const uint8_t N[8] = {48, 52, 55, 59, 62, 65, 69, 72};
    uint32_t i;
    for (i = 0; i < n && i < 8u; i++) trk_note_on(&trk[k], (uint8_t)(N[i] + (k % 3u) * 2u), 100);
}
static void sends(uint32_t rt)                           /* the buses, every part into them: CLOUD (4), delay, chorus */
{
    uint32_t k;
    song.g[G_RTYPE] = (int16_t)rt; song.g[G_DFDBK] = 70; song.g[G_DTIME] = DT_FREE + 40;
    for (k = 0; k < NTRK; k++) { trk[k].p[P_DLY] = 70; trk[k].p[P_REV] = 80; trk[k].p[P_CHOR] = 70; }
}
static void guard_reset(void)
{
    load_rl = 0; load_tight = 0; guard_cap = guard_run = 0; guard_calm = 0; load_avg_q8 = load_all_q8 = 0;
    rl_wait = rl_back = rl_rel = rl_late = 0; load_cores2 = 0; rl_fast_q8 = rl_pre_q8 = 0; cpu_pause = 0;
    memset(rl_sav, 0, sizeof rl_sav);
}
static void scene(uint32_t s)
{
    uint32_t k;
    guard_reset();
    switch (s) {
    case 0:                                              /* the heaviest GEN song (callgrind: gen/s123), CLOUD, the
                                                          * delay, the chorus */
        ui_power_on();
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        gp.genre = GEN_EXPLORE; gp.seed = 123; gp.energy = 85; gp.form = 0;
        gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        life_v[LIFE_AMT] = 100;
        gen_go();
        for (k = 0; k < 64u; k++) { mix_block(vo, CTL); math_poll(); dw_poll(); }
        transport_req = 1; ui_input();
        sends(4);
        song.master_q12 = 4096;
        break;
    case 1: {                                            /* HIVE x4, CLOUD */
        static const uint8_t HP[4] = {0, 1, 4, 9};
        quiet();
        for (k = 0; k < 4u; k++) { part(k, ENGI_HIVE, HP[k]); notes(k, 2); }
        sends(4);
        break; }
    case 2:                                              /* WAVE x4, unison 7 */
        quiet();
        for (k = 0; k < 4u; k++) {
            part(k, ENGI_WAVE, 4);
            trk[k].p[P_MORPH] = 7; trk[k].p[P_E6] = 127; trk[k].p[P_VOICE] = V_POLY; trk[k].p[P_E5] = 64;
            notes(k, 2);
        }
        break;
    default: {                                           /* FUNC x4, MOTION */
        static const uint8_t FP[4] = {0, 4, 13, 17};
        quiet();
        for (k = 0; k < 4u; k++) {
            part(k, ENGI_FUNC, FP[k]);
            fp_kp[k][4] = 64; fp_kp[k][5] = 90; fp_kp[k][2] = 64;
            notes(k, 2);
        }
        transport_req = 1; ui_input();
        break; }
    }
    for (k = 0; k < 64u; k++) rt_half();
}
static const char *const SCENE[4] = {"GEN song + CLOUD + delay + chorus", "HIVE x4 + CLOUD", "WAVE unison 7 x4",
                                     "FUNC MOTION x4"};
/* what each stage gives back, of the scene's load with none (1/1000, the larger core's instructions a block,
 * callgrind: gen/s123.cloud 106662 .. 93407, hive/x4.cloud 69383 .. 57420, wave/unison7.x4 117643 .. 60987,
 * func/motion.x4 105770 .. 80637): RL_TIGHT RL_DLY RL_WAVE3 RL_HIVE RL_WAVE1 RL_QUART */
static const uint16_t GIVE[4][RL_MAX] = {{2, 63, 0, 0, 0, 59}, {0, 0, 0, 172, 0, 0}, {0, 0, 301, 0, 181, 0},
                                         {0, 0, 2, 0, 3, 233}};
static const double OVER[4] = {1.10, 1.18, 1.6, 1.25};  /* (each scene's heaviest halves with no relief, x the half) */
static double given(uint32_t s)                          /* the share the stages on give back now (their state) */
{
    double a[RL_MAX] = {0}, g = 0;
    uint32_t k, nw = 0, nh = 0;
    a[0] = load_tight;
    a[1] = load_rl >= RL_DLY;
    a[5] = load_rl >= RL_QUART;
    for (k = 0; k < NTRK; k++)
        if (trk[k].engine == ENGI_WAVE && !trk[k].p[P_MUTE]) {
            nw++; a[2] += wf[k].uo3 / 16.0; a[4] += wf[k].uo1 / 16.0;
        } else if (trk[k].engine == ENGI_HIVE && !trk[k].p[P_MUTE]) {
            nh++; a[3] += hv_of(k)->rl;
        }
    if (nw) a[2] /= nw, a[4] /= nw;
    if (nh) a[3] /= nh;
    for (k = 0; k < RL_MAX; k++) g += a[k] * GIVE[s][k] / 1000.0;
    return g;
}
static int rbad;
static void rcheck(const char *what, int ok)
{
    printf("relief: %-100s %s\n", what, ok ? "ok" : "FAIL");
    rbad += !ok;
}
#define HALVES 1400u                                     /* ~4 s of overload */
int main(void)
{
    uint32_t s, i;
    char line[240];
    mc_host = 1;
    for (s = 0; s < 4u; s++) {
        double peak = 0, pct = 0;
        uint32_t k, ref = 0, late = 0, shed = 0, top = 0, moves = 0, last = 0, cap = 0, back = 0, busy_mx = 1;
        int fd[2];
        pid_t pid;
        /* no overload: the guard fed a light load, every sample as with no guard at all (the reference in a child,
         * from the same state) */
        scene(s);
        if (pipe(fd)) return 2;
        fflush(stdout);
        if (!(pid = fork())) {
            rt_hash = 2166136261u;
            for (i = 0; i < 600u; i++) rt_half();
            if (write(fd[1], &rt_hash, sizeof rt_hash) != sizeof rt_hash) _exit(1);
            _exit(0);
        }
        rt_hash = 2166136261u;
        for (i = 0; i < 600u; i++) {
            rt_half(); load_half(40, 40, 1); top |= load_rl | load_tight;
            if (voices_busy() > busy_mx) busy_mx = voices_busy();
        }
        if (read(fd[0], &ref, sizeof ref) != sizeof ref) ref = ~rt_hash;
        waitpid(pid, 0, 0); close(fd[0]); close(fd[1]);
        snprintf(line, sizeof line, "%s: no overload, not a sample changed (the guard at 40 %%, no stage)", SCENE[s]);
        rcheck(line, rt_hash == ref && !top);
        /* the overload, rising there over 0.3 s (as notes come in), then held ~4 s */
        for (i = 0; i < HALVES; i++) {
            double f = i < 100u ? 0.6 + 0.4 * i / 100.0 : 1.0, load = 0.6 + 0.4 * voices_busy() / busy_mx;
            rt_half();
            pct = OVER[s] * 100.0 * f * (load < 1.0 ? load : 1.0) * (1.0 - given(s));
            load_half((uint32_t)pct, (uint32_t)pct, 1);
            if (pct > 100.0) {                           /* (audio.c: a late half) */
                late++;
                shed += load_late();
            }
            if (i >= 200u && pct > peak) peak = pct;
            if (load_rl > top) top = load_rl;
            if (i >= 400u) moves += load_rl != last;
            last = load_rl;
            cap |= guard_cap;
        }
        snprintf(line, sizeof line, "%s at %.0f %%: no voice shed (%u), none paused; stage %u, %u halves late, the cap %s, "
                 "at most %.0f %% after", SCENE[s], OVER[s] * 100.0, shed, top, late, cap ? "on" : "never", peak);
        rcheck(line, !shed && !cpu_pause && !late && top >= RL_TIGHT && peak < 100.0);
        snprintf(line, sizeof line, "%s: the stages held while it lasted, no pumping (%u moves in the last 2.9 s)",
                 SCENE[s], moves);
        rcheck(line, moves <= 1u);
        for (i = 0; i < 6000u && (load_rl || load_tight); i++) {   /* the overload gone (a third of it): every stage given
                                                         * back, one at a time */
            double load = 0.6 + 0.4 * voices_busy() / busy_mx;
            rt_half();
            pct = OVER[s] * 33.0 * (load < 1.0 ? load : 1.0) * (1.0 - given(s));
            load_half((uint32_t)pct, (uint32_t)pct, 1);
            back = i;
        }
        for (k = 0; k < 32u; k++) rt_half();             /* (the fades back in) */
        {   /* just past the trip for 10 s (its heaviest halves at 92 %): the ladder settles, no pumping */
            uint32_t mv = 0, l0 = load_rl, sh0 = 0, dn = 0, re = 0;
            for (i = 0; i < 3500u; i++) {
                double load = 0.6 + 0.4 * voices_busy() / busy_mx;
                rt_half();
                pct = 92.0 * (load < 1.0 ? load : 1.0) * (1.0 - given(s));
                load_half((uint32_t)pct, (uint32_t)pct, 1);
                if (pct > 100.0) sh0 += load_late();
                mv += load_rl != l0;
                dn += load_rl < l0;
                re += load_rl > l0 && dn;                /* (a stage given back, then taken again: a pump) */
                l0 = load_rl;
            }
            snprintf(line, sizeof line, "%s: just past the trip for 10 s, %u stage moves (%u taken again), no voice shed (%u)",
                     SCENE[s], mv, re, sh0);
            rcheck(line, re <= 1u && !sh0);
            for (i = 0; i < 6000u && (load_rl || load_tight); i++) { rt_half(); load_half(30, 30, 1); }
            for (k = 0; k < 32u; k++) rt_half();
        }
        snprintf(line, sizeof line, "%s: the overload over (%.0f %%), every stage given back (%.1f s)", SCENE[s],
                 OVER[s] * 33.0, (double)back * HALF_FRAMES / FS);
        rcheck(line, !load_rl && !load_tight && given(s) == 0.0);
    }
    {   /* (c1fault) core 1 lost (CORES 1 (FAULT)) under FUNC x4: the load doubled at once, every half late. The guard
         * on at the loss; the late halves take the stages, then the cap, and no voice is shed before the half rate had
         * its time (~93 ms, RL_LATE halves late past all of it); a lasting overload still sheds (the last resort) */
        uint32_t sh = 0, first = 0, h, rl1 = 0;
        scene(3);
        load_half(60, 60, 1);
        load_half(60, 60, 0);                            /* (the loss) */
        rl1 = load_tight && load_rl >= RL_TIGHT;
        for (h = 0; h < 400u; h++) {
            rt_half();
            load_half(200, 200, 0);
            if (load_late()) { sh++; if (!first) first = h + 1u; }
        }
        snprintf(line, sizeof line, "one core left under FUNC x4 (200 %%): the guard on at the loss, every stage and the cap "
                 "before a voice is shed (the first at half %u, past %u), the last resort still there (%u, one each %u)", first, RL_LATE, sh, RL_UP + 1u);
        rcheck(line, rl1 && first > RL_LATE && load_rl == RL_MAX && guard_cap && sh > 0u && sh <= 400u / (RL_UP + 1u) + 1u);
        guard_reset();
    }
    {   /* a late half with stages left: a stage, no shed; with every stage on: the cap, and the shed (as before) */
        guard_reset();
        for (i = 0; i < RL_MAX; i++) load_step();
        rcheck("a late half: a stage of the ladder first; past the last, the cap and the shed (audio.c)",
               load_rl == RL_MAX && !guard_cap && !load_step() && guard_cap);
        guard_reset();
    }
    printf("relief: %s\n", rbad ? "FAILED" : "all relief checks passed");
    return rbad != 0;
}
