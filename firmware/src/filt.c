/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE) the filter TYPEs every engine shares (voice.c), kept from the retired ANALOG (eng_analog.c) */
/* (JIANT 0.5) FILTER TYPE (P_FTYPE): LP as before (analog_render's loop); HP BP from the same state-variable filter
 * (analog_ext's loop, every WAVE); COMB the filter open, the part's sum through a feedback comb tuned to the newest
 * note and CUT (+-32 semitones about it), RES its feedback (voice.c track_comb).
 * (0.9.9-snd1) COMB- the same comb with its feedback negative: odd harmonics, hollow (its line half as long: the note
 * where COMB's is). FT_POST(ft): a TYPE the part's (voice.c tf_run), every engine's own filter open under it.
 * (codex/filters2) MS20 retired (the owner: "it barely resembles the original", LP24 alike): its number 5 stays, an alias
 * shown as LP24 that the knob passes over (params.c enum_orig), LP24 at DRIVE 70 % at least (voice.c tf_lp24: what
 * sounds closest), so a project or a preset of before still plays it */
enum { FT_LP, FT_HP, FT_BP, FT_COMB, FT_COMBN, FT_LP24H, FT_LP24, FT_N };   /* (V10.22: LP24, voice.c tf_lp24) */
#define FT_IS_LP24(ft) ((ft) == FT_LP24 || (ft) == FT_LP24H)
#define FT_IS_COMB(ft) ((ft) == FT_COMB || (ft) == FT_COMBN)
#define FT_POST(ft) ((ft) >= FT_COMB)
/* the TYPE stored, in range (was & 3: a value past the last read as another TYPE) */
static inline uint32_t ft_of(const int16_t *p)
{
    return (uint32_t)clamp(p[P_FTYPE], 0, FT_N - 1);
}
/* the TYPE a voice's own filter plays (ANALOG, LOFI): LP HP BP, or open (FT_COMB in svf_mode) under a part's TYPE;
 * (codex/filter2-mod) open under FILTER 2's METAL too (NOISE on, MODE METAL: voice.c fn_mode, FN_METAL_ON), the part's */
static __attribute__((noinline)) uint32_t fn_metal(const int16_t *p)   /* (voice.c FN_METAL_ON: one copy) */
{
    (void)p; return 0;   /* (BYTE: FILTER 2's NOISE gone) */
}
static inline uint32_t ft_voice(const int16_t *p)
{
    uint32_t f = (uint16_t)p[P_FTYPE];                  /* (a stray negative one: open, as a part's) */
    return f < FT_COMB && !fn_metal(p) ? f : FT_COMB;
}
static inline int32_t svf_mode(const tsvf_t *c, int32_t in, int32_t *ic1, int32_t *ic2, uint32_t ft, int32_t k)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return ft == FT_HP ? in - v2 - ((k * v1) >> 12) : ft == FT_BP ? (k * v1) >> 12 : ft == FT_COMB ? in : v2;
}
/* the integer square root (voice.c's LP24 tf_lp24; was the retired WEST's) */
static uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
