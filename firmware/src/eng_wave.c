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
#ifndef WF_IDEAL
#define WF_IDEAL 0                                   /* (tests: every voice the formula as written) */
#endif
#ifndef WF_NOFRAMES
#define WF_NOFRAMES 0                                /* (tests: the one table at POS, as before frames) */
#endif
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
/* (waveframes) and the spectral warps, drawn into the frames (wf_spec: nothing a sample): SMEAR each harmonic's level
 * smeared into the ones above it, SKEW the harmonics pulled up (the spectrum stretched, its fundamental kept), SHEPARD the
 * eight frames an octave's climb (POS round and round: endless), DISPERSE each harmonic delayed by its square (a chirp a
 * cycle, more up POS); AMT their depth (SHEPARD: where in A's formula) */
static const char *const N_WARP[] = {"OFF", "SYNC", "BEND", "MIRROR", "QUANT", "FM", "SMEAR", "SKEW", "SHEPRD", "DISPRS", 0};
enum { WW_OFF, WW_SYNC, WW_BEND, WW_MIRROR, WW_QUANT, WW_FM, WW_SMEAR, WW_SKEW, WW_SHEP, WW_DISP };

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

/* (waveframes) a voice's phases into W, bent by WARP (BEND MIRROR: their slopes once a block, no division a sample);
 * the phase after them */
typedef struct { uint32_t warp, amt, mb, r0, r1, sm; } wph_t;
static void wph_init(wph_t *q, uint32_t warp, uint32_t amt)
{
    q->warp = warp, q->amt = amt, q->mb = 0, q->sm = warp == WW_SYNC ? 16u + amt * 112u / 127u : 0u;   /* (SYNC: x1 .. x8) */
    if (warp == WW_BEND || warp == WW_MIRROR) {
        uint32_t top = warp == WW_BEND ? 32768u : 65535u;
        q->mb = warp == WW_BEND ? 32768u - amt * 29491u / 127u : 65535u - amt * 32767u / 127u;
        q->r0 = (top << 16) / q->mb, q->r1 = (top << 16) / (65536u - q->mb);   /* (32-bit: no 64-bit divide on the FM-1) */
    }
}
static __attribute__((noinline)) uint32_t wave_phw(const wph_t *q, uint32_t *W, uint32_t n, uint32_t ph, uint32_t iu)
{
    uint32_t i, mb = q->mb;
    for (i = 0; i < n; i++, ph += iu) {
        uint32_t w = ph >> 16;
        if (mb)
            w = w < mb ? (uint32_t)(((uint64_t)w * q->r0) >> 16) : q->warp == WW_BEND ? 32768u + (uint32_t)(((uint64_t)(w - mb) * q->r1) >> 16)
                : (uint32_t)(((uint64_t)(65535u - w) * q->r1) >> 16);
        else if (q->sm)
            w = w * q->sm >> 4;
        else if (q->warp == WW_QUANT)
            w = wave_warp(w, WW_QUANT, q->amt);
        W[i] = w & 0xFFFFu;
    }
    return ph;
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

/* (waveframes, the owner: "a fatter, deeply modulatable wavetable with its own character") FRAMES: A drawn at eight
 * positions of POS, 128 points each (int16, as wt_tab), read a sample at a time between two points and two frames
 * (bilinear): POS moved by anything (the LFO, ENV 2, the matrix, CLAY, WSEQ) glides at the audio's rate, nothing redrawn.
 * Memory: none of its own: the part's COMB line (voice.c comb_buf, 1024 samples = 8 x 128), free while the part's COMB,
 * METAL and HIVE do not play it (wf_line_busy; HIVE's clear and COMB's entry clear the frames' key). The grid: a
 * formula straight in A (SINE SAWSQ ODD SINTR) two frames at POS 0 and 127, exact everywhere; the others eight across
 * how far POS can move this part (wf_span: the LFO, ENV, the matrix's depths, the knob one of them: a sound at rest is
 * the old table point for point), SHEPARD eight round the octave. B (MIX, FM) two frames in 6 .. 7 when SAWSQ or SINTR
 * (then A six), else as it was (SINE ODD a borrowed row, the others the formula). Drawn again only when A, B, the spectral warp or the grid
 * change, a step a block (fvm_warm_tok: one part's a block, 32 points or one spectral pass): meanwhile the table at POS
 * (wt_get) as before. STEP (FRAMES): no glide between frames, the nearest one (a PPG's grain) */
#define WF_N 128u
#define WF_LIN(f) ((f) == 0u || (f) == 4u || (f) == 8u || (f) == 14u)   /* (A in them: (1 - A) x + A y) */
static int16_t comb_buf[NTRK][1024] __attribute__((section(".pool")));   /* (voice.c's, COMB_LEN) */
typedef struct {
    uint32_t key;                                    /* A B, the layout, the spectral warp and AMT drawn (0: none) */
    int32_t g0, gs, rq;                              /* A's grid: frame 0's POS, the step (Q8; 0: one frame), 1 / step */
    int32_t lane;                                    /* WSEQ's POS this block (Q8) */
    uint8_t na, nb, ok, use;                         /* A's frames (0 ..), B's (6 ..); drawn whole; read this block */
    uint8_t bf, bc, mx0, mx1;                        /* the frame being drawn, its step; MIX at the block's start, end */
} wf_t;
static wf_t wf[NTRK] __attribute__((section(".pool")));
static uint16_t wf_vpos[NTRK * NVOICE] __attribute__((section(".pool")));   /* a voice's POS at its last sample, Q8 + 1 */
static int wf_line_busy(uint32_t tr);                /* (voice.c, after the COMB and the matrix) */
static int wf_span(const track_t *t, int32_t base, uint32_t full, int32_t *lo, int32_t *hi);
static int32_t wf_lane(track_t *t);
static int32_t *wf_side(uint32_t r, uint32_t on);
static uint32_t wf_frames(const int16_t *p) { return (uint32_t)clamp(p[P_KEY] - 1, 0, 3); }   /* bit 0 STEP, 1 WSEQ (P_KEY:
                                                         * a GEN texture's 1 there, SMOOTH as 0) */
static int wf_tabl(const fvm_prog_t *pg) { return pg->n && !pg->nstate && !(pg->uses & ~(1u << FA_W)); }

/* 128 points' complex FFT (in place, int32; inv: the inverse, halved each pass: / 128) */
static __attribute__((noinline)) void wf_fft(int32_t *re, int32_t *im, uint32_t inv)
{
    uint32_t i, j = 0, k, m, s;
    for (i = 1; i < WF_N; i++) {
        uint32_t b = WF_N >> 1;
        for (; j & b; b >>= 1) j ^= b;
        j |= b;
        if (i < j) { int32_t x = re[i]; re[i] = re[j]; re[j] = x; x = im[i]; im[i] = im[j]; im[j] = x; }
    }
    for (s = 0; s < 7u; s++)
        for (m = 0; m < (1u << s); m++) {
            int32_t c = sine_i((m << (31u - s)) + 0x40000000u), sn = sine_i(m << (31u - s));
            if (!inv) sn = -sn;
            for (k = m; k < WF_N; k += 2u << s) {
                int32_t *ar = &re[k], *ai = &im[k], *br = &re[k + (1u << s)], *bi = &im[k + (1u << s)];
                int32_t tr = (int32_t)(((int64_t)*br * c - (int64_t)*bi * sn) >> 15);
                int32_t ti = (int32_t)(((int64_t)*bi * c + (int64_t)*br * sn) >> 15), xr = *ar, xi = *ai;
                if (inv) { xr >>= 1; xi >>= 1; tr >>= 1; ti >>= 1; }
                *ar = xr + tr; *ai = xi + ti; *br = xr - tr; *bi = xi - ti;
            }
        }
}
/* the harmonics 1 .. 63's power */
static int64_t wf_pow(const int32_t *re, const int32_t *im)
{
    int64_t e = 0;
    uint32_t h;
    for (h = 1; h < 64u; h++) e += (int64_t)re[h] * re[h] + (int64_t)im[h] * im[h];
    return e;
}
/* a frame through a spectral warp: src's harmonics moved (kind, AMT amt; DISPERSE: fp the frame's POS; SHEPARD: j its
 * place round the octave), their power kept, into dst (src may be dst) */
static __attribute__((noinline)) void wf_spec(const int16_t *src, int16_t *dst, uint32_t kind, uint32_t amt, int32_t fp,
                                              uint32_t j)
{
    int32_t re[WF_N], im[WF_N], g;
    int64_t e0, e1;
    uint32_t i, h, n;
    for (i = 0; i < WF_N; i++) { re[i] = src[i] * 16; im[i] = 0; }
    wf_fft(re, im, 0);
    e0 = wf_pow(re, im);
    if (kind == WW_SMEAR) {                          /* each level into the next: S = M + amt (S' - M), the phases kept */
        int32_t mx = 1, sh = 0, sm = 0, ur = 0, ui = -16384;
        for (h = 1; h < 64u; h++) mx |= (re[h] < 0 ? -re[h] : re[h]) | (im[h] < 0 ? -im[h] : im[h]);
        while (mx >> sh > 32767) sh++;
        for (h = 1; h < 64u; h++) {
            int32_t r = re[h] >> sh, q = im[h] >> sh, a = (int32_t)isqrt32((uint32_t)(r * r) + (uint32_t)(q * q));
            if (a > 1) { ur = (r << 14) / a; ui = (q << 14) / a; }   /* (none there: the one below's phase) */
            sm = a + (((sm - a) * (int32_t)amt) >> 7);
            re[h] = (sm * ur >> 14) << sh;
            im[h] = (sm * ui >> 14) << sh;
        }
    } else if (kind == WW_SKEW) {                    /* harmonic h from h x 64 / (64 + AMT), between two (from the top) */
        for (h = 63; h >= 2u; h--) {
            uint32_t q = (h << 14) / (64u + amt), k = q >> 8, f = q & 255u;
            int32_t r0 = k ? re[k] : 0, i0 = k ? im[k] : 0;
            re[h] = r0 + (int32_t)(((int64_t)(re[k + 1u] - r0) * (int32_t)f) >> 8);
            im[h] = i0 + (int32_t)(((int64_t)(im[k + 1u] - i0) * (int32_t)f) >> 8);
        }
    } else if (kind == WW_DISP) {                    /* harmonic h turned by AMT (h - 1)^2 (more up POS): its delay h's */
        for (h = 2; h < 64u; h++) {
            uint32_t ph = amt * (h - 1u) * (h - 1u) * (uint32_t)(128 + (fp >> 8)) << 13;
            int32_t c = sine_i(ph + 0x40000000u), sn = sine_i(ph), r = re[h];
            re[h] = (int32_t)(((int64_t)r * c - (int64_t)im[h] * sn) >> 15);
            im[h] = (int32_t)(((int64_t)im[h] * c + (int64_t)r * sn) >> 15);
        }
    } else {                                         /* SHEPARD: six octaves of the wave, a bell over them moved up an
                                                      * octave by the eight frames (sin^2: each octave in and out) */
        int32_t w[6];
        for (n = 0; n < 6u; n++) { int32_t x = sine_i(((n + 1u) * 8u - j) * 44739243u); w[n] = x * x >> 15; }
        for (h = 63; h >= 1u; h--) {
            int64_t r = 0, q = 0;
            for (n = 0; n < 6u && !(h & ((1u << n) - 1u)); n++) { r += (int64_t)re[h >> n] * w[n]; q += (int64_t)im[h >> n] * w[n]; }
            re[h] = (int32_t)(r >> 15);
            im[h] = (int32_t)(q >> 15);
        }
    }
    e1 = wf_pow(re, im);                             /* the power as it was (sqrt(e0 / e1), Q12, at most x8) */
    for (; e1 >= 65536 || e0 >= (1 << 22); e0 >>= 1, e1 >>= 1) {}
    g = e1 <= 0 ? 4096 : e0 >= e1 << 6 ? 8 << 12 : (int32_t)isqrt32((((uint32_t)e0 << 10) / (uint32_t)e1) << 14);
    for (h = 1; h < 64u; h++) {
        re[h] = (int32_t)(((int64_t)re[h] * g) >> 12);
        im[h] = (int32_t)(((int64_t)im[h] * g) >> 12);
        re[WF_N - h] = re[h];
        im[WF_N - h] = -im[h];
    }
    re[64] = im[64] = 0;
    wf_fft(re, im, 1);
    for (i = 0; i < WF_N; i++) dst[i] = (int16_t)clamp(re[i] >> 4, -32768, 32767);
}
/* A's program compiled, else B's (b: wanted): 1 when one was (a step) */
static int wf_comp(uint32_t tr, uint32_t fa, uint32_t fb, uint32_t b)
{
    lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
    if (lofi_prg_id[tr] != 0x100u + fa) { (void)wave_prog(lofi_slot(tr), &lofi_prg_id[tr], fa, wk); return 1; }
    if (b && wave_id_b[tr] != 0x100u + fb) {
        if (!wave_prg_b[tr].ins)
            fvm_prog_init(&wave_prg_b[tr], wave_ins_b[tr], 32u);
        (void)wave_prog(&wave_prg_b[tr], &wave_id_b[tr], fb, wk);
        return 1;
    }
    return 0;
}
/* a step of the frames' drawing: 32 points of a frame (A's at its POS, B's at 0 and 127, SHEPARD's base at AMT), or a
 * frame through its spectral warp; the programs compiled first (a step each) */
static void wf_build(uint32_t tr, wf_t *f, uint32_t kind, uint32_t amt, lofi_work_t *wk)
{
    int16_t *F = comb_buf[tr];
    uint32_t sh = kind == WW_SHEP, bf = f->bf, fa = f->key & 15u, fb = f->key >> 4 & 15u, slot, i;
    int32_t pq;
    if (wf_comp(tr, fa, fb, f->nb))
        return;
    if (bf >= (uint32_t)f->na + f->nb + sh) { f->ok = 1; return; }
    if (sh && bf) {                                  /* (frames 7 .. 0 from the base in 0, drawn last) */
        wf_spec(F, F + (8u - bf) * WF_N, kind, amt, 0, 8u - bf);
        f->bf++;
        return;
    }
    slot = bf < f->na ? bf : 6u + bf - f->na;
    pq = sh ? (int32_t)amt << 8 : bf < f->na ? f->g0 + (int32_t)bf * f->gs : bf > f->na ? 127 << 8 : 0;
    if (f->bc < 4u) {
        int32_t st[FVM_MAXSTATE] = {0};
        uint32_t T[FVM_BLK] = {0}, W[FVM_BLK], y[FVM_BLK], k = f->bc * FVM_BLK;
        fvm_env_t e = {{T, W, 0, 0, 0}, {0}, st};
        e.u[FU_AF] = (uint32_t)clamp(pq, 0, 127 << 8) * 516u >> 8;   /* (POS's FIX, as wt_get's) */
        for (i = 0; i < FVM_BLK; i++) W[i] = (k + i) * (65536u / WF_N);
        fvm_run(bf < f->na ? lofi_slot(tr) : &wave_prg_b[tr], &e, &wk->s, FVM_BLK, y);
        for (i = 0; i < FVM_BLK; i++) F[slot * WF_N + k + i] = (int16_t)clamp((int32_t)y[i] >> 1, -32768, 32767);
        if (++f->bc < 4u || (kind && !sh && bf < f->na))
            return;
    } else
        wf_spec(F + slot * WF_N, F + slot * WF_N, kind, amt, pq, 0);
    f->bc = 0;
    f->bf++;
}
/* two frames blended once (POS still between them a run: one row read, not two), a core's */
static int16_t wf_row[2][WF_N] __attribute__((section(".pool")));
static const int16_t *wf_mix(const int16_t *F, int32_t fi)
{
    const int16_t *r = F + ((uint32_t)fi >> 12 & 7u) * WF_N, *q = F + ((((uint32_t)fi >> 12) + 1u) & 7u) * WF_N;
    int16_t *d = wf_row[MC_CPU() & 1u];
    uint32_t i;
    for (i = 0; i < WF_N; i++) d[i] = (int16_t)(r[i] + (((q[i] - r[i]) * (fi & 4095)) >> 12));
    return d;
}
/* frames read at phases W, the frame fi (Q12, frame 0 at F) dfi further a sample (STEP: the nearest frame) */
static void wf_read(const int16_t *F, const uint32_t *W, uint32_t n, int32_t fi, int32_t dfi, uint32_t stp, uint32_t *y)
{
    uint32_t i;
    if (stp) fi += 2048;
    if (!dfi && (stp || !(fi & 4095))) {             /* (one frame the whole run: wt_read's) */
        const int16_t *r = F + ((uint32_t)(stp || !(fi & 4095) ? fi : fi & ~4095) >> 12 & 7u) * WF_N;
        for (i = 0; i < n; i++) {                    /* (wt_read's, round: a frame has no 129th point) */
            uint32_t w = W[i] & 0xFFFFu, j = w >> 9;
            int32_t a = r[j];
            y[i] = (uint32_t)((a + (((r[(j + 1u) & (WF_N - 1u)] - a) * (int32_t)(w & 511u)) >> 9)) * 2);
        }
        return;
    }
    if (!stp && fi >> 12 == (fi + dfi * (int32_t)(n - 1u)) >> 12) {   /* (the same two frames the whole run) */
        const int16_t *r = F + ((uint32_t)fi >> 12 & 7u) * WF_N, *q = F + ((((uint32_t)fi >> 12) + 1u) & 7u) * WF_N;
        for (i = 0; i < n; i++, fi += dfi) {
            uint32_t w = W[i] & 0xFFFFu, j = w >> 9, j1 = (j + 1u) & (WF_N - 1u);
            int32_t x = (int32_t)(w & 511u), a = r[j] + (((r[j1] - r[j]) * x) >> 9), c = q[j] + (((q[j1] - q[j]) * x) >> 9);
            y[i] = (uint32_t)((a + (((c - a) * (fi & 4095)) >> 12)) * 2);
        }
        return;
    }
    for (i = 0; i < n; i++, fi += dfi) {
        uint32_t w = W[i] & 0xFFFFu, j = w >> 9, j1 = (j + 1u) & (WF_N - 1u), b = stp ? 0u : (uint32_t)fi & 4095u;
        const int16_t *r = F + ((uint32_t)fi >> 12 & 7u) * WF_N;
        int32_t x = (int32_t)(w & 511u), a = r[j] + (((r[j1] - r[j]) * x) >> 9);
        if (b) {
            const int16_t *q = F + ((((uint32_t)fi >> 12) + 1u) & 7u) * WF_N;
            int32_t c = q[j] + (((q[j1] - q[j]) * x) >> 9);
            a += ((c - a) * (int32_t)b) >> 12;
        }
        y[i] = (uint32_t)(a * 2);
    }
}

/* (waveframes) a block of the part, sounding or not: which frames it wants (A B, the warp, the grid), drawn a step at a
 * time when they are not those; MIX's ramp; the sides (WIDTH); then the warm-up as before */
static void wave_block(track_t *t)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, fa = (uint32_t)p[P_E0] % WAVE_NF, fb = (uint32_t)p[P_E4] % WAVE_NF;
    uint32_t warp = (uint32_t)clamp(p[P_E2], 0, WW_DISP), amt = (uint32_t)clamp(p[P_E3], 0, 127);
    uint32_t kind = warp >= WW_SMEAR ? warp : 0u, sh = warp == WW_SHEP, seq = wf_frames(p) >> 1, key = 0, na, nb = 0;
    wf_t *f = &wf[tr];
    int32_t base = clamp(p[P_E1], 0, 127) << 8, lo = 0, hi = 0, g0 = 0, gs = 0;
    wt_go[tr] = 3u;
    f->mx0 = f->mx1;
    f->mx1 = (uint8_t)clamp(p[P_E5], 0, 127);
    if (seq)
        base = f->lane = wf_lane(t);
    if (p[P_DXIMG] > 0)
        (void)wf_side(0, 1u);
    if (fa != 11u && fa != 13u && !p[P_CAOS] && warp != WW_QUANT && !wf_line_busy(tr) &&
        (lofi_prg_id[tr] != 0x100u + fa || wf_tabl(lofi_slot(tr)))) {   /* (BITS NOISE, CAOS QUANT: the formula's own) */
        uint32_t mm = wf_span(t, base, seq, &lo, &hi);
        if (!sh && (fb == 4u || fb == 14u) && (p[P_E5] || warp == WW_FM || mm))   /* (SINE ODD: the borrowed row, as before:
                                                         * A keeps its eight) */
            nb = 2u;
        if (sh) {
            na = 8u, gs = 16 << 8;
        } else if (!kind && WF_LIN(fa)) {
            na = fa ? 2u : 1u, gs = fa ? 127 << 8 : 0;
        } else if (hi > lo) {                        /* (on the knob: its frame the old table's) */
            na = nb ? 6u : 8u;
            gs = (hi - lo) / (int32_t)(na - 2u) + 1;
            g0 = base - (base - lo + gs - 1) / gs * gs;
        } else
            na = 1u, g0 = base;
        key = 0x80000000u | fa | fb << 4 | nb << 8 | kind << 10 | (kind ? amt : 0u) << 14 | na << 21;
        if (f->key == key && !sh && (kind || !WF_LIN(fa))) {   /* (the grid drawn still serves: no drawing when the
                                                         * knob moves a little; on the knob when nothing moves it) */
            int32_t a = f->g0, b = f->g0 + (f->na - 1) * f->gs;
            if (a <= lo && b >= hi && f->gs <= 2 * gs + 512 && (gs || (f->gs ? !((base - a) % f->gs) : base == a)))
                g0 = f->g0, gs = f->gs;
        }
        if (f->key != key || f->g0 != g0 || f->gs != gs) {
            f->key = key, f->g0 = g0, f->gs = gs, f->na = (uint8_t)na, f->nb = (uint8_t)nb;
            f->rq = gs ? (int32_t)((4096u << 16) / (uint32_t)gs) : 0;
            f->ok = f->bf = f->bc = 0;
        }
        if (!f->ok && fvm_warm_tok) {                /* (one step a block, one part's) */
            fvm_warm_tok = 0;
            wf_build(tr, f, kind, amt, &lofi_work[MC_CPU() & 1u]);
        }
    } else
        f->key = f->ok = 0;
    f->use = f->ok && !WF_NOFRAMES;
    if (fvm_warm_tok && trk_quiet(t) && wf_comp(tr, fa, fb, p[P_E5] > 0 || warp == WW_FM))   /* (engopt) the formulas
                                                         * compiled before the first note (no frames: BITS NOISE ..) */
        fvm_warm_tok = 0;
}

/* (waveframes) UNISON: VOICES (P_MORPH) AUTO (DETUNE 0: one, else three, as before) or 1 .. 7, CLASSIC, SHIMMER (octaves)
 * and CHORD (fifths, octaves): voice u's pitch ratio (Q12), its detune's side (Q8 of DETUNE's spread), its place across
 * (Q6 of WIDTH; odd ones right), its phase (u 3 .. 6 in the engine's s[]: ph[3] the voice's three) */
static const uint16_t WF_RAT[3][7] = {{4096, 4096, 4096, 4096, 4096, 4096, 4096}, {4096, 8192, 8192, 4096, 4096, 16384, 16384},
                                      {4096, 6144, 8192, 6144, 8192, 12288, 16384}};
static const int16_t WF_OFS[7] = {0, 256, -256, 128, -128, 192, -192};
static const uint8_t WF_PW[7] = {0, 64, 64, 32, 32, 48, 48}, WF_PH[4] = {0, 1, 4, 5};
static const char *const N_VOICES[] = {"AUTO", "1", "2", "3", "4", "5", "6", "7", "2 OCT", "3 OCT", "4 OCT", "5 OCT",
                                       "6 OCT", "7 OCT", "2 5TH", "3 5TH", "4 5TH", "5 5TH", "6 5TH", "7 5TH", 0};
static const char *const N_FRAMES[] = {"SMOOTH", "SMOOTH", "STEP", "SEQ", "SEQ ST", 0};
static const param_desc_t WAVE_XDESC[4] = {   /* (params.c track_desc: a WAVE track's VOICES WIDTH BLEND FRAMES) */
    {"VOICES", F_ENUM, 0, 19, 0, N_VOICES}, {"WIDTH", F_PCT, 0, 127, 0, 0}, {"BLEND", F_PCT, 0, 127, 0, 0},
    {"FRAMES", F_ENUM, 1, 4, 1, N_FRAMES}};
static void wave_note_on(track_t *t, voice_t *v)
{
    uint32_t sp = t->p[P_MORPH] > 0 ? 0x9E3779B9u : 0u;   /* (VOICES set: the voices' phases apart, no coherent start) */
    v->ph[0] = 0;
    v->ph[1] = sp;
    v->ph[2] = sp * 2u;
    v->s[0] = (int32_t)(sp * 3u);
    v->s[1] = (int32_t)(sp * 4u);
    v->s[4] = (int32_t)(sp * 5u);
    v->s[5] = (int32_t)(sp * 6u);
    v->s[2] = v->s[3] = 0;
    v->s[6] = v->s[7] = 0;
    wf_vpos[((uint32_t)(t - trk) % NTRK) * NVOICE + (uint32_t)(v - t->v) % NVOICE] = 0;
}

#define WAVE_CH FVM_BLK                               /* (BYTE 0.16) a part's samples: the block in two, its arrays half
                                                         * the stack (the audio's interrupt overflowed its 8 KB on the FM-1) */
static __attribute__((noinline)) void wave_part(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m, uint32_t o0)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, u;
    lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
    uint32_t fa = (uint32_t)p[P_E0] % WAVE_NF, fb = (uint32_t)p[P_E4] % WAVE_NF;
    uint32_t warp = (uint32_t)clamp(p[P_E2], 0, WW_DISP), amt = (uint32_t)clamp(p[P_E3], 0, 127);
    uint32_t mix = (uint32_t)clamp(p[P_E5], 0, 127), det = (uint32_t)clamp(p[P_E6], 0, 127);
    uint32_t sub = (uint32_t)clamp(p[P_E7], 0, 127), caos = (uint32_t)clamp(p[P_CAOS], 0, 127);
    int32_t pos = clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /* (the matrix's SHAPE moves POS) */
    uint32_t inc = pitch_inc(clamp(m->pitch16, 0, 2047)), need_b = mix || warp == WW_FM;
    uint32_t nu = p[P_MORPH] > 0 ? (p[P_MORPH] == 1 ? 1u : 3u) : det ? 3u : 1u;   /* (waveframes: the formula's three at most) */
    const fvm_prog_t *pa, *pb;
    wph_t wq;
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
    wph_init(&wq, warp, amt);
    {
        uint32_t posa[WAVE_CH];                       /* POS per sample (CAOS moves it): the formula reads A a block,
                                                      * so a CAOS block splits in runs of FVM_BLK at its first value */
        for (i = 0; i < n; i++)
            posa[i] = (uint32_t)acc[i], acc[i] = 0;
        for (u = 0; u < nu; u++) {
            uint32_t ph = v->ph[u], ui = u == 0u ? inc : u == 1u ? inc + (inc >> 16) * det * 3u : inc - (inc >> 16) * det * 3u;
            uint32_t o;
            ph = wave_phw(&wq, W, n, ph, ui);
            for (o = 0; o < n; o += FVM_BLK) {
                uint32_t c = n - o < FVM_BLK ? n - o : FVM_BLK, k;
                ea.arr[FA_T] = eb.arr[FA_T] = T + o;
                ea.arr[FA_W] = eb.arr[FA_W] = W + o;
                ea.u[FU_AF] = eb.u[FU_AF] = posa[o];
                uint32_t q1 = PROF_T();
                if (pb) {
                    (u ? fvm_run : formula_run)(pb, &eb, &wk->s, c, yb + o);   /* (byte-cpu-light: LIFE on the
                                                     * first unison voice; the detuned two plain) */
                    if (warp == WW_FM)                   /* FM: B into A's phase, AMT the index (up to a turn) */
                        for (k = 0; k < c; k++)
                            W[o + k] = (W[o + k] + (uint32_t)(((int32_t)yb[o + k] * (int32_t)amt) >> 7)) & 0xFFFFu;
                }
                (u ? fvm_run : formula_run)(pa, &ea, &wk->s, c, ya + o);   /* (waveframes: the formula as written;
                                                     * a table is wave_fpart's) */
                PROF_ADD(PQ_WVM, q1);
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
        int32_t s = nu == 1u ? acc[i] >> 1 : (int32_t)(((int64_t)acc[i] * 18919) >> 16);   /* (Q16 -> Q15; waveframes: three
                                                      * voices / sqrt 3 x 0.5, one's power, was / 3: 1.25 dB over one) */
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
     MODS(MENV2(60, 100, 40, 70), MMOD(1, PS_ENV2, PD_SHP, 40), P_MORPH, 7, P_DXIMG, 90)},   /* the saws bloom square,
                                                         * settle; (waveframes) seven of them, across */
    {"SHIMMER", {14, 40, 3, 20, 0, 0, 30, 0}, {80, 100, 110, 110}, 0, TAG(PAD), FX(0, 50, 60, 100), PAT(5),
     MODS(MENV2(110, 0, 127, 110), MMOD(1, PS_ENV2, PD_SHP, 50), P_LRATE, 30, MMOD(2, PS_LFO, PD_E(3), 20), P_MORPH, 11,
          P_DXIMG, 80)},                                                    /* the fold's point; (waveframes) octaves */
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
    /* (waveframes) the frames' own: SHEPARD round and round a bar (the LFO's saw on POS), the PPG's steps down an
     * envelope (FRAMES STEP), a wave sequence (FRAMES SEQ: the STEP MOD lane, a frame a sixteenth, glided), seven saws
     * wide */
    {"SHEPARD", {4, 64, 8, 10, 0, 0, 30, 0}, {90, 90, 110, 100}, 0, TAG(PAD), FX(0, 40, 30, 70), PAT(5),
     MODS(MLFO(LW_SAW, -3, 0, 0), P_LD_SHP, 63, P_MORPH, 5, P_DXIMG, 70)},
    {"PPG STEP", {2, 0, 0, 0, 0, 0, 0, 0}, {0, 80, 40, 50}, 0, TAG(KEYS), FX(0, 20, 35, 40), PAT(6),
     MODS(MENV2(0, 85, 0, 50), MMOD(1, PS_ENV2, PD_SHP, 63), P_KEY, 2)},
    {"WAVE SEQ", {1, 0, 0, 0, 0, 0, 12, 0}, {0, 80, 100, 40}, 0, TAG(KEYS), FX(0, 25, 40, 40), PAT(6),
     MODS(P_KEY, 3, P_MSLEN, 8, P_MSDIV, 2, P_MSSLW, 12, P_MS0, 0, P_MS0 + 1, 90, P_MS0 + 2, 36, P_MS0 + 3, 127,
          P_MS0 + 4, 18, P_MS0 + 5, 72, P_MS0 + 6, 108, P_MS0 + 7, 54)},
    {"HYPER SAW", {4, 0, 0, 0, 0, 0, 100, 0}, {10, 90, 110, 70}, 0, TAG(PAD), FX(0, 30, 30, 60), PAT(5),
     MODS(P_MORPH, 7, P_DXIMG, 127, P_LRTRG, 40)},
};
/* browsing: KEYS PLUCK, PAD, LEAD VOX, BASS, BELL, FX, INIT */
static const uint8_t WAVE_ORDER[] = {1, 20, 21, 26, 27, 2, 13, 14, 15, 16, 17, 18, 19, 25, 28, 7, 3, 22, 4, 8, 9, 10, 11, 12, 6,
                                    23, 5, 24, 0};
_Static_assert(sizeof WAVE_ORDER == NELEM(WAVE_PRESETS), "WAVE's browsing order: every preset once");

static void wave_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t q0 = PROF_T();
    wave_render_x(t, v, out, n, m);
    PROF_ADD(PQ_WALL, q0);
}
/* (waveframes) a voice's render through the frames (or the one table at POS, wt_get, while they are drawn): the ramps
 * across it of A's frame, B's, MIX (Q12 a frame, Q8 MIX; their start and a sample's step) */
typedef struct { int32_t fa, dfa, fb, dfb, mx, dmx; uint32_t pos; } wf_rp_t;
static __attribute__((noinline)) void wave_fpart(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m,
                                                 uint32_t o0, const int16_t *F, const wf_rp_t *rp)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, u, cpu = MC_CPU() & 1u;
    lofi_work_t *wk = &lofi_work[cpu];
    const wf_t *f = &wf[tr];
    uint32_t fa = (uint32_t)p[P_E0] % WAVE_NF, fb = (uint32_t)p[P_E4] % WAVE_NF;
    uint32_t warp = (uint32_t)clamp(p[P_E2], 0, WW_DISP), amt = (uint32_t)clamp(p[P_E3], 0, 127);
    uint32_t det = (uint32_t)clamp(p[P_E6], 0, 127), sub = (uint32_t)clamp(p[P_E7], 0, 127);
    uint32_t vc = (uint32_t)clamp(p[P_MORPH], 0, 19), nu = !vc ? (det ? 3u : 1u) : vc < 8u ? vc : vc < 14u ? vc - 6u : vc - 12u;
    uint32_t mode = vc < 8u ? 0u : vc < 14u ? 1u : 2u, stp = wf_frames(p) & 1u, hs = vh_sh_c[cpu] == 1u;
    uint32_t inc = pitch_inc(clamp(m->pitch16, 0, 2047)), fr = F == comb_buf[tr] || F == wf_row[cpu];
    int32_t mx = rp->mx + rp->dmx * (int32_t)o0, fai = rp->fa + rp->dfa * (int32_t)o0, fbi = rp->fb + rp->dfb * (int32_t)o0;
    uint32_t need_b = rp->mx || rp->dmx || warp == WW_FM;
    wph_t wq;
    int32_t gc = nu > 1u ? 32768 - clamp(p[P_LRTRG], 0, 127) * 256 : 32768, gsd = 32768, khc, khs, rr;   /* (BLEND: the
                                                         * centre out, the sides kept) */
    int32_t wd = nu > 1u ? clamp(p[P_DXIMG], 0, 127) * (256 - clamp(p[P_DIST], 0, 64) * 3) >> 8 : 0;
    const fvm_prog_t *pa, *pb = 0;
    const int16_t *tb = 0, *FB = fr && f->nb ? comb_buf[tr] + 6u * WF_N : 0;
    uint32_t W[FVM_BLK], T[FVM_BLK], ya[FVM_BLK], yb[FVM_BLK];
    int32_t acc[FVM_BLK], sl[FVM_BLK], sr[FVM_BLK];
    fvm_env_t ea = {{T, W, 0, 0, 0}, {0}, &v->s[6]}, eb = {{T, W, 0, 0, 0}, {0}, &v->s[7]};
    if (n > FVM_BLK)
        n = FVM_BLK;
    if (!fr) fai = 0, fbi = 0;
    if (!wave_prg_b[tr].ins)
        fvm_prog_init(&wave_prg_b[tr], wave_ins_b[tr], 32u);
    pa = wave_prog(lofi_slot(tr), &lofi_prg_id[tr], fa, wk);
    pb = need_b ? wave_prog(&wave_prg_b[tr], &wave_id_b[tr], fb, wk) : 0;
    xm_env(&ea, pa);
    if (pb)
        xm_env(&eb, pb);
    ea.u[FU_AF] = eb.u[FU_AF] = rp->pos * 516u >> 8;
    ea.u[FU_NOTE] = eb.u[FU_NOTE] = v->note;
    ea.u[FU_VEL] = eb.u[FU_VEL] = v->vel;
    if (pb && !FB && (fb == 0u || fb == 8u)) {       /* (B: a borrowed row, as before; frames: its own two) */
        uint32_t rb = wt_brow(tr);
        tb = rb < NTRK ? wt_get(tr, rb, 1, pb, fb, rp->pos >> 8, 1u, &eb, wk) : 0;
    }
    if (fl.on || (pb && !FB && !tb))
        for (i = 0; i < n; i++)
            T[i] = ((uint32_t)v->s[2] + i) >> 2;     /* (t ~11 kHz from the note's start: B's formula, LIFE) */
    khc = khs = 16384;                               /* (each voice's share: the power of 1 / sqrt(N), BLEND's centre and */
    if (nu > 1u) {                                   /* sides; one voice x 0.5: Q16 -> Q15, as before) */
        rr = (int32_t)isqrt32((uint32_t)((gc >> 1) * (gc >> 1)) + (nu - 1u) * (uint32_t)((gsd >> 1) * (gsd >> 1)));
        khc = (gc << 13) / rr;
        khs = (gsd << 13) / rr;
    }
    wph_init(&wq, warp, amt);
    if (fr && nu > 4u && !stp && !rp->dfa && (fai & 4095)) {   /* (POS between two frames the run: blended once for
                                                         * the unison's voices) */
        F = wf_mix(F, fai);
        fai = 0;
    }
    for (i = 0; i < n; i++)
        acc[i] = 0;
    if (wd)
        for (i = 0; i < n; i++)
            sl[i] = sr[i] = 0;
    for (u = 0; u < nu; u++) {
        uint32_t *pp = u < 3u ? &v->ph[u] : (uint32_t *)&v->s[WF_PH[u - 3u]], ph = *pp, iu = inc;
        int32_t o = WF_OFS[u], kh = u ? khs : khc, pg = (wd * WF_PW[u]) >> 7, *sd = u & 1u ? sl : sr;
        fvm_env_t em = ea;
        if (mode)
            iu = (inc >> 12) * WF_RAT[mode][u] + ((inc & 4095u) * WF_RAT[mode][u] >> 12);
        if (u && vc && det)                          /* (VOICES set: each its own slow drift, a quarter of the spread) */
            o += sine_i((uint32_t)v->s[2] * (19000u + u * 7300u)) >> 9;
        iu += (uint32_t)(int32_t)(((int64_t)(int32_t)((iu >> 16) * det * 3u) * o) >> 8);
        if (!wq.mb && warp != WW_SYNC && !pb && !fl.on && (stp || (fai >> 12 == (fai + rp->dfa * (int32_t)(n - 1u)) >> 12))) {
            const int16_t *r = F + ((uint32_t)(fai + (stp ? 2048 : 0)) >> 12 & 7u) * WF_N, *q = F + ((((uint32_t)fai >> 12) + 1u) & 7u) * WF_N;
            int32_t fi = fai, b = !stp && (rp->dfa || (fai & 4095));   /* (the phase read as it runs: one frame or two) */

            for (i = 0; i < n; i++, ph += iu, fi += rp->dfa) {
                uint32_t j = ph >> 25, j1 = (j + 1u) & (WF_N - 1u);
                int32_t x = (int32_t)(ph >> 16 & 511u), a = r[j] + (((r[j1] - r[j]) * x) >> 9);
                if (b) {
                    int32_t c = q[j] + (((q[j1] - q[j]) * x) >> 9);
                    a += ((c - a) * (fi & 4095)) >> 12;
                }
                ya[i] = (uint32_t)(a * 2);
            }
            goto mixed;
        }
        ph = wave_phw(&wq, W, n, ph, iu);
        if (pb) {
            if (FB || tb) {
                if (FB) wf_read(FB, W, n, fbi, rp->dfb, 0u, yb); else wt_read(tb, W, n, yb);
                if (!u) fl_post(pb, &eb, &wk->s, n, yb);
            } else if (u < 3u)                       /* (the formula: three voices' B, the others the third's) */
                (u ? fvm_run : formula_run)(pb, &eb, &wk->s, n, yb);
            if (warp == WW_FM)
                for (i = 0; i < n; i++)
                    W[i] = (W[i] + (uint32_t)(((int32_t)yb[i] * (int32_t)amt) >> 7)) & 0xFFFFu;
        }
        {
            uint32_t k = MC_CPU() & 1u, *Wm = fl_wt[k][0], *Tm = fl_wt[k][1];   /* (LIFE LINK's cross-FM, its LIFE after) */
            if (fl.on && fl.link_amt && fl_fm(&ea, n, Wm, Tm)) { em.arr[FA_W] = Wm; em.arr[FA_T] = Tm; }
            wf_read(F, em.arr[FA_W], n, fai, rp->dfa, stp, ya);
            if (!u) fl_post(pa, &em, &wk->s, n, ya);
        }
    mixed:
        if (pb && !rp->dmx) {                        /* (A .. B: as before, exactly; MIX moving: its ramp) */
            uint32_t mix = (uint32_t)mx >> 8;
            for (i = 0; i < n; i++) {
                uint32_t d = yb[i] - ya[i];
                ya[i] += (uint32_t)((int32_t)d >> 7) * mix + ((d & 127u) * mix >> 7);
            }
        } else if (pb)
            for (i = 0; i < n; i++)
                ya[i] += (uint32_t)((((int32_t)(yb[i] - ya[i]) >> 1) * ((mx + rp->dmx * (int32_t)i) >> 1)) >> 13);
        if (pg)
            for (i = 0; i < n; i++) {
                int32_t c = (((int32_t)ya[i] >> 1) * kh) >> 14;
                acc[i] += c;
                sd[i] += c * pg >> 6;
            }
        else
            for (i = 0; i < n; i++)
                acc[i] += (((int32_t)ya[i] >> 1) * kh) >> 14;
        *pp = ph;
    }
    {
        int32_t *dl = wd ? wf_side(0, 0) : 0, *dr = wd ? wf_side(1u, 0) : 0;
        uint32_t x = vo_off_c[cpu] + (o0 << hs);
        for (i = 0; i < n; i++) {
            int32_t s = acc[i];
            if (sub) {
                uint32_t sp = (uint32_t)v->s[3];
                s += sine_i(sp) * (int32_t)sub >> 8;
                v->s[3] = (int32_t)(sp + (inc >> 1));
            }
            out[i] += voice_amp(soft_knee(s >> 1, 16000) << 1, m, i + o0);
            if (dl && x < CTL) {                     /* (WIDTH: what each side gives up, as IMAGE's) */
                int32_t a = amp_at(m, i + o0), l = mulq15(mulq15(sl[i], a), VOICE_FS), r = mulq15(mulq15(sr[i], a), VOICE_FS);
                dl[x] += l, dr[x] += r;
                if (hs) dl[x + 1u] += l, dr[x + 1u] += r;
            }
            x += 1u << hs;
        }
    }
    v->s[2] = (int32_t)((uint32_t)v->s[2] + n);
}
static int32_t wf_map(const wf_t *f, int32_t q)      /* POS (Q16) -> A's frame (Q12) */
{
    return f->gs ? clamp((int32_t)(((int64_t)((q >> 8) - f->g0) * f->rq) >> 16), 0, (f->na - 1) << 12) : 0;
}
static void wave_render_x(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK, vi = tr * NVOICE + (uint32_t)(v - t->v) % NVOICE, o;
    uint32_t warp = (uint32_t)clamp(p[P_E2], 0, WW_DISP), fa = (uint32_t)p[P_E0] % WAVE_NF;
    wf_t *f = &wf[tr];
    if (!WF_IDEAL && n && !p[P_CAOS] && !(pt_ops & PT_TIME_OPS) && warp != WW_QUANT) {   /* (CAOS, the white punch-ins, QUANT: the
                                                         * formula as written, wave_part) */
        const int16_t *F = comb_buf[tr];
        wf_rp_t rp = {0, 0, 0, 0, 0, 0, 0};
        uint32_t sh = f->use && warp == WW_SHEP;
        int32_t p1 = ((wf_frames(p) >> 1) ? f->lane : p[P_E1] << 8) + m->shape - (64 << 8), p0;
        p1 = sh ? p1 & 0x7FFF : clamp(p1, 0, 127 << 8);   /* (SHEPARD round: POS 127 next to 0) */
        p0 = wf_vpos[vi] ? wf_vpos[vi] - 1 : p1;
        if (sh && p1 - p0 > 16384) p0 += 32768;
        else if (sh && p0 - p1 > 16384) p0 -= 32768;
        rp.pos = (uint32_t)p1;
        if (!f->use) {                               /* (the frames being drawn: the table at POS, as before) */
            lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
            fvm_env_t e = {{0, 0, 0, 0, 0}, {0}, &v->s[6]};
            F = wt_get(tr, tr, 0, wave_prog(lofi_slot(tr), &lofi_prg_id[tr], fa, wk), fa, (uint32_t)p1 >> 8, 1u, &e, wk);
        }
        if (F) {
            int32_t d = ((p1 - p0) << 8) / (int32_t)n, q0 = (p0 << 8) + d, q1 = p1 << 8, k = n > 1u ? (int32_t)n - 1 : 1;
            wf_vpos[vi] = (uint16_t)(p1 + 1);
            if (f->use) {                            /* (POS from the last sample's to this one's: A's frame, B's) */
                rp.fa = sh ? q0 >> 8 : wf_map(f, q0);
                rp.dfa = sh ? d >> 8 : (wf_map(f, q1) - rp.fa) / k;
                rp.fb = f->nb > 1u ? (q0 >> 8) * 129 >> 10 : 0;
                rp.dfb = f->nb > 1u ? ((q1 >> 8) * 129 >> 10) - rp.fb : 0;
                rp.dfb /= k;
            }
            rp.dmx = ((f->mx1 - f->mx0) << 8) / (int32_t)n;
            rp.mx = (f->mx0 << 8) + rp.dmx;
            for (o = 0; o < n; o += FVM_BLK)
                wave_fpart(t, v, out + o, n - o < FVM_BLK ? n - o : FVM_BLK, m, o, F, &rp);
            return;
        }
    }
    wf_vpos[vi] = 0;
    for (o = 0; o < n; o += WAVE_CH)
        wave_part(t, v, out + o, n - o < WAVE_CH ? n - o : WAVE_CH, m, o);
}
static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"WAVE", "OSC B"},
    .edit = {
        {"A", F_ENUM, 0, WAVE_NF - 1u, 0, N_WAVEF},
        {"POS", F_PCT, 0, 127, 0, 0},
        {"WARP", F_ENUM, 0, WW_DISP, 0, N_WARP},
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
