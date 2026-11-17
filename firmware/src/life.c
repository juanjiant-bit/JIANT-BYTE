/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* BYTE v0.3: musical LIFE. PULSE is a shallow beat-synchronous breath; COLOR a slow,
 * parallel one-pole lowpass; LINK a normalized envelope interaction. It never changes
 * oscillator phase, formula time, CAOS, note timing, pitch or FX sends. All gains <= 1.
 * The visual beings and explicit matrix LIFE/BEING sources keep swimming independently. */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))
typedef struct {
    int32_t bx[NTRK], by[NTRK];
    uint32_t bclk[NTRK];
    int16_t en[NTRK], breath[NTRK], w[NTRK][NTRK];
    uint8_t dir[NTRK], fol, on, init;
    uint32_t phase, slowphase;                                    /* one turn per quarter note */
    uint16_t pulse, color;                             /* 0..1024 */
    uint16_t link[NTRK], gain[NTRK], wet[NTRK], coef[NTRK];
    int32_t lp[NTRK][3];                               /* mono sum and the two stereo image sides */
    uint8_t active[NTRK];
} life_t;
static life_t life LIFE_POOL;
static __attribute__((noinline)) int32_t life_noise(uint32_t seed, uint32_t clk, uint32_t lg) { return drift_noise(seed, clk, lg); }
static int32_t life_pt(uint32_t ti, int32_t lof)
{
    return clamp(life_v[LIFE_AMT], 0, 100) * clamp(life_v[LIFE_N + ti % NTRK] + lof, 0, 100) * 256 / 10000;
}
static LIFE_SZ int32_t life_src(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk) % NTRK;
    return s == MS_LIFE ? life.breath[i] : clamp(32767 - life.by[i], -32768, 32767);
}
static LIFE_SZ void life_swim(uint32_t i, int32_t en)
{
    uint32_t k, c = life.bclk[i] += 8u * (24u + ((uint32_t)en >> 8));   /* (loud: ~6 x the pace) */
    for (k = 0; k < 2u; k++) {                          /* x, then y */
        int32_t *q = k ? &life.by[i] : &life.bx[i];
        int32_t v = (life_noise(i * 977u + k * 0x3b0u + 11u, c >> 4, 11u) * (96 + (en >> 7))) >> 17;
        *q += (life.dir[i] >> k) & 1u ? -v : v;
        if (*q < 4000 || *q > 61535) {
            life.dir[i] ^= (uint8_t)(1u << k);
            *q = clamp(*q, 4000, 61535);
        }
    }
    life.breath[i] = (int16_t)life_noise(i * 31u + 7u, c >> 3, 9u);
}

static LIFE_SZ void life_master(void)
{
    uint32_t i, j, step = div_samples(2), beat = step * 4u;
    uint32_t den = step ? step : 1u, pos = clk_pos == CLK_START ? 0u : clk_pos, frac;
    if (!beat) beat = 1u;
    /* Q16 fraction of a sixteenth, using only the target's 32-bit divide. */
    while (den > 65535u) { den = (den + 1u) >> 1; pos >>= 1; }
    if (pos >= den) pos = den - 1u;
    frac = (pos << 16) / den;
    if (song.playing) {
        life.phase = ((clk_n & 3u) << 30) + (frac << 14);
        life.slowphase = ((clk_n & 63u) << 26) + (frac << 10);
    } else {
        uint32_t delta = (0xffffffffu / beat) * CTL;
        life.phase += delta; life.slowphase += delta >> 4;
    }
    /* 0 at each beat, peak between beats; COLOR traverses a smooth four-bar arc. */
    life.pulse = (uint16_t)((32767 - sine_i(life.phase + 0x40000000u)) >> 6);
    life.color = (uint16_t)((32767 + sine_i(life.slowphase)) >> 6);
    life.on = life_v[LIFE_AMT] > 0;
    life.fol = 0;
    if (!life.init) {
        life.init = 1;
        for (i = 0; i < NTRK; i++) {
            life.bx[i] = 10000 + i * 8500;
            life.by[i] = i < 3u ? 16000 : 46000;
        }
    }
    for (i = 0; i < NTRK; i++) {
        int32_t e = i < trk_live && !trk[i].p[P_MUTE] ? clamp(track_env[i] * 2, 0, 32767) : 0;
        int32_t d = e - life.en[i];
        life.en[i] += (int16_t)(d / (d > 0 ? 8 : 128)); /* ~12 ms attack / 185 ms release */
        if (life.on && i < trk_live && !trk[i].p[P_MUTE] && life_v[LIFE_LINK]) life.fol |= 1u << i;
        if ((mns_clk & 7u) == i) life_swim(i, life.en[i]);
    }
    life.fol &= (uint8_t)~track_follow_mask;
    track_follow_mask |= life.fol;
    for (i = 0; i < NTRK; i++) {
        uint32_t sum = 0, weight = 0;
        for (j = 0; j < NTRK; j++) {
            uint32_t w = i != j && j < trk_live && !trk[j].p[P_MUTE] && life_v[LIFE_N + j] ?
                         (trk[j].engine == ENGI_DRUM ? 2u : 1u) : 0u;
            life.w[i][j] = (int16_t)(w ? 8192u * w : 0u);
            sum += w * (uint32_t)life.en[j]; weight += w;
        }
        /* Averaging, rather than summing, prevents dense songs from increasing the depth. */
        life.link[i] = (uint16_t)(weight ? (sum / weight) >> 5 : 0u);
    }
}
static LIFE_SZ int32_t life_ctx(uint32_t ti, int32_t *c)
{
    c[0] = c[1] = c[2] = 0;
    return life_pt(ti, MOD.on ? MOD.lof : 0);
}
static __attribute__((noinline)) void life_process(uint32_t ti, int32_t *out, uint32_t n, int32_t depth)
{
    uint32_t i, c, side = (dx_xon & 1u) != 0;
    int32_t *ch[3] = {out, dx_sl, dx_sr};
    int32_t pulse = clamp(life_v[LIFE_PULSE], 0, 100), color = clamp(life_v[LIFE_COLOR], 0, 100);
    int32_t link = clamp(life_v[LIFE_LINK], 0, 100), g, wet, cf, g0, w0, c0;
    ti %= NTRK;
    if (!n) return;
    if (!depth || !(pulse || color || link)) { life.active[ti] = 0; return; }
    /* Q10: at full depth PULSE <=12%, LINK <=8%, COLOR <=25% wet. */
    g = 1024 - (int32_t)(life.pulse * (uint32_t)pulse * 123u / 102400u) -
               (int32_t)(life.link[ti] * (uint32_t)link * 82u / 102400u);
    g = 1024 - ((1024 - g) * depth >> 8);
    wet = (color * depth) / 100;                        /* <=256/1024 */
    cf = 128 + (life.color * 512 >> 10);                /* stable one pole; no resonance or feedback */
    if (!life.active[ti]) {
        life.gain[ti] = 1024; life.wet[ti] = 0; life.coef[ti] = (uint16_t)cf;
        for (c = 0; c < 3u; c++) life.lp[ti][c] = c && !side ? 0 : ch[c][0];
        life.active[ti] = 1;
    }
    g0 = life.gain[ti]; w0 = life.wet[ti]; c0 = life.coef[ti];
    g = g0 + clamp(g - g0, -8, 8); wet = w0 + clamp(wet - w0, -8, 8); /* knob moves smoothed */
    {
        int32_t da = (cf - c0) * 65536 / (int32_t)n;
        int32_t dm = (wet - w0) * 65536 / (int32_t)n;
        int32_t dg = (g - g0) * 65536 / (int32_t)n;
        for (c = 0; c < (side ? 3u : 1u); c++) {
            int32_t z = life.lp[ti][c], a = c0 * 65536, mix = w0 * 65536, amp = g0 * 65536;
            if (!w0) z = ch[c][0];
            for (i = 0; i < n; i++) {
                int32_t x = ch[c][i]; a += da; mix += dm; amp += dg;
                if (wet || w0) {
                    z += (int32_t)(((int64_t)x - z) * (a >> 16) >> 10);
                    x += (int32_t)(((int64_t)z - x) * (mix >> 16) >> 10);
                }
                ch[c][i] = (int32_t)((int64_t)x * (amp >> 16) >> 10);
            }
            life.lp[ti][c] = z;
        }
    }
    life.gain[ti] = (uint16_t)g; life.wet[ti] = (uint16_t)wet; life.coef[ti] = (uint16_t)cf;
}
static void life_reset(void) { memset(&life, 0, sizeof life); }
