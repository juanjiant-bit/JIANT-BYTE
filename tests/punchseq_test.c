/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE) GLO > PUNCH SEQ (pfx.c psq, ui.c psq_*): a row's step lit at the playhead holds its punch-in on every track as
 * its key would (OCT+ the pitch, HALF the time, DRONE CLAY's restart), let go when the step goes off and at STOP; a key
 * held live keeps its effect on whatever the rows say; the page's knobs and keys; stored in the project ('P' 'Q') and
 * read back; an older project (no block) loads it empty; CLEAR (system_init) empties it */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int pq_bad;
#define OK(c, ...) do { int ok_ = (c); printf("punchseq: "); printf(__VA_ARGS__); printf("    %s\n", ok_ ? "ok" : "FAIL"); pq_bad += !ok_; } while (0)
static int32_t pq_buf[CTL * 2];
static void blocks(uint32_t n) { while (n--) mix_block(pq_buf, CTL); }
static void until_step(uint32_t s)                      /* the blocks until the playhead is on step s (bounded) */
{
    uint32_t i;
    for (i = 0; i < 20000u && psq_at != s; i++) mix_block(pq_buf, CTL);
}
static int all_pit(int v)
{
    uint32_t k, ok = 1;
    for (k = 0; k < NTRK; k++) ok &= trk[k].pfx_pit == v;
    return (int)ok;
}
static void setup(void)
{
    ui_power_on(); song.sel = 0; frame();
    psq_clear();
    song.master_q12 = 4096;
    perf_held = perf_latched = 0; perf_kill = 0; scene_pfx = 0;
}
static void stop(void) { transport_req = 2; blocks(40); }
int main(int argc, char **argv)
{
    (void)argv; if (argc > 1) return 0;
    {   /* a step lit: its punch-in on every track; the next step off: let go; STOP: let go (HALF: the formulas' time,
         * pt_ops, as its key's: no PFX_HALF since BYTE 0.16) */
        setup();
        psq.fx[1] = 1;                                  /* row 2 OCT+ */
        psq.st[1] = 1u << 0 | 1u << 2;
        psq.fx[2] = 2;                                  /* row 3 HALF */
        psq.st[2] = 1u << 2;
        transport_req = 1; blocks(4);
        until_step(0); blocks(2);
        int on0 = all_pit(192) && (psq_now & PF_BIT(PF_OCTU)) && !(psq_now & PF_BIT(PF_HALF));
        until_step(1); blocks(2);
        int off1 = all_pit(0) && !psq_now;
        until_step(2); blocks(2);
        int both = all_pit(192) && (psq_now & PF_BIT(PF_OCTU)) && (psq_now & PF_BIT(PF_HALF));
        stop();
        int st = all_pit(0) && !trk[0].pfx && psq_at == 0xFFu && !psq_now;
        OK(on0 && off1 && both && st, "a lit step holds its punch-in on every track (%d), the next step off lets go (%d), rows overlap (%d), STOP lets go (%d)", on0, off1, both, st);
    }
    {   /* RES and LEN: 1/8 steps hold two 1/16s; LEN 4 wraps */
        setup();
        psq.fx[0] = 1; psq.st[0] = 1u << 1; psq.res = 2; psq.len = 4;
        transport_req = 1; blocks(4);
        uint32_t i, lit = 0, maxat = 0;
        for (i = 0; i < 8000u; i++) {                   /* a few bars */
            mix_block(pq_buf, CTL);
            if (psq_at != 0xFFu && psq_at > maxat) maxat = psq_at;
            lit += trk[0].pfx_pit == 192;
        }
        uint32_t s8 = div_samples(2) * 2u / CTL;        /* blocks a 1/8 */
        stop();
        OK(maxat == 3u && lit > s8 && lit < 8000u / 3u, "RES 1/8, LEN 4: the playhead wraps at 4 (%u), a step of the four lit (%u blocks lit, a 1/8 %u)", maxat, lit, s8);
    }
    {   /* a key held live wins while held */
        setup();
        psq.fx[0] = 1; psq.st[0] = 1u << 0;            /* OCT+ on step 1 only */
        transport_req = 1; blocks(4);
        until_step(1); blocks(2);
        int seqoff = all_pit(0);
        perf_held |= PF_BIT(PF_OCTU);                  /* the key held: on at the next 1/16 */
        until_step(2); blocks(2);
        int live = all_pit(192);
        until_step(0); blocks(2);
        int both = all_pit(192);
        until_step(1); blocks(2);
        int still = all_pit(192);                      /* the row off, the key held: on */
        perf_held &= ~PF_BIT(PF_OCTU);
        blocks(2);
        int gone = all_pit(0);
        stop();
        OK(seqoff && live && both && still && gone, "a key held live keeps its effect on whatever the row says (%d %d %d %d), let go: off (%d)", seqoff, live, both, still, gone);
    }
    {   /* DRONE CLAY: a sequenced punch-in on the time restarts the composition as a key's (clay.c clay_pq) */
        setup(); select_engine(ENGI_WAVE); frame();
        for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
        clay_defaults(clay[0]);
        psq.fx[0] = 2; psq.st[0] = 1u << 4;            /* HALF on step 5 */
        transport_req = 1; blocks(4);
        until_step(3); blocks(2);
        uint32_t before = clay_pq[0];
        until_step(4); blocks(2);
        uint32_t during = clay_pq[0];
        until_step(5); blocks(2);
        uint32_t after = clay_pq[0];
        stop();
        OK(clay[0][CL_ON] && !before && (during & (1u << PT_HALF)) && !after,
           "DRONE CLAY: HALF from the rows reaches its composition's time (pt_ops, clay_pq %x), let go %x", during, after);
    }
    {   /* the page: GLO > PUNCH SEQ, its knobs and keys */
        setup(); go_home(); frame();
        int ok = nav_put(SEC_GLO, "PUNCH SEQ") && cur_page()->graph == GR_PUNCH && psq_on() && keys_mode() == 2u;
        turn(EN_K1, 2); frames(2);
        ok &= psq_row == 2u;
        turn(EN_K2, 3); frames(2);
        ok &= psq.fx[2] == 5u;                         /* HALF + 3: S8 */
        turn(EN_K3, -20); frames(2);
        ok &= psq.len == 4u;
        turn(EN_K4, 5); frames(2);
        ok &= psq.res == 3u;
        tap_key(key_at(0, 3));
        ok &= psq.st[2] == 1u << 3;
        tap_key(key_at(0, 3));
        ok &= psq.st[2] == 0u;
        tap_key(key_at(1, 0));                         /* black key 1: row 1 */
        ok &= psq_row == 0u;
        tap_key(key_at(0, 15));
        ok &= psq.st[0] == 1u << 15;
        OK(ok, "GLO > PUNCH SEQ: ROW EFFECT LEN RES, the white keys the row's steps, black 1..4 the row (row %u fx %u len %u res %u)", psq_row, psq.fx[2], psq.len, psq.res);
        go_home(); frame();
    }
    {   /* stored and read back; an older project (no block): empty; CLEAR empties it */
        static project_t q, r; static project_store_t st;
        static const uint8_t D[PSQ_BYTES] = PSQ_DEF_BYTES;
        setup();
        psq.st[0] = 0x8001u; psq.st[3] = 0x0F0Fu; psq.fx[0] = 14; psq.fx[3] = 10; psq.len = 12; psq.res = 0;
        psq_t was = psq;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        psq_clear();
        ok &= !project_restore_runtime(&r) && !memcmp(&psq, &was, sizeof psq);
        OK(ok, "stored and read back (steps %04x %04x, fx %u %u, len %u, res %u)", psq.st[0], psq.st[3], psq.fx[0], psq.fx[3], psq.len, psq.res);
        setup();                                        /* an older project: no 'P' 'Q' (an empty one writes none) */
        project_capture(&q);
        ok = !proj_psq_used(&q) && proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        psq = was;
        ok &= !project_restore_runtime(&r) && !memcmp(&psq, D, sizeof psq);
        OK(ok, "an older project (no block) loads it empty");
        psq = was;
        system_init();
        OK(!memcmp(&psq, D, sizeof psq), "CLEAR (system_init) empties it");
    }
    printf("punchseq: %s\n", pq_bad ? "FAIL" : "all ok");
    return pq_bad != 0;
}
