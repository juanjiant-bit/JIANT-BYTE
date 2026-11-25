/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE 0.17) FUNC as an engine of its own (eng_lofi.c ENG_FUNC): its number, its place in the picker, its pages and
 * names; a project of before it (MATH WAVE FUNC) loading as FUNC, one of it stored as MATH WAVE FUNC (a firmware without
 * it loads it); every FORM's preset sounding, none clipping, at its own knobs and at their ends; the new recipes'
 * rhythms where they are written. With a directory as its argument: a WAV of each new recipe there */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
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
static int16_t pcm[FS * 9u * 2u], dpcm[FS * 24u * 2u];
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

/* ---- (funcplay) FUNC played live: its sections, fills, groove, layers, the keys' loop */
#define FPN (FS * 12u)
static int16_t *fa, *fbuf;
static uint32_t fpb[FPN / CTL + 4u];                   /* each block's P (Q16 beats) at its start */
typedef void (*fp_cb_t)(uint32_t blk);
static uint32_t fp_beat_blk(double beat)               /* the block whose P range holds beat */
{
    uint32_t j;
    for (j = 1; j < FPN / CTL; j++) if (fpb[j] > (uint32_t)(beat * 65536.0)) return j - 1u;
    return 0;
}
static void fp_setup(uint32_t form)
{
    setup(form - 1u);
    TSEL->p[P_DLY] = TSEL->p[P_REV] = TSEL->p[P_CHOR] = 0;
    memset(fp_st, 0, sizeof fp_st);
    memset(fp_kp[0] + 2, 0, 4u * sizeof fp_kp[0][0]); memset(fn_sg, 0, sizeof fn_sg);   /* (funcmotion: MOTION still,
                                                         * its own checks in funcmotion_test.c) */
    song.g[G_BPM] = 120;
    {   /* (the same start each time: the buses' tails out, the generators' seeds) */
        int32_t x[CTL * 2]; uint32_t j;
        for (j = 0; j < FS * 2u / CTL; j++) mix_block(x, CTL);
        rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
        ap_rng = 0x2545F491u; pfx_seed = 0x9E3779B9u; mns_clk = 0; lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0;
        memset(TSEL->v, 0, sizeof TSEL->v); TSEL->lfo_ph = 0; TSEL->lfo_val = TSEL->lfo_fade = 0; fx_trip(0, CTL); memset(&fx, 0, sizeof fx);
        tf_ic1[0] = tf_ic2[0] = 0; memset(fns, 0, sizeof fns); memset(fnb, 0, sizeof fnb); drift_clk[0] = 0; pl_g[0] = 0; dist_w[0] = 0; memset(l24_s, 0, sizeof l24_s); memset(ms_s, 0, sizeof ms_s); memset(dist_nlp, 0, sizeof dist_nlp);
    }
}
static int fp_stem;                                    /* (momentary MUTATE) the part's own (its stem: no master's leveler) */
/* the transport playing ms from its start, the left channel into o (fp_stem: the part's stem), a call before each block */
static void fp_play(uint32_t ms, int16_t *o, fp_cb_t cb)
{
    int32_t x[CTL * 2];
    uint32_t n = FS / 1000u * ms / CTL, j, i;
    if (fp_stem) fx_stems = 1;
    transport_req = 1;
    for (j = 0; j < n && (j + 1u) * CTL <= FPN; j++) {
        if (cb) cb(j);
        mix_block(x, CTL);
        fpb[j] = func_clock_begin;
        for (i = 0; i < CTL; i++) o[j * CTL + i] = (int16_t)clamp(fp_stem ? stem_buf[0][i] : x[2u * i], -32768, 32767);
    }
    fpb[j] = func_clock_end;
    transport_req = 2;
    for (j = 0; j < FS / 4u / CTL; j++) mix_block(x, CTL);
}
/* (each run from the same state: a child process plays it, the parent's state untouched; its output, its blocks' P
 * and its FUNC state after, shared) */
static struct { uint32_t fpb[FPN / CTL + 4u]; fp_t st; } *fp_sh;
static void fp_run(uint32_t ms, int16_t *o, fp_cb_t cb)
{
    int st = 0;
    pid_t pid;
    fflush(stdout);
    pid = fork();
    if (!pid) { fp_play(ms, o, cb); memcpy(fp_sh->fpb, fpb, sizeof fpb); fp_sh->st = fp_st[0]; _exit(0); }
    waitpid(pid, &st, 0);
    assert(WIFEXITED(st) && !WEXITSTATUS(st));
    memcpy(fpb, fp_sh->fpb, sizeof fpb);
}
static uint32_t fp_first_diff(const int16_t *a, const int16_t *b, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && a[i] == b[i]; i++) {}
    return i;
}
static double fp_rms(const int16_t *a, uint32_t from, uint32_t to)
{
    double e = 0; uint32_t i;
    for (i = from; i < to; i++) e += (double)a[i] * a[i];
    return sqrt(e / (to > from ? to - from : 1u));
}
static uint32_t fp_at_blk, fp_ev, fp_key, fp_up_blk;
static void fp_cb_ev(uint32_t j) { if (j == fp_at_blk) func_apply(0, fp_ev); }
static void fp_cb_key(uint32_t j)                      /* a key down at fp_at_blk, up at fp_up_blk: the keyboard's way */
{
    if (j == fp_at_blk) fm1_in.notes = 1u << fp_key;
    if (j == fp_up_blk) fm1_in.notes = 0;
}
static void fp_cb_two(uint32_t j)                      /* FREEZE (black 4) fp_at_blk .. fp_up_blk, ROLL (white 6) 12.6 .. 13.1 */
{
    uint32_t n = 0;
    if (j >= fp_at_blk && j < fp_up_blk) n |= 1u << 8;
    if (j >= fp_beat_blk(12.6) && j < fp_beat_blk(13.1)) n |= 1u << 9;
    fm1_in.notes = n;
}
static void funcplay_checks(int argc, char **argv)
{
    uint32_t j, d, k, f;
    fa = mmap(0, FPN * 4u + sizeof *fp_sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(fa != MAP_FAILED);
    fbuf = fa + FPN; fp_sh = (void *)(fbuf + FPN);
    /* 1 as written at the middles (old projects, presets): its render the old path; the live path at the middles the
     * same samples, bit for bit (a GROOVE asked: func_run runs, nothing moves) */
    for (f = 1; f < FUNC_NFORM; f++) {
        fp_setup(f);
        for (k = 0; k < 5u; k++) assert(TSEL->p[FUNC_PID[k]] == FUNC_PX[k].def);
        assert(fp_kp[0][0] == 64 && !fp_kp[0][1]);
        assert(!func_live(TSEL));
        fp_run(3000, fa, 0);
        fp_at_blk = 0; fp_ev = 1;                           /* (GROOVE asked at the start: the live path all along) */
        fp_run(3000, fbuf, fp_cb_ev);
        d = fp_first_diff(fa, fbuf, FS * 3u);
        if (d < FS * 3u - CTL) fprintf(stderr, "FORM %u: the live path differs at %u (%d %d)\n", f, d, fa[d], fbuf[d]);
        assert(d >= FS * 3u - CTL);
    }
    puts("funcplay: every FORM at the middles: the old render, and the live path bit for bit PASS");
    /* 2 a section lands on the bar line: INTRO asked in bar 2 changes nothing before bar 3's first sample */
    for (f = 1; f < FUNC_NFORM; f += 3u) {
        uint32_t b8;
        fp_setup(f); fp_run(8000, fa, 0);
        fp_at_blk = fp_beat_blk(5.3); fp_ev = 2;            /* (INTRO) */
        fp_run(8000, fbuf, fp_cb_ev);
        b8 = fp_beat_blk(8.0);
        d = fp_first_diff(fa, fbuf, FS * 8u);
        printf("FORM %2u INTRO asked at beat 5.3: first change at block %u (bar 3 in block %u)\n", f, d / CTL, b8);
        assert(d / CTL >= b8 && d / CTL <= b8 + 1u && d < FS * 8u && fp_sh->st.sec == FS_INTRO);
        fp_run(8000, fa, fp_cb_ev);                         /* (the same again: deterministic) */
        assert(fp_first_diff(fa, fbuf, FS * 8u) == FS * 8u);
    }
    /* INTRO and BREAK: the drums gone (AMEN, all drums: near silence), BUILD denser, DROP denser */
    {
        double r[5];
        static const uint8_t EV[5] = {1, 2, 3, 4, 5};
        for (k = 0; k < 5u; k++) {
            if (!k) fp_setup(16);
            fp_at_blk = 0; fp_ev = EV[k];
            fp_run(6000, fa, fp_cb_ev);
            r[k] = fp_rms(fa, FS * 2u + FS / 10u, FS * 6u);
        }
        printf("AMEN by section: GROOVE %.0f INTRO %.0f BUILD %.0f DROP %.0f BREAK %.0f\n", r[0], r[1], r[2], r[3], r[4]);
        assert(r[1] < r[0] * 0.05 && r[4] < r[0] * 0.05 && r[3] > r[0] && r[2] > r[1]);
    }
    puts("funcplay: sections on the bar line, deterministic; INTRO / BREAK no drums, BUILD and DROP denser PASS");
    /* 3 a fill held through the keys (white 6: ROLL): from the next 1/16, and back as written after the 1/16 past the
     * key-up (the formula's time; the filter's memory a little longer) */
    {
        uint32_t s0, e0;
        fp_setup(16); fp_run(6000, fa, 0);
        fp_key = 9; fp_at_blk = fp_beat_blk(4.1); fp_up_blk = fp_beat_blk(5.6);
        fp_run(6000, fbuf, fp_cb_key);
        s0 = fp_first_diff(fa, fbuf, FS * 6u) / CTL;
        assert(s0 >= fp_beat_blk(4.25) && s0 < fp_beat_blk(5.75));
        printf("ROLL: from beat %.4f to %.4f\n", fp_sh->st.fs / 65536.0, fp_sh->st.fe / 65536.0);
        assert(fp_sh->st.fs == 0x44000u && fp_sh->st.fe == 0x5C000u);      /* (pressed at 4.1, up at 5.6: 4.25 .. 5.75) */
        for (e0 = FS * 6u - 1u; e0 > 0 && fa[e0] == fbuf[e0]; e0--) {}
        printf("ROLL held 4.1 .. 5.6: changed blocks %u .. %u (the 1/16s at 4.25: %u, 5.75: %u)\n", s0, e0 / CTL,
               fp_beat_blk(4.25), fp_beat_blk(5.75));
        assert(fp_sh->st.fill == 0 && !fp_sh->st.fkey);
        {   /* after 5.75 + a beat: within a hair of as written */
            uint32_t a0 = (fp_beat_blk(6.75) + 1u) * CTL, i; int32_t mx = 0;
            for (i = a0; i < FS * 6u; i++) { int32_t q = fa[i] - fbuf[i]; if (q < 0) q = -q; if (q > mx) mx = q; }
            printf("  after the release: the largest difference %d\n", mx);
            assert(mx < 64);
        }
        /* each fill on the formula's time: grid-locked from its start, its time back after its end */
        for (k = FF_ROLL; k <= FF_DBL; k++) {
            fp_t ff; fp_q_t q;
            memset(&ff, 0, sizeof ff); memset(&q, 0, sizeof q); q.ck = q.rc = ~0u;
            ff.fill = (uint8_t)k; ff.fs = 0x40000u + 0x4000u; ff.fe = ff.fs + 0x10000u;
            assert(func_tp(&ff, &q, ff.fs - 1u) == ff.fs - 1u && func_tp(&ff, &q, ff.fe) == ff.fe && func_tp(&ff, &q, ff.fe + 777u) == ff.fe + 777u);
            assert(func_tp(&ff, &q, ff.fs) == ff.fs);
            assert(func_tp(&ff, &q, ff.fs + 0xB000u) != ff.fs + 0xB000u);
        }
    }
    puts("funcplay: a fill held on the keys: from the next 1/16, released at the 1/16 after, as written again PASS");
    /* 3b (momentary MUTATE) a mutation held on the keys (blacks 1..4: NOTES RHYTHM SWAP FREEZE): from the next 1/16,
     * at the 1/16 after the key is up as written again, nothing kept (the formula's state, its sound); a tap one 1/16;
     * held with a fill: both */
    {
        static const uint8_t MK[4] = {1, 3, 5, 8};      /* (CLAY_KMAP: 16 .. 19) */
        uint32_t s0, e0, m, i;
        int32_t mx;
        fp_stem = 1;
        for (f = 2; f <= 16u; f += 14u) {
            fp_setup(f); fp_run(12000, fa, 0);
            for (m = 0; m < 4u; m++) {
                fp_key = MK[m]; fp_at_blk = fp_beat_blk(6.1); fp_up_blk = fp_beat_blk(17.6);
                fp_run(12000, fbuf, fp_cb_key);
                s0 = fp_first_diff(fa, fbuf, FS * 12u) / CTL;
                for (e0 = FS * 12u - 1u; e0 > 0 && fa[e0] == fbuf[e0]; e0--) {}
                for (mx = 0, i = (fp_beat_blk(18.75) + 1u) * CTL; i < FS * 12u; i++) { int32_t q = fa[i] - fbuf[i]; if (q < 0) q = -q; if (q > mx) mx = q; }
                printf("FORM %2u %s held 6.1 .. 17.6: on %.4f off %.4f; changed blocks %u .. %u (6.25: %u, 17.75: %u), after %d\n",
                       f, "NOTES\0 RHYTHM\0SWAP\0  FREEZE" + 7u * m, fp_sh->st.ms / 65536.0, fp_sh->st.mr / 65536.0, s0,
                       e0 / CTL, fp_beat_blk(6.25), fp_beat_blk(17.75), mx);
                assert(fp_sh->st.ms == 0x64000u && fp_sh->st.mr == 0x11C000u);
                assert(!fp_sh->st.mut && !fp_sh->st.mkey && !fp_sh->st.mreq && !fp_sh->st.mpf);
                assert(s0 < FS * 12u / CTL && s0 >= fp_beat_blk(6.25) && s0 <= fp_beat_blk(17.75));
                assert(e0 / CTL <= fp_beat_blk(18.75) && !mx);   /* (the part's own: as written, bit for bit) */
            }
        }
        /* a tap: one 1/16 (on 6.25, off 6.5) */
        fp_setup(16); fp_run(8000, fa, 0);
        fp_key = 3; fp_at_blk = fp_beat_blk(6.1); fp_up_blk = fp_at_blk + 2u;
        fp_run(8000, fbuf, fp_cb_key);
        assert(fp_sh->st.ms == 0x64000u && fp_sh->st.mr == 0x68000u && !fp_sh->st.mut);
        assert(fp_first_diff(fa, fbuf, FS * 8u) / CTL >= fp_beat_blk(6.25));
    }
    {   /* FREEZE held 6.1 .. 17.6 with ROLL held 12.6 .. 13.1: both, then as written */
        int16_t *fz = mmap(0, FPN * 4u, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0), *rl = fz + FPN;   /* (the
                                                         * children's: shared) */
        uint32_t i, both = 0, d1, d2;
        assert(fz != MAP_FAILED);
        fp_setup(16); fp_run(12000, fa, 0);
        fp_key = 8; fp_at_blk = fp_beat_blk(6.1); fp_up_blk = fp_beat_blk(17.6); fp_run(12000, fz, fp_cb_key);
        fp_key = 9; fp_at_blk = fp_beat_blk(12.6); fp_up_blk = fp_beat_blk(13.1); fp_run(12000, rl, fp_cb_key);
        fp_at_blk = fp_beat_blk(6.1); fp_up_blk = fp_beat_blk(17.6); fp_run(12000, fbuf, fp_cb_two);
        for (i = fp_beat_blk(12.75) * CTL; i < fp_beat_blk(13.25) * CTL; i++) both += fbuf[i] != fz[i] && fbuf[i] != rl[i];
        d1 = fp_first_diff(fz, fbuf, FS * 12u) / CTL; d2 = fp_first_diff(rl, fbuf, FS * 12u) / CTL;
        printf("FREEZE + ROLL: %u samples of 12.75 .. 13.25 neither's alone; off FREEZE's from block %u (12.75: %u), ROLL's from %u\n",
               both, d1, fp_beat_blk(12.75), d2);
        assert(both > 1000u && d1 >= fp_beat_blk(12.75) && d2 >= fp_beat_blk(6.25) && d2 < fp_beat_blk(12.75));
        assert(!fp_sh->st.mut && !fp_sh->st.fill && !fp_sh->st.mkey && !fp_sh->st.fkey);
        for (i = (fp_beat_blk(18.75) + 1u) * CTL; i < FS * 12u; i++) assert(fa[i] == fbuf[i]);   /* (as written again) */
        munmap(fz, FPN * 4u);
        fp_stem = 0;
    }
    puts("funcplay: a mutation held on the keys: from the next 1/16, as written at the 1/16 after, with a fill both PASS");
    /* 4 the groove: SWING's odd 1/16 later, COMPLEX broken by its share, DENSITY thins / fills, RANGE octaves */
    {
        fp_t ff; fp_q_t q; uint32_t r, late = 0, moved, sw[3];
        memset(&ff, 0, sizeof ff);
        memset(&q, 0, sizeof q); q.ck = q.rc = ~0u; q.S = 6000u; q.r1 = (0x4000u << 16) / (0x4000u + q.S); q.r2 = (0x4000u << 16) / (0x4000u - q.S);
        for (r = 0; r < 0x10000u && func_tp(&ff, &q, r) < 0x4000u; r++) {}
        late = r;
        assert(late >= 0x4000u + 5990u && late <= 0x4000u + 6010u && func_tp(&ff, &q, 0x8000u) == 0x8000u && func_tp(&ff, &q, 0) == 0);
        for (k = 0; k <= 127u; k += 127u) {
            memset(&q, 0, sizeof q); q.ck = q.rc = ~0u; q.cpx = k; q.seed = 99u;
            for (moved = 0, r = 0; r < 64u; r++) moved += func_tp(&ff, &q, r << 14 | 0x100u) >> 14 != r;
            sw[k ? 1 : 0] = moved;
        }
        memset(&q, 0, sizeof q); q.ck = q.rc = ~0u; q.cpx = 64u; q.seed = 99u;
        for (moved = 0, r = 0; r < 64u; r++) moved += func_tp(&ff, &q, r << 14 | 0x100u) >> 14 != r;
        sw[2] = moved;
        printf("SWING 100%%: step 2 at %.4f beat; COMPLEX 0 / 50 / 100 %%: %u %u %u of 64 steps moved\n", late / 65536.0, sw[0], sw[2], sw[1]);
        assert(sw[0] == 0 && sw[2] > 16u && sw[2] < 48u && sw[1] >= 60u);
        {   /* DENSITY: AMEN's hits away at -64, fewer at -32, more at +32 (its energy); ACID's eu() steps */
            double r0, rm, rl, rp, a0, am;
            fp_setup(16); fp_run(4000, fa, 0); r0 = fp_rms(fa, FS / 2u, FS * 4u);
            TSEL->p[P_FDEN] = -64; fp_run(4000, fa, 0); rm = fp_rms(fa, FS / 2u, FS * 4u);
            TSEL->p[P_FDEN] = -32; fp_run(4000, fa, 0); rl = fp_rms(fa, FS / 2u, FS * 4u);
            TSEL->p[P_FDEN] = 40; fp_run(4000, fa, 0); rp = fp_rms(fa, FS / 2u, FS * 4u);
            fp_setup(5); fp_run(4000, fa, 0); a0 = fp_rms(fa, FS / 2u, FS * 4u);
            TSEL->p[P_FDEN] = -64; fp_run(4000, fa, 0); am = fp_rms(fa, FS / 2u, FS * 4u);
            printf("DENSITY AMEN -64 %.0f -32 %.0f 0 %.0f +40 %.0f; ACID 0 %.0f -64 %.0f\n", rm, rl, r0, rp, a0, am);
            assert(rm < r0 * 0.02 && rl < r0 && rp > r0 && am < a0 * 0.8);
        }
        {   /* RANGE: every note an octave up at +64, down at -64; the calls' pitch (fu_music) */
            fu_cx_t c = {0, 64, 0, 0, 5u};
            uint32_t P = 0x12345u, T0 = 0, x, y, i, up = 0;
            const uint32_t *ar[FA_N] = {0};
            fvm_env_t e = {{0}, {0}, 0, &c};
            ar[FA_BEAT] = &P; ar[FA_T] = &T0;
            memcpy(e.arr, ar, sizeof ar);
            for (i = 0; i < 16u; i++) { x = 60; y = 0x1000u; P = i << 14 | 0x1000u; assert(!fu_music(0, &x, &y, &e, 0)); up += x == 72u; }
            c.range = -64;
            for (i = 0; i < 16u; i++) { x = 60; y = 0x1000u; P = i << 14 | 0x1000u; assert(!fu_music(0, &x, &y, &e, 0)); assert(x == 48u); }
            c.range = 0; c.mask = 1;
            x = 0x1000u; y = 0; assert(fu_music(2, &x, &y, &e, 0) && !fu_music(0, &x, &y, &e, 0));
            assert(up == 16u);
        }
        {   /* SWING, COMPLEX, RANGE heard: the pattern moves (and back as written at their middles: part 1) */
            fp_setup(1); fp_run(3000, fa, 0);
            TSEL->p[P_FSWG] = 100; fp_run(3000, fbuf, 0); assert(fp_first_diff(fa, fbuf, FS * 3u) < FS * 3u); TSEL->p[P_FSWG] = 0;
            TSEL->p[P_FCPX] = 20; fp_run(3000, fbuf, 0); assert(fp_first_diff(fa, fbuf, FS * 3u) < FS * 3u); TSEL->p[P_FCPX] = 127;
            TSEL->p[P_FRNG] = 50; fp_run(3000, fbuf, 0); assert(fp_first_diff(fa, fbuf, FS * 3u) < FS * 3u); TSEL->p[P_FRNG] = 0;
        }
    }
    puts("funcplay: SWING the odd 1/16s later, COMPLEX its share broken, DENSITY thins and fills, RANGE octaves PASS");
    /* 5 layers: B at BLEND 0 nothing (bit for bit), at 127 B alone (as A playing it), in the middle both */
    {
        double ra, rb, rab;
        fp_setup(16); fp_run(4000, fa, 0); ra = fp_rms(fa, FS / 2u, FS * 4u);
        TSEL->p[P_FFB] = 5; fp_kp[0][0] = 0; math_poll(); assert(fp_b[0].tr == 1u && fp_b[0].form == 5u);
        fp_run(4000, fbuf, 0); assert(fp_first_diff(fa, fbuf, FS * 4u) == FS * 4u);
        fp_kp[0][0] = 127; fp_run(4000, fbuf, 0);
        TSEL->p[P_FFB] = 0; math_poll(); TSEL->p[P_E7] = 5; for (k = 0; k < 4u; k++) TSEL->p[P_E2 + k] = FUNC_PRESETS[4].e[2u + k];
        math_poll(); fp_run(4000, fa, 0); rb = fp_rms(fa, FS / 2u, FS * 4u);
        {
            int32_t mx = 0, q; uint32_t i;
            for (i = 0; i < FS * 4u; i++) { q = fa[i] - fbuf[i]; if (q < 0) q = -q; if (q > mx) mx = q; }
            printf("AMEN + ACID: BLEND 127 against ACID alone: the largest difference %d\n", mx);
            assert(mx < 16);
        }
        fp_setup(16); TSEL->p[P_FFB] = 5; fp_kp[0][0] = 64; math_poll(); fp_run(4000, fbuf, 0);
        rab = fp_rms(fbuf, FS / 2u, FS * 4u);
        printf("AMEN %.0f ACID %.0f AMEN+ACID (BLEND 50 %%) %.0f\n", ra, rb, rab);
        assert(rab > ra * 0.6 && rab > rb * 0.6);
        /* B muted (black 8): A alone again, bit for bit */
        fp_setup(16); TSEL->p[P_FFB] = 17; math_poll();
        fp_run(2000, fbuf, 0);
        TSEL->p[P_FFB] = 0; fp_run(2000, fa, 0); assert(fp_first_diff(fa, fbuf, FS * 2u) < FS * 2u);
        TSEL->p[P_FFB] = 17; fp_st[0].mute = 4u; fp_run(2000, fbuf, 0); fp_st[0].mute = 0;
        d = fp_first_diff(fa, fbuf, FS * 2u);
        assert(d >= FS * 2u - 256u);                    /* (2 s of blocks: 88000 samples) */
        /* a third part with B: no slot (two at once), its A alone */
        for (k = 1; k < 3u; k++) { set_engine_of(&trk[k], ENGI_FUNC); trk[k].eng_req = ENGI_FUNC; trk[k].p[P_FFB] = 3; }
        math_poll();
        assert(fp_b[0].tr == 1u && fp_b[1].tr == 2u);
        trk[1].p[P_FFB] = 0; math_poll(); assert(fp_b[1].tr == 3u);
    }
    puts("funcplay: layer B blends (0 nothing, 127 B alone, between both), its mute, two slots PASS");
    /* 6 the project: a FUNC part's GROOVE (its slots), its BLEND and KEYS ('F' 'P'), saved and loaded; a project of
     * before (no 'F' 'P' block) as written whatever its slots held */
    {
        project_t *pa = &a, *pb2 = &b;
        fp_setup(1);
        TSEL->p[P_FDEN] = -20; TSEL->p[P_FSWG] = 40; TSEL->p[P_FFB] = 16; fp_kp[0][0] = 90; fp_kp[0][1] = 1;
        project_capture(pa);
        assert(pa->fpx[0] == (128u | 4u) && pa->fbl[0] == 90u);
        assert(proj_pack(&stored, pa) && proj_import(pb2, stored.raw, sizeof stored.raw) && pb2->fpx[0] == pa->fpx[0] && pb2->fbl[0] == 90u);
        fp_kp[0][0] = 64; fp_kp[0][1] = 0; TSEL->p[P_FDEN] = 0;
        assert(!project_restore_runtime(pb2));
        assert(fp_kp[0][0] == 90 && fp_kp[0][1] == 1 && TSEL->p[P_FDEN] == -20 &&
               TSEL->p[P_FSWG] == 40 && TSEL->p[P_FFB] == 16 && fp_st[0].sec == FS_GROOVE);
        project_capture(pa); pa->fpx[0] = 0; pa->t[0].p[P_FFB] = 3; pa->t[0].p[P_FSWG] = 90;
        assert(proj_pack(&stored, pa) && proj_import(pb2, stored.raw, sizeof stored.raw) && !pb2->fpx[0]);
        assert(!project_restore_runtime(pb2) && TSEL->p[P_FFB] == 0 && TSEL->p[P_FSWG] == 0 && fp_kp[0][0] == 64 && !func_live(TSEL));
    }
    puts("funcplay: GROOVE, BLEND and KEYS saved in the project and loaded; an old project as written PASS");
    /* 7 the keys: the selected FUNC part's alone, PLAY KEYS ON, running; OFF (or stopped) they are notes again */
    {
        int32_t x[CTL * 2];
        fp_setup(1);
        transport_req = 1; for (j = 0; j < 20u; j++) mix_block(x, CTL);
        fm1_in.notes = 1u << 7; mix_block(x, CTL); fm1_in.notes = 0; mix_block(x, CTL);   /* (white 5: BREAK) */
        assert(fp_st[0].sreq == 5u || fp_st[0].sec == FS_BREAK);
        fp_kp[0][1] = 1; memset(fp_st, 0, sizeof fp_st);
        fm1_in.notes = 1u << 7; mix_block(x, CTL); fm1_in.notes = 0; mix_block(x, CTL);
        assert(!fp_st[0].sreq && fp_st[0].sec == FS_GROOVE);
        fp_kp[0][1] = 0;
        transport_req = 2; for (j = 0; j < 40u; j++) mix_block(x, CTL);
        fm1_in.notes = 1u << 7; mix_block(x, CTL); fm1_in.notes = 0; mix_block(x, CTL);   /* (stopped: a note) */
        assert(!fp_st[0].sreq);
        func_gen_set(0, FS_DROP, 20); assert(fp_st[0].sreq == FS_DROP + 1u && fp_st[0].gden == 20);
    }
    puts("funcplay: the keys the selected FUNC part's (KEYS ON, playing), notes otherwise; GEN's hook PASS");
    /* 8 its pages: RECIPE KNOBS GROOVE LAYERS, FUNC's alone; the names, their help */
    {
        fp_setup(1);
        assert(nav_put(SEC_EDIT, "GROOVE") && !strcmp(nav_page_name(), "GROOVE")); frame();
        assert(nav_put(SEC_EDIT, "LAYERS") && !strcmp(nav_page_name(), "LAYERS")); frame();
        for (k = 0; k < 4u; k++) { int16_t *vp; const param_desc_t *dd = page_desc(cur_page(), k & 3u, &vp); (void)dd; }
        for (k = 0; k < NELEM(FUNC_PX); k++) assert(strlen(FUNC_PX[k].label) + 2u + strlen(FUNC_PXT[k]) <= 26u && func_knob_tip(&FUNC_PX[k]));
        for (k = 0; k < 5u; k++) assert(track_desc(TSEL, FUNC_PID[k]) == &FUNC_PX[k]);
        for (k = MD_FX1; k < MD_N; k++) assert(mod_dst_ok(TSEL, k) && mod_param(k) == FUNC_XDST[k - MD_FX1]);
        select_engine(ENGI_LOFI); frame();
        assert(!nav_put(SEC_EDIT, "GROOVE") && !mod_dst_ok(TSEL, MD_FX1));
    }
    puts("funcplay: GROOVE and LAYERS FUNC's own pages, named, a line of help each; the matrix's DENSITY .. BLEND PASS");
    if (argc > 1) {                                     /* the demos: INTRO -> BUILD -> DROP with a ROLL; DENSITY, SWING
                                                         * swept; AMEN + ACID */
        char path[300];
        int32_t x[CTL * 2]; uint32_t n, i, at;
        /* HOUSE: INTRO two bars, BUILD four (asked in bar 2: from bar 3), a ROLL held over its last two beats, DROP four */
        fp_setup(7); fp_st[0].sec = FS_INTRO;
        n = FS * 20u / CTL; at = 0; transport_req = 1;
        for (j = 0; j < n; j++) {
            uint32_t P = func_clock_end;
            if (P >= 0x70000u && !fp_st[0].sreq && fp_st[0].sec == FS_INTRO) func_apply(0, 3);
            if (P >= 0x170000u && P < 0x178000u && fp_st[0].sec == FS_BUILD && !fp_st[0].sreq) func_apply(0, 4);
            fp_st[0].fkey = P >= 0x160000u && P < 0x17C000u ? FF_ROLL : 0;
            mix_block(x, CTL);
            for (i = 0; i < CTL; i++) if (at < NELEM(dpcm) / 2u) { dpcm[2u * at] = (int16_t)clamp(x[2u * i], -32768, 32767); dpcm[2u * at + 1u] = (int16_t)clamp(x[2u * i + 1u], -32768, 32767); at++; }
        }
        transport_req = 2; for (j = 0; j < 40u; j++) mix_block(x, CTL);
        snprintf(path, sizeof path, "%s/funcplay_house_intro_build_drop_roll.wav", argv[1]); wav(path, dpcm, at); printf("wrote %s\n", path);
        for (k = 0; k < 2u; k++) {                      /* DENSITY -64 .. +64, SWING 0 .. 100 %, over 16 s */
            fp_setup(k ? 1 : 16);
            n = FS * 16u / CTL; at = 0; transport_req = 1;
            for (j = 0; j < n; j++) {
                if (k) TSEL->p[P_FSWG] = (int16_t)(j * 127u / n);
                else TSEL->p[P_FDEN] = (int16_t)((int32_t)(j * 128u / n) - 64);
                mix_block(x, CTL);
                for (i = 0; i < CTL; i++) if (at < NELEM(dpcm) / 2u) { dpcm[2u * at] = (int16_t)clamp(x[2u * i], -32768, 32767); dpcm[2u * at + 1u] = (int16_t)clamp(x[2u * i + 1u], -32768, 32767); at++; }
            }
            transport_req = 2; for (j = 0; j < 40u; j++) mix_block(x, CTL);
            snprintf(path, sizeof path, "%s/funcplay_%s.wav", argv[1], k ? "song_swing_sweep" : "amen_density_sweep");
            wav(path, dpcm, at); printf("wrote %s\n", path);
        }
        fp_setup(16); TSEL->p[P_FFB] = 5; fp_kp[0][0] = 64; math_poll();
        { int32_t pk; double r; play(8000, &pk, &r, 1); }
        snprintf(path, sizeof path, "%s/funcplay_amen_acid_layers.wav", argv[1]); wav(path, pcm, FS * 8u); printf("wrote %s\n", path);
    }
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

    /* its pages: RECIPE (FORM RUN ROOT CUT) and KNOBS (A B C D by FORM), no other engine's EDIT; MATH not theirs */
    ui_power_on(); song.sel = 0; select_engine(ENGI_FUNC); frame();
    assert(TSEL->eng_req == ENGI_FUNC && TSEL->p[P_E1] == RW_FUNC && TSEL->p[P_E7] == 1);
    {
        uint32_t seen = 0, i;
        for (i = 0; i < NPAGES; i++) {
            if (PAGES[i].scope != SC_ENGINE || PAGES[i].fam != FAM_EDIT || !page_visible(i)) continue;
            if (PAGES[i].id[0] == P_E7) { assert(PAGES[i].id[1] == P_E0 && PAGES[i].id[2] == P_E6 && PAGES[i].id[3] == ID_FM0 + 4u); seen |= 1; }
            else { assert(PAGES[i].id[0] == P_E2 && PAGES[i].id[3] == P_E5); seen |= 2; }
            assert(!strcmp(ENG_FUNC.page_title[PG_TI(&PAGES[i])], PAGES[i].id[0] == P_E7 ? "RECIPE" : "KNOBS"));
        }
        assert(seen == 3u);
        assert(nav_put(SEC_EDIT, "EDIT 1") && cur_page()->id[0] == P_E7 && !strcmp(nav_page_name(), "RECIPE"));
        frame();
        assert(nav_put(SEC_EDIT, "EDIT 2") && cur_page()->id[0] == P_E2 && !strcmp(nav_page_name(), "KNOBS"));
        frame();
        for (f = 1; f < FUNC_NFORM; f++) {               /* the knobs by FORM's names, the cards' and the matrix's */
            TSEL->p[P_E7] = (int16_t)f;
            for (k = 0; k < 4u; k++) assert(!strcmp(track_desc(TSEL, P_E2 + k)->label, func_kn(f, k)) && track_desc(TSEL, P_E2 + k)->max == 127 &&
                                            !strcmp(func_knob_tip(track_desc(TSEL, P_E2 + k)), func_tip(f, k)));
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
            for (k = 0; k < 4u; k++) assert(!strcmp(func_kn(f, k), N[f - 15u][k]) && strlen(func_tip(f, k)) > 4u);
    }
    puts("funcengine: RECIPE and KNOBS its own EDIT pages, by their names; MATH's its own; the new FORMs' knobs named PASS");

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

    funcplay_checks(argc, argv);
    if (argc > 2) {                                     /* the new recipes heard: a WAV each (8 s, 120 BPM) */
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
