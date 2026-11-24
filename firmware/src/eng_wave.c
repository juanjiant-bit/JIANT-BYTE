/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 3) WAVE: a parametric wavetable, after the software wavetable synths' layout (two oscillators,
 * a position through the table, a warp of the phase, unison, a sub), every piece of it an equation. The table is not
 * stored: each oscillator is a formula f(w, A) of the FVM (fvm.c), w the phase of the cycle in turns and A the
 * position POS (0 .. 1), run a block at a time; moving POS moves through the "frames" the formula draws.
 *   A      the first oscillator's formula (WAVE_BANK)       POS    the position (A in the formula), the matrix's SHAPE
 *   WARP   the phase bent before the formula: OFF SYNC BEND MIRROR QUANT FM (B into A's phase)        AMT its amount
 *   B      the second oscillator's formula                   MIX    A .. B
 *   DETUNE 0 one voice; up three, spread around the note     SUB    a sine an octave down
 * CAOS (P_CAOS, MATH's) moves POS with a bytebeat of the note's time: 0 still, up to POS run by the formula's bits.
 * State: v->ph[0..2] the three phases (32 bits a turn), v->s[2] t (the note's clock, 32 t a cycle), v->s[3] the sub's
 * phase, v->s[6] / s[7] A's and B's states (ph lp hp: one each). Memory: one more program a part (wave_prg_b); A's in
 * MATH's (lofi_prg: a part is MATH or WAVE). Not yet: a table captured from a running formula (CAPTURA) and band-
 * limited copies of it (they need RAM the effects' lines hold today: JIANT-BYTE.md 5) */
#define ENGI_WAVE 13u                                /* (the retired ADD's number: engines.c ENGINES[13]) */
static const char *const WAVE_BANK[] = {
    "sin(w)",                                        /* SINE */
    "sin(w+sin(w*2)*A*2)",                           /* FM2: a 2:1 modulator, its index POS */
    "sin(w+sin(w*(1+floor(A*7)))*A)",                /* FMR: the ratio and the index up with POS */
    "tri(w*(1+A*7))",                                /* TRISY: a triangle synced up */
    "saw(w)*(1-A)+sqr(w)*A",                         /* SAWSQ: saw into square */
    "sat(sin(w)*(1+A*12))",                          /* DRIVE: a sine driven */
    "sin(w)*sin(w*(2+floor(A*6)))",                  /* RING: ringed with a partial */
    "sin(sin(w)*(1+A*4))",                           /* FOLD: folded */
    "sin(w)+sin(w*3)*A/3+sin(w*5)*A/5+sin(w*7)*A/7", /* ODD: odd partials grown */
    "(sqr(w)-sqr(w+A/2))/2",                         /* PULSE: its width POS */
    "sin(floor(w*(4+A*28))/(4+A*28))",               /* STEP: a stepped sine, fewer steps down */
    "fix((int(w*256)^int(A*255))&255)/128-1",        /* BITS: the phase's byte xor POS's */
    "sin(w*(1+A*15))*(1-saw(w))/2",                  /* FORMT: a synced sine in a window */
    "rnd(floor(w*(8+A*248)))",                       /* NOISE: a cycle of noise, finer up */
    "sin(w)*(1-A)+tri(w*2)*A",                       /* SINTR: sine into a triangle an octave up */
    "sat((sin(w)+sin(w*2+A))*(1+A*3))",              /* SATUR: two partials, phased, saturated */
};
static const char *const N_WAVEF[] = {"SINE", "FM2", "FMR", "TRISY", "SAWSQ", "DRIVE", "RING", "FOLD", "ODD", "PULSE",
                                      "STEP", "BITS", "FORMT", "NOISE", "SINTR", "SATUR", 0};
#define WAVE_NF (sizeof WAVE_BANK / sizeof WAVE_BANK[0])
_Static_assert(WAVE_NF == 16u && sizeof N_WAVEF / sizeof N_WAVEF[0] == WAVE_NF + 1u, "WAVE: a name a formula");
static const char *const N_WARP[] = {"OFF", "SYNC", "BEND", "MIRROR", "QUANT", "FM", 0};
enum { WW_OFF, WW_SYNC, WW_BEND, WW_MIRROR, WW_QUANT, WW_FM };

static const fvm_prog_t *wave_prog(fvm_prog_t *pg, uint16_t *id, uint32_t f, lofi_work_t *w)
{
    uint32_t want = 0x100u + f % WAVE_NF;
    if (*id != want) {
        const char *s = WAVE_BANK[f % WAVE_NF];
        uint32_t n = 0;
        while (s[n]) n++;
        if (fvm_compile(&w->c, pg, s, n, 0) || pg->nstate > 1u)
            pg->n = 0;                               /* (silent) */
        *id = (uint16_t)want;
    }
    return pg;
}
/* the phase w (Q16 turns, 0 .. 65535) bent by WARP, its amount k (0 .. 127) */
static inline uint32_t wave_warp(uint32_t w, uint32_t mode, uint32_t k)
{
    switch (mode) {
    case WW_SYNC: return (w * (16u + k * 112u / 127u) >> 4) & 0xFFFFu;   /* x1 .. x8 the cycle */
    case WW_BEND: {                                  /* the half-cycle's point moved: 0.5 .. 0.05 */
        uint32_t m = 32768u - k * 29491u / 127u;
        return w < m ? w * 32768u / m : 32768u + (w - m) * 32768u / (65536u - m);
    }
    case WW_MIRROR: {                                /* folded back on itself, the fold's point by AMT */
        uint32_t m = 65535u - k * 32767u / 127u;
        return w < m ? w * 65535u / m : (65535u - w) * 65535u / (65536u - m);
    }
    case WW_QUANT: {                                 /* 64 .. 4 steps a cycle */
        uint32_t s = 65536u / (64u - k * 60u / 127u);
        return w / s * s;
    }
    }
    return w;
}

/* (byte-cpu-light) A formula of the bank that is f(w, A) alone and smooth, drawn once into a table of 128 points a
 * part and an oscillator at its POS, read by every voice and unison voice of the part (linear between points: under
 * -48 dB for the bank's partials up to the 7th; SAWSQ's and PULSE's edges a ramp of 1/128 turn). BITS STEP NOISE stay exact, as CAOS
 * (POS moving inside a block), LIFE's own voice and USER. Only a part of more than one voice or with unison (where the
 * table is shared); a single line runs the formula as written. */
static uint32_t trk_nvoice(const track_t *t);   /* (voice.c) */
#define WT_N 128u
static int16_t wt_tab[NTRK][1][WT_N + 1u];          /* (A's only: 1.5 KB of RAM, all the RAM has) */
static uint16_t wt_key[NTRK][1], wt_bkey[NTRK][1];  /* 1 + formula x 128 + POS (0: none); the one being drawn */
static uint8_t wt_bi[NTRK][1], wt_bn[NTRK][1], wt_go[NTRK];   /* .. its next point, its points drawn; a quarter allowed
                                                         * this block (wave_block), a bit an oscillator */
static void wave_block(track_t *t) { wt_go[(uint32_t)(t - trk) % NTRK] = 3u; }
static int wt_smooth(uint32_t f) { return f != 10u && f != 11u && f != 13u; }   /* (SAWSQ PULSE too: their edge a 1/128 turn ramp) */
static const int16_t *wt_get(uint32_t tr, uint32_t r, uint32_t o, const fvm_prog_t *pg, uint32_t f, uint32_t pos, uint32_t many,
                             const fvm_env_t *e0, lofi_work_t *wk)
{
    uint16_t key = (uint16_t)(1u + f * 128u + pos);
    uint32_t i, k, k0, k1, W[FVM_BLK], T[FVM_BLK] = {0}, y[FVM_BLK];
    if (!pg->n || pg->nstate || (pg->uses & ~(1u << FA_W)) || !wt_smooth(f))
        return 0;
    if (!many || (f == 3u && o))                        /* (one voice, no unison: the formula as written; B's TRISY: its
                                                         * sync edges exact) */
        return 0;
    if (wt_key[r][0] == key)
        return wt_tab[r][0];
    /* (BYTE, CPU: the owner, "one WAVE voice 453 us") a new POS redrawn a quarter a block (32 points), read meanwhile as
     * it is (the old POS's points and the new's: neighbours, alike); the first one whole. Was the whole table in one
     * block each time POS moved (an envelope, an LFO on it: every block) */
    if (wt_key[r][0] && !(wt_go[tr] >> o & 1u))   /* (one quarter a part a block, whichever voice asks first) */
        return wt_tab[r][0];
    wt_go[tr] = (uint8_t)(wt_go[tr] & ~(1u << o));
    if (wt_bkey[r][0] != key) { wt_bkey[r][0] = key; wt_bn[r][0] = 0; }   /* (a POS moving every block: the points
                                                         * redrawn round and round, each at most four blocks old) */
    k0 = wt_key[r][0] ? wt_bi[r][0] : 0u;
    k1 = wt_key[r][0] ? k0 + 32u : WT_N;
    {
        fvm_env_t e = *e0;
        e.arr[FA_T] = T; e.arr[FA_W] = W;
        e.u[FU_AF] = pos * 516u;
        for (k = k0; k < k1; k += FVM_BLK) {
            for (i = 0; i < FVM_BLK; i++) W[i] = (k + i) % WT_N * (65536u / WT_N);
            fvm_run(pg, &e, &wk->s, FVM_BLK, y);
            for (i = 0; i < FVM_BLK; i++) wt_tab[r][0][(k + i) % WT_N] = (int16_t)clamp((int32_t)y[i] >> 1, -32768, 32767);
        }
    }
    wt_tab[r][0][WT_N] = wt_tab[r][0][0];
    wt_bi[r][0] = (uint8_t)(k1 % WT_N);
    wt_bn[r][0] = (uint8_t)(wt_bn[r][0] + 32u > 255u ? 255u : wt_bn[r][0] + 32u);
    if (!wt_key[r][0] || wt_bn[r][0] >= WT_N)
        wt_key[r][0] = key;
    return wt_tab[r][0];
}
/* (byte-cpu-light) B's table: a row of a track that is not WAVE (its own row idle), the k-th such for the k-th WAVE
 * track (no RAM: who has which is the tracks' engines); none free, B exact as before. Only where the table's error is
 * under -60 dB (SINE, STEP: wave_part); a sharper B (FM2, DRIVE ..) stays exact */
static uint32_t wt_brow(uint32_t tr)
{
    uint32_t j, k = 0;
    for (j = 0; j < tr; j++)
        k += trk[j].engine == ENGI_WAVE;
    for (j = 0; j < NTRK; j++)
        if (trk[j].engine != ENGI_WAVE && !k--)
            return j;
    return NTRK;
}
static inline void wt_read(const int16_t *t, const uint32_t *W, uint32_t n, uint32_t *y)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t w = W[i] & 0xFFFFu, j = w >> 9;
        int32_t a = t[j];
        y[i] = (uint32_t)((a + (((t[j + 1u] - a) * (int32_t)(w & 511u)) >> 9)) * 2);
    }
}

static void wave_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[2] = v->s[3] = 0;
    v->s[6] = v->s[7] = 0;
}

static __attribute__((noinline)) void wave_fm_read(const int16_t *ta, const fvm_prog_t *pa, const fvm_env_t *ea,
                                                   fvm_scratch_t *sc, uint32_t c, uint32_t *y, int post)
{
    uint32_t k = MC_CPU() & 1u, *Wm = fl_wt[k][0], *Tm = fl_wt[k][1];   /* (formula_life.c: a core's, not the stack's) */
    fvm_env_t em = *ea;                              /* (its LIFE after: on the moved phase too) */
    if (fl.on && fl.link_amt && c <= FVM_BLK && fl_fm(ea, c, Wm, Tm)) { em.arr[FA_W] = Wm; if (ea->arr[FA_T]) em.arr[FA_T] = Tm; }
    wt_read(ta, em.arr[FA_W], c, y);
    if (post) fl_post(pa, &em, sc, c, y);
}
#define WAVE_CH FVM_BLK                               /* (BYTE 0.16) a part's samples: the block in two, its arrays half
                                                         * the stack (the audio's interrupt overflowed its 8 KB on the FM-1) */
static __attribute__((noinline)) void wave_part(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, uint32_t o0)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, u;
    lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
    uint32_t fa = (uint32_t)p[P_E0] % WAVE_NF, fb = (uint32_t)p[P_E4] % WAVE_NF;
    uint32_t warp = (uint32_t)clamp(p[P_E2], 0, WW_FM), amt = (uint32_t)clamp(p[P_E3], 0, 127);
    uint32_t mix = (uint32_t)clamp(p[P_E5], 0, 127), det = (uint32_t)clamp(p[P_E6], 0, 127);
    uint32_t sub = (uint32_t)clamp(p[P_E7], 0, 127), caos = (uint32_t)clamp(p[P_CAOS], 0, 127);
    int32_t pos = clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /* (the matrix's SHAPE moves POS) */
    uint32_t inc = pitch_inc(clamp(m->pitch16, 0, 2047)), nu = det ? 3u : 1u, need_b = mix || warp == WW_FM;
    const fvm_prog_t *pa, *pb;
    const int16_t *ta, *tb;
    uint32_t W[WAVE_CH], T[WAVE_CH], ya[WAVE_CH], yb[WAVE_CH];
    int32_t acc[WAVE_CH];
    fvm_env_t ea = {{T, W, 0, 0, 0}, {0}, &v->s[6]}, eb = {{T, W, 0, 0, 0}, {0}, &v->s[7]};
    if (n > WAVE_CH)
        n = WAVE_CH;
    if (!wave_prg_b[tr].ins)
        fvm_prog_init(&wave_prg_b[tr], wave_ins_b[tr], 32u);
    pa = wave_prog(lofi_slot(tr), &lofi_prg_id[tr], fa, wk);
    pb = need_b ? wave_prog(&wave_prg_b[tr], &wave_id_b[tr], fb, wk) : 0;
    xm_env(&ea, pa);                                 /* (cross-mod: O1 .. O6) */
    if (pb)
        xm_env(&eb, pb);
    for (i = 0; i < n; i++) {
        uint32_t tt = (uint32_t)v->s[2] + i, ps = (uint32_t)pos;
        T[i] = pt_time(tt >> 2, 0u);                 /* (t: ~11 kHz from the note's start; the white punch-ins') */
        if (caos)                                    /* CAOS: POS moved by the time's bits, more and faster up */
            ps = (ps + (((tt >> (13u - caos / 16u)) * (tt >> 9) & 127u) * caos >> 7)) & 127u;
        if (pt_ops & PT_TIME_OPS)                    /* (BYTE 0.16) the white punch-ins: a wave has no time of its own,
                                                      * so POS walks by the transformed time's bits (each its rhythm) */
            ps = (ps + ((T[i] >> 9) * 37u & 63u)) & 127u;
        acc[i] = (int32_t)(ps * 516u);               /* (POS in FIX, 0 .. 65532: kept here a moment) */
    }
    ea.u[FU_AF] = eb.u[FU_AF] = (uint32_t)acc[0];
    ea.u[FU_NOTE] = eb.u[FU_NOTE] = v->note;
    ea.u[FU_VEL] = eb.u[FU_VEL] = v->vel;
    {   /* (byte-cpu-light) the parts' tables, CAOS off (POS still in the block) */
        uint32_t many = nu > 1u || trk_nvoice(t) > 1u, exact = caos || warp == WW_QUANT || (pt_ops & PT_TIME_OPS);   /* (the white punch-ins move t: the formula itself) */   /* (QUANT's steps land on the edges) */
        uint32_t q0 = PROF_T();
        uint32_t rb = pb && !exact && (fb == 0u || fb == 8u) ? wt_brow(tr) : NTRK;   /* (B's table: SINE, STEP
                                                         * alone, under -60 dB of the formula's own; the rest exact) */
        ta = exact ? 0 : wt_get(tr, tr, 0, pa, fa, (uint32_t)pos, many, &ea, wk);
        tb = rb < NTRK ? wt_get(tr, rb, 1, pb, fb, (uint32_t)pos, many, &eb, wk) : 0;   /* (B: a borrowed row, wt_brow) */
        PROF_ADD(PQ_WTAB, q0);
    }
    {
        uint32_t posa[WAVE_CH];                       /* POS per sample (CAOS moves it): the formula reads A a block,
                                                      * so a CAOS block splits in runs of FVM_BLK at its first value */
        for (i = 0; i < n; i++)
            posa[i] = (uint32_t)acc[i], acc[i] = 0;
        for (u = 0; u < nu; u++) {
            uint32_t ph = v->ph[u], ui = u == 0u ? inc : u == 1u ? inc + (inc >> 16) * det * 3u : inc - (inc >> 16) * det * 3u;
            uint32_t o;
            if (warp == WW_OFF || warp == WW_FM) {       /* (BYTE 0.16, CPU) no warp: the phase as it is */
                for (i = 0; i < n; i++, ph += ui)
                    W[i] = ph >> 16;
            } else if (warp == WW_BEND || warp == WW_MIRROR) {   /* (the two sides' slopes once a block: no division
                                                         * a sample; Q16 reciprocals, a 64-bit product) */
                uint32_t mb = warp == WW_BEND ? 32768u - amt * 29491u / 127u : 65535u - amt * 32767u / 127u;
                uint32_t top = warp == WW_BEND ? 32768u : 65535u;
                uint32_t r0 = (top << 16) / mb, r1 = (top << 16) / (65536u - mb);   /* (32-bit: no 64-bit divide on the FM-1) */
                for (i = 0; i < n; i++, ph += ui) {
                    uint32_t w = ph >> 16;
                    W[i] = (w < mb ? (uint32_t)(((uint64_t)w * r0) >> 16)
                            : warp == WW_BEND ? 32768u + (uint32_t)(((uint64_t)(w - mb) * r1) >> 16)
                            : (uint32_t)(((uint64_t)(65535u - w) * r1) >> 16)) & 0xFFFFu;
                }
            } else {
                for (i = 0; i < n; i++) {
                    W[i] = (wave_warp(ph >> 16, warp, amt)) & 0xFFFFu;
                    ph += ui;
                }
            }
            for (o = 0; o < n; o += FVM_BLK) {
                uint32_t c = n - o < FVM_BLK ? n - o : FVM_BLK, k;
                ea.arr[FA_T] = eb.arr[FA_T] = T + o;
                ea.arr[FA_W] = eb.arr[FA_W] = W + o;
                ea.u[FU_AF] = eb.u[FU_AF] = posa[o];
                uint32_t q1 = PROF_T();
                if (pb) {
                    if (tb) { wt_read(tb, W + o, c, yb + o); if (!u) fl_post(pb, &eb, &wk->s, c, yb + o); }
                    else (u ? fvm_run : formula_run)(pb, &eb, &wk->s, c, yb + o);   /* (byte-cpu-light: LIFE on the
                                                     * first unison voice; the detuned two plain) */
                    if (warp == WW_FM)                   /* FM: B into A's phase, AMT the index (up to a turn) */
                        for (k = 0; k < c; k++)
                            W[o + k] = (W[o + k] + (uint32_t)(((int32_t)yb[o + k] * (int32_t)amt) >> 7)) & 0xFFFFu;
                }
                if (ta) {
                    wave_fm_read(ta, pa, &ea, &wk->s, c, ya + o, !u);   /* (BYTE 0.16) LIFE LINK's cross-FM on the
                                                     * table's read too, out of line: its copies off this frame */
                }
                else (u ? fvm_run : formula_run)(pa, &ea, &wk->s, c, ya + o);
                PROF_ADD(ta || tb ? PQ_WRD : PQ_WVM, q1);
            }
            if (!mix)                                    /* (Q16: A .. B; byte-cpu-light: the choice a block, and */
                for (i = 0; i < n; i++)                  /* (d x MIX) >> 7 in 32 bits, exactly the 64-bit product's) */
                    acc[i] += (int32_t)ya[i];
            else
                for (i = 0; i < n; i++) {
                    uint32_t d = (pb ? yb[i] : 0u) - ya[i];
                    acc[i] += (int32_t)(ya[i] + (uint32_t)((int32_t)d >> 7) * mix + ((d & 127u) * mix >> 7));
                }
            v->ph[u] = ph;
        }
    }
    for (i = 0; i < n; i++) {
        int32_t s = nu == 1u ? acc[i] >> 1 : (int32_t)(((int64_t)acc[i] * 21845) >> 16);   /* (Q16 -> Q15, the unison's
                                                      * mean: a third and a half as a product, not a division a sample) */
        if (sub) {
            uint32_t sp = (uint32_t)v->s[3];
            s += sine_i(sp) * (int32_t)sub >> 8;
            v->s[3] = (int32_t)(sp + (inc >> 1));
        }
        out[i] += voice_amp(soft_knee(s >> 1, 16000) << 1, m, i + o0);
    }
    v->s[2] = (int32_t)((uint32_t)v->s[2] + n);
}

static const preset_t WAVE_PRESETS[] = {
    /* name, {A, POS, WARP, AMT, B, MIX, DETUNE, SUB}, {ATK DEC SUS REL}, ... (MATH's presets' layout) */
    {"INIT WAVE", {0, 0, 0, 0, 0, 0, 0, 0}, {0, 64, 100, 30}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
    {"FM KEYS", {1, 40, 0, 0, 0, 0, 0, 0}, {0, 70, 60, 40}, 0, TAG(KEYS), FX(0, 15, 25, 30), PAT(6),
     MODS(MENV2(0, 60, 0, 30), MMOD(1, PS_ENV2, PD_SHP, 30))},          /* POS opens with the note, closes */
    {"SAW PAD", {4, 20, 0, 0, 3, 50, 40, 0}, {80, 90, 100, 95}, 0, TAG(PAD), FX(0, 40, 30, 60), PAT(5),
     MODS(P_LRATE, 12, MMOD(1, PS_LFO, PD_SHP, 20))},                  /* three voices, POS breathing */
    {"SYNC LEAD", {3, 30, 1, 60, 0, 0, 12, 0}, {2, 70, 100, 40}, 0, 1 | TAG(LEAD), FX(0, 0, 35, 25), PAT(4),
     MODS(MMOD(1, PS_MODW, PD_SHP, 40), P_GLIDE, 10)},
    {"SUB BASS", {7, 30, 2, 40, 0, 0, 0, 80}, {0, 60, 90, 20}, 0, 1 | TAG(BASS), FX(0, 0, 5, 0), PAT(2),
     MODS(P_GLIDE, 8)},
    {"BIT TABLE", {11, 64, 4, 50, 13, 30, 0, 0}, {0, 75, 80, 40}, 0, 1 | TAG(FX), FX(0, 10, 30, 30), PAT(3),
     MODS(P_LWAVE, LW_SH, P_LRATE, -8, MMOD(1, PS_LFO, PD_SHP, 40))},  /* POS jumps each 1/8 */
    {"FM BELL", {0, 0, 5, 70, 2, 0, 0, 0}, {0, 88, 0, 70}, 12, TAG(BELL), FX(0, 20, 35, 55), PAT(7),
     MODS(MMOD(1, PS_VEL, PD_SHP, 30))},                               /* B into A's phase */
    {"FORMANT", {12, 50, 0, 0, 8, 40, 20, 0}, {30, 80, 90, 60}, 0, TAG(PAD), FX(0, 30, 30, 50), PAT(5),
     MODS(P_LRATE, 6, MMOD(1, PS_LFO, PD_SHP, 35))},
    /* (wavepresets) appended: a stored number names the same sound. Each moves: ENV 2 (per voice on PD_SHP, POS; the
     * latest note's on PD_E: AMT MIX) or the LFO; B a table (SINE ODD) where it plays with DETUNE (CPU) */
    {"REESE", {4, 8, 0, 0, 0, 0, 90, 50}, {0, 70, 110, 25}, 0, 1 | TAG(BASS), FX(10, 0, 0, 10), PAT(2),
     MODS(P_GLIDE, 8, P_FCUT, 76, P_FRES, 30, MLFO(LW_TRI, 20, 0, 14), MMOD(1, PS_LFO, PD_SHP, 16))},   /* the filter, POS drift */
    {"ACID BASS", {3, 6, 0, 0, 0, 0, 0, 30}, {0, 60, 100, 15}, 0, 1 | TAG(BASS), FX(25, 0, 20, 0), PAT(2),
     MODS(P_GLIDE, 12, P_FCUT, 46, P_FRES, 100, MENV2(0, 76, 0, 20), MMOD(1, PS_ENV2, PD_SHP, 50),
          MMOD(2, PS_ENV2, PD_FCUT, 40))},                                 /* ENV 2: the sync and the filter snap */
    {"FM GROWL", {5, 27, 5, 30, 2, 0, 0, 40}, {0, 70, 110, 20}, 0, 1 | TAG(BASS), FX(15, 0, 0, 0), PAT(2),
     MODS(P_GLIDE, 6, MENV2(0, 75, 30, 30), MMOD(1, PS_ENV2, PD_E(3), 40), MLFO(LW_TRI, -5, 0, 0),
          MMOD(2, PS_LFO, PD_E(3), 16), MMOD(3, PS_LFO, PD_SHP, 8))},     /* FM's index: struck, then a 1/4 growl */
    {"808 BOOM", {5, 6, 0, 0, 0, 0, 0, 30}, {0, 100, 0, 50}, 0, 1 | TAG(BASS), FX(5, 0, 0, 5), PAT(2),
     MODS(MENV2(0, 68, 0, 10), MMOD(1, PS_ENV2, PD_PITCH, 36), MMOD(2, PS_ENV2, PD_SHP, 40))},   /* the drop, the click */
    {"WOBBLE", {7, 40, 0, 0, 0, 0, 0, 60}, {0, 70, 110, 20}, 0, 1 | TAG(BASS), FX(20, 0, 0, 0), PAT(2),
     MODS(P_FCUT, 96, MLFO(LW_SIN, -8, 0, 20), MMOD(1, PS_LFO, PD_SHP, 40))},   /* the fold in 1/8, from each note */
    {"SWEEP PAD", {7, 0, 0, 0, 0, 0, 40, 0}, {90, 90, 110, 100}, 0, TAG(PAD), FX(0, 30, 30, 70), PAT(5),
     MODS(MENV2(115, 0, 127, 105), MMOD(1, PS_ENV2, PD_SHP, 63), P_LRATE, 10, MMOD(2, PS_LFO, PD_SHP, 14))},   /* ~4 s up */
    {"MORPH PAD", {4, 30, 0, 0, 8, 64, 50, 0}, {70, 90, 110, 95}, 0, TAG(PAD), FX(0, 35, 25, 60), PAT(5),
     MODS(MLFO(LW_TRI, -2, 0, 0), P_LPHASE, -1, MMOD(1, PS_LFO, PD_E(5), 30), MMOD(2, PS_LFO, PD_SHP, -20))},   /* A .. B, 2 bars */
    {"GLASS PAD", {2, 62, 0, 0, 0, 40, 20, 0}, {40, 90, 100, 90}, 0, TAG(PAD), FX(0, 40, 35, 75), PAT(5),
     MODS(P_LRATE, 30, MMOD(1, PS_LFO, PD_E(5), 30), MMOD(2, PS_VEL, PD_SHP, 8))},   /* the 4:1 glass, a sine in and out */
    {"PWM STRNG", {9, 80, 0, 0, 0, 0, 0, 0}, {50, 90, 110, 80}, 0, TAG(PAD), FX(0, 60, 20, 60), PAT(5),
     MODS(P_LRATE, 45, MMOD(1, PS_LFO, PD_SHP, 36))},                      /* the width swung */
    {"SUPERSAW", {4, 0, 0, 0, 0, 0, 127, 20}, {20, 90, 110, 70}, 0, TAG(PAD), FX(0, 60, 30, 55), PAT(5),
     MODS(MENV2(60, 100, 40, 70), MMOD(1, PS_ENV2, PD_SHP, 40))},          /* the saws bloom square, settle */
    {"SHIMMER", {14, 40, 3, 20, 0, 0, 30, 0}, {80, 100, 110, 110}, 0, TAG(PAD), FX(0, 50, 60, 100), PAT(5),
     MODS(MENV2(110, 0, 127, 110), MMOD(1, PS_ENV2, PD_SHP, 50), P_LRATE, 30, MMOD(2, PS_LFO, PD_E(3), 20))},   /* the fold's point */
    {"DARK DRONE", {8, 30, 0, 0, 0, 0, 60, 60}, {100, 100, 127, 110}, 0, TAG(PAD), FX(10, 20, 20, 80), PAT(5),
     MODS(P_FCUT, 60, P_FRES, 40, MLFO(LW_SIN, 4, 0, 16), MMOD(1, PS_LFO, PD_SHP, 30))},   /* the odd partials, slowly */
    {"FM PLUCK", {1, 0, 0, 0, 0, 0, 0, 0}, {0, 85, 0, 60}, 0, TAG(PLUCK), FX(0, 15, 40, 40), PAT(6),
     MODS(MENV2(0, 70, 0, 40), MMOD(1, PS_ENV2, PD_SHP, 63), MMOD(2, PS_VEL, PD_SHP, 20), P_ED_SHP, 40)},   /* the index struck */
    {"METAL PLK", {2, 90, 1, 0, 0, 0, 0, 0}, {0, 82, 0, 55}, 0, TAG(PLUCK), FX(0, 10, 45, 45), PAT(6),
     MODS(MENV2(0, 72, 0, 40), MMOD(1, PS_ENV2, PD_SHP, 30), MMOD(2, PS_ENV2, PD_E(3), 30))},   /* the ratio steps down */
    {"YAH LEAD", {12, 20, 0, 0, 8, 30, 10, 0}, {5, 80, 110, 40}, 0, 1 | TAG(VOX), FX(0, 10, 40, 35), PAT(4),
     MODS(P_GLIDE, 15, MENV2(65, 75, 30, 40), MMOD(1, PS_ENV2, PD_SHP, 45), P_LRATE, 88, P_LFADE, 70, P_LD_PIT, 1)},   /* "ya" */
    {"CHIME", {6, 100, 0, 0, 0, 120, 0, 0}, {0, 100, 0, 95}, 0, TAG(BELL), FX(0, 20, 40, 65), PAT(7),
     MODS(MENV2(0, 80, 0, 60), MMOD(1, PS_ENV2, PD_E(5), -50))},          /* the 5th and 7th die first */
    {"NOISE RISE", {13, 0, 0, 0, 7, 50, 0, 0}, {60, 90, 127, 90}, 0, 1 | TAG(FX), FX(0, 20, 50, 70), PAT(3),
     MODS(MENV2(112, 0, 127, 90), MMOD(1, PS_ENV2, PD_SHP, 63), MMOD(2, PS_ENV2, PD_PITCH, 40))},   /* finer, up */
};
/* browsing: KEYS PLUCK, PAD, LEAD VOX, BASS, BELL, FX, INIT */
static const uint8_t WAVE_ORDER[] = {1, 20, 21, 2, 13, 14, 15, 16, 17, 18, 19, 7, 3, 22, 4, 8, 9, 10, 11, 12, 6, 23, 5, 24, 0};
_Static_assert(sizeof WAVE_ORDER == NELEM(WAVE_PRESETS), "WAVE's browsing order: every preset once");

static void wave_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t q0 = PROF_T();
    wave_render_x(t, v, out, n, m);
    PROF_ADD(PQ_WALL, q0);
}
static void wave_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    for (uint32_t o = 0; o < n; o += WAVE_CH)
        wave_part(t, v, out + o, n - o < WAVE_CH ? n - o : WAVE_CH, m, o);
}
static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"WAVE", "OSC B"},
    .edit = {
        {"A", F_ENUM, 0, WAVE_NF - 1u, 0, N_WAVEF},
        {"POS", F_PCT, 0, 127, 0, 0},
        {"WARP", F_ENUM, 0, WW_FM, 0, N_WARP},
        {"AMT", F_PCT, 0, 127, 0, 0},
        {"B", F_ENUM, 0, WAVE_NF - 1u, 0, N_WAVEF},
        {"MIX", F_PCT, 0, 127, 0, 0},
        {"DETUNE", F_PCT, 0, 127, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0},
    },
    .presets = WAVE_PRESETS,
    .npresets = NELEM(WAVE_PRESETS),
    .note_on = wave_note_on,
    .render = wave_render,
    .block = wave_block,                             /* (BYTE, CPU) a table's quarter a block */
    .half = 1,                                       /* (byte-cpu-light) voice_half's generic path: f(w, A) at the doubled increment */
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
