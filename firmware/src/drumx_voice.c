/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): DRUM-X, after the idea of Sonic Charge's Microtonic (a tuned oscillator with a pitch
 * envelope and FM, a filtered noise generator, their envelopes, two patches and a MORPH between them), built anew
 * small and in fixed point for the FM-1. */
/* A DRUM-X sound (a lane of a kit) is dx_lane_t, 11 bytes: the mode and five values for each of its patches A and B:
 *   PITCH  the oscillator, MIDI 24..119 (33 Hz .. 7.9 kHz), 3/4 semitone a step
 *   PMOD   the pitch envelope: up to +4 octaves at the hit, falling with a sixth of the decay's tau (1..40 ms)
 *   DECAY  the amplitude's tau, 3 ms .. 1.5 s (exponential)
 *   NOISE  the mix: 0 the oscillator only .. 127 the noise only
 *   COLOR  the oscillator's FM index (SINE: its drive) and the noise filter's cutoff (30 Hz .. 16 kHz)
 * The mode: the oscillator (SINE, FM at 1:1, METAL at 1:1.47, BELL at 1:2.76), the noise filter (LP, BP, HP), the
 * noise's envelope (as the oscillator's, or SNAP: a quarter of its tau); (JIANT 0.4, the mode's free bits 5..7) the
 * pitch modulation, as Microtonic's: DECAY (the envelope above), LONG (its tau half the decay's, up to 250 ms: the
 * 808 boom), NOISE (a random pitch each block, scaled by the envelope: glitch, grit), SINE (a sine at 1/8 of the
 * pitch, scaled by the envelope: wobble, laser); and DRIVE (the oscillator x3 into a soft clip: dense kicks, fat toms).
 * MORPH (0 A .. 127 B) mixes the two patches value by value, read every block: it may move while a hit rings.
 * Envelopes per block (Q30 values, Q16 factors, dx_fall), ramped linearly inside it; the pitch ramps too. A hit
 * that has rung out (-84 dB) stops computing (live 0) and writes zeros. Only 32-bit products. */
enum { DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_RIM, DV_BELL, DV_NLANE };   /* the kit's lanes */
enum { DXW_SINE, DXW_FM, DXW_METAL, DXW_BELL };
enum { DXF_LP, DXF_BP, DXF_HP, DXF_GR };                 /* (V10.7.8) GR: the guiro's scrape (dx_run_fx) */
#define DX_MODE(w, f, snap) ((uint8_t)((w) | (f) << 2 | (snap) << 4))
enum { DXM_DECAY, DXM_LONG, DXM_NOISE, DXM_SINE };       /* (JIANT 0.4) the pitch modulation: mode bits 5..6 */
#define DX_DRIVE 0x80u                                   /* .. the oscillator driven: bit 7 */
enum { DXP_PITCH, DXP_PMOD, DXP_DECAY, DXP_NOISE, DXP_COLOR, DXP_N };
typedef struct {
    uint8_t mode;
    uint8_t a[DXP_N], b[DXP_N];                          /* 0..127 */
} dx_lane_t;
typedef struct {                                         /* a hit */
    uint32_t ph, ph2, inc;                               /* the phases; the last block's end increment */
    int32_t ea, en, ep;                                  /* amplitude, noise (Q30), pitch (Q15) envelopes */
    int32_t ic1, ic2, rng;                               /* the noise filter's state, the noise */
    int32_t mo;                                          /* WARP: the modulator's last output (its feedback) */
    uint32_t ph3;                                        /* (JIANT 0.4) the SINE pitch modulation's phase */
    int32_t hl;                                          /* (0.6.17) the last half-rate sample (dx_run's upsampling) */
    int32_t b1, b2;                                      /* (0.6.36) TONE's band-pass state */
    uint32_t ph4;                                        /* (0.6.49) LAYER's oscillator phase */
    uint8_t live, trig, choke;
    uint8_t sv;                                          /* (V10.7.8) the hit's velocity (eng_drum.c; 0 a full one): GR's
                                                          * stroke, long or short (in the struct's padding) */
    uint16_t hpos, hsum, hseed;
    uint32_t fturn;                                      /* (JIANT BYTE) a formula lane's whole turns since the hit (w) */
    int32_t fst[2];                                      /* .. its formula's states (ph lp hp) */
    uint32_t dwage;                                      /* (DRUM-W) half-rate samples since the hit (the scan, the tick) */
    int32_t dwn;                                         /* (DRUM-W) the noise colour's state */
    int16_t mo1;                                         /* (0.9.9-snd1) FEEDBK: the output a sample before mo (the mean
                                                          * of the two fed back; in the struct's padding) */                          /* (0.7.9) CLAP's HANDS: half-rate samples since the hit, the
                                                          * hands come in so far (Q12, over the hit's own), this hit's dice */
} dx_voice_t;


/* ---- DRUM-W: a drum machine on wavetables. Each pad two tables of WAVE's bank (f(w, A), drawn in the main loop by
 * dw_poll as 4 frames of 32 points, A = 0 .. 1: all sixteen, 2.1 KB of the pool), A the body and B the layer at its
 * RATIO of the pitch, joined by a STRUCTURE (MIX FM RING SYNC FOLD: B's AMOUNT); each scanned from its POS, moved by
 * the pad's own modulators: MOD ENV (ATTACK DECAY AMOUNT) and MOD LFO (RATE SHAPE DEPTH, from the hit), each to a DEST
 * (POS A, POS B, both, B's AMOUNT, PITCH, the NOISE's colour), and the velocity (VEL: a soft hit elsewhere). The two frames a table is between mixed once a
 * block, one interpolated read a sample a table (half rate). Then the noise (its colour WHITE PINK BROWN VIOLET RESON,
 * through DRUM-X's filter), a TICK on the attack, DRUM-X's envelopes, pitch envelope, A / B patches and their MORPH */
static uint32_t dx_pow2(uint32_t x);
#define DW_N 32u                                         /* a frame's points (+1: the first again) */
#define DW_FR 4u                                         /* frames a table, A = 0, 1/3, 2/3, 1 */
typedef struct {
    uint8_t ta, tb, pos, bpos;                           /* A's table, B's; their places (0..127) */
    uint8_t str, ratio, bamt;                            /* the STRUCTURE, B's RATIO, its AMOUNT */
    uint8_t eatk, edec, eamt, edst;                      /* MOD ENV: ATTACK, DECAY, AMOUNT (64 none), its DEST */
    uint8_t lrate, lshp, ldep, ldst;                     /* MOD LFO: RATE (0 off), SHAPE, DEPTH (64 none), its DEST */
    uint8_t vel;                                         /* VEL: a soft hit elsewhere in the tables (64 none) */
    uint8_t noise, tick, tlev, gain;                     /* the noise's colour, the TICK and its level; the pad's trim (64 x1,
                                                          * 0 as 64: the factory's balance, LEVELS on it) */
} dw_lane_t;
enum { DWS_MIX, DWS_FM, DWS_RING, DWS_SYNC, DWS_FOLD, DWS_N };
enum { DWD_POSA, DWD_POSB, DWD_POS, DWD_BAMT, DWD_PITCH, DWD_NOISE, DWD_N };   /* a modulator's DEST */
enum { DWL_TRI, DWL_SINE, DWL_SQR, DWL_RAMP, DWL_SH, DWL_N };
enum { DWN_WHITE, DWN_PINK, DWN_BROWN, DWN_VIOLET, DWN_RESON };
enum { DWT_OFF, DWT_PULSE, DWT_SAW, DWT_RECT, DWT_BURST, DWT_HF, DWT_CLICK };
#define DW_NRAT 12u
static const char *const N_DWS[] = {"MIX", "FM", "RING", "SYNC", "FOLD", 0};
static const char *const N_DWD[] = {"POS A", "POS B", "POS AB", "B AMT", "PITCH", "NOISE", 0};
static const char *const N_DWN[] = {"WHITE", "PINK", "BROWN", "VIOLET", "RESON", 0};
static const char *const N_DWR[] = {"1/2", "1", "1.41", "1.5", "2", "2.76", "3", "3.5", "4", "5.4", "7", "8", 0};
static const uint16_t DW_RAT[DW_NRAT] = {2048, 4096, 5793, 6144, 8192, 11305, 12288, 14336, 16384, 22118, 28672, 32768};   /* Q12 */
/* the factory kit's pads: KICK SNARE CLAP HAT C HAT O TOM RIM BELL
 *  {A, B, POS, B POS, STRUCT, RATIO, AMOUNT, ENV: ATK DEC AMT DEST, LFO: RATE SHAPE DEPTH DEST, VEL, NOISE, TICK, LEVEL} */
#define DW_DEF_KIT { \
    {1, 0, 0, 0, DWS_FM, 0, 24, 0, 14, 108, DWD_BAMT, 0, DWL_TRI, 64, DWD_POSA, 72, DWN_BROWN, DWT_CLICK, 40, 92},     /* KICK: a sine, a sub-octave FM struck that dies in the first beat */ \
    {2, 13, 2, 60, DWS_MIX, 4, 50, 0, 40, 98, DWD_POS, 0, DWL_TRI, 64, DWD_POSA, 84, DWN_WHITE, DWT_BURST, 63, 70},   /* SNARE: FMR down, a NOISE cycle */ \
    {13, 11, 16, 60, DWS_RING, 5, 70, 0, 30, 88, DWD_POSA, 90, DWL_SH, 76, DWD_POSB, 64, DWN_PINK, DWT_OFF, 0, 110},  /* CLAP: NOISE ringed by BITS */ \
    {11, 13, 127, 100, DWS_RING, 10, 90, 0, 12, 48, DWD_POS, 0, DWL_TRI, 64, DWD_POSA, 90, DWN_VIOLET, DWT_HF, 47, 64},   /* HAT C: BITS x NOISE */ \
    {11, 9, 122, 90, DWS_RING, 9, 80, 10, 70, 58, DWD_POS, 70, DWL_TRI, 72, DWD_POSB, 90, DWN_VIOLET, DWT_HF, 31, 64}, /* HAT O */ \
    {1, 0, 0, 0, DWS_MIX, 0, 0, 0, 20, 80, DWD_POSA, 0, DWL_TRI, 64, DWD_POSA, 72, DWN_BROWN, DWT_CLICK, 31, 104},    /* TOM: FM2 to the sine */ \
    {9, 6, 110, 70, DWS_MIX, 6, 40, 0, 12, 64, DWD_POSA, 0, DWL_TRI, 64, DWD_POSA, 80, DWN_RESON, DWT_PULSE, 47, 64},  /* RIM: a narrow PULSE, a RING */ \
    {0, 2, 0, 50, DWS_FM, 5, 70, 0, 60, 30, DWD_BAMT, 40, DWL_SINE, 70, DWD_POSB, 72, DWN_WHITE, DWT_RECT, 31, 48},  /* BELL: a sine, FMR's FM at 2.76 */ \
}
static const dw_lane_t DW_DEF[8] = DW_DEF_KIT;
static dw_lane_t dw_kit[8] = DW_DEF_KIT;   /* (the music's: the factory kit's from the start; a kit or a project sets it) */
static void dw_sane(dw_lane_t *d)                        /* (a stored pad in range: no value indexes past a name) */
{
    uint8_t *u = (uint8_t *)d;
    uint32_t k;
    for (k = 0; k < sizeof *d; k++) u[k] &= 127u;
    d->ta &= 15u; d->tb &= 15u; d->str %= DWS_N; d->ratio %= DW_NRAT; d->edst %= DWD_N; d->ldst %= DWD_N; d->lshp %= DWL_N;
    if (d->noise > DWN_RESON) d->noise = 0;
    if (d->tick > DWT_CLICK) d->tick = 0;
}
static int8_t dw_tab[16][DW_FR][DW_N + 1u] __attribute__((section(".pool")));   /* (2.1 KB of the pool: every table of the bank) */
static volatile uint16_t dw_have;                        /* bit f: table f drawn (dw_poll; 0 while it draws) */
static int32_t dw_lcyc[2][DW_N];                         /* (LIFE) each core's last pad cycle (dw_body) */
static volatile uint16_t dw_live[8];                     /* (the panel) each pad's place now, 0 .. 768 (+ 1 << 15: sounding) */
/* the main loop (project.c dw_poll): a table not drawn yet, the kit's first (one a call) */
static void dw_poll_ws(gq_work_t *w)
{
    uint32_t want = 0, l, k, f, i, j;
    for (l = 0; l < 8u; l++)
        want |= 1u << (dw_kit[l].ta & 15u) | 1u << (dw_kit[l].tb & 15u);
    want &= ~(uint32_t)dw_have;
    if (!want)
        want = 0xFFFFu & ~(uint32_t)dw_have;              /* (then the rest: a table browsed is there at once) */
    for (f = 0; f < 16u && !((want >> f) & 1u); f++)
        ;
    if (f == 16u)
        return;
    {
        const char *tx = WAVE_BANK[f];
        uint32_t n = 0, W[FVM_BLK], T[FVM_BLK] = {0}, y[FVM_BLK];
        fvm_env_t e = {{0}, {0}, w->st};
        while (tx[n]) n++;
        if (fvm_compile(&w->cc, FVM_FULL(&w->pg), tx, n, 0) || w->pg.p.nstate > 1u) {
            dw_have |= (uint16_t)(1u << f);              /* (never: the bank compiles; a sine stays there) */
            return;
        }
        e.arr[FA_T] = T; e.arr[FA_W] = W;
        for (j = 0; j < DW_FR; j++) {
            e.u[FU_AF] = j * 127u / (DW_FR - 1u) * 516u;
            for (k = 0; k < DW_N; k += FVM_BLK) {
                for (i = 0; i < FVM_BLK; i++) W[i] = (k + i) * (65536u / DW_N);
                fvm_run(&w->pg.p, &e, &w->sc, FVM_BLK, y);
                for (i = 0; i < FVM_BLK && k + i < DW_N; i++)
                    dw_tab[f][j][k + i] = (int8_t)clamp((int32_t)y[i] >> 9, -127, 127);
            }
            dw_tab[f][j][DW_N] = dw_tab[f][j][0];
        }
    }
    dw_have |= (uint16_t)(1u << f);
}
static uint32_t dw_time(uint32_t t) { return 64u + ((dx_pow2(t * 9u * 4096u / 127u) >> 8) * 22u >> 4); }   /* (half-rate samples:
                                                          * 3 ms .. ~0.9 s) */
static int32_t dw_tick(uint32_t ty, uint32_t k)          /* the TICK's k-th half-rate sample (0 past it) */
{
    int32_t e = 64 - (int32_t)k;
    if (e <= 0) return 0;
    switch (ty) {
    case DWT_PULSE: return k < 2u ? 32767 : 0;
    case DWT_SAW: return (e * 1024 - 32768) * e >> 6;
    case DWT_RECT: return k < 24u ? ((k & 4u) ? 20000 : -20000) : 0;
    case DWT_BURST: return (int32_t)((fvm_hash(k * 977u + 13u) >> 16) - 32768u) * e >> 6;
    case DWT_HF: return sine_i(k * 0x2C000000u) * e >> 6;
    case DWT_CLICK: return k < 32u ? sine_i(k * 0x0C000000u) * (32 - (int32_t)k) >> 5 : 0;
    }
    return 0;
}
/* x octaves (x / 4096 of them, below 16) as a Q16 factor 2^(x / 4096): the whole octaves a shift of 1.0, the rest
 * t (0 .. 1, the low 12 bits) a third-order polynomial 1 + c1 t + c2 t^2 + c3 t^3, its coefficients Q16 (c1 0.6957,
 * c2 0.2266, c3 0.0777: off by 0.01 % at most), taken as ((c3 t + c2) t + c1) t + 1 */
static __attribute__((noinline)) uint32_t dx_pow2(uint32_t x)
{
    uint32_t t = x & 0xfffu, acc;
    acc = 14851u + ((t * 5092u) >> 12);
    acc = 45594u + ((t * acc) >> 12);
    acc = 65536u + ((t * acc) >> 12);
    return acc << (x >> 12);
}
/* what an envelope with a time constant of `us` microseconds keeps over one block, Q16: e^-a with a = CTL / (FS tau)
 * (in Q16: 95108934 / us for a block of 64 at 44.1 kHz), from the series of e^-a up to its a^4 / 24 term, each
 * power truncated to Q16 and each term rounded down on its own; a held at 0.70 (46000), where the fourth order still
 * falls, so a tau under ~1.4 blocks loses half its level each block; 1.0 written as 65535 */
#define DX_BLOCK_A (CTL * 65536.0e6 / FS)
static uint32_t dx_fall(uint32_t us)
{
    uint32_t a = (uint32_t)DX_BLOCK_A / (us | 1u), pw, keep;
    if (a > 46000u)
        a = 46000u;
    pw = (a * a) >> 16;
    keep = 65536u - a + pw / 2u;
    pw = (a * pw) >> 16;
    keep -= pw / 6u;
    pw = (a * pw) >> 16;
    keep += pw / 24u;
    return keep < 65536u ? keep : 65535u;
}

static void dx_trigger(dx_voice_t *v) { v->trig = 1; v->choke = 0; }
static void dx_choke(dx_voice_t *v) { if (v->live) v->choke = 1; }

/* a decay value (0..127) -> its tau in us: 3 ms x 2^(9 d / 127) */
static uint32_t dx_tau(uint32_t d) { return ((dx_pow2(d * 9u * 4096u / 127u) >> 8) * 3000u) >> 8; }

/* (DRUM-W) table f's frame at place sp (0 .. 768: the four frames' span), into m[DW_N + 1]: once a block */
static void dw_frame(int16_t *m, uint32_t f, int32_t sp)
{
    uint32_t k, i = (uint32_t)sp >> 8, fr = (uint32_t)sp & 255u;
    const int8_t *a, *b;
    if (!((dw_have >> f) & 1u)) {                        /* (not drawn yet, a ms after the power-on: a sine) */
        for (k = 0; k <= DW_N; k++)
            m[k] = (int16_t)((sine_i(k << 27) * 31) >> 5);
        return;
    }
    if (i >= DW_FR - 1u) { i = DW_FR - 2u; fr = 256u; }
    a = dw_tab[f][i];
    b = dw_tab[f][i + 1u];
    for (k = 0; k <= DW_N; k++)
        m[k] = (int16_t)(a[k] * 256 + (b[k] - a[k]) * (int32_t)fr);
}
#define DW_RD(m, p) ((m)[(p) >> 27] + ((((m)[((p) >> 27) + 1u] - (m)[(p) >> 27]) * (int32_t)(((p) >> 12) & 0x7FFFu)) >> 15))
/* (DRUM-W) the pad's modulators, once a block at the hit's age: MOD ENV, MOD LFO and VEL, Q15 each, summed by DEST
 * into o[DWD_N]; (DRUM's own) STRIKE (MOD ENV's knob) and WAVE (TONE's) move both places too */
static __attribute__((noinline)) void dw_mods(dx_voice_t *v, uint32_t ln, uint32_t n, int32_t fm, int32_t cofs, int32_t *o)
{
    const dw_lane_t *D = &dw_kit[ln];
    uint32_t age = v->dwage, ta = D->eatk ? dw_time(D->eatk) : 0u, td = dw_time(D->edec), k;
    int32_t e, sv = v->sv ? v->sv : 127, a[DWD_N] = {0};
    if (D->eamt != 64u) {                                /* the ENV: up over ATTACK, down over DECAY (squared) */
        if (age < ta)
            e = (int32_t)((age * (0x7FFFFFFFu / ta)) >> 16);   /* (age < ta: fits) */
        else {
            uint32_t x = age - ta >= td ? 65535u : ((age - ta) * (0xFFFFFFFFu / td)) >> 16;
            e = (int32_t)(((65535u - x) * (65535u - x)) >> 17);
        }
        a[D->edst % DWD_N] += (e * ((int32_t)D->eamt - 64)) >> 6;
    }
    if (D->lrate) {                                      /* the LFO from the hit, 0.27 .. 62 Hz */
        uint32_t ph = v->ph4;
        int32_t l;
        switch (D->lshp) {
        case DWL_SINE: l = sine_i(ph); break;
        case DWL_SQR: l = ph < 0x80000000u ? 32767 : -32767; break;
        case DWL_RAMP: l = (int32_t)(ph >> 16) - 32768; break;
        case DWL_SH: l = (int32_t)(fvm_hash((ph >> 29) * 977u + v->hseed) >> 17) - 32768; break;   /* (8 steps a turn) */
        default: l = (int32_t)(ph >> 15); l = l < 65536 ? l - 32768 : 98303 - l; break;
        }
        a[D->ldst % DWD_N] += (l * ((int32_t)D->ldep - 64)) >> 6;
        v->ph4 = ph + (pitch_inc((uint32_t)D->lrate * 12u) >> 5) * 2u * n;
    }
    a[DWD_POS] += (127 - sv) * ((int32_t)D->vel - 64) * 4;   /* VEL */
    for (k = 0; k < DWD_N; k++) a[k] = clamp(a[k], -32767, 32767);
    o[0] = (((a[DWD_POSA] + a[DWD_POS]) * 768) >> 15);
    o[1] = (((a[DWD_POSB] + a[DWD_POS]) * 768) >> 15);
    o[2] = (a[DWD_BAMT] * 127) >> 15;
    o[3] = (a[DWD_PITCH] * 192) >> 15;                   /* (1/16 semitones: +-12) */
    o[4] = (a[DWD_NOISE] * 64) >> 15;
    e = ((clamp(fm, 0, 127) * 6 * v->ep) >> 15) + clamp(cofs, -64, 63) * 6;
    o[0] += e;
    o[1] += e;
}
/* (DRUM-W) a pad's body, n half-rate samples into y: its places this block (POS, B POS and the modulators' oa, ob),
 * the frames there, then A and B by the STRUCTURE (B's AMOUNT and gofs), at the oscillator's gain (ga + dga, Q23) */
static __attribute__((noinline)) void dw_body(dx_voice_t *v, uint32_t ln, int32_t *y, uint32_t n, int32_t inc, int32_t dinc,
                                              int32_t ga, int32_t dga, const int32_t *mo)
{
    const dw_lane_t *D = &dw_kit[ln];
    uint32_t ph = v->ph, ph2 = v->ph2, i, st, stb, rq = DW_RAT[D->ratio % DW_NRAT];
    int32_t top = (int32_t)(DW_FR - 1u) * 256, sa, sb, g = clamp((int32_t)D->bamt + mo[2], 0, 127) * 258, dstb;
    int16_t ma[DW_N + 1u], mb[DW_N + 1u];
    sa = clamp(D->pos * top / 127 + mo[0], 0, top);
    sb = clamp(D->bpos * top / 127 + mo[1], 0, top);
    dw_live[ln] = (uint16_t)(sa | 0x8000);
    dw_frame(ma, D->ta & 15u, sa);
    if (fl.on)                                           /* (LIFE: the cycle playing, its FRAGMENT) */
        for (i = 0; i < DW_N; i++) dw_lcyc[MC_CPU() & 1u][i] = ma[i];
    st = (uint32_t)inc << 4;
    if (!g) {                                            /* (B out: A alone) */
        for (i = 0; i < n; i++, inc += dinc, ga += dga) {
            y[i] = mulq15(DW_RD(ma, ph), ga >> 8);
            ph += (uint32_t)inc << 4;
        }
        v->ph = ph;
        return;
    }
    dw_frame(mb, D->tb & 15u, sb);
    stb = (uint32_t)(((uint64_t)st * rq) >> 12);         /* B at its RATIO, its step ramped as A's */
    dstb = ((int32_t)(((uint64_t)((uint32_t)(inc + dinc * (int32_t)n) << 4) * rq) >> 12) - (int32_t)stb) / (int32_t)n;
#define DW_LOOP(BODY) \
    for (i = 0; i < n; i++, inc += dinc, ga += dga, stb += (uint32_t)dstb) { \
        int32_t b = DW_RD(mb, ph2), a, o; \
        uint32_t p1 = ph + ((uint32_t)inc << 4); \
        BODY \
        y[i] = mulq15(clamp(o, -32767, 32767), ga >> 8); \
        ph = p1; \
        ph2 += stb; \
    }
    switch (D->str) {                                    /* (a loop a STRUCT: no choice a sample) */
    case DWS_FM:                                         /* B on A's phase, up to a turn */
        DW_LOOP(o = DW_RD(ma, ph + ((uint32_t)mulq15(b, g) << 17)); (void)a;)
        break;
    case DWS_RING:                                       /* (the ring x2: as loud as A) */
        DW_LOOP(a = DW_RD(ma, ph); o = a + mulq15(clamp(mulq15(a, b) * 2, -32767, 32767) - a, g);)
        break;
    case DWS_SYNC:                                       /* B restarted with each of A's turns */
        DW_LOOP(a = DW_RD(ma, ph); o = a + mulq15(b - a, g); if (p1 < ph) ph2 = 0u - stb;)
        break;
    case DWS_FOLD:                                       /* A + B, folded at +-T */
        DW_LOOP(a = clamp(DW_RD(ma, ph) + mulq15(b, g) * 2, -5 * 16384, 5 * 16384);
                a = a > 16384 ? 32768 - a : a < -16384 ? -32768 - a : a;
                a = a > 16384 ? 32768 - a : a < -16384 ? -32768 - a : a;
                o = a * 2;)
        break;
    default:
        DW_LOOP(o = DW_RD(ma, ph) + mulq15(b, g); (void)a;)
        break;
    }
#undef DW_LOOP
    v->ph = ph;
    v->ph2 = ph2;
}
/* one block of lane L's hit v into y (n <= CTL): Q15, peaks near full scale. morph 0..127; tune16 1/16 semitones;
 * dofs / cofs / nofs move DECAY / COLOR / NOISE (-64..63). warp (DRUM's FOLD since JIANT 0.5, was WARP, 0..127): the
 * oscillator through a wavefolder after its envelope, x1 .. x9 into three folds at +-T: the hit's loud start folds
 * into bright partials, its tail unfolds back to the plain wave (a West Coast drum); 0: the patch as it is.
 * fm (DRUM's FM ENV, JIANT 0.6.36; was FM, 0..127): how much FM the hit strikes with, its index following the pitch
 * envelope (1..40 ms) down to the lane's own: an attack's beater / click / metal edge, the tail as the kit has it
 * (the old FM, a fixed harmonic FM over the whole hit, muddied every sound). The oscillator and the noise each run their own loop (the wave and the
 * filter picked once a block), skipped when the mix leaves them out; gains and the pitch step per sample */
/* (JIANT 0.6.36) EDIT > TONE, the kit's (project.c keeps it in the kit's reserved byte 7: 0 in older projects, off):
 * low nibble BP, a band-pass on the metal sounds (the hats, the rim, the cowbell), 1..15 its centre ~1.5 .. 9 kHz;
 * high nibble FEEDBK, every sound's oscillator fed back into its own phase (1..15: warm, growl, grit, noise) */
static uint8_t dx_tone;
/* (JIANT 0.7.9) KIT 3 > HANDS, the kit's (project.c keeps it in the high nibble of the kit's reserved byte 0, the
 * mute groups' low nibble: 0 in older projects, off): 1..15 more hands on the CLAP, each the hit's noise struck again
 * at a moment of its own (~3 .. 45 ms after it, the dice thrown again each hit: no tempo in it) and at a level of its
 * own, then dying as the hit does: a few people clapping, then a crowd. 0: the clap as it was */
static uint8_t dx_hands;
static dx_lane_t dx_kit[8] __attribute__((section(".data.dx_kit")));                              /* (the music's kit: DX_KITS below; V10.6o: no initializer, the
                                                          * power-on and a project set it) */
/* (JIANT 0.6.49) EDIT > LAYER, each sound's (the kit's: project.c keeps it, FUNC's 'D' 'L' block; 0 before it): low
 * nibble LAYER, a second oscillator (a triangle) at sqrt 2 of the pitch (the most out of tune with it there is) frequency-modulated
 * by the first, more as it goes up (1..15: a metal edge .. a clang with no note: the sound melts into the rhythm and
 * stays out of the harmony); high nibble FEEDBK, the sound's own oscillator fed back (on top of TONE's, the kit's) */
static uint8_t dx_lay[8] __attribute__((section(".data.dx_lay")));                                /* (V10.6o: no initializer; the power-on and a project set it: the
                                                          * factory kit's, DX_LAY_DEF) */
/* (JIANT BYTE, phase 4) a formula lane: dx_form[lane] 0 the oscillator as DRUM-X's, 1..DX_NFORM a formula of DX_FORMS for
 * it (SOUND 3's MODE past its 32: ui_dx.c), its time from 0 at each hit: w the turns at the lane's pitch (PITCH, PMOD's
 * sweep on it), t 32 a turn, T the seconds since the hit, A COLOR (0 .. 1), v the hit's velocity. The rest of the lane as
 * ever: DECAY's envelope, the noise and its filter, MORPH between A and B, the SENDs, the group mutes. Stored in the
 * project's 'D' 'F' block (project.c), 0 before. Each lane's program compiled when its formula changes (dx_fprog) */
static const char *const N_DXFORM[] = {"SINE", "SAT", "FM", "METAL", "BITS", "XOR", "NOISE", "CHIRP", "STEP", "RING",
                                       "STUT", "FOLD", "CRUSH", "TONE2", "HAT", "BYTE",
                                       "FKICK", "FSNR", "FCLAP", "FHAT", "FOHAT", "FTOM", "FRIM", "FCOW"};
#define DX_NFORM (sizeof N_DXFORM / sizeof N_DXFORM[0])
enum { DFM_RATE, DFM_BEND, DFM_CRUSH, DFM_N };
/* Per-lane formula modifiers. Zero is an exact bypass, including old projects. */
static int8_t dx_fmod[8][DFM_N];
#define DX_USER 255u
#define DX_TEXT 64u
static char dx_text[8][DX_TEXT] __attribute__((section(".pool")));
static uint8_t dx_form[8];                           /* (JIANT BYTE) 0, or 1 + the formula of DX_FORMS */
/* (JIANT BYTE) the factory: every lane its floatbeat (KICK SNARE CLAP HATC HATO TOM RIM BELL: FKICK .. FCOW) */
static const uint8_t DX_FORM_DEF[8] = {0};   /* (DRUM-W: none; its tables are the formulas) */
static uint8_t dx_fid[8];                            /* the formula compiled in dx_fprg (0: none yet) */
#define DX_LAY_L(x) ((uint32_t)(x) & 15u)
#define DX_LAY_FB(x) ((uint32_t)(x) >> 4)
#define DX_TONE_BP(t) ((uint32_t)(t) & 15u)
#define DX_TONE_FB(t) ((uint32_t)(t) >> 4)
/* (0.7.9) CLAP's HANDS, once a half-rate block (~0.7 ms: each hand's onset on its block, its rise the block's ramp):
 * the noise's gain at the block's start and end, gn and *g1 (Q23), times 1 + the hands in. Hand k strikes at its own
 * moment (this hit's dice, v->hseed, 3 .. 45 ms), at its own level, then decays as the hit: in the sum it is the hit's
 * envelope times its level over where that envelope was when it struck, so one sum carries them all (v->hsum, Q12) */
static __attribute__((noinline)) int32_t dx_hands_step(dx_voice_t *v, uint32_t n, int32_t gn, int32_t *g1, int32_t en1)
{
    uint32_t k, nh = dx_hands ? 1u + (uint32_t)dx_hands * 5u / 15u : 0u, span = 60u + (uint32_t)dx_hands * 60u, p0 = v->hpos;
    int32_t s0 = v->hsum, s = s0, w = 2 * 23 / (int32_t)(2u * nh + 1u);   /* (V10.6q: the more hands the softer each: the
                                                          * crowd about as loud as a few, was +10.6 dB at 15) */
    if (p0 < 768u) {                                     /* (V10.6q) the 808's: three claps 11.6 ms apart, each a falling
                                                          * saw (squared) on the noise, then the tail */
        int32_t a = 256 - (int32_t)(p0 & 255u), b = 255 - (int32_t)((p0 + n - 1u) & 255u);
        gn = (gn >> 16) * a * a;
        if (p0 + n < 768u)
            *g1 = (*g1 >> 16) * b * b;
    }
    for (k = 0; k < nh && p0 < 1200u; k++) {             /* (all in by ~54 ms: then the sum alone) */
        uint32_t h = ((uint32_t)v->hseed + k * 0x9E37u) * 2654435761u, on = 66u + (h >> 8) % span;
        if (on >= p0 && on < p0 + n) {
            int32_t f = (int32_t)((1u << 30) / (uint32_t)((en1 >> 12) | 1));   /* 1 / the envelope there, Q12 */
            if (f > 16 << 12) f = 16 << 12;
            s += (((int32_t)(1200u + ((h >> 20) & 2047u)) * w / 23) * f) >> 12;   /* its level, ~0.3 .. 0.8 */
        }
    }
    s = clamp(s, 0, 12 << 12);
    v->hsum = (uint16_t)(s > 65535 ? 65535 : s);
    v->hpos = (uint16_t)(p0 + n < 65535u ? p0 + n : 65535u);
    *g1 = (int32_t)(mul64(*g1, 4096 + s) >> 12);
    return (int32_t)(mul64(gn, 4096 + s0) >> 12);
}
static __attribute__((noinline)) void dx_run_fx(const dx_lane_t *L, dx_voice_t *v, int32_t morph, int32_t tune16,
                                             int32_t dofs, int32_t cofs, int32_t nofs, int32_t warp, int32_t fm, int32_t *y,
                                             uint32_t n, uint32_t feedback)
{
    int32_t life_color=0,q[DXP_N], mo[5], p16, depth, ea0, ea1, en1, ep1, og, ng, ga, dga, gn, dgn, kd, inc, dinc, mt, cx;
    uint32_t i, k, flt = (L->mode >> 2) & 3u, ta, tn, tp, kb, inc1;
    tsvf_t c;
    uint32_t nf = n, ln = L >= dx_kit && L < dx_kit + 8 ? (uint32_t)(L - dx_kit) : 0u;
    (void)feedback;
    if (flt == DXF_GR) flt = DXF_BP;                     /* (DRUM-W: no guiro; its filter a band) */
    if (!v->live && !v->trig) {
        for (i = 0; i < n; i++)
            y[i] = 0;
        dw_live[ln] &= 0x7FFFu;                          /* (DRUM-W: the panel's pad unlit) */
        return;
    }
    if (nf < 2u) {                                       /* (ctl64) one sample (a hit starting at a block's last sample,
                                                          * voice.c track_render): the last one held, nothing moves */
        y[0] = v->hl;
        return;
    }
    n >>= 1;                                             /* (0.6.17) at half rate (22 kHz); the block back to full rate
                                                          * at the end, linearly (half the work of the heaviest lanes).
                                                          * (V10.6q) no one-pole after it: at 22 kHz its "8 kHz" was a
                                                          * 1.8 dB shelf, 12 % of a hit's work */
    morph = clamp(morph, 0, 127);
    for (k = 0; k < DXP_N; k++)
        q[k] = L->a[k] + ((L->b[k] - L->a[k]) * morph) / 127;
    q[DXP_DECAY] = clamp(q[DXP_DECAY] + dofs, 0, 127);
    for (k = 0; k < 2u; k++) {                           /* (V10.6q) COLOR, NOISE: the offset as it is where it fits, scaled
                                                          * to the room where it would clip (was clipped: half a knob dead
                                                          * on a sound near an end); DECAY stays in octaves (SHORT: half) */
        int32_t o = clamp(k ? nofs : cofs, -64, 63), *x = &q[k ? DXP_NOISE : DXP_COLOR];
        *x += o > 0 ? o * (127 - *x < 63 ? 127 - *x : 63) / 63 : o * (*x < 64 ? *x : 64) / 64;
    }
    life_color=q[DXP_COLOR];
    if(fl.on)q[DXP_COLOR]=fl_classic_color(life_color);
    if (!ln)                                             /* (V10.6z8, the owner: "the kick sounds like a snare") the
                                                          * KICK's NOISE on a cube: the kit's 40 (V10.6q) or 4..24 (before)
                                                          * as good as none, the body whole; the knob up to 127 still there
                                                          * (64: an eighth, 100: a half) */
        q[DXP_NOISE] = (q[DXP_NOISE] * q[DXP_NOISE] * q[DXP_NOISE]) >> 14;
    ta = dx_tau((uint32_t)q[DXP_DECAY]);
    tn = (L->mode >> 4) & 1u ? (ta >> 2 < 8000u ? ta >> 2 : 8000u) : ta;   /* SNAP: a quarter, 8 ms at most (V10.6q: a click) */
    tp = ((L->mode >> 5) & 3u) == DXM_LONG ? (uint32_t)clamp((int32_t)(ta / 2u), 4000, 250000)
                                           : (uint32_t)clamp((int32_t)(ta / (ln ? 8u : 6u)), 1000, ln ? 25000 : 40000);
                                                         /* (V10.6q: / 12, 15 ms: thin; / 8, 25 ms: the click and the
                                                          * body; V10.6z8: the KICK's / 6, 40 ms again, as before V10.6q:
                                                          * the big sweep the owner asked back) */
    p16 = 384 + q[DXP_PITCH] * 12 + tune16;
    depth = q[DXP_PMOD] * 6;
    ea0 = v->live ? v->ea : 1 << 30;                     /* (V10.6q) the block's start: a hit struck again while it rings
                                                          * rises from where it was over the block, its phases and
                                                          * filters as they were (a jump to 0 clicked: x35 a step) */
    if (v->trig) {                                       /* the hit: from the top */
        if (!v->live) {
            v->ph = v->ph2 = v->ph4 = 0;
            v->ic1 = v->ic2 = 0;
            v->b1 = v->b2 = 0;
            v->mo = 0;
            v->mo1 = 0;
        }
        v->trig = 0;
        v->live = 1;
        v->ea = v->en = 1 << 30;
        v->ep = 32767;
        if (!v->rng)
            v->rng = 0x2545F491;
        v->hpos = v->hsum = 0;
        v->dwage = 0;                                    /* (DRUM-W) the scan, the LFO and the tick from the top */
        v->ph4 = 0;
        v->hseed = (uint16_t)(((uint32_t)v->rng * 2654435761u) >> 16);   /* (not drawn from the noise: OFF as it was) */
        v->inc = pitch_inc((uint32_t)clamp(p16 + depth, 0, 2047));
    }
    dw_mods(v, ln, n, fm, cofs, mo);                     /* (DRUM-W) the pad's modulators, once a block */
    if (fl.on && fl_ctx[MC_CPU() & 1u].id) {            /* (LIFE's on the pad: its places moved, finer than COLOR's steps) */
        int32_t lo = (fl.shape[fl_ctx[MC_CPU() & 1u].id - 1u].num * 6) >> 8;
        mo[0] += lo;
        mo[1] += lo;
    }
    k = 11890000u / ((pitch_inc((uint32_t)clamp(p16, 0, 2047)) >> 12) | 1u);   /* (V10.6q) half its period (us): no shorter */
    kb = v->choke ? dx_fall(1500u) : dx_fall(ta > k ? ta : k);   /* (a choked hit: gone in ~5 ms; a blip shorter than its own
                                                          * half wave was all DC) */
    ea1 = mulq16(v->ea, kb);
    en1 = mulq16(v->en, v->choke ? kb : dx_fall(tn));
    ep1 = (int32_t)(((uint32_t)v->ep * dx_fall(tp)) >> 16);
    {
        uint32_t pm = (L->mode >> 5) & 3u;
        int32_t off = (depth * ep1) >> 15;              /* DECAY, LONG: the envelope */
        if (pm == DXM_NOISE) {                          /* (JIANT 0.4) a random pitch each block, the envelope's depth */
            int32_t r = v->rng ? v->rng : 0x2545F491;
            off = (off * ((int32_t)noise32(&r) >> 17)) >> 14;
            v->rng = r;
        } else if (pm == DXM_SINE) {                    /* .. a sine at 1/8 of the pitch */
            v->ph3 += v->inc << 3;                      /* (V10.6q: << 3, 1/8 at CTL 64; was 1/16) */
            off = (off * sine_i(v->ph3)) >> 15;
        }
        inc1 = pitch_inc((uint32_t)clamp(p16 + off + mo[3], 0, 2047));
    }
    ng = q[DXP_NOISE] * 258;
    og = ((32767 - ng) * 11) >> 4;                       /* (x 11 / 16: about the DRUM kits' level) */
    ng = (ng * 11) >> 4;
    {                                                    /* (DRUM-W) the pad's trim */
        int32_t gt = dw_kit[ln].gain ? dw_kit[ln].gain : 64;
        og = og * gt >> 6;
        ng = ng * gt >> 6;
    }
    cx = clamp(q[DXP_COLOR] + mo[4], 0, 127) + (!ln << 5);   /* (the filter's cutoff, as a
                                                          * COLOR; V10.6z8: the KICK's 2 1/4 octaves up, its noise turned
                                                          * up a knock, a beater: not a rumble under the body) */
    if (flt < DXF_HP && cx < 96)                         /* (V10.6q) LP, BP: the band's power goes with its cutoff, the
                                                          * gain back as 1 / sqrt(fc) to COLOR 96's (x8 at most): a dark
                                                          * snare or clap as loud as a bright one (were up to 18 dB under) */
        ng = (ng * (int32_t)(dx_pow2((uint32_t)(cx < 12 ? 84 : 96 - cx) * 146u) >> 4)) >> 12;
    ga = mulq15(og, ea0 >> 15) << 8;   /* the oscillator's gain, Q23, stepped (the
                                                          * cowbell's in two stages: half on the noise's SNAP, a click,
                                                          * half on the decay, the tail) */
    dga = ((mulq15(og, ea1 >> 15) << 8) - ga) / (int32_t)n;
    gn = mulq15(ng >> 3, v->en >> 15) << 11;             /* the noise's (ng up to x8: >> 3) */
    {
        int32_t g1 = mulq15(ng >> 3, en1 >> 15) << 11;
        if (ln == DV_CLAP && (v->hpos < 1200u || v->hsum))   /* (V10.6q) the 808's claps; (0.7.9) HANDS: more hands */
            gn = dx_hands_step(v, n, gn, &g1, en1);
        dgn = (g1 - gn) / (int32_t)n;
    }
    inc = (int32_t)(v->inc >> 4) * 2;                    /* (>> 4: the step fits; x 2: half rate) */
    dinc = ((int32_t)(inc1 >> 4) * 2 - inc) / (int32_t)n;
    if (og) {                                            /* (DRUM-W) the body: its tables */
        const dw_lane_t *D = &dw_kit[ln];
        uint32_t age = v->dwage;
        dw_body(v, ln, y, n, inc, dinc, ga, dga, mo);
        if (D->tick && age < 64u) {                      /* the TICK: its first ms, at the hit's level */
            int32_t tg = mulq15(og, ea0 >> 15) * (int32_t)D->tlev >> 7;
            for (i = 0; i < n && age + i < 64u; i++)
                y[i] = clamp(y[i] + mulq15(dw_tick(D->tick, age + i), tg), -32767, 32767);
        }
        if (L->mode & DX_DRIVE) {
            int32_t dl = 14000 + !ln * 8000;
            for (i = 0; i < n; i++)
                y[i] = (softclip(y[i] * 3) * dl) >> 15;
        }
        if (warp > 0) {                                  /* FOLD (DRUM's): after the envelope, so it follows it */
            int32_t fold = clamp(warp, 0, 127), g = 16 + ((fold * fold * 5) >> 10), c = g * 6144 / (8 + g);
            for (i = 0; i < n; i++) {
                int32_t z = clamp((y[i] * g) >> 4, -5 * 16384, 5 * 16384), a;
                for (a = 0; a < 3; a++)
                    z = z > 16384 ? 32768 - z : z < -16384 ? -32768 - z : z;
                y[i] = (z * c) >> 12;
            }
        }
    } else {
        for (i = 0; i < n; i++)
            y[i] = 0;
    }
    v->dwage = v->dwage + n < 0x7FFFFFFFu ? v->dwage + n : v->dwage;
    mt = 0;
    if (ng || mt) {                                      /* the noise (and METAL) through its filter */
        int32_t ic1 = v->ic1, ic2 = v->ic2, rng = v->rng;
        int32_t mr, ck, cv;                              /* (V10.6o: one loop) the output r & mr - k v1 - cv v2: LP v2,
                                                          * BP v1, HP r - k v1 - v2 (each exactly as its own loop was) */
        uint32_t ncol = dw_kit[ln].noise;                /* (DRUM-W) the colour: RESON its filter ringing */
        int32_t nc = v->dwn;
        kd = ncol == DWN_RESON ? 700 : flt == DXF_BP ? 2400 : 6000;   /* (BP a little resonant) */
        tsvf_coef_k(&c, (cx + 14) << 8, kd);   /* (+ an octave of the table: the same Hz at half rate) */
        mr = flt == DXF_LP || flt == DXF_BP ? 0 : -1;
        ck = flt == DXF_LP ? 0 : flt == DXF_BP ? -4096 : kd;
        cv = flt == DXF_LP ? -1 : flt == DXF_BP ? 0 : 1;
        for (i = 0; i < n; i++, gn += dgn) {
            int32_t wn = (int32_t)noise32(&rng) >> 17;
            if (ncol == DWN_PINK) { nc += (wn - nc) >> 2; wn = (wn + nc * 3) >> 2; }
            else if (ncol == DWN_BROWN) { nc += (wn - nc) >> 4; wn = clamp(nc * 3, -32767, 32767); }
            else if (ncol == DWN_VIOLET) { int32_t d = wn - nc; nc = wn; wn = d >> 1; }
            int32_t r = mulq15((wn * 23170) >> 15, gn >> 8) + ((y[i] & mt) >> 1), v3 = r - ic2, v1, v2;
                                                         /* (+-11585: 1/sqrt2, half rate: the same power in half the
                                                          * band, +3 dB; its gain before the filter, V10.6q) */
            v1 = (c.a1 * ic1 + c.a2 * v3) >> 13;
            v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            y[i] = (y[i] & ~mt) + clamp(((r & mr) - ((ck * v1) >> 12) - cv * v2) << 1, -32767, 32767);
        }
        v->ic1 = ic1;
        v->ic2 = ic2;
        v->rng = rng;
        v->dwn = nc;
    }
    if (DX_TONE_BP(dx_tone) && (0xD8u >> ln) & 1u) {     /* (0.6.36) TONE's BP on the metal sounds (the hats, the rim,
                                                          * the cowbell); (V10.6q) around the sound's own COLOR, an
                                                          * octave under it .. over it, half dry (was 1.5 .. 9 kHz for all
                                                          * four and the band alone: 14 dB apart, the hats darkened) */
        int32_t b1 = v->b1, b2 = v->b2;
        tsvf_t cb;
        tsvf_coef_k(&cb, (q[DXP_COLOR] + 2 * (int32_t)DX_TONE_BP(dx_tone) - 2) << 8, 1800);   /* (half rate: + an octave) */
        for (i = 0; i < n; i++) {
            int32_t v3 = y[i] - b2, v1 = (cb.a1 * b1 + cb.a2 * v3) >> 13, v2 = b2 + ((cb.a2 * b1 + cb.a3 * v3) >> 13);
            b1 = clamp(2 * v1 - b1, -150000, 150000);
            b2 = clamp(2 * v2 - b2, -150000, 150000);
            y[i] = clamp((y[i] >> 1) + v1 - (v1 >> 2), -32767, 32767);   /* (its centre a little louder: the band sings) */
        }
        v->b1 = b1;
        v->b2 = b2;
    }
    for (i = n; i-- > 0;) {                              /* (0.6.17) back to full rate: each half-rate sample and the
                                                          * mean of it and the one before (the last block's: v->hl) */
        int32_t a = y[i], b = i ? y[i - 1] : v->hl;
        y[2u * i + 1u] = a;
        y[2u * i] = (a + b) >> 1;
    }
    if (nf & 1u)                                         /* (an odd block: its last sample held) */
        y[nf - 1u] = y[nf - 2u];
    if(fl.on)fl_classic_capture(life_color,(ea1>en1?ea1:en1)>>15,og?dw_lcyc[MC_CPU()&1u]:0,og?FVM_BLK:0u);
    v->hl = y[nf - 1u];
    v->ea = ea1;
    v->en = en1;
    v->ep = ep1;
    v->inc = inc1;
    if (v->ea < (1 << 19) && v->en < (1 << 19)) {       /* -66 dB (0.6.14: was -84: a tail computed below hearing) */
        v->live = 0;
        v->hl = 0;
    }
}

/* (0.7.2) the factory kits: DRUM's presets (eng_drum.c DRUM_PRESETS, one a kit, in this order), each the eight sounds
 * ({mode, A, B}, A and B {PITCH PMOD DECAY NOISE COLOR}), their LAYERs and their SENDs (bus << 6 | amount). The first
 * is the factory kit (DX_KIT_DEF, below). dx_kit is the music's kit: a section's (project.c keeps it in the project,
 * the song stages it with the section), edited on EDIT > SOUND (ui_input.c). Loaded whole by a preset (ui.c
 * apply_preset_to: dx_kit_load) into the section's kit; INIT SOUND and a new project take the first */
/* (V10.6z8, the owner: "the kicks sound like a snare", "the hats a giant low clap") every KICK as before V10.6q (LP,
 * its old NOISE and COLOR: V10.6q's HP noise at 40 was the snare in it); the 808 BOOM, LOFI DUST, ACOUSTIC, DUB ROOTS
 * and HIP HOP hats' COLOR up (their band at 1.2 .. 2.5 kHz under the preset's TONE: now ~5 .. 8 kHz) */
typedef struct { dx_lane_t lane[8]; uint8_t lay[8]; uint8_t snd[8]; } dx_kit_t;
static const dx_kit_t DX_KITS[] = {
    {   /* DRUM-X */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {0, 95, 100, 4, 20}, {2, 110, 92, 10, 60}},   /* KICK (DRUM-W: CyDrums' curve, 200 > 34 Hz over 300 ms, long) */
            {DX_MODE(DXW_FM, DXF_BP, 0), {40, 30, 52, 80, 90}, {46, 18, 64, 104, 70}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 46, 127, 84}, {54, 0, 58, 127, 76}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 22, 96, 116}, {108, 0, 14, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 62, 96, 112}, {108, 0, 74, 112, 118}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {30, 40, 66, 14, 54}, {36, 60, 76, 20, 74}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {63, 10, 14, 16, 46}, {70, 24, 20, 30, 60}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* 909 CLUB */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DX_DRIVE, {7, 90, 68, 4, 50}, {12, 120, 84, 10, 90}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 0), {40, 34, 60, 92, 104}, {46, 22, 72, 116, 84}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 52, 127, 88}, {54, 0, 64, 127, 80}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {106, 0, 16, 96, 126}, {114, 0, 8, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {106, 0, 66, 96, 122}, {114, 0, 78, 112, 127}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 54, 66, 14, 68}, {38, 74, 76, 20, 88}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 12, 16, 50}, {72, 24, 18, 30, 64}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* 808 BOOM */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5, {3, 40, 102, 0, 6}, {8, 70, 118, 6, 46}},   /* KICK */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {36, 18, 46, 90, 66}, {42, 6, 58, 114, 46}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {56, 0, 50, 127, 70}, {50, 0, 62, 127, 62}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {92, 0, 14, 96, 118}, {100, 0, 6, 112, 124}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {92, 0, 60, 96, 114}, {100, 0, 72, 112, 120}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1) | DXM_LONG << 5, {24, 26, 82, 14, 42}, {30, 46, 92, 20, 62}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 14, 16, 38}, {72, 24, 20, 30, 52}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {62, 0, 56, 34, 0}, {66, 0, 66, 46, 16}},   /* BELL */
        },
        {0, 4, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* LOFI DUST */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {11, 78, 58, 14, 0}, {16, 108, 74, 20, 40}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {44, 30, 36, 104, 68}, {50, 18, 48, 127, 48}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {62, 0, 36, 127, 60}, {56, 0, 48, 127, 52}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {104, 0, 12, 96, 124}, {112, 0, 4, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {104, 0, 48, 96, 120}, {112, 0, 60, 112, 126}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 40, 52, 20, 34}, {38, 60, 62, 30, 54}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {69, 10, 8, 16, 30}, {76, 24, 14, 30, 44}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {66, 0, 32, 34, 10}, {70, 0, 42, 46, 36}},   /* BELL */
        },
        {0, 9, 0, 0, 0, 0, 5, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* INDUSTRIAL */
        {
            {DX_MODE(DXW_FM, DXF_LP, 1) | DX_DRIVE, {7, 104, 80, 18, 64}, {12, 127, 96, 24, 104}},   /* KICK */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {42, 54, 64, 102, 124}, {48, 42, 76, 126, 104}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 54, 127, 108}, {54, 0, 66, 127, 100}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 32, 106, 127}, {108, 0, 24, 122, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 76, 106, 127}, {108, 0, 88, 122, 127}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1) | DX_DRIVE, {30, 64, 76, 20, 88}, {36, 84, 86, 30, 108}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1) | DXM_NOISE << 5, {63, 22, 20, 28, 70}, {70, 36, 26, 42, 84}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1) | DXM_NOISE << 5, {64, 0, 48, 46, 96}, {68, 0, 58, 58, 120}},   /* BELL */
        },
        {0, 12, 0, 0, 0, 0, 6, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* ACOUSTIC */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {9, 60, 64, 12, 0}, {14, 90, 80, 18, 34}},   /* KICK */
            {DX_MODE(DXW_BELL, DXF_BP, 0), {38, 30, 56, 98, 74}, {44, 18, 68, 122, 54}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 48, 127, 66}, {52, 0, 60, 127, 58}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {96, 0, 18, 96, 116}, {104, 0, 10, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {96, 0, 66, 96, 112}, {104, 0, 78, 112, 118}},   /* HAT OP */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {28, 28, 70, 20, 8}, {34, 48, 80, 30, 28}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {61, 10, 16, 16, 26}, {68, 24, 22, 30, 40}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {62, 0, 46, 34, 20}, {66, 0, 56, 46, 44}},   /* BELL */
        },
        {0, 5, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* DUB ROOTS */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5, {3, 58, 92, 4, 0}, {8, 88, 108, 10, 38}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 0), {34, 30, 64, 88, 80}, {40, 18, 76, 112, 60}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 52, 127, 72}, {52, 0, 64, 127, 64}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {98, 0, 20, 96, 122}, {106, 0, 12, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {98, 0, 68, 96, 118}, {106, 0, 80, 112, 124}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {26, 40, 76, 14, 44}, {32, 60, 86, 20, 64}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {59, 10, 24, 16, 46}, {66, 24, 30, 30, 60}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 52, 34, 8}, {68, 0, 62, 46, 40}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* TECHNO RAW */
        {
            {DX_MODE(DXW_FM, DXF_LP, 1) | DX_DRIVE, {7, 96, 74, 10, 56}, {12, 126, 90, 16, 96}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_HP, 0), {40, 40, 46, 102, 110}, {46, 28, 58, 126, 90}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 48, 127, 94}, {54, 0, 60, 127, 86}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {104, 0, 14, 100, 127}, {112, 0, 6, 116, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {104, 0, 62, 100, 127}, {112, 0, 74, 116, 127}},   /* HAT OP */
            {DX_MODE(DXW_METAL, DXF_HP, 1), {30, 52, 68, 16, 46}, {36, 72, 78, 26, 66}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 10, 22, 58}, {72, 24, 16, 36, 72}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 40, 70}, {68, 0, 54, 52, 96}},   /* BELL */
        },
        {0, 7, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* BREAKS */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {11, 78, 68, 12, 32}, {16, 108, 84, 18, 72}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {42, 38, 54, 96, 112}, {48, 26, 66, 120, 92}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 48, 127, 94}, {54, 0, 60, 127, 86}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {100, 0, 26, 102, 116}, {108, 0, 18, 118, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {100, 0, 70, 102, 112}, {108, 0, 82, 118, 118}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 48, 66, 18, 66}, {38, 68, 76, 28, 86}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {63, 10, 14, 20, 56}, {70, 24, 20, 34, 70}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* HIP HOP (V10.34, a user's ask): boom bap: a round kick with a knock, a fat cracking snare, a tight clap,
         * dusty hats (band-passed), a low tom, a woody rim, a shaker in the BELL's place */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {6, 84, 74, 8, 26}, {10, 112, 90, 16, 62}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {38, 34, 62, 106, 82}, {44, 22, 76, 124, 62}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 42, 127, 76}, {52, 0, 54, 127, 66}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {98, 0, 12, 96, 116}, {106, 0, 6, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {98, 0, 54, 96, 110}, {106, 0, 66, 112, 116}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {26, 36, 70, 14, 40}, {32, 56, 80, 20, 60}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {61, 10, 10, 16, 34}, {68, 24, 16, 30, 48}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {84, 0, 20, 110, 70}, {92, 0, 28, 124, 80}},   /* SHAKER */
        },
        {0, 8, 0, 0, 0, 0, 4, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* LATIN PERC (V10.7.8, the owner: a guiro, the percussion of cumbia, reggaeton, baile funk): a bombo, a timbal,
         * the llamador, the GUIRO on the closed hat (the filter GR: an accent its long stroke, a soft hit its "chk"),
         * maracas, a conga, the clave, the cowbell */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {9, 70, 66, 4, 20}, {14, 96, 80, 10, 50}},   /* BOMBO */
            {DX_MODE(DXW_BELL, DXF_BP, 0), {50, 20, 40, 70, 96}, {56, 12, 50, 90, 80}},   /* TIMBAL */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {54, 30, 40, 20, 60}, {60, 40, 48, 30, 70}},   /* LLAMADOR */
            {DX_MODE(DXW_METAL, DXF_GR, 0), {100, 110, 20, 127, 106}, {108, 122, 26, 127, 112}},   /* GUIRO */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {84, 0, 22, 118, 110}, {92, 0, 30, 124, 116}},   /* MARACAS */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {44, 30, 50, 12, 40}, {50, 44, 58, 18, 56}},   /* CONGA */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {76, 6, 10, 8, 30}, {82, 14, 14, 16, 44}},   /* CLAVE */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* COWBELL */
        },
        {0, 4, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
};
#define DX_NKIT ((uint32_t)(sizeof DX_KITS / sizeof DX_KITS[0]))
#define DXK_LATIN 10u                                    /* (V10.7.8) LATIN PERC (gen.c gen_kit: the latin genres' percussion) */
_Static_assert(DXK_LATIN + 1u == DX_NKIT, "LATIN PERC the last kit");
/* (V10.6o) the factory kit (DX_KIT_DEF: what a project without one gets) and (0.6.49) its LAYERs (FEEDBK 0: the snare,
 * the hats and the bell, no note to them; the rest clean, CPU): the first kit's, DRUM-X (were copies of it) */
#define DX_KIT_DEF (DX_KITS[0].lane)
#define DX_LAY_DEF (DX_KITS[0].lay)
/* a stored kit as the voice takes it: values 0..127, the mode's fields in range (wave 0..3, filter 0..2, snap) */
static int dx_kit_ok(const dx_lane_t *k)
{
    uint32_t l, i;
    for (l = 0; l < 8u; l++) {
        for (i = 0; i < DXP_N; i++)
            if (k[l].a[i] > 127u || k[l].b[i] > 127u)
                return 0;
    }
    return 1;
}


/* Classic callers retain exactly the original feedback path. */
static void dx_run(const dx_lane_t *L, dx_voice_t *v, int32_t morph, int32_t tune16,
                   int32_t dofs, int32_t cofs, int32_t nofs, int32_t warp, int32_t fm, int32_t *y, uint32_t n)
{ dx_run_fx(L,v,morph,tune16,dofs,cofs,nofs,warp,fm,y,n,0); }
