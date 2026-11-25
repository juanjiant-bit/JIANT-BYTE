/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 1) FVM: the formula machine. Every sound, modifier, mix, effect and punch-in of JIANT BYTE is
 * a formula of time (JIANT-BYTE.md, 4), compiled here from its text to a small program and run on blocks of samples.
 *
 * The language is C's (and the JavaScript bytebeats'): numbers, the variables below, + - * / % & | ^ << >> ~ ! < <= >
 * >= == != && || ?: and parentheses, a string read as a table ("36364689"[t>>11&7]), and calls. Every expression has
 * one of two types, decided when it is compiled (nothing to pay as it runs):
 *   INT  32 bits, unsigned: C's bytebeat, wrapping (t*(42&t>>10) as ever). Integers (123, 0x1F, 9u) are INT.
 *   FIX  Q16.16, signed: the floatbeat. A number with a point (0.5) is FIX.
 * Mixed, INT goes up to FIX; the bit operators take the integer part (JavaScript's ToInt32) and give an INT; INT x FIX
 * keeps INT's 32 bits (the integer part wraps, the fraction is exact: what periodic functions need). A division or a
 * modulo by zero is 0. The result plays by its type: INT as bytebeat (its low byte), FIX as floatbeat (-1 .. 1).
 *
 * Variables: per sample t (INT, the formula's clock), w (FIX, the note's phase in turns), T (FIX, seconds), x / X (INT /
 * FIX, the output a block ago); per block a b c d (INT 0 .. 255), A B C D (FIX 0 .. 1), n (the note), v (the velocity),
 * s (the step), p (FIX: the song's position in beats); per sample O1 .. O6 (FIX: each part's output a block ago,
 * cross-mod; xm_buf, eng_lofi.c); PI (FIX). A per-sample variable the host does not make reads as 0.
 * Calls: sin cos tri saw sqr (a period a turn: an INT argument gives a byte, 256 a turn; a FIX one -1 .. 1), abs min max
 * floor int fix rnd (a hash: INT any, FIX -1 .. 1) sat (a soft clip) clip (-1 .. 1); with a state of their own (one a
 * call, per voice): ph(f) (an oscillator's phase at f Hz, in turns: sin(ph(440)); kept in 32 bits, so it is exact
 * at an LFO's rate too), lp(x,k) hp(x,k) (one-pole filters, k 0 .. 1).
 *
 * The machine runs one instruction over the whole block (FVM_BLK samples) before the next: the cost of reading the
 * program is paid once a block, what is left is the arithmetic. An operand that is a number, a block variable or a
 * per-sample variable is the instruction's own (t >> 10 is one instruction); numbers are folded when compiled. The
 * operators' meaning is written once (FVM_OPS2 / FVM_OPS1) and used both by the folding and by the machine. */
#ifndef FVM_C
#define FVM_C

#define FVM_BLK 32u              /* samples an instruction runs over (the audio's control block, CTL) */
#define FVM_MAXINS 64u           /* instructions a program */
#define FVM_DEPTH 12u            /* its stack (blocks of FVM_BLK) */
#define FVM_MAXSTATE 8u          /* calls with a state of their own (ph lp hp) */
#define FVM_POOL 32u             /* bytes of string tables (the same string once: fvm_primary) */
#define FVM_MAXNODE 160u         /* the compiler's tree */
#define FVM_PH_K ((uint32_t)(0x100000000ull / FS))   /* ph(): a turn (2^32) over the sample rate */

enum { FT_INT, FT_FIX };
/* the per-sample variables (fvm_env_t arr[]) and the block's (u[]) */
enum { FA_T, FA_W, FA_TS, FA_X, FA_XF, FA_O1, FA_O6 = FA_O1 + 5, FA_IN, FA_BEAT, FA_N };   /* (FA_IN: an effect's input) */   /* (FA_O1..: the parts' outputs, cross-mod) */
enum { FU_A, FU_B, FU_C, FU_D, FU_AF, FU_BF, FU_CF, FU_DF, FU_NOTE, FU_VEL, FU_STEP, FU_BEAT, FU_HZ, FU_BPS, FU_SCL, FU_SCT, FU_N };   /* (func-scale) FU_SCL FU_SCT: fvm_music.c fu_scale */

typedef struct {
    uint8_t op, md;              /* md: the right operand (or a push's value): FM_STK FM_IMM FM_UNI FM_ARR */
    uint16_t sl;                 /* (a call's state slot) */
    uint32_t k;                  /* the number, the block variable's or the per-sample variable's index, a table */
} fvm_ins_t;
enum { FM_STK, FM_IMM, FM_UNI, FM_ARR, FM_LOCAL, FM_STATE };

/* a program: its instructions in the caller's storage (ins, cap of them: a part's MATH formula 64, a drum lane's 24),
 * so a short formula costs only its own room */
typedef struct {
    fvm_ins_t *ins;
    uint8_t cap, n, depth, nstate, ty;   /* ty: the result's type */
    uint16_t uses;               /* the per-sample variables it reads (1 << FA_..): the host makes only those */
    uint8_t npool, func;
    uint8_t life_in, life_num, life_fn, life_ft; /* typed input/literal indices (+1), safe prefix fragment */
    uint8_t pool[FVM_POOL];
} fvm_prog_t;
static inline void fvm_prog_init(fvm_prog_t *p, fvm_ins_t *ins, uint32_t cap)
{
    p->ins = ins;
    p->cap = (uint8_t)(cap > FVM_MAXINS ? FVM_MAXINS : cap);
    p->n = 0;
}
/* a program with room for the longest */
typedef struct { fvm_prog_t p; fvm_ins_t ins[FVM_MAXINS]; } fvm_full_t;
#define FVM_FULL(x) (fvm_prog_init(&(x)->p, (x)->ins, FVM_MAXINS), &(x)->p)

/* (funcplay) FUNC's live groove on its music calls and eu() (eng_lofi.c func_run): DENSITY (-64 thins every hit and
 * note away .. +64 retriggers them each 1/16, by a hash of the step they started on), RANGE (-64 .. +64: that share of
 * the notes an octave down / up), NOTES (a mutation: an interval of 0 7 5 12 by the same hash), the parts muted (bit 0
 * the drums, bit 1 the notes). 0 (every host but FUNC's live path): the formula as written, bit for bit */
typedef struct { int8_t dens, range; uint8_t mask, notes; uint32_t seed; } fu_cx_t;
typedef struct {
    const uint32_t *arr[FA_N];   /* per sample (only those in prog.uses need to be there) */
    uint32_t u[FU_N];            /* per block */
    int32_t *state;              /* FVM_MAXSTATE, the voice's, zero at its start */
    const fu_cx_t *fx;           /* (funcplay) 0: none */
} fvm_env_t;

typedef struct { uint32_t s[FVM_DEPTH][FVM_BLK]; } fvm_scratch_t;   /* 1.5 KB: one a core (the second renders tracks too) */

/* errors (fvm_compile's), with the place in the text */
enum { FE_OK, FE_SYNTAX, FE_NAME, FE_ARGS, FE_LONG, FE_DEEP, FE_TREE, FE_POOL, FE_STATE, FE_EMPTY };
typedef struct { uint8_t code; uint16_t pos; } fvm_err_t;

/* ---- the operators, written once */
static inline uint32_t fvm_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    return h ^ (h >> 13);
}
/* (BYTE 0.16, the owner: "pick two of a table at random and combine them into a third: the same pattern hardly ever
 * twice") eu(step, seed): an action pattern of 16 steps a bar, every 4 bars two euclidean rhythms drawn from FVM_EU by
 * the seed and the section, joined by a way drawn too (OR, AND, XOR, a bar each): 1 the step plays */
static const uint8_t FVM_EU[16][2]={{3,8},{5,8},{2,8},{3,16},{5,16},{7,16},{9,16},{4,16},{11,16},{5,12},{7,12},{4,12},
                                    {3,6},{2,6},{6,16},{13,16}};
static uint32_t fvm_eu(uint32_t s, uint32_t seed)
{
    uint32_t h = fvm_hash(seed * 0x9E3779B1u + (s >> 6) * 0x85EBCA77u), i, g[2];
    for (i = 0; i < 2u; i++) {
        uint32_t e = (h >> (i * 8u)) & 15u, k = FVM_EU[e][0], n = FVM_EU[e][1], r = (h >> (16u + i * 4u)) & 15u;
        g[i] = ((s + r) * k) % n < k;
    }
    switch (h >> 30) {
    case 0: return g[0] | g[1];
    case 1: return (g[0] & g[1]) | ((s & 15u) == 0u);  /* (AND: the bar's first step kept: never a silent bar) */
    case 2: return g[0] ^ g[1];
    default: return (s >> 4) & 1u ? g[1] : g[0];
    }
}
#define I32(v) ((int32_t)(v))
#define U32(v) ((uint32_t)(v))
#define FVM_OPS2(X) \
    X(ADD, a + b) X(SUB, a - b) X(MUL, a * b) \
    X(DIVU, b ? a / b : 0u) X(MODU, b ? a % b : 0u) \
    X(AND, a & b) X(OR, a | b) X(XOR, a ^ b) X(SHL, a << (b & 31u)) X(SHR, a >> (b & 31u)) \
    X(LTU, U32(a < b)) X(LEU, U32(a <= b)) X(GTU, U32(a > b)) X(GEU, U32(a >= b)) X(EQ, U32(a == b)) X(NE, U32(a != b)) \
    X(LTS, U32(I32(a) < I32(b))) X(LES, U32(I32(a) <= I32(b))) X(GTS, U32(I32(a) > I32(b))) X(GES, U32(I32(a) >= I32(b))) \
    X(LAND, U32(a && b)) X(LOR, U32(a || b)) \
    X(MINU, a < b ? a : b) X(MAXU, a > b ? a : b) X(MINS, I32(a) < I32(b) ? a : b) X(MAXS, I32(a) > I32(b) ? a : b) \
    X(MULF, U32(I32(((int64_t)I32(a) * I32(b)) >> 16))) \
    X(MULIF, U32((int64_t)a * I32(b))) \
    X(DIVF, I32(b) ? U32(I32(((int64_t)I32(a) * 65536) / I32(b))) : 0u) \
    X(DIVFI, (I32(b) && I32(b) != -1) ? U32(I32(a) / I32(b)) : (I32(b) ? U32(-I32(a)) : 0u)) \
    X(MODF, (I32(b) && I32(b) != -1) ? U32(I32(a) % I32(b)) : 0u) \
    X(EU, fvm_eu(a, b))
#define FVM_OPS1(X) \
    X(NEG, U32(-I32(a))) X(NOT, ~a) X(LNOT, U32(!a)) X(ITOF, a << 16) X(FTOI, U32(I32(a) >> 16)) \
    X(ABS, I32(a) < 0 ? U32(-I32(a)) : a) X(FLOOR, a & 0xFFFF0000u) \
    X(SINI, U32((sine_i(a << 24) + 32768) >> 8)) X(SINF, U32(sine_i(a << 16) * 2)) \
    X(TRII, (a & 128u) ? 255u - ((a & 127u) << 1) : (a & 127u) << 1) \
    X(TRIF, (a & 0x8000u) ? U32(196608 - 4 * I32(a & 0xFFFFu)) : U32(4 * I32(a & 0xFFFFu) - 65536)) \
    X(SAWI, a & 255u) X(SAWF, U32(2 * I32(a & 0xFFFFu) - 65536)) \
    X(SQRI, (a & 128u) ? 0u : 255u) X(SQRF, (a & 0x8000u) ? U32(-65536) : 65536u) \
    X(RNDI, fvm_hash(a)) X(RNDF, U32(I32(fvm_hash(a)) >> 15)) \
    X(SAT, U32(softclip(I32(a) >> 1) * 2))
#define FVM_ENUM(n, e) FO_##n,
enum { FVM_OPS2(FVM_ENUM) FO_NBIN,
       FVM_OPS1(FVM_ENUM) FO_NUN,
       FO_PUSH, FO_SEL, FO_TAB, FO_PH, FO_LP, FO_HP, FO_SAVE, FO_MUSIC };
#undef FVM_ENUM
#define FO_ISBIN(o) ((o) < FO_NBIN)
#define FO_ISUN(o) ((o) > FO_NBIN && (o) < FO_NUN)

static uint32_t fvm_op2(uint32_t op, uint32_t a, uint32_t b)
{
    switch (op) {
#define FVM_CASE(n, e) case FO_##n: return (e);
    FVM_OPS2(FVM_CASE)
#undef FVM_CASE
    }
    return 0u;
}
static uint32_t fvm_op1(uint32_t op, uint32_t a)
{
    switch (op) {
#define FVM_CASE(n, e) case FO_##n: return (e);
    FVM_OPS1(FVM_CASE)
#undef FVM_CASE
    }
    return 0u;
}

/* ---- the machine */
typedef struct { uint8_t input, number; uint32_t value; const int32_t *offset; } fvm_patch_t;
/* (cpupass) an operator over a block's samples, its own loop each (the switch once a block, not a sample): the
 * machine's (fvm_exec) and FUNC's block runner (fvm_func_block) alike */
static __attribute__((noinline)) void fvm_blk_op(uint32_t op, uint32_t *L, const uint32_t *R, uint32_t b, uint32_t n)
{
    uint32_t i;
    switch (op) {
#define FVM_BINLOOP(nm, ex) case FO_##nm: \
        if (R) { for (i = 0; i < n; i++) { uint32_t a = L[i], b = R[i]; L[i] = (ex); } } \
        else { for (i = 0; i < n; i++) { uint32_t a = L[i]; L[i] = (ex); } } \
        break;
    FVM_OPS2(FVM_BINLOOP)
#undef FVM_BINLOOP
#define FVM_UNLOOP(nm, ex) case FO_##nm: for (i = 0; i < n; i++) { uint32_t a = L[i]; L[i] = (ex); } break;
    FVM_OPS1(FVM_UNLOOP)
#undef FVM_UNLOOP
    }
}
#include "fvm_music.c"
#include "fvm_func_run.c"
static __attribute__((noinline)) void fvm_exec(const fvm_prog_t *p, const fvm_env_t *e, fvm_scratch_t *sc,
                                              uint32_t n, uint32_t *out, const fvm_patch_t *patch)
{
    if(p->func){if(!fvm_func_block(p,e,sc,n,out))fvm_func_run(p,e,n,out);return;}
    uint32_t pc, sp = 0u, i, nn, injected[FVM_BLK];
    /* (byte-cpu-light) a slot made of numbers and uniforms alone (A's "1+A*7"): its first lane computed, the rest the
     * same; spread when it meets a sample's value or a filter. The same results, lane for lane (not with a patch) */
    uint8_t uf[FVM_DEPTH];
    const int uo = !patch;
    if (n > FVM_BLK)
        n = FVM_BLK;
    if (!p->n) {
        for (i = 0; i < n; i++)
            out[i] = 0u;
        return;
    }
    for (pc = 0; pc < p->n; pc++) {
        const fvm_ins_t *in = &p->ins[pc];
        uint32_t op = in->op, md = in->md, b = in->k;
        uint32_t *L;
        const uint32_t *R = 0;
        if (patch && pc + 1u == patch->number) b = patch->value;
        if (op == FO_PUSH) {
            L = sc->s[sp];
            uf[sp++] = 0;
            if (md == FM_ARR && e->arr[b]) {
                R = e->arr[b];
                for (i = 0; i < n; i++)
                    L[i] = R[i];
            } else {
                if (md == FM_ARR)                        /* (a variable the host did not make: zeros) */
                    md = FM_IMM, b = 0u;
                if (md == FM_UNI)
                    b = e->u[b];
                if (uo) { L[0] = b; uf[sp - 1u] = 1; continue; }
                for (i = 0; i < n; i++)
                    L[i] = b;
            }
            if (patch && pc + 1u == patch->input)
                for (i=0;i<n;i++) L[i] += (uint32_t)patch->offset[i];
            continue;
        }
        if (FO_ISBIN(op) || op == FO_LP || op == FO_HP) {
            if (md == FM_STK) {
                R = sc->s[--sp];
                if (uf[sp]) { b = R[0]; R = 0; }         /* (a uniform operand: as a number) */
            } else if (md == FM_ARR) {
                R = e->arr[b];
                if (!R)                                  /* (a variable the host did not make: 0) */
                    b = 0u;
            } else if (md == FM_UNI) {
                b = e->u[b];
            }
        }
        if (patch && pc + 1u == patch->input) {
            for(i=0;i<n;i++)injected[i]=(R?R[i]:b)+(uint32_t)patch->offset[i];
            R=injected;
        }
        nn = n;
        if (op == FO_SEL) {
            if (uf[sp - 3u] && uf[sp - 2u] && uf[sp - 1u])
                nn = 1u;
            else
                for (i = sp - 3u; i < sp; i++)
                    if (uf[i]) { uint32_t *S = sc->s[i], v = S[0], k; for (k = 1; k < n; k++) S[k] = v; uf[i] = 0; }
        } else if (uf[sp - 1u]) {
            if (!R && (FO_ISBIN(op) || FO_ISUN(op))) {  /* (stays uniform: its one lane, the scalar ops) */
                uint32_t *S = sc->s[sp - 1u];
                S[0] = FO_ISBIN(op) ? fvm_op2(op, S[0], b) : fvm_op1(op, S[0]);
                continue;
            } else if (op == FO_TAB)
                nn = 1u;
            else {
                uint32_t *S = sc->s[sp - 1u], v = S[0];
                for (i = 1; i < n; i++) S[i] = v;
                uf[sp - 1u] = 0;
            }
        }
        L = sc->s[sp - 1u];
        if (FO_ISBIN(op) || FO_ISUN(op)) {
            fvm_blk_op(op, L, R, b, n);
            continue;
        }
        switch (op) {
        case FO_SEL: {                               /* c ? x : y: [.. c x y] -> [.. r] */
            const uint32_t *X = sc->s[sp - 2u], *Y = sc->s[sp - 1u];
            uint32_t *C = sc->s[sp - 3u];
            for (i = 0; i < nn; i++)
                C[i] = C[i] ? X[i] : Y[i];
            sp -= 2u;
            break;
        }
        case FO_TAB: {                               /* a string as a table: its byte at the index, modulo its length */
            const uint8_t *tb = p->pool + (b >> 8);
            uint32_t len = b & 255u;
            for (i = 0; i < nn; i++)
                L[i] = tb[L[i] % len];
            break;
        }
        case FO_PH: {                                /* ph(f): f Hz (FIX), the phase in 32 bits, out in turns */
            int32_t *st = &e->state[in->sl & 7u];
            uint32_t s = (uint32_t)*st;
            for (i = 0; i < n; i++) {
                s += (uint32_t)(((int64_t)(int32_t)L[i] * (int64_t)FVM_PH_K) >> 16);
                L[i] = s >> 16;
            }
            *st = (int32_t)s;
            break;
        }
        case FO_LP:
        case FO_HP: {                                /* one pole: y += (x - y) k; hp = x - y */
            int32_t *st = &e->state[in->sl & 7u], y = *st;
            for (i = 0; i < n; i++) {
                int32_t x = (int32_t)L[i], k = (int32_t)(R ? R[i] : b);
                k = k < 0 ? 0 : k > 65536 ? 65536 : k;
                y += (int32_t)(((int64_t)(x - y) * k) >> 16);
                L[i] = (uint32_t)(op == FO_LP ? y : x - y);
            }
            *st = y;
            break;
        }
        }
    }
    if (uf[0]) {
        uint32_t v = sc->s[0][0];
        for (i = 0; i < n; i++)
            out[i] = v;
    }
    else
        for (i = 0; i < n; i++)
            out[i] = sc->s[0][i];
}

static __attribute__((noinline)) void fvm_run(const fvm_prog_t *p,const fvm_env_t *e,fvm_scratch_t *s,uint32_t n,uint32_t *out)
{ fvm_exec(p,e,s,n,out,0); }

/* the result as a sample (Q15): INT its low byte, as bytebeat ((v & 255) * 256 - 32640, MATH's); FIX -1 .. 1 */
static inline int32_t fvm_sample(uint32_t ty, uint32_t v)
{
    if (ty == FT_INT)
        return (int32_t)(v & 255u) * 256 - 32640;
    {
        int32_t s = (int32_t)v >> 1;
        return s > 32767 ? 32767 : s < -32767 ? -32767 : s;
    }
}

/* ---- the compiler: the text to a tree (types and folding as it is built), the tree to the program */
enum { NK_NUM, NK_ARR, NK_UNI, NK_OP1, NK_OP2, NK_SEL, NK_TAB, NK_ST, NK_LOCAL, NK_STATE, NK_ARG, NK_CALL, NK_MUSIC };
typedef struct {
    uint8_t kind, op, ty, a, b, c;   /* a b c: children (node indices) */
    uint32_t v;                  /* NK_NUM its value; NK_ARR / NK_UNI the index; NK_TAB the table (offset << 8 | length) */
} fvm_node_t;

typedef struct {
    const char *s;
    uint32_t i, len;
    fvm_node_t nd[FVM_MAXNODE];
    uint32_t nn;
    fvm_prog_t *p;
    fvm_err_t err;
    uint32_t sp, maxsp;
    struct {char name[8];uint8_t kind,slot,ty;} binding[8];
    struct {char name[8],arg[8];uint8_t root;} fn[2];
    uint8_t nb,nlocal,nfn,in_fn,gen_depth,call_depth,parse_depth;
    int8_t bat_k;                /* (engopt) fvm_binat's last answer, at bat_i (the text's index, blanks skipped) */
    uint16_t bat_i;
} fvm_cc_t;                      /* ~2 KB: compiled outside the audio (the UI, the editor), one at a time */

static void fvm_fail(fvm_cc_t *c, uint32_t code)
{
    if (!c->err.code) {
        c->err.code = (uint8_t)code;
        c->err.pos = (uint16_t)c->i;
    }
}
static uint32_t fvm_new(fvm_cc_t *c, uint32_t kind, uint32_t op, uint32_t ty, uint32_t a, uint32_t b, uint32_t cc,
                        uint32_t v)
{
    fvm_node_t *n;
    if (c->nn >= FVM_MAXNODE) {
        fvm_fail(c, FE_TREE);
        return 0u;
    }
    n = &c->nd[c->nn];
    n->kind = (uint8_t)kind;
    n->op = (uint8_t)op;
    n->ty = (uint8_t)ty;
    n->a = (uint8_t)a;
    n->b = (uint8_t)b;
    n->c = (uint8_t)cc;
    n->v = v;
    return c->nn++;
}
static uint32_t fvm_num(fvm_cc_t *c, uint32_t ty, uint32_t v) { return fvm_new(c, NK_NUM, 0, ty, 0, 0, 0, v); }
#define ND(x) (c->nd[x])

static uint32_t fvm_un(fvm_cc_t *c, uint32_t op, uint32_t a, uint32_t ty)
{
    if (ND(a).kind == NK_NUM)                        /* (folded) */
        return fvm_num(c, ty, fvm_op1(op, ND(a).v));
    return fvm_new(c, NK_OP1, op, ty, a, 0, 0, 0);
}
static uint32_t fvm_tofix(fvm_cc_t *c, uint32_t a) { return ND(a).ty == FT_FIX ? a : fvm_un(c, FO_ITOF, a, FT_FIX); }
static uint32_t fvm_toint(fvm_cc_t *c, uint32_t a) { return ND(a).ty == FT_INT ? a : fvm_un(c, FO_FTOI, a, FT_INT); }
static uint32_t fvm_bin(fvm_cc_t *c, uint32_t op, uint32_t a, uint32_t b, uint32_t ty)
{
    if (ND(a).kind == NK_NUM && ND(b).kind == NK_NUM)
        return fvm_num(c, ty, fvm_op2(op, ND(a).v, ND(b).v));
    return fvm_new(c, NK_OP2, op, ty, a, b, 0, 0);
}

/* an operator of the text (its token) on two operands: the types decide the operation */
enum { TK_ADD = 1, TK_SUB, TK_MUL, TK_DIV, TK_MOD, TK_AND, TK_OR, TK_XOR, TK_SHL, TK_SHR, TK_LT, TK_LE, TK_GT, TK_GE,
       TK_EQ, TK_NE, TK_LAND, TK_LOR, TK_MIN, TK_MAX };
static uint32_t fvm_binop(fvm_cc_t *c, uint32_t tk, uint32_t a, uint32_t b)
{
    uint32_t ii = ND(a).ty == FT_INT && ND(b).ty == FT_INT;
    switch (tk) {
    case TK_AND: return fvm_bin(c, FO_AND, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    case TK_OR: return fvm_bin(c, FO_OR, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    case TK_XOR: return fvm_bin(c, FO_XOR, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    case TK_SHL: return fvm_bin(c, FO_SHL, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    case TK_SHR: return fvm_bin(c, FO_SHR, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    case TK_LAND: return fvm_bin(c, FO_LAND, a, b, FT_INT);
    case TK_LOR: return fvm_bin(c, FO_LOR, a, b, FT_INT);
    }
    if (ii) {
        static const uint8_t OPI[] = {0, FO_ADD, FO_SUB, FO_MUL, FO_DIVU, FO_MODU, 0, 0, 0, 0, 0, FO_LTU, FO_LEU, FO_GTU,
                                      FO_GEU, FO_EQ, FO_NE, 0, 0, FO_MINU, FO_MAXU};
        return fvm_bin(c, OPI[tk], a, b, tk >= TK_LT && tk <= TK_NE ? FT_INT : FT_INT);
    }
    if (tk == TK_MUL && ND(a).ty != ND(b).ty) {      /* INT x FIX: INT's 32 bits kept */
        if (ND(a).ty == FT_FIX) {
            uint32_t x = a;
            a = b;
            b = x;
        }
        return fvm_bin(c, FO_MULIF, a, b, FT_FIX);
    }
    if (tk == TK_DIV && ND(a).ty == FT_FIX && ND(b).ty == FT_INT)
        return fvm_bin(c, FO_DIVFI, a, b, FT_FIX);
    a = fvm_tofix(c, a);
    b = fvm_tofix(c, b);
    {
        static const uint8_t OPF[] = {0, FO_ADD, FO_SUB, FO_MULF, FO_DIVF, FO_MODF, 0, 0, 0, 0, 0, FO_LTS, FO_LES, FO_GTS,
                                      FO_GES, FO_EQ, FO_NE, 0, 0, FO_MINS, FO_MAXS};
        return fvm_bin(c, OPF[tk], a, b, tk >= TK_LT && tk <= TK_NE ? FT_INT : FT_FIX);
    }
}

/* ---- the text */
static uint32_t fvm_ch(fvm_cc_t *c) { return c->i < c->len ? (uint8_t)c->s[c->i] : 0u; }
static void fvm_ws(fvm_cc_t *c)
{
    while (c->i < c->len && (c->s[c->i] == ' ' || c->s[c->i] == '\t' || c->s[c->i] == '\n' || c->s[c->i] == '\r'))
        c->i++;
}
static int fvm_eat(fvm_cc_t *c, const char *t)       /* the token t next? (taken) */
{
    uint32_t k = 0;
    fvm_ws(c);
    while (t[k] && c->i + k < c->len && c->s[c->i + k] == t[k])
        k++;
    if (t[k])
        return 0;
    /* (a one-character operator is not the start of a longer one: < of <<, & of &&, | of ||) */
    if (!t[1] && c->i + 1u < c->len) {
        char n = c->s[c->i + 1u];
        if ((t[0] == '<' || t[0] == '>') && (n == t[0] || n == '='))
            return 0;
        if ((t[0] == '&' || t[0] == '|') && n == t[0])
            return 0;
        if ((t[0] == '=' || t[0] == '!') && n == '=')
            return 0;
    }
    c->i += k;
    return 1;
}
static int fvm_cmp(const char *a, const char *b)   /* (strcmp: the firmware's libc has none) */
{
    while (*a && *a == *b)
        a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}
static int fvm_isal(uint32_t ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; }
static int fvm_isdg(uint32_t ch) { return ch >= '0' && ch <= '9'; }

static uint32_t fvm_expr(fvm_cc_t *c);
static uint32_t fvm_cond(fvm_cc_t *c);

static uint32_t fvm_number(fvm_cc_t *c)
{
    uint32_t v = 0u, fr = 0u, sc = 1u, isf = 0;
    if (fvm_ch(c) == '0' && (c->i + 1u < c->len) && (c->s[c->i + 1u] == 'x' || c->s[c->i + 1u] == 'X')) {
        c->i += 2u;
        for (;;) {
            uint32_t ch = fvm_ch(c), d;
            if (fvm_isdg(ch)) d = ch - '0';
            else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10u;
            else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10u;
            else break;
            v = v * 16u + d;
            c->i++;
        }
    } else {
        while (fvm_isdg(fvm_ch(c)))
            v = v * 10u + (fvm_ch(c) - '0'), c->i++;
        if (fvm_ch(c) == '.') {
            isf = 1;
            c->i++;
            while (fvm_isdg(fvm_ch(c))) {
                if (sc < 100000000u) {
                    fr = fr * 10u + (fvm_ch(c) - '0');
                    sc *= 10u;
                }
                c->i++;
            }
        }
    }
    while (fvm_ch(c) == 'u' || fvm_ch(c) == 'U')    /* (C's suffix: written as in C) */
        c->i++;
    if (isf)
        return fvm_num(c, FT_FIX, (v << 16) + (uint32_t)(((uint64_t)fr * 65536u + sc / 2u) / sc));
    return fvm_num(c, FT_INT, v);
}

static const struct { char nm[5]; uint8_t kind, ty, ix; } FVM_VARS[] = {
    {"t", NK_ARR, FT_INT, FA_T}, {"w", NK_ARR, FT_FIX, FA_W}, {"T", NK_ARR, FT_FIX, FA_TS},
    {"x", NK_ARR, FT_INT, FA_X}, {"X", NK_ARR, FT_FIX, FA_XF},
    {"a", NK_UNI, FT_INT, FU_A}, {"b", NK_UNI, FT_INT, FU_B}, {"c", NK_UNI, FT_INT, FU_C}, {"d", NK_UNI, FT_INT, FU_D},
    {"A", NK_UNI, FT_FIX, FU_AF}, {"B", NK_UNI, FT_FIX, FU_BF}, {"C", NK_UNI, FT_FIX, FU_CF}, {"D", NK_UNI, FT_FIX, FU_DF},
    {"n", NK_UNI, FT_INT, FU_NOTE}, {"v", NK_UNI, FT_INT, FU_VEL}, {"s", NK_UNI, FT_INT, FU_STEP},
    {"P", NK_ARR, FT_FIX, FA_BEAT}, {"p", NK_UNI, FT_FIX, FU_BEAT}, {"freq", NK_UNI, FT_FIX, FU_HZ},
    {"O1", NK_ARR, FT_FIX, FA_O1}, {"O2", NK_ARR, FT_FIX, FA_O1 + 1}, {"O3", NK_ARR, FT_FIX, FA_O1 + 2},   /* (JIANT BYTE) */
    {"O4", NK_ARR, FT_FIX, FA_O1 + 3}, {"O5", NK_ARR, FT_FIX, FA_O1 + 4}, {"O6", NK_ARR, FT_FIX, FA_O1 + 5},   /* cross-mod */
    {"I", NK_ARR, FT_FIX, FA_IN},
    {"w3", NK_ARR, FT_FIX, FA_XF}, {"w5", NK_ARR, FT_FIX, FA_IN}, {"wd", NK_ARR, FT_FIX, FA_X},   /* (FORM 3) the chord's third
                                                     * and fifth, the root a little sharp (FORM's own arrays: X, I, x elsewhere) */                    /* (JIANT BYTE) an effect's input (fx.c dist_eq) */                  /* (JIANT BYTE) the song's position in beats (a modifier's clock) */
};
enum { FC_SIN, FC_COS, FC_TRI, FC_SAW, FC_SQR, FC_ABS, FC_MIN, FC_MAX, FC_FLOOR, FC_INT, FC_FIX, FC_RND, FC_SAT,
       FC_CLIP, FC_PH, FC_LP, FC_HP, FC_EU, FC_NOTE, FC_CHORD, FC_KICK, FC_SNARE, FC_HAT };
static const struct { char nm[6]; uint8_t id, nargs; } FVM_CALLS[] = {
    {"sin", FC_SIN, 1}, {"cos", FC_COS, 1}, {"tri", FC_TRI, 1}, {"saw", FC_SAW, 1}, {"sqr", FC_SQR, 1},
    {"abs", FC_ABS, 1}, {"min", FC_MIN, 2}, {"max", FC_MAX, 2}, {"floor", FC_FLOOR, 1}, {"int", FC_INT, 1},
    {"fix", FC_FIX, 1}, {"rnd", FC_RND, 1}, {"sat", FC_SAT, 1}, {"clip", FC_CLIP, 1}, {"ph", FC_PH, 1},
    {"lp", FC_LP, 2}, {"hp", FC_HP, 2}, {"eu", FC_EU, 2},
    {"note", FC_NOTE, 3}, {"chord", FC_CHORD, 3}, {"kick", FC_KICK, 1}, {"snare", FC_SNARE, 1}, {"hat", FC_HAT, 1},
};

static uint32_t fvm_call(fvm_cc_t *c, uint32_t id, uint32_t *ar)
{
    uint32_t a = ar[0], b = ar[1], fx = ND(a).ty == FT_FIX;
    if(id>=FC_NOTE){
        if(!c->p->func){fvm_fail(c,FE_NAME);return 0;}
        c->p->uses|=1u<<FA_T;
        return fvm_new(c,NK_MUSIC,FO_MUSIC,FT_FIX,id<FC_KICK?fvm_toint(c,a):fvm_tofix(c,a),id<FC_KICK?fvm_tofix(c,b):0,id<FC_KICK?fvm_tofix(c,ar[2]):0,id-FC_NOTE);
    }
    switch (id) {
    case FC_SIN: return fx ? fvm_un(c, FO_SINF, a, FT_FIX) : fvm_un(c, FO_SINI, a, FT_INT);
    case FC_COS: return fx ? fvm_un(c, FO_SINF, fvm_bin(c, FO_ADD, a, fvm_num(c, FT_FIX, 16384u), FT_FIX), FT_FIX)
                           : fvm_un(c, FO_SINI, fvm_bin(c, FO_ADD, a, fvm_num(c, FT_INT, 64u), FT_INT), FT_INT);
    case FC_TRI: return fx ? fvm_un(c, FO_TRIF, a, FT_FIX) : fvm_un(c, FO_TRII, a, FT_INT);
    case FC_SAW: return fx ? fvm_un(c, FO_SAWF, a, FT_FIX) : fvm_un(c, FO_SAWI, a, FT_INT);
    case FC_SQR: return fx ? fvm_un(c, FO_SQRF, a, FT_FIX) : fvm_un(c, FO_SQRI, a, FT_INT);
    case FC_ABS: return fvm_un(c, FO_ABS, a, ND(a).ty);
    case FC_MIN: return fvm_binop(c, TK_MIN, a, b);
    case FC_MAX: return fvm_binop(c, TK_MAX, a, b);
    case FC_FLOOR: return fx ? fvm_un(c, FO_FLOOR, a, FT_FIX) : a;
    case FC_INT: return fvm_toint(c, a);
    case FC_FIX: return fvm_tofix(c, a);
    case FC_RND: return fx ? fvm_un(c, FO_RNDF, a, FT_FIX) : fvm_un(c, FO_RNDI, a, FT_INT);
    case FC_SAT: return fvm_un(c, FO_SAT, fvm_tofix(c, a), FT_FIX);
    case FC_CLIP:
        return fvm_bin(c, FO_MAXS, fvm_bin(c, FO_MINS, fvm_tofix(c, a), fvm_num(c, FT_FIX, 65536u), FT_FIX),
                       fvm_num(c, FT_FIX, (uint32_t)-65536), FT_FIX);
    case FC_PH:
        return fvm_new(c, NK_ST, FO_PH, FT_FIX, fvm_tofix(c, a), 0, 0, 0);
    case FC_EU:
        return fvm_bin(c, FO_EU, fvm_toint(c, a), fvm_toint(c, b), FT_INT);
    default:                                         /* lp hp */
        return fvm_new(c, NK_ST, id == FC_LP ? FO_LP : FO_HP, FT_FIX, fvm_tofix(c, a), fvm_tofix(c, b), 0, 0);
    }
}

static uint32_t fvm_unary(fvm_cc_t *c);
static uint32_t fvm_primary(fvm_cc_t *c)
{
    uint32_t ch, k;
    fvm_ws(c);
    ch = fvm_ch(c);
    if (fvm_isdg(ch) || (ch == '.' && c->i + 1u < c->len && fvm_isdg((uint8_t)c->s[c->i + 1u])))
        return fvm_number(c);
    if (ch == '(') {
        c->i++;
        k = fvm_expr(c);
        if (!fvm_eat(c, ")"))
            fvm_fail(c, FE_SYNTAX);
        return k;
    }
    if (ch == '"' || ch == '\'') {                   /* a string as a table: "..."[i] */
        uint32_t q = ch, st = c->p->npool, len = 0, ix, k0;
        c->i++;
        while (c->i < c->len && (uint8_t)c->s[c->i] != q) {
            if (c->p->npool >= FVM_POOL) {
                fvm_fail(c, FE_POOL);
                return 0u;
            }
            c->p->pool[c->p->npool++] = (uint8_t)c->s[c->i++];
            len++;
        }
        if (fvm_ch(c) != q || !len || len > 255u) {
            fvm_fail(c, FE_SYNTAX);
            return 0u;
        }
        c->i++;
        for (k0 = 0; k0 + len <= st; k0++) {         /* (the same string once: its earlier copy, the new one dropped) */
            uint32_t j = 0;
            while (j < len && c->p->pool[k0 + j] == c->p->pool[st + j]) j++;
            if (j == len) {
                c->p->npool = (uint8_t)st;
                st = k0;
                break;
            }
        }
        if (!fvm_eat(c, "[")) {
            fvm_fail(c, FE_SYNTAX);
            return 0u;
        }
        ix = fvm_toint(c, fvm_expr(c));
        if (!fvm_eat(c, "]"))
            fvm_fail(c, FE_SYNTAX);
        if (ND(ix).kind == NK_NUM)
            return fvm_num(c, FT_INT, c->p->pool[st + ND(ix).v % len]);
        return fvm_new(c, NK_TAB, FO_TAB, FT_INT, ix, 0, 0, st << 8 | len);
    }
    if (fvm_isal(ch)) {
        uint32_t s0 = c->i, n;
        char nm[8];
        while (fvm_isal(fvm_ch(c)) || fvm_isdg(fvm_ch(c)))
            c->i++;
        n = c->i - s0;
        if (n >= sizeof nm) {
            fvm_fail(c, FE_NAME);
            return 0u;
        }
        for (k = 0; k < n; k++)
            nm[k] = c->s[s0 + k];
        nm[n] = 0;
        fvm_ws(c);
        if (fvm_ch(c) == '(') {
            uint32_t ar[3] = {0, 0, 0}, na = 0;
            c->i++;
            for (k = 0; k < NELEM(FVM_CALLS); k++)
                if (!fvm_cmp(FVM_CALLS[k].nm, nm))
                    break;
            if (k == NELEM(FVM_CALLS)) {
                uint32_t fn;for(fn=0;fn<c->nfn;fn++)if(!fvm_cmp(c->fn[fn].name,nm))break;
                if(fn==c->nfn){c->i=s0;fvm_fail(c,FE_NAME);return 0;}
                uint32_t arg=fvm_tofix(c,fvm_cond(c));
                if(!fvm_eat(c,")")){fvm_fail(c,FE_ARGS);return 0;}
                return fvm_new(c,NK_CALL,0,ND(c->fn[fn].root).ty,arg,c->fn[fn].root,0,0);
            }
            if (!fvm_eat(c, ")")) {
                do {
                    if (na >= 3u) {
                        fvm_fail(c, FE_ARGS);
                        return 0u;
                    }
                    ar[na++] = fvm_cond(c);
                } while (fvm_eat(c, ","));
                if (!fvm_eat(c, ")"))
                    fvm_fail(c, FE_SYNTAX);
            }
            if (na != FVM_CALLS[k].nargs) {
                fvm_fail(c, FE_ARGS);
                return 0u;
            }
            return fvm_call(c, FVM_CALLS[k].id, ar);
        }
        if(c->in_fn&&!fvm_cmp(nm,c->fn[c->nfn].arg))return fvm_new(c,NK_ARG,0,FT_FIX,0,0,0,0);
        for(k=0;k<c->nb;k++)if(!fvm_cmp(c->binding[k].name,nm))
            return fvm_new(c,c->binding[k].kind,0,c->binding[k].ty,0,0,0,c->binding[k].slot);
        if(!fvm_cmp(nm,"sr"))return fvm_num(c,FT_INT,FS);
        if (!fvm_cmp(nm, "PI"))
            return fvm_num(c, FT_FIX, 205887u);      /* (3.14159 in Q16) */
        for (k = 0; k < NELEM(FVM_VARS); k++)
            if (!fvm_cmp(FVM_VARS[k].nm, nm)) {
                if (FVM_VARS[k].kind == NK_ARR)
                    c->p->uses |= (uint16_t)(1u << FVM_VARS[k].ix);
                return fvm_new(c, FVM_VARS[k].kind, 0, FVM_VARS[k].ty, 0, 0, 0, FVM_VARS[k].ix);
            }
        c->i = s0;
        fvm_fail(c, FE_NAME);
        return 0u;
    }
    fvm_fail(c, FE_SYNTAX);
    return 0u;
}
static uint32_t fvm_unary(fvm_cc_t*);
static uint32_t fvm_unary_inner(fvm_cc_t *c)
{
    uint32_t a;
    if (c->err.code)
        return 0u;
    if (fvm_eat(c, "-")) {
        a = fvm_unary(c);
        return fvm_un(c, FO_NEG, a, ND(a).ty);
    }
    if (fvm_eat(c, "+"))
        return fvm_unary(c);
    if (fvm_eat(c, "~"))
        return fvm_un(c, FO_NOT, fvm_toint(c, fvm_unary(c)), FT_INT);
    if (fvm_eat(c, "!"))
        return fvm_un(c, FO_LNOT, fvm_unary(c), FT_INT);
    return fvm_primary(c);
}
static uint32_t fvm_unary(fvm_cc_t*c)
{
    if(c->parse_depth>=16u){fvm_fail(c,FE_DEEP);return 0;}
    c->parse_depth++;uint32_t r=fvm_unary_inner(c);c->parse_depth--;return r;
}
/* the binary levels, C's precedence, loosest last */
static const struct { char t[3]; uint8_t lv, tk; } FVM_BINS[] = {
    {"*", 0, TK_MUL}, {"/", 0, TK_DIV}, {"%", 0, TK_MOD},
    {"+", 1, TK_ADD}, {"-", 1, TK_SUB},
    {"<<", 2, TK_SHL}, {">>", 2, TK_SHR},
    {"<=", 3, TK_LE}, {">=", 3, TK_GE}, {"<", 3, TK_LT}, {">", 3, TK_GT},
    {"==", 4, TK_EQ}, {"!=", 4, TK_NE},
    {"&", 5, TK_AND}, {"^", 6, TK_XOR}, {"|", 7, TK_OR}, {"&&", 8, TK_LAND}, {"||", 9, TK_LOR},
};
/* (engopt) the binary operator next (the blanks before it skipped, as fvm_eat does): its FVM_BINS index, NELEM for
 * >>>, -1 none. At most one of the table's can be there (fvm_eat's one-character rule: < of <<, & of &&, ..), so each
 * level asks this once, not each of its operators in turn (eighteen fvm_eat a level and an operand: most of a formula's
 * compiling, in the audio when a part's formula changes); the same tree */
static int fvm_binat(fvm_cc_t *c)
{
    uint32_t k, a, b;
    fvm_ws(c);
    if (c->bat_i == c->i)                            /* (each level asks at the same place: once) */
        return c->bat_k;
    c->bat_i = (uint16_t)c->i;
    c->bat_k = -1;
    a = fvm_ch(c);
    b = c->i + 1u < c->len ? (uint8_t)c->s[c->i + 1u] : 0u;
    if (a == '>' && b == '>' && c->i + 2u < c->len && c->s[c->i + 2u] == '>')
        return c->bat_k = (int8_t)NELEM(FVM_BINS);
    for (k = 0; k < NELEM(FVM_BINS); k++) {
        const char *t = FVM_BINS[k].t;
        if ((uint8_t)t[0] != a) continue;
        if (t[1]) { if ((uint8_t)t[1] == b) return c->bat_k = (int8_t)k; continue; }
        if ((a == '<' || a == '>') && (b == a || b == '=')) continue;
        if ((a == '&' || a == '|') && b == a) continue;
        if ((a == '=' || a == '!') && b == '=') continue;
        return c->bat_k = (int8_t)k;
    }
    return -1;
}
static uint32_t fvm_level(fvm_cc_t *c, uint32_t lv)
{
    uint32_t a = lv ? fvm_level(c, lv - 1u) : fvm_unary(c);
    for (;;) {
        int k;
        if (c->err.code)
            return 0u;
        k = fvm_binat(c);
        if (k == (int)NELEM(FVM_BINS) && lv == 2u) {   /* (JavaScript's unsigned shift: the same on INT) */
            c->i += 3u;
            a = fvm_binop(c, TK_SHR, a, fvm_level(c, 1u));
        } else if (k >= 0 && k < (int)NELEM(FVM_BINS) && FVM_BINS[k].lv == lv) {
            c->i += FVM_BINS[k].t[1] ? 2u : 1u;
            a = fvm_binop(c, FVM_BINS[k].tk, a, lv ? fvm_level(c, lv - 1u) : fvm_unary(c));
        } else
            return a;
    }
}
static uint32_t fvm_cond(fvm_cc_t *c)
{
    uint32_t q = fvm_level(c, 9u), x, y;
    if (c->err.code || !fvm_eat(c, "?"))
        return q;
    x = fvm_expr(c);
    if (!fvm_eat(c, ":")) {
        fvm_fail(c, FE_SYNTAX);
        return 0u;
    }
    y = fvm_cond(c);
    if (ND(x).ty != ND(y).ty) {
        x = fvm_tofix(c, x);
        y = fvm_tofix(c, y);
    }
    if (ND(q).kind == NK_NUM)                        /* (folded) */
        return ND(q).v ? x : y;
    return fvm_new(c, NK_SEL, FO_SEL, ND(x).ty, q, x, y, 0);
}
static uint32_t fvm_expr(fvm_cc_t *c)
{
    uint32_t a = fvm_cond(c);
    while (!c->err.code && fvm_eat(c, ","))          /* (the comma: the last one counts) */
        a = fvm_cond(c);
    return a;
}

/* ---- the tree to the program */
static void fvm_emit(fvm_cc_t *c, uint32_t op, uint32_t md, uint32_t k, uint32_t sl, uint32_t ty, uint32_t rhs)
{
    fvm_ins_t *in;
    if (c->p->n >= c->p->cap) {
        fvm_fail(c, FE_LONG);
        return;
    }
    in = &c->p->ins[c->p->n++];
    in->op = (uint8_t)op;
    in->md = (uint8_t)md;
    in->sl = (uint16_t)(sl | (ty << 8) | (rhs << 9));
    in->k = k;
}
static void fvm_push_sp(fvm_cc_t *c, int32_t d)
{
    c->sp = (uint32_t)((int32_t)c->sp + d);
    if (c->sp > c->maxsp)
        c->maxsp = c->sp;
}
/* a node that can be an instruction's own operand: its mode and value (0: it is not) */
static uint32_t fvm_leaf(const fvm_node_t *n, uint32_t *k)
{
    *k = n->v;
    return n->kind == NK_NUM ? FM_IMM : n->kind == NK_UNI ? FM_UNI : n->kind == NK_ARR ? FM_ARR : n->kind == NK_LOCAL ? FM_LOCAL : n->kind == NK_STATE ? FM_STATE : FM_STK;
}
static int fvm_commut(uint32_t op)
{
    return op == FO_ADD || op == FO_MUL || op == FO_AND || op == FO_OR || op == FO_XOR || op == FO_EQ || op == FO_NE ||
           op == FO_LAND || op == FO_LOR || op == FO_MINU || op == FO_MAXU || op == FO_MINS || op == FO_MAXS ||
           op == FO_MULF;
}
static uint32_t fvm_mirror(uint32_t op)              /* a < b == b > a */
{
    switch (op) {
    case FO_LTU: return FO_GTU;
    case FO_GTU: return FO_LTU;
    case FO_LEU: return FO_GEU;
    case FO_GEU: return FO_LEU;
    case FO_LTS: return FO_GTS;
    case FO_GTS: return FO_LTS;
    case FO_LES: return FO_GES;
    case FO_GES: return FO_LES;
    }
    return fvm_commut(op) ? op : FO_NBIN;
}
static void fvm_gen(fvm_cc_t*,uint32_t);
static void fvm_gen_inner(fvm_cc_t *c, uint32_t x)
{
    const fvm_node_t *n = &ND(x);
    uint32_t md, k;
    if (c->err.code)
        return;
    switch (n->kind) {
    case NK_MUSIC:
        fvm_gen(c,n->a);
        if(n->v<2u){fvm_gen(c,n->b);fvm_gen(c,n->c);}
        fvm_emit(c,FO_MUSIC,FM_IMM,n->v,0,FT_FIX,0);
        if(n->v<2u)fvm_push_sp(c,-2);
        return;
    case NK_ARG:
        if(!c->call_depth){fvm_fail(c,FE_NAME);return;}
        fvm_emit(c,FO_PUSH,FM_LOCAL,5u+c->call_depth,0,FT_FIX,FT_FIX);fvm_push_sp(c,1);return;
    case NK_CALL:
        if(c->call_depth>=4u){fvm_fail(c,FE_DEEP);return;}
        fvm_gen(c,n->a);fvm_emit(c,FO_SAVE,FM_LOCAL,6u+c->call_depth,0,0,0);fvm_push_sp(c,-1);
        c->call_depth++;fvm_gen(c,n->b);c->call_depth--;return;
    case NK_NUM:
    case NK_UNI:
    case NK_LOCAL:
    case NK_STATE:
    case NK_ARR:
        md = fvm_leaf(n, &k);
        fvm_emit(c, FO_PUSH, md, k, 0, n->ty, n->ty);
        fvm_push_sp(c, 1);
        return;
    case NK_OP1:
        fvm_gen(c, n->a);
        fvm_emit(c, n->op, FM_STK, 0, 0, n->ty, 0);
        return;
    case NK_TAB:
        fvm_gen(c, n->a);
        fvm_emit(c, FO_TAB, FM_STK, n->v, 0, n->ty, 0);
        return;
    case NK_SEL:
        fvm_gen(c, n->a);
        fvm_gen(c, n->b);
        fvm_gen(c, n->c);
        fvm_emit(c, FO_SEL, FM_STK, 0, 0, n->ty, 0);
        fvm_push_sp(c, -2);
        return;
    case NK_ST:
        if (c->p->nstate >= FVM_MAXSTATE) {
            fvm_fail(c, FE_STATE);
            return;
        }
        fvm_gen(c, n->a);
        if (n->op == FO_PH) {
            fvm_emit(c, FO_PH, FM_STK, 0, c->p->nstate++, n->ty, 0);
            return;
        }
        md = fvm_leaf(&ND(n->b), &k);
        if (md == FM_STK) {
            fvm_gen(c, n->b);
            fvm_push_sp(c, -1);
        }
        fvm_emit(c, n->op, md, k, c->p->nstate++, n->ty, ND(n->b).ty);
        return;
    case NK_OP2: {
        uint32_t op = n->op, l = n->a, r = n->b, ml = fvm_leaf(&ND(l), &k);
        md = fvm_leaf(&ND(r), &k);
        if (md == FM_STK && ml != FM_STK && fvm_mirror(op) != FO_NBIN) {   /* the operand on the right: one push less */
            op = fvm_mirror(op);
            r = n->a;
            l = n->b;
            md = fvm_leaf(&ND(r), &k);
        }
        fvm_gen(c, l);
        if (md == FM_STK) {
            fvm_gen(c, r);
            fvm_push_sp(c, -1);
        }
        fvm_emit(c, op, md, k, 0, n->ty, ND(r).ty);
        return;
    }
    }
}

static void fvm_gen(fvm_cc_t*c,uint32_t x)
{
    if(c->gen_depth>=24u){fvm_fail(c,FE_DEEP);return;}
    c->gen_depth++;fvm_gen_inner(c,x);c->gen_depth--;
}
#include "fvm_func_compile.c"

/* compile src (len bytes) into p; FE_OK (0) or the error, its place in e */
static uint32_t fvm_compile(fvm_cc_t *c, fvm_prog_t *p, const char *src, uint32_t len, fvm_err_t *e)
{
    uint32_t root, cap = p->cap;
    fvm_ins_t *ins = p->ins;
    memset(p, 0, sizeof *p);
    p->ins = ins;
    p->cap = (uint8_t)(ins ? cap : 0u);
    c->s = src;
    c->i = 0;
    c->len = len;
    c->nn = 0;
    c->p = p;
    c->err.code = FE_OK;
    c->err.pos = 0;
    c->sp = c->maxsp = 0; c->nb=c->nlocal=c->nfn=c->in_fn=c->gen_depth=c->call_depth=c->parse_depth=0;
    c->bat_i = 0xFFFFu;
    fvm_ws(c);
    if (c->i >= len) {
        c->err.code = FE_EMPTY;
    } else {
        if(fvm_eat(c,"func")){
            p->func=1;
            uint32_t nesting=0,unary=0,ternary=0;
            if(len>255u)fvm_fail(c,FE_LONG);
            for(uint32_t j=c->i;j<len&&!c->err.code;j++){
                char z=src[j];if(z=='('){if(++nesting>12u)fvm_fail(c,FE_DEEP);}else if(z==')'&&nesting)nesting--;
                unary=(z=='!'||z=='~'||z=='-'||z=='+')?unary+1u:0u;
                if(unary>12u||(z=='?'&&++ternary>8u))fvm_fail(c,FE_DEEP);
            }
            root=c->err.code?0:fvm_func_parse(c);
        }
        else root = fvm_expr(c);
        fvm_ws(c);
        if (!c->err.code && c->i < len)
            fvm_fail(c, FE_SYNTAX);
        if (!c->err.code) {
            p->ty = ND(root).ty;
            fvm_gen(c, root);
        }
        if (!c->err.code && c->maxsp > FVM_DEPTH)
            fvm_fail(c, FE_DEEP);
    }
    if (e)
        *e = c->err;
    if (c->err.code) {
        memset(p, 0, sizeof *p);
        p->ins = ins;
        p->cap = (uint8_t)(ins ? cap : 0u);
        return c->err.code;
    }
    p->depth = (uint8_t)c->maxsp;
    if(p->func){
        uint32_t voices=0;for(uint32_t k=0;k<p->n;k++)if(p->ins[k].op==FO_MUSIC)voices+=p->ins[k].k==1u?4u:1u;
        if(voices>8u){p->n=0;if(e){e->code=FE_LONG;e->pos=(uint16_t)len;}return FE_LONG;}
        p->life_fn=p->n;p->life_ft=FT_FIX;return FE_OK;
    }
    { /* Compiler-derived, stack-complete prefixes only. No state, tables or cross-track reads. */
        uint32_t i, sp=0, safe=1, waveform=0;
        for(i=0;i<p->n;i++) {
            const fvm_ins_t *in=&p->ins[i]; uint32_t op=in->op;
            if(op==FO_PUSH) sp++;
            else if((FO_ISBIN(op)||op==FO_LP||op==FO_HP)&&in->md==FM_STK) sp--;
            else if(op==FO_SEL) sp-=2;
            if(op==FO_PH||op==FO_LP||op==FO_HP||op==FO_TAB||(in->md==FM_ARR&&in->k>=FA_X)) safe=0;
            if(op>=FO_SINI&&op<=FO_RNDF)waveform=1;
            if(safe&&sp==1&&(waveform||
                (p->ty==FT_INT&&i+1u==p->n))) {
                p->life_fn=(uint8_t)(i+1);p->life_ft=(uint8_t)((in->sl>>8)&1u);
            }
            if((op==FO_PUSH||FO_ISBIN(op))&&in->md==FM_ARR&&in->k<=FA_TS &&
               (!p->life_in || (in->k==FA_W&&p->ins[p->life_in-1u].k!=FA_W))) p->life_in=(uint8_t)(i+1);
            if(!p->life_num&&in->md==FM_IMM&&(op==FO_PUSH||FO_ISBIN(op))) {
                uint32_t q=(in->sl&512u)?in->k>>8:in->k<<8;
                if(in->k && in->k<=((in->sl&512u)?32u*65536u:32u) && q) p->life_num=(uint8_t)(i+1);
            }
        }
    }
    return FE_OK;
}
#undef ND
#undef I32
#undef U32

/* ---- the formula's clock: where t comes from (JIANT-BYTE.md 5, "Tiempo de las ecuaciones", KEY)
 * Two clocks run in it: the note's (t at the note's speed, as MATH's: a higher key, a faster formula) and the tempo's
 * (t at the reference note's speed, or locked to the song's position: the key does not move it). KEY picks which
 * makes t:
 *   LIBRE   the note's (MATH as ever)
 *   ALTURA  the tempo's above bit SPLIT (the rhythm, the phrase), the note's below (the timbre, the pitch)
 *   FIJO    the tempo's alone (the keys only trigger, or pick with n)
 * then RATE (its speed), START (where it starts), LOOP (a window it repeats in), WARP (a mutation, MATH's VAR, and a
 * fold, MATH's BEND). w is the phase of the t made (32 t a turn, MATH's FLOAT: its fraction is the note's), T the seconds since
 * the clock's reset. RESET: the host calls fvm_clock_reset at a note, a step or a bar (SYNC's choice). */
enum { FK_LIBRE, FK_ALTURA, FK_FIJO };
enum { FY_NOTE, FY_STEP, FY_BAR, FY_SONG, FY_FREE };
typedef struct {
    uint8_t key, sync, split;    /* KEY, SYNC (what resets it, or locks it to the song), SPLIT (ALTURA's bit, 0..24) */
    uint8_t mut;                 /* WARP: MATH's VAR (0..127: v >> 4 the way, v & 15 how much) */
    uint8_t fold;                /* WARP: t ^ t >> fold (0: none) */
    uint8_t loop;                /* LOOP: t held in 2^loop steps (0: free) */
    uint32_t rate;               /* RATE, Q16 (65536: as written; x1/16 .. x16) */
    uint32_t start;              /* START: t's first step */
} fvm_clk_par_t;
typedef struct {
    uint32_t note_inc;           /* the note's clock: t a sample, Q24 */
    uint32_t ref_inc;            /* the tempo's, free: the reference note's t a sample, Q24 */
    uint32_t song_t, song_inc;   /* SYNC BAR / SONG: the tempo's t at the block's start and its step a sample (Q16) */
} fvm_clk_ctx_t;
typedef struct {
    uint32_t tn, tnf;            /* the note's clock: t and its fraction (Q24) */
    uint32_t tq, tqf;            /* the tempo's */
    uint64_t sec;                /* T: seconds, Q32.32 of Q16 (the raw value << 16) */
} fvm_clk_t;

static uint32_t fvm_mut(uint32_t t, uint32_t v)       /* MATH's VAR (eng_lofi.c lofi_mut), the same mutations */
{
    uint32_t k = v & 15u, m;
    switch ((v >> 4) & 7u) {
    case 0: return t;
    case 1: return t + (t >> (9u - k / 2u)) * (k + 1u);
    case 2: return t ^ (t >> (2u + k / 2u));
    case 3: return t | (t >> (1u + k / 2u));
    case 4: return t & ~(1u << (2u + k / 2u));
    case 5: m = (1u << (3u + k / 2u)) - 1u; return (t & ~m) | (~t & m);
    case 6: return t * (3u + k);
    default: return t ^ ((t >> 3) * ((k + 1u) * 0x9E3779B1u >> 24));
    }
}
static void fvm_clock_reset(fvm_clk_t *c)
{
    memset(c, 0, sizeof *c);
}
/* n samples of t (INT), w and T (FIX), each only if asked for (0: not made) */
static __attribute__((noinline)) void fvm_clock(fvm_clk_t *c, const fvm_clk_par_t *p, const fvm_clk_ctx_t *x,
                                                uint32_t n, uint32_t *T, uint32_t *W, uint32_t *S)
{
    uint32_t i, rate = p->rate ? p->rate : 65536u;
    uint32_t ni = (uint32_t)(((uint64_t)x->note_inc * rate) >> 16), qi = (uint32_t)(((uint64_t)x->ref_inc * rate) >> 16);
    uint32_t m = p->split >= 32u ? 0xFFFFFFFFu : (1u << p->split) - 1u, lm = p->loop ? (1u << p->loop) - 1u : 0u;
    uint32_t song = p->sync == FY_BAR || p->sync == FY_SONG;
    uint32_t sr = (uint32_t)(((uint64_t)x->song_inc * rate) >> 16);
    for (i = 0; i < n; i++) {
        uint32_t t, tq = song ? (uint32_t)(((uint64_t)x->song_t * rate >> 16) + ((uint64_t)i * sr >> 16)) : c->tq;
        t = p->key == FK_LIBRE ? c->tn : p->key == FK_FIJO ? tq : (tq & ~m) | (c->tn & m);
        if (lm)
            t &= lm;
        t += p->start;
        if (p->mut)
            t = fvm_mut(t, p->mut);
        if (p->fold)
            t ^= t >> p->fold;
        if (T)
            T[i] = t;
        if (W)
            W[i] = (t << 11) | ((c->tnf >> 16) << 3);   /* (32 t a turn, Q16 turns: MATH's x << 3, of the t made here; only
                                                         * t's low 5 bits reach the fraction: ALTURA keeps the note's) */
        if (S)
            S[i] = (uint32_t)(c->sec >> 16);
        c->tnf += ni;
        c->tn += c->tnf >> 24;
        c->tnf &= 0xFFFFFFu;
        c->tqf += qi;
        c->tq += c->tqf >> 24;
        c->tqf &= 0xFFFFFFu;
        c->sec += (uint64_t)(0x100000000ull / FS);   /* (2^32 / FS: a sample's seconds, Q16 << 16) */
    }
}

#endif
