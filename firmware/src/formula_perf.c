/* SPDX-License-Identifier: GPL-3.0-only
 * Momentary spectral exchange. Amounts select donor literals, harmonic
 * fragments and phase links in the shared active-voice network. */
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
#define formula_run fl_run
