/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE 0.17) HIVE: strings with feedback, a drone that lives. Four waveguide strings in two pairs (root + its partner,
 * the second pair's root + its partner: PAIR's tuning, 5+9 the root, fifth, octave and ninth), each a delay line whose
 * loop gain FEEDBK takes from a plucked decay to just over 1: a tanh in the loop holds it, so a string sounds on by
 * itself. Each string's length is moved by its partner's output (CROSS: beating, a howl at the top); a long, dark,
 * saturating echo of the mix (ECHO, TIME) goes back into all four (it blooms). A touch is a bytebeat burst into the
 * strings (TOUCH: its formula, the note's velocity its strength; BOW PULSE RAIN SWELL go on touching), HOLD keeps the
 * drone after the keys (as DRONE: STOP, MUTE, PANIC or the note alone again let it go).
 *   the notes   the first key held is the first pair's root, the newest the second's; one key: both pairs from it
 *   the rate    the strings at 11 kHz (the output read between their samples: as FORM's states; the strings are dark, at
 *               22 kHz their renders had under 0.05 % of their power above 5.5 kHz), at 22 kHz when a string passes
 *               ~2.2 kHz (above ~B5 on 5+9); a string too low for the line an octave up (under ~32 Hz on 5+9)
 *   the screen  the strings as lines, their pairs linked, the echo a pulse (ui_engvis.c vx_hive)
 * Memory (no RAM of its own): the part's COMB line (voice.c comb_buf, 2 KB: a HIVE part's COMB and METAL are dry) holds
 * the four strings, laid out by their lengths; the part's DRUM-X kit row (eng_drum.c drum_kit, 896 B: a HIVE part plays no
 * kit) its state (hv_st_t) and the echo's line (HV_ES samples clocked as a bucket brigade: TIME sets its clock, a long
 * time darker). Both rows cleared when the part's voice ends and when the engine changes, as each owner expects them. */
#define HV_N 4u
#define HV_LINE COMB_LEN                                 /* (the line the strings share) */
#define HV_MAGIC 0x48495645u
#define HV_GAIN 43000                                    /* the mix's level (Q15: x 1.3; the prototype's 0.3, its renders
                                                          * normalised; the cubic's knee 2/3 of the tanh's) */
typedef struct {
    uint32_t magic;                                      /* HV_MAGIC: the rows are HIVE's (else cleared at the next use) */
    uint16_t base[HV_N], cap[HV_N], w[HV_N];             /* each string's place in the line, its length, its write point */
    int16_t o[HV_N];                                     /* .. its last sample read (its partner's CROSS) */
    int32_t lp[HV_N], dc[HV_N];                          /* .. its loop's damping and its DC (Q8 finer) */
    int32_t per[HV_N];                                   /* .. its loop's length now, Q8 samples (glided to the note's) */
    uint8_t sh, ph, n1, n2;                              /* the strings' rate FS >> sh; the output's place between two of
                                                          * their samples; the pairs' notes (n2 0: both from n1) */
    uint8_t lay, duck, tn, hold;                         /* laid out; blocks of a relayout's dip; the note touched; HOLD */
    uint16_t ew, tv;                                     /* the echo's next stage; the touch's velocity */
    uint32_t eclk, tt, rng;                              /* the echo's clock (Q16), the samples since the touch, noise */
    int32_t elp, e0, e1, y0, y1, tamp;                   /* the echo's input (low-passed), its last two taps; the output's
                                                          * last two samples; the touch's level (Q15) */
    int16_t lv[HV_N + 1u];                               /* the strings' and the echo's levels (the screen) */
    uint8_t nk, pl;                                      /* the keys held at the last block; the transport playing */
} hv_st_t;
#define HV_ES ((uint32_t)(sizeof drum_kit[0] - sizeof(hv_st_t)) / 2u)   /* the echo's stages (376) */
_Static_assert(sizeof(hv_st_t) % 4u == 0u && HV_ES >= 320u, "HIVE: its state and its echo in a kit row");
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

typedef struct {                                         /* a block's values (hv_prep) */
    int32_t g, a, ein, edir, regen, ek, tl, tk, a0, ast;
    int32_t xk[HV_N];
    uint32_t einc, rain;
    const uint8_t *F;                                    /* (the touch's row of HV_TF) */
    uint8_t form;
} hv_blk_t;

/* the four strings' periods, Q8 at 22 kHz, from the notes and PAIR */
static __attribute__((noinline)) void hv_periods(const track_t *t, const voice_t *v, const vmod_t *m, const hv_st_t *s, uint32_t *P)
{
    const int16_t *C = HV_PAIR[(uint32_t)clamp(t->p[P_E4], 0, 7)];
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
/* the rate and the layout for periods P (Q8 at 22 kHz; out: at the rate chosen, an octave up where none fits) */
static __attribute__((noinline)) void hv_layout(hv_st_t *s, uint32_t *P)
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
    if (s->lay && sh == s->sh && fit)
        return;
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
    s->lay = 1;
}
static void hv_touch(hv_st_t *s, uint32_t vel)
{
    s->tt = 0;
    s->tv = (uint16_t)vel;
    s->tamp = 0;
}
/* the block's values from the knobs (the matrix's SHAPE on CROSS, its CUT on BRIGHT) */
static __attribute__((noinline)) void hv_prep(const track_t *t, const voice_t *v, const vmod_t *m, hv_st_t *s, hv_blk_t *B)
{
    const int16_t *p = t->p;
    uint32_t P[HV_N], k, sh, f = (uint32_t)clamp(p[P_E0], 0, 127), lo = 0, x2;
    int32_t c = clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8), 0, 127), e = clamp(p[P_E2], 0, 127);
    int32_t br = clamp(p[P_E5] + (m->cutoff >> 8), 0, 127), x, tq, ts, lv;
    uint32_t tm = (uint32_t)clamp(p[P_E3], 0, 12), form = (uint32_t)clamp(p[P_E6], 0, 7), bt;
    const uint8_t *F = HV_TF[form];
    hv_periods(t, v, m, s, P);
    hv_layout(s, P);
    sh = s->sh;
    /* BRIGHT: the loop's one pole, its time constant tau 2^((64 - BRIGHT) / 16) samples at 44.1 kHz (64: the
     * prototype's 0.5), a = 1 / (1 + tau) at the strings' rate; its delay (tau) out of the length: in tune */
    x = 64 - br - 16 * (int32_t)sh;
    tq = (int32_t)HV_EXP16[x & 15];
    tq = x >= 0 ? tq << (x >> 4) : tq >> (-(x >> 4));
    B->a = (int32_t)((1u << 27) / (uint32_t)(4096 + tq));
    /* FEEDBK: the loop's gain, 0.90 .. 1 at 100 (a decay ever longer), over 1 above it (on by itself, ~1.024 at 127);
     * the key let go without HOLD: the strings let go too */
    B->g = f <= 100u ? 32768 - (int32_t)(((100u - f) * (100u - f) * 21475u) >> 16) : 32768 + (int32_t)(f - 100u) * 29;
    if (!v->gate && B->g > 32604)
        B->g = 32604;
    x2 = (uint32_t)(c * c) * 3932u >> 14;                /* CROSS: (c / 127)^2 x 12 % of the length, Q15 */
    for (k = 0; k < HV_N; k++) {
        int32_t T = (int32_t)P[k] - (tq >> 4), lim;
        T = T < (2 << 8) ? 2 << 8 : T;
        lim = ((int32_t)s->cap[k] - (s->cap[k] >> 3) - 3) << 8;
        if (T > lim) T = lim;
        if (!s->per[k])
            s->per[k] = T;
        else {                                           /* (a new note glides there: 3 % a block at most) */
            int32_t d = T - s->per[k], st = (s->per[k] >> 5) + 64;
            s->per[k] += d > st ? st : d < -st ? -st : d;
        }
        if (s->per[k] > lim) s->per[k] = lim;
        B->xk[k] = (int32_t)(((uint32_t)s->per[k] * x2) >> 15);
        if ((uint32_t)s->per[k] > lo) lo = (uint32_t)s->per[k];
    }
    /* ECHO: into the strings (x 0.06, the prototype's), heard a little, its own feedback; TIME its length (the echo's
     * clock: HV_ES stages over it), its input low-passed under the clock's half */
    B->ein = (e * 1982) >> 7;
    B->edir = e * 192;
    B->regen = 13107 + e * 60;
    bt = (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 300);   /* (a beat, samples at 44.1 kHz) */
    ts = tm < 7u ? (int32_t)(((uint32_t)FS >> sh) * HV_TMS[tm] / 1000u) : (int32_t)((bt >> sh) * HV_T16[tm - 7u] >> 2);
    ts = clamp(ts, (int32_t)HV_ES, 66150 >> sh);
    B->einc = (HV_ES << 16) / (uint32_t)ts;
    B->ek = clamp((int32_t)(B->einc * 3u >> 1), 64, 32767);
    /* TOUCH: a burst from the note (its length a share of the longest string), or on while held */
    lv = (int32_t)(s->tv * s->tv) * 258 >> 7;            /* (the velocity squared, Q15) */
    lv = (F[7] * lv >> 7) << 8;                          /* (Q23: a slow SWELL's steps) */
    B->F = F;
    B->form = (uint8_t)form;
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
/* the strings' samples: ni of them into Y (Q15). Three passes: the echo's taps (its stages read are older than any
 * block: HV_ES of its clock ticks, a tick at most a sample) and the touch, into each string's input; each pair of
 * strings over the block (a pair's two strings sample by sample: each reads its partner); then the echo's input */
#define HV_NI (CTL / 2u + 1u)
#define HV_SB 8u
/* one string over m samples: read (its length moved by its partner's outputs pm), damp, the gain and its input in,
 * saturate, write; its outputs into om (the four's state two loads a sub-block: no spills a sample) */
static __attribute__((noinline)) void hv_str(hv_st_t *s, int16_t *L, uint32_t k, const int16_t *pm, int16_t *om,
                                             const int16_t *in, uint32_t m, const hv_blk_t *B)
{
    int16_t *b = L + s->base[k];
    int32_t c = s->cap[k], w = s->w[k], l = s->lp[k], dc = s->dc[k] >> 8, p = s->per[k], x = B->xk[k], g = B->g, a = B->a;
    int32_t o = s->o[k], sum = 0, n = (int32_t)m;
    while (n > 0) {                                      /* (up to the line's end, then from its start: no wrap a sample) */
        int32_t e = c - w < n ? c - w : n, j;
        for (j = 0; j < e; j++) {
            int32_t r = ((w + j) << 8) - p - ((pm[j] * x) >> 15), i0, z;
            if (r < 0) r += c << 8;
            i0 = r >> 8;
            o = b[i0] + (((b[i0 + 1] - b[i0]) * (r & 255)) >> 8);   /* (b[c]: b[0] again) */
            om[j] = (int16_t)o;
            l += ((o - l) * a) >> 15;
            sum += l;
            z = (((l - dc) * g) >> 15) + in[j];
            b[w + j] = (int16_t)hv_sat(z);
        }
        pm += e; om += e; in += e; n -= e;
        w += e;
        if (w == c) w = 0;
        b[c] = b[0];
    }
    s->dc[k] += ((sum << 8) - s->dc[k] * (int32_t)m) >> 10;   /* (its DC a sub-block, Q8 finer: ~3 Hz) */
    s->w[k] = (uint16_t)w; s->lp[k] = l; s->o[k] = (int16_t)o;
}
static __attribute__((noinline)) void hv_run(hv_st_t *s, int16_t *L, int16_t *E, int32_t *Y, uint32_t ni, const hv_blk_t *B)
{
    uint32_t j, k, ew = s->ew, eclk = s->eclk, tt = s->tt, rng = s->rng, tch = (uint32_t)(s->tamp | B->tl | (int32_t)B->rain);
    int32_t e0 = s->e0, e1 = s->e1, tamp = s->tamp;
    int16_t eo[HV_NI], in[HV_N][HV_NI];
    int16_t mo[HV_N][HV_SB + HV_NI];                     /* (each string's outputs, after its last HV_SB of the block before) */
    uint32_t j0;
    for (j = 0; j < ni; j++) {                           /* the echo heard (eo) and into the strings */
        int32_t x = e0 + (((e1 - e0) * (int32_t)(eclk >> 1)) >> 15);
        eo[j] = (int16_t)x;
        in[0][j] = (int16_t)((x * B->ein) >> 15);
        eclk += B->einc;
        if (eclk >> 16) {
            eclk &= 0xFFFFu;
            e0 = e1;
            e1 = E[ew];
            ew = ew + 1u == HV_ES ? 0u : ew + 1u;
        }
    }
    if (tch) {                                           /* the touch: the bytebeat (or the noise) at its level, a string
                                                          * each its own (k + 3 x the string) */
        const uint8_t *F = B->F;
        uint32_t nz = !F[0];
        for (j = 0; j < ni; j++) {
            uint32_t x = nz ? rng : ((tt + j) << s->sh) * F[4] >> F[5];
            int32_t ta;
            if (B->rain && (rng >> 21) < B->rain)        /* (RAIN: a drop) */
                tamp = (int32_t)(3355443u * s->tv / 127u);
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
            hv_str(s, L, k, mo[k + 1u] + j0, mo[k] + HV_SB + j0, (tch ? in[k] : in[0]) + j0, m, B);
            hv_str(s, L, k + 1u, mo[k] + HV_SB + j0, mo[k + 1u] + HV_SB + j0, (tch ? in[k + 1u] : in[0]) + j0, m, B);
        }
    }
    {
        int32_t elp = s->elp;
        ew = s->ew;
        eclk = s->eclk;
        for (j = 0; j < ni; j++) {                       /* the echo: the mix x 0.45 in, its stages clocked */
            int32_t mix = mo[0][HV_SB + j] + mo[1][HV_SB + j] + mo[2][HV_SB + j] + mo[3][HV_SB + j];
            elp += ((((mix * 14746) >> 15) - elp) * B->ek) >> 15;
            eclk += B->einc;
            if (eclk >> 16) {
                eclk &= 0xFFFFu;
                E[ew] = (int16_t)(hv_sat((elp + ((E[ew] * B->regen) >> 15)) >> 1) * 2);   /* (saturating) */
                ew = ew + 1u == HV_ES ? 0u : ew + 1u;
            }
            mix = soft_knee(((mix * HV_GAIN) >> 15) + ((eo[j] * B->edir) >> 15), 24000);   /* the mix, the echo heard */
            Y[j] = mulq15(mix, (B->a0 + (int32_t)j * B->ast) >> 6);   /* (its level: the ramp at the sample, x VOICE_FS) */
        }
        s->elp = elp;
    }
    s->ew = (uint16_t)ew; s->eclk = eclk; s->tt = tt + ni; s->rng = rng;
    s->e0 = e0; s->e1 = e1; s->tamp = tamp;
    for (k = 0; k <= HV_N; k++) {                        /* (the screen: the strings' and the echo's levels, rough) */
        int32_t v = k < HV_N ? s->o[k] + L[s->base[k] + (s->cap[k] >> 1)] : e1 + e0;
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
    hv_run(s, comb_buf[tr], (int16_t *)(void *)(s + 1), Y, ni, &B);
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
    if (s->hold && ((!t->p[P_E7] && !t->nmono) || t->p[P_MUTE] || (s->pl && !song.playing)))
        drone_let(t, 255u);                              /* (HOLD off with no key down, MUTE, STOP: as DRONE's) */
    s->hold = t->p[P_E7] > 0;
    s->pl = song.playing;
}

static const preset_t HIVE_PRESETS[] = {
    /* name, {FEEDBK CROSS ECHO TIME, PAIR BRIGHT TOUCH HOLD}, {ATK DEC SUS REL}; MONO: the keys held make the pairs */
    {"DRONE QUINTA", {104, 18, 70, 4, 0, 64, HT_PLUCK, 1}, {8, 64, 127, 80}, 0, 1 | TAG(PAD), FX(0, 0, 0, 40), PAT(5)},
    {"BATIDO CRUZADO", {113, 40, 102, 3, 1, 64, HT_GRIT, 1}, {8, 64, 127, 80}, 0, 1 | TAG(PAD), FX(0, 0, 0, 35), PAT(5)},
    {"TOQUE + ECO", {102, 28, 45, 5, 2, 64, HT_PULSE, 1}, {8, 64, 127, 80}, 0, 1 | TAG(FX), FX(0, 0, 0, 40), PAT(5)},
    {"KOTO", {72, 6, 34, 8, 3, 92, HT_PLUCK, 0}, {0, 64, 127, 50}, 0, 1 | TAG(PLUCK), FX(0, 0, 20, 30), PAT(3)},
    {"ABISMO", {106, 14, 92, 6, 7, 22, HT_SWELL, 1}, {70, 64, 127, 100}, 0, 1 | TAG(PAD), FX(0, 0, 0, 50), PAT(8),
     MODS(P_LRATE, 4, MMOD(1, PS_LFO, PD_E(1), 18))},   /* a dark low drone, CROSS breathing */
    {"AULLIDO", {124, 100, 40, 7, 6, 110, HT_NOISE, 0}, {0, 64, 127, 30}, 0, 1 | TAG(LEAD), FX(10, 0, 30, 20), PAT(4),
     MODS(MMOD(1, PS_MODW, PD_E(1), 15), P_GLIDE, 12)},   /* the screaming cross-mod lead: MOD WHEEL howls */
    {"LLUVIA", {96, 20, 80, 9, 4, 80, HT_RAIN, 1}, {20, 64, 127, 90}, 0, 1 | TAG(FX), FX(0, 0, 0, 45), PAT(5)},
    {"ARCO", {98, 10, 50, 4, 3, 56, HT_BOW, 0}, {30, 64, 127, 70}, 0, 1 | TAG(PAD), FX(0, 10, 0, 40), PAT(5),
     MODS(P_LRATE, 10, MMOD(1, PS_LFO, PD_E(1), 12))},
};

static const engine_t ENG_HIVE = {
    .name = "HIVE",
    .page_title = {"HIVE", "TOUCH"},
    .edit = {
        {"FEEDBK", F_PCT, 0, 127, 104, 0},
        {"CROSS", F_PCT, 0, 127, 18, 0},
        {"ECHO", F_PCT, 0, 127, 70, 0},
        {"TIME", F_ENUM, 0, 12, 4, N_HVTIME},
        {"PAIR", F_ENUM, 0, 7, 0, N_HVPAIR},
        {"BRIGHT", F_PCT, 0, 127, 64, 0},
        {"TOUCH", F_ENUM, 0, 7, 0, N_HVTOUCH},
        {"HOLD", F_ENUM, 0, 1, 1, N_ONOFF},
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
