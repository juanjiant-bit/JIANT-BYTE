/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 9 in its first form) the equations' generator: a formula invented from a seed, from tonal to wild.
 * MATH's WAVE = USER plays a part's own formula: its text (lyr_txt, the part's text the project keeps: VOICE's lyric
 * place, a MATH part has none) or, without one, the formula ALGO's value (the seed, 0 .. 191) and CAOS invent here:
 *   CAOS 0 .. 42    tonal: a floatbeat in tune (sines at the note's whole ratios, a step's envelope, a melody from a
 *                   table of just ratios) or a melodic bytebeat (t times a table's digit)
 *   CAOS 43 .. 85   the classic bytebeats' families (t * (a & t >> b), the or of shifts, a tune of t >> 9 % 13 ..)
 *   CAOS 86 .. 127  a tree of operators at random, the deeper the wilder
 * each judged before it plays (fvm_gen_judge, the formula run over a stretch of time): silent, stuck, all DC or no
 * change refused, the next seed tried (a dozen at most). The same seed and CAOS the same formula, every time: the
 * project keeps the knob, not the text. Run in the main loop (math_poll, ui_input.c), never in the audio: the work
 * space is the project scratch's (one main-loop job at a time); the audio compiles the text when its version moves */
#ifndef FVM_GEN_C
#define FVM_GEN_C

static uint32_t gq_next(uint32_t *s)                 /* xorshift32 */
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static uint32_t gq_n(uint32_t *s, uint32_t n) { return n ? gq_next(s) % n : 0u; }
typedef struct { char *b; uint32_t n, cap; } gq_out_t;
static void gq_s(gq_out_t *o, const char *s)
{
    while (*s && o->n + 1u < o->cap)
        o->b[o->n++] = *s++;
    o->b[o->n] = 0;
}
static void gq_d(gq_out_t *o, uint32_t v)
{
    char t[12];
    uint32_t k = 0;
    do t[k++] = (char)('0' + v % 10u); while ((v /= 10u) && k < sizeof t);
    while (k && o->n + 1u < o->cap)
        o->b[o->n++] = t[--k];
    o->b[o->n] = 0;
}
/* a template: %d a number in [lo, hi] (the next pair of LO / HI), %s a melody table (8 digits), %% before %d a modulo's
 * sign (the % then the number), any other % itself (a modulo by a number written in); the rest as it is */
static void gq_fill(gq_out_t *o, uint32_t *s, const char *tp, const uint8_t *lo, const uint8_t *hi)
{
    uint32_t k = 0;
    for (; *tp; tp++) {
        if (tp[0] == '%' && tp[1] == 'd') {
            gq_d(o, lo[k] + gq_n(s, (uint32_t)hi[k] - lo[k] + 1u));
            k++;
            tp++;
        } else if (tp[0] == '%' && tp[1] == 's') {
            static const char SC[] = "2345689";      /* (digits of a scale's ratios, read as x - 48: whole partials) */
            uint32_t i;
            char b[2] = {0, 0};
            gq_s(o, "\"");
            for (i = 0; i < 8u; i++) {
                b[0] = SC[gq_n(s, sizeof SC - 1u)];
                gq_s(o, b);
            }
            gq_s(o, "\"");
            tp++;
        } else {
            char b[2] = {*tp, 0};
            gq_s(o, b);
        }
    }
}
typedef struct { const char *tp; uint8_t lo[4], hi[4]; } gq_tp_t;
static const gq_tp_t GQ_TONAL[] = {                  /* in tune: whole ratios of the note, a step's envelope */
    {"sin(w*%d)*(1-(t&2047)/2048.)", {1}, {4}},
    {"sin(w+sin(w*%d)*%d/8)", {1, 2}, {5, 24}},
    {"sin(w*(%d+(t>>%d&%d)))", {1, 9, 1}, {2, 12, 7}},
    {"sin(w*%d)*sin(w*(t>>%d&%d|1))", {1, 9, 3}, {3, 12, 7}},
    {"sat(sin(w*%d)*%d)", {1, 2}, {3, 6}},
    {"(tri(w*%d)+sin(w*%d))/2", {1, 2}, {2, 4}},
    {"sin(w*(%s[t>>%d&7]-48)/4)", {11}, {13}},
    {"sin(w*(%s[t>>%d&7]-48)/4)*(1-(t&2047)/2048.)", {11}, {12}},
    {"t*(%s[t>>%d&7]-48)>>%d", {11, 0}, {13, 1}},
    {"sin(w)*(%d-(t>>%d&%d))/%d", {4, 9, 3, 4}, {8, 12, 3, 8}},
};
static const gq_tp_t GQ_CLASSIC[] = {               /* the bytebeats' families */
    {"t*(%d&t>>%d)", {3, 8}, {63, 13}},
    {"t*(t>>%d|t>>%d)", {5, 8}, {9, 13}},
    {"(t*%d&t>>%d)|(t*%d&t>>%d)", {2, 3, 2, 7}, {9, 6, 9, 11}},
    {"t>>%d|t>>%d|t*%d", {2, 5, 1}, {7, 12, 7}},
    {"(t^t>>%d)*(t>>%d&%d)", {2, 9, 3}, {8, 12, 7}},
    {"(t>>%d&t>>%d)*%d", {4, 6, 3}, {7, 9, 31}},
    {"t*((t>>%d|t>>%d)&%d&t>>%d)", {8, 9, 31, 3}, {11, 13, 127, 6}},
    {"t*(t>>%d&%d)^t>>%d", {6, 3, 3}, {11, 15, 6}},
    {"t*(((t>>%d)^(t>>%d)-1^1)%13)", {8, 8}, {10, 10}},
    {"((t>>%d|t%%d)&(t>>%d|t%%d))<<3", {10, 15, 5, 11}, {13, 31, 8, 29}},
};
static const char GQ_OPS[][3] = {"&", "|", "^", "+", "-", "*", ">>", "%"};
static void gq_tree(gq_out_t *o, uint32_t *s, uint32_t depth)
{
    uint32_t r = gq_n(s, 10u);
    if (!depth || r < 3u) {                          /* a leaf: t, t >> k, a number */
        if (r == 0u) { gq_s(o, "t"); return; }
        if (r == 1u) { gq_d(o, 1u + gq_n(s, 255u)); return; }
        gq_s(o, "(t>>");
        gq_d(o, 1u + gq_n(s, 14u));
        gq_s(o, ")");
        return;
    }
    {
        uint32_t op = gq_n(s, sizeof GQ_OPS / sizeof GQ_OPS[0]);
        gq_s(o, "(");
        gq_tree(o, s, depth - 1u);
        gq_s(o, GQ_OPS[op]);
        if (op == 6u) {                              /* (a shift by a small amount) */
            gq_d(o, 1u + gq_n(s, 12u));
        } else {
            gq_tree(o, s, depth - 1u);
        }
        gq_s(o, ")");
    }
}
/* the text of seed `seed`'s try `k` at CAOS c into b (cap bytes, cap <= 64) */
static void fvm_gen_text(char *b, uint32_t cap, uint32_t seed, uint32_t k, uint32_t c)
{
    gq_out_t o = {b, 0, cap};
    uint32_t s = (seed + 1u) * 2654435761u ^ (k + 1u) * 40503u ^ c * 2246822519u;
    b[0] = 0;
    gq_next(&s);
    if (c < 86u) {                                   /* a template, and one time in two a layer on it of its own type
                                                      * (a partial in tune on a floatbeat, an or / xor of t's bits on a
                                                      * bytebeat): more of them, each its own */
        const gq_tp_t *t = c < 43u ? &GQ_TONAL[gq_n(&s, sizeof GQ_TONAL / sizeof GQ_TONAL[0])]
                                   : &GQ_CLASSIC[gq_n(&s, sizeof GQ_CLASSIC / sizeof GQ_CLASSIC[0])];
        uint32_t lay = gq_n(&s, 2u), fx = t->tp[0] != 't' && t->tp[0] != '(';
        if (lay)
            gq_s(&o, "(");
        gq_fill(&o, &s, t->tp, t->lo, t->hi);
        if (lay && fx) {
            static const uint8_t L0[2] = {2, 2}, L1[2] = {6, 5};
            gq_fill(&o, &s, ")+sin(w*%d)/%d", L0, L1);
        } else if (lay) {
            static const uint8_t L0[1] = {3}, L1[1] = {12};
            gq_fill(&o, &s, gq_n(&s, 2u) ? ")|t>>%d" : ")^t>>%d", L0, L1);
        }
    } else {
        gq_s(&o, "t*");
        gq_tree(&o, &s, 2u + (c - 86u) / 21u);       /* (2 .. 3 levels: a wilder one deeper) */
    }
}

/* the judge: the formula over 2048 samples at C4's speed, early and late in its time; 1 = it plays */
typedef struct {
    fvm_cc_t cc;
    fvm_full_t pg;
    fvm_scratch_t sc;
    int32_t st[FVM_MAXSTATE];
} gq_work_t;
static int fvm_gen_judge(gq_work_t *w, const fvm_prog_t *p)
{
    uint32_t pass, i, j, T[FVM_BLK], W[FVM_BLK], o[FVM_BLK], ok = 1;
    for (pass = 0; pass < 2u && ok; pass++) {
        uint32_t t0 = pass ? 1u << 16 : 0u, prev = 0, ch = 0, seen[8] = {0}, distinct = 0;
        int64_t sum = 0, sq = 0;
        memset(w->st, 0, sizeof w->st);
        for (i = 0; i < 2048u; i += FVM_BLK) {
            fvm_env_t en = {{T, W, W, T, W}, {0}, w->st};
            en.u[FU_A] = 5u;
            for (j = 0; j < FVM_BLK; j++) {
                uint32_t x = (t0 + (i + j) * 3u / 16u) << 8 | ((i + j) * 3u % 16u) << 4;   /* (C4: ~0.19 t a sample) */
                T[j] = x >> 8;
                W[j] = x << 3;
            }
            fvm_run(p, &en, &w->sc, FVM_BLK, o);
            for (j = 0; j < FVM_BLK; j++) {
                int32_t v = p->ty == FT_INT ? (int32_t)(o[j] & 255u) - 128 : (int32_t)o[j] >> 9;   /* (FIX: +-128) */
                uint32_t q = (uint32_t)(v + 128) & 255u;
                sum += v;
                sq += (int64_t)v * v;
                ch += (uint32_t)v != prev;
                prev = (uint32_t)v;
                if (!(seen[q >> 5] >> (q & 31u) & 1u)) {
                    seen[q >> 5] |= 1u << (q & 31u);
                    distinct++;
                }
            }
        }
        {
            int64_t mean = sum / 2048, var = sq / 2048 - mean * mean;
            ok = var > 300 && distinct >= 8u && ch > 64u && (mean < 0 ? -mean : mean) < 80;
        }
    }
    return ok;
}
/* a formula for seed, CAOS c into b (64 bytes): the first try the judge lets through (or the last one tried) */
static uint32_t fvm_invent(gq_work_t *w, char *b, uint32_t seed, uint32_t c)
{
    uint32_t k;
    for (k = 0; k < 12u; k++) {
        fvm_err_t e;
        uint32_t n = 0;
        fvm_gen_text(b, 64u, seed, k, c);
        while (b[n]) n++;
        if (!fvm_compile(&w->cc, FVM_FULL(&w->pg), b, n, &e) && fvm_gen_judge(w, &w->pg.p))
            return k;
    }
    return k;
}
#endif
