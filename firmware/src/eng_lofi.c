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
static const char *const N_BYTE[] = {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11", "B12", "B13", "B14", "B15", "B16", "B17", "B18", "B19", "B20", "B21", "B22", "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32", "B33", "B34", "B35", "B36", "B37", "B38", "B39", "B40", "B41", "B42", "B43", "B44", "B45", "B46", "B47", "B48", 0};   /* (p6: 48) */
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
static const char *const N_FLOAT[] = {"F01", "F02", "F03", "F04", "F05", "F06", "F07", "F08", "F09", "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31", "F32", "F33", "F34", "F35", "F36", "F37", "F38", "F39", "F40", "F41", "F42", "F43", "F44", "F45", "F46", "F47", "F48", 0};   /* (p6: 48) */
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 191, 64, N_BYTE};    /* (p6: 48; the knob's: edit[2] keeps
                                                         * 0..127, FUN7's byte) */
/* (JIANT BYTE) MOTION: the formula's slow time (its rhythm, its phrase) as a note value of the song's tempo: LIBRE the
 * slow time at its own pace (MATH's MOTN 0, as before), then 1/32 .. 4 bars, straight, dotted (.) and triplets (T).
 * MOT_24: the value in 1/24 of a beat */
static const char *const N_MOTION[] = {"LIBRE", "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4",
                                       "1/4.", "1/2T", "1/2", "1/2.", "1", "2", "4", 0};
static const uint16_t MOT_24[17] = {24, 3, 4, 6, 9, 8, 12, 18, 16, 24, 36, 32, 48, 72, 96, 192, 384};
static const param_desc_t LOFI_MOTN = {"MOTION", F_ENUM, 0, 16, 0, N_MOTION};
static const param_desc_t LOFI_SEED = {"SEED", F_INT, 0, 191, 64, 0};
static const param_desc_t LOFI_MUTA = {"MUTA", F_INT, 0, 127, 0, 0};
/* (JIANT BYTE) USER's CHIP: up to FULL (a bytebeat's 16 bits between its steps; a floatbeat as it is) */
static const param_desc_t LOFI_UCHIP = {"CHIP", F_ENUM, 0, 4, 4, N_CHIP};   /* (phase 9) USER: VAR 1..63 mutates, 64.. crosses */   /* (JIANT BYTE) USER: ALGO the generator's seed */   /* (p6: 128 .. 255 SYNC; V10.6d: 0) */
static const param_desc_t LOFI_FALGO = {"ALGO", F_INT, 0, 191, 64, N_FLOAT};   /* = edit[2] (FLOAT) */
/* .. its CHIP the resolution: 4 bits, 4 bits held over two samples, FULL (smooth, the default), 1 bit, STEP (4 bits and
 * the stepped envelope) */
static const char *const N_FCHIP[] = {"4BIT", "4B/2", "FULL", "1BIT"};
static const param_desc_t LOFI_FCHIP = {"CHIP", F_ENUM, 0, 3, 2, N_FCHIP};    /* = edit[0] (FLOAT; 0.5.5: no STEP) */
/* (0.5.4) BYTE's and FLOAT's own second page: MOTN (E4, where CUT was: the time's speed, 64 as written), GRIT (BYTE,
 * E5: the samples held longer, a rate crusher), FBK (FLOAT, E5: the output back into the formula's time) */
static const param_desc_t LOFI_FDCY = {"FEEDBK", F_PCT, 0, 127, 0, 0};            /* = edit[5] (FLOAT, 0.6.4: was DRV, a
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
typedef struct { uint32_t mul, sy, sb, sr, inc, key, qb, qr; } lofi_tm_t;
static void lofi_time(const int16_t *p, lofi_tm_t *m)
{
    int32_t kv = p[P_KEY];
    uint32_t mo = (uint32_t)clamp(p[P_E4], 0, 16), tm = kv <= 0 ? 0u : kv == 2 || kv >= 16 ? 2u : 1u, r = 0, b;
    memset(m, 0, sizeof *m);
    m->mul = lofi_motion(0);
    if (!mo && !tm)
        return;                                         /* (LIBRE, UNSYNC: MATH as ever) */
    b = lofi_sync(&r);                                  /* (the song's: 8192 a beat) */
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
 * (fvm_bank.c), each part's compiled when its WAVE or ALGO changes (lofi_prog, in the audio: a few hundred microseconds
 * once) and run a block at a time. lofi_byte / lofi_float above stay as the reference (tests/fvm_test.c). The
 * machine's work space and the compiler's share one area a core (they never run at once): fvm_cc_t's ~2 KB. A formula
 * keeps at most LOFI_NSTATE states (ph lp hp) in the voice (v->s[6], v->s[7]); one with more is refused (silent) */
#define LOFI_NSTATE 2u
typedef union { fvm_scratch_t s; fvm_cc_t c; } lofi_work_t;
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
/* the formula over m samples (the variables' arrays m long), FVM_BLK at a time */
static void lofi_run(const fvm_prog_t *pg, fvm_env_t *en, lofi_work_t *wk, uint32_t m, uint32_t *out)
{
    uint32_t o, k;
    const uint32_t *a0[FA_N];
    for (k = 0; k < FA_N; k++)
        a0[k] = en->arr[k];
    for (o = 0; o < m; o += FVM_BLK) {
        for (k = 0; k < FA_N; k++)
            en->arr[k] = a0[k] ? a0[k] + o : 0;
        formula_run(pg, en, &wk->s, m - o < FVM_BLK ? m - o : FVM_BLK, out + o);
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
    if(user)return lofi_slot(tr); /* USER programs are validated/published by math_poll, never compiled here. */
    if (lofi_prg_id[tr] != id) {
        const char *s = user ? math_txt[tr] : flt ? FVM_FLOAT_BANK[f % FVM_NFLOAT] : FVM_BYTE_BANK[f % FVM_NBYTE];
        uint32_t n = 0;
        while (s[n]) n++;
        if (fvm_compile(&w->c, lofi_slot(tr), s, n, 0) || lofi_prg[tr].nstate > LOFI_NSTATE)
            lofi_prg[tr].n = 0;                      /* (silent) */
        lofi_prg_id[tr] = (uint16_t)id;
    }
    return &lofi_prg[tr];
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
    hold *= 1 + (p[P_E5] >> 3);                         /* (0.5.4) GRIT: each sample held up to 16 x longer */
    int32_t vsh = (int32_t)VH_SH(), sk = vsh ? 3 : 2;   /* (as JIANT FM V10.7.56) at half the rate: the hold half as
                                                         * many samples, the two smoothing poles squared */
    hold = (hold + vsh) >> vsh;
    if (n > LOFI_N)
        n = LOFI_N;
    {   /* (JIANT BYTE) pass 1: MATH's time as ever, the times the formula is read at (a new held sample) gathered */
        uint32_t ctt = tt, ctf = tf, cts = ts;
        int32_t cc = cnt;
        for (i = 0; i < n; i++) {
            if (--cc <= 0) {
                if (sy)                                  /* (SYNC: the slow bits at the bar's position) */
                    cts = (sb + ((i * sr) >> 16)) * mul;
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
    v->ph[1] = ts;
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
    const fvm_prog_t *pg = lofi_prog(t, wk, 1, f);
    uint32_t T[LOFI_N], W[LOFI_N], vals[LOFI_N];
    uint32_t caos = (uint32_t)clamp(p[P_CAOS], 0, 127), qr, qb, key, qm = 0;
    int32_t fb0 = fb;
    fvm_env_t en = {{T, W, 0, 0, 0}, {0}, &v->s[6]};
    lofi_time(p, &tmc);                                 /* (JIANT BYTE) TIME and MOTION */
    mul = tmc.mul; sy = tmc.sy; sb = tmc.sb; sr = tmc.sr; tinc = tmc.inc; key = tmc.key; qb = tmc.qb; qr = tmc.qr;
    xm_env(&en, pg);
    en.u[FU_NOTE] = v->note;
    en.u[FU_VEL] = v->vel;
    if (n > LOFI_N)
        n = LOFI_N;
    if (!fbk) {                                          /* (JIANT BYTE) the block's times, then the formula at once */
        uint32_t ctt = tt, ctf = tf, cts = ts;
        for (i = 0; i < n; i++) {
            uint32_t te, tl, tw;
            if (sy)
                cts = (sb + ((i * sr) >> 16)) * mul;
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
            formula_run(pg, &e1, &wk->s, 1, &o1);
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
    v->ph[1] = ts;
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
static const char MATH_FUNC_R_ACID[] = "func{let k=int(P*4);let e=1-P*4%1;let y=w*(\"44464348\"[k*5+rnd(k>>4^d)&7]-48)*.25;let f=y%1;return sat((saw(y)*.4+sin(f*(2+e*e*16*A))*(1-f)*(.5+B))*(1+C*5)*(.3+eu(k,d)*.7)*e);}";
static const char MATH_FUNC_R_REESE[] = "func{let k=int(P*2);let y=w*(\"4434\"[k+rnd(k>>3^d)&3]-48)*.25;let z=saw(y)+saw(y*(1.003+A*.01))+saw(y*.5)*.6;return sat(tri(z*(.25+B*.5))*(1+C*4))*(.6+eu(k,d)*.4);}";
static const char MATH_FUNC_R_HOUSE[] = "func{let g=P%1;let k=int(P*4);let r=\"0573\"[(k>>3)+rnd(k>>5^d)&3]-48;return sat(chord(n+r+12,P%2,A)*g*.7+sin(w*.25+sin(w*.5)*B*.3)*eu(k,d)*.6*(1-g*.5)+kick(g)*C);}";
static const char MATH_FUNC_R_STRUM[] = "func{let k=int(P*4);let g=((k%8>5?k%8-6:k%8%3)+P*4%1)/4;let r=n+\"0375\"[(k>>4)+rnd(k>>6^d)&3]-48;return sat((note(r,g,A)+note(r+3+int(B*2),g-.06,A)+note(r+7,g-.12,A))*(1+C*3)*.5);}";
static const char MATH_FUNC_R_JUST[] = "func{let k=int(P);let y=w*(\"44564454\"[k+rnd(k>>3^d)&7]-48)*.25;let f=y%1;let e=1-P%1;return sat(sin(f*f*(2+A*12)+sin(y*2)*B*.3)*e*(.4+eu(k*4,d)*.6)*(1+C*3)+sin(w*(1+e*e*e*8))*e*e*e*.5);}";
static const char MATH_FUNC_R_PLUCK[] = "func{let k=int(P*4);let y=w*(\"44464348\"[k*5+rnd(k>>4^d)&7]-48)*.25;let u=y%1*2-1;let z=u*u*u;let e=1-P*4%1;return sat(sin(z*z*z*(1+A*3)+y+sin(y*2)*B)*e*e*(.3+eu(k,d)*.7)*(1+C*3));}";
static const char MATH_FUNC_R_NIBBLE[] = "func{let k=int(P*4);let y=w*(\"4568<068\"[(k^k>>2)+rnd(k>>5^d)&7]-48)*.25;let h=.5+sin(P*.125)*A*.45;return sat(((y%1<h)*2-1)*(y>0)*(.3+eu(k,d)*.7)*(1-P*4%1)*(1+C*2)+sin(w*.5)*B*.5);}";
static const char MATH_FUNC_R_BITS[] = "func{let k=int(P*4);let m=int(t/(4+int(A*4)));let y=fix((m*(3+(k>>2^d)%5)&(m>>5|m>>8))%256)/128-1;return sat(y*(.3+eu(k,d)*.7)*(1+C*3)+kick(P%1)*B+hat(P*4%1)*.3);}";
/* (BYTE 0.16, the owner: "more extreme timbres in the palette") DRUMS: drums from the formula, no 808: a kick by a
 * folded falling sine (C its fold), a noise snare ringing (B its pitch) and square hats (A), each lane its own eu()
 * fused from the seed. SCREECH: cross-FM whose index falls each step (A), the ratio (B), folded (C) and crushed to
 * 3..15 levels (D), its action eu() */
static const char MATH_FUNC_R_BEATS[] = "func{let k=int(P*4);let g=P*4%1;let q=(1-g)*(1-g);let z=rnd(T);return sat(tri(sin(g*(2+q*9))*(1+C*7))*q*eu(k,d)*1.5+z*q*(sin(g*(20+B*80))*eu(k+2,d+5)*.5+sqr(w*7)*q*eu(k*3,d+9)*A*.4));}";
static const char MATH_FUNC_R_SCREECH[] = "func{let k=int(P*4);let g=P*4%1;let i=(1-g)*(2+A*14);let y=tri(sin(w*(1+int(B*6))+sin(w*3.01)*i)*(1+C*8));let m=3+D*12;return floor(y*m)/m*(.3+eu(k,d)*.7);}";
static const char *const MATH_FUNC_R[] = {MATH_FUNC_R_ACID,MATH_FUNC_R_REESE,MATH_FUNC_R_HOUSE,MATH_FUNC_R_STRUM,MATH_FUNC_R_JUST,MATH_FUNC_R_PLUCK,MATH_FUNC_R_NIBBLE,MATH_FUNC_R_BITS,MATH_FUNC_R_BEATS,MATH_FUNC_R_SCREECH};
static uint32_t func_clock_begin,func_clock_end;
static uint8_t func_song_mask;
/* (BYTE 0.16, the owner: "that PLAY starts the formula: set it explicitly; by default a key sounds it") RUN: NOTE (the
 * key pressed sounds it, at its pitch; the default), PLAY (the transport's: its voice and clock, as SONG was), FREE (it
 * runs on its own: a voice always, its own clock, at the root) */
static int func_song(const track_t*t){return t->eng_req==ENGI_LOFI&&t->p[P_E1]==RW_FUNC&&t->p[P_E0]==5;}
static int func_free(const track_t*t){return t->eng_req==ENGI_LOFI&&t->p[P_E1]==RW_FUNC&&t->p[P_E0]==6;}
/* (BYTE 0.16, the owner: "the whole formula is never heard") the formula's clock: the transport's in PLAY and NOTE
 * while the song plays (the key its pitch and gate, the formula where the song is: a 1/16 step a sixteenth of it, in
 * place); stopped, NOTE from the key, FREE its own */
#define func_bar(t) (song.playing&&!func_free(t))
static int math_is_func(const char*s){while(*s&&(uint8_t)*s<=32u)s++;return s[0]=='f'&&s[1]=='u'&&s[2]=='n'&&s[3]=='c';}
static const char *math_func_text(const track_t*t,const char*custom){return t->p[P_E7]==1?MATH_FUNC_DEFAULT:t->p[P_E7]==2?MATH_FUNC_BEAT:t->p[P_E7]==3?MATH_FUNC_GROWL:t->p[P_E7]==4?MATH_FUNC_HALF:t->p[P_E7]>=5&&t->p[P_E7]<5+(int)NELEM(MATH_FUNC_R)?MATH_FUNC_R[t->p[P_E7]-5]:math_is_func(custom)?custom:MATH_FUNC_DEFAULT;}
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
static __attribute__((noinline)) void lofi_func_render(track_t*t,voice_t*v,int32_t*out,uint32_t n,const vmod_t*m,uint32_t inc,const tsvf_t*flt)
{
    uint32_t T[LOFI_N],W[LOFI_N],S[LOFI_N],P[LOFI_N],Y[LOFI_N],q=(uint32_t)v->s[2],ph=v->ph[0],rate;
    const fvm_prog_t*p=lofi_slot((uint32_t)(t-trk));
    fvm_env_t en={{T,W,S,0,0},{0},&v->s[6]};xm_env(&en,p);
    for(uint32_t k=0;k<4;k++){uint32_t a=(uint32_t)clamp(t->p[P_E2+k],0,127);en.u[FU_A+k]=a*2u;en.u[FU_AF+k]=a*65536u/127u;}
    en.u[FU_NOTE]=(uint32_t)clamp((int32_t)v->note+(t->pfx_pit+pt_vp[(uint32_t)(t-trk)%NTRK]-pt_wob)/16,0,127);   /* (BYTE
                                                         * punchfx) OCT- / OCT+ and CAOS MAX's throw on n too: its notes
                                                         * are n's, not the voice's pitch (WOBBLE: in w, and P swayed) */
    en.u[FU_VEL]=v->vel;en.u[FU_HZ]=(uint32_t)(((uint64_t)inc*FS)>>16);
    en.u[FU_BEAT]=lofi_sync(&rate)*8u;en.u[FU_STEP]=t->seq_idx;
    en.u[FU_BPS]=(uint32_t)FS*60u/(uint32_t)clamp(song.g[G_BPM],20,300);en.arr[FA_BEAT]=P;
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
    lofi_run(p,&en,&lofi_work[MC_CPU()&1u],n,Y);
    int32_t y1=v->s[4],y2=v->s[5],kd=8192-(t->p[P_FRES]>50?t->p[P_FRES]:50)*7600/127;
    for(uint32_t i=0;i<n;i++){int32_t sample=fvm_sample(FT_FIX,Y[i]);out[i]+=voice_amp(soft_knee(svf_mode(flt,sample>>1,&y1,&y2,ft_voice(t->p),kd),16000)<<1,m,i);}
    v->s[2]=(int32_t)(q+(n<<sh));v->ph[0]=ph;v->s[4]=y1;v->s[5]=y2;
}

/* (func-tame) FUNC's A B C D by FORM: their names on the cards, the matrix, the editor (all through lofi_desc), and a
 * line each in the header while one turns (ui_input.c knob_hint, MENU > DISPLAY > HELP). CUSTOM (and a LOOP past the
 * recipes', as math_func_text) the program's own A B C D. Flash only; the stored values the same (0 .. 127) */
#define FK(a,b,c,d) {{a,F_PCT,0,127,64,0},{b,F_PCT,0,127,32,0},{c,F_PCT,0,127,16,0},{d,F_PCT,0,127,0,0}}
static const param_desc_t FUNC_KNOBS[15][4]={FK("A","B","C","D"),FK("COLOR","SNARE","HATS","STEPS"),
    FK("COLOR","SNARE","HATS","KICK"),FK("VOWEL","WOBBLE","DRIVE","SEED"),FK("DEPTH","RING","DRIVE","SEED"),
    FK("CUTOFF","RESO","DRIVE","SEED"),FK("DETUNE","TONE","DRIVE","SEED"),FK("CHORD","BASS","KICK","SEED"),
    FK("COLOR","CHORD","DRIVE","SEED"),FK("BRIGHT","FM","DRIVE","SEED"),FK("SHAPE","TONE","DRIVE","SEED"),
    FK("PWM","SUB","DRIVE","SEED"),FK("SLOW","KICK","DRIVE","SEED"),FK("HATS","SNARE","KICK","SEED"),
    FK("INDEX","RATIO","FOLD","CRUSH")};
#undef FK
#define FUNC_SEED "PICKS NOTES + RHYTHM"
static const char *const FUNC_TIPS[15][4]={
    {"CONTROL A, 0 TO 1","CONTROL B, 0 TO 1","CONTROL C, 0 TO 1","CONTROL D, 0 TO 1"},
    {"CHORD + MELODY TONE","EVERY 2 BEATS OR 1","2, 4 OR 8 A BEAT","1, 2 OR 4 A BEAT"},
    {"BASS HARMONICS","EVERY 2 BEATS OR 1","2, 4 OR 8 A BEAT","3-3-2, 4 OR 2 A BAR"},
    {"FORMANT OPENING","2, 4, 6, 8 A BAR","SATURATION",FUNC_SEED},
    {"WOBBLE DEPTH","RING MOD PITCH","SATURATION",FUNC_SEED},
    {"ACID FILTER OPEN","RESONANCE SQUELCH","SATURATION",FUNC_SEED},
    {"SAW SPREAD","FOLD BRIGHTNESS","SATURATION",FUNC_SEED},
    {"MINOR, MAJOR, 7TH","BASS FM GROWL","KICK LEVEL",FUNC_SEED},
    {"NOTE HARMONICS","MINOR, MAJOR, SUS4","SATURATION",FUNC_SEED},
    {"PHASE BEND","FM AMOUNT","SATURATION",FUNC_SEED},
    {"PLUCK SHARPNESS","FM BODY","SATURATION",FUNC_SEED},
    {"PULSE WIDTH SWEEP","SUB LEVEL","SATURATION",FUNC_SEED},
    {"BYTEBEAT SLOWER","KICK LEVEL","SATURATION",FUNC_SEED},
    {"HATS LEVEL","SNARE PITCH","PUNCH, FOLDED",FUNC_SEED},
    {"FM SCREAM","MODULATOR 1 TO 7","WAVEFOLD GRIT","3 TO 15 LEVELS"}};
#undef FUNC_SEED
static uint32_t func_form(const track_t*t){return t->p[P_E7]>=1&&t->p[P_E7]<5+(int)NELEM(MATH_FUNC_R)?(uint32_t)t->p[P_E7]:0u;}
_Static_assert(NELEM(FUNC_KNOBS)==5+NELEM(MATH_FUNC_R)&&NELEM(FUNC_TIPS)==NELEM(FUNC_KNOBS),"FUNC: a name for every FORM's knobs");
/* its line of help: one of FUNC_KNOBS's, else 0 (knob_hint's own table) */
static const char *func_knob_tip(const param_desc_t*d){
    uintptr_t o=(uintptr_t)d-(uintptr_t)&FUNC_KNOBS[0][0];
    return o<sizeof FUNC_KNOBS?FUNC_TIPS[o/sizeof FUNC_KNOBS[0]][o%sizeof FUNC_KNOBS[0]/sizeof(param_desc_t)]:0;
}
static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if(t->p[P_E1]==RW_FUNC){
        static const char*const runs[]={"-","-","-","-","TRIG","PLAY","FREE"},*const forms[]={"CUSTOM","SONG","BEAT","GROWL","HALFTIME","ACID","REESE","HOUSE","STRUM","JUST","PLUCK","NIBBLE","BITS","DRUMS","SCREECH"};
        static const param_desc_t run={"RUN",F_ENUM,4,6,5,runs},form={"FORM",F_ENUM,0,14,1,forms},root={"ROOT",F_INT,-24,24,0,0};
        return k==0u?&run:k>=2u&&k<=5u?&FUNC_KNOBS[func_form(t)][k-2u]:k==6u?&root:k==7u?&form:0;
    }
    if (k == 4u)                                        /* (the knobs' ranges past edit[]'s, as before) */
        return &LOFI_MOTN;
    if (t->p[P_E1] == RW_USER)                          /* (JIANT BYTE) the part's own formula: ALGO its seed */
        return k == 0u ? &LOFI_UCHIP : k == 2u ? &LOFI_SEED : k == 3u ? &LOFI_MUTA : 0;
    if (t->p[P_E1] != RW_FLOAT)
        return k == 2u ? &LOFI_ALGO : 0;
    return k == 0u ? &LOFI_FCHIP : k == 2u ? &LOFI_FALGO : k == 5u ? &LOFI_FDCY : 0;
}

static void lofi_block(track_t *t)                   /* (engopt) BYTE's / FLOAT's formula compiled before the note */
{
    if (fvm_warm_tok && (t->p[P_E1] == RW_BYTE || t->p[P_E1] == RW_FLOAT)) {
        uint32_t tr = (uint32_t)(t - trk) % NTRK, flt = t->p[P_E1] == RW_FLOAT, f = (uint32_t)t->p[P_E2] >> 2;
        if (lofi_prg_id[tr] != 1u + (flt ? 64u : 0u) + f && trk_quiet(t)) {
            fvm_warm_tok = 0;
            (void)lofi_prog(t, &lofi_work[MC_CPU() & 1u], flt, f);
        }
    }
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
    if(user&&math_fade[vi]){destination=math_transition[MC_CPU()&1u];memset(destination,0,n*sizeof *destination);}
    int32_t rs = p[P_FRES] > 50 ? p[P_FRES] : 50;       /* (JIANT 0.5) its RES the FILTER page's, 50 at least */
    tsvf_t flt;
    vmod_t mg = *m;                                     /* (0.9.11) the engine's level: on the block's amplitude ramp */
    mg.amp0 = mulq15(m->amp0, LOFI_GAIN);
    mg.amp1 = mulq15(m->amp1, LOFI_GAIN);
    inc = pitch_inc(clamp(mg.pitch16, 0, 2047));       /* (the track's LFO in m->pitch16 already) */
    tsvf_coef(&flt, (p[P_FCUT] << 8) + mg.cutoff + pt_cw, rs);   /* (JIANT 0.5: the FILTER page's CUT; punchfx: WOBBLE's) */
    if(p[P_E1]==RW_FUNC)lofi_func_render(t,v,destination,n,&mg,inc,&flt);
    else if (p[P_E1] == RW_FLOAT || (p[P_E1] == RW_USER && math_ty[(uint32_t)(t - trk) % NTRK] == FT_FIX))   /* (JIANT 0.5.1)
                                                         * floatbeat (JIANT BYTE: a USER one in FIX too) */
        lofi_float_render(t, v, destination, n, &mg, inc, &flt);
    else                                                /* (JIANT 0.4) bytebeat */
        lofi_byte_render(t, v, destination, n, &mg, inc, 1 + (chip == CHIP_4B2 ? 1 : 0),
                         chip == CHIP_FULL ? 16 : chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4, &flt);
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
    {"FUNC SONG", {5,8,80,0,64,64,0,1}, {0,0,127,24}, 0, TAG(KEYS), FX(0,0,0,0)},
    {"FUNC BEAT", {5,8,64,0,64,64,0,2}, {0,0,127,24}, 0, TAG(BASS), FX(0,0,0,0)},
};
/* (V10.6i) browsed: the voices to play, the experiments, INIT (ui.c: in LOFI_ORDER's place; LOFI_PC stays) */
static const uint8_t MATH_ORDER[] = {3, 4, 5, 6, 7, 11,   /* to play */
                                     0, 1, 2, 8, 10,      /* experiments */
                                     9,12,13};                  /* INIT / FUNC */
_Static_assert(sizeof MATH_ORDER == NELEM(LOFI_PRESETS), "MATH's browsing order: every preset once");

static const engine_t ENG_LOFI = {
    .name = "MATH",
    .page_title = {"FORMULA", "MOTION"},
    .edit = {
        {"CHIP", F_ENUM, 0, 3, 2, N_CHIP},           /* (FLOAT: 8BIT reads FULL, lofi_desc) */
        {"WAVE", F_ENUM, RW_BYTE, RW_FUNC, RW_BYTE, N_RWAVE},   /* (JIANT BYTE: USER, the part's own) */
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
