/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6d) MATH (was LOFI): the formulas alone, BYTE (bytebeat) and FLOAT (floatbeat). The chip waves (PLS TRI SAW
 * NOIS WRAM, the STEP envelope) were redundant beside ANALOG (the user's call): a sound of theirs that arrives -- a
 * project, a user preset -- plays as ANALOG's nearest (core.h analog_from_chip). The engine's number (3) and its stored
 * values stay: WAVE 5 BYTE, 6 FLOAT; CHIP 0..3 the bits (STEP, 4, read as 4BIT: the same sound on BYTE and FLOAT) */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT", "FULL"};   /* (JIANT BYTE: FULL, USER's) */
static const char *const N_RWAVE[] = {"-", "-", "-", "-", "-", "BYTE", "FLOAT", "USER", "FUNC"};   /* (WAVE 5, 6: the stored numbers;
                                                         * JIANT BYTE: 7 the part's own formula, fvm_gen.c) */
enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT, CHIP_FULL };
enum { RW_BYTE = 5, RW_FLOAT = 6, RW_USER = 7, RW_FUNC = 8 };

/* (JIANT 0.4) WAVE = BYTE: bytebeat, an 8-bit formula of the time t played as the wave (after viznut's 2011
 * one-liners: the music is in the formula). DUTY picks one of 32 (ALGO, DUTY / 4), CRSH is the formulas' variable
 * a (VAR: 1 .. 16); t runs with the note played (C4: ~8 kHz, the classic rate; an octave up twice as fast), restarts
 * at each note-on; the hold (4B/2), the bits (CHIP), TONE and the rest as for the other waves. Our own picks and
 * variations; each a few operations a sample */
/* (fmgen45, room) BYTE's 48 names "B01" .. "B48" and FLOAT's "F01" .. "F48" written in the pool (zeroed at boot) the
 * first time a desc is asked for (lofi_desc: every way to LOFI_ALGO / LOFI_FALGO), not 776 B of flash */
static const char *lofi_nt[98] __attribute__((section(".pool")));
#define N_BYTE lofi_nt
#define N_FLOAT (lofi_nt + 49)
/* (JIANT 0.5) BYTE's own: RES the filter's resonance (VIB's slot), LOOP (MASK's slot) t held in a window of 2^17 ..
 * 2^5 steps: the formula's long evolution (0: free) folded into a short cycle repeating at the note: a pitched tone */
/* (JIANT 0.5) E4 (CUT) left: the filter is the FILTER page's (TYPE, CUT, RES: P_FTYPE P_FCUT P_FRES), on every engine */
/* (JIANT 0.5.1) each formula's start, in 512 steps of t: where it is already moving (some sit silent for up to
 * ~2 s from t = 0); a note starts there, LOOP's window too. The first 17 were rewritten in 0.5.1, away from the
 * well-worn one-liners (t * (42 & t >> 10), t & t >> 8 ..): rhythms and arpeggios after Tejeez, xpansive's
 * "lost in space", a melody of t >> 9 modulo 13, textures of t % 24 and t % 19 */
static const uint8_t BYTE_T0[32] = {0, 0, 0, 0, 0, 0, 0, 31, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0, 16, 0, 7, 16, 0, 1, 8, 15, 8, 0, 15, 23, 8, 0};
static __attribute__((noinline)) uint32_t lofi_byte(uint32_t f, uint32_t t, uint32_t a)
{
    switch (f & 63u) {
    case 0: return (~t >> 2) * ((127u & t * (7u & t >> 10)) < (245u & t * (2u + (5u & t >> 14))));   /* (Tejeez) */
    case 1: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (5u + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (Tejeez) */
    case 2: return ((-t & 4095u) * (255u & t * (t & t >> 13)) >> 12) + (127u & t * (234u & t >> 8 & t >> 3) >> (3u & t >> 14));   /* (xpansive) */
    case 3: return t * (((t >> 9) ^ ((t >> 9) - 1u) ^ 1u) % 13u);
    case 4: return ((t >> 13 | t % 24u) & (t >> 7 | t % 19u)) << 3;
    case 5: return t * (t ^ (t + (t >> 15 | 1u)) ^ (((t - 1280u) ^ t) >> 10));
    case 6: return (t * (0xCA98u >> (t >> 9 & 14u) & 15u)) | t >> 8;
    case 7: return (t * (t >> 8 | t >> 9) & 46u & t >> 8) ^ ((t & t >> 13) | t >> 6);
    case 8: return (t >> 4) * (13u & 0x8898A989u >> (t >> 11 & 30u));
    case 9: return t * (t >> ((8u * ((t >> 15) | (t >> 8))) & 31u) & (20u | (((t >> 19) * 5u) >> (t & 31u)) | t >> 3));
    case 10: return (t & t % 255u) - (t * 3u & t >> 13 & t >> 6);
    case 11: return (t * (t >> 9 | t >> 13) & (t >> 6 | t >> 11) & 127u) + ((t >> 4) * (3u + (t >> 12 & 3u)) & 64u);
    case 12: return t + (t & (t ^ t >> 6)) - t * ((t >> 9) & (t % 16u ? 2u : 6u) & t >> 9);
    case 13: return t * (t >> 11 & t >> 8 & 123u & t >> 3);
    case 14: return (t * 9u & t >> 4) | (t * 5u & t >> 7) | (t * 3u & t >> 10);
    case 15: return (t * ((t & 4096u ? (t % 65536u < 59392u ? 7u : t & 7u) : 16u) + (1u & t >> 14)) >> (3u & -t >> (t & 2048u ? 2 : 10))) | t >> (t & 16384u ? (t & 4096u ? 10 : 3) : 2);   /* (Tejeez) */
    case 16: return (t * ((3u + (1u ^ (t >> 10 & 5u))) * (a + (3u & t >> 14)))) >> (t >> 8 & 3u);   /* (a: VAR) */
    case 17: return t * ((t >> (a & 15u) | t >> 8) & 63u & t >> 4);
    case 18: return (t * a & t >> 7) | (t * 3u & t >> 10);
    case 19: return (t >> a | t) * (t >> 9 & 7u);
    case 20: return t * (t >> 8 & a) ^ t >> 4;
    case 21: return (t * a & t >> 8) * (t >> 12 & 3u);
    case 22: return t * (0x9AD5u >> (t >> (8u + (a & 3u)) & 14u) & 15u);
    case 23: return (t ^ t >> a) * (t >> 10 & 5u);
    case 24: return t >> 2 & (t * a >> 6 | t >> 9);
    case 25: return (t | t >> a | t >> 7) * (t >> 13 & 7u);
    case 26: return t * ((t >> 11 & 3u) + a) & t >> 6;
    case 27: return (t * (a | 1u) % (257u - (t >> 10 & 127u))) & 255u;
    case 28: return t * (t >> 10 & 7u & a) | t >> 5;
    case 29: return (t >> 5 & t >> 7) * a + (t >> 9 & 32u);
    case 30: return t * (t >> 12 & a ? 3u : 2u) & t >> 6;
    /* (p6, the owner: more of them) ALGO 128 .. 191: sixteen more, the classics' families and our own */
    case 32: return (t * (t >> 5 | t >> 8)) >> (t >> 16 & 7u);
    case 33: return t * ((t >> 12 | t >> 8) & 63u & t >> 4);
    case 34: return (t >> 6 | t | t >> (t >> 16 & 7u)) * 10u + (t >> 11 & 7u);
    case 35: return (t | (t >> 9 | t >> 7)) * t & (t >> 11 | t >> 9);
    case 36: return (t * 5u & t >> 7) | (t * 3u & t * 4u >> 10);
    case 37: return (t >> 7 | t | t >> 6) * 10u + 4u * ((t & t >> 13) | t >> 6);
    case 38: return t & 4096u ? (t * (t ^ t % 255u) | t >> 4) >> 1 : (t >> 3) | (t & 8192u ? t << 2 : t);
    case 39: return (t * (t >> 9 | t >> 13) & 16u) * 8u + (t >> 6 & t >> 8 & 31u);
    case 40: return ((t >> 4) * (t >> 5 & 0x2Au)) | t >> 3;
    case 41: return t * ((t >> 3 | t >> 9) & 82u & t >> 9);
    case 42: return (t * a & t >> 4) | (t >> 5 & t * (a + 1u) >> 7);
    case 43: return t * (t >> ((t >> 11 & 3u) + 4u) & a * 3u);
    case 44: return (t >> 2) * ((t >> (8u + (a & 3u)) & 7u) | (t >> 12 & 3u));
    case 45: return (t * (0x5A3Cu >> (t >> 11 & 12u) & 15u)) >> (t >> 14 & 1u);
    case 46: return (t * (t >> 13 & 1u ? 3u : 4u) & t >> 5) | (t * (a + 2u) >> 9 & t >> 7);
    case 47: return (t ^ t >> a) * ((t >> 9) % 5u + 1u);
    default: return (t * a >> 4 | t >> 3) ^ (t >> 7 & t >> 11);
    }
}
/* (JIANT 0.5.1; 0.5.6 a floatbeat generator) WAVE = FLOAT: floatbeat, bytebeat's sibling in floats. Each ALGO is a
 * formula of t, as BYTE's, but made of sines: sin(t x (t >> 11 ^ t >> 13 & 7)), FM whose index walks with t's bits,
 * melodies read from nibble tables, kicks and noise hats inside the formula, folds and powers of sines .. rhythm,
 * melody and timbre at once. t runs 32 steps a cycle of the note (the formula in tune with the key, its rhythm faster
 * up the keyboard, as bytebeat's), with 8 bits of fraction for the sines (x = t in Q8; S(x) = sin(2 pi x / 32)).
 * The same t as BYTE's: MOTN runs its slow bits at their own speed, VAR mutates it (lofi_mut), BEND folds it, LOOP
 * holds it in a window; FBK (E5, 0.6.4) feeds the output back into t, from a shimmer to chaos; CHIP the bits, RES the
 * filter's */
static char lofi_nm[96][4] __attribute__((section(".lowbuf")));   /* (the low RAM, zeroed at boot too) */
static __attribute__((noinline)) void lofi_names(void)
{
    uint32_t i, n;
    for (i = 0; i < 96u; i++) {
        char *c = lofi_nm[i];
        n = i % 48u + 1u;
        c[0] = i < 48u ? 'B' : 'F'; c[1] = (char)('0' + n / 10u); c[2] = (char)('0' + n % 10u);
        lofi_nt[i + i / 48u] = c;
    }
}
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 191, 64, (const char *const *)N_BYTE};    /* (p6: 48; the knob's: edit[2] keeps
                                                         * 0..127, FUN7's byte) */
/* (JIANT BYTE) MOTION: the formula's slow time (its rhythm, its phrase) as a note value of the song's tempo: LIBRE the
 * slow time at its own pace (MATH's MOTN 0, as before), then 1/32 .. 4 bars, straight, dotted (.) and triplets (T).
 * MOT_24: the value in 1/24 of a beat */
static const char *const N_MOTION[] = {"LIBRE", "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4",
                                       "1/4.", "1/2T", "1/2", "1/2.", "1", "2", "4", 0};
static const uint16_t MOT_24[17] RAM_RO(mot_24) = {24, 3, 4, 6, 9, 8, 12, 18, 16, 24, 36, 32, 48, 72, 96, 192, 384};
static const param_desc_t LOFI_MOTN = {"MOTION", F_ENUM, 0, 16, 0, N_MOTION};
static const param_desc_t LOFI_SEED = {"SEED", F_INT, 0, 191, 64, 0};
static const param_desc_t LOFI_MUTA = {"MUTA", F_INT, 0, 127, 0, 0};
/* (JIANT BYTE) USER's CHIP: up to FULL (a bytebeat's 16 bits between its steps; a floatbeat as it is) */
static const param_desc_t LOFI_UCHIP = {"CHIP", F_ENUM, 0, 4, 4, N_CHIP};   /* (phase 9) USER: VAR 1..63 mutates, 64.. crosses */   /* (JIANT BYTE) USER: ALGO the generator's seed */   /* (p6: 128 .. 255 SYNC; V10.6d: 0) */
static const param_desc_t LOFI_FALGO = {"ALGO", F_INT, 0, 191, 64, (const char *const *)N_FLOAT};   /* = edit[2] (FLOAT) */
/* .. its CHIP the resolution: 4 bits, 4 bits held over two samples, FULL (smooth, the default), 1 bit, STEP (4 bits and
 * the stepped envelope) */
static const char *const N_FCHIP[] = {"4BIT", "4B/2", "FULL", "1BIT"};
static const param_desc_t LOFI_FCHIP = {"CHIP", F_ENUM, 0, 3, 2, N_FCHIP};    /* = edit[0] (FLOAT; 0.5.5: no STEP) */
/* (0.5.4) BYTE's and FLOAT's own second page: MOTN (E4, where CUT was: the time's speed, 64 as written), GRIT (BYTE,
 * E5: the samples held longer, a rate crusher), FBK (FLOAT, E5: the output back into the formula's time) */
static const param_desc_t LOFI_FDCY RAM_RO(lofi_fdcy) = {"FEEDBK", F_PCT, 0, 127, 0, 0};            /* = edit[5] (FLOAT, 0.6.4: was DRV, a
                                                         * fold that silenced it and doubled the FILTER page's DIST) */
/* MOTN 0..127 -> the time's speed, Q8: x1/16 .. x1 (64) .. x15.5, exponential */
/* (p6, the owner: "the notes only transpose, not the speed") MOTION 128 .. 255: SYNC, the formula's slow bits (its
 * rhythm, its melody) at the song's position, a 1/16 every 2048 t (the speeds as 0 .. 127's), the note only its pitch
 * (the fast bits): every note the same sequence transposed, in time with the bar. The bar's clock is seq.c's (the same
 * variables, declared again here: engines.c comes first). Returns the slow t at the block's start, *rate its step a
 * sample (Q16) */
static volatile uint32_t beat_pos, beat_n;
static volatile uint32_t bar_n;
static __attribute__((noinline)) uint32_t lofi_sync(uint32_t *rate)
{
    uint32_t bl = (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 300);
    *rate = (8192u << 16) / bl;
    return (bar_n * 4u + beat_n) * 8192u + beat_pos * 8192u / bl;
}
static uint32_t lofi_motion(int32_t m)
{
    uint32_t e = (uint32_t)clamp(m >= 128 ? m - 128 : m, 0, 127) + 64u;
    return ((16u + (e & 15u)) << (e >> 4)) >> 4;
}
/* (JIANT BYTE) TIME (P_KEY) and MOTION (P_E4) -> how the slow time runs this block. TIME: UNSYNC (0) the time from
 * the note (each note starts it again), SYNC (1) the slow time the song's (the key only its pitch: the fast bits),
 * FIJO (2) every bit the song's (the keys only start it). (A stored 3 .. 15 is a SYNC, 16 a FIJO: JIANT BYTE's first
 * KEY.) MOTION: LIBRE the slow time at MATH's own pace (x1/16 of the note's), a note value the slow time 8192 a value
 * (the formula's t >> 13 steps once a value). Out: *mul (Q8 a note step, LIBRE), *sy (the song's clock for the slow
 * bits) with *sb / *sr (slow t at the block's start, its step a sample Q16), *inc (UNSYNC on a value: the slow time's
 * step a sample, Q8), *key (LK_FIJO: all of t the song's, qb / qr) */
enum { LK_LIBRE, LK_ALTURA, LK_FIJO };
/* (loopq) SYNC's LOOP 1 .. 8 (P_E7): 4 BAR 2 BAR 1 BAR 1/2 1/4 1/8 1/16 1/32, the song's window (2^17 .. 2^10 of
 * lofi_sync's 8192 a beat: a mask, no division), the slow t its place in it, the note's fast t from its start: *lw
 * on, *wn the window's number (the voice's v->ph[1], dead in SYNC: a new one, the note's t from 0 at the block's
 * start), *iw the sample the next one starts in this block (~0: none) and *s2 the slow t's base from there (the note's
 * t, its hold from 0 again). 9 .. 127 the tone zone as ever */
typedef struct { uint32_t mul, sy, sb, sr, inc, key, qb, qr, lw, wn, iw, s2; } lofi_tm_t;
#ifndef FM1_OPAQUE
#define FM1_OPAQUE(v) ((void)(v))                    /* (the host: no reach to mind) */
#endif
static void lofi_time(const int16_t *p, lofi_tm_t *m)
{
    int32_t kv = p[P_KEY];
    uint32_t mo = (uint32_t)clamp(p[P_E4], 0, 16), tm = kv <= 0 ? 0u : kv == 2 || kv >= 16 ? 2u : 1u, r = 0, b;
    memset(m, 0, sizeof *m);
    m->mul = lofi_motion(0);
    m->iw = ~0u;                                        /* (loopq: no window's start) */
    if (!mo && !tm)
        return;                                         /* (LIBRE, UNSYNC: MATH as ever) */
    b = lofi_sync(&r);                                  /* (the song's: 8192 a beat) */
    if (tm == 1u && (uint32_t)p[P_E7] - 1u < 8u) {     /* (loopq) SYNC: LOOP a division of the song */
        uint32_t sh = 18u - (uint32_t)p[P_E7], d = (1u << sh) - (b & ((1u << sh) - 1u)), rb = r << VH_SH();
        m->lw = 1u;
        m->wn = b >> sh;
        b &= (1u << sh) - 1u;
        if (d <= (CTL * rb) >> 16)                      /* (the next window in this block: its first sample) */
            m->iw = ((d << 16) + rb - 1u) / rb;
    }
    {
        uint32_t v = MOT_24[mo ? mo : 9u];              /* (LIBRE in SYNC: a value a beat) */
        if (tm) {
            m->sy = tm == 1u;
            m->sb = b / v * 24u + b % v * 24u / v;   /* (32 bits: no 64-bit division on the chip) */
            m->sr = r * 24u / v;
            if (tm == 2u) {
                m->key = LK_FIJO;
                m->qb = m->sb;
                m->qr = m->sr;
                m->sy = 0;
            }
            m->mul = 256u;
        } else {
            m->inc = (r * 24u / v) >> 8;                /* (UNSYNC on a value: the tempo's pace from the note, Q8) */
            m->mul = 256u;
        }
        m->sr <<= VH_SH();                              /* (byte-cpu-light, as JIANT FM V10.7.56) at half the rate the
        m->qr <<= VH_SH();                               * tempo's steps a sample twice as long */
        m->inc <<= VH_SH();
        m->s2 = 0u - ((m->iw * m->sr) >> 16);
    }
}
/* (0.5.4) VAR as a mutation of the time (BYTE's t, FLOAT's step count): v >> 4 picks the way, v & 15 how much. 0..15
 * the time as written; then stairs that transpose as it goes, xor and or with itself shifted, a bit dropped (holes in
 * the rhythm), the low bits reversed (the phrase backwards), multiplied (faster, higher), scrambled */
static uint32_t lofi_mut(uint32_t t, uint32_t v)
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
#define S(x) sine_i((uint32_t)(x) << 19)                 /* sin of x (t in Q8): a turn every 32 t */
#define PM(y, k) ((uint32_t)(((int32_t)(y) * (int32_t)(k)) >> 6))   /* a sine (Q15) times k: k / 16 of a turn */
/* the floatbeat formula f at t (its integer) and x (t in Q8); aux: a formula's own state (a kick's phase) */
static __attribute__((noinline)) int32_t lofi_float(uint32_t f, uint32_t t, uint32_t x, int32_t *aux)
{
    static const uint8_t R[8] = {16, 18, 20, 21, 24, 27, 30, 32};   /* just ratios, Q4: 1 9/8 5/4 4/3 3/2 5/3 15/8 2 */
    uint32_t st = t >> 11, fr = t & 2047u, h;
    int32_t E = 32767 - (int32_t)(fr << 4), a, b;          /* (a step's envelope: 1 .. 0) */
    switch (f & 63u) {
    case 0: return S(x * (((t >> 11) ^ (t >> 13)) & 7u) + x) * E >> 15;                       /* sin(t (t>>11 ^ t>>13 & 7)) */
    case 1: return S(x + PM(S(x * 3u), (int32_t)((t >> 10) & 15u) * 3));                      /* FM, its index walking */
    case 2: return S((x >> 4) * R[(0x36364689u >> ((st & 7u) * 4u)) & 7u]) * E >> 15;         /* a melody, a nibble table */
    case 3: return S(x * (1u + ((t >> 12) & 3u))) * S(x * (((t >> 9) & 7u) + 1u)) >> 15;      /* a ring, rhythmic */
    case 4: {                                                                                  /* a kick, a bass */
        uint32_t k = fr < 1024u ? (1024u - fr) * (1024u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 40;
        a = (st & 1u) ? 0 : sine_i((uint32_t)*aux << 13) * (int32_t)(1024u - (fr < 1024u ? fr : 1024u)) >> 10;
        return (a + (S(x >> 1) >> 1)) >> 1;
    }
    case 5: return S(x * ((((t >> 9) & (t >> 11)) & 7u) | 1u)) * E >> 15;                     /* Sierpinski partials */
    case 6: return softclip(S(x * (1u + (st & 1u))) * (1 + (int32_t)((t >> 11) & 7u) * 3) >> 1);   /* drive, rhythmic */
    case 7: return S((x >> 4) * R[(st * 3u + (st >> 2)) & 7u] * (1u + (st & 1u)));            /* fifths and octaves */
    case 8: {                                                                                  /* two arpeggios */
        static const uint8_t H[8] = {2, 3, 4, 5, 6, 8, 5, 3};
        return (S(x * H[(t >> 9) & 7u] >> 1) + S(x * H[(t >> 11) & 7u] >> 2)) >> 1;
    }
    case 9: h = (t * 2654435761u) >> 17;                                                       /* a snare, a tone */
        a = (st & 1u) ? (int32_t)h - 16384 : 0;
        return (mulq15(a, E) + S(x)) >> 1;
    case 10: return S(x + PM(S(x * (((t >> 12) & 7u) + 1u)), E >> 10));                       /* a pluck of FM */
    case 11: return S(x + PM(S(t << 2), 6));                                                  /* a vibrato, deep (4 Hz) */
    case 12: return S((uint32_t)(S(x) * (2 + (int32_t)((t >> 11) & 7u))) >> 2);               /* folded, rhythmic */
    case 13: a = S(x * (1u + (st & 1u))); b = S((x >> 4) * (24u + (st & 6u) * 4u));           /* rectified, leaping */
             return (a < 0 ? -a : a) - (b < 0 ? -b : b);
    case 14: return S(x & ~((1u << (4u + (st & 7u))) - 1u));                                  /* a staircase sine */
    case 15: return S(x * (((t >> 8) % 7u) + 1u)) * E >> 15;                                  /* partials mod 7 */
    case 16: {                                                                                 /* three against four */
        int32_t e3 = 32767 - (int32_t)((t % 3072u) * 10u), e4 = E;
        return (mulq15(S(x * (1u + (st & 1u))), e4) + mulq15(S((x >> 4) * (24u + (st & 2u) * 8u)), e3 > 0 ? e3 : 0)) >> 1;
    }
    case 17: return S((x >> 6) * (64u + ((t >> 6) & 255u))) * E >> 15;                        /* a riser each bar */
    case 18: return mulq15(S(x * (1u + st % 3u)) * ((t >> 10) & 1u ? 1 : 0) + S(x + (t << 2)), 16384);   /* beating, gated */
    case 19: {                                                                                 /* odd partials, built up */
        uint32_t k, n = 1u + ((t >> 11) & 3u);
        for (a = 0, k = 0; k < n; k++)
            a += S(x * (2u * k + 1u)) / (int32_t)(2u * k + 1u);
        return a * 3 >> 2;
    }
    case 20: return S(x * (2u + ((st * 5u) & 7u)) >> 1);                                      /* leaps */
    case 21: {                                                                                 /* a drum loop */
        uint32_t k = fr < 768u ? (768u - fr) * (768u - fr) >> 6 : 0u;
        *aux += (int32_t)k + 30;
        a = (st & 3u) == 0u ? sine_i((uint32_t)*aux << 13) * (int32_t)(768u - (fr < 768u ? fr : 768u)) / 768 : 0;
        b = (t >> (8u + (st & 3u))) & 1u ? (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(1024u - (t & 1023u)) >> 12 : 0;
        return (a + b + (S(x >> 1) >> 2)) >> 1;
    }
    case 22: return S(x * (1u + (((t >> 9) & (t >> 7)) & 7u)) + (((t >> 9) & (t >> 7) & 255u) << 9));   /* bytebeat in it */
    case 23: return S(x * 2u + PM(S(x * 5u), (int32_t)((t >> 11) & 3u) * 8));                 /* FM 5:2, stepped */
    case 24: a = S(x * (1u + (st & 3u))); return mul64_sh15(a, a < 0 ? -a : a) * E >> 15;   /* squared */
    case 25: return S(x & ~(1u << (8u + (st & 7u)))) * E >> 15;                              /* a bit of t dropped: jumps */
    case 26: return S((x >> 4) * (34u - ((t >> 11) & 15u)) >> 1) * E >> 15;                   /* a descending line */
    case 27: return (S(x) + mulq15(S(x * (((t >> 13) & 3u) + 2u)), E)) >> 1;                   /* a partial, struck */
    case 28: return S(x + PM(S(x * 2u), S(t) >> 10));                                         /* FM, its index a 1 Hz sine */
    case 29: return S((x >> 4) * (16u + ((t >> 7) & 63u)));                                   /* a glide up partials */
    case 30: h = ((st + 1u) * 2654435761u) >> 29; return S((x >> 4) * R[h]) * E >> 15;        /* a random line */
    /* (p6) ALGO 128 .. 191: sixteen more */
    case 32: return S(x * (st % 5u + 1u)) * E >> 15;                                          /* partials mod 5, stepped */
    case 33: return S(x + PM(S(x * 7u), E >> 11));                                            /* FM 7:1, plucked */
    case 34: return (S(x) + S(x * 3u >> 1) + S((x >> 4) * R[st & 7u])) / 3;                    /* a fifth over a line */
    case 35: return (st & 3u) != 3u ? S((x >> 4) * R[(st * 5u) & 7u] * 2u) * E >> 15 : 0;     /* a melody with rests */
    case 36: return S(x + PM(S(x >> 1), (int32_t)((t >> 9) & 15u) * 2));                      /* a sub's FM, growling */
    case 37: a = S(x); return a > 0 ? a : a / 4;                                              /* half rectified */
    case 38: return S(x * (1u + ((t >> 10) & 1u) * 2u)) * S(t << 1) >> 15;                    /* tremolo, octave leaps */
    case 39: return S((x >> 3) * (8u + ((t >> 8) & 7u))) * E >> 15;                           /* a stair arpeggio */
    case 40: return (mulq15(S(x * 2u), E) + mulq15(S(x * 3u), 32767 - E)) >> 1;               /* the 2nd into the 3rd */
    case 41: return S(x + PM(S(x * 5u), (int32_t)(st & 7u) * 4));                             /* FM 5:1, its index stepped */
    case 42: return fr < 1024u ? S((x >> 4) * R[(t >> 13) & 7u]) * E >> 15 : 0;               /* a gated pad */
    case 43: return softclip((S(x) * 3 + S(x * 2u) * 2) >> 1);                                /* driven, two partials */
    case 44: return S(x * ((((t >> 9) ^ (t >> 12)) & 7u) | 1u));                              /* xor partials */
    case 45: return S((x >> 5) * (32u + ((t >> 5) & 31u))) * E >> 15;                         /* chirps */
    case 46: return (S(x) * S(x * (1u + (st & 3u))) >> 15) + (S(x >> 1) >> 2);               /* a ring over a sub */
    case 47: return S(x + PM(S(x * 3u), S(t << 1) >> 11));                                    /* FM 3:1, its index a slow sine */
    default: a = ((st * 5u) & 7u) < 5u ? E : 0;                                               /* euclid hats, a chord */
        b = (int32_t)(((t * 2654435761u) >> 17) - 16384u) * (int32_t)(512u - (t & 511u)) >> 11;
        h = (x >> 4) * R[(st >> 1) & 7u] >> 4;                                                 /* (the chord's root walks) */
        return (S(h * 16u) + S(h * 20u) + S(h * 24u)) / 4 + (mulq15(b, a) >> 1);
    }
}
#undef S
#undef PM

/* (JIANT BYTE, phase 2) MATH plays its formulas through the FVM (fvm.c): BYTE's 48 and FLOAT's 48 as text
 * (fvm_bank.c), each part's compiled when its WAVE or ALGO changes (lofi_prog asks, the main loop compiles: c1fault,
 * fvm_want) and run a block at a time. lofi_byte / lofi_float above stay as the reference (tests/fvm_test.c). The
 * machine's work space one area a core (lofi_work; the compiler's the main loop's own). A formula
 * keeps at most LOFI_NSTATE states (ph lp hp) in the voice (v->s[6], v->s[7]); one with more is refused (silent) */
#define LOFI_NSTATE 2u
typedef union { fvm_scratch_t s; } lofi_work_t;   /* (c1fault: no compiler here any more, fvm_serve's in the main loop:
                                                      * 2 x 0.5 KiB of the pool back) */
static lofi_work_t lofi_work[2] __attribute__((section(".pool")));                     /* one a core (MC_CPU) */
static fvm_prog_t lofi_prg[NTRK];
static fvm_ins_t lofi_ins[NTRK][FVM_MAXINS] __attribute__((section(".pool")));         /* (its instructions: a part's formula the longest, 64) */
static uint16_t lofi_prg_id[NTRK];                   /* 1 + 64 x FLOAT + ALGO / 4 (0: none yet) */
static fvm_prog_t *lofi_slot(uint32_t tr)            /* a part's program, its storage given once */
{
    if (!lofi_prg[tr].ins)
        fvm_prog_init(&lofi_prg[tr], lofi_ins[tr], FVM_MAXINS);
    return &lofi_prg[tr];
}
#include "fvm_bank.c"
/* (FORM 3) a part's second program: WAVE's B, FORM's FORMULA B (a part is one engine at a time) */
static fvm_prog_t wave_prg_b[NTRK];                  /* B's program a part (A's: lofi_prg) */
static fvm_ins_t wave_ins_b[NTRK][32] __attribute__((section(".pool")));               /* (its instructions: the bank's longest fits in 32) */
static uint16_t wave_id_b[NTRK];
/* (c1fault) The audio never compiles a formula: the parser's recursion (fvm_level: ~750 B of stack a level of
 * parentheses) on core 1's 6 KiB, under a part's render, overflowed it (FUNC BANK's B: ABOUT CORES 1 (FAULT), C1 PC at
 * fvm_level). Its render asks (fvm_want) for program want in slot k (pg, its id pg_id) from the text src (a bank's:
 * FVM_ABANK, its number); the main loop compiles it and publishes it whole (engines.c fvm_serve), as math_poll does
 * USER's. Meanwhile the slot's own program if of the same kind (ids within 64: the next ALGO, the next WAVE), else
 * silence. fl: the most states (bits 0..3), FVM_AFIX FIX only, FVM_AFADE the part's voices faded into it (math_fade).
 * Slots: A (lofi_slot) tr, B (wave_prg_b) NTRK + tr, DIST EQ 2 NTRK + tr, the macros 3 NTRK + k, ARP's EQ 3 NTRK + 4:
 * each written by the one core rendering it, read by the main loop with interrupts off */
typedef struct { fvm_prog_t *pg; uint16_t *id; const char *src; uint16_t want; uint8_t fl, tr; } fvm_ask_t;
#define FVM_NASK (3u * NTRK + 5u)
#define FVM_AFIX 16u
#define FVM_AFADE 32u
#define FVM_ABANK(i) ((const char *)(uintptr_t)((i) + 1u))
static fvm_ask_t fvm_asks[FVM_NASK] __attribute__((section(".pool")));
static fvm_prog_t fvm_mute __attribute__((section(".pool")));   /* (n 0: silence) */
static __attribute__((noinline)) const fvm_prog_t *fvm_want(uint32_t k, fvm_prog_t *pg, uint16_t *pg_id, uint32_t want, const char *src, uint32_t fl)
{
    fvm_ask_t *a = &fvm_asks[k];
    if (*pg_id == want) return pg;
    a->id = pg_id; a->src = src; a->want = (uint16_t)want; a->fl = (uint8_t)fl; a->tr = (uint8_t)(k % NTRK);
    a->pg = pg;
    return (*pg_id ^ want) >> 6 ? &fvm_mute : pg;
}
/* (JIANT BYTE, phase 6) cross-mod: each part's output (its dry sum, FIX: full scale 1.0) of the block before, what a
 * formula reads as O1 .. O6; written as the parts render (fx.c xm_put), turned over at each block's start (xm_turn) */
static int32_t xm_buf[2][NTRK][CTL] __attribute__((section(".pool")));
static uint8_t xm_cur;
static uint8_t xm_want, xm_live;                     /* (byte-cpu-light) bit k: a formula read Ok this block; the
                                                      * parts written (the reads of the block before): no copy, no
                                                      * clearing of a part no formula reads (a new read: 0 a block) */
/* (engopt) a formula compiled, a first table drawn, before its part's first note: one a block (fvm_warm), in a part
 * with no voice (its idle block, not the one that starts the song with every part's at once: CPU FULL) */
static volatile uint8_t fvm_warm_tok;
static int trk_quiet(const track_t *t)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (t->v[k].active) return 0;
    return 1;
}
static void xm_turn(void)
{
    uint32_t k, c = (uint32_t)(xm_live | xm_want);
    fvm_warm_tok = 1;
    xm_cur ^= 1u;
    for (k = 0; k < NTRK; k++)                         /* (the one written now: a silent part stays 0) */
        if ((c >> k) & 1u)
            memset(xm_buf[xm_cur ^ 1u][k], 0, sizeof xm_buf[0][0]);
    xm_live = xm_want;
    xm_want = 0;
}
static void xm_put(uint32_t tr, const int32_t *b, uint32_t n)
{
    uint32_t i;
    int32_t *d = xm_buf[xm_cur ^ 1u][tr % NTRK];
    if (!((xm_live >> (tr % NTRK)) & 1u))
        return;
    for (i = 0; i < n && i < CTL; i++)
        d[i] = b[i] < -65536 ? -131072 : b[i] > 65536 ? 131072 : b[i] * 2;   /* (VOICE_FS-ish full scale -> FIX 1) */
}
static void xm_env(fvm_env_t *e, const fvm_prog_t *p)   /* O1 .. O6, when the formula reads them */
{
    uint32_t k;
    xm_want |= (uint8_t)((p->uses >> FA_O1) & ((1u << NTRK) - 1u));
    for (k = 0; k < NTRK; k++)
        e->arr[FA_O1 + k] = (p->uses >> (FA_O1 + k)) & 1u ? (const uint32_t *)xm_buf[xm_cur][k] : 0;
}
#define LOFI_N 64u                                   /* a render's samples at most (CTL); the FVM runs FVM_BLK at once */
_Static_assert(CTL <= LOFI_N, "MATH: a render longer than its arrays");
/* (devcpu2) formula_run from the RAM code (lofi_run, lofi_float_render) through a register: fl_run sits near the
 * image's end, past the reach build.py keeps from RAMTEXT (7.5 KiB lower) */
static inline __attribute__((always_inline)) void formula_run_r(const fvm_prog_t *p, const fvm_env_t *e, fvm_scratch_t *sc,
                                                                uint32_t n, uint32_t *out)
{
    void (*f)(const fvm_prog_t *, const fvm_env_t *, fvm_scratch_t *, uint32_t, uint32_t *) = formula_run;
    FM1_OPAQUE(f);
    f(p, e, sc, n, out);
}
/* the formula over m samples (the variables' arrays m long), FVM_BLK at a time */
static __attribute__((noinline)) void lofi_run(const fvm_prog_t *pg, fvm_env_t *en, lofi_work_t *wk, uint32_t m, uint32_t *out)
{
    uint32_t o, k;
    const uint32_t *a0[FA_N];
    for (k = 0; k < FA_N; k++)
        a0[k] = en->arr[k];
    for (o = 0; o < m; o += FVM_BLK) {
        for (k = 0; k < FA_N; k++)
            en->arr[k] = a0[k] ? a0[k] + o : 0;
        formula_run_r(pg, en, &wk->s, m - o < FVM_BLK ? m - o : FVM_BLK, out + o);
    }
    for (k = 0; k < FA_N; k++)
        en->arr[k] = a0[k];
}
/* (JIANT BYTE) KEY (P_KEY): what the keys do to the formula's time. LIBRE: MATH's (the note's speed, MOTION's slow
 * bits); ALTURA (ALT5 .. ALT19: SPLIT 5 .. 19): the bits above SPLIT from the song's position (lofi_sync: the rhythm and the phrase in time with the
 * bar, the same whatever the key), those below the note's (its timbre, its pitch); FIJO: every bit the song's (the
 * keys only start it). tq: the song's t at the sample */
static inline uint32_t lofi_key(uint32_t key, uint32_t te, uint32_t tq, uint32_t m)
{
    return key == LK_ALTURA ? (tq & ~m) | (te & m) : key == LK_FIJO ? tq : te;
}
/* (JIANT BYTE) CAOS (P_CAOS 0..127): 0 the formula's time as written (MATH's, bit for bit). Up from there the time is
 * cut in stretches of 2048 steps, each its own hash: a share of them (c / 128) folded (t ^ t >> k, deeper as it rises:
 * k 2 .. 11 at the start, 2 .. 4 at the top), from 48 jumps (a stretch moved by its hash, more of them and further), from
 * 96 the output back into the time (fb: the voice's last sample, the block's start). A gradual control: a little CAOS a
 * few stretches bent, all of it nothing as written */
static inline uint32_t lofi_caos(uint32_t t, uint32_t c, int32_t fb)
{
    uint32_t h;
    if (!c)
        return t;
    h = fvm_hash(t >> 11);
    if ((h & 127u) < c)
        t ^= t >> (2u + (h >> 8) % (10u - c / 16u));
    if (c >= 48u && ((h >> 16) & 127u) < (c - 48u) * 3u / 2u)
        t += h >> (24u - c / 16u);
    if (c >= 96u)
        t += (uint32_t)((fb >> 6) * (int32_t)(c - 95u));
    return t;
}
/* (JIANT BYTE) USER: a part's own formula (math_poll, in the main loop, writes it, its type, then its version) */
static char math_txt[NTRK][64] __attribute__((section(".pool")));
static uint8_t math_ty[NTRK];
static volatile uint8_t math_ver[NTRK];
/* Audio publishes effective USER controls; the main loop compiles them. */
static volatile uint32_t math_request_base[NTRK];
static volatile uint32_t math_request[NTRK]; /* bit31 active, seed bits0..7, mutation bits8..14 */
static int32_t math_last[NTRK*NVOICE];
static uint8_t math_seen[NTRK*NVOICE],math_fade[NTRK*NVOICE];
static int32_t math_transition[2][CTL] __attribute__((section(".pool")));
static const fvm_prog_t *lofi_prog(const track_t *t, lofi_work_t *w, uint32_t flt, uint32_t f)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, user = t->p[P_E1] >= RW_USER;
    uint32_t id = user ? 0x400u | math_ver[tr] : 1u + (flt ? 64u : 0u) + f;
    (void)w;
    if(user)return lofi_slot(tr); /* USER programs are validated/published by math_poll, never compiled here. */
    return fvm_want(tr, lofi_slot(tr), &lofi_prg_id[tr], id, FVM_ABANK(f % FVM_NBYTE + (flt ? FVM_NBYTE : 0u)), LOFI_NSTATE);   /*
                                                         * (c1fault: the main loop's, fvm_serve) */
}

/* (0.9.11) LOFI's level and its edges. Through the master's leveler (fx.c lev_block: peaks to ~-9 dBFS, 2:1) LOFI measured
 * as loud as the others in LUFS (-12.4 against the melodic engines' -12.3, the factory presets' medians, C4 at 100), but
 * it was heard far louder and piercing: its waves are full-scale and dense (square-ish bytebeat, naive pulses and saws:
 * a low crest factor, energy spread from 60 Hz to 16 kHz over every critical band), so a Zwicker loudness put it at
 * +6 phon over the melodic engines (BYTEBEAT over the loudest ANALOG preset), its sharpness twice theirs, ~35 dB more
 * of its energy above 6 kHz; after a quiet part the leveler, lifted, let its onset through +5 LU over the sound before.
 * LOFI_GAIN, the voice's level by WAVE (Q15, on the block's amplitude ramp): -4.5 dB PLS SAW NOIS, -1.5 TRI, -3 WRAM,
 * -6 BYTE FLOAT. LOFI_SOFT, a fixed one-pole (-3 dB at ~7.5 kHz, -6.8 at the top) on the chip waves and FLOAT, ahead
 * of the FILTER page's CUT (which keeps its scale): the aliasing pulses' and saws' top filed, the 4 .. 8 kHz bite of
 * FLOAT's FM a little; BYTE's own two poles lower (lofi_byte_render). Its state in v->ph[2] (kept on a retrigger).
 * Three or four operations a sample; user presets and old projects play quieter and softer too */
#define LOFI_GAIN 16384                                 /* (V10.6d: BYTE's and FLOAT's, -6 dB; the chips' gone) */
#define LOFI_SOFT(x, y) ((y) + ((((x) - (y)) * 5) >> 3))   /* y + 5/8 (x - y): -3 dB at ~7.5 kHz, -1.2 at 4 */

/* BYTE's samples (lofi_render's set-up done: inc the note's, the hold, the bits, the filter): t in v->s[2], its
 * fraction (Q24) in v->ph[0] */
static __attribute__((noinline)) void lofi_byte_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m, uint32_t inc, int32_t hold, int32_t bits,
                                                      const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t vr = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /* (0.6.81) LFO / ENV ->
                                                         * SHAPE move VAR (SHAPE did nothing on BYTE) */
    uint32_t f = (uint32_t)p[P_E2] >> 2, a = 1u + (vr >> 3), tt = (uint32_t)v->s[2], tf = v->ph[0], i;
    uint32_t ti = (inc >> 8) * 31u;                     /* t a sample, Q24: C4 (inc ~2.55e7) ~0.18 = 8 kHz */
    uint32_t ft = ft_voice(p);            /* (JIANT 0.5) the FILTER's type (COMB: open, voice.c combs) */
    int32_t kd = 8192 - (p[P_FRES] > 50 ? p[P_FRES] : 50) * 7600 / 127;   /* (0.5.4: the FILTER page's RES alone) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: the fold's shift, 13 .. 2 (0: none) */
    int32_t held = v->s[0], cnt = v->s[1], ic1 = v->s[4], ic2 = v->s[5], l1 = v->s[3], l2 = (int32_t)v->ph[2];
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t t0 = (uint32_t)(f < 32u ? BYTE_T0[f] : 0u) << 9;    /* (0.5.1: the formula's start) */
    lofi_tm_t tmc;
    uint32_t mul, ts = v->ph[1], var = p[P_E1] == RW_USER ? 0u : vr;   /* (0.5.4) MOTN: the slow time, Q8;
                                                         * (JIANT BYTE) USER: VAR moves the text (fvm_move), not this */
    uint32_t sy, sr, sb, tinc;                          /* (JIANT BYTE) TIME and MOTION: lofi_time */
    lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
    const fvm_prog_t *pg = lofi_prog(t, wk, 0, f);
    uint32_t tws[LOFI_N * 2u], vals[LOFI_N * 2u], ne = 0, k = 0, full = bits >= 16, h;
    uint8_t frq[LOFI_N];                                /* (JIANT BYTE) FULL: where between t and t + 1, Q8 */
    uint32_t caos = (uint32_t)clamp(p[P_CAOS], 0, 127), qr, qb, key, qm = 0;
    int32_t fb0 = held;
    lofi_time(p, &tmc);
    mul = tmc.mul; sy = tmc.sy; sb = tmc.sb; sr = tmc.sr; tinc = tmc.inc; key = tmc.key; qb = tmc.qb; qr = tmc.qr;
    if (tmc.lw) {                                       /* (loopq) SYNC's LOOP a division: no mask; a new window */
        lm = 0xFFFFFFFFu;
        if (v->ph[1] != tmc.wn)
            tt = tf = 0, cnt = 0;
    }
    hold *= 1 + (p[P_E5] >> 3);                         /* (0.5.4) GRIT: each sample held up to 16 x longer */
    int32_t vsh = (int32_t)VH_SH(), sk = vsh ? 3 : 2;   /* (as JIANT FM V10.7.56) at half the rate: the hold half as
                                                         * many samples, the two smoothing poles squared */
    hold = (hold + vsh) >> vsh;
    if (n > LOFI_N)
        n = LOFI_N;
    {   /* (JIANT BYTE) pass 1: MATH's time as ever, the times the formula is read at (a new held sample) gathered */
        uint32_t ctt = tt, ctf = tf, cts = ts, csb = sb;
        int32_t cc = cnt;
        for (i = 0; i < n; i++) {
            if (i == tmc.iw)                             /* (loopq: the song's next window, the note's t from 0) */
                csb = tmc.s2, ctt = ctf = 0, cc = 0;
            if (--cc <= 0) {
                if (sy)                                  /* (SYNC: the slow bits at the bar's position) */
                    cts = (csb + ((i * sr) >> 16)) * mul;
                for (h = 0; h <= full; h++) {            /* (FULL: at t + 1 too, its value the next one between) */
                    uint32_t te, tl, c1 = ctt + h;
                    te = (c1 & 255u) | ((cts >> 8) & ~255u);   /* (MOTN: the slow bits at
                                                         * their own speed; 64: t as written) */
                    te = pt_time(lofi_key(key, te, qb + ((i * qr) >> 16), qm), 0u);
                    tl = lofi_mut((te & lm) + t0, var);  /* (VAR: mutated) */
                    tws[ne + h * LOFI_N] = lofi_caos(bs ? tl ^ (tl >> bs) : tl, caos, fb0);
                }
                frq[ne++] = (uint8_t)(ctf >> 16);
                cc = hold;
            }
            ctf += ti;
            ctt += ctf >> 24;
            ctf &= 0xFFFFFFu;
            cts += tinc ? tinc : ((ti >> 8) * mul) >> 16;
        }
    }
    if (ne) {                                            /* the formula at those times, in one run */
        fvm_env_t en = {{tws, 0, 0, 0, 0}, {0}, &v->s[6]};
        xm_env(&en, pg);
        en.u[FU_A] = a;
        en.u[FU_NOTE] = v->note;
        en.u[FU_VEL] = v->vel;
        lofi_run(pg, &en, wk, ne, vals);
        if (full) {                                      /* (FULL: the next t's values) */
            en.arr[FA_T] = tws + LOFI_N;
            lofi_run(pg, &en, wk, ne, vals + LOFI_N);
        }
    }
    for (i = 0; i < n; i++) {                            /* pass 2: the held samples, the bits, the filters */
        if (i == tmc.iw)
            sb = tmc.s2, tt = tf = 0, cnt = 0;
        if (--cnt <= 0) {
            int32_t s;
            if (full && k < ne) {                        /* (JIANT BYTE) FULL: between this t's byte and the next's,
                                                         * 16 bits: the bytebeat's wave without its steps */
                int32_t v0 = (int32_t)(vals[k] & 255u), v1 = (int32_t)(vals[k + LOFI_N] & 255u);
                s = v0 * 256 + (v1 - v0) * (int32_t)frq[k] - 32640;
                k++;
            } else
                s = (int32_t)((k < ne ? vals[k++] : 0u) & 255u) * 256 - 32640;
            if (sy)
                ts = (sb + ((i * sr) >> 16)) * mul;
            cnt = hold;
            if (bits < 8)
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);   /* (FULL: none) */
            held = s;
        }
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += tinc ? tinc : ((ti >> 8) * mul) >> 16;
        l1 += (((held >> 1) - l1) * sk) >> 2;                  /* (JIANT 0.5) the 8-bit steps rounded: two one-poles before
                                                         * the filter (BYTE was knife-sharp); 0.9.11: at ~4.9 kHz, were
                                                         * ~6.3 (x 0.6): their images softer, the steps' edges filed */
        l2 += ((l1 - l2) * sk) >> 2;
        out[i] += voice_amp(soft_knee(svf_mode(flt, l2, &ic1, &ic2, ft, kd), 16000) << 1, m, i);
    }
    v->s[3] = l1;
    v->ph[2] = (uint32_t)l2;
    v->s[0] = held;
    v->s[1] = cnt;
    v->ph[1] = tmc.lw ? tmc.wn + (tmc.iw < n) : ts;
    v->s[2] = (int32_t)tt;
    v->s[4] = ic1;
    v->s[5] = ic2;
    v->ph[0] = tf;
}

/* FLOAT's samples (lofi_render's set-up done: inc the note's, the filter): t in v->s[2], its fraction (Q24) in v->ph[0],
 * MOTN's slow t (Q8) in v->ph[1], a formula's own state in v->s[3], the last output in v->s[0] (0.5.6) */
static __attribute__((noinline)) void lofi_float_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                       const vmod_t *m, uint32_t inc, const tsvf_t *flt)
{
    const int16_t *p = t->p;
    uint32_t f = (uint32_t)p[P_E2] >> 2, tt = (uint32_t)v->s[2], tf = v->ph[0], ts = v->ph[1], i;
    uint32_t ti = inc >> 3;                             /* (t a sample, Q24: 32 a cycle of the note) */
    lofi_tm_t tmc;
    uint32_t sy, sr, sb, tinc, mul, var = (uint32_t)clamp(p[P_E3] + ((m->shape - (64 << 8)) >> 8), 0, 127);   /*
                                                         * (0.6.81: SHAPE moves VAR, as BYTE's) */
    uint32_t bs = p[P_E6] ? 13u - (uint32_t)p[P_E6] / 11u : 0u;   /* BEND: t ^ t >> 13 .. 2 */
    if (p[P_E1] == RW_USER) var = 0;                    /* (JIANT BYTE: USER's VAR moves the text, fvm_move) */
    uint32_t lm = p[P_E7] ? (1u << (17u - (uint32_t)p[P_E7] * 12u / 127u)) - 1u : 0xFFFFFFFFu;   /* LOOP: t's window */
    uint32_t ft = ft_voice(p);
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50, kd = 8192 - rs * 7600 / 127, y1 = v->s[4], y2 = v->s[5];
    int32_t aux = v->s[3], fb = v->s[0], fbk = p[P_E5], sm = (int32_t)v->ph[2];   /* (sm: LOFI_SOFT's state; aux:
                                                         * MATH's, the FVM's states are v->s[6], v->s[7]) */
    uint32_t chip = (uint32_t)p[P_E0], qs = chip == CHIP_8BIT || chip == CHIP_FULL ? 0u : chip == CHIP_1BIT ? 15u : 12u;   /* (CHIP: the bits; FULL, USER's: none) */
    lofi_work_t *wk = &lofi_work[MC_CPU() & 1u];
    void (*ltm)(const int16_t *, lofi_tm_t *) = lofi_time;
    FM1_OPAQUE(ltm);                                    /* (devcpu: this render in the RAM code, lofi_time once a block
                                                         * in the flash, out of its direct call's reach) */
    const fvm_prog_t *pg = lofi_prog(t, wk, 1, f);
    uint32_t T[LOFI_N], W[LOFI_N], vals[LOFI_N];
    uint32_t caos = (uint32_t)clamp(p[P_CAOS], 0, 127), qr, qb, key, qm = 0;
    int32_t fb0 = fb;
    fvm_env_t en = {{T, W, 0, 0, 0}, {0}, &v->s[6]};
    ltm(p, &tmc);                                      /* (JIANT BYTE) TIME and MOTION */
    mul = tmc.mul; sy = tmc.sy; sb = tmc.sb; sr = tmc.sr; tinc = tmc.inc; key = tmc.key; qb = tmc.qb; qr = tmc.qr;
    if (tmc.lw) {                                       /* (loopq: as BYTE's) */
        lm = 0xFFFFFFFFu;
        if (v->ph[1] != tmc.wn)
            tt = tf = 0;
    }
    xm_env(&en, pg);
    en.u[FU_NOTE] = v->note;
    en.u[FU_VEL] = v->vel;
    if (n > LOFI_N)
        n = LOFI_N;
    if (!fbk) {                                          /* (JIANT BYTE) the block's times, then the formula at once */
        uint32_t ctt = tt, ctf = tf, cts = ts, csb = sb;
        for (i = 0; i < n; i++) {
            uint32_t te, tl, tw;
            if (i == tmc.iw)
                csb = tmc.s2, ctt = ctf = 0;
            if (sy)
                cts = (csb + ((i * sr) >> 16)) * mul;
            te = (ctt & 255u) | ((cts >> 8) & ~255u);
            te = pt_time(lofi_key(key, te, qb + ((i * qr) >> 16), qm), 0u);
            tl = lofi_mut(te & lm, var);
            tw = lofi_caos(bs ? tl ^ (tl >> bs) : tl, caos, fb0);
            T[i] = tw;
            W[i] = ((tw << 8 | ctf >> 16) << 3);   /* (the note's phase:
                                                         * MATH's x, in turns; TALK: the others' sound into it, FM) */
            ctf += ti;
            ctt += ctf >> 24;
            ctf &= 0xFFFFFFu;
            cts += tinc ? tinc : ((ti >> 8) * mul) >> 16;
        }
        lofi_run(pg, &en, wk, n, vals);
    }
    for (i = 0; i < n; i++) {
        uint32_t te, tl, tw;
        int32_t s;
        if (i == tmc.iw)
            sb = tmc.s2, tt = tf = 0;
        if (sy)                                          /* (p6 SYNC: the slow bits at the bar's position) */
            ts = (sb + ((i * sr) >> 16)) * mul;
        if (fbk) {                                        /* (BYTE 0.16, CPU: the time only here; without FBK the block's
                                                         * loop above made it) */
            te = (tt & 255u) | ((ts >> 8) & ~255u);
            te = pt_time(lofi_key(key, te, qb + ((i * qr) >> 16), qm), 0u);
            tl = lofi_mut(te & lm, var);
            tw = lofi_caos(bs ? tl ^ (tl >> bs) : tl, caos, fb0);                                        /* FBK: the last output moves t, up to +-16 t (half a turn of
                                                         * the sines, Q8): the formula modulates itself, FM-like; the
                                                         * output of a sample feeds the next: one sample a run */
            int32_t d = (fb * fbk) >> 10;
            uint32_t T1 = tw + (uint32_t)(d >> 8), W1 = (((tw << 8 | tf >> 16) + (uint32_t)d) << 3), o1;
            fvm_env_t e1 = {{&T1, &W1, 0, 0, 0}, {0}, &v->s[6]};   /* (FEEDBK: no cross-mod here) */
            e1.u[FU_NOTE] = v->note;
            e1.u[FU_VEL] = v->vel;
            formula_run_r(pg, &e1, &wk->s, 1, &o1);
            s = (int32_t)o1 >> 1;
        } else {
            s = (int32_t)vals[i] >> 1;                   /* (FIX Q16 -> MATH's Q15) */
        }
        if (qs)
            s = ((s + (1 << (qs - 1u))) >> qs) << qs;
        if (chip == CHIP_4B2 && (tt & 1u))                /* (4B/2: held over two t) */
            s = fb;
        fb = s;
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        ts += tinc ? tinc : ((ti >> 8) * mul) >> 16;
        sm = VH_SH() ? sm + ((((s >> 1) - sm) * 55) >> 6) : LOFI_SOFT(s >> 1, sm);   /* (as JIANT FM V10.7.56:
                                                         * at half the rate the pole squared, 1 - (3/8)^2) */
        out[i] += voice_amp(soft_knee(svf_mode(flt, sm, &y1, &y2, ft, kd), 16000) << 1, m, i);
    }
    v->ph[2] = (uint32_t)sm;
    v->s[0] = fb;
    v->s[2] = (int32_t)tt;
    v->s[3] = aux;
    v->s[4] = y1;
    v->s[5] = y2;
    v->ph[0] = tf;
    v->ph[1] = tmc.lw ? tmc.wn + (tmc.iw < n) : ts;
}

static const char MATH_FUNC_DEFAULT[] = "func{let bt=P%64;let r=bt*(1<<int(D*2.99));return sat(chord(\"0573\"[int(bt/8)]+n-60,bt%8,A)*.5+note(\"0247\"[int(r)]+n-48,r%1,A)*.5+(int(bt)-8<40)*(kick(bt%1)*.75+snare((bt+1)%(2-(B>.5)))*.5+hat(bt*(2<<int(C*2.99))%1)*.75));}";
static const char MATH_FUNC_BEAT[] = "func{let bt=P%32;let st=int(bt*4);return sat(note(n-24+(st^(st>>2))%5*3,bt*4%1,A)*.8+(bt<24)*(kick(bt%4%((\"324\"[int(D*2.99)]-48)*.5))*.8+snare((bt+1)%(2-(B>.5)))*.4+hat(bt*(2<<int(C*2.99))%1)*.6));}";
/* (BYTE 0.16, FORM 4's first recipes, the owner's references 12 and 4, docs/FORM-REFERENCIAS.md) GROWL: two formants by
 * sync on the note's cycle, a trapezoid wobble moving the vowel (A the vowel, B the wobble 1/4 .. 1/16, C the drive),
 * the notes from a table fused by the seed (D) and the section, its action eu(): two euclidean rhythms drawn and joined.
 * HALFTIME: the wobble's speed by bits fused with the seed, a ring (B), the sub inside the saturation, kick and snare
 * at half time (A the wobble's depth, C the drive, D the seed). No state: they run by blocks */
static const char MATH_FUNC_GROWL[] = "func{let k=int(P*4);let y=w*(\"44464348\"[k*3+rnd(k>>5^d)&7]-48)*.25;let f=y%1;let l=sin(P*(1+int(B*3))*.5)*.5+.5;return sat((sin(f*(3+l*6*A))*.6+sin(f*(9+l*25))*.4)*(1+C*6)*eu(k,d)+sin(y*.5)*.6);}";
static const char MATH_FUNC_HALF[] = "func{let g=P%4;let k=int(P*4);let q=sin(P*(\"1248\"[(k>>2^k>>5^d)&3]-48)*.25)*.5+.5;return sat(sat(sin(w*.5%1*(2+q*q*12*A))*sin(w*(3+B*4))*(3+C*12)*eu(k,d)+sin(w*.5)*2)*.6+kick(g)+snare((g+2)%4)*.7);}";
/* (BYTE 0.16) the rest of FORM 4's first ten, the references (docs/FORM-REFERENCIAS.md): ACID (5), REESE, HOUSE (2),
 * STRUM (1), JUST (3), PLUCK (9), NIBBLE (11), BITS (6, 8); each with the seed (D) and eu()'s action, none with state */
static const char MATH_FUNC_R_ACID[] = "func{let k=int(P*4);let h=rnd(k%16^d);let e=1-P*4%1;let q=h%4==0;let y=w*(\"4446434844864368\"[h&15]-48)*.25;let f=y%1;return sat((saw(y)*.4+sin(f*(2+e*e*A*(4+q*C*12)))*(1-f)*(.3+B*.8))*(1.5+q*C*1.4)*eu(k,d)*e*e);}";
static const char MATH_FUNC_R_REESE[] = "func{let k=int(P*2);let y=w*(\"4434\"[k+rnd(k>>3^d)&3]-48)*.25;let z=saw(y)+saw(y*(1.003+A*.01))+saw(y*.5)*.6;return sat(tri(z*(.25+B*.5))*(1+C*4))*(.6+eu(k,d)*.4);}";
static const char MATH_FUNC_R_HOUSE[] = "func{let g=P%1;let k=int(P*4);let r=\"0573\"[(k>>3)+rnd(k>>5^d)&3]-48;return sat(chord(n+r+12,P%2,A)*g*.7+sin(w*.25+sin(w*.5)*B*.3)*eu(k,d)*.6*(1-g*.5)+kick(g)*C);}";
static const char MATH_FUNC_R_STRUM[] = "func{let k=int(P*4);let g=((k%8>5?k%8-6:k%8%3)+P*4%1)/4;let r=n+\"0375\"[(k>>4)+rnd(k>>6^d)&3]-48;return sat((note(r,g,A)+note(r+3+int(B*2),g-.06,A)+note(r+7,g-.12,A))*(1+C*3)*.5);}";
static const char MATH_FUNC_R_JUST[] = "func{let k=int(P);let y=w*(\"44564454\"[k+rnd(k>>3^d)&7]-48)*.25;let f=y%1;let e=1-P%1;return sat(sin(f*f*(2+A*12)+sin(y*2)*B*.3)*e*(.4+eu(k*4,d)*.6)*(1+C*3)+sin(w*(1+e*e*e*8))*e*e*e*.5);}";
static const char MATH_FUNC_R_PLUCK[] = "func{let k=int(P*4);let y=w*(\"44464348\"[k*5+rnd(k>>4^d)&7]-48)*.25;let u=y%1*2-1;let z=u*u*u;let e=1-P*4%1;return sat(sin(z*z*z*(1+A*3)+y+sin(y*2)*B)*e*e*(.3+eu(k,d)*.7)*(1+C*3));}";
static const char MATH_FUNC_R_NIBBLE[] = "func{let k=int(P*4);let g=P*4%1;let o=k&3;let j=k+int(rnd(k>>4^d)%4*C);return sat(kick(P%1)*.9+note(n-24+(o==3)*(A>.5)*12,g,.2)*(o>0)*.8+hat((P+.5)%1*.5)*.3+note(n+12+\"0137:<\"[j%6]-48,g,B)*eu(k,d)*.35);}";
static const char MATH_FUNC_R_BITS[] = "func{let l=sin(P*(\"1213242334344344\"[int(A*3.99)*4+(rnd(int(P/4)^d)&3)]-48))*.5+.5;return sat(tri(sin(w*.5%1*(2+l*10*B))*(1+B*2))*l*.55+sin(w*.25)*C*.7+kick(P%4)*.9+snare((P+2)%4)*.7+hat(P*2%1)*.2);}";
/* (BYTE 0.16, the owner: "more extreme timbres in the palette") DRUMS: drums from the formula, no 808: a kick by a
 * folded falling sine (C its fold), a noise snare ringing (B its pitch) and square hats (A), each lane its own eu()
 * fused from the seed. (func-level, the owner: "SCREECH NIBBLE BITS noise, hard to use") their slots: PSY (11), a
 * rolling psytrance bass (the kick on the beat, the bass on the three offbeat 16ths, ROLL its octave jumps), the open
 * hat off the beat, a rising lead arpeggio on 0 b2 b3 5 b7 8 (through SCALE: fu_scale), ACID its squelch, TWIST its
 * motif; DSTEP (12), dubstep at half time (kick on 1, snare on 3, a fill kick), a wobble bass whose rate (1/4 1/8 1/8T
 * 1/16) the bar picks (WOBBLE: the set), GROWL its formant and fold, SUB; MELODY (14), a motif of four bars on the beat,
 * two of call, two of answer resolving to the fifth and the root, LEAP its range, SPACE its rests, over a breathing
 * chord (slow attack, its tail) and a bass hard-synced 1 .. 8 times its cycle (SYNC). ACID (func-level): a 303's
 * sixteen steps by the seed, rests, accents (louder, more of the sweep: ACCENT), octave jumps, ENV MOD the decay's
 * sweep, RESO; AMBIENT breathing over eight bars */
static const char MATH_FUNC_R_BEATS[] = "func{let k=int(P*4);let g=P*4%1;let q=(1-g)*(1-g);let z=rnd(T);return sat(tri(sin(g*(2+q*9))*(1+C*7))*q*eu(k,d)*1.5+z*q*(sin(g*(20+B*80))*eu(k+2,d+5)*.5+sqr(w*7)*q*eu(k*3,d+9)*A*.4));}";
static const char MATH_FUNC_R_SCREECH[] = "func{let k=int(P);let i=rnd(k%8^d^int(P/8)%2*(k%8<6))%(2+int(A*5));return (note(n+12+\"024579<\"[i]-48,P%1,.3)*(rnd(k^d)%8>=B*6)+chord(n,P%4,0)*min(P%4,1)*.8+sin(w*.5%1*(1+C*7))*.25)*.5;}";
/* (BYTE 0.17, FUNC its own engine) six more, the gaps between the others: DUB, offbeat chord stabs repeated as an echo
 * (B its time: 1/16 1/8 3/16), the sub (C), a one-drop kick and rim on 3; AMEN, a two-bar break (kick and snare as
 * masks of its 32 steps, the beats shuffled by B, the seed's pick), 8th hats (A); TRANCE, a 16th arpeggio up and down a
 * minor chord over i VI III VII, its length (B), the offbeat bass (C); AMBIENT, a drone of sines on the note, its fifth,
 * octave and sub, slowly phase-modulated (A), drifting (B), a high partial (C); MINIMAL, a 4/4 kick, offbeat hats (B),
 * a blip of FM (A) on eu()'s steps; JUNGLE, a sub on a fused table every two beats, growled (B), wobbling (A), its
 * break (kick on 1 and the "and" of 3, snare on 2 and 4). No state, drums on the 1/32 grid (tests/funcbeat_test.c) */
static const char MATH_FUNC_R_DUB[] = "func{let g=(P+.5)%1;let k=int(P);let r=n+\"0573\"[(k>>2)+rnd(k>>4^d)&3]-48;let q=g%((\"246\"[int(B*2.99)]-48)*.125);return sat(chord(r,q,A)*max(1-q*5,0)*(1-g*.7)+note(r-24,P%1,0)*C*.8+kick((P+2)%4)*.8);}";
static const char MATH_FUNC_R_AMEN[] = "func{let j=int(P*4);let m=rnd(j>>2^d);let i=((m&255)<b?m>>8&7:j>>2&7)*4+(j&3);let g=P*4%1*.25;return sat(kick(g+(4227527674>>i&1)*2)*.9+snare(g+(3178196335>>i&1)*2)*(.4+C*.5)+hat(P*2%1*.5)*A*.6);}";
static const char MATH_FUNC_R_TRANCE[] = "func{let j=int(P*4);let r=n+\"083:\"[(j>>4)+rnd(j>>6^d)&3]-48;let g=P*4%1;let h=(P+.5)%1;return sat(note(r+\"037<C<73\"[(j+d)&7]-48,g*.25,A)*max(1-g*(4.2-B*3.5),0)*(.4+eu(j,d)*.6)*1.3+note(r-24,h,0)*C*max(1-h*2.5,0));}";
static const char MATH_FUNC_R_AMBIENT[] = "func{let j=P*(.02+B*.12);return sat((sin(w+sin(w*2+j)*A*.7)*.5+sin(w*1.5+sin(j*3)*.2)*(.3+sin(j+d*.01)*.2)+sin(w*.5)*.35+sin(w*3.01)*C*.12*(1+sin(j*2.3)))*(.55+sin(P*.125)*.25));}";
static const char MATH_FUNC_R_MINIMAL[] = "func{let k=int(P*4);let g=P*4%1;let e=(1-g)*(1-g)*(1-g);return sat(kick(P%1)*.9+hat((P+.5)%1)*B*.6+sin(w*(\"4568\"[k+rnd(k>>4^d)&3]-48)*.25+sin(w*2)*e*A*2)*e*eu(k,d)*(.5+C*2)*.5);}";
static const char MATH_FUNC_R_JUNGLE[] = "func{let j=int(P*4);let g=P*4%1*.25;let y=w*(\"4436\"[(j>>3)+rnd(j>>5^d)&3]-48)*.125;return sat(sin(y+sin(y*2)*B*.6)*(.7+C*1.5)*(1-sin(P*.5)*A*.45)+kick(g+(4227791870>>j&1)*2)*.8+snare(g+(4025479151>>j&1)*2)*.5);}";
static const char *const MATH_FUNC_R[] = {MATH_FUNC_R_ACID,MATH_FUNC_R_REESE,MATH_FUNC_R_HOUSE,MATH_FUNC_R_STRUM,MATH_FUNC_R_JUST,MATH_FUNC_R_PLUCK,MATH_FUNC_R_NIBBLE,MATH_FUNC_R_BITS,MATH_FUNC_R_BEATS,MATH_FUNC_R_SCREECH,
    MATH_FUNC_R_DUB,MATH_FUNC_R_AMEN,MATH_FUNC_R_TRANCE,MATH_FUNC_R_AMBIENT,MATH_FUNC_R_MINIMAL,MATH_FUNC_R_JUNGLE};
#define FUNC_NFORM (6u+NELEM(MATH_FUNC_R))       /* CUSTOM, SONG, BEAT, GROWL, HALFTIME and the rest; BANK last */
/* (funcbank, the owner: "the formulas of the first FUNC, the ones that made chords with the harmony") BANK: FORM 3's
 * formula bank (fvm_bank.c FB_*, curated: in families, each in tune with the chord and at its level) as a recipe: its
 * knobs FORM A, FORM B (by name), MORPH (A to B, equal power), DEF (the formula's A); w w3 w5 the chord's root, third
 * and fifth from SCL > HARMONY (its LENGTH the chords' changes), else the key's (the part's SCALE): funcplay.c
 * func_bank_run. A in the part's program (math_poll), B in its second (wave_prg_b: a part is one engine at a time) */
#define FUNC_BANK (FUNC_NFORM-1u)
static const char *const N_FUNC_FORM[]={"CUSTOM","SONG","BEAT","GROWL","HALFTIME","ACID","REESE","HOUSE","STRUM","JUST","PLUCK",
    "PSY","DSTEP","DRUMS","MELODY","DUB","AMEN","TRANCE","AMBIENT","MINIMAL","JUNGLE","BANK"};
static const char *const N_FUNC_RUN[]={"-","-","-","-","TRIG","PLAY","FREE"};
_Static_assert(NELEM(N_FUNC_FORM)==FUNC_NFORM,"FUNC: a name for every FORM");
static const engine_t ENG_FUNC;                         /* (BYTE 0.17) below: FUNC, an engine of its own */
/* (BYTE 0.17) a part plays FUNC: the FUNC engine (its WAVE always FUNC; a MATH WAVE FUNC, a store's or the editor's, is
 * moved to it by the next math_poll: engines.c math_poll_ws). One compare: the RAM code's callers (seq.c, voice.c) */
static inline __attribute__((always_inline)) int func_on(const track_t*t){return t->eng_req==ENGI_FUNC;}
static uint32_t func_clock_begin,func_clock_end;
static uint8_t func_song_mask;
/* (BYTE 0.16, the owner: "that PLAY starts the formula: set it explicitly; by default a key sounds it") RUN: NOTE (the
 * key pressed sounds it, at its pitch; the default), PLAY (the transport's: its voice and clock, as SONG was), FREE (it
 * runs on its own: a voice always, its own clock, at the root) */
static inline int func_song(const track_t*t){return func_on(t)&&t->p[P_E0]==5;}
static inline int func_free(const track_t*t){return func_on(t)&&t->p[P_E0]==6;}
/* (BYTE 0.16, the owner: "the whole formula is never heard") the formula's clock: the transport's in PLAY and NOTE
 * while the song plays (the key its pitch and gate, the formula where the song is: a 1/16 step a sixteenth of it, in
 * place); stopped, NOTE from the key, FREE its own */
#define func_bar(t) (song.playing&&!func_free(t))
static int math_is_func(const char*s){while(*s&&(uint8_t)*s<=32u)s++;return s[0]=='f'&&s[1]=='u'&&s[2]=='n'&&s[3]=='c';}
static const char *func_form_text(int32_t f){return f==1?MATH_FUNC_DEFAULT:f==2?MATH_FUNC_BEAT:f==3?MATH_FUNC_GROWL:f==4?MATH_FUNC_HALF:f>=5&&f<(int)FUNC_BANK?MATH_FUNC_R[f-5]:0;}
/* (funcbank) BANK's formula k (0 A, 1 B) of part t, into o (256 B); its number */
static uint32_t func_bank_f(const track_t*t,uint32_t k){return (uint32_t)clamp(t->p[P_E2+k],0,FB_N-1u);}
static void func_bank_text(const track_t*t,uint32_t k,char*o){fvm_bank_to(2u*FVM_NBYTE+func_bank_f(t,k),o);}
static const char *math_func_text(const track_t*t,const char*custom){
    const char*s=func_form_text(t->p[P_E7]);
    if(t->p[P_E7]==(int)FUNC_BANK){func_bank_text(t,0,fvm_tx);return fvm_tx;}   /* (funcbank: A's; the main loop's buffer) */
    return s?s:math_is_func(custom)?custom:MATH_FUNC_DEFAULT;}
/* (funcplay, the owner: "FUNC to play, not to listen to") FUNC played live (funcplay.c): its SECTION (GROOVE as written,
 * INTRO BUILD DROP BREAK: at the next bar line), a FILL held (ROLL STUTTER REVERSE DOUBLE: from the next 1/16 to the 1/16
 * after the key is let go), the mutations (NOTES RHYTHM SWAP FREEZE), the parts muted (bit 0 the drums, 1 the notes,
 * 2 layer B), the root's degree; what the panel and the render read. The keys (seq.c key_on) and GEN (func_gen_set) write
 * it; the pool's (zeroed: GROOVE, nothing held) */
enum { FS_GROOVE, FS_INTRO, FS_BUILD, FS_DROP, FS_BREAK };
enum { FF_ROLL = 1, FF_STUT, FF_REV, FF_DBL };
enum { FM_NOTES = 1, FM_RHYTHM = 2, FM_SWAP = 4, FM_FREEZE = 8 };
typedef struct {
    uint8_t sec, sreq, dreq, fill, fkey, mut, mute;   /* (sreq dreq: + 1, 0 none; these and semi gden: live) */
    int8_t semi, gden, dsemi, cut;      /* the root's semitones, GEN's density, the semitones asked, the block's CUT move */
    uint8_t deg, mkey, mreq, mpf;       /* (momentary MUTATE) the mutations' keys held, pressed (not yet on), pending
                                         * (1 on at ms, 2 off at mr); mut: those playing */
    uint32_t qbar, sbar, fs, fe, frz, lastP;   /* bars, Q16 beats: the asked's line, the section's first, the fill's
                                         * start and end (0 open), the bar frozen; the last P */
    uint32_t ms, me, mr;                /* (momentary MUTATE) on at, held at least until, off at (Q16 beats) */
    uint16_t act;                       /* (func-rate) A's RATE + 3, B's << 3, B OCT + 2 << 6 as playing (fp_kp's: at
                                         * the bar playing, OCT at once), each xor its middle: 0 as written */
    int32_t bty;                        /* (func-rate) B TONE's pole */
} fp_t;
static fp_t fp_st[NTRK] __attribute__((section(".pool")));
/* (funcmotion, the owner: "FUNC to move its stereo and its timbre by the formula") the matrix's FN HIT FN NOTE FN BAR
 * (mod.c MS_FNHIT ..): a FUNC part's formula as sources, from what it plays (lofi_func_render, fn_track: its latest
 * voice, a block late): HIT its onsets (1 at each, decaying ~0.1 s), NOTE how high it plays (its zero crossings, smoothed), BAR its
 * time's place in four bars (bipolar, a triangle); side: L or R at each onset, by its 1/16 (the same each time round). Any other part's: 0 */
typedef struct { int16_t v[3], slow, line; uint8_t side, rsv; } fn_sg_t;
#define FN_SRC0 29u                                     /* (mod.c MS_FNHIT: checked there) */
static int fn_read(const track_t *t)                    /* a MOD slot of t reads FN HIT NOTE BAR */
{
    uint32_t k, r = 0;
    for (k = 0; k < 12u; k += 3u) r |= (uint32_t)t->p[P_M1SRC + k] - FN_SRC0 < 3u;
    return (int)r;
}   /* (line: NOTE's own, slowly) */
static fn_sg_t fn_sg[NTRK] __attribute__((section(".pool")));
/* (funcplay) a FUNC part's LAYERS' BLEND (0 A .. 64 both .. 127 B) and KEYS (0 ON), the project's 'F' 'P'; (funcmotion)
 * its MOTION: STEREO MOVE TIMBRE COLOR (FUNC_PX; mod.c func_mod), the project's 'F' 'M' (none: 0, as before); (func-rate)
 * its RATE (-3 .. 3: 1/4X .. 1X .. 4X) and layer B's LEVEL (dB) TONE (a tilt) OCT RATE, the project's 'F' 'X' (none,
 * all 0: as before) */
enum { FK_RATE = 6, FK_BLEV, FK_BTONE, FK_BOCT, FK_BRATE, FK_N };
static int16_t fp_kp[NTRK][FK_N] __attribute__((section(".pool")));
typedef struct { fvm_prog_t p; uint8_t tr, form; } fpb_t;   /* layer B's program: two parts at once (the pool's room) */
static fpb_t fp_b[2] __attribute__((section(".pool")));
static fvm_ins_t fp_bins[2][FVM_MAXINS] __attribute__((section(".pool")));
static uint32_t func_run(track_t*t,voice_t*v,const fvm_prog_t*pa,fvm_env_t*en,uint32_t n,uint32_t*Y,uint32_t*W,uint32_t*P);
static void func_bank_run(track_t*t,voice_t*v,const fvm_prog_t*pa,const fvm_env_t*en,uint32_t n,uint32_t*Y);
static uint32_t func_form(const track_t*t);
/* layer A's block: BANK's (its two formulas, the chord) or the recipe's */
static void func_arun(track_t*t,voice_t*v,const fvm_prog_t*pa,fvm_env_t*en,uint32_t n,uint32_t*Y)
{
    if(func_form(t)==FUNC_BANK)func_bank_run(t,v,pa,en,n,Y);
    else lofi_run(pa,en,&lofi_work[MC_CPU()&1u],n,Y);
}
static int func_live(const track_t*t);
static uint32_t math_music_cost(const track_t*t){
    if(t->p[P_E1]!=RW_FUNC)return 2u;
    const fvm_prog_t*p=lofi_slot((uint32_t)(t-trk)%NTRK);uint32_t cost=2;
    for(uint32_t k=0;k<p->n;k++)if(p->ins[k].op==FO_MUSIC)cost+=p->ins[k].k==1u?4u:1u;
    return cost;
}
/* (BYTE punchfx) FUNC's P through the white punch-ins (formula_perf.c pt_beat_x): the block's ends, the samples between
 * on the line joining them (a block is ~1.5 ms, the punch-ins' windows a 1/32 at least); a window's edge inside it
 * (the two ends further apart than twice the time's own run): each sample */
static __attribute__((noinline)) void pt_beat_blk(uint32_t *P, uint32_t n, uint32_t song_p)
{
    uint32_t a = pt_beat_x(P[0], song_p), z = pt_beat_x(P[n - 1u], song_p), i;
    int32_t d = (int32_t)(z - a), s = (int32_t)(P[n - 1u] - P[0]);
    if (n > 1u && d <= 2 * s + 2 && d >= -2 * s - 2) {
        int32_t k = d * 256 / (int32_t)(n - 1u);
        for (i = 0; i < n; i++)
            P[i] = (a + (uint32_t)((k * (int32_t)i) >> 8)) & 0x7fffffffu;
    } else
        for (i = 0; i < n; i++)
            P[i] = pt_beat_x(P[i], song_p) & 0x7fffffffu;
}
/* (funcmotion) the block's sources of FUNC part g (mod.c fn_sg) from its formula's n samples Y (every other: its peak
 * within a bit, its zero crossings) and its time p0. Once a block, the latest voice's: no cost a sample */
static __attribute__((noinline)) void fn_track(fn_sg_t*g,const uint32_t*Y,uint32_t n,uint32_t p0)
{
    uint32_t i,zc=0;
    int32_t pk=0,pr=0,z,s=g->slow;
    for(i=0;i<n;i+=2u){int32_t y=(int32_t)Y[i]>>1;pk|=y^y>>31;zc+=(uint32_t)(y^pr)>>31;pr=y;}
    pk=pk>32767?32767:pk;
    z=(int32_t)(zc*131068u/n);
    if(pk-s>(s>>1)+1500&&g->v[0]<12000){g->v[0]=32767;g->side=(uint8_t)(fvm_hash(p0>>14)&1u);}   /* an onset: well over its slow level */
    else g->v[0]=(int16_t)(g->v[0]-(g->v[0]>>7));
    g->slow=(int16_t)(s+((pk-s)>>4));
    g->v[1]=(int16_t)(g->v[1]+(((z>32767?32767:z)-g->v[1])>>3));
    g->line=(int16_t)(g->line+((g->v[1]-g->line)>>8));
    p0=p0>>4&0xFFFFu;                                   /* (four bars: 2^20) */
    g->v[2]=(int16_t)((int32_t)(p0<32768u?p0:65535u-p0)*2-32767);
}
/* (funcmotion) COLOR c (1 .. 127) on x: up to ~9 times into a triangle's fold; past 64 its low bits dropped too */
static inline int32_t fn_color(int32_t x,int32_t c)    /* (func-smooth: c Q8, 0 .. 127 << 8; 0 x as it is) */
{
    uint32_t u=(uint32_t)((x*(4096+c)>>12)+32768)&0x1FFFFu;
    int32_t y=u<65536u?(int32_t)u-32768:98304-(int32_t)u;
    return c>64*256?y&-(1<<((c-64*256)>>10)):y;
}
/* .. on the block's formula, in place; (func-smooth, the owner: "it clicks while I morph") from c0 (the block before's)
 * to c a sample at a time, never a step at the block's edge */
static __attribute__((noinline)) void fn_color_blk(uint32_t*Y,uint32_t n,int32_t c0,int32_t c)
{
    int32_t dc=(c-c0)/(int32_t)n;
    for(uint32_t i=0;i+1u<n;i++){c0+=dc;Y[i]=(uint32_t)(fn_color((int32_t)Y[i]>>1,c0)*2);}   /* (the fold wraps: no clamp first) */
    Y[n-1u]=(uint32_t)(fn_color((int32_t)Y[n-1u]>>1,c)*2);
}
/* (func-smooth) knob k's 0 .. 1 (Q16) as the formula reads it, voice v's: at the note's start (st) as it is, then each
 * block a sixteenth of the way (~23 ms): a knob turned (or moved by TIMBRE, the matrix) never steps the formula. Its
 * state in s[0] s[1] (two a word, halved: 0 .. 32768); still, exactly the knob's */
static __attribute__((noinline)) uint32_t fn_knob(voice_t*v,uint32_t k,uint32_t a,int st)
{
    uint32_t*w=(uint32_t*)&v->s[k>>1],h=(k&1u)*16u,x=*w>>h&0xFFFFu,y=a>>1;
    int32_t d=(int32_t)y-(int32_t)x;
    x=st||(d>>4)==0?y:x+(uint32_t)(d>>4);
    *w=(*w&~(0xFFFFu<<h))|x<<h;
    return x==y?a:x<<1;
}
/* (func-rate) P (Q16 beats) times RATE r (0 .. 6: 1/4 1/2 2/3 1 3/2 2 4), exactly: on the grid, never drifting */
static __attribute__((noinline)) void fn_rate(const uint32_t*P,uint32_t*D,uint32_t n,uint32_t r)
{
    for(uint32_t i=0;i<n;i++){
        uint32_t x=P[i];
        x=r==0?x>>2:r==1?x>>1:r==2?(x<<1)/3u:r==4?x+(x>>1):r==5?x<<1:r==6?x<<2:x;
        D[i]=x&0x7fffffffu;
    }
}
static uint32_t scale_mask(const track_t *t);          /* (seq.c) */
static uint32_t func_form(const track_t*t);
static uint32_t func_lv(uint32_t f);
/* (func-scale) the music calls onto part t's SCALE (fvm_music.c fu_scale): none for MAJ CHR (the recipes as written)
 * and CUSTOM; the scale's seven degrees (one of fewer notes spread over them) */
static __attribute__((noinline)) void func_scl(const track_t*t,fvm_env_t*en)
{
    uint32_t m=scale_mask(t)&0xFFFu,S[12],c=0,j,x=0;
    if(m==0xAB5u||m==0xFFFu||t->p[P_E7]<1||t->p[P_E7]>=(int)FUNC_NFORM){en->u[FU_SCL]=0;return;}
    for(j=0;j<12u;j++)if(m>>j&1u)S[c++]=j;
    for(j=0;j<7u;j++)x|=S[(j*c+3u)/7u]<<4u*j;
    en->u[FU_SCL]=1u<<16|m|(uint32_t)(t->p[P_ROOT]%12)<<12;en->u[FU_SCT]=x;
}
static __attribute__((noinline)) void lofi_func_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m,uint32_t inc,const tsvf_t*flt)
{
    uint32_t T[LOFI_N],W[LOFI_N],S[LOFI_N],P[LOFI_N],Y[LOFI_N],q=(uint32_t)v->s[2],ph=v->ph[0],rate;
    const fvm_prog_t*p=lofi_slot((uint32_t)(t-trk));
    fvm_env_t en={{T,W,S,0,0},{0},&v->s[6]};xm_env(&en,p);
    for(uint32_t k=func_form(t)==FUNC_BANK?2u:0u;k<4;k++){uint32_t a=(uint32_t)clamp(t->p[P_E2+k],0,127);en.u[FU_A+k]=a*2u;en.u[FU_AF+k]=fn_knob(v,k,a*65536u/127u,!q);}   /*
                                                         * (funcbank: FORM A B picks, no glide) */
    en.u[FU_NOTE]=(uint32_t)clamp((int32_t)v->note+(t->pfx_pit+pt_vp[(uint32_t)(t-trk)%NTRK]-pt_wob)/16,0,127);   /* (BYTE
                                                         * punchfx) OCT- / OCT+ and CAOS MAX's throw on n too: its notes
                                                         * are n's, not the voice's pitch (WOBBLE: in w, and P swayed) */
    en.u[FU_VEL]=v->vel;en.u[FU_HZ]=(uint32_t)(((uint64_t)inc*FS)>>16);
    en.u[FU_BEAT]=lofi_sync(&rate)*8u;en.u[FU_STEP]=t->seq_idx;
    en.u[FU_BPS]=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);en.arr[FA_BEAT]=P;func_scl(t,&en);
    if(n>LOFI_N)n=LOFI_N;
    if(!n)return;
    uint32_t pos,step,rem,carry=0,den,sh=VH_SH(),sd=(uint32_t)FS;
    if(func_bar(t)){pos=func_clock_begin;den=n;step=(func_clock_end-pos)/den;rem=(func_clock_end-pos)%den;}
    else {uint64_t ticks=(uint64_t)q<<16;den=en.u[FU_BPS];pos=(uint32_t)(ticks/den);carry=(uint32_t)(ticks%den);step=(65536u<<sh)/den;rem=(65536u<<sh)%den;}
    /* Exact integer interpolation: no 64-bit division in the sample loop. (BYTE 0.16) Nor S's (the seconds, Q16): once a
     * block, then stepped; at half the rate (VH_SH: MATH always at 22 kHz) each sample two of t, its time whole */
    uint64_t s0=(uint64_t)q*65536u;uint32_t sv=(uint32_t)(s0/sd),sr=(uint32_t)(s0%sd),ss=(65536u<<sh)/sd,srm=(65536u<<sh)%sd;
    for(uint32_t i=0;i<n;i++){T[i]=q+(i<<sh);W[i]=ph>>16;S[i]=sv;sv+=ss;sr+=srm;if(sr>=sd){sr-=sd;sv++;}ph+=inc;P[i]=pos&0x7fffffffu;pos+=step;carry+=rem;while(carry>=den){carry-=den;pos++;}}
    if(pt_ops&PT_BEAT_OPS)pt_beat_blk(P,n,func_bar(t)); /* (BYTE punchfx) the white punch-ins on FUNC's time: its P (a
                                                         * LOOP's only on the song's: FREE, stopped, its own clock) */
    uint32_t ra=(fp_st[(uint32_t)(t-trk)%NTRK].act&7u)^3u;   /* (func-rate) A's RATE as playing */
    int32_t lv=(int32_t)func_lv(func_form(t));          /* (func-level) the FORM's level, before the knee (with B: the
                                                         * louder one's, func_run) */
    if(func_live(t))lv=(int32_t)func_run(t,v,p,&en,n,Y,W,P);   /* (funcplay) its sections, fills, groove, layer B */
    else{if(ra!=3u)fn_rate(P,P,n,ra);func_arun(t,v,p,&en,n,Y);}
    int32_t y1=v->s[4],y2=v->s[5],kd=8192-(t->p[P_FRES]>50?t->p[P_FRES]:50)*7600/127;
    uint32_t tr=(uint32_t)(t-trk)%NTRK;
    fn_sg_t*g=&fn_sg[tr];const int16_t*k=fp_kp[tr];
    int32_t c=k[5]|k[4]?clamp(k[5]+(k[4]*(g->v[2]+g->v[0]-16384)>>16),1,127)<<8:0,c0=q?(int32_t)v->ph[2]:c;   /* (funcmotion)
                                                         * COLOR, TIMBRE's sway (Q8; func-smooth: from the block before's) */
    if(v==&t->v[t->m_vi%NVOICE]&&(k[2]|k[4]||fn_read(t)))fn_track(g,Y,n,P[0]);   /* (funcmotion) the sources: its latest
                                                         * voice's, when MOTION or a slot reads them (else no work) */
    if(c|c0)fn_color_blk(Y,n,c0,c);
    v->ph[2]=(uint32_t)c;
    for(uint32_t i=0;i<n;i++){int32_t sample=fvm_sample(FT_FIX,Y[i])*lv>>7;out[i]+=voice_amp(soft_knee(svf_mode(flt,sample>>1,&y1,&y2,ft_voice(t->p),kd),16000)<<1,m,i);}
    v->s[2]=(int32_t)(q+(n<<sh));v->ph[0]=ph;v->s[4]=y1;v->s[5]=y2;
}

/* (func-tame) FUNC's A B C D by FORM: their names on the cards, the matrix, the editor (all through lofi_desc), and a
 * line each in the header while one turns (ui_input.c knob_hint, MENU > DISPLAY > HELP). CUSTOM (and a LOOP past the
 * recipes', as math_func_text) the program's own A B C D. Flash only; the stored values the same (0 .. 127). (funcmotion,
 * flash) their names alone in flash, each part's four cards made from them (fk_d: its form's, the same whoever asks,
 * the audio's matrix or the panel; 0 .. 127, at 64 32 16 0) */
/* (func-rate, flash) one string each, four a FORM ("" SEED / its tip): func_kn func_tip walk them */
static const char FUNC_KN_S[]="A\0""B\0""C\0""D\0"
    "COLOR\0""SNARE\0""HATS\0""STEPS\0"
    "COLOR\0""SNARE\0""HATS\0""KICK\0"
    "VOWEL\0""WOBBLE\0""DRIVE\0""\0"
    "DEPTH\0""RING\0""DRIVE\0""\0"
    "ENVMOD\0""RESO\0""ACCENT\0""\0"
    "DETUNE\0""TONE\0""DRIVE\0""\0"
    "CHORD\0""BASS\0""KICK\0""\0"
    "COLOR\0""CHORD\0""DRIVE\0""\0"
    "BRIGHT\0""FM\0""DRIVE\0""\0"
    "SHAPE\0""TONE\0""DRIVE\0""\0"
    "ROLL\0""ACID\0""TWIST\0""\0"
    "WOBBLE\0""GROWL\0""SUB\0""\0"
    "HATS\0""SNARE\0""KICK\0""\0"
    "LEAP\0""SPACE\0""SYNC\0""\0"
    "CHORD\0""ECHO\0""BASS\0""\0"
    "HATS\0""CHOP\0""SNARE\0""\0"
    "TONE\0""GATE\0""BASS\0""\0"
    "FM\0""DRIFT\0""SHINE\0""\0"
    "BLIP\0""HATS\0""DRIVE\0""\0"
    "WOBBLE\0""GROWL\0""DRIVE\0""\0"
    "FORM A\0""FORM B\0""MORPH\0""DEF\0";
#define FUNC_TIPS_S_TEXT "CONTROL A, 0 TO 1\0""CONTROL B, 0 TO 1\0""CONTROL C, 0 TO 1\0""CONTROL D, 0 TO 1\0" \
    "CHORD + MELODY TONE\0""EVERY 2 BEATS OR 1\0""2, 4 OR 8 A BEAT\0""1, 2 OR 4 A BEAT\0" \
    "BASS HARMONICS\0""EVERY 2 BEATS OR 1\0""2, 4 OR 8 A BEAT\0""3-3-2, 4 OR 2 A BAR\0" \
    "FORMANT OPENING\0""2, 4, 6, 8 A BAR\0""SATURATION\0""\0" \
    "WOBBLE DEPTH\0""RING MOD PITCH\0""SATURATION\0""\0" \
    "DECAY OPENS TONE\0""RESONANCE SQUELCH\0""ACCENT STRENGTH\0""\0" \
    "SAW SPREAD\0""FOLD BRIGHTNESS\0""SATURATION\0""\0" \
    "MINOR, MAJOR, 7TH\0""BASS FM GROWL\0""KICK LEVEL\0""\0" \
    "NOTE HARMONICS\0""MINOR, MAJOR, SUS4\0""SATURATION\0""\0" \
    "PHASE BEND\0""FM AMOUNT\0""SATURATION\0""\0" \
    "PLUCK SHARPNESS\0""FM BODY\0""SATURATION\0""\0" \
    "BASS: OCTAVE JUMPS\0""LEAD SQUELCH\0""LEAD MOTIF TWISTS\0""\0" \
    "WOBBLE RATES\0""FORMANT + FOLD\0""SUB BASS LEVEL\0""\0" \
    "HATS LEVEL\0""SNARE PITCH\0""PUNCH, FOLDED\0""\0" \
    "MELODY LEAPS\0""RESTS, SPACE\0""BASS HARD SYNC 1-8X\0""\0" \
    "MINOR, MAJOR, 7TH\0""REPEAT 1/16 1/8 3/16\0""SUB BASS LEVEL\0""\0" \
    "8TH HATS LEVEL\0""BEATS SHUFFLED\0""SNARE LEVEL\0""PICKS THE CHOPS\0" \
    "NOTE HARMONICS\0""NOTE LENGTH\0""OFFBEAT BASS\0""\0" \
    "FM SHIMMER\0""SLOW TO FASTER\0""HIGH PARTIAL\0""PHASES OF PARTIALS\0" \
    "BLIP FM DEPTH\0""OFFBEAT HAT LEVEL\0""BLIP LEVEL\0""\0" \
    "VOLUME WOBBLE\0""SUB FM GROWL\0""BASS LEVEL\0""\0" \
    "THE FIRST FORMULA\0""THE SECOND FORMULA\0""FORM A .. FORM B\0""THE FORMULAS' A\0"
#include "text_z.h"
static const char *fk_walk(const char*s,uint32_t n){while(n--)while(*s++){}return s;}
static const char *func_kn(uint32_t f,uint32_t k){const char*s=fk_walk(FUNC_KN_S,(f%FUNC_NFORM)*4u+(k&3u));return *s?s:"SEED";}
static const char *func_tip(uint32_t f,uint32_t k){const char*s=fk_walk(FUNC_TIPS_S,(f%FUNC_NFORM)*4u+(k&3u));return *s?s:"PICKS NOTES + RHYTHM";}
static uint32_t func_form(const track_t*t){return t->p[P_E7]>=1&&t->p[P_E7]<(int)FUNC_NFORM?(uint32_t)t->p[P_E7]:0u;}
/* (func-level, the owner: "AMBIENT and GROWL far too loud, ACID too, next to DUB and MINIMAL") each FORM's level (128 as
 * written), measured: the recipes' loudness (K-weighted, 8 bars at their preset, MOTION on) within 1.5 dB of DUB's
 * and MINIMAL's; on the formula's samples (lofi_func_render), layer B's at its own (func_brel) */
static const uint8_t FUNC_LV[FUNC_NFORM]={128,129,85,58,58,216,32,127,56,48,105,124,89,182,144,129,137,178,137,127,53,128};   /*
                                                         * (funcbank: BANK's each formula's own, FB_LV) */
static uint32_t func_lv(uint32_t f){return FUNC_LV[f%FUNC_NFORM];}
/* layer B's BLEND gain g (funcplay.c func_run): its FORM's level over L (the louder layer's, applied after), B GAIN's
 * dB on it; 0 .. 65535 */
static uint32_t func_brel(const track_t*t,uint32_t fb,uint32_t g,uint32_t L)
{
    static const uint16_t DB6[6]={2048,2299,2580,2896,3251,3649};   /* 2^(r/6), Q11 */
    uint32_t d=(uint32_t)(fp_kp[(uint32_t)(t-trk)%NTRK][FK_BLEV]+18);   /* (its dB + 18: 1 .. 3 octaves down, 1 up) */
    uint32_t x=g*func_lv(fb)/L*DB6[d%6u]>>(14u-d/6u);
    return x>65535u?65535u:x;
}

/* (funcplay) GROOVE (every recipe's: on its time and its calls, funcplay.c) and LAYERS: their knobs (core.h P_FDEN ..),
 * BLEND and KEYS views of fp_kp (params.c ID_FP0) */
static const char *const N_FUNC_FB[]={"OFF","SONG","BEAT","GROWL","HALFTIME","ACID","REESE","HOUSE","STRUM","JUST","PLUCK",
    "PSY","DSTEP","DRUMS","MELODY","DUB","AMEN","TRANCE","AMBIENT","MINIMAL","JUNGLE"};
_Static_assert(NELEM(N_FUNC_FB)==FUNC_BANK,"FUNC: FORM B's names (BANK not a layer: its own A and B)");
static const char *const N_FKEYS[]={"ON","OFF",0};
static const char *const N_FMOVE[]={"NOTE","HIT","BAR","ALL",0};
/* (func-rate, the owner: "a RATE, on the tempo") the formula's time (P) times one of these: on the grid, never drifting */
static const char *const N_FRATE[]={"1/4X","1/2X","2/3X","1X","3/2X","2X","4X",0};
static const param_desc_t FUNC_PX[16]={{"DENSITY",F_BIPCT,-64,63,0,0},{"SWING",F_PCT,0,127,0,0},{"COMPLEX",F_PCTINV,0,127,127,0},
    {"RANGE",F_BIPCT,-64,63,0,0},{"FORM B",F_ENUM,0,FUNC_BANK-1u,0,N_FUNC_FB},{"BLEND",F_PCT,0,127,64,0},
    {"KEYS",F_ENUM,0,1,0,N_FKEYS},{"STEREO",F_PCT,0,127,0,0},{"MOVE",F_ENUM,0,3,0,N_FMOVE},{"TIMBRE",F_PCT,0,127,0,0},
    {"COLOR",F_PCT,0,127,0,0},{"RATE",F_ENUM,-3,3,0,N_FRATE+3},{"B GAIN",F_INT_DB,-12,6,0,0},{"B TONE",F_BIPCT,-64,63,0,0},
    {"B OCT",F_INT,-2,2,0,0},{"B RATE",F_ENUM,-3,3,0,N_FRATE+3}};
static const char FUNC_PXT[] = "THIN .. FILL HITS\0ODD 1/16S LATER\0STRAIGHT..BROKEN\0NOTES 8VA DOWN..UP\0A SECOND RECIPE\0A .. A+B .. B\0SECTIONS, FILLS\0PAN BY THE FORMULA\0PAN: NOTE HIT BAR\0HITS MOVE CUT + A\0FOLD .. CRUSH\0TIME ON THE TEMPO\0UNDER A .. OVER\0DARK .. BRIGHT\0OCTAVES DOWN, UP\0B'S TIME";   /* (str_nth: a text each, no pointer each) */
static const uint8_t FUNC_PID[5]={P_FDEN,P_FSWG,P_FCPX,P_FRNG,P_FFB};
static const param_desc_t *func_pdesc(uint32_t id)    /* (params.c track_desc: a FUNC part's) */
{
    uint32_t k;
    for(k=0;k<5u;k++)if(FUNC_PID[k]==id)return &FUNC_PX[k];
    return 0;
}
static const param_desc_t *func_view(uint32_t k,int16_t**valp)   /* (params.c page_desc: LAYERS' BLEND, KEYS; MOTION's) */
{
    *valp=&fp_kp[song.sel%NTRK][k%FK_N];
    return &FUNC_PX[5u+k%FK_N];
}
/* (funcmotion) the FUNC presets' MOTION: STEREO MOVE TIMBRE COLOR a nibble each (x 8; MOVE as it is) */
static const uint16_t FUNC_MO[FUNC_NFORM-1u]={0x6340,0x4142,0x2240,0x2253,0x3230,0x4243,0x7151,0x8030,0x6041,0x7152,
    0x4320,0x3220,0x6141,0x6230,0x6252,0x5131,0x6351,0x9240,0x8162,0x3240,0x4100};   /* (func-level: GROWL ACID JUNGLE no
                                                         * COLOR, PSY DSTEP MELODY new) */
static void func_motion(track_t*t,uint32_t pi)        /* (ui.c apply_preset_to) */
{
    uint32_t k,m=FUNC_MO[pi%(FUNC_NFORM-1u)];
    for(k=0;k<4u;k++)fp_kp[(uint32_t)(t-trk)%NTRK][2u+k]=(int16_t)((m>>(12u-4u*k)&15u)<<(k==1u?0:3));
}
/* a FUNC part as written: GROOVE and LAYERS at their middles (apply_preset_to, project.c, math_poll) */
static void func_neutral(track_t*t)
{
    uint32_t k;
    for(k=0;k<5u;k++)t->p[FUNC_PID[k]]=FUNC_PX[k].def;
    int16_t*q=fp_kp[(uint32_t)(t-trk)%NTRK];
    memset(q+1,0,(FK_N-1u)*sizeof*q);q[0]=64;           /* (funcmotion: MOTION still; func-rate: 1X, B as A's) */
}
static param_desc_t fk_d[NTRK][4];
static uint8_t fk_f[NTRK][4];                           /* (its label's FORM + 1: walked once, not each block's ask) */
static const param_desc_t *func_knob(const track_t*t,uint32_t k)   /* part t's knob k (0 .. 3: A B C D) by its FORM */
{
    uint32_t tr=(uint32_t)(t-trk)%NTRK,f=func_form(t);
    param_desc_t*d=&fk_d[tr][k&3u];
    if(fk_f[tr][k&3u]!=f+1u){d->label=func_kn(f,k);fk_f[tr][k&3u]=(uint8_t)(f+1u);}
    d->fmt=F_PCT;d->max=127;d->def=(int16_t)(k<3u?64>>k:0);d->names=0;
    if(f==FUNC_BANK){                                   /* (funcbank) FORM A, FORM B by name; MORPH 0; DEF 64 */
        static const char *fb_nm[FB_N+1u] __attribute__((section(".bss.funcbank")));   /* (out of the merged statics:
                                                         * their neighbours' offsets kept short, GEN's code its size) */
        if(!fb_nm[0])for(uint32_t i=0;i<FB_N;i++)fb_nm[i]=fk_walk(FB_NAMES,i);   /* (not str_nth: the audio reaches this, the UI's -Oz keeps str_nth) */
        if(k<2u){d->fmt=F_ENUM;d->max=FB_N-1u;d->names=fb_nm;}
        d->def=(int16_t)(k==3u?64:k==1u?2:0);
    }
    return d;
}
/* its line of help: one of the knobs' by FORM (FUNC_PX's), else 0 (knob_hint's own table) */
static const char *func_knob_tip(const param_desc_t*d){
    uintptr_t o=(uintptr_t)d-(uintptr_t)fk_d,q=(uintptr_t)d-(uintptr_t)FUNC_PX;
    if(q<sizeof FUNC_PX)return str_nth(FUNC_PXT, q/sizeof(param_desc_t));
    o/=sizeof(param_desc_t);
    return o<4u*NTRK?func_tip(func_form(&trk[o/4u]),o%4u):0;
}
/* (BYTE 0.17) FUNC's EDIT k: RUN ROOT FORM its own, A B C D by FORM (WAVE 0: edit[], FUNC alone) */
static const param_desc_t *func_desc(const track_t*t,uint32_t k){return k-2u<4u?func_knob(t,k-2u):k-1u?&ENG_FUNC.edit[k&7u]:0;}
/* (loopq) LOOP while TIME is SYNC: 1 .. 8 the song's divisions by name (params.c param_format), 9 .. 127 its % */
static const char *const N_LOOPQ[] = {"OFF", "4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
static const param_desc_t LOFI_LOOPQ = {"LOOP", F_PCT, 0, 127, 0, 0};
static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if(t->p[P_E1]==RW_FUNC)                             /* (a MATH part's, until it is FUNC's: as FUNC's) */
        return func_desc(t,k);
    if (!N_FLOAT[0])                                    /* (fmgen45) the names written once */
        lofi_names();
    if (k == 4u)                                        /* (the knobs' ranges past edit[]'s, as before) */
        return &LOFI_MOTN;
    if (k == 7u && t->p[P_KEY] > 0 && t->p[P_KEY] != 2 && t->p[P_KEY] < 16)   /* (loopq: SYNC, lofi_time's) */
        return &LOFI_LOOPQ;
    if (t->p[P_E1] == RW_USER)                          /* (JIANT BYTE) the part's own formula: ALGO its seed */
        return k == 0u ? &LOFI_UCHIP : k == 2u ? &LOFI_SEED : k == 3u ? &LOFI_MUTA : 0;
    if (t->p[P_E1] != RW_FLOAT)
        return k == 2u ? &LOFI_ALGO : 0;
    return k == 0u ? &LOFI_FCHIP : k == 2u ? &LOFI_FALGO : k == 5u ? &LOFI_FDCY : 0;
}

static void lofi_block(track_t *t)                   /* (engopt) BYTE's / FLOAT's formula compiled before the note */
{
    if (t->p[P_E1] == RW_BYTE || t->p[P_E1] == RW_FLOAT) {   /* (c1fault: asked, the main loop compiles it) */
        uint32_t flt = t->p[P_E1] == RW_FLOAT;
        if (trk_quiet(t))
            (void)lofi_prog(t, 0, flt, (uint32_t)t->p[P_E2] >> 2);
    }
}
/* (func-rate) part t's RATE, B RATE, B OCT as asked: OCT at once, the RATEs at the bar while the song plays (at once
 * else); a change faded (math_fade: the formula's time moved, its sound from where it was) */
static __attribute__((noinline)) void func_act(track_t *t)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    const int16_t *q = fp_kp[tr];
    fp_t *f = &fp_st[tr];
    uint32_t w = (uint32_t)((q[FK_RATE] + 3) ^ 3) | (uint32_t)((q[FK_BRATE] + 3) ^ 3) << 3 | (uint32_t)((q[FK_BOCT] + 2) ^ 2) << 6;
    if (func_form(t) == FUNC_BANK) {                    /* (funcbank) BANK's B, as FORM B asks: (c1fault) the main
                                                         * loop compiles it (fvm_serve), the voices faded into it */
        if (!wave_prg_b[tr].ins) fvm_prog_init(&wave_prg_b[tr], wave_ins_b[tr], 32u);
        (void)fvm_want(NTRK + tr, &wave_prg_b[tr], &wave_id_b[tr], 0x8000u | func_bank_f(t, 1),
                       FVM_ABANK(2u * FVM_NBYTE + func_bank_f(t, 1)), 1u | FVM_AFADE);
    }
    if (func_bar(t) && func_clock_begin >> 18 == func_clock_end >> 18) w = (w & 0x1C0u) | (f->act & 0x3Fu);   /* (not yet) */
    if (w == f->act) return;
    f->act = (uint16_t)w;
    memset(&math_fade[tr * NVOICE], 128, NVOICE);
}
static void lofi_note_on(track_t *t, voice_t *v)
{
    uint32_t id=(uint32_t)(t-trk)*NVOICE+(uint32_t)(v-t->v);
    math_seen[id]=math_ver[(uint32_t)(t-trk)];math_fade[id]=0;math_last[id]=0;
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = 0;                 /* the time t, from 0 */
    v->ph[1] = 0u;               /* (BYTE: its slow time; FLOAT: its phase, a step at once) */
    v->s[3] = 0;                 /* (BYTE: its smoothing's first pole) */
    v->ph[2] = 0;                /* (.. its second; FLOAT: its patch's own state) */
    v->s[4] = v->s[5] = 0;       /* (JIANT 0.4) the filter's two states */
    v->s[6] = v->s[7] = 0;       /* (JIANT BYTE) the formula's own states (fvm.c ph lp hp) */
    if (t->p[P_E1] == RW_FLOAT || t->p[P_E1] == RW_FUNC)                         /* (0.5.6: its t from 0, its fraction too) */
        v->ph[0] = 0;
}

/* the set-up both formulas share (the level, the pitch, the filter), then BYTE's or FLOAT's samples */
static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], inc;
    uint32_t tr=(uint32_t)(t-trk),vi=tr*NVOICE+(uint32_t)(v-t->v),user=p[P_E1]>=RW_USER;
    int32_t *destination=out,before_last=n?out[n-1]:0;
    if(user&&(!(lofi_prg_id[tr]&0x400u)||!lofi_prg[tr].n))return;
    if(user&&math_seen[vi]!=math_ver[tr]){math_seen[vi]=math_ver[tr];math_fade[vi]=128;v->s[6]=v->s[7]=0;if(p[P_E1]==RW_FUNC){v->ph[0]=0;v->s[2]=0;}}
    if(p[P_E1]==RW_FUNC)func_act(t);                    /* (func-rate) RATE, B's, B OCT: as asked, faded */
    if(user&&math_fade[vi]){destination=math_transition[MC_CPU()&1u];memset(destination,0,n*sizeof *destination);}
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50;       /* (JIANT 0.5) its RES the FILTER page's, 50 at least */
    tsvf_t flt;
    vmod_t mg = *m;                                     /* (0.9.11) the engine's level: on the block's amplitude ramp */
    mg.amp0 = mulq15(m->amp0, LOFI_GAIN);
    mg.amp1 = mulq15(m->amp1, LOFI_GAIN);
    if (p[P_E1] == RW_FUNC) {                           /* (funcplay) the root's degree, the section's CUT */
        mg.pitch16 += fp_st[tr % NTRK].semi * 16;
        mg.cutoff += fp_st[tr % NTRK].cut * 256;
    }
    inc = pitch_inc(clamp(mg.pitch16, 0, 2047));       /* (the track's LFO in m->pitch16 already) */
    int32_t cw = (p[P_FCUT] << 8) + mg.cutoff + pt_cw;
    if (p[P_E1] == RW_FUNC) {                           /* (func-smooth) FUNC's cut (CUT, TIMBRE, a section's) glides: an
                                                         * eighth of the way a block, at the note's start as it is */
        int32_t d = cw - v->s[3];
        v->s[3] = cw = !v->s[2] || !(d >> 3) ? cw : v->s[3] + (d >> 3);
    }
    tsvf_coef(&flt, cw, rs);                            /* (JIANT 0.5: the FILTER page's CUT; punchfx: WOBBLE's) */
    if(p[P_E1]==RW_FUNC){                               /* (devcpu2: lofi_render in RAM, FUNC's and BYTE's renders in the
                                                         * flash past its direct reach: through a register) */
        void (*f)(track_t*,voice_t*,int32_t*,uint32_t,const vmod_t*,uint32_t,const tsvf_t*)=lofi_func_render;
        FM1_OPAQUE(f);f(t,v,destination,n,&mg,inc,&flt);
    }
    else if (p[P_E1] == RW_FLOAT || (p[P_E1] == RW_USER && math_ty[(uint32_t)(t - trk) % NTRK] == FT_FIX))   /* (JIANT 0.5.1)
                                                         * floatbeat (JIANT BYTE: a USER one in FIX too) */
        lofi_float_render(t, v, destination, n, &mg, inc, &flt);
    else {                                              /* (JIANT 0.4) bytebeat */
        void (*f)(track_t *, voice_t *, int32_t *, uint32_t, const vmod_t *, uint32_t, int32_t, int32_t, const tsvf_t *) =
            lofi_byte_render;
        FM1_OPAQUE(f);
        f(t, v, destination, n, &mg, inc, 1 + (chip == CHIP_4B2 ? 1 : 0),
          chip == CHIP_FULL ? 16 : chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4, &flt);
    }
    if(destination!=out){
        uint32_t remaining=math_fade[vi];int32_t held=math_last[vi],last=held;
        for(uint32_t i=0;i<n;i++){
            last=remaining?(held*(int32_t)remaining+destination[i]*(int32_t)(128u-remaining))/128:destination[i];
            out[i]+=last;if(remaining)remaining--;
        }
        math_fade[vi]=(uint8_t)remaining;
        /* Keep the same held endpoint during the entire 128-sample crossfade. */
        if(!remaining)math_last[vi]=last;
    }else if(n)math_last[vi]=out[n-1]-before_last;

}

/* (V10.6d) every sound starts still (MOTION 0: the formula's slow bits at 1/16, no evolution to startle) and with RTRG on
 * (ui.c apply_preset_to: each note the formula from its start, a voice that plays in tune); BYTE first, then FLOAT
 * (their stored numbers kept: 0..2, 8 BYTE; 9..11 FLOAT), 3..7 (where the chip presets were) FLOAT voices at the note:
 * keys, a bass, a pad, a lead, a bell */
static const preset_t LOFI_PRESETS[] = {
    /* name, {CHIP, WAVE, ALGO, VAR, MOTION, GRIT / FEEDBK, BEND, LOOP}. (V10.6i, reworked in place: the stored numbers
     * kept, LOFI_PC and GEN's roles too: 4 6 11) MOTION 0 on all (the time still); the voices to play filtered, the
     * filter opened by velocity and its envelope; the BYTE ones and FLOAT ARP the experiments, moved by the matrix (the
     * track's SHP moves VAR, CUT the FILTER page's CUT); INIT FLOAT a bare sine (LOOP 127: one cycle of F01) */
    /* BYTE: 8 bits, ALGO B02 (4 / 4 = 1); B17 (64 / 4 = 16) with VAR 40 (a = 6); B05 folded (BEND), resonant */
    {"BYTEBEAT", {2, 5, 4, 0, 0, 0, 0, 0}, {0, 80, 110, 40}, 0, 1 | TAG(FX), FX(0, 0, 25, 20), PAT(4),
     MODS(MMOD(1, PS_MODW, PD_E(3), 40), MMOD(2, PS_AT, PD_E(6), 40))},      /* MODW mutates the time, AT folds it */
    {"BYTE VAR", {2, 5, 64, 40, 0, 0, 0, 0}, {0, 80, 110, 50}, 0, 1 | TAG(FX), FX(0, 20, 30, 25), PAT(3),
     MODS(P_LWAVE, LW_SH, P_LRATE, -8, MMOD(1, PS_LFO, PD_E(3), 24), MMOD(2, PS_RAND, PD_PAN, 30))},   /* a new VAR each 1/8 */
    {"BYTE BENT", {2, 5, 16, 0, 0, 24, 70, 0}, {0, 80, 110, 45}, 0, 1 | TAG(FX), FX(10, 10, 30, 25), PAT(4),   /* (0.5.4: GRIT 24) */
     MODS(MENV2(0, 50, 0, 30), MMOD(1, PS_ENV2, PD_E(6), -40), MMOD(2, PS_RAND, PD_REV, 30))},   /* unfolds as it sounds */
    /* FLOAT at the note: F28 a partial struck; F20 odd partials (a hollow square); F12 a deep vibrato (slow, MOTION 0);
     * F24 FM 5:2; F11 a pluck of FM */
    {"MATH KEYS", {2, 6, 108, 0, 0, 0, 0, 0}, {0, 75, 40, 45}, 20, TAG(KEYS), FX(0, 20, 30, 35), PAT(6),
     MODS(P_FCUT, 84, MMOD(1, PS_VEL, PD_CUT, 20))},
    {"MATH BASS", {2, 6, 76, 0, 0, 0, 0, 0}, {0, 60, 90, 22}, 24, 1 | TAG(BASS), FX(0, 0, 5, 0), PAT(2),
     MODS(P_FCUT, 70, P_GLIDE, 10)},
    {"MATH PAD", {2, 6, 44, 0, 0, 0, 0, 0}, {75, 90, 78, 95}, 0, TAG(PAD), FX(0, 50, 30, 60), PAT(5),
     MODS(P_FCUT, 96, P_LRATE, 20, P_LD_FLT, 10)},
    {"MATH LEAD", {2, 6, 92, 0, 0, 0, 0, 0}, {4, 70, 100, 42}, 0, 1 | TAG(LEAD), FX(0, 0, 35, 25), PAT(4),
     MODS(P_LRATE, 88, P_LFADE, 72, MMOD(1, PS_MODW, PD_VIB, 2), P_GLIDE, 10)},
    {"MATH BELL", {2, 6, 40, 0, 0, 0, 0, 0}, {0, 88, 0, 70}, 12, TAG(BELL), FX(0, 20, 35, 55), PAT(7),
     MODS(P_FCUT, 100, MMOD(1, PS_VEL, PD_CUT, 22))},
    /* (JIANT 0.5, appended) BYTE B11 looped (LOOP 80: a short cycle, a tone at the note), resonant; its LOOP leaps on
     * the 1/8 (V10.6i), MODW the VAR */
    {"BYTE TONE", {2, 5, 40, 0, 0, 0, 0, 80}, {0, 70, 100, 40}, 0, 1 | TAG(LEAD), FX(0, 0, 30, 20), PAT(4),
     MODS(P_LWAVE, LW_SQR, P_LRATE, -8, MMOD(1, PS_LFO, PD_E(7), 6), MMOD(2, PS_MODW, PD_E(3), 30))},
    /* (V10.6i) a bare sine (was FLOAT FM) */
    {"INIT FLOAT", {2, 6, 0, 0, 0, 0, 0, 127}, {0, 64, 80, 20}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
    /* (JIANT 0.5.1, appended; 0.5.6 floatbeat) FLOAT: F03 a nibble-table melody (S&H 1/16 on its filter, each note its
     * place); F29 FM breathing at 1 Hz, slow (V10.6i: the filter drifting) */
    {"FLOAT ARP", {2, 6, 8, 0, 0, 0, 0, 0}, {0, 50, 80, 50}, 0, 1 | TAG(PLUCK), FX(0, 25, 40, 30), PAT(3),
     MODS(P_FCUT, 100, P_LWAVE, LW_SH, P_LRATE, -10, P_LD_FLT, 16, MMOD(1, PS_RAND, PD_PAN, 40))},
    {"FLOAT PAD", {2, 6, 112, 0, 0, 0, 0, 0}, {80, 90, 69, 100}, 0, 1 | TAG(PAD), FX(30, 10, 50, 40), PAT(3), BREATHE(14, 0, 0),
     MODS(P_FCUT, 90)},
};
/* (V10.6i) browsed: the voices to play, the experiments, INIT (ui.c: in LOFI_ORDER's place; LOFI_PC stays) */
static const uint8_t MATH_ORDER[] = {3, 4, 5, 6, 7, 11,   /* to play */
                                     0, 1, 2, 8, 10,      /* experiments */
                                     9};                  /* INIT (BYTE 0.17: FUNC SONG and BEAT, 12 13, FUNC's own) */
_Static_assert(sizeof MATH_ORDER == NELEM(LOFI_PRESETS), "MATH's browsing order: every preset once");

static const engine_t ENG_LOFI = {
    .name = "MATH",
    .page_title = {"FORMULA", "MOTION"},
    .edit = {
        {"CHIP", F_ENUM, 0, 3, 2, N_CHIP},           /* (FLOAT: 8BIT reads FULL, lofi_desc) */
        {"WAVE", F_ENUM, RW_BYTE, RW_USER, RW_BYTE, N_RWAVE},   /* (JIANT BYTE: USER, the part's own; BYTE 0.17: FUNC an
                                                         * engine of its own, ENG_FUNC) */
        {"ALGO", F_INT, 0, 127, 64, 0},              /* (the knob 0..191 and the names: lofi_desc) */
        {"VAR", F_PCT, 0, 127, 0, 0},
        {"MOTION", F_INT, 0, 127, 0, 0},             /* (V10.6d: 0 the default; the knob to 255 SYNC: lofi_desc) */
        {"GRIT", F_PCT, 0, 127, 0, 0},               /* (FLOAT: FEEDBK) */
        {"BEND", F_PCT, 0, 127, 0, 0},
        {"LOOP", F_PCT, 0, 127, 0, 0},
    },
    .presets = LOFI_PRESETS,
    .npresets = NELEM(LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .block = lofi_block,
    .half = 1,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x31,                /* the held sample and the filter */
    .desc = lofi_desc,
};

/* (BYTE 0.17, the owner: "FUNC ended up very hidden; it would be good if it had its own space") FUNC: MATH's FUNC as an
 * engine. Underneath it is MATH's: the same renderer (lofi_render, lofi_func_render), program slot (lofi_slot, published
 * by math_poll), voice state and values, WAVE always FUNC (P_E1: what every FUNC path tests); only its pages, names and
 * presets are its own. Its pages (params.c PAGES, ui.c page_visible): PLAY FORM RUN ROOT CUT, KNOBS A B C D by FORM.
 * Stored as MATH WAVE FUNC (project.c), so a project of it loads on a firmware without it, and one of before as FUNC.
 * Its presets: a FORM each (preset k is FORM k + 1), RUN PLAY (the transport's clock; stopped, a key sounds it) */
#define FU(f, a, b, c, d) {5, RW_FUNC, a, b, c, d, 0, f}
static const preset_t FUNC_PRESETS[] = {
    {"SONG", FU(1, 80, 0, 64, 64), {0, 0, 127, 24}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
    {"BEAT", FU(2, 64, 0, 64, 64), {0, 0, 127, 24}, 0, TAG(BASS), FX(0, 0, 0, 0)},
    {"GROWL", FU(3, 50, 40, 14, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 0, 0), MODS(P_FCUT, 96)},
    {"HALFTIME", FU(4, 80, 40, 24, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 0, 0)},
    {"ACID", FU(5, 70, 40, 60, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 20, 0), MODS(P_FCUT, 104)},
    {"REESE", FU(6, 50, 60, 20, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 0, 0)},
    {"HOUSE", FU(7, 40, 30, 90, 0), {0, 0, 127, 24}, 0, TAG(KEYS), FX(0, 0, 20, 20)},
    {"STRUM", FU(8, 60, 0, 20, 0), {0, 0, 127, 24}, 0, TAG(PLUCK), FX(0, 0, 25, 30)},
    {"JUST", FU(9, 50, 40, 20, 0), {0, 0, 127, 24}, 0, TAG(BELL), FX(0, 0, 25, 30)},
    {"PLUCK", FU(10, 64, 40, 20, 0), {0, 0, 127, 24}, 0, TAG(PLUCK), FX(0, 0, 25, 25)},
    {"PSY", FU(11, 40, 50, 30, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 20, 10)},
    {"DSTEP", FU(12, 50, 26, 60, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 0, 10), MODS(P_FCUT, 92)},
    {"DRUMS", FU(13, 64, 40, 30, 0), {0, 0, 127, 24}, 0, TAG(DRUM), FX(0, 0, 0, 10)},
    {"MELODY", FU(14, 50, 30, 30, 0), {0, 0, 127, 60}, 0, TAG(LEAD), FX(0, 20, 30, 40)},
    {"DUB", FU(15, 30, 127, 80, 0), {0, 0, 127, 24}, 0, TAG(KEYS), FX(0, 0, 40, 40)},
    {"AMEN", FU(16, 70, 30, 60, 0), {0, 0, 127, 24}, 0, TAG(DRUM), FX(0, 0, 0, 15)},
    {"TRANCE", FU(17, 60, 64, 70, 0), {0, 0, 127, 24}, 0, TAG(PLUCK), FX(0, 30, 45, 40)},
    {"AMBIENT", FU(18, 34, 40, 28, 0), {0, 0, 127, 60}, 0, TAG(PAD), FX(0, 50, 30, 70), MODS(P_FCUT, 100)},
    {"MINIMAL", FU(19, 70, 80, 40, 0), {0, 0, 127, 24}, 0, TAG(DRUM), FX(0, 0, 30, 10)},
    {"JUNGLE", FU(20, 40, 36, 30, 0), {0, 0, 127, 24}, 1, TAG(BASS), FX(0, 0, 0, 10), MODS(P_FCUT, 100)},
    {"BANK", FU(21, 0, 2, 0, 64), {0, 0, 127, 24}, 0, TAG(PAD), FX(0, 0, 20, 30)},   /* (funcbank) PAD to SUPERSAW */
};
#undef FU
_Static_assert(NELEM(FUNC_PRESETS) == FUNC_NFORM - 1u, "FUNC: a preset for every FORM");
static const engine_t ENG_FUNC = {
    .name = "FUNC",
    .page_title = {"RECIPE", "KNOBS"},   /* (RECIPE: the SOUND group's; PLAY is the voices' tab) */
    .edit = {
        {"RUN", F_ENUM, 4, 6, 5, N_FUNC_RUN},
        {"WAVE", F_ENUM, RW_FUNC, RW_FUNC, RW_FUNC, N_RWAVE},   /* (fixed: MATH's FUNC paths read it) */
        {"A", F_PCT, 0, 127, 64, 0}, {"B", F_PCT, 0, 127, 32, 0}, {"C", F_PCT, 0, 127, 16, 0}, {"D", F_PCT, 0, 127, 0, 0},
        {"ROOT", F_INT, -24, 24, 0, 0},
        {"FORM", F_ENUM, 0, FUNC_NFORM - 1u, 1, N_FUNC_FORM},
    },
    .presets = FUNC_PRESETS,
    .npresets = NELEM(FUNC_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .half = 1,
    .knob = {P_E2, P_E3, P_E4, P_E5},
    .keep = 0x31,
    .desc = func_desc,
};
#define ENG_IS_MATH(e) ((e) == &ENG_LOFI || (e) == &ENG_FUNC)   /* (BYTE 0.17) MATH's renderer: MATH or FUNC */
