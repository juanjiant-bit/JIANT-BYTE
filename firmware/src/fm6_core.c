/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2012 Google Inc.
 * Copyright 2016-2025 Pascal Gauthier.
 * C port for Felucca (integer only, no float), 2026: Leo Kuroshita (@kurogedelic), Hügelton Instruments
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* FM6 core: msfa (the 6-operator FM synthesis core of Dexed, Source/msfa) ported to C.
 * Kept from msfa: its envelope (env.cc, with ACCURATE_ENVELOPE), pitch envelope (pitchenv.cc), LFO (lfo.cc),
 * note set-up (the note: level / rate scaling, velocity, operator pitch) and its renderer (fm_core.cc,
 * fm_op_kernel.cc): one operator at a time over a block, two buses, the 32 algorithm tables, feedback on
 * the operator that has it. Changed for Felucca:
 *   - no float or double: the start-up tables (Exp2, Freqlut, Lfo, PitchEnv, the detune curve, FINE) are
 *     const tables from tools/gen_tables.py (FM6_*); the amplitude-modulation exp() is an Exp2 of the same
 *     curve; msfa's float pitch bend, MPE and microtuning are gone (Felucca's pitch, bend, glide and tune
 *     arrive as the note's pitch in 1/16 semitone plus a fine factor: fm6_note_logfreq);
 *   - the sine is Felucca's 1024-point SINE (Q15), linearly interpolated, scaled by the Q24 gain to msfa's
 *     Q24 operator output;
 *   - a block is CTL (64; 32 until 0.9.9 ctl64) samples (msfa: 64);
 *   - no portamento, no operator switches, no controllers (mod wheel, breath, ...): Felucca's matrix does that.
 * The patch is the 155-byte single-voice layout (FP_* below), operator 0 = the sixth operator, as msfa. */

#define FM6_N CTL
#define FM6_LG_N 6
#if (1 << FM6_LG_N) != FM6_N
#error "FM6_LG_N does not match CTL"
#endif

/* the 155-byte patch: 6 x 21 operator bytes (the sixth operator first), then the voice */
enum {
    FP_R1 = 0, FP_L1 = 4, FP_BP = 8, FP_LD, FP_RD, FP_LC, FP_RC, FP_RS, FP_AMS, FP_KVS, FP_OL, FP_MODE, FP_FC, FP_FF,
    FP_DET, FP_OP = 21,
    FP_PR1 = 126, FP_PL1 = 130, FP_ALG = 134, FP_FB, FP_OKS, FP_LFS, FP_LFD, FP_LPMD, FP_LAMD, FP_LKS, FP_LFW,
    FP_LPMS, FP_TRNSP, FP_NAME, FP_SIZE = 155
};

/* ---------------------------------------------------------------- math --- */
/* 2^(x / 2^24), Q24 (msfa Exp2::lookup): the Q30 mantissa from FM6_EXP2, shifted by the integer part */
static inline uint32_t fm6_mant(int32_t x)
{
    uint32_t f = (uint32_t)x & 0xFFFFFFu, k = f >> 15, a = FM6_EXP2[k];   /* (V10.6k: 513 points, 2^(i / 512) - 1) */
    return (1u << 30) + a + (uint32_t)(umul64(FM6_EXP2[k + 1u] - a, f & 0x7FFFu) >> 15);
}
static inline int32_t fm6_exp2(int32_t x)
{
    int32_t i = x >> 24;
    uint32_t m = fm6_mant(x);
    return i < -24 ? 0 : (int32_t)(m >> (i > 5 ? 1 : 6 - i));      /* (msfa: up to 2^6; here 2^5 at most) */
}
/* phase increment (Q24 cycles per sample) of log2 frequency x (Q24) (msfa Freqlut::lookup) */
static inline int32_t fm6_freq(int32_t x)
{
    int32_t i = x >> 24;
    uint32_t q = (uint32_t)(umul64(fm6_mant(x), FM6_FREQ_R) >> 25);    /* 2^(i + 21) Hz units */
    if (i < 0)
        return 0;
    return (int32_t)(q >> (21 - (i > 21 ? 21 : i)));
}
/* sine of a Q24 phase (one cycle = 2^24), Q15, linear between SINE's 1024 points (its 1025th = the 1st) */
static inline int32_t fm6_sin(int32_t ph)
{
    const int16_t *s = &SINE[((uint32_t)ph >> 14) & 1023u];
    return s[0] + (((s[1] - s[0]) * (int32_t)(ph & 0x3FFF)) >> 14);
}

/* ------------------------------------------------------------ envelope --- */
typedef struct {                 /* msfa Env: level in Q24 log2 (2^24 = 6 dB), 16 more bits than the original */
    int32_t level, target, step; /* >0: increment; <0: remaining static samples (mutually exclusive) */
    int16_t outlevel;            /* microsteps (99 * 32 = full) */
    uint8_t rscale, ix, rising, down;
} fm6_env_t;
_Static_assert(sizeof(fm6_env_t) == 20, "compact FM6 envelope RAM budget");

static const uint8_t FM6_LEVELLUT[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
static int32_t fm6_scaleout(int32_t ol) { return ol >= 20 ? 28 + ol : FM6_LEVELLUT[ol < 0 ? 0 : ol]; }

/* samples at 44.1 kHz a stage holds when its level does not move (msfa ACCURATE_ENVELOPE) */
static const int32_t FM6_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250, 882000, 705600, 705600, 584325, 507150, 502740,
    441000, 418950, 352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530, 145530, 125685, 110250, 110250,
    88200, 88200, 74970, 61740, 61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562, 22050, 18522, 17640,
    15435, 14112, 13230, 11025, 9261, 9261, 7717, 6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705, 573, 573, 529, 441, 441};

/* stage ix (0..3; 4 = done) of an envelope whose rates and levels are op[FP_R1..], op[FP_L1..] */
static void fm6_env_advance(fm6_env_t *e, const uint8_t *op, uint32_t ix)
{
    int32_t lv, actual, qrate;
    e->ix = (uint8_t)ix;
    if (ix >= 4u)
        return;
    lv = op[FP_L1 + ix];
    actual = fm6_scaleout(lv) >> 1;
    actual = (actual << 6) + e->outlevel - 4256;
    actual = actual < 16 ? 16 : actual;
    e->target = actual << 16;
    e->rising = e->target > e->level;
    qrate = ((op[FP_R1 + ix] * 41) >> 6) + e->rscale;
    qrate = qrate > 63 ? 63 : qrate;
    e->step = (4 + (qrate & 3)) << (2 + FM6_LG_N + (qrate >> 2));
    if (e->target == e->level || (ix == 0u && lv == 0)) {
        int32_t sr = op[FP_R1 + ix] + e->rscale;
        sr = sr > 99 ? 99 : sr;
        int32_t samples = sr < 77 ? FM6_STATICS[sr] : 20 * (99 - sr);
        if (sr < 77 && ix == 0u && lv == 0)
            samples /= 20;                     /* the attack is scaled faster (a stage change, not per sample) */
        if (samples) e->step = -samples;
    }
}

static void fm6_env_init(fm6_env_t *e, const uint8_t *op, int32_t outlevel, int32_t rate_scaling)
{
    e->outlevel = (int16_t)outlevel;
    e->rscale = (uint8_t)rate_scaling;
    e->level = 0;
    e->down = 1;
    fm6_env_advance(e, op, 0);
}

static int32_t fm6_env_get(fm6_env_t *e, const uint8_t *op)
{
    if (e->step < 0) {
        e->step += FM6_N;
        if (e->step >= 0) {
            e->step = 0;
            fm6_env_advance(e, op, e->ix + 1u);
        }
    }
    if (e->ix < 3u || (e->ix < 4u && !e->down)) {
        if (e->step < 0) {
            ;
        } else if (e->rising) {
            if (e->level < (1716 << 16))
                e->level = 1716 << 16;
            e->level += (((17 << 24) - e->level) >> 24) * e->step;
            if (e->level >= e->target) {
                e->level = e->target;
                fm6_env_advance(e, op, e->ix + 1u);
            }
        } else {
            e->level -= e->step;
            if (e->level <= e->target) {
                e->level = e->target;
                fm6_env_advance(e, op, e->ix + 1u);
            }
        }
    }
    return e->level;
}

static void fm6_env_key(fm6_env_t *e, const uint8_t *op, int down)
{
    if (e->down != (uint8_t)down) {
        e->down = (uint8_t)down;
        fm6_env_advance(e, op, down ? 0u : 3u);
    }
}

/* ------------------------------------------------------ pitch envelope --- */
typedef struct {
    int32_t level, target, inc;
    uint8_t ix, rising, down;
    uint8_t half;                /* (codex/voices; was padding) bit 0: the note renders at half the rate (eng_fm6.c),
                                  * bit 1: from this block, or back at the full one (fm6_note_compute) */
} fm6_penv_t;



/* ----------------------------------------------------------------- LFO --- */
typedef struct {
    uint32_t phase, delta, dstate, dinc, dinc2;
    uint8_t wave, rnd, sync, rsv;
} fm6_lfo_t;

static void fm6_lfo_reset(fm6_lfo_t *l, const uint8_t *p)   /* the patch's LFO (msfa Lfo::reset); phase kept */
{
    int32_t a = 99 - (p[FP_LFD] % 100u);
    l->delta = FM6_LFO_DELTA[p[FP_LFS] % 100u];
    if (a == 99) {
        l->dinc = l->dinc2 = ~0u;
    } else {
        a = (16 + (a & 15)) << (1 + (a >> 4));
        l->dinc = (uint32_t)(FM6_LFO_UNIT * a);
        a &= 0xff80;
        a = a < 0x80 ? 0x80 : a;
        l->dinc2 = (uint32_t)(FM6_LFO_UNIT * a);
    }
    l->wave = p[FP_LFW];
    l->sync = p[FP_LKS] != 0;
}

static int32_t fm6_lfo_sample(fm6_lfo_t *l)   /* 0..1, Q24; once a block */
{
    int32_t x;
    l->phase += l->delta;
    switch (l->wave) {
    case 0:                                       /* triangle */
        x = (int32_t)(l->phase >> 7);
        x ^= -(int32_t)(l->phase >> 31);
        return x & ((1 << 24) - 1);
    case 1:                                       /* saw down */
        return (int32_t)((~l->phase ^ (1u << 31)) >> 8);
    case 2:                                       /* saw up */
        return (int32_t)((l->phase ^ (1u << 31)) >> 8);
    case 3:                                       /* square */
        return (int32_t)(((~l->phase) >> 7) & (1u << 24));
    case 4:                                       /* sine */
        return (1 << 23) + (fm6_sin((int32_t)(l->phase >> 8)) << 8);
    case 5:                                       /* sample and hold */
        if (l->phase < l->delta)
            l->rnd = (uint8_t)((l->rnd * 179 + 17) & 0xff);
        return ((l->rnd ^ 0x80) + 1) << 16;
    }
    return 1 << 23;
}

static int32_t fm6_lfo_delay(fm6_lfo_t *l)   /* the delay ramp, 0..1 Q24; once a block */
{
    uint32_t d = l->dstate < (1u << 31) ? l->dinc : l->dinc2, s = l->dstate + d;
    if (s < l->dstate)                            /* past 2^32: delayed in full */
        return 1 << 24;
    l->dstate = s;
    return s < (1u << 31) ? 0 : (int32_t)((s >> 7) & ((1u << 24) - 1u));
}


/* ------------------------------------------------------- the algorithms --- */
enum { FM6_OB1 = 1, FM6_OB2 = 2, FM6_OADD = 4, FM6_IB1 = 16, FM6_IB2 = 32, FM6_FBIN = 64, FM6_FBOUT = 128 };
static const uint8_t FM6_ALG[32][6] = {      /* per operator (the sixth first): in bus, out bus, add, feedback */
    {0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x11, 0x14, 0xc1, 0x14}, {0xc1, 0x11, 0x14, 0x01, 0x11, 0x14},
    {0xc1, 0x11, 0x94, 0x01, 0x11, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}, {0xc1, 0x94, 0x01, 0x14, 0x01, 0x14},
    {0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}, {0x01, 0x11, 0xc5, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x05, 0x14, 0xc1, 0x14},
    {0x01, 0x05, 0x14, 0xc1, 0x11, 0x14}, {0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x05, 0x14, 0xc1, 0x14},
    {0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}, {0xc1, 0x05, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x05, 0x11, 0x14, 0xc1, 0x14},
    {0xc1, 0x11, 0x02, 0x25, 0x05, 0x14}, {0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}, {0x01, 0x11, 0x11, 0xc5, 0x05, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x14}, {0x01, 0x14, 0x14, 0xc1, 0x14, 0x14},
    {0xc1, 0x14, 0x14, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}, {0xc1, 0x14, 0x14, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}, {0xc1, 0x05, 0x14, 0x01, 0x14, 0x04}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x04},
    {0x04, 0xc1, 0x11, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}, {0xc4, 0x04, 0x04, 0x04, 0x04, 0x04}};

/* bit k: operator k (the sixth first) is a carrier of algorithm a (0..31): it writes the output bus (msfa's
 * isCarrier also counts the modulators that add into bus 1 or 2) */
static uint32_t fm6_carriers(uint32_t a)
{
    /* Immutable algorithm metadata: avoid scanning six operators for each
     * voice's compute/done checks and the track's macro synchronization. */
    static const uint8_t CARRIER_MASK[32] = {
        0x28, 0x28, 0x24, 0x24, 0x2a, 0x2a, 0x28, 0x28, 0x28, 0x24, 0x24, 0x28, 0x28, 0x28, 0x28, 0x20,
        0x20, 0x20, 0x26, 0x34, 0x36, 0x2e, 0x36, 0x3e, 0x3e, 0x34, 0x34, 0x29, 0x3a, 0x39, 0x3e, 0x3f
    };
    return CARRIER_MASK[a & 31u];
}

/* ------------------------------------------------------- the operators --- */
typedef struct {                 /* msfa FmOpParams */
    int32_t level_in;            /* Q24 log2 */
    int32_t gain_out;            /* Q24 */
    int32_t freq;                /* Q24 cycles per sample */
    int32_t phase;               /* Q24 */
} fm6_op_t;

#define FM6_THRESH 1120          /* msfa kLevelThresh: an operator below it on both block ends is skipped */
#define FM6_DONE (FM6_THRESH * 8) /* (JIANT 0.6.14) a released note ends once its carriers are under it (~18 dB above
                                   * the skip: a long release's tail, inaudible, computed for seconds on six operators) */

/* one operator over the block: modulated by in (0: none), into out (add: += else =). Four loops, so none
 * tests a flag per sample. (codex/voices) add's bit 1: at half the rate, FM6_N / 2 samples (freq is doubled) */
#define FM6_OP_LOOP(PH, STORE)                                                    \
    for (i = 0; i < n; i++) {                                                   \
        g += dg;                                                                \
        out[i] STORE mul64_sh15(fm6_sin(PH), g);                                \
        phase = (int32_t)((uint32_t)phase + (uint32_t)freq);                    \
    }


/* msfa FmCore::render: the six operators of algorithm alg into out (Q24). Unlike msfa, out starts empty:
 * the first carrier sounding writes it, the next ones add. Returns 0 when none sounded (out untouched) */
#ifndef MC_CPU
#define MC_CPU() 0u                 /* (built alone: tests/fm6_test.c) */
#endif

/* ------------------------------------------------------------ the note --- */
typedef struct {                 /* a sounding note (msfa's note): 272 bytes */
    fm6_env_t env[6];
    fm6_op_t op[6];
    int32_t base[6];             /* the operator's pitch: ratio: log2 offset from the note's (Q24); fixed: log2 Hz */
    int32_t fb[2];
    fm6_penv_t penv;
    uint8_t fixed;               /* bit k: operator k has a fixed frequency */
    uint8_t live, down, trim;    /* trim (eng_fm6.c, JIANT 0.6.8): the output attenuation its last block had */
    int32_t dc;                  /* (eng_fm6.c) the output's DC blocker */
} fm6_note_t;

#define FM6_LOGF0 50857777       /* log2 of MIDI note 0's frequency, Q24 */







/* (codex/voices) a released note hurried (voice.c rel_hurry: the budget nearly full): every operator past its key-up
 * falls step more a block (Q24 log2), under its L4 too (a patch whose release ends above silence would hold its voice);
 * one still rising to its L4 is left to get there */

