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
/* (phase 9) a formula moved: VAR on a USER part (engines.c math_poll_ws). v 1 .. 63 mutates (numbers nudged, the more
 * the further; a shift's amount by one; & | ^ swapped; a melody table's digits), v 64 .. 127 crosses (a bracket of the
 * formula, or all of it, for a bracket of a factory formula of its kind: fvm_bank.c). Each try judged; the first that
 * plays into b (64 bytes), else b as it was. Deterministic: the text, v and the seed the same, the same result */
#include "fvm_bank.c"
static int gq_dig(char c) { return c >= '0' && c <= '9'; }
static int gq_word(char c) { return gq_dig(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.'; }
/* a mutation of a into o (cap 96): 0 when nothing to change */
static uint32_t gq_mutate(const char *a, char *o, uint32_t *s, uint32_t v)
{
    uint32_t i = 0, n = 0, cand = 0, idx = 0, force, q = 0, did = 0;
    for (i = 0; a[i]; i++) {                         /* (count what can change: numbers, operators, table digits) */
        if (a[i] == '"') q ^= 1u;
        if (q && gq_dig(a[i])) cand++;
        else if (!q && gq_dig(a[i]) && (!i || !gq_word(a[i - 1]))) {
            uint32_t j = i;
            while (gq_dig(a[j])) j++;
            cand += !gq_word(a[j]);
        } else if (!q && (a[i] == '&' || a[i] == '|' || a[i] == '^'))
            cand++;
    }
    if (!cand) return 0;
    force = gq_n(s, cand);
    q = 0;
    for (i = 0; a[i] && n < 92u;) {
        char c = a[i];
        int hit;
        if (c == '"') q ^= 1u;
        if (q && gq_dig(c)) {
            hit = idx++ == force || gq_n(s, 80u) < v;
            o[n++] = hit ? "2345689"[gq_n(s, 7u)] : c;
            did += hit;
            i++;
        } else if (!q && gq_dig(c) && (!i || !gq_word(a[i - 1]))) {
            uint32_t j = i, x = 0;
            while (gq_dig(a[j])) x = x * 10u + (uint32_t)(a[j++] - '0');
            if (gq_word(a[j])) {                     /* (2048. 0x.. 1u: as written) */
                while (i < j && n < 92u) o[n++] = a[i++];
                continue;
            }
            hit = idx++ == force || gq_n(s, 80u) < v;
            if (hit) {
                int sh = i >= 2u && ((a[i - 1] == '>' && a[i - 2] == '>') || (a[i - 1] == '<' && a[i - 2] == '<'));
                if (sh || (i >= 3u && a[i - 1] == ' ' && a[i - 2] == '>' && a[i - 3] == '>')) {
                    x = gq_n(s, 2u) ? x + 1u : x - 1u;
                    if (x < 1u) x = 1u;
                    if (x > 20u) x = 20u;
                } else {
                    uint32_t d = 1u + x * v / 128u;
                    x = gq_n(s, 2u) || x <= d ? x + 1u + gq_n(s, d) : x - 1u - gq_n(s, d);
                    if (x > 9999u) x = 9999u;
                }
                {
                    gq_out_t g = {o, n, 96u};
                    gq_d(&g, x);
                    n = g.n;
                }
                did++;
            } else
                while (i < j && n < 92u) o[n++] = a[i], i++;
            i = j;
        } else if (!q && (c == '&' || c == '|' || c == '^')) {
            hit = idx++ == force || gq_n(s, 80u) < v;
            o[n++] = c;
            if (hit) {
                static const char R[3][2] = {{'|', '^'}, {'&', '^'}, {'&', '|'}};
                o[n - 1] = R[c == '&' ? 0 : c == '|' ? 1 : 2][gq_n(s, 2u)];
                did++;
            }
            i++;
        } else
            o[n++] = c, i++;
    }
    o[n] = 0;
    return did && !a[i];
}
/* the bracket that opens at i in a: its length (with both brackets), 0 when none closes it */
static uint32_t gq_span(const char *a, uint32_t i)
{
    uint32_t d = 0, j = i;
    for (; a[j]; j++) {
        if (a[j] == '(') d++;
        else if (a[j] == ')' && !--d) return j - i + 1u;
    }
    return 0;
}
/* a cross of a into o (cap 96) with the factory formula f of its kind (flt): a bracket of a (or a whole) for one of f's */
static uint32_t gq_cross(const char *a, char *o, uint32_t *s, uint32_t flt, uint32_t f)
{
    const char *d = fvm_bank(flt, f % FVM_NBYTE);
    uint32_t na = 0, nr = 0, i, ri, rn, di, dn, k = 0, n = 0;
    while (a[na]) na++;
    for (i = 0; a[i]; i++) nr += a[i] == '(' && gq_span(a, i);
    k = gq_n(s, nr + 1u);                            /* (the recipient: a bracket, or the last choice the whole) */
    ri = 0;
    rn = na;
    for (i = 0; a[i]; i++)
        if (a[i] == '(' && gq_span(a, i) && !k--) {
            ri = i;
            rn = gq_span(a, i);
            break;
        }
    {                                                /* (the donor: a bracket of d short enough, at random) */
        uint32_t room = 63u + rn - na, cnt = 0, pick;
        for (i = 0; d[i]; i++) {
            uint32_t l = d[i] == '(' ? gq_span(d, i) : 0;
            cnt += l > 4u && l <= room;
        }
        if (!cnt) return 0;
        pick = gq_n(s, cnt);
        di = dn = 0;
        for (i = 0; d[i]; i++) {
            uint32_t l = d[i] == '(' ? gq_span(d, i) : 0;
            if (l > 4u && l <= room && !pick--) {
                di = i;
                dn = l;
                break;
            }
        }
    }
    if (!dn) return 0;
    if (rn == na) {                                  /* (the whole: the donor beside it, an operator between) */
        static const char *const OPI[3] = {"^", "|", "+"}, *const OPF[3] = {"+", "*", "-"};
        const char *op = flt ? OPF[gq_n(s, 3u)] : OPI[gq_n(s, 3u)];
        if (na + dn + 3u > 63u) {
            for (i = 0; i < dn; i++) o[n++] = d[di + i];
        } else {
            o[n++] = '(';
            for (i = 0; i < na; i++) o[n++] = a[i];
            o[n++] = ')';
            o[n++] = op[0];
            for (i = 0; i < dn; i++) o[n++] = d[di + i];
        }
    } else {
        for (i = 0; i < ri; i++) o[n++] = a[i];
        for (i = 0; i < dn; i++) o[n++] = d[di + i];
        for (i = ri + rn; i < na; i++) o[n++] = a[i];
    }
    o[n] = 0;
    return n <= 63u;
}
static uint32_t fvm_move(gq_work_t *w, char *b, uint32_t seed, uint32_t v)
{
    char o[96];
    uint32_t k, flt;
    fvm_err_t e;
    if (!v || !b[0]) return 0;
    {
        uint32_t n = 0;
        while (b[n]) n++;
        if (fvm_compile(&w->cc, FVM_FULL(&w->pg), b, n, &e)) return 0;
        flt = w->pg.p.ty == FT_FIX;
    }
    for (k = 0; k < 8u; k++) {
        uint32_t s = (seed * 2654435761u) ^ (v * 40503u) ^ (k * 0x9E3779B9u) ^ 0x51ED27u, n = 0, got;
        gq_next(&s);
        got = v < 64u ? gq_mutate(b, o, &s, v) : gq_cross(b, o, &s, flt, (v - 64u) + k * 7u);
        if (!got) continue;
        while (o[n]) n++;
        if (n > 63u || !fvm_cmp(o, b)) continue;
        if (!fvm_compile(&w->cc, FVM_FULL(&w->pg), o, n, &e) && fvm_gen_judge(w, &w->pg.p)) {
            memcpy(b, o, n + 1u);
            return 1;
        }
    }
    {   /* (none played: b as it was, its program the work's again) */
        uint32_t n = 0;
        while (b[n]) n++;
        fvm_compile(&w->cc, FVM_FULL(&w->pg), b, n, &e);
    }
    return 0;
}
#endif
