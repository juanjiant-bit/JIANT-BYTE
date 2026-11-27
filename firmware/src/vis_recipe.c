/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (visgen) VIS's recipe: a song's visual from what made it (docs/VISGEN.md, the approved mock-up docs/visgen-mock.html).
 * A pure function, no UI and no state: GEN's seed, the genre, the energy, the engines the song plays (VIS_E_* bits) and
 * the recipe byte (OCT+ on HOME > VIS: another recipe for the same song, 0 the song's own) -> what is drawn (content),
 * on what (skeleton), what goes over it, how it is bent (1 or 2 deformations), how the layers meet (blend), the colours
 * (palette), how much they melt (MIX), how much the music moves it (REACT); then sections B, C, D each change one piece
 * (the skeleton, a deformation, or the layer over). JIANT Studio reimplements it in JS: docs/VISGEN.md is its spec, its
 * test vectors made by tests/visgen_test.c from this code. Every number unsigned 32-bit (JS: >>> 0, Math.imul) */
enum { VI_LIFE, VI_KEY, VI_HARMONY, VI_LFO, VI_SPLASH, VI_RADAR, VI_ORBIT, VI_CLAY, VI_TUNNEL, VI_CHORUS, VI_HIVE,
       VI_FUNC, VI_MATH, VI_N, VI_NONE = 255 };
enum { VD_TWIST, VD_FOLD, VD_MIRROR, VD_POLAR, VD_MELT, VD_SCAN, VD_N };
enum { VB_THERM, VB_SUM, VB_MIX };                                   /* thermal pulse, colour sum, thermal + wire */
enum { VP_AUTO, VP_THERM, VP_HUD, VP_TRACKS, VP_ICE, VP_AMBER, VP_N };
enum { VE_WAVE, VE_MATH, VE_FUNC, VE_BANK, VE_DRUMW, VE_HIVE, VE_CLAY, VE_FX };   /* the engine mask's bits */
#define VIS_GENRES 27u                                               /* gen_tables.c GENRE; 27 (or more): none (EXPLORE) */
typedef struct {
    uint8_t content, fam, mood, blend, pal, mix, react, lis;         /* lis: the Lissajous a:b (VIS_LIS) */
    uint8_t skel[4], over[4];                                        /* sections A..D (over: VI_NONE none) */
    uint8_t def[4][2], amt[4][2];                                    /* .. their deformations (VI_NONE none), 0..100 */
} vis_recipe_t;
static const uint8_t VIS_LIS[7][2] = {{1, 2}, {2, 3}, {3, 4}, {3, 2}, {1, 3}, {3, 5}, {2, 5}};
/* a genre's family: 0 MACHINE 1 HOUSE 2 LATIN 3 IDM 4 SOFT 5 HEAVY (none: HOUSE) */
static const uint8_t VIS_FAM[VIS_GENRES] = {0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 0, 3, 3, 3, 4, 3, 1, 0, 4, 4, 5, 5, 4, 5, 2, 4, 5};
/* the ingredients' weights in the pool (x10): the five always there, then each engine's */
static const uint8_t VIS_W[VI_N] = {12, 10, 8, 7, 5, 30, 30, 30, 30, 30, 30, 30, 30};
static const uint8_t VIS_ENG_ING[VI_N] = {0, 0, 0, 0, 0, VE_DRUMW, VE_WAVE, VE_CLAY, VE_FX, VE_FX, VE_HIVE, VE_FUNC, VE_MATH};
#define VIS_SKEL_OK 0x17EBu                                          /* can be a skeleton: LIFE KEY LFO RADAR ORBIT CLAY
                                                                      * TUNNEL CHORUS HIVE MATH (not HARMONY SPLASH FUNC) */
/* a family's taste for each skeleton (x10; 0: 4) */
static const uint8_t VIS_FSK[6][VI_N] = {
    {10, 0, 0, 0, 0, 30, 0, 10, 30, 0, 0, 0, 0},                    /* MACHINE: TUNNEL RADAR, LIFE CLAY */
    {30, 20, 0, 0, 0, 0, 20, 0, 0, 10, 0, 0, 0},                    /* HOUSE: LIFE, KEY ORBIT, CHORUS */
    {10, 0, 0, 10, 0, 30, 0, 0, 0, 0, 30, 0, 0},                    /* LATIN: RADAR HIVE, LIFE LFO */
    {0, 0, 0, 0, 0, 20, 10, 20, 0, 0, 0, 0, 30},                    /* IDM: MATH, CLAY RADAR, ORBIT */
    {0, 30, 0, 0, 0, 0, 20, 0, 10, 20, 10, 0, 0},                   /* SOFT: KEY, CHORUS ORBIT, TUNNEL HIVE */
    {0, 0, 0, 0, 0, 20, 0, 10, 30, 0, 0, 0, 0},                     /* HEAVY: TUNNEL, RADAR, CLAY */
};
static const uint8_t VIS_FBL[6][3] = {{3, 0, 1}, {1, 1, 3}, {0, 3, 1}, {2, 2, 0}, {0, 1, 3}, {3, 0, 1}};   /* THERM SUM MIX */
static const uint8_t VIS_FDF[6][VD_N] = {                            /* TWIST FOLD MIRROR POLAR MELT SCAN */
    {2, 0, 0, 1, 0, 3}, {2, 0, 2, 2, 0, 0}, {1, 2, 3, 0, 0, 0}, {1, 3, 0, 0, 0, 3}, {0, 0, 2, 2, 3, 0}, {2, 0, 0, 0, 2, 2}};
static const uint8_t VIS_FPAL[6][3] = {{VP_THERM, VP_AMBER, 0}, {VP_THERM, VP_HUD, 0}, {VP_TRACKS, VP_THERM, 0},
                                       {VP_ICE, VP_THERM, 0}, {VP_ICE, VP_AMBER, VP_HUD}, {VP_AMBER, VP_THERM, 0}};
static uint32_t vr_x;
static uint32_t vr_rnd(void) { vr_x ^= vr_x << 13; vr_x ^= vr_x >> 17; vr_x ^= vr_x << 5; return vr_x; }
static uint32_t vr_pick(const uint32_t *w, uint32_t n)              /* an index by its weight (all 0: VI_NONE) */
{
    uint32_t i, s = 0, r;
    for (i = 0; i < n; i++) s += w[i];
    if (!s) return VI_NONE;
    r = vr_rnd() % s;
    for (i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; }
    return VI_NONE;
}
/* mask: VE_* bits; rec: the recipe byte */
static void vis_recipe(vis_recipe_t *R, uint32_t seed, uint32_t genre, uint32_t energy, uint32_t mask, uint32_t rec)
{
    uint32_t i, k, s, h = 2166136261u, pool[VI_N], sk[VI_N], ov[VI_N], w[VD_N], fam, nd;
    const uint32_t in[5] = {seed & 0x1FFFFFu, genre, energy, mask & 255u, rec & 255u};
    for (i = 0; i < 5u; i++) { h = (h ^ in[i]) * 16777619u; h ^= h >> 13; }
    vr_x = h ? h : 1u;
    energy = energy > 100u ? 100u : energy;
    fam = genre < VIS_GENRES ? VIS_FAM[genre] : 1u;
    R->fam = (uint8_t)fam;
    R->mood = (uint8_t)(energy < 40u ? 0u : energy < 70u ? 1u : 2u);   /* CALM GROOVE PEAK */
    for (i = 0; i < VI_N; i++)                                       /* the pool: the five, the song's engines' */
        pool[i] = i < VI_RADAR || ((mask >> VIS_ENG_ING[i]) & 1u) ? VIS_W[i] : 0u;
    R->content = (uint8_t)vr_pick(pool, VI_N);
    for (i = 0; i < VI_N; i++) {
        sk[i] = i != R->content && ((VIS_SKEL_OK >> i) & 1u) ? pool[i] * (VIS_FSK[fam][i] ? VIS_FSK[fam][i] : 4u) : 0u;
        ov[i] = i != R->content && i != VI_SPLASH ? pool[i] : 0u;
    }
    R->skel[0] = (uint8_t)vr_pick(sk, VI_N);
    R->over[0] = VI_NONE;
    if (vr_rnd() % 220u < 66u + energy) {                            /* a third layer over: likelier the louder */
        k = ov[R->skel[0]]; ov[R->skel[0]] = 0;
        R->over[0] = (uint8_t)vr_pick(ov, VI_N);
        ov[R->skel[0]] = k;
    }
    for (i = 0; i < 3u; i++) w[i] = VIS_FBL[fam][i];
    R->blend = (uint8_t)vr_pick(w, 3u);
    for (i = 0; i < VD_N; i++) w[i] = VIS_FDF[fam][i];
    if ((mask >> VE_BANK) & 1u && w[VD_MELT] < 3u) w[VD_MELT] = 3u;  /* (BANK: the sample stretched, melt) */
    nd = energy > 70u ? 2u : 1u;
    R->def[0][0] = R->def[0][1] = VI_NONE;
    R->amt[0][0] = R->amt[0][1] = 0;
    for (i = 0; i < nd; i++) {
        k = vr_pick(w, VD_N);
        if (k == VI_NONE) break;
        w[k] = 0;
        R->def[0][i] = (uint8_t)k;
        R->amt[0][i] = (uint8_t)(45u + vr_rnd() % 50u);
    }
    k = VIS_FPAL[fam][2] ? 3u : 2u;
    R->pal = VIS_FPAL[fam][vr_rnd() % k];
    R->mix = (uint8_t)(62u + vr_rnd() % 34u);
    R->react = (uint8_t)(45u + energy * 10u / 17u > 100u ? 100u : 45u + energy * 10u / 17u);
    R->lis = (uint8_t)(vr_rnd() % 7u);
    for (s = 1; s < 4u; s++) {                                       /* B C D: each one piece changed */
        uint32_t r = vr_rnd() % 100u;
        R->skel[s] = R->skel[s - 1u]; R->over[s] = R->over[s - 1u];
        R->def[s][0] = R->def[s - 1u][0]; R->def[s][1] = R->def[s - 1u][1];
        R->amt[s][0] = R->amt[s - 1u][0]; R->amt[s][1] = R->amt[s - 1u][1];
        if (r < 45u) {                                               /* another skeleton (not the one over) */
            uint32_t o = R->over[s] < VI_N ? R->over[s] : R->skel[s], ko = sk[o];
            k = sk[R->skel[s]]; sk[R->skel[s]] = 0; sk[o] = 0;
            i = vr_pick(sk, VI_N);
            sk[o] = ko; sk[R->skel[s - 1u]] = k;
            if (i != VI_NONE) R->skel[s] = (uint8_t)i;
        } else if (r < 75u) {                                        /* a deformation more (the first goes when two) */
            uint32_t f[VD_N], n = 0;
            for (i = 0; i < VD_N; i++)
                if (i != R->def[s][0] && i != R->def[s][1]) f[n++] = i;
            k = f[vr_rnd() % n];
            if (R->def[s][1] != VI_NONE) { R->def[s][0] = R->def[s][1]; R->amt[s][0] = R->amt[s][1]; }
            i = R->def[s][0] == VI_NONE ? 0u : 1u;
            R->def[s][i] = (uint8_t)k;
            R->amt[s][i] = (uint8_t)(50u + vr_rnd() % 45u);
        } else {                                                     /* another layer over (or none) */
            for (i = 0; i < VI_N; i++)
                ov[i] = i != R->content && i != VI_SPLASH && i != R->over[s - 1u] && i != R->skel[s] ? pool[i] : 0u;
            R->over[s] = (uint8_t)vr_pick(ov, VI_N);
        }
    }
}
