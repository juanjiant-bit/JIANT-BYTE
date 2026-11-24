/* SPDX-License-Identifier: GPL-3.0-only
 * Momentary spectral exchange. Amounts select donor literals, harmonic
 * fragments and phase links in the shared active-voice network. */
/* (BYTE, CPU) ABOUT's fine timings: TIMER4 ticks (24 MHz) summed per section, read a second at a time (ui_menu.c) */
#if FELUCCA_DUAL_CORE
#define PROF_T() fm1_mc_ticks()                     /* (hal/fm1_multicore.h: TIMER4, 24 MHz) */
#else
#define PROF_T() 0u
#endif
enum { PQ_WTAB, PQ_WVM, PQ_WRD, PQ_WALL, PQ_DMOD, PQ_DBODY, PQ_DALL, PQ_N };
static volatile uint32_t prof_tk[PQ_N] __attribute__((section(".lowbuf")));
#define PROF_ADD(k, t0) (prof_tk[k] += PROF_T() - (t0))
static uint32_t formula_punch;
static uint8_t punch_amount[3];
static const uint8_t punch_profile[][3]={
    {80,10,10},{20,80,0},{10,15,85},{60,35,15},{45,65,0},
    {15,50,65},{65,10,60},{30,75,35},{50,45,45},{70,70,25},{55,55,70}
};
static void punch_exchange_tick(void)
{
    for(unsigned axis=0;axis<3;axis++){
        int target=0;
        for(uint32_t bits=formula_punch;bits;bits&=bits-1){
            int value=punch_profile[__builtin_ctz(bits)][axis];
            if(value>target)target=value;
        }
        int current=punch_amount[axis];
        punch_amount[axis]=(uint8_t)(current+clamp(target-current,-2,3));
    }
}
/* (BYTE 0.16, the owner: "the white keys do nothing, only CAOS MAX") the ten white punch-ins as transforms of a
 * formula's time t, MATH's, WAVE's and FORM's (OCT- and OCT+ also the pitch, every engine: pfx.c pfx_pit). sh: the
 * engine's time against MATH's (~8 kHz t): its windows that much longer. Bit k: the key k (PF_OCTD + k) */
static uint32_t pt_ops;
enum { PT_OCTD, PT_OCTU, PT_HALF, PT_WOB, PT_REP, PT_S8, PT_S16, PT_BITS, PT_MIR, PT_SCR };
static __attribute__((noinline)) uint32_t pt_time_x(uint32_t t, uint32_t sh)
{
    uint32_t o = pt_ops;
    if (o & (1u << PT_HALF))                            /* HALF: the rhythm's bits at half speed, the tone's as they were */
        t = ((t >> 1) & ~((256u << sh) - 1u)) | (t & ((256u << sh) - 1u));
    if (o & (1u << PT_REP)) {                           /* REPEAT: each bar's first eighth again (~1/8 at 120) */
        uint32_t L = 2048u << sh;
        t = (t & ~(4u * L - 1u)) | (t & (L - 1u));
    }
    if (o & ((1u << PT_S8) | (1u << PT_S16))) {          /* STEP: the slow bits held a step, the tone running */
        uint32_t M = (o & (1u << PT_S16) ? 1024u : 2048u) << sh;
        t = (t & ~(M - 1u)) | (t & ((256u << sh) - 1u));
    }
    if (o & (1u << PT_MIR)) {                           /* MIRROR: forward, then back, ~half a second each */
        uint32_t W = 4096u << sh;
        if (t & W) t ^= W - 1u;
    }
    if (o & (1u << PT_SCR))                             /* SCRAMBLE: the beats out of order */
        t ^= ((((t >> (13u + sh)) * 0x9E3779B1u) >> 30) & 3u) << (11u + sh);
    if (o & (1u << PT_BITS))                            /* BITS: t with itself, shifted */
        t ^= t >> (5u + sh);
    if (o & (1u << PT_WOB))                             /* WOBBLE: the time swayed, ~2 Hz */
        t += (uint32_t)(sine_i((t >> sh) << 18) >> (9 - (int32_t)sh));
    return t;
}
#define PT_TIME_OPS (~((1u << PT_OCTD) | (1u << PT_OCTU)) & 1023u)   /* (OCT- OCT+: the pitch, pfx.c) */
static inline uint32_t pt_time(uint32_t t, uint32_t sh) { return (pt_ops & PT_TIME_OPS) ? pt_time_x(t, sh) : t; }
/* (BYTE punchfx, the owner: "some punch-ins are not noticeable with the engines") what t cannot carry, for every engine
 * (pfx.c pfx_block, each block): PT_CAOS, pt_ops' bit 10 (not a time op: pt_time never sees it), CAOS MAX held; PT_LOOP
 * bit 11, LOOP 3 / 5 / 7 (FUNC's P looped: pt_lp0, pt_lpn); pt_vp a part's pitch added this block (1/16 semitones:
 * WOBBLE's sway and CAOS MAX's throw, voice.c and eng_drum.c on every voice, never in the note numbers), pt_wob WOBBLE's
 * share of it. In .bss: the pool has not a byte over its 8 KiB of headroom */
#define PT_CAOS 10u
#define PT_LOOP 11u
static int16_t pt_vp[NTRK], pt_wob;
static int16_t pt_cw;                                   /* (WOBBLE: MATH's filter's cutoff, eng_lofi.c, Q8) */
static uint32_t pt_lp0;                                 /* (the LOOP's first 1/16, in P: Q16 beats) */
static uint8_t pt_lpn;                                  /* (its 1/16s: 3, 5, 7; 0 none) */
/* (BYTE punchfx) the white punch-ins on a time counted in beats (Q16: FUNC's P, the transport's), tempo-locked and in
 * the bar (each window from the bar's start, as pt_time's from MATH's 0): HALF each bar's first half at half speed,
 * REPEAT each half bar's first 1/8 four times, STEP 8 / 16 each 1/8 / 1/16 its first half twice, MIRROR every second
 * beat backwards, SCRAMBLE each half bar's four 1/8s out of order, WOBBLE the time swayed +-1/16 at the 1/8, CAOS MAX
 * a 1/16 in four thrown somewhere else in its bar, LOOP 3 / 5 / 7 the last 3 / 5 / 7 1/16s again (as the steps', pfx.c:
 * from the one playing as it went in; the others then on the looped time). BITS: the part's crush (fx.c), not the time */
static __attribute__((noinline)) uint32_t pt_beat_x(uint32_t p, uint32_t song_p)
{
    const uint32_t B = 1u << 16;
    uint32_t o = pt_ops, bar, q;
    if ((o & (1u << PT_LOOP)) && song_p && pt_lpn && p - pt_lp0 < 0x40000000u)   /* (song_p: p the song's, as pt_lp0) */
        p = pt_lp0 + (p - pt_lp0) % (pt_lpn * (B / 4u));
    bar = p & ~(4u * B - 1u);
    q = p - bar;
    if (o & (1u << PT_HALF))
        q >>= 1;
    if (o & (1u << PT_REP))
        q &= ~(2u * B - 1u) | (B / 2u - 1u);
    if (o & (1u << PT_S8))
        q &= ~(B / 2u - 1u) | (B / 4u - 1u);
    if (o & (1u << PT_S16))
        q &= ~(B / 4u - 1u) | (B / 8u - 1u);
    if ((o & (1u << PT_MIR)) && (q & B))
        q ^= B - 1u;
    if (o & (1u << PT_SCR))
        q ^= (fvm_hash((p >> 17) * 977u + 3u) & 3u) << 15;
    if (o & (1u << PT_CAOS)) {
        uint32_t h = fvm_hash((p >> 14) * 0x9E3779B1u + 11u);
        if (!(h & 3u))
            q = (q + ((h >> 8) & 15u) * (B / 4u)) & (4u * B - 1u);
    }
    p = bar + q;
    if (o & (1u << PT_WOB))
        p += (uint32_t)(sine_i(p << 17) >> 1);
    return p;
}
#define PT_BEAT_OPS (PT_TIME_OPS & ~(1u << PT_BITS) | 1u << PT_CAOS | 1u << PT_LOOP)

#define formula_run fl_run
