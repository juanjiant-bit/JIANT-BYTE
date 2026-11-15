/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 1) FVM (src/fvm.c): the language, the machine, the factory formulas (src/fvm_bank.c) against
 * MATH's (eng_lofi.c), and its cost.
 *   the language   numbers, types (INT / FIX), C's precedence, folding, division by zero, tables, calls, errors
 *   BYTE           MATH's 48 bytebeats as text: bit for bit lofi_byte's, every VAR (a 1..16), t over 2^20 steps
 *                  and random 32-bit times, in blocks of FVM_BLK
 *   FLOAT          MATH's 48 floatbeats in FIX: against lofi_float at C4 for 2 s (the same t and phase): the
 *                  correlation, the level (RMS) and the peak
 *   state          ph / lp / hp per voice, across blocks as in one
 *   cost           host instructions (ns) a sample: the FVM's block against MATH's per-sample call, BYTE and FLOAT
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/fvm_test tests/fvm_test.c -lm && build/host/fvm_test */
#include <stdarg.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/fvm.c"
#include "../firmware/src/fvm_bank.c"

static int fails;
static void ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("fvm: %-96s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

static fvm_cc_t cc;
static fvm_scratch_t scr;
static int32_t st[FVM_MAXSTATE];

static int comp(fvm_prog_t *p, const char *s, fvm_err_t *e)
{
    return (int)fvm_compile(&cc, p, s, (uint32_t)strlen(s), e);
}
/* one value of a formula at t (a block of one), the variables given */
static uint32_t eval1(const char *s, uint32_t t, uint32_t a, uint32_t *ty)
{
    static fvm_prog_t p;
    uint32_t tt[1] = {t}, w[1] = {t << 11}, o[1];
    fvm_env_t e = {{tt, w, w, tt, w}, {0}, st};
    fvm_err_t er;
    if (comp(&p, s, &er)) {
        printf("fvm: compile %s: error %u at %u\n", s, er.code, er.pos);
        return 0xDEADBEEFu;
    }
    e.u[FU_A] = a;
    memset(st, 0, sizeof st);
    fvm_run(&p, &e, &scr, 1, o);
    if (ty)
        *ty = p.ty;
    return o[0];
}

static void test_language(void)
{
    uint32_t ty, t;
    fvm_prog_t p;
    fvm_err_t e;
    int good = 1;
    /* precedence and C's unsigned arithmetic, against the compiler's own C */
    for (t = 0; t < 200000u; t += 997u) {
        uint32_t a = 7u;
        good &= eval1("t*(42&t>>10)", t, a, 0) == (t * (42u & t >> 10));
        good &= eval1("t>>4|t>>5&t*3^t+a-2*t%7", t, a, 0) == ((t >> 4) | ((t >> 5) & (t * 3u)) ^ (t + a - 2u * t % 7u));
        good &= eval1("t<4096?t*3:t&t>>8", t, a, 0) == (t < 4096u ? t * 3u : t & t >> 8);
        good &= eval1("-t&4095", t, a, 0) == (-t & 4095u);
        good &= eval1("!(t&1)+~t", t, a, 0) == ((uint32_t)!(t & 1u) + ~t);
        good &= eval1("t>>>3", t, a, 0) == t >> 3;
        good &= eval1("(t&a)==a||t%3==0&&t>99", t, a, 0) == (uint32_t)(((t & a) == a) || (t % 3u == 0u && t > 99u));
    }
    ok(good, "C's precedence and unsigned arithmetic (7 expressions x 201 t)");
    ok(eval1("5/0+7%0", 1, 0, &ty) == 0u && ty == FT_INT, "division and modulo by zero: 0");
    ok(eval1("1.5+1", 0, 0, &ty) == 0x28000u && ty == FT_FIX, "1.5 + 1 = 2.5, FIX (an INT goes up)");
    ok(eval1("0.25*4", 0, 0, &ty) == 0x10000u && ty == FT_FIX, "0.25 * 4 = 1.0 (INT x FIX)");
    ok(eval1("2.75&3", 0, 0, &ty) == 2u && ty == FT_INT, "a bit operator takes the integer part: 2.75 & 3 = 2, INT");
    ok(eval1("-1.5/2", 0, 0, &ty) == (uint32_t)-49152 && ty == FT_FIX, "-1.5 / 2 = -0.75");
    ok(eval1("3.0/4.0", 0, 0, 0) == 49152u, "3.0 / 4.0 = 0.75");
    ok(eval1("sin(0.25)", 0, 0, 0) >= 65530u && eval1("sin(0.75)", 0, 0, 0) <= (uint32_t)-65530, "sin(0.25 turn) = 1, sin(0.75) = -1");
    ok(eval1("sin(64)", 0, 0, 0) == 255u && eval1("sin(192)", 0, 0, 0) == 0u && eval1("sin(0)", 0, 0, 0) == 128u,
       "sin of an INT: a byte, 256 a turn (64 -> 255, 192 -> 0, 0 -> 128)");
    ok(eval1("\"0123\"[t]", 6, 0, 0) == '2' && eval1("\"0123\"[5]", 0, 0, 0) == '1', "a string as a table, modulo its length");
    ok(eval1("clip(3.5)+clip(-0.5)", 0, 0, 0) == 32768u, "clip: 1 + -0.5");
    ok(eval1("min(3,t)+max(2.5,t)", 1, 0, 0) == 0x38000u, "min / max, the types unified");
    ok(eval1("a*2+0x10", 0, 5, 0) == 26u, "a block variable and hex");
    /* folding: a formula of numbers is one push */
    comp(&p, "(3+4)*2<<1", &e);
    ok(p.n == 1u && p.ins[0].op == FO_PUSH && p.ins[0].k == 28u, "numbers folded: (3+4)*2<<1 is one push of 28");
    comp(&p, "t>>10", &e);
    ok(p.n == 2u, "t >> 10: two instructions (the push of t, the shift by its own number)");
    comp(&p, "10&t", &e);
    ok(p.n == 2u, "10 & t: the number moved to the right (commutative): two instructions");
    comp(&p, "100<t*3", &e);
    ok(p.n == 3u && p.ins[2].op == FO_GTU, "100 < t*3 becomes t*3 > 100: three instructions");
    /* errors: their kind and place */
    ok(comp(&p, "t*(3", &e) == FE_SYNTAX, "an open parenthesis: a syntax error");
    ok(comp(&p, "t*q", &e) == FE_NAME && e.pos == 2u, "an unknown name, at its place (2)");
    ok(comp(&p, "sin(t,2)", &e) == FE_ARGS, "a call with the wrong number of arguments");
    ok(comp(&p, "", &e) == FE_EMPTY && comp(&p, "   ", &e) == FE_EMPTY, "an empty formula");
    ok(comp(&p, "t 3", &e) == FE_SYNTAX, "text left over");
    {
        char deep[400];
        int i, k = 0;
        for (i = 0; i < 14; i++) k += sprintf(deep + k, "(t-");
        k += sprintf(deep + k, "t");
        for (i = 0; i < 14; i++) k += sprintf(deep + k, "*t)");
        ok(comp(&p, deep, &e) == FE_DEEP, "too deep for the stack: refused, not run");
    }
    {
        char lng[600];
        int i, k = 0;
        for (i = 0; i < 70; i++) k += sprintf(lng + k, "(t>>%d)+", i % 20);
        k += sprintf(lng + k, "t");
        ok(comp(&p, lng, &e) == FE_LONG || comp(&p, lng, &e) == FE_TREE, "too long: refused");
    }
    ok(comp(&p, "ph(0.1)+ph(0.2)+ph(0.3)+ph(0.4)+ph(0.5)+ph(0.6)+ph(0.7)+ph(0.8)+ph(0.9)", &e) == FE_STATE,
       "more stateful calls than FVM_MAXSTATE: refused");
    /* fuzz: random text never crashes nor runs a refused program */
    {
        static const char AL[] = "t a w T x 0123456789.()+-*/%&|^~!<>=?:\"[],sinclpabsx";
        uint32_t r = 12345u, i, k, nok = 0;
        char buf[64];
        for (i = 0; i < 200000u; i++) {
            uint32_t n = 1u + (r >> 27);
            for (k = 0; k < n; k++) {
                r = r * 1664525u + 1013904223u;
                buf[k] = AL[(r >> 24) % (sizeof AL - 1u)];
            }
            buf[n] = 0;
            if (!fvm_compile(&cc, &p, buf, n, &e)) {
                uint32_t tt[FVM_BLK], o[FVM_BLK], j;
                fvm_env_t en = {{tt, tt, tt, tt, tt}, {0}, st};
                for (j = 0; j < FVM_BLK; j++) tt[j] = r + j;
                fvm_run(&p, &en, &scr, FVM_BLK, o);
                nok++;
            } else if (p.n) {
                break;
            }
        }
        ok(i == 200000u, "200000 random texts: no crash, a refused one leaves an empty program (%u compiled and run)", nok);
    }
}

/* MATH's bytebeats, bit for bit */
static void test_byte(void)
{
    uint32_t f, bad = 0, maxn = 0, maxd = 0;
    static fvm_prog_t p;
    for (f = 0; f < FVM_NBYTE; f++) {
        fvm_err_t e;
        uint32_t a, b, i, nb = 0, worst = 0;
        if (comp(&p, FVM_BYTE_BANK[f], &e)) {
            printf("fvm: B%02u compile error %u at %u: %s\n", f + 1, e.code, e.pos, FVM_BYTE_BANK[f]);
            bad++;
            continue;
        }
        if (p.n > maxn) maxn = p.n;
        if (p.depth > maxd) maxd = p.depth;
        for (a = 1; a <= 16u; a++) {
            uint32_t r = 777u * a + f;
            for (b = 0; b < (1u << 20) / FVM_BLK + 64u; b++) {
                uint32_t tt[FVM_BLK], o[FVM_BLK];
                fvm_env_t en = {{tt, 0, 0, 0, 0}, {0}, st};
                en.u[FU_A] = a;
                for (i = 0; i < FVM_BLK; i++) {
                    if (b < (1u << 20) / FVM_BLK)
                        tt[i] = b * FVM_BLK + i;
                    else
                        r = r * 1664525u + 1013904223u, tt[i] = r;
                }
                fvm_run(&p, &en, &scr, FVM_BLK, o);
                for (i = 0; i < FVM_BLK; i++)
                    if (o[i] != lofi_byte(f, tt[i], a))
                        nb++;
            }
            if (nb > worst) worst = nb;
        }
        if (worst) {
            printf("fvm: B%02u differs (%u samples): %s\n", f + 1, worst, FVM_BYTE_BANK[f]);
            bad++;
        }
    }
    ok(!bad, "BYTE: MATH's 48 bytebeats bit for bit (every a 1..16, t 0..2^20 and 2048 random times): %u differ", bad);
    ok(maxn <= FVM_MAXINS && maxd <= FVM_DEPTH, "BYTE: the longest %u instructions (of %u), the deepest %u (of %u)", maxn,
       FVM_MAXINS, maxd, FVM_DEPTH);
}

/* MATH's floatbeats against the FVM's FIX: C4, 2 s, the same t and phase */
static void test_float(void)
{
    static fvm_prog_t p;
    uint32_t f, inc = pitch_inc(60 << 4), ti = inc >> 3, nsm = 88200u, lowc = 0;
    double sumc = 0, minc = 2;
    printf("fvm: FLOAT  corr   rms(fvm/math)  peak  instructions\n");
    for (f = 0; f < FVM_NFLOAT; f++) {
        fvm_err_t e;
        uint32_t tt = 0, tf = 0, i, j;
        int32_t aux = 0;
        double sxy = 0, sxx = 0, syy = 0, pk = 0, c;
        if (comp(&p, FVM_FLOAT_BANK[f], &e)) {
            printf("fvm: F%02u compile error %u at %u: %s\n", f + 1, e.code, e.pos, FVM_FLOAT_BANK[f]);
            fails++;
            continue;
        }
        memset(st, 0, sizeof st);
        for (i = 0; i < nsm; i += FVM_BLK) {
            uint32_t T[FVM_BLK], W[FVM_BLK], o[FVM_BLK];
            int32_t ref[FVM_BLK];
            fvm_env_t en = {{T, W, 0, 0, 0}, {0}, st};
            for (j = 0; j < FVM_BLK; j++) {
                uint32_t x = tt << 8 | tf >> 16;
                T[j] = tt;
                W[j] = x << 3;
                ref[j] = lofi_float(f, tt, x, &aux) * 2;
                tf += ti;
                tt += tf >> 24;
                tf &= 0xFFFFFFu;
            }
            fvm_run(&p, &en, &scr, FVM_BLK, o);
            for (j = 0; j < FVM_BLK; j++) {
                double y = (double)(int32_t)o[j], x = ref[j];
                sxy += x * y; sxx += x * x; syy += y * y;
                if (fabs(y) > pk) pk = fabs(y);
            }
        }
        c = sxx > 0 && syy > 0 ? sxy / sqrt(sxx * syy) : (sxx == syy ? 1 : 0);
        printf("fvm: F%02u    %.3f  %.3f          %.2f  %u\n", f + 1, c, sxx > 0 ? sqrt(syy / sxx) : 0, pk / 65536, p.n);
        sumc += c;
        if (c < minc) minc = c;
        if (c < 0.98) lowc++;
    }
    ok(!lowc, "FLOAT: MATH's 48 floatbeats in FIX: mean correlation %.3f, lowest %.3f, %u under 0.98",
       sumc / FVM_NFLOAT, minc, lowc);
}

/* ph lp hp: their state is the voice's, carried across blocks */
static void test_state(void)
{
    static fvm_prog_t p;
    uint32_t o1[FVM_BLK * 4], o2[FVM_BLK], tt[FVM_BLK * 4], i, b, same = 1;
    int32_t s1[FVM_MAXSTATE] = {0}, s2[FVM_MAXSTATE] = {0};
    fvm_err_t e;
    comp(&p, "lp(sin(ph(0.01))+rnd(t)*0.1,0.05)+hp(sin(w),0.2)", &e);
    for (i = 0; i < FVM_BLK * 4; i++) tt[i] = i * 977u;
    for (b = 0; b < 4u; b++) {
        fvm_env_t en = {{tt + b * FVM_BLK, tt + b * FVM_BLK, 0, 0, 0}, {0}, s1};
        fvm_run(&p, &en, &scr, FVM_BLK, o1 + b * FVM_BLK);
    }
    for (b = 0; b < 4u; b++) {                       /* again, in blocks of 8 */
        uint32_t k;
        for (k = 0; k < FVM_BLK; k += 8u) {
            fvm_env_t en = {{tt + b * FVM_BLK + k, tt + b * FVM_BLK + k, 0, 0, 0}, {0}, s2};
            fvm_run(&p, &en, &scr, 8u, o2 + k);
        }
        for (i = 0; i < FVM_BLK; i++) same &= o2[i] == o1[b * FVM_BLK + i];
    }
    ok(p.nstate == 3u && same, "ph / lp / hp: three states, the same output in blocks of 32 or of 8");
}

/* the formula's clock: KEY, RATE, START, LOOP, WARP, SYNC, T */
static void clk_run(fvm_clk_par_t *p, fvm_clk_ctx_t *x, uint32_t ns, uint32_t *T, uint32_t *W, uint32_t *S)
{
    fvm_clk_t c;
    uint32_t i;
    fvm_clock_reset(&c);
    for (i = 0; i < ns; i += FVM_BLK) {
        fvm_clock(&c, p, x, FVM_BLK, T + i, W ? W + i : 0, S ? S + i : 0);
        x->song_t += (x->song_inc * FVM_BLK) >> 16;
    }
}
static void test_clock(void)
{
    enum { NS = 44100u / FVM_BLK * FVM_BLK };
    static uint32_t T1[NS], W1[NS], T2[NS], W2[NS], S1[NS];
    uint32_t inc = pitch_inc(60 << 4), inc2 = pitch_inc(67 << 4), i, good;
    fvm_clk_par_t p = {0};
    fvm_clk_ctx_t x = {inc >> 3, pitch_inc(60 << 4) >> 3, 0, 0};
    /* LIBRE = MATH's FLOAT time: t and x from its accumulators, VAR 37 and BEND 6 and LOOP 14 */
    p.key = FK_LIBRE; p.mut = 37; p.fold = 6; p.loop = 14;
    clk_run(&p, &x, NS, T1, W1, 0);
    {
        uint32_t tt = 0, tf = 0, ti = inc >> 3;
        good = 1;
        for (i = 0; i < NS; i++) {
            uint32_t tl = lofi_mut(tt & 16383u, 37u), tw = tl ^ (tl >> 6);
            good &= T1[i] == tw && W1[i] == ((tw << 8 | tf >> 16) << 3);
            tf += ti; tt += tf >> 24; tf &= 0xFFFFFFu;
        }
    }
    ok(good, "clock: KEY LIBRE is MATH's time bit for bit (t, and w = its x << 3), with WARP (VAR 37, BEND 6) and LOOP 14");
    /* ALTURA: the bits above SPLIT the same for C4 and G4 (the rhythm fixed), the ones below the note's */
    p = (fvm_clk_par_t){0};
    p.key = FK_ALTURA; p.split = 9;
    x.note_inc = inc >> 3;
    clk_run(&p, &x, NS, T1, W1, 0);
    x.note_inc = inc2 >> 3;
    clk_run(&p, &x, NS, T2, W2, 0);
    {
        uint32_t hi = 1, lo = 0;
        for (i = 0; i < NS; i++) {
            hi &= (T1[i] >> 9) == (T2[i] >> 9);
            lo += (T1[i] & 511u) != (T2[i] & 511u);
        }
        ok(hi && lo > NS / 2u, "clock: KEY ALTURA: C4 and G4 the same rhythm above bit 9, their own timbre below (%u of %u differ)",
           lo, NS);
    }
    {   /* .. and the pitch: w turns at the note's rate (G4 ~1.5 x C4's) */
        uint32_t k1 = 0, k2 = 0;
        for (i = 1; i < NS; i++) {
            k1 += (W1[i] & 0x8000u) && !(W1[i - 1] & 0x8000u);
            k2 += (W2[i] & 0x8000u) && !(W2[i - 1] & 0x8000u);
        }
        ok(k2 * 100u > k1 * 145u && k2 * 100u < k1 * 155u, "clock: KEY ALTURA: the pitch the note's (%u turns at G4, %u at C4)", k2, k1);
    }
    p.key = FK_FIJO;
    x.note_inc = inc >> 3;
    clk_run(&p, &x, NS, T1, 0, 0);
    x.note_inc = inc2 >> 3;
    clk_run(&p, &x, NS, T2, 0, 0);
    good = !memcmp(T1, T2, sizeof T1);
    ok(good, "clock: KEY FIJO: t the same whatever the key");
    /* RATE x2: twice the steps; START; LOOP */
    p = (fvm_clk_par_t){0};
    x.note_inc = inc >> 3;
    clk_run(&p, &x, NS, T1, 0, 0);
    p.rate = 131072u;
    clk_run(&p, &x, NS, T2, 0, 0);
    ok(T2[NS - 1] >= 2u * T1[NS - 1] - 2u && T2[NS - 1] <= 2u * T1[NS - 1] + 2u, "clock: RATE x2: %u steps in a second (x1: %u)",
       T2[NS - 1], T1[NS - 1]);
    p.rate = 0; p.start = 1000u; p.loop = 8;
    clk_run(&p, &x, NS, T1, 0, 0);
    good = 1;
    for (i = 0; i < NS; i++) good &= T1[i] >= 1000u && T1[i] < 1256u;
    ok(good, "clock: START 1000 and LOOP 8: t in 1000 .. 1255");
    /* SYNC BAR: the tempo's t is the song's, whatever the note */
    p = (fvm_clk_par_t){0};
    p.key = FK_FIJO; p.sync = FY_BAR;
    x.song_t = 5000u; x.song_inc = 13107u;           /* (0.2 t a sample) */
    clk_run(&p, &x, NS, T1, 0, S1);
    good = T1[0] == 5000u && T1[FVM_BLK] == 5000u + ((13107u * FVM_BLK) >> 16);
    ok(good, "clock: SYNC BAR: t follows the song's position (%u at the start, %u a block later)", T1[0], T1[FVM_BLK]);
    ok(S1[NS - 1] > 65400u && S1[NS - 1] < 65600u, "clock: T a second after %u samples: %.4f", NS, S1[NS - 1] / 65536.0);
}

/* the cost: the FVM's block against MATH's call a sample */
static double fvm_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}
static volatile uint32_t sink;
static void test_cost(void)
{
    static fvm_prog_t pb[FVM_NBYTE], pf[FVM_NFLOAT];
    uint32_t f, i, b, nb = 4096u, tt[FVM_BLK], o[FVM_BLK];
    double t0, fb, nbm, ff, nfm;
    int32_t aux = 0;
    fvm_err_t e;
    for (f = 0; f < FVM_NBYTE; f++) comp(&pb[f], FVM_BYTE_BANK[f], &e);
    for (f = 0; f < FVM_NFLOAT; f++) comp(&pf[f], FVM_FLOAT_BANK[f], &e);
    for (i = 0; i < FVM_BLK; i++) tt[i] = 100000u + i;
    t0 = fvm_now_ns();
    for (f = 0; f < FVM_NBYTE; f++)
        for (b = 0; b < nb; b++) {
            fvm_env_t en = {{tt, tt, tt, tt, tt}, {0}, st};
            en.u[FU_A] = 5u;
            fvm_run(&pb[f], &en, &scr, FVM_BLK, o);
            sink += o[3];
        }
    fb = (fvm_now_ns() - t0) / (FVM_NBYTE * nb * (double)FVM_BLK);
    t0 = fvm_now_ns();
    for (f = 0; f < FVM_NBYTE; f++)
        for (b = 0; b < nb; b++)
            for (i = 0; i < FVM_BLK; i++)
                sink += lofi_byte(f, tt[i] + b, 5u);
    nbm = (fvm_now_ns() - t0) / (FVM_NBYTE * nb * (double)FVM_BLK);
    t0 = fvm_now_ns();
    for (f = 0; f < FVM_NFLOAT; f++)
        for (b = 0; b < nb; b++) {
            fvm_env_t en = {{tt, tt, tt, tt, tt}, {0}, st};
            fvm_run(&pf[f], &en, &scr, FVM_BLK, o);
            sink += o[3];
        }
    ff = (fvm_now_ns() - t0) / (FVM_NFLOAT * nb * (double)FVM_BLK);
    t0 = fvm_now_ns();
    for (f = 0; f < FVM_NFLOAT; f++)
        for (b = 0; b < nb; b++)
            for (i = 0; i < FVM_BLK; i++)
                sink += (uint32_t)lofi_float(f, tt[i] + b, (tt[i] + b) << 8, &aux);
    nfm = (fvm_now_ns() - t0) / (FVM_NFLOAT * nb * (double)FVM_BLK);
    printf("fvm: cost a sample (host, -O2): BYTE FVM %.2f ns, MATH %.2f ns (x%.2f); FLOAT FVM %.2f ns, MATH %.2f ns (x%.2f)\n",
           fb, nbm, fb / nbm, ff, nfm, ff / nfm);
    ok(fb < 4.0 * nbm + 2.0 && ff < 4.0 * nfm + 2.0, "cost: the FVM within 4x MATH's hand-written C (+2 ns)");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (getenv("FVM_ONLY_COST")) { test_cost(); return 0; }
    test_language();
    test_byte();
    test_float();
    test_state();
    test_clock();
    if (!getenv("FVM_NO_COST"))
        test_cost();
    printf(fails ? "fvm: %d FAILED\n" : "fvm: all ok\n", fails);
    return fails != 0;
}
