/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 6) the macros as equations (mod.c macro_f, MAC_FORMS, mac_eval; fx.c mac_tick):
 *   every equation compiles in 20 instructions
 *   the knob     macro_f 0: the macro the matrix reads is the knob's, as ever
 *   TRI          over a bar from its low to its high; STEP four steps a bar; S&H a new value each 1/8
 *   the knob     turned left past 0 the equations (up to the last), right back to the knob, then its value
 *   the matrix   a part's MOD slot on M1 follows the equation, block by block through the mix (mac_tick)
 *   stored       the project's 'M' 'F' block, there and back
 *   cc -O1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/macro_eq_test tests/macro_eq_test.c -lm */
#include <stdarg.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int mq_fails;
static void mq_ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("maceq: %-98s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        mq_fails++;
}
static uint32_t at(uint32_t k, uint32_t f, uint32_t beat_q16)
{
    macro_f[k] = (uint8_t)f;
    mac_eval(beat_q16);
    return macro_e[k];
}

int main(int argc, char **argv)
{
    static fvm_cc_t cc;
    static fvm_prog_t pg;
    static fvm_ins_t pi[20];
    uint32_t f, bad = 0, i;
    (void)argc;
    (void)argv;
    fvm_prog_init(&pg, pi, 20u);
    for (f = 0; f < MAC_NF; f++) {
        fvm_err_t er;
        if (fvm_compile(&cc, &pg, MAC_FORMS[f], (uint32_t)strlen(MAC_FORMS[f]), &er)) {
            printf("maceq: %s: error %u at %u\n", N_MACF[f], er.code, er.pos);
            bad++;
        }
    }
    mq_ok(!bad, "the %u equations compile in 20 instructions", (unsigned)MAC_NF);
    memset(macro_f, 0, sizeof macro_f);
    macro_v[0] = 33; macro_v[1] = 0; macro_v[2] = 127; macro_v[3] = 64;
    mac_eval(12345u);
    mq_ok(!memcmp(macro_e, macro_v, 4), "the knob: the matrix's macros are the knobs' (%u %u %u %u)", macro_e[0], macro_e[1],
          macro_e[2], macro_e[3]);
    {
        uint32_t lo = 255, hi = 0, v;
        for (i = 0; i < 64u; i++) {
            v = at(0, 1, i * 4u * 65536u / 64u);     /* (TRI over a bar: four beats) */
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        mq_ok(lo < 5u && hi > 122u, "TRI: over a bar from %u to %u", lo, hi);
    }
    {
        uint32_t s[4], k, distinct = 0;
        for (i = 0; i < 4u; i++) s[i] = at(1, 7, i * 65536u + 100u);
        for (i = 0; i < 4u; i++) {
            for (k = 0; k < i && s[k] != s[i]; k++) {}
            distinct += k == i;
        }
        mq_ok(distinct == 4u, "STEP: four steps a bar (%u %u %u %u)", s[0], s[1], s[2], s[3]);
    }
    {
        uint32_t same = 1, a = at(2, 8, 100u), b = at(2, 8, 30000u), c = at(2, 8, 33000u);
        same = a == b;                               /* (the same 1/8) */
        mq_ok(same && c != b, "S&H: one value within a 1/8 (%u %u), another the next (%u)", a, b, c);
    }
    {
        uint32_t ok1, ok2, ok3;
        memset(macro_f, 0, sizeof macro_f);
        macro_v[3] = 0;
        macro_turn(3, -1, -1);
        ok1 = macro_f[3] == 1u && macro_v[3] == 0u;
        for (i = 0; i < 40u; i++) macro_turn(3, -1, -1);
        ok2 = macro_f[3] == MAC_NF;
        for (i = 0; i < 40u; i++) macro_turn(3, 1, 1);
        ok3 = macro_f[3] == 0u && macro_v[3] > 0u;
        mq_ok(ok1 && ok2 && ok3, "the knob: left past 0 the equations (1 .. %u), right back to the knob and its value (%u)",
              (unsigned)MAC_NF, macro_v[3]);
    }
    {   /* the matrix: M1 as a part's source, block by block through the mix */
        static int32_t o[2u * CTL];
        int32_t lo = 1 << 30, hi = -(1 << 30);
        memset(macro_f, 0, sizeof macro_f);
        macro_v[0] = 0;
        macro_f[0] = 1u;                             /* (TRI) */
        song.g[G_BPM] = 120;
        for (i = 0; i < 2u * FS / CTL; i++) {        /* (two seconds stopped: its own clock, a bar) */
            int32_t v;
            mix_block(o, CTL);
            v = mod_tsrc(&trk[0], MS_M1, 0);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        mq_ok(hi - lo > 25000, "the matrix: M1 on TRI moves through the mix, stopped (its source %d .. %d)", lo, hi);
    }
    {
        static project_t q, r;
        static project_store_t st;
        memset(macro_f, 0, sizeof macro_f);
        macro_f[0] = 3; macro_f[2] = MAC_NF;
        project_capture(&q);
        proj_pack(&st, &q);
        memset(&r, 0, sizeof r);
        mq_ok(proj_import(&r, st.raw, (int)sizeof st.raw) && r.mac_form[0] == 3u && r.mac_form[1] == 0u &&
              r.mac_form[2] == MAC_NF, "stored: the macros' equations there and back ('M' 'F')");
    }
    printf(mq_fails ? "maceq: %d FAILED\n" : "maceq: all ok\n", mq_fails);
    return mq_fails != 0;
}
