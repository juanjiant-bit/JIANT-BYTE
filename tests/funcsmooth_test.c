/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (func-smooth, func-level, func-scale, func-rate) FUNC while it is morphed: each control turned during playback (the
 * knobs A B C, MOTION's TIMBRE COLOR STEREO, BLEND, CUT; D the recipes' seed, a pick) reaches the sound over blocks, never as a step at a block's
 * edge: from one state, the part rendered once as it is and once with the control moved, their difference grows from
 * 0 (its first three samples a small share of where it goes); the recipes' levels within 1.5 dB of DUB's and
 * MINIMAL's; the music calls on the SCALE (SONG in C minor: C minor's notes alone; MAJ as written); RATE on the grid,
 * at the bar while the song plays, faded; RATE and LAYER B's saved in the project, one of before 1X */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
static project_t a, b;
static project_store_t stored;
static void setup(uint32_t pi)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    for (uint32_t k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, pi); math_poll(); frame(); song.master_q12 = 4096;
    TSEL->p[P_DLY] = TSEL->p[P_REV] = TSEL->p[P_CHOR] = 0;
    song.g[G_BPM] = 120;
    memset(fp_st, 0, sizeof fp_st);
    int32_t x[CTL * 2]; uint32_t j;
    for (j = 0; j < FS / CTL; j++) mix_block(x, CTL);
    rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
    ap_rng = 0x2545F491u; pfx_seed = 0x9E3779B9u; mns_clk = 0; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0;
    memset(TSEL->v, 0, sizeof TSEL->v); TSEL->lfo_ph = 0; TSEL->lfo_val = TSEL->lfo_fade = 0; fx_trip(0, CTL); memset(&fx, 0, sizeof fx);
    memset(fn_sg, 0, sizeof fn_sg);
}
enum { C_A, C_B, C_C, C_D, C_STEREO, C_TIMBRE, C_COLOR, C_BLEND, C_CUT, C_N };
static const char *const CN[C_N] = {"A", "B", "C", "D", "STEREO", "TIMBRE", "COLOR", "BLEND", "CUT"};
#define NBK 24u
static int32_t *shb, base[NBK * CTL / 2];
static void render(uint32_t nb, int32_t *o)
{
    int32_t x[CTL * 2]; uint32_t j, i;
    for (j = 0; j < nb; j++) { mix_block(x, CTL); if (o) for (i = 0; i < CTL / 2u; i++) o[j * CTL / 2u + i] = x[4u * i] + x[4u * i + 1u]; }
}
static void change(uint32_t c)
{
    int16_t *q = c <= C_D ? &TSEL->p[P_E2 + c] : c == C_STEREO ? &fp_kp[0][2] : c == C_TIMBRE ? &fp_kp[0][4] :
                 c == C_COLOR ? &fp_kp[0][5] : c == C_BLEND ? &fp_kp[0][0] : &TSEL->p[P_FCUT];
    *q = (int16_t)(*q > 63 ? *q - 4 : *q + 4);         /* (a quick turn: four steps in a frame) */
}
/* the onset ratio (at the half rate's samples, both sides summed): of the change's difference, how much in its first
 * three samples; a step 1, a glide small (-1: no difference) */
static double trial(uint32_t c)
{
    uint32_t i, f, nn = NBK * CTL / 2u;
    pid_t pid = fork();
    if (!pid) { change(c); render(NBK, shb); _exit(0); }
    waitpid(pid, 0, 0);
    render(NBK, base);
    double x = 0, m = 0;                            /* (from the change: silent there, nothing to step) */
    for (i = 0, f = 0; i < nn; i++) { double d = fabs((double)shb[i] - base[i]); if (i >= f && i < f + 3u && d > x) x = d; if (i >= f && d > m) m = d; }
    return m > 0 ? x / m : -1;
}
/* the loudness (BS.1770's K weighting, its 400 ms blocks gated: -70 and 10 dB under), the part at its preset, 14 s */
#define LB (FS / 10u)
static double kms[200];
static double lufs(uint32_t pi)
{
    static const double B1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285}, A1[2] = {-1.69065929318241, 0.73248077421585},
                        A2[2] = {-1.99004745483398, 0.99007225036621};
    int32_t x[CTL * 2]; uint32_t j, i, ch, n = FS * 16u / CTL, nb = 0, k, used;
    double z[2][4] = {{0}}, acc = 0, sum, g, tot;
    setup(pi); fp_kp[0][0] = 64;
    transport_req = 1;
    for (j = 0; j < n; j++) {
        mix_block(x, CTL);
        if (j < FS * 2u / CTL) continue;
        for (i = 0; i < CTL; i++) {
            for (ch = 0; ch < 2u; ch++) {
                double v = x[2u * i + ch] / 32768.0, *q = z[ch];
                double y1 = B1[0] * v + q[0]; q[0] = B1[1] * v - A1[0] * y1 + q[1]; q[1] = B1[2] * v - A1[1] * y1;
                double y2 = y1 + q[2]; q[2] = -2.0 * y1 - A2[0] * y2 + q[3]; q[3] = y1 - A2[1] * y2;
                acc += y2 * y2;
            }
            if (((j - FS * 2u / CTL) * CTL + i + 1u) % LB == 0u && nb < 200u) { kms[nb++] = acc / LB; acc = 0; }
        }
    }
    transport_req = 2;
    for (sum = 0, used = 0, k = 0; k + 3u < nb; k++) {   /* (400 ms: four 100 ms) */
        double m = (kms[k] + kms[k + 1] + kms[k + 2] + kms[k + 3]) / 4;
        if (-0.691 + 10 * log10(m + 1e-12) > -70) { sum += m; used++; }
    }
    g = -0.691 + 10 * log10(sum / (used ? used : 1u) + 1e-12) - 10;
    for (tot = 0, used = 0, k = 0; k + 3u < nb; k++) {
        double m = (kms[k] + kms[k + 1] + kms[k + 2] + kms[k + 3]) / 4, l = -0.691 + 10 * log10(m + 1e-12);
        if (l > -70 && l > g) { tot += m; used++; }
    }
    return -0.691 + 10 * log10(tot / (used ? used : 1u) + 1e-12);
}
int main(void)
{
    uint32_t pi, c, k;
    shb = mmap(0, sizeof base, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shb != MAP_FAILED);
    /* 1 the controls turned while it plays: no step (the presets the owner heard click and the new ones) */
    {
        static const uint8_t PR[] = {2, 4, 10, 11, 13, 17, 19};   /* GROWL ACID PSY DSTEP MELODY AMBIENT JUNGLE */
        double worst[C_N] = {0};
        for (k = 0; k < sizeof PR; k++) {
            pi = PR[k];
            for (c = 0; c < C_N; c++) {
                if (c == C_D) continue;         /* (D: the seed, a pick: a step it is) */
                double r[10]; uint32_t nr = 0, t;
                setup(pi);
                if (c == C_BLEND) { TSEL->p[P_FFB] = (int16_t)((pi + 7u) % 20u + 1u); fp_kp[0][0] = 64; math_poll(); }
                transport_req = 1;
                render(FS / 2u / CTL, 0);
                for (t = 0; t < 10u; t++) { render(37u + t * 13u, 0); double q = trial(c); if (q >= 0) r[nr++] = q; }
                transport_req = 2;
                for (uint32_t i = 0; i < nr; i++) for (uint32_t j = i + 1u; j < nr; j++) if (r[j] < r[i]) { double z = r[i]; r[i] = r[j]; r[j] = z; }
                double med = nr >= 3u ? r[nr / 2u] : 0;   /* (silent at most: rests) */
                if (med > worst[c]) worst[c] = med;
                if (med > 0.2) { printf("%s %s: onset %.2f\n", FUNC_PRESETS[pi].name, CN[c], med); fflush(stdout); }
                assert(med <= 0.2);
            }
        }
        printf("onset ratio, the worst preset's (a step: 1):");
        for (c = 0; c < C_N; c++) printf(" %s %.2f", CN[c], worst[c]);
        puts("");
    }
    puts("funcsmooth: A B C, TIMBRE COLOR STEREO, BLEND, CUT turned while it plays: no step PASS");
    /* 2 the levels: every recipe at its preset within 1.5 dB of DUB's and MINIMAL's */
    {
        double ref = (lufs(14) + lufs(18)) / 2, l;
        for (pi = 0; pi < NELEM(FUNC_PRESETS); pi++) {
            l = lufs(pi) - ref;
            printf("%s %+.1f dB\n", FUNC_PRESETS[pi].name, l); fflush(stdout);
#ifndef FS_CAL
            assert(l > -1.5 && l < 1.5);
#endif
        }
    }
    puts("funcsmooth: every recipe at its preset within 1.5 dB of DUB and MINIMAL PASS");
    /* 3 the SCALE: SONG in C minor (its music calls' notes C minor's), MAJ and CUSTOM as written */
    {
        fvm_env_t e; uint32_t n, m = 0x5ADu;            /* (C minor: 0 2 3 5 7 8 10) */
        setup(0); TSEL->p[P_SCALE] = 2; TSEL->p[P_ROOT] = 0;
        memset(&e, 0, sizeof e); func_scl(TSEL, &e);
        assert((e.u[FU_SCL] & 0xFFFu) == m);
        for (n = 36; n < 96u; n++) assert(m >> (uint32_t)fu_scale((int32_t)n, &e) % 12u & 1u);
        assert(fu_scale(64, &e) == 63 && fu_scale(69, &e) == 68 && fu_scale(71, &e) == 70 && fu_scale(62, &e) == 62);
        TSEL->p[P_ROOT] = 9; func_scl(TSEL, &e);         /* (A minor) */
        for (n = 36; n < 96u; n++) assert(m >> (uint32_t)(fu_scale((int32_t)n, &e) + 3) % 12u & 1u);
        TSEL->p[P_SCALE] = 1; func_scl(TSEL, &e); assert(!e.u[FU_SCL]);
        TSEL->p[P_SCALE] = 2; TSEL->p[P_E7] = 0; func_scl(TSEL, &e); assert(!e.u[FU_SCL]);
        /* SONG's own: its chords and its notes through fvm_music, C minor's pitch classes alone */
        setup(0); TSEL->p[P_SCALE] = 2; TSEL->p[P_ROOT] = 0; memset(&e, 0, sizeof e); func_scl(TSEL, &e);
        {
            static const char *const R = "0573", *const M = "0247";
            for (k = 0; k < 4u; k++) {
                int32_t r = 60 + R[k] - 48, x = fu_scale(r, &e);
                assert(m >> (uint32_t)x % 12u & 1u && m >> (uint32_t)fu_scale(x + 3, &e) % 12u & 1u &&
                       m >> (uint32_t)fu_scale(x + 4, &e) % 12u & 1u && m >> (uint32_t)fu_scale(x + 7, &e) % 12u & 1u);
                assert(m >> (uint32_t)fu_scale(60 + M[k] - 48, &e) % 12u & 1u);
            }
        }
    }
    puts("funcsmooth: SONG in C minor plays C minor's notes alone; MAJ and CUSTOM as written PASS");
    /* 4 RATE: P times 1/4 .. 4 exactly (on the grid); asked while the song plays, at the bar line, faded */
    {
        static const uint32_t NUM[7] = {1, 1, 2, 1, 3, 2, 4}, DEN[7] = {4, 2, 3, 1, 2, 1, 1};
        uint32_t P[64], D[64], r, i;
        for (r = 0; r < 7u; r++) {
            for (i = 0; i < 64u; i++) P[i] = (i * 12345u + r * 777u) << 10;
            fn_rate(P, D, 64, r);
            for (i = 0; i < 64u; i++) assert(D[i] == (uint32_t)((uint64_t)P[i] * NUM[r] / DEN[r]) % 0x80000000u);
        }
        setup(0);
        transport_req = 1; render(FS / CTL, 0);
        assert(song.playing && !fp_st[0].act);
        fp_kp[0][FK_RATE] = 2;                          /* (2X) */
        for (i = 0; i < 4000u && !fp_st[0].act; i++) render(1, 0);
        assert(fp_st[0].act == ((2u + 3u) ^ 3u) && (func_clock_end >> 18) != (func_clock_begin >> 18) && math_fade[0] > 0u);
        fp_kp[0][FK_RATE] = 0; render(4000, 0); assert(!fp_st[0].act);
        transport_req = 2;
    }
    puts("funcsmooth: RATE 1/4X .. 4X on the grid, at the bar, faded PASS");
    /* 5 RATE and LAYER B's saved; a project of before: 1X, B as A's */
    {
        setup(4);
        for (k = 0; k < 5u; k++) fp_kp[0][FK_RATE + k] = (int16_t)(k == 1u ? -6 : k == 2u ? 40 : 1 + (int)k / 3);
        project_capture(&a);
        assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw));
        for (k = 0; k < 5u; k++) fp_kp[0][FK_RATE + k] = 0;
        assert(!project_restore_runtime(&b) && fp_kp[0][FK_RATE] == 1 && fp_kp[0][FK_BLEV] == -6 && fp_kp[0][FK_BTONE] == 40 &&
               fp_kp[0][FK_BOCT] == 2 && fp_kp[0][FK_BRATE] == 2);
        project_capture(&a); memset(a.fxr, 0, sizeof a.fxr);   /* (one of before: no 'F' 'X') */
        assert(proj_pack(&stored, &a) && proj_import(&b, stored.raw, sizeof stored.raw));
        assert(!project_restore_runtime(&b));
        for (k = 0; k < 5u; k++) assert(!fp_kp[0][FK_RATE + k]);
    }
    puts("funcsmooth: RATE and LAYER B saved in the project; one of before 1X PASS");
    puts("funcsmooth: all ok");
    return 0;
}
