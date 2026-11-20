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
static int16_t wt_tab[NTRK][1][WT_N + 1u];          /* (A's only: 1.5 KB of RAM; the pool keeps its 8 KB of headroom) */
static uint16_t wt_key[NTRK][1];                    /* 1 + formula x 128 + POS (0: none) */
static int wt_smooth(uint32_t f) { return f != 10u && f != 11u && f != 13u; }   /* (SAWSQ PULSE too: their edge a 1/128 turn ramp) */
static const int16_t *wt_get(uint32_t tr, uint32_t o, const fvm_prog_t *pg, uint32_t f, uint32_t pos, uint32_t many,
                             const fvm_env_t *e0, lofi_work_t *wk)
{
    uint16_t key = (uint16_t)(1u + f * 128u + pos);
    uint32_t i, k, W[FVM_BLK], T[FVM_BLK] = {0}, y[FVM_BLK];
    if (!pg->n || pg->nstate || (pg->uses & ~(1u << FA_W)) || !wt_smooth(f))
        return 0;
    if (!many)                                          /* (one voice, no unison: the formula as written) */
        return 0;
    if (wt_key[tr][o] == key)
        return wt_tab[tr][o];
    {
        fvm_env_t e = *e0;
        e.arr[FA_T] = T; e.arr[FA_W] = W;
        e.u[FU_AF] = pos * 516u;
        for (k = 0; k < WT_N; k += FVM_BLK) {
            for (i = 0; i < FVM_BLK; i++) W[i] = (k + i) * (65536u / WT_N);
            fvm_run(pg, &e, &wk->s, FVM_BLK, y);
            for (i = 0; i < FVM_BLK; i++) wt_tab[tr][o][k + i] = (int16_t)clamp((int32_t)y[i] >> 1, -32768, 32767);
        }
    }
    wt_tab[tr][o][WT_N] = wt_tab[tr][o][0];
    wt_key[tr][o] = key;
    return wt_tab[tr][o];
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

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
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
    uint32_t W[LOFI_N], T[LOFI_N], ya[LOFI_N], yb[LOFI_N];
    int32_t acc[LOFI_N];
    fvm_env_t ea = {{T, W, 0, 0, 0}, {0}, &v->s[6]}, eb = {{T, W, 0, 0, 0}, {0}, &v->s[7]};
    if (n > LOFI_N)
        n = LOFI_N;
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
        ta = exact ? 0 : wt_get(tr, 0, pa, fa, (uint32_t)pos, many, &ea, wk);
        tb = 0;                                      /* (B exact: its table would need the pool's headroom) */
    }
    {
        uint32_t posa[LOFI_N];                       /* POS per sample (CAOS moves it): the formula reads A a block,
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
                if (pb) {
                    if (tb) { wt_read(tb, W + o, c, yb + o); if (!u) fl_post(pb, &eb, &wk->s, c, yb + o); }
                    else (u ? fvm_run : formula_run)(pb, &eb, &wk->s, c, yb + o);   /* (byte-cpu-light: LIFE on the
                                                     * first unison voice; the detuned two plain) */
                    if (warp == WW_FM)                   /* FM: B into A's phase, AMT the index (up to a turn) */
                        for (k = 0; k < c; k++)
                            W[o + k] = (W[o + k] + (uint32_t)(((int32_t)yb[o + k] * (int32_t)amt) >> 7)) & 0xFFFFu;
                }
                if (ta) { wt_read(ta, W + o, c, ya + o); if (!u) fl_post(pa, &ea, &wk->s, c, ya + o); }
                else (u ? fvm_run : formula_run)(pa, &ea, &wk->s, c, ya + o);
            }
            for (i = 0; i < n; i++) {
                int32_t a = (int32_t)ya[i], b = pb ? (int32_t)yb[i] : 0;
                acc[i] += mix ? a + (int32_t)(((int64_t)(b - a) * (int32_t)mix) >> 7) : a;   /* (Q16: A .. B) */
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
        out[i] += voice_amp(soft_knee(s >> 1, 16000) << 1, m, i);
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
};
static const uint8_t WAVE_ORDER[] = {1, 2, 3, 4, 6, 7, 5, 0};
_Static_assert(sizeof WAVE_ORDER == NELEM(WAVE_PRESETS), "WAVE's browsing order: every preset once");

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
    .half = 1,                                       /* (byte-cpu-light) voice_half's generic path: f(w, A) at the doubled increment */
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
