/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (c1fault) The owner's FM-1 (FUNC BANK beta): ABOUT "CORES 1 (FAULT)", C1 PC at fvm_level's first instruction. BANK's
 * B was compiled in the audio (eng_lofi.c func_act) on core 1, whose render stack is 6 KiB: the formula parser recurses
 * ten fvm_level frames, fvm_unary, fvm_primary, fvm_cond and fvm_expr for each level of parentheses (752 B a level on
 * the pi32v2, its -Os frames from build/felucca.dis), on top of the render it was called from. Here:
 *   1  every formula the audio plays (BANK's 45, BYTE's and FLOAT's 48, WAVE's 16, the DIST EQs, the macros' and the
 *      ARP's equations) and every FUNC recipe (its FORMs, its layer B) compiled with a probe of the parser's depth and
 *      the host's stack: the device's bound (FVM_DEV_*: the frames) for the compile from where the audio did it, under
 *      core 1's stack budget (a compile alone; BANK's, BYTE's, WAVE's, the EQs' also on top of the render the audio
 *      compiled them from: FUNC's recipes and FLOAT's only ever in the main loop), each group's deepest reported;
 *   2  no compile reachable from the audio (fvm.c fvm_audio_compiles: mix_block's and track_render's scope): FUNC BANK
 *      (every FORM A, FORM B, MORPH), every FUNC recipe with its layer B, MATH BYTE / FLOAT (every ALGO), WAVE (A, B),
 *      the DIST EQs, the macros, the ARP's EQs; each program published by the main loop (fvm_serve, math_poll) and
 *      heard; and none compiled again while nothing changes (no compile a block) */
#include <stdint.h>
static uint32_t pr_depth, pr_gen, pr_stk;
static uintptr_t pr_lo;
#define FVM_STACK_PROBE() do { char pr_c; if ((uintptr_t)&pr_c < pr_lo) pr_lo = (uintptr_t)&pr_c; \
    if (c->parse_depth > pr_depth) pr_depth = c->parse_depth; if (c->gen_depth > pr_gen) pr_gen = c->gen_depth; } while (0)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* the device's frames (pi32v2 -Os, build/felucca.dis: [--sp] = {rets, r..} and sp += -n of each prologue) */
#define FVM_DEV_LEVEL (12u + 76u + 20u + 44u + 10u * 60u)   /* fvm_unary fvm_primary fvm_expr fvm_cond, fvm_level x 10 */
#define FVM_DEV_COMPILE 92u                              /* fvm_compile (fvm_func_parse in it) */
#define FVM_DEV_LEAF 160u                                /* past the last level: fvm_binop fvm_bin fvm_op2 fvm_eu .. */
#define FVM_DEV_GEN 60u                                  /* fvm_gen a node (after the parse: not on top of it) */
#define FVM_DEV_C1 (912u + 380u + 292u + 128u + 48u)     /* where the audio compiled: mc_job voice_block voice_half lofi_render func_act */
#define FVM_C1_BUDGET (7168u - 256u)                     /* core 1's render stack (app.ld: 7 KiB, c1fault; was 6) less the guard band */

static int cbad;
static void ccheck(const char *what, int ok)
{
    printf("c1stack: %-110s %s\n", what, ok ? "ok" : "FAIL");
    cbad += !ok;
}
static uint32_t dev_bound(uint32_t pd, uint32_t gd)
{
    uint32_t parse = FVM_DEV_COMPILE + pd * FVM_DEV_LEVEL + FVM_DEV_LEAF, gen = FVM_DEV_COMPILE + gd * FVM_DEV_GEN + FVM_DEV_LEAF;
    return parse > gen ? parse : gen;
}
/* text s compiled at the top of a fresh stack probe: the parse depth (pd), the host's bytes; the device's bound */
static uint32_t probe(const char *s, uint32_t *pd, uint32_t *host, int *err)
{
    static fvm_full_t pg;
    static fvm_cc_t cc;
    char top;
    uint32_t n = 0;
    while (s[n]) n++;
    pr_depth = pr_gen = 0;
    pr_lo = (uintptr_t)&top;
    *err = (int)fvm_compile(&cc, FVM_FULL(&pg), s, n, 0);
    *pd = pr_depth;
    pr_stk = pg.p.depth;
    *host = (uint32_t)((uintptr_t)&top - pr_lo);
    return dev_bound(pr_depth, pr_gen);
}
static const char *t_bank(uint32_t i);
static void group(const char *name, uint32_t n, const char *(*text)(uint32_t), int budget)
{
    uint32_t i, worst = 0, wpd = 0, whost = 0, wi = 0, errs = 0, stk = 0;
    char line[300];
    for (i = 0; i < n; i++) {
        uint32_t pd, host, d;
        int e;
        d = probe(text(i), &pd, &host, &e);
        errs += e != 0;
        if (pr_stk > stk) stk = pr_stk;
        if (d > worst) { worst = d; wpd = pd; whost = host; wi = i; }
    }
    snprintf(line, sizeof line, "%s: %u compile; the deepest (#%u, %u levels) %u B on the device (+%u B under core 1's render: "
             "%u of %u), %u B on the host", name, n - errs, wi, wpd, worst, FVM_DEV_C1, worst + FVM_DEV_C1, FVM_C1_BUDGET, whost);
    ccheck(line, !errs && worst < FVM_C1_BUDGET && (!budget || worst + FVM_DEV_C1 < FVM_C1_BUDGET));
    if (text == t_bank) {                                /* (funcplay.c func_bank_run: its variables above FB_ROW) */
        snprintf(line, sizeof line, "BANK's formulas: the machine's stack %u blocks at most, under func_bank_run's FB_ROW (%u)", stk, FB_ROW);
        ccheck(line, stk <= FB_ROW);
    }
}
static char tx[256];
static const char *t_bank(uint32_t i) { fvm_bank_to(2u * FVM_NBYTE + i, tx); return tx; }
static const char *t_byte(uint32_t i) { fvm_bank_to(i, tx); return tx; }
static const char *t_float(uint32_t i) { fvm_bank_to(FVM_NBYTE + i, tx); return tx; }
static const char *t_wave(uint32_t i) { return WAVE_BANK[i]; }
static const char *t_deq(uint32_t i) { return DIST_EQ[i]; }
static const char *t_mac(uint32_t i) { return MAC_FORMS[i]; }
static const char *t_arp(uint32_t i) { return ARP_EQ[i]; }
static const char *t_func(uint32_t i) { return func_form_text((int32_t)i + 1); }

static int32_t vo[2 * CTL];
static double rms_acc;
static void blocks(uint32_t n)                           /* n blocks, the main loop's between them (as the FM-1's) */
{
    uint32_t b, q;
    for (b = 0; b < n; b++) {
        mix_block(vo, CTL);
        for (q = 0; q < 2u * CTL; q++) rms_acc += (double)vo[q] * vo[q];
        math_poll(); dw_poll();
    }
}
static void quiet(void)
{
    uint32_t k;
    ui_power_on();
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
    for (i = 0; i < 512u && (i < 32u || t->xf_on || t->eng_req != t->engine); i++) blocks(1);
}
static void hold2(uint32_t k)
{
    trk_note_on(&trk[k], 60, 100);
    trk_note_on(&trk[k], 64, 100);
}
/* a scenario: the audio's compiles (none), the main loop's, the level heard */
static void scen(const char *what, uint32_t a0, uint32_t c0, double r0)
{
    char line[240];
    snprintf(line, sizeof line, "%s: no compile in the audio (%u), %u by the main loop, heard (rms %.0f)", what,
             fvm_audio_compiles - a0, fvm_ncompile - c0, sqrt(r0));
    ccheck(line, fvm_audio_compiles == a0 && r0 > 0.0);
}
int main(void)
{
    uint32_t k, i, a0, c0, na = 0;
    char line[240];
    group("BANK's formulas", FB_N, t_bank, 1);
    group("FUNC's recipes", FUNC_BANK - 1u, t_func, 0);     /* (the main loop's only, math_poll: not under a render) */
    group("BYTE's formulas", FVM_NBYTE, t_byte, 1);
    group("FLOAT's formulas", FVM_NFLOAT, t_float, 0);   /* (the deepest, 8 levels: under a render, past even 7 KiB) */
    group("WAVE's formulas", WAVE_NF, t_wave, 1);
    group("the DIST EQs", DIST_NEQ, t_deq, 1);
    group("the macros' equations", MAC_NF, t_mac, 1);
    group("the ARP's equations", ARP_NEQ, t_arp, 1);

    mc_host = 1;
    /* FUNC BANK: every FORM A, FORM B, MORPH; two BANK parts */
    quiet();
    a0 = fvm_audio_compiles; c0 = fvm_ncompile; rms_acc = 0;
    part(0, ENGI_FUNC, FUNC_BANK - 1u); part(1, ENGI_FUNC, FUNC_BANK - 1u);
    hold2(0); hold2(1);
    for (k = 0; k < FB_N; k++) {
        trk[0].p[P_E2] = (int16_t)k; trk[0].p[P_E3] = (int16_t)((k * 7u + 3u) % FB_N); trk[0].p[P_E4] = (int16_t)(k * 3u % 128u);
        trk[1].p[P_E3] = (int16_t)k; trk[1].p[P_E4] = 64;
        blocks(6);
        if (wave_id_b[0] != (0x8000u | (uint32_t)trk[0].p[P_E3]) || wave_id_b[1] != (0x8000u | k)) break;
    }
    ccheck("BANK's B published by the main loop for every FORM B (fvm_serve), the part's voices faded into it", k == FB_N);
    scen("FUNC BANK, every FORM A / FORM B / MORPH, two parts", a0, c0, rms_acc);
    c0 = fvm_ncompile;
    blocks(400);
    snprintf(line, sizeof line, "FUNC BANK held, nothing changed: %u compiles in 400 blocks (none a block)", fvm_ncompile - c0);
    ccheck(line, fvm_ncompile == c0);
    /* every FUNC recipe, its layer B every FORM; MOTION */
    quiet();
    a0 = fvm_audio_compiles; c0 = fvm_ncompile; rms_acc = 0;
    for (k = 0; k + 1u < FUNC_BANK; k++) {
        part(k % 3u, ENGI_FUNC, k);
        trk[k % 3u].p[P_FFB] = (int16_t)((k + 4u) % FUNC_BANK);
        hold2(k % 3u);
        blocks(24);
    }
    scen("every FUNC recipe, its layer B", a0, c0, rms_acc);
    c0 = fvm_ncompile;
    blocks(400);
    snprintf(line, sizeof line, "FUNC recipes held: %u compiles in 400 blocks", fvm_ncompile - c0);
    ccheck(line, fvm_ncompile == c0);
    /* MATH: BYTE, FLOAT, each ALGO; its presets */
    quiet();
    a0 = fvm_audio_compiles; c0 = fvm_ncompile; rms_acc = 0;
    part(0, ENGI_LOFI, 0); part(1, ENGI_LOFI, 3);
    trk[0].p[P_E1] = RW_BYTE; trk[1].p[P_E1] = RW_FLOAT;
    hold2(0); hold2(1);
    for (i = 0; i < 48u; i++) {
        trk[0].p[P_E2] = trk[1].p[P_E2] = (int16_t)(i * 4u);
        blocks(4);
    }
    ccheck("MATH BYTE / FLOAT: every ALGO's program published (lofi_prog asks, fvm_serve)",
           lofi_prg_id[0] == 1u + 47u && lofi_prg_id[1] == 1u + 64u + 47u);
    for (i = 0; i < NELEM(LOFI_PRESETS) && i < 24u; i++) { part(2, ENGI_LOFI, i); hold2(2); blocks(8); }
    scen("MATH BYTE / FLOAT, every ALGO; the MATH presets", a0, c0, rms_acc);
    c0 = fvm_ncompile;
    blocks(400);
    snprintf(line, sizeof line, "MATH parts held: %u compiles in 400 blocks", fvm_ncompile - c0);
    ccheck(line, fvm_ncompile == c0);
    /* WAVE A, B; the DIST EQs; the macros; the ARP's EQs */
    quiet();
    a0 = fvm_audio_compiles; c0 = fvm_ncompile; rms_acc = 0;
    part(0, ENGI_WAVE, 4); hold2(0);
    trk[0].p[P_E5] = 64;
    for (i = 0; i < WAVE_NF; i++) { trk[0].p[P_E0] = (int16_t)i; trk[0].p[P_E4] = (int16_t)(WAVE_NF - 1u - i); blocks(8); }
    for (i = 0; i < DIST_NEQ; i++) { trk[0].p[P_DTYPE] = (int16_t)(DT_EQ0 + i); trk[0].p[P_DIST] = 60; blocks(6); }
    ccheck("the DIST EQs published: the last one's program", deq_id[0] == DT_EQ0 + DIST_NEQ - 1u && deq_prg[0].n > 0u);
    for (i = 0; i < MAC_NF; i++) { macro_f[i & 3u] = (uint8_t)(i + 1u); blocks(4); }
    ccheck("the macros' equations published", mac_id[(MAC_NF - 1u) & 3u] == MAC_NF);
    part(1, ENGI_WAVE, 2);
    for (i = 0; i < ARP_NEQ; i++) {
        trk[1].p[P_AMODE] = (int16_t)(AM_EQ0 + i);
        trk[1].held[0] = 60; trk[1].held[1] = 64; trk[1].held[2] = 67; trk[1].nheld = 3;   /* (keys held: the ARP) */
        for (k = 0; k < 2000u && arp_id != AM_EQ0 + i; k++) blocks(1);
        na += arp_id == AM_EQ0 + i;
    }
    snprintf(line, sizeof line, "the ARP's equations: each one's program published (%u of %u)", na, (uint32_t)ARP_NEQ);
    ccheck(line, na == ARP_NEQ);
    scen("WAVE A and B, the DIST EQs, the macros, the ARP's EQs", a0, c0, rms_acc);
    printf("c1stack: %s\n", cbad ? "FAILED" : "all c1stack checks passed");
    return cbad != 0;
}
