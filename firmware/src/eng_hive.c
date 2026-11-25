/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE 0.17) HIVE: strings with feedback, a drone that lives. Four waveguide strings in two pairs (root + its partner,
 * the second pair's root + its partner: PAIR's tuning, 5+9 the root, fifth, octave and ninth), each a delay line whose
 * loop gain FEEDBK takes from a plucked decay to just over 1: a tanh in the loop holds it, so a string sounds on by
 * itself. Each string's length is moved by its partner's output (CROSS: beating, a howl at the top); a saturating echo
 * of the mix (ECHO, TIME) heard over the strings, a little of it back into all four (it blooms). A touch is a bytebeat
 * burst into the strings (TOUCH: its formula, the note's velocity its strength; BOW PULSE RAIN SWELL go on touching),
 * HOLD keeps the drone after the keys (as DRONE: STOP, MUTE, PANIC or the note alone again let it go).
 *   (hive2) the timbre  STIFF (allpasses in each loop: stretched partials, a bell), BODY (two resonators on the mix: wood,
 *               metal, glass), POS (the strings heard at a second point: thin near the bridge, hollow at the middle),
 *               DRIVE (a bent saturation on the heard strings); all eight of E1..E8 matrix targets, each smoothed and
 *               ramped across the block (no zipper); TOUCH PAIR TIME HOLD the third page, TOUCH (HV_PG3).
 *   (hive2) the echo  was ECHO's loss: the echo into the strings at 0.06 grew through their resonance (x 1 / (1 - gain))
 *               into a second loop over 1, the echo line saturated: at ECHO 127 a dark wall ~8x the strings (+42 dB at
 *               FEEDBK 70), its BBD of 376 stages ~300-700 Hz wide; the repeat itself -8..-14 dB, none at 30 MS. Now
 *               the line a byte a stage (664 of them), its tap at TIME (a stage a sample under ~60 ms: the whole band),
 *               the repeats -4..-7 dB under the touch, the bloom at 0.006, the strings' own level untouched
 *   the notes   the first key held is the first pair's root, the newest the second's; one key: both pairs from it
 *   the rate    the strings at 11 kHz (the output read between their samples: as FORM's states; the strings are dark, at
 *               22 kHz their renders had under 0.05 % of their power above 5.5 kHz), at 22 kHz when a string passes
 *               ~2.2 kHz (above ~B5 on 5+9); a string too low for the line an octave up (under ~32 Hz on 5+9)
 *   the screen  the strings as lines, their pairs linked, the echo a pulse (ui_engvis.c vx_hive)
 * Memory (no RAM of its own): the part's COMB line (voice.c comb_buf, 2 KB: a HIVE part's COMB and METAL are dry) holds
 * the four strings, laid out by their lengths; the part's DRUM-X kit row (eng_drum.c drum_kit, 896 B: a HIVE part plays no
 * kit) its state (hv_st_t) and the echo's line (HV_ES stages, a byte each, clocked as a bucket brigade past the line's
 * length: TIME sets its clock, a long time darker, a tape's wow). Both rows cleared when the part's voice ends and when
 * the engine changes, as each owner expects them. */
#define HV_N 4u
#define HV_LINE COMB_LEN                                 /* (the line the strings share) */
#define HV_MAGIC 0x48495645u
#define HV_RLG 181                                       /* (BYTE relief) the input at half the rate, x sqrt 2 (Q7): what
                                                          * a touch and the bloom give a second, a band-limited input's
                                                          * and a broadband one's between (HIVE's presets +-3.5 dB) */
#define HV_GAIN 43000                                    /* the mix's level (Q15: x 1.3; the prototype's 0.3, its renders
                                                          * normalised; the cubic's knee 2/3 of the tanh's) */
typedef struct {
    uint32_t magic;                                      /* HV_MAGIC: the rows are HIVE's (else cleared at the next use) */
    uint16_t base[HV_N], cap[HV_N], w[HV_N];             /* each string's place in the line, its length, its write point */
    int16_t o[HV_N];                                     /* .. its last sample read (its partner's CROSS) */
    int32_t lp[HV_N], dc[HV_N];                          /* .. its loop's damping and its DC (Q8 finer) */
    int32_t per[HV_N];                                   /* .. its loop's length now, Q8 samples (glided to the note's) */
    int32_t ap[HV_N][2];                                 /* .. STIFF's two allpasses */
    int16_t cav[HV_N];                                   /* .. their coefficient as the last block ended (ramped) */
    int32_t pd[HV_N];                                    /* .. POS's pickup, Q8 samples after the read (ramped) */
    uint8_t sh, ph, n1, n2;                              /* the strings' rate FS >> sh; the output's place between two of
                                                          * their samples; the pairs' notes (n2 0: both from n1) */
    uint8_t lay, duck, tn, hold;                         /* laid out; blocks of a relayout's dip; the note touched; HOLD */
    uint16_t ew, tv;                                     /* the echo's next stage; the touch's velocity */
    uint32_t eclk, tt, rng;                              /* the echo's clock (Q16), the samples since the touch, noise */
    int32_t elp, e0, e1, y0, y1, tamp;                   /* the echo's input (low-passed), its last two taps; the output's
                                                          * last two samples; the touch's level (Q15) */
    int32_t eol, bd[4], dco;                             /* the echo heard (smoothed); BODY's two resonators (lp, bp);
                                                          * DRIVE's DC */
    int32_t rv[6];                                       /* POS's depth, level; BODY's share; DRIVE's gain, way out, bend:
                                                          * as the last block ended (each ramped across the block) */
    int16_t sm[8];                                       /* E1..E8 as heard (Q8: each a block a quarter of the way to its
                                                          * knob, the matrix's moves too: no zipper) */
    int16_t lv[HV_N + 1u];                               /* the strings' and the echo's levels (the screen) */
    uint8_t nk, pl, smon, rl;                            /* the keys held at the last block; the transport playing; sm, rv
                                                          * set; (BYTE relief) the strings an octave under the rate laid
                                                          * out for (sh one more) */
} hv_st_t;
/* the echo's stages: a byte each (sign, 3 bits of octave, 4 of mantissa: a BBD's grain), the rest of the kit row */
#define HV_ES ((uint32_t)(sizeof drum_kit[0] - sizeof(hv_st_t)))
_Static_assert(sizeof(hv_st_t) % 4u == 0u && HV_ES >= 600u, "HIVE: its state and its echo in a kit row");
static hv_st_t *hv_of(uint32_t tr) { return (hv_st_t *)(void *)drum_kit[tr % NTRK]; }
static void hv_clear(uint32_t tr)
{
    memset(drum_kit[tr % NTRK], 0, sizeof drum_kit[0]);
    memset(comb_buf[tr % NTRK], 0, sizeof comb_buf[0]);
    wf[tr % NTRK].key = 0;                               /* (waveframes) a WAVE part's frames there, gone */
    wf[tr % NTRK].use = 0;
}
static hv_st_t *hv_get(uint32_t tr)
{
    hv_st_t *s = hv_of(tr);
    if (s->magic != HV_MAGIC) {
        hv_clear(tr);
        s->magic = HV_MAGIC;
        s->sh = 1;
        s->rng = 0x9E3779B9u + tr;
    }
    return s;
}

static const char *const N_HVTIME[] = {"30MS", "60MS", "120MS", "190MS", "280MS", "430MS", "650MS",
                                       "1/16", "1/8", "3/16", "1/4", "3/8", "1/2", 0};
static const uint16_t HV_TMS[7] = {30, 60, 120, 190, 280, 430, 650};
static const uint8_t HV_T16[6] = {1, 2, 3, 4, 6, 8};
static const char *const N_HVPAIR[] = {"5+9", "BEAT", "4+8", "OCT", "MIN7", "MAJ7", "TRI", "SUB", 0};
/* the four strings in cents from the root: the first pair's root and partner, the second's (two keys: the second pair
 * from the newest, its partner as far above it) */
static const int16_t HV_PAIR[8][HV_N] = {
    {0, 702, 1200, 1404},                                /* 5+9: root, fifth, octave, ninth */
    {0, 5, 1200, 1209},                                  /* BEAT: two near unisons (1.003, 2.01) */
    {0, 702, 500, 1700},                                 /* 4+8: root, fifth; the fourth and its octave */
    {0, 1200, 702, 1902},                                /* OCT: root, octave; fifth, twelfth */
    {0, 300, 700, 1000},                                 /* MIN7 */
    {0, 400, 700, 1100},                                 /* MAJ7 */
    {0, 600, 1100, 1700},                                /* TRI: the tritones, a howl */
    {0, 702, -1200, -498},                               /* SUB: the second pair an octave down */
};
static const char *const N_HVTOUCH[] = {"PLUCK", "GRIT", "KNOCK", "NOISE", "BOW", "PULSE", "RAIN", "SWELL", 0};
enum { HT_PLUCK, HT_GRIT, HT_KNOCK, HT_NOISE, HT_BOW, HT_PULSE, HT_RAIN, HT_SWELL };
/* (hive2) the third page, TOUCH: TOUCH PAIR TIME HOLD, lists (no matrix's move in them), in slots a HIVE part has free
 * (core.h P_HVTCH ..; params.c track_desc), E1..E8 the eight a matrix moves */
static const param_desc_t HV_PG3[4] = {
    {"TOUCH", F_ENUM, 0, 7, 0, N_HVTOUCH},
    {"PAIR", F_ENUM, 0, 7, 0, N_HVPAIR},
    {"TIME", F_ENUM, 0, 12, 4, N_HVTIME},
    {"HOLD", F_ENUM, 0, 1, 1, N_ONOFF},
};
/* a touch: the prototype's bytebeat ((t x m) & (t >> s)) & 255, two of them (k and k + 3: m = k % 5 + 2, s = k % 3 + 4;
 * the second pair's strings the first's turned over; NOISE RAIN: noise), t's multiplier and shift; a burst's length in
 * quarters of the longest string (0: it goes on while held), its level, its rise (Q16 a sample at 11 kHz) */
static const uint8_t HV_TF[8][9] = {
    /* m0 s0 m1 s1 mul sh len lvl rise */
    {4, 5, 2, 5, 3, 0, 4, 64, 0},                        /* PLUCK (k 7) */
    {3, 6, 6, 6, 5, 0, 8, 52, 0},                        /* GRIT (k 11) */
    {4, 6, 2, 6, 1, 2, 2, 90, 0},                        /* KNOCK (k 2) */
    {0, 0, 0, 0, 1, 0, 1, 64, 0},                        /* NOISE */
    {6, 5, 4, 5, 1, 1, 0, 5, 160},                       /* BOW (k 4) */
    {4, 6, 2, 6, 1, 3, 0, 4, 160},                       /* PULSE (k 2) */
    {0, 0, 0, 0, 1, 0, 0, 0, 255},                       /* RAIN (its drops' fall) */
    {6, 4, 4, 4, 1, 0, 0, 7, 4},                         /* SWELL (k 9) */
};
static const uint16_t HV_EXP16[16] = {4096, 4277, 4467, 4664, 4871, 5087, 5312, 5547,
                                      5793, 6049, 6317, 6597, 6889, 7194, 7512, 7845};   /* 2^(i/16), Q12 */
/* 2^(x / 4096) x 4096 for x >= -65536 (Q12 octaves: the table between its sixteenths) */
static int32_t hv_exp2(int32_t x)
{
    int32_t i = (x >> 8) & 15, f = x & 255, oc = x >> 12;
    int32_t a = HV_EXP16[i], b = i == 15 ? 8192 : HV_EXP16[i + 1];
    a += ((b - a) * f) >> 8;
    return oc >= 0 ? a << oc : a >> -oc;
}

typedef struct {                                         /* a block's values (hv_prep) */
    int32_t g, a, ein, edir, regen, ek, tl, tk, a0, ast;
    int32_t xk[HV_N], ca[HV_N], pdt[HV_N], p0[HV_N];     /* CROSS, STIFF's allpasses, POS's pickup, its length's step a
                                                          * sample: each string's */
    int32_t rt[6], rs[6];                                /* POS's depth and level, BODY's share, DRIVE's gain, its way out,
                                                          * its bend; each's step a sample from the last block's (s->rv) */
    int32_t bf[2], bq, bg;                               /* BODY: its two resonators' frequencies, their damping and gain */
    uint32_t einc, rain, en;                             /* the echo's clock, RAIN, the echo's taps (at most HV_ES) */
    const uint8_t *F;                                    /* (the touch's row of HV_TF) */
    uint8_t form, rl;                                    /* (rl: hv_st_t's: the DC's time kept) */
} hv_blk_t;

/* the four strings' periods, Q8 at 22 kHz, from the notes and PAIR */
static __attribute__((noinline)) void hv_periods(const track_t *t, const voice_t *v, const vmod_t *m, const hv_st_t *s, uint32_t *P)
{
    const int16_t *C = HV_PAIR[(uint32_t)clamp(t->p[P_HVPAIR], 0, 7)];
    int32_t p1 = m->pitch16 + ((int32_t)s->n1 - (int32_t)v->note) * 16, p2 = p1, c2 = C[2];
    uint32_t k;
    if (s->n2) {
        p2 = m->pitch16 + ((int32_t)s->n2 - (int32_t)v->note) * 16;
        c2 = 0;
    }
    for (k = 0; k < HV_N; k++) {
        uint32_t inc = cents_inc(k < 2u ? p1 : p2, k < 2u ? C[k] : k == 2u ? c2 : c2 + C[3] - C[2], m->fine);
        P[k] = 0xFFFFFFFFu / ((inc >> 7) | 1u);
        if (P[k] < (4u << 8))
            P[k] = 4u << 8;                              /* (at most ~5.5 kHz) */
    }
}
static uint32_t hv_req(uint32_t p) { return (p >> 8) + (p >> 11) + 4u; }   /* a string's room: CROSS's swing, the read */
/* (BYTE relief) the strings to half their rate (r 1) or back (0), in their places: each line's samples by age, the
 * newest first (down: age i takes age 2i, older, not yet written over) or the oldest first (up: age i between ages i / 2
 * and i / 2 + 1, newer); their lengths, POS's pickup and the touch's time with them: the sound goes on, no dip */
static __attribute__((noinline)) void hv_rerate(hv_st_t *s, int16_t *L, uint32_t r)
{
    uint32_t k;
    for (k = 0; k < HV_N; k++) {
        int16_t *b = L + s->base[k];
        int32_t c = s->cap[k], w = s->w[k], i;
#define HV_AT(a) b[w - (a) < 0 ? w - (a) + c : w - (a)]
        if (r)
            for (i = 1; 2 * i < c; i++)
                HV_AT(i) = HV_AT(2 * i);
        else
            for (i = c - 1; i > 1; i--)
                HV_AT(i) = (int16_t)((HV_AT(i >> 1) + HV_AT((i + 1) >> 1)) >> 1);
#undef HV_AT
        b[c] = b[0];
        s->per[k] = r ? s->per[k] >> 1 : s->per[k] << 1;
        s->pd[k] = r ? s->pd[k] >> 1 : s->pd[k] << 1;
    }
    s->tt = r ? s->tt >> 1 : s->tt << 1;
    s->sh = (uint8_t)(s->sh + (r ? 1u : 0u) - (r ? 0u : 1u));
    s->rl = (uint8_t)r;
}
/* the rate and the layout for periods P (Q8 at 22 kHz; out: at the rate chosen, an octave up where none fits) */
static __attribute__((noinline)) void hv_layout(hv_st_t *s, uint32_t *P, int16_t *L)
{
    uint32_t k, sh = 2, sum = 0, req[HV_N], fit = 1, b = 0;
    for (k = 0; k < HV_N; k++)                           /* 11 kHz; 22 kHz when a string passes ~2.2 kHz (10 samples) */
        if (P[k] < (10u << 8)) sh = 1;
    for (k = 0; k < HV_N; k++) {
        P[k] >>= sh - 1u;
        sum += hv_req(P[k]);
    }
    while (sum > HV_LINE - HV_N) {                       /* (too low even at 11 kHz: the longest an octave up) */
        uint32_t j = 0;
        for (k = 1; k < HV_N; k++)
            if (P[k] > P[j]) j = k;
        sum -= hv_req(P[j]);
        P[j] >>= 1;
        sum += hv_req(P[j]);
    }
    for (k = 0; k < HV_N; k++) {
        req[k] = hv_req(P[k]);
        fit &= req[k] <= s->cap[k];
    }
    if (s->lay && sh + s->rl == s->sh && fit)
        goto relief;
    for (k = 0; k < HV_N; k++) {                         /* the spare room shared by length: a glide, a bend fit */
        uint32_t c = req[k] + (HV_LINE - HV_N - sum) * req[k] / sum;
        s->base[k] = (uint16_t)b;
        s->cap[k] = (uint16_t)c;
        s->w[k] = (uint16_t)(s->w[k] % c);
        b += c + 1u;                                     /* (and its guard: the line's first sample again) */
        s->per[k] = 0;                                   /* (the new length at once) */
    }
    if (s->lay)
        s->duck = 3;                                     /* (the lines' old contents in new places: a dip) */
    s->sh = (uint8_t)sh;
    s->rl = 0;
    s->lay = 1;
relief:                                                  /* (BYTE relief) RL_HIVE: an octave lower where every string
                                                          * keeps 10 samples (5.5 kHz; 11 kHz past ~550 Hz) */
    if (!(fit = load_rl >= RL_HIVE) && !s->rl)
        return;
    for (k = 0; k < HV_N; k++)
        fit &= P[k] >= (20u << 8);
    if (fit != s->rl)
        hv_rerate(s, L, fit);
    for (k = 0; k < HV_N; k++)
        P[k] >>= s->rl;
}
static void hv_touch(hv_st_t *s, uint32_t vel)
{
    s->tt = 0;
    s->tv = (uint16_t)vel;
    s->tamp = 0;
}
/* a resonator's SVF frequency 2 sin(pi f / fs), Q15, f = 2^(x / 4096) x 40 Hz at the strings' rate (at most fs / 6.5) */
static int32_t hv_svf_f(int32_t x, uint32_t sh)
{
    int32_t w = (int32_t)((((uint32_t)hv_exp2(x) >> 4) * 5974u << sh) >> 14);   /* (pi f / fs, Q15) */
    w = w > 15800 ? 15800 : w;
    return 2 * w;                                        /* (2 sin w: w, a little sharp at the top) */
}
/* the block's values from the knobs (the matrix's SHAPE on CROSS, its CUT on BRIGHT), each heard a quarter of the way
 * a block (sm: a knob, an LFO or ENV 2 on any of E1..E8 moves it without a step) */
static __attribute__((noinline)) void hv_prep(const track_t *t, const voice_t *v, const vmod_t *m, hv_st_t *s, hv_blk_t *B)
{
    const int16_t *p = t->p;
    uint32_t P[HV_N], k, sh, lo = 0, x2;
    int32_t tq, ts, lv, tg[8], f, c, e, st, bo, br, po, dr;
    uint32_t tm = (uint32_t)clamp(p[P_HVTIME], 0, 12), form = (uint32_t)clamp(p[P_HVTCH], 0, 7), bt;
    const uint8_t *F = HV_TF[form];
    for (k = 0; k < 8u; k++)
        tg[k] = clamp(p[P_E0 + k], 0, 127) << 8;
    tg[1] = clamp(tg[1] + m->shape - (64 << 8), 0, 127 << 8);   /* (CROSS: the matrix's SHAPE) */
    tg[5] = clamp(tg[5] + m->cutoff, 0, 127 << 8);       /* (BRIGHT: its CUT) */
    for (k = 0; k < 8u; k++) {
        int32_t d = tg[k] - s->sm[k];
        s->sm[k] = (int16_t)(!s->smon || (d < 64 && d > -64) ? tg[k] : s->sm[k] + (d >> 2));
    }
    f = s->sm[0]; c = s->sm[1]; e = s->sm[2]; st = s->sm[3]; bo = s->sm[4]; br = s->sm[5]; po = s->sm[6]; dr = s->sm[7];
    hv_periods(t, v, m, s, P);
    hv_layout(s, P, comb_buf[(uint32_t)(t - trk) % NTRK]);
    sh = s->sh;
    /* BRIGHT: the loop's one pole, its time constant tau 2^((64 - BRIGHT) / 16) samples at 44.1 kHz (64: the
     * prototype's 0.5), a = 1 / (1 + tau) at the strings' rate; its delay (tau) out of the length: in tune */
    tq = hv_exp2((64 << 8) - br - (16 << 8) * (int32_t)sh);   /* (BRIGHT's Q8 sixteenths of an octave: Q12 octaves) */
    B->a = (int32_t)((1u << 27) / (uint32_t)(4096 + tq));
    /* FEEDBK: the loop's gain, 0.90 .. 1 at 100 (a decay ever longer), over 1 above it (on by itself, ~1.024 at 127);
     * the key let go without HOLD: the strings let go too */
    if (f <= 25600) {
        uint32_t d = (uint32_t)(25600 - f);
        B->g = 32768 - (int32_t)((((d * d) >> 12) * 21475u) >> 20);
    } else
        B->g = 32768 + ((f - 25600) * 29 >> 8);
    if (!v->gate && B->g > 32604)
        B->g = 32604;
    x2 = (((uint32_t)c * (uint32_t)c >> 10) * 3932u) >> 20;   /* CROSS: (c / 127)^2 x 12 % of the length, Q15 */
    /* POS: the strings heard at a second point, POS 1 .. 127 from near the bridge (6 % of a string: thin, nasal) to its
     * middle (50 %: hollow, the odd harmonics), less the first point's (a pickup's comb), the root's level kept */
    {
        int32_t fr = 1966 + (int32_t)(((uint32_t)po * 14418u) >> 15), dep = po >= 8192 ? 32767 : po << 2;
        int32_t sn = (int32_t)((uint32_t)(4 * fr) * (uint32_t)(32768 - fr) >> 15), g2;   /* sin(pi fr), parabola */
        g2 = 32768 - dep + (int32_t)(((uint32_t)dep * (uint32_t)sn) >> 14);   /* |1 - dep e^(j 2 pi fr)| at the root, Q15,
                                                          * near enough (1 to 2 sin(pi fr) as dep goes) */
        B->rt[0] = po ? dep : 0;
        B->rt[1] = (int32_t)((1u << 29) / (uint32_t)(g2 < 10923 ? 10923 : g2));   /* (Q14: at most x 3) */
        B->pdt[HV_N - 1u] = fr;                          /* (x each length below: the last string's last) */
    }
    /* STIFF: two allpasses in each loop, the high partials sooner round it than the root: stretched, inharmonic (a stiff
     * string, a bar, a bell); their delay at the root (1 .. 16 samples each, a sixth of the string at most) out of the
     * length: in tune */
    {
        int32_t dd0 = 256 + (int32_t)((((uint32_t)st * (uint32_t)st >> 12) * 244u) >> 14);
        for (k = 0; k < HV_N; k++) {
            int32_t T = (int32_t)P[k] - (tq >> 4), lim, dd = dd0, lmd = (int32_t)P[k] / 6;
            if (dd > lmd) dd = lmd < 256 ? 256 : lmd;
            B->ca[k] = (int32_t)(((256 - dd) * 32768) / (256 + dd));   /* (STIFF 0: 0, two samples' delay; always on, no
                                                          * step when the matrix moves STIFF through 0) */
            {                                            /* (each's delay at the root: d / (1 + 2.76 (d^2 - 1) / P^2)) */
                int32_t d2 = ((dd * dd) >> 16) - 1, p2 = (int32_t)((P[k] >> 8) * (P[k] >> 8)) + 1;
                T -= 2 * (int32_t)(((uint32_t)dd << 12) / (uint32_t)(4096 + 11305 * d2 / p2));
            }
            T = T < (2 << 8) ? 2 << 8 : T;
            lim = ((int32_t)s->cap[k] - (s->cap[k] >> 3) - 3) << 8;
            if (T > lim) T = lim;
            if (!s->per[k] || s->per[k] > lim)
                s->per[k] = T;
            {                                            /* (a new note glides there: 3 % a block at most, ramped across
                                                          * the block, hv_str: no step) */
                int32_t d = T - s->per[k], stp = (s->per[k] >> 5) + 64;
                if (d < (32 >> sh) && d > -(32 >> sh)) s->per[k] = T, d = 0;   /* (there within a step a sample: at once) */
                B->p0[k] = (d > stp ? stp : d < -stp ? -stp : d) / (int32_t)(64u >> sh);
            }
            B->xk[k] = (int32_t)(((uint32_t)s->per[k] * x2) >> 15);
            B->pdt[k] = (int32_t)(((uint32_t)s->per[k] * (uint32_t)B->pdt[HV_N - 1u]) >> 15);
            if (!s->pd[k]) s->pd[k] = B->pdt[k];
            if ((uint32_t)s->per[k] > lo) lo = (uint32_t)s->per[k];
        }
    }
    /* DRIVE: the strings heard through a saturation, x 1 .. 6 into it (a tanh's) and bent (its square added, its DC
     * taken out: the even harmonics, a valve's growl); out by as much, x 1 .. 1.25 (quiet notes near as they were) */
    {
        int32_t dv = dr >> 8;
        B->rt[3] = 4096 + ((dv * dv * 5) >> 2);           /* (x 1 .. ~6) */
        B->rt[4] = (int32_t)(((uint32_t)(4096 + dv * 8) << 15) / (uint32_t)B->rt[3]);   /* (x 1 .. 1.25 / the gain: from 0 no step) */
        B->rt[5] = dv * 180;                                /* (the square's share, Q15: up to 0.7) */
    }
    /* BODY: the mix through two resonators (a body: 0 none; low and wide, wood; higher, the second further, metal; high
     * and ringing, glass), heard more as BODY goes up */
    B->rt[2] = 0;
    if (bo) {
        int32_t xo = (int32_t)(((uint32_t)bo * 113u) >> 8) + 5900;   /* (Q12 octaves over 40 Hz: 1.4 .. 5, 110 Hz .. 1.3 kHz) */
        B->bq = 23000 - (int32_t)(((uint32_t)bo * 21500u) >> 15);
        B->bg = 4096 + (int32_t)(((uint32_t)bo * 387u) >> 10);
        B->bf[0] = hv_svf_f(xo, sh);
        B->bf[1] = hv_svf_f(xo + 2400 + (int32_t)(((uint32_t)bo * 245u) >> 11), sh);   /* (x 1.5 .. x 2.9 above it) */
        B->rt[2] = bo >= 8192 ? 32767 : bo << 2;
    }
    /* ECHO: a line of HV_ES stages, TIME long (shorter than the line: a stage a sample, its whole band; longer: clocked
     * slower, darker, a BBD); the mix x 0.25 in, its repeats back in (0.2 .. 0.75, saturating); heard up to ~ -4 dB under
     * the strings, the strings' own level untouched; a little of it into the strings (the bloom), kept under what their
     * resonance would grow into a second loop */
    {
        int32_t u = e >> 8, u2 = u * u;
        B->edir = (u * 25756) >> 7;
        B->regen = 6554 + (u * 18022 >> 7);
        B->ein = (int32_t)(((uint32_t)u2 * 100u) >> 14) * (s->rl ? HV_RLG : 128) / 128;   /* (~0.006 at 127: the loop
                                                          * through the strings under 1; HV_RLG) */
    }
    bt = (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 300);   /* (a beat, samples at 44.1 kHz) */
    ts = tm < 7u ? (int32_t)(((uint32_t)FS >> sh) * HV_TMS[tm] / 1000u) : (int32_t)((bt >> sh) * HV_T16[tm - 7u] >> 2);
    ts = clamp(ts, 64, 66150 >> sh);
    B->en = (uint32_t)ts < HV_ES ? (uint32_t)ts : HV_ES;
    B->einc = (uint32_t)ts <= HV_ES ? 65536u : (HV_ES << 16) / (uint32_t)ts;
    if (B->einc < 65536u) {                              /* (clocked: a tape's wow, +-0.4 % over ~3 s: a held drone and its
                                                          * repeats drift apart, no fixed comb dulling it) */
        int32_t ph = (int32_t)((s->tt << sh) & 0x1FFFFu), tri = (ph > 65535 ? 131072 - ph : ph) - 32768;
        B->einc = (uint32_t)((int32_t)B->einc + (((int32_t)(B->einc >> 8) * tri) >> 15));
    }
    B->ek = clamp((int32_t)(B->einc * 3u), 64, 32767);  /* (~ the clock's half: its band, no more) */
    /* TOUCH: a burst from the note (its length a share of the longest string), or on while held */
    lv = (int32_t)(s->tv * s->tv) * 258 >> 7;            /* (the velocity squared, Q15) */
    lv = ((F[7] * lv >> 7) << 8) / 128 * (s->rl ? HV_RLG : 128);   /* (Q23: a slow SWELL's steps; HV_RLG; Q15 at most) */
    lv = lv > (32767 << 8) ? 32767 << 8 : lv;
    B->F = F;
    B->form = (uint8_t)form;
    B->rl = s->rl;
    B->tl = 0;
    B->rain = form == HT_RAIN && v->gate ? 2u << (sh - 1u) : 0u;   /* (RAIN: ~20 drops a second, each a few ms) */
    if (F[6]) {
        uint32_t len = (lo >> 8) * F[6] / 4u + 16u;
        if (!s->tt)
            s->tamp = lv;
        B->tk = s->tt < len ? (int32_t)((3u << 16) / len) : 65535;
    } else {
        B->tk = (int32_t)F[8] << (sh + (form == HT_RAIN ? 3u : 0u) - 1u);
        if (v->gate && !(form == HT_PULSE && ((s->tt << sh) / bt & 1u)))   /* (PULSE: on the even beats) */
            B->tl = lv;
    }
    {   /* the voice's amplitude ramp (vmod_t amp0 .. amp1 over CTL) at the strings' samples, x VOICE_FS, x the dip */
        int32_t gain = s->duck ? (int32_t)(4u - s->duck) * 8192 : 32768, a0 = mulq15(mulq15(m->amp0, VOICE_FS), gain);
        int32_t a1 = mulq15(mulq15(m->amp1, VOICE_FS), gain);
        B->a0 = a0 * 64;                                 /* (from the block's first sample: hive_render moves it) */
        B->ast = (a1 - a0) << sh;
    }
    {
        uint32_t q;
        if (!s->smon)                                    /* (the first block: from here) */
            for (q = 0; q < 6u; q++) s->rv[q] = B->rt[q];
        s->smon = 1;
        for (q = 0; q < 6u; q++)
            B->rs[q] = (B->rt[q] - s->rv[q]) / (int32_t)(CTL >> sh);
    }
    if (s->duck)
        s->duck--;
}

/* tanh(1.6 z) / 1.6 (the prototype's) to its third power: z - 0.853 z^3, flat from |z| 0.625 (0.417): no table, no
 * branch; its curve near 0 the tanh's, so a string just over 1 settles where the prototype's did */
static inline int32_t hv_sat(int32_t z)
{
    z = z > 20480 ? 20480 : z < -20480 ? -20480 : z;
    return z - ((((z * z) >> 15) * z >> 15) * 27962 >> 15);
}
/* an echo stage: a byte (sign, octave 0..7, 4 bits of mantissa) */
static inline uint32_t hv_enc(int32_t x)
{
    uint32_t sg = x < 0 ? 0x80u : 0u, a = (uint32_t)(x < 0 ? -x : x), e = 0;   /* (|x| < 2^15: the echo's saturation) */
    while (a >> (e + 8u)) e++;                           /* (at most 7: a tick a sample at most) */
    return sg | e << 4 | (a >> (e ? e + 3u : 4u) & 15u);
}
static inline int32_t hv_dec(uint32_t c)
{
    uint32_t e = c >> 4 & 7u, m = c & 15u;
    int32_t a = e ? (int32_t)(((m | 16u) << (e + 3u)) + (1u << (e + 2u))) : (int32_t)(m << 4);
    return c & 0x80u ? -a : a;
}
/* the strings' samples: ni of them into Y (Q15). Three passes: the echo's taps (its stages read are older than any
 * block: HV_ES of its clock ticks, a tick at most a sample) and the touch, into each string's input; each pair of
 * strings over the block (a pair's two strings sample by sample: each reads its partner); then the echo's input */
#define HV_NI (CTL / 2u + 1u)
#define HV_SB 8u
/* one string over m samples: read (its length moved by its partner's outputs pm), damp, the gain and its input in,
 * STIFF's allpasses, DRIVE, saturate, write; its outputs into om (the four's state two loads a sub-block: no spills a
 * sample), heard (POS's pickup) into mx */
static __attribute__((noinline)) void hv_str(hv_st_t *s, int16_t *L, uint32_t k, const int16_t *pm, int16_t *om,
                                             const int16_t *in, uint32_t m, const hv_blk_t *B, int32_t *mx, int32_t j0)
{
    int16_t *b = L + s->base[k];
    int32_t c = s->cap[k], w = s->w[k], l = s->lp[k], dc = s->dc[k] >> 8, p = s->per[k], x = B->xk[k], g = B->g, a = B->a;
    int32_t o = s->o[k], sum = 0, n = (int32_t)m, ca = s->cav[k], s1 = s->ap[k][0], s2 = s->ap[k][1];
    int32_t cst = (B->ca[k] - ca) / (int32_t)m;          /* (STIFF: to this block's over the sub-block) */
    int32_t pd = s->pd[k], pdt = B->pdt[k], dst = B->rs[0], pst = B->rs[1], ps = B->p0[k];
    int32_t dep = s->rv[0] + dst * j0, pmk = s->rv[1] + pst * j0;   /* (POS: ramped across the block) */
    while (n > 0) {                                      /* (up to the line's end, then from its start: no wrap a sample) */
        int32_t e = c - w < n ? c - w : n, j;
        for (j = 0; j < e; j++) {
            int32_t r = ((w + j) << 8) - p - ((pm[j] * x) >> 15), i0, z, h = o;
            if (r < 0) r += c << 8;
            i0 = r >> 8;
            o = b[i0] + (((b[i0 + 1] - b[i0]) * (r & 255)) >> 8);   /* (b[c]: b[0] again) */
            om[j] = (int16_t)o;
            h = o;
            if (dep) {                                   /* (POS: the wave further on, less) */
                int32_t r2 = r + pd, o2;
                if (r2 >= c << 8) r2 -= c << 8;
                i0 = r2 >> 8;
                o2 = b[i0] + (((b[i0 + 1] - b[i0]) * (r2 & 255)) >> 8);
                h = ((o - ((o2 * dep) >> 15)) * pmk) >> 14;
                pd += (pdt - pd) >> 4;
            }
            dep += dst; pmk += pst; p += ps;
            mx[j] += h;
            l += ((o - l) * a) >> 15;
            sum += l;
            z = (((l - dc) * g) >> 15) + in[j];
            if (ca | cst) {                              /* (STIFF: y = c x + s, s = x - c y, twice) */
                int32_t y = ((ca * z) >> 15) + s1;
                s1 = z - ((ca * y) >> 15);
                z = ((ca * y) >> 15) + s2;
                s2 = y - ((ca * z) >> 15);
                ca += cst;
            } else {                                     /* (cpupass: STIFF 0, c 0: the two a sample's delay each, as
                                                          * the four products of 0 were) */
                int32_t y = s1;
                s1 = z;
                z = s2;
                s2 = y;
            }
            b[w + j] = (int16_t)hv_sat(z);
        }
        pm += e; om += e; in += e; mx += e; n -= e;
        w += e;
        if (w == c) w = 0;
        b[c] = b[0];
    }
    s->dc[k] += ((sum << 8) - s->dc[k] * (int32_t)m) >> (10u - B->rl);   /* (its DC a sub-block, Q8 finer: ~3 Hz) */
    s->per[k] = p; s->w[k] = (uint16_t)w; s->lp[k] = l; s->o[k] = (int16_t)o;
    s->ap[k][0] = s1; s->ap[k][1] = s2; s->pd[k] = dep ? pd : pdt; s->cav[k] = (int16_t)B->ca[k];
}
/* BODY and DRIVE on the mix, each its own pass (none when off), their values ramped across the block (rt: this block's,
 * rs: the step a sample; s->rv: the last block's) */
static __attribute__((noinline)) void hv_tone(hv_st_t *s, int32_t *mx, uint32_t ni, const hv_blk_t *B)
{
    uint32_t j;
    int32_t bw = s->rv[2], dq = s->rv[3], idq = s->rv[4], dl = s->rv[5], dco = s->dco;
    const int32_t *rt = B->rt, *rs = B->rs;
    if (bw | rt[2]) {                                    /* BODY: two SVFs, their band-passes at the input's level at the peak */
        for (j = 0; j < ni; j++) {
            int32_t mix = mx[j], xi = (mix * B->bq) >> 16, r = 0, q;
            for (q = 0; q < 2; q++) {                    /* (lp bd[2q], bp bd[2q + 1]) */
                int32_t *d = s->bd + 2 * q, h = xi - d[0] - ((d[1] * B->bq) >> 15);
                d[1] += (B->bf[q] * h) >> 15;
                d[0] += (B->bf[q] * d[1]) >> 15;
                r += d[1] * (2 - q);
            }
            mix -= (mix * bw) >> 17;                     /* (the dry at 3/4, the resonances at their peaks: */
            mx[j] = mix + ((((r * B->bg) >> 12) >> 3) * bw >> 12);   /* (x 2 and x 1, x 1 .. 4 as BODY goes up) */
            bw += rs[2];
        }
    }
    if (dl | rt[5] | (dco > 16 || dco < -16)) {          /* DRIVE (on till its DC has gone: no step) */
        for (j = 0; j < ni; j++) {
            int32_t u = (mx[j] * dq) >> 12, sq;
            u = hv_sat(u >> 1) * 2;                      /* (its own clamp first: no overflow) */
            sq = (((u * u) >> 15) * dl) >> 15;
            dco += (sq - dco) >> 9;
            mx[j] = ((u + sq - dco) * idq) >> 15;
            dq += rs[3]; idq += rs[4]; dl += rs[5];
        }
    } else
        dco = 0;
    s->dco = dco;
    for (j = 0; j < 6u; j++) s->rv[j] = rt[j];
}
static __attribute__((noinline)) void hv_run(hv_st_t *s, int16_t *L, uint8_t *E, int32_t *Y, uint32_t ni, const hv_blk_t *B)
{
    uint32_t j, k, ew = s->ew, er, eclk = s->eclk, tt = s->tt, rng = s->rng, tch = (uint32_t)(s->tamp | B->tl | (int32_t)B->rain);
    int32_t e0 = s->e0, e1 = s->e1, tamp = s->tamp, eol = s->eol;
    int16_t eo[HV_NI], in[HV_N][HV_NI];
    int16_t mo[HV_N][HV_SB + HV_NI];                     /* (each string's outputs, after its last HV_SB of the block before) */
    int32_t mx[HV_NI];                                   /* (the four heard, POS's) */
    uint32_t j0;
    er = ew + HV_ES - B->en;                             /* (the tap: TIME's stages behind the next written) */
    if (er >= HV_ES) er -= HV_ES;
    for (j = 0; j < ni; j++) {                           /* the echo heard (eo) and into the strings */
        int32_t x = e0 + (((e1 - e0) * (int32_t)(eclk >> 1)) >> 15);
        eol += ((x - eol) * B->ek) >> 15;
        eo[j] = (int16_t)eol;
        in[0][j] = (int16_t)((eol * B->ein) >> 15);
        mx[j] = 0;
        eclk += B->einc;
        if (eclk >> 16) {
            eclk &= 0xFFFFu;
            e0 = e1;
            e1 = hv_dec(E[er]);
            er = er + 1u == HV_ES ? 0u : er + 1u;
        }
    }
    if (tch) {                                           /* the touch: the bytebeat (or the noise) at its level, a string
                                                          * each its own (k + 3 x the string) */
        const uint8_t *F = B->F;
        uint32_t nz = !F[0], trl = B->rl;
        for (j = 0; j < ni; j++) {
            uint32_t x = nz ? rng : ((tt + j) << s->sh) * F[4] >> F[5];
            int32_t ta;
            if (B->rain && (rng >> 21) < B->rain)        /* (RAIN: a drop) */
                tamp = (int32_t)(3355443u * s->tv / 127u) * (trl ? HV_RLG : 128) / 128;
            ta = tamp >> 8;
            {
                int32_t b0 = (int32_t)(nz ? x : x * F[0] & x >> F[1]) & 255, f = in[0][j];
                int32_t b1 = (int32_t)(nz ? x >> 8 : x * F[2] & x >> F[3]) & 255;
                b0 -= 128;
                b1 -= 128;
                b0 = (b0 * ta) >> 7;
                b1 = (b1 * ta) >> 7;
                in[0][j] = (int16_t)(f + b0); in[1][j] = (int16_t)(f + b1);   /* (the second pair: the first's, over) */
                in[2][j] = (int16_t)(f - b1); in[3][j] = (int16_t)(f - b0);
            }
            tamp += mulq16(B->tl - tamp, (uint32_t)B->tk);
            if (!B->tl && tamp < (64 << 8))
                tamp = 0;
            rng = rng * 1664525u + 1013904223u;
        }
    }
    for (k = 0; k < HV_N; k++)                           /* (the outputs before this block: the last one's, held) */
        for (j = 0; j < HV_SB; j++) mo[k][j] = s->o[k];
    for (j0 = 0; j0 < ni; j0 += HV_SB) {                 /* a pair a sub-block: the first string moved by its partner's
                                                          * sub-block before (HV_SB samples late), the partner by the first's
                                                          * own (the prototype's order: 0 then 1) */
        uint32_t m = ni - j0 < HV_SB ? ni - j0 : HV_SB;
        for (k = 0; k < HV_N; k += 2u) {
            hv_str(s, L, k, mo[k + 1u] + j0, mo[k] + HV_SB + j0, (tch ? in[k] : in[0]) + j0, m, B, mx + j0, (int32_t)j0);
            hv_str(s, L, k + 1u, mo[k] + HV_SB + j0, mo[k + 1u] + HV_SB + j0, (tch ? in[k + 1u] : in[0]) + j0, m, B, mx + j0, (int32_t)j0);
        }
    }
    hv_tone(s, mx, ni, B);
    {
        int32_t elp = s->elp;
        eclk = s->eclk;
        for (j = 0; j < ni; j++) {                       /* the echo: the mix x 0.25 in, its stages clocked */
            int32_t mix = mx[j];
            elp += (((mix >> 2) - elp) * B->ek) >> 15;
            eclk += B->einc;
            if (eclk >> 16) {
                eclk &= 0xFFFFu;
                E[ew] = (uint8_t)hv_enc(hv_sat((elp + ((eo[j] * B->regen) >> 15)) >> 1) * 2);   /* (saturating) */
                ew = ew + 1u == HV_ES ? 0u : ew + 1u;
            }
            mix = soft_knee(((mix * HV_GAIN) >> 15) + ((eo[j] * B->edir) >> 13), 24000);   /* the mix, the echo heard */
            Y[j] = mulq15(mix, (B->a0 + (int32_t)j * B->ast) >> 6);   /* (its level: the ramp at the sample, x VOICE_FS) */
        }
        s->elp = elp;
    }
    s->ew = (uint16_t)ew; s->eclk = eclk; s->tt = tt + ni; s->rng = rng;
    s->e0 = e0; s->e1 = e1; s->tamp = tamp; s->eol = eol;
    for (k = 0; k <= HV_N; k++) {                        /* (the screen: the strings' and the echo's levels, rough) */
        int32_t v = k < HV_N ? s->o[k] + L[s->base[k] + (s->cap[k] >> 1)] : eol * 2;
        s->lv[k] = (int16_t)((s->lv[k] * 7 + (v < 0 ? -v : v) * 2) >> 3);
    }
}

static void hive_note_on(track_t *t, voice_t *v)
{
    hv_st_t *s = hv_get((uint32_t)(t - trk));
    if (!t->nmono || t->nmono > 7u) {                    /* (POLY: the note both pairs'; MONO: hv_notes, the keys) */
        s->n1 = v->note;
        s->n2 = 0;
    }
    s->tn = v->note;
    hv_touch(s, v->vel);
}
static __attribute__((noinline)) void hv_notes(const track_t *t, const voice_t *v, hv_st_t *s)
{
    if (t->nmono >= 2u && t->nmono <= 7u) {              /* the first key held the first pair, the newest the second */
        s->n1 = t->mono_stack[0];
        s->n2 = t->mono_stack[t->nmono - 1u];
    } else if (t->nmono == 1u && v->gate) {              /* one key: both from it (a key of the two held: they stay) */
        uint32_t k = t->mono_stack[0];
        if (!(s->n2 && (k == s->n1 || k == s->n2))) {
            s->n1 = (uint8_t)k;
            s->n2 = 0;
        }
    }
    if (!s->n1)
        s->n1 = v->note;
    if (s->n2 == s->n1)
        s->n2 = 0;
    if (v->note != s->tn) {                              /* a key more (LEGATO, HOLD's glide): a touch; a key let go */
        if (t->nmono > s->nk || hv_hold(t))              /* back to one held: none */
            hv_touch(s, v->vel);
        s->tn = v->note;
    }
    s->nk = t->nmono;
}
static void hive_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, j = 0, ph, sh, i0, ni;
    hv_st_t *s = hv_get(tr);
    hv_blk_t B;
    int32_t Y[CTL / 2u + 1u], y0, y1;
    if (n > CTL)
        n = CTL;
    hv_notes(t, v, s);
    hv_prep(t, v, m, s, &B);                             /* (the rate and the layout first: i0 from them) */
    sh = s->sh;
    ph = s->ph & ((1u << sh) - 1u);
    i0 = ph ? (1u << sh) - ph : 0u;                      /* (the first new string sample at i0, then every 2^sh) */
    ni = i0 < n ? 1u + ((n - 1u - i0) >> sh) : 0u;
    B.a0 += (B.ast >> sh) * (int32_t)i0;                 /* (the ramp from the first new sample) */
    hv_run(s, comb_buf[tr], (uint8_t *)(void *)(s + 1), Y, ni, &B);
    y0 = s->y0; y1 = s->y1;
    for (i = 0; ph && i < n; i++) {                      /* up to 44.1 kHz: a line between the strings' samples */
        out[i] += y0 + (((y1 - y0) * (int32_t)(ph + 1u)) >> sh);
        ph = (ph + 1u) & ((1u << sh) - 1u);
    }
    for (j = 0; j < ni; j++) {                           /* (each new sample: its 2 or 4 outputs) */
        int32_t d;
        uint32_t q, e = n - i < (1u << sh) ? n - i : 1u << sh;
        y0 = y1; y1 = Y[j]; d = y1 - y0;
        if (e == 2u && sh == 1u) {                       /* (22 kHz, a whole pair: the mean, the sample) */
            out[i] += (y0 + y1) >> 1;
            out[i + 1u] += y1;
            i += 2u;
        } else if (e == 4u && sh == 2u) {                /* (cpupass: 11 kHz, a whole four: the line's quarters) */
            out[i] += y0 + (d >> 2);
            out[i + 1u] += y0 + (d >> 1);
            out[i + 2u] += y0 + ((d * 3) >> 2);
            out[i + 3u] += y1;
            i += 4u;
        } else
            for (q = 1; q <= e; q++)
                out[i++] += y0 + ((d * (int32_t)q) >> sh);
        ph = e & ((1u << sh) - 1u);
    }
    s->y0 = y0; s->y1 = y1; s->ph = (uint8_t)ph;
    if (t->xf_on && t->xf == 1u && eng_idx(t->eng_req) != ENGI_HIVE)
        hv_clear(tr);                                    /* (the engine's last block: the rows back to their owners) */
}
/* the loudness the strings' own: velocity is the touch's strength (the ADSR still shapes it) */
static int32_t hive_amp(track_t *t, voice_t *v, int32_t adsr)
{
    (void)t;
    return v->vel ? clamp(adsr * 127 / v->vel, 0, 32767) : adsr;
}
/* a block of the part, sounding or not: nothing sounding, the rows cleared (DRUM-X's kit and the COMB find theirs);
 * HOLD turned off with no key down: what it held lets go */
static void hive_block(track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i, any = 0;
    hv_st_t *s = hv_of(tr);
    if (s->magic != HV_MAGIC)
        return;
    for (i = 0; i < NVOICE; i++)
        any |= t->v[i].active;
    if (!any) {
        hv_clear(tr);
        return;
    }
    if (s->hold && ((!t->p[P_HVHOLD] && !t->nmono) || t->p[P_MUTE] || (s->pl && !song.playing)))
        drone_let(t, 255u);                              /* (HOLD off with no key down, MUTE, STOP: as DRONE's) */
    s->hold = t->p[P_HVHOLD] > 0;
    s->pl = song.playing;
}

/* a preset's third page (TOUCH PAIR TIME HOLD) in its MODS (they are not E1..E8; each 0 left out: PLUCK 5+9 30MS OFF) */
static const preset_t HIVE_PRESETS[] = {
    /* name, {FEEDBK CROSS ECHO STIFF, BODY BRIGHT POS DRIVE}, {ATK DEC SUS REL}; MODS(TOUCH PAIR TIME HOLD ..);
     * MONO: the keys held make the pairs. (hive2: ECHO a clear repeat now, the strings' level its own: each retuned) */
    {"DRONE QUINTA", {104, 18, 76, 0, 0, 64, 0, 0}, {8, 64, 127, 80}, 0, 1 | TAG(PAD), FX(0, 0, 0, 40), PAT(5),
     MODS(P_HVTIME, 4, P_HVHOLD, 1)},
    {"BATIDO CRUZADO", {113, 40, 96, 0, 0, 64, 0, 20}, {8, 64, 127, 80}, 0, 1 | TAG(PAD), FX(0, 0, 0, 35), PAT(5),
     MODS(P_HVTCH, HT_GRIT, P_HVPAIR, 1, P_HVTIME, 3, P_HVHOLD, 1)},
    {"TOQUE + ECO", {106, 28, 100, 0, 0, 70, 0, 0}, {8, 64, 127, 80}, 0, 1 | TAG(FX), FX(0, 0, 0, 40), PAT(5),
     MODS(P_HVTCH, HT_PULSE, P_HVPAIR, 2, P_HVTIME, 9, P_HVHOLD, 1)},   /* (the pulses, their dotted-eighth repeats) */
    {"KOTO", {72, 6, 56, 10, 30, 92, 24, 0}, {0, 64, 127, 50}, 0, 1 | TAG(PLUCK), FX(0, 0, 20, 30), PAT(3),
     MODS(P_HVPAIR, 3, P_HVTIME, 8)},
    /* a dark low drone, CROSS breathing */
    {"ABISMO", {106, 14, 92, 0, 20, 22, 0, 30}, {70, 64, 127, 100}, 0, 1 | TAG(PAD), FX(0, 0, 0, 50), PAT(8),
     MODS(P_HVTCH, HT_SWELL, P_HVPAIR, 7, P_HVTIME, 6, P_HVHOLD, 1, P_LRATE, 4, MMOD(1, PS_LFO, PD_E(1), 18))},
    /* the screaming cross-mod lead: MOD WHEEL howls */
    {"AULLIDO", {124, 100, 40, 0, 0, 110, 0, 50}, {0, 64, 127, 30}, 0, 1 | TAG(LEAD), FX(10, 0, 30, 20), PAT(4),
     MODS(P_HVTCH, HT_NOISE, P_HVPAIR, 6, P_HVTIME, 7, MMOD(1, PS_MODW, PD_E(1), 15), P_GLIDE, 12)},
    {"LLUVIA", {96, 20, 90, 0, 0, 80, 40, 0}, {20, 64, 127, 90}, 0, 1 | TAG(FX), FX(0, 0, 0, 45), PAT(5),
     MODS(P_HVTCH, HT_RAIN, P_HVPAIR, 4, P_HVTIME, 9, P_HVHOLD, 1)},
    /* (a wooden body under the bow) */
    {"ARCO", {98, 10, 56, 0, 46, 56, 0, 0}, {30, 64, 127, 70}, 0, 1 | TAG(PAD), FX(0, 10, 0, 40), PAT(5),
     MODS(P_HVTCH, HT_BOW, P_HVPAIR, 3, P_HVTIME, 4, P_LRATE, 10, MMOD(1, PS_LFO, PD_E(1), 12))},
    /* (hive2) the new timbres. CAMPANA: stiff strings, stretched partials, a bell (its quarter-note repeats) */
    {"CAMPANA", {88, 4, 60, 118, 0, 104, 0, 0}, {0, 64, 127, 70}, 0, 1 | TAG(BELL), FX(0, 0, 10, 40), PAT(3),
     MODS(P_HVTCH, HT_KNOCK, P_HVPAIR, 3, P_HVTIME, 10)},
    /* METAL: a struck sheet, a metal body ringing, the tritones' howl */
    {"METAL", {110, 22, 70, 52, 104, 84, 0, 50}, {10, 64, 127, 80}, 0, 1 | TAG(FX), FX(0, 0, 0, 35), PAT(5),
     MODS(P_HVTCH, HT_GRIT, P_HVPAIR, 6, P_HVTIME, 5, P_HVHOLD, 1)},
    /* HUECO: plucked at the middle, hollow (the odd harmonics), in a box */
    {"HUECO", {84, 8, 50, 0, 36, 96, 127, 0}, {0, 64, 127, 60}, 0, 1 | TAG(PLUCK), FX(0, 0, 15, 30), PAT(3),
     MODS(P_HVTIME, 9)},
    /* MAREA, the tide: the body and the pickup swaying with the LFO, the strings stiffening as ENV 2 swells, easing */
    {"MAREA", {108, 16, 84, 30, 60, 60, 50, 10}, {60, 64, 127, 100}, 0, 1 | TAG(PAD), FX(0, 0, 0, 50), PAT(8),
     MODS(P_HVTCH, HT_SWELL, P_HVPAIR, 4, P_HVTIME, 12, P_HVHOLD, 1, MLFO(LW_TRI, 2, 0, 0), MMOD(1, PS_LFO, PD_E(4), 40),
          MMOD(2, PS_ENV2, PD_E(3), 50), MMOD(3, PS_LFO, PD_E(6), -30), MENV2(100, 110, 20, 100))},
};


static const engine_t ENG_HIVE = {
    .name = "HIVE",
    .page_title = {"HIVE", "COLOR"},                     /* (hive2: TOUCH the third, HV_PG3) */
    .edit = {
        {"FEEDBK", F_PCT, 0, 127, 104, 0},
        {"CROSS", F_PCT, 0, 127, 18, 0},
        {"ECHO", F_PCT, 0, 127, 70, 0},
        {"STIFF", F_PCT, 0, 127, 0, 0},
        {"BODY", F_PCT, 0, 127, 0, 0},
        {"BRIGHT", F_PCT, 0, 127, 64, 0},
        {"POS", F_PCT, 0, 127, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0},
    },
    .presets = HIVE_PRESETS,
    .npresets = NELEM(HIVE_PRESETS),
    .poly = 1,                                           /* (one voice: the part's four strings are its) */
    .note_on = hive_note_on,
    .render = hive_render,
    .amp = hive_amp,
    .block = hive_block,
    .knob = {P_E0, P_E1, P_E2, P_E5},
};
