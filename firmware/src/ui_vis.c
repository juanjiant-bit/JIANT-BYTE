/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (visgen; VIS lite, the owner: "something simpler that works and doesn't spend CPU", "visuals must cost ZERO when they
 * are not on screen") HOME > VIS and MENU > SAVER: the song's visual, the whole screen (HOME > VIS: a knob turned, OCT+,
 * its card a moment over it). The song's SET of 8 recipes (vis_recipe.c: the recipe byte and the 7 after it): a kick
 * cuts to the next, a snare (or a timbre change) jumps, a section reshuffles (docs/VISGEN.md "Cuts"). The recipe drawn
 * says which ingredient (its content) is drawn as its own clean shape (VIS_SHP: rings, satellites, waves, polygons,
 * Lissajous; 1 to 3 strokes) on the skeleton's carrier (a sphere, polar, the orbit, a wave: MIX of it), at most one
 * deformation (only while it sounds), the camera turning and zooming. No layer over, no mirror drawn twice, no heat, no
 * grain. At most VIS_SEG segments a frame, 2 px, in colours of at least ~40 % luminance.
 * The cost: nothing of it runs unless the visual is on the screen (vis_saver, ui_draw.c) or Studio watches it (WATCH
 * bit 7: vis_state, editor.c, 25 looks a second); nothing in the audio path (the parts' levels: their own peaks, fx.c
 * t->peak, read here). Drawn every 2nd UI frame, the points worked out once a frame (in 1.6 KB of the display cache's
 * hashes, free while the visual fills the screen: the page after it drawn whole), each band drawing their lines; the
 * audio's load (the guard's, voice.c load_avg_q8 load_rl) thins it: past 50 % fewer segments and frames, past 70 % or
 * the guard on: not drawn at all (the last frame stays). Included by ui_input.c */
#define VABS(v) ((v) < 0 ? -(v) : (v))
#define VNI __attribute__((noinline))                  /* (each its own function: smaller; tools/build.py places them past the sound) */
#define VIS_SEG 200u                                    /* the most segments a frame */
#define VIS_NPT 400u                                    /* the points stored: x, y (VIS_UP: a break, y its colour) */
#define VIS_PT ((int16_t *)(dc_hash + DC_USE - 800u))
#define VIS_UP (-32768)
static struct {
    vis_recipe_t r;                                /* the set's recipe drawn (worked out on a cut: the BT segment's) */
    uint32_t key, tst, lms, seen;                  /* the set's inputs; Studio's timbre signature; the levels' last
                                                    * look; the visual's last frame on screen */
    int16_t lv[NTRK];                              /* each part's level (0..256: its peak, a quick fall) */
    uint32_t t, ct, tim;                           /* (from here on cleared with the set) now (ms); the last cut; the
                                                    * timbre's signature */
    int32_t kick, snr, lvl, plvl, sc, rot, zp, mix, react, en, def, cx, cy, R;   /* 0..256 (sc Q10; rot a turn 65536;
                                                    * en: the sound, held ~0.7 s; def: the deformation now) */
    uint16_t np, ns, cap, col;                     /* the points stored, the segments, their cap; the stroke's colour */
    uint8_t ri, rr, pos, sec, chord, root, q, hacc, hst, dwp, pal, dk, sk, pen;   /* (hacc: the frame's hits; hst:
                                                    * Studio's; dk: the deformation; sk: the carrier's skeleton) */
} vg __attribute__((section(".pool")));
/* the five palettes' ramps (docs/VISGEN.md VP_*: THERM HUD TRACKS ICE AMBER), four stops each */
static const uint16_t VIS_RAMP[5][4] = {
    {RGB(57, 40, 172), RGB(255, 64, 74), RGB(255, 190, 90), RGB(255, 255, 255)},
    {RGB(41, 60, 255), RGB(58, 208, 255), RGB(238, 242, 90), RGB(255, 255, 255)},
    {RGB(164, 93, 255), RGB(255, 56, 180), RGB(255, 137, 41), RGB(255, 210, 57)},
    {RGB(10, 20, 90), RGB(41, 60, 255), RGB(58, 208, 255), RGB(255, 255, 255)},
    {RGB(60, 15, 0), RGB(255, 120, 20), RGB(255, 200, 60), RGB(255, 245, 190)}};
static VNI uint32_t vis_lum(uint32_t c) { return ((c >> 11) * 616u + ((c >> 5) & 63u) * 600u + (c & 31u) * 232u) >> 8; }   /* 0..250 */
static VNI uint16_t vis_col(uint32_t v)                     /* 128..255 along the palette's upper half, at least ~40 % luminance */
{
    const uint16_t *p = VIS_RAMP[vg.pal];
    uint32_t k = v * 3u >> 8;
    uint16_t c = ux_mix(p[k], p[k + 1u], (int32_t)((v * 3u & 255u) * 100u >> 8));
    while (vis_lum(c) < 100u)                           /* (a dark stop: lifted toward white) */
        c = ux_mix(c, 0xFFFFu, 30);
    return c;
}
#define vsin bg_sin                                     /* (a turn 65536, Q15) */
static uint32_t vph(uint32_t k) { return vg.t * k >> 4; }   /* the time's phase at k / 167 rad a second */

/* ------------------------------------------------------------------------------------------- the song --- */
/* an engine's VE_* bit (engines.c's numbers: LOFI MATH, DRUM DRUM-W, WAVE, HIVE, FUNC) */
static const uint8_t VIS_EB[NENGINES] = {[ENGI_LOFI] = 2, [ENGI_DRUM] = 16, [ENGI_WAVE] = 1, [ENGI_HIVE] = 32, [ENGI_FUNC] = 4};
static VNI uint32_t vis_bits(uint32_t i)                    /* VE_* of part i */
{
    const track_t *t = &trk[i];
    uint32_t b = VIS_EB[trk_eng(t)];
    if (b == 4u && t->p[P_E7] == (int)FUNC_BANK) b = 12u;
    return b | (clay[i][CL_ON] ? 64u : 0u) | (t->p[P_DLY] > 0 || t->p[P_CHOR] > 0 ? 128u : 0u);
}
static VNI uint32_t vis_mask(void)                          /* VE_* of the parts playing */
{
    uint32_t i, m = 0;
    for (i = 0; i < trk_live && i < NTRK; i++) m |= vis_bits(i);
    return m;
}
/* the song's seed, genre, energy, recipe byte (o[4]); a song GEN did not make: its tempo's and first part's, none, 50 */
static VNI void vis_song(uint32_t *o)
{
    uint32_t h = ((uint32_t)song.g[G_BPM] * 131u + (uint32_t)trk[0].p[P_ROOT] * 17u + (uint32_t)trk[0].p[P_SLEN]) * 2654435761u;
    int g = vis_id[2] != 0;
    o[0] = g ? (uint32_t)vis_id[0] | (uint32_t)vis_id[1] << 8 : h >> 11;
    o[1] = g ? vis_id[2] - 1u : VIS_GENRES;
    o[2] = g ? vis_id[3] : 50u;
    o[3] = vis_id[4];
}
static VNI void vis_gen_set(uint32_t seed, uint32_t genre, uint32_t energy)   /* (gen.c: a piece made; its recipe byte 0) */
{
    vis_id[0] = (uint8_t)seed; vis_id[1] = (uint8_t)(seed >> 8);
    vis_id[2] = (uint8_t)((genre < VIS_GENRES ? genre : VIS_GENRES) + 1u);
    vis_id[3] = (uint8_t)(energy > 100u ? 100u : energy);
    vis_id[4] = 0;
}
static VNI void vis_oct(int up)                             /* OCT+ another recipe, OCT- the song's own (0) */
{
    vis_id[4] = up ? (uint8_t)(vis_id[4] + 1u) : 0u;
    vis_ov = (fm1_ms & ~15u) | 4u;                      /* (RECIPE n over the visual a moment) */
}
static const track_t *vis_key(void)                     /* the part whose scale and chord are drawn: the first melodic */
{
    uint32_t i;
    for (i = 0; i < trk_live && i < NTRK; i++)
        if (!drum_track(&trk[i]))
            return &trk[i];
    return TSEL;
}
/* the chord now (SCL > HARMONY's playing, else the key's): its root (0..11), quality (0 MAJ 1 MIN 2 DIM 3 AUG 4 SUS);
 * its index */
static VNI uint32_t vis_chord(void)
{
    const track_t *t = vis_key();
    uint32_t m = scale_mask(t) & 0xFFFu, i = 0, d = 0, x = 0, b = 0, r;
    if (__builtin_popcount(m) != 7) m = 0xAB5u;
    if (hm_n && song.playing) { i = hm_now(); d = (uint32_t)(hm_dg[i] % 7 + 7) % 7u; x = hm_x[i]; }
    for (r = 0;; b++)                                   /* the degree's semitone (b) */
        if (((m >> b) & 1u) && r++ == d) break;
    r = (m | m << 12) >> b;                             /* the scale from it: its third and fifth */
    vg.root = (uint8_t)(((uint32_t)clamp(t->p[P_ROOT], 0, 11) + (x & 3u ? HM_SEMI(x) : b)) % 12u);
    vg.q = (uint8_t)(r & 8u ? (r & 128u ? 1u : 2u) : r & 16u ? (r & 128u ? 0u : 3u) : 4u);
    return i;
}
static VNI uint32_t vis_sec(void) { return live.cur >= 0 ? (uint32_t)live.cur & 3u : song.playing ? (bar_n >> 3) & 3u : 0u; }
static VNI void vis_hits(void)                              /* DRUM-W's lanes struck since the last look: vg.hacc */
{
    uint32_t k, h = 0, now = 0;
    for (k = 0; k < 8u; k++) {                          /* (bit 0 BD; 1 SD CP; 2 CH OH; 3 TM RS CB) */
        now |= (uint32_t)(dw_live[k] >> 15 & 1u) << k;
        if ((now & ~(uint32_t)vg.dwp) >> k & 1u)
            h |= 1u << (0x33322110u >> 4u * k & 15u);
    }
    vg.dwp = (uint8_t)now;
    vg.hacc |= (uint8_t)h;
    vg.hst |= (uint8_t)h;
}
/* the parts' levels (vg.lv): each part's peak since the last look (fx.c t->peak, kept by the audio as ever; read and
 * cleared here, as the pages' meters do: nothing added to the audio path), a quick fall (~0.25 s from full) */
static VNI void vis_levels(void)
{
    uint32_t i, e = fm1_ms - vg.lms;
    if (e < 8u)
        return;
    vg.lms = fm1_ms;
    for (i = 0; i < NTRK; i++) {
        int32_t v = trk[i].peak * 256 / 7000, l = vg.lv[i] - (int32_t)(e > 256u ? 256u : e);
        trk[i].peak = 0;
        v = v > 256 ? 256 : v;
        vg.lv[i] = (int16_t)(v > l ? v : l > 0 ? l : 0);
    }
}
/* the timbre's signature: each part's engine, preset and filter CUT (a quarter of its range) */
static VNI uint32_t vis_tsig(void)
{
    uint32_t i, st = 0;
    for (i = 0; i < trk_live && i < NTRK; i++)
        st = st * 31u + trk[i].eng_req * 7u + trk[i].preset + ((uint32_t)trk[i].p[P_FCUT] >> 5) * 131u;
    return st;
}
/* JIANT Studio's state (docs/VISGEN.md): VIS_SONG's fields, VIS_TICK's, playing (21); the hits since the last call
 * (bit 4: the timbre changed since then). Only while Studio asks (editor.c WATCH bit 7, web_vis_info) */
static VNI void vis_state(int32_t *o)
{
    uint32_t s[4], i, st = vis_tsig();
    vis_song(s);
    vis_levels();
    o[0] = (int32_t)s[0]; o[1] = (int32_t)s[3]; o[2] = (int32_t)s[1]; o[3] = (int32_t)s[2]; o[4] = (int32_t)vis_mask();
    o[5] = (int32_t)vis_sec(); o[6] = vis_k[2]; o[7] = vis_k[0]; o[8] = vis_k[1]; o[9] = vis_k[3];
    for (i = 0; i < 6u; i++) o[10u + i] = i < NTRK ? clamp(vg.lv[i] >> 1, 0, 127) : 0;
    vis_hits();
    if (vg.tst && st != vg.tst) vg.hst |= 16u;          /* (the first look: none) */
    vg.tst = st;
    o[16] = vg.hst; vg.hst = 0;
    (void)vis_chord();
    o[17] = vg.root; o[18] = vg.q; o[19] = (int32_t)(trk[0].seq_idx & 63u);
    o[20] = song.playing || chain_busy() || seq_counting();   /* (as web_gen_info's) */
}

/* ------------------------------------------------------------------------------------------- the shape --- */
/* each ingredient's clean shape: up to 3 strokes, one curve each, {n, a, b, fg, o, ph, sp} (n 0: none): n corners (bit
 * 7: x and y swapped), corner k at t = k / n of a turn: x = a cos(t f / 2 + time x), y = b sin(t g / 2 + time y)
 * (f g: fg's nibbles; f = g = 2 a circle (n 3, 4, 7: a polygon), f = 1 a line across with a wave on it, f g 0: the
 * recipe's Lissajous), centred o away at ph / 256 of a turn (+ time x: a satellite); sp's nibbles the time's speeds
 * (x, y). Units: 8 of the -1024..1024 box */
static const uint8_t VIS_SHP[VI_N][3][7] = {
    {{32, 88, 88, 0x22, 0, 0, 0x11}, {12, 16, 16, 0x22, 112, 0, 0x33}, {12, 12, 12, 0x22, 112, 128, 0x33}},   /* LIFE */
    {{7, 100, 100, 0x22, 0, 0, 0x11}, {32, 100, 100, 0x22, 0, 0, 0x11}, {24, 46, 46, 0x22, 0, 0, 0x22}},     /* KEY */
    {{3, 100, 100, 0x22, 0, 0, 0x11}, {32, 100, 100, 0x22, 0, 0, 0x11}, {24, 36, 36, 0x22, 0, 0, 0x33}},     /* HARMONY */
    {{32, 120, 56, 0x14, 0, 0, 0x04}, {12, 16, 16, 0x22, 56, 64, 0x30}},                                     /* LFO */
    {{48, 90, 90, 0x46, 0, 0, 0x10}, {40, 50, 50, 0x64, 0, 0, 0x01}},                                        /* SPLASH */
    {{24, 50, 50, 0x22, 0, 0, 0x11}, {40, 110, 110, 0x22, 0, 0, 0x11}, {2, 120, 120, 0x22, 0, 0, 0x33}},     /* RADAR */
    {{64, 110, 110, 0, 0, 0, 0x20}},                                                                         /* ORBIT */
    {{12, 128, 36, 0x13, 64, 64, 0x02}, {12, 128, 36, 0x15, 0, 0, 0x03}, {12, 128, 36, 0x12, 64, 192, 0x02}},   /* CLAY */
    {{4, 120, 120, 0x22, 0, 0, 0x11}, {4, 78, 78, 0x22, 0, 0, 0x22}, {4, 42, 42, 0x22, 0, 0, 0x33}},         /* TUNNEL */
    {{32, 128, 70, 0x14, 0, 0, 0x05}, {32, 128, 70, 0x16, 0, 0, 0x08}, {24, 30, 30, 0x22, 0, 0, 0x11}},      /* CHORUS */
    {{0x98, 128, 22, 0x14, 0, 0, 0x06}, {0x98, 128, 22, 0x15, 60, 0, 0x07}, {0x98, 128, 22, 0x13, 60, 128, 0x05}},   /* HIVE */
    {{8, 128, 28, 0x16, 70, 64, 0x01}, {8, 128, 28, 0x1A, 0, 0, 0x02}, {8, 128, 28, 0x16, 70, 192, 0x03}},   /* FUNC */
    {{32, 90, 90, 0x22, 0, 0, 0x11}, {16, 30, 30, 0x22, 0, 0, 0x11}, {4, 124, 124, 0x22, 0, 0, 0x10}}};     /* MATH */
/* corner k of stroke S in the box: o[0], o[1] (y up) */
static VNI void vis_shape(const uint8_t *S, uint32_t k, int32_t *o)
{
    uint32_t n = S[0] & 127u, f = S[3] >> 4, g = S[3] & 15u, tx = vph((S[6] >> 4) * 8u), t = k * 65536u / n, w;
    int32_t a = S[1] * (256 + vg.lvl / 4) >> 5, b = S[2] * (256 + vg.lvl / 4) >> 5, x, y;
    if (!S[3]) f = VIS_LIS[vg.r.lis][0] * 2u, g = VIS_LIS[vg.r.lis][1] * 2u;
    x = vsin((t * f >> 1) + tx + 0x4000u) * a >> 15;
    y = vsin((t * g >> 1) + vph((S[6] & 15u) * 8u)) * b >> 15;
    if (S[0] & 128u) w = (uint32_t)x, x = y, y = (int32_t)w;
    w = ((uint32_t)S[5] << 8) + tx;
    o[0] = x + (vsin(w + 0x4000u) * S[4] >> 12);
    o[1] = y + (vsin(w) * S[4] >> 12);
}

/* ------------------------------------------------------------------------------------------- the map --- */
/* skeleton sk's carrier at (u, v) (-1024..1024): o[0], o[1] (y up), o[2] its nearness (0..1024) */
static VNI void vis_car(uint32_t sk, int32_t u, int32_t v, int32_t *o)
{
    int32_t x = u * 943 >> 10, y = v * 922 >> 10, z = 1024, k;
    uint32_t q = (uint32_t)(u + 1024) * 32u;           /* (u across a turn) */
    if (sk <= VI_KEY) {                                 /* a sphere turning, tilted 0.37 rad (LIFE's, KEY's slower) */
        uint32_t lon = q + vph(sk ? 37u : 75u), lat = (uint32_t)(v * 15);
        int32_t cl = vsin(lat + 0x4000u), Y = vsin(lat), Z = cl * vsin(lon + 0x4000u) >> 15, R = 810;
        x = (cl * vsin(lon) >> 15) * R >> 15;
        y = ((Y * 30490 - Z * 12000) >> 15) * R >> 15;
        z = (((Y * 12000 + Z * 30490) >> 15) + 32768) >> 6;
    } else if (sk == VI_RADAR || sk == VI_TUNNEL) {     /* polar: round the turn, v up the radius (RADAR's turning), or
                                                         * into the depth (TUNNEL's) */
        k = sk == VI_RADAR ? (v + 1024) * 471 >> 10 : 1048576 / (1024 + ((v + 1024) * 1229 >> 10));
        if (sk == VI_RADAR) q += vg.t >> 1;
        x = vsin(q) * k >> 15;
        y = vsin(q + 0x4000u) * k >> 15;
    } else if (sk == VI_ORBIT) {                        /* swung along the recipe's Lissajous */
        x = (u * 3 >> 2) + (vsin(VIS_LIS[vg.r.lis][0] * (uint32_t)(v + 1024) * 12u + vph(42u)) * 220 >> 15);
        y = (v * 3 >> 2) + (vsin(VIS_LIS[vg.r.lis][1] * (uint32_t)(u + 1024) * 12u + vph(29u)) * 220 >> 15);
    } else if ((1u << sk) & (1u << VI_CHORUS | 1u << VI_LFO | 1u << VI_HIVE | 1u << VI_CLAY)) {   /* the flat bent by a
                                                         * wave: CHORUS's fast, LFO's slow, HIVE's quick, CLAY's in steps */
        if (sk == VI_CLAY) u &= ~255;
        y += vsin((uint32_t)(u * (int32_t)(sk * 9u)) - vph(sk * 40u)) * (120 + (vg.lvl * vg.react >> 9)) >> 15;
    }
    o[0] = x; o[1] = y; o[2] = z;
}
/* a point of the shape onto the screen, stored (the far side of a sphere: a break); the segments counted and capped */
static VNI void vis_put(int32_t u, int32_t v)
{
    int32_t n[3], f[3], k, a = vg.rot, d = vg.def, X, Y;
    int16_t *p = VIS_PT + 2u * vg.np;
    if (vg.np >= VIS_NPT || (vg.pen && vg.ns >= vg.cap))
        return;
    n[0] = u; n[1] = v; n[2] = 1024;
    vis_car(vg.sk, u, v, f);                            /* the carrier, MIX of it */
    for (k = 0; k < 3; k++) n[k] += (f[k] - n[k]) * vg.mix >> 8;
    if (d > 12) {                                       /* the one deformation, with the sound */
        if (vg.dk == VD_FOLD) {                         /* back at +-k */
            k = 1126 - (874 * (d > 256 ? 256 : d) >> 8);
            n[0] = n[0] > k ? 2 * k - n[0] : n[0] < -k ? -2 * k - n[0] : n[0];
        } else if (vg.dk == VD_MIRROR)                  /* the left half the right's (drawn once) */
            n[0] += (VABS(n[0]) - n[0]) * (d > 256 ? 256 : d) >> 8;
        else if (vg.dk == VD_TWIST || vg.dk == VD_POLAR) {   /* the middle turned more, rippled; rings turning */
            int32_t r = VABS(n[0]), q = VABS(n[1]);     /* (the radius, near enough: the larger and half the other) */
            r = r > q ? r + q / 2 : q + r / 2;
            a += vg.dk == VD_TWIST ? (d * (r < 1024 ? 1024 - r : 0) >> 8) * (vsin(vph(62u)) >> 8) >> 5
                                   : d * vsin((uint32_t)(r * 92) - vph(334u)) >> 13;
        }
    }
    k = vsin((uint32_t)a + 0x4000u);                    /* (the camera's turn and zoom) */
    a = vsin((uint32_t)a);
    X = vg.cx + (((n[0] * k - n[1] * a) >> 15) * vg.R >> 10);
    Y = vg.cy - (((n[0] * a + n[1] * k) >> 15) * vg.R >> 10);
    if (d > 12 && vg.dk == VD_MELT)                     /* each column of 3 drops its own */
        Y += ((int32_t)((uint32_t)(X >> 2) * 2654435761u >> 27) + 5) * d * 154 >> 16;
    if (d > 12 && vg.dk == VD_SCAN && vg.snr > 12) {    /* a snare: bands of 16 rows shifted aside */
        uint32_t b = (uint32_t)(Y + 320) >> 4;
        X += (vg.snr * d >> 11) * (int32_t)(((b * 7u) ^ (trk[0].seq_idx * 11u)) & 3u) * (b & 1u ? 1 : -1);
    }
    if (n[2] < 460) {                                   /* (the far side: not drawn) */
        if (vg.pen) { p[0] = VIS_UP; p[1] = (int16_t)vg.col; vg.np++; vg.pen = 0; }
        return;
    }
    p[0] = (int16_t)clamp(X, -120, 360); p[1] = (int16_t)clamp(Y, -120, 360);
    vg.np++;
    vg.ns += vg.pen;
    vg.pen = 1;
}
/* the frame: the song's set, the music, the cuts, the camera; the content's shape mapped into points */
static VNI void vis_frame(uint32_t fresh, uint32_t cap)
{
    uint32_t s[4], i, e, k, key = 2166136261u, now = fm1_ms, st, hit, m = vis_mask();
    int32_t u, v;
    const vis_recipe_t *R = &vg.r;
    vis_song(s);
    for (i = 0; i < 4u; i++) key = (key ^ s[i]) * 16777619u;
    key ^= m;
    if (key != vg.key || fresh) {                       /* a new song, recipe, or the page entered: from clean */
        if (key != vg.key) {                            /* (the knobs: the song's own recipe's) */
            if (bt_hw_seg())                            /* (the recipe: in the Bluetooth segment) */
                vis_recipe(&vg.r, s[0], s[1], s[2], m, s[3]);
            vis_k[0] = R->mix; vis_k[1] = 50; vis_k[3] = R->react;
        }
        vg.key = key;
        memset(&vg.t, 0, (uint32_t)((uint8_t *)(&vg + 1) - (uint8_t *)&vg.t));   /* (the camera, the cuts, the hits) */
        vg.t = now;
        vg.sec = vg.rr = 255;
    }
    e = now - vg.t;
    e = e > 100u ? 100u : e;
    vg.t = now;
    vg.en -= (int32_t)(e * 3u / 8u);                    /* the sound: held, let go in ~0.7 s */
    vis_levels();
    for (i = 0, k = 0; i < NTRK; i++)                   /* the loudest part */
        k = (uint32_t)vg.lv[i] > k ? (uint32_t)vg.lv[i] : k;
    st = vis_tsig();                                    /* the timbre */
    vg.lvl = (int32_t)k;
    vis_hits();
    hit = vg.hacc & 3u;
    vg.hacc = (uint8_t)(vg.hacc & ~3u);
    if ((int32_t)k > vg.plvl + 40) hit |= 1u;           /* (a kick: DRUM-W's BD, else the level jumping) */
    vg.plvl = (int32_t)k;
    vg.kick = hit & 1u ? 256 : vg.kick * 205 >> 8;
    vg.snr = hit & 2u ? 256 : vg.snr * 200 >> 8;
    k = k * 2u > (uint32_t)vg.kick ? k * 2u : (uint32_t)vg.kick;
    if ((int32_t)k > vg.en) vg.en = k > 256u ? 256 : (int32_t)k;
    if (vg.en < 0) vg.en = 0;
    vg.chord = (uint8_t)vis_chord();
    if (st != vg.tim) hit |= 2u;                        /* (the timbre changing: as a snare) */
    i = vis_sec();
    if (scrn.st == SCR_VIS)                             /* (the saver, as JIANT FM's: another scene every 30 s) */
        i = (i + now / 30000u) & 3u;
    if (i != vg.sec) { vg.sec = (uint8_t)i; vg.pos = 0; hit = 4u; }   /* (a section: reshuffled) */
    if (hit && (hit > 3u || now - vg.ct >= 150u)) {     /* the cuts: a kick the next, a snare a jump */
        vg.pos = (uint8_t)(vg.pos + (hit & 1u) + (hit & 2u ? 3u : 0u));
        vg.ct = now; vg.zp = vg.en; vg.tim = st;
    }
    vg.ri = (uint8_t)((vg.pos * (2u * vg.sec + 1u) + vg.sec) & 7u);
    if (vg.ri != vg.rr) {                               /* a cut: that recipe of the set */
        vg.rr = vg.ri;
        if (bt_hw_seg())
            vis_recipe(&vg.r, s[0], s[1], s[2], m, s[3] + vg.ri);
    }
    vg.sk = R->skel[vg.sec];
    vg.mix = vis_k[0] * 655 >> 8;                       /* (0..100 to 0..256) */
    vg.react = vis_k[3] * 655 >> 8;
    vg.pal = (uint8_t)(((vis_k[2] ? vis_k[2] : R->pal) + 4u) % 5u);
    i = R->def[vg.sec][0];                              /* the deformation (the first: one at a time), gliding to its
                                                         * amount x DEFORM x the sound (at rest none) */
    k = i < VD_N ? (uint32_t)(R->amt[vg.sec][0] * vis_k[1] * 13 >> 8) * (uint32_t)vg.en >> 8 : 0u;
    if (i != vg.dk) vg.dk = (uint8_t)i, vg.def = 0;
    vg.def += ((int32_t)k - vg.def) >> 2;
    vg.zp = vg.zp * 215 >> 8;
    /* the camera: a zoom breathing and pulsing (a cut, a kick, the level), a turn drifting (a kick throws it on), a drift */
    vg.sc = 1024 + (vsin(vph(11u)) * 154 >> 15) + (vg.react * (vg.kick * 80 + vg.lvl * 60 + vg.zp * 80) >> 16);
    vg.rot += (int32_t)((40u + (uint32_t)(vg.kick * vg.react >> 7)) * e >> 4) * (vg.ri & 1u ? 1 : -1);
    vg.R = 104 * vg.sc >> 10;
    vg.cx = 120 + (vsin(vph(13u)) * 12 >> 15);
    vg.cy = 120 + (vsin(vph(17u) + 0x4000u) * 8 >> 15);
    vg.np = vg.ns = 0;
    vg.cap = (uint16_t)cap;
    for (i = 0; i < 3u; i++) {                          /* the content's strokes, each its colour */
        const uint8_t *S = VIS_SHP[R->content % VI_N][i];
        uint32_t n = S[0] & 127u, b = n < 12u ? 32u / n : 1u, j;
        if (!n || vg.np + 1u >= VIS_NPT)
            continue;
        vg.col = vis_col(160u + ((i * 61u + vg.chord * 53u + vg.ri * 29u) % 96u));
        VIS_PT[2u * vg.np] = VIS_UP; VIS_PT[2u * vg.np + 1u] = (int16_t)vg.col;
        vg.np++; vg.pen = 0;
        for (k = 0, u = v = 0; k <= n; k++) {          /* (a polygon's edge in pieces: bent by the carrier as a curve) */
            int32_t o[2];
            vis_shape(S, k, o);
            for (j = k ? 1u : b; j <= b; j++)
                vis_put(u + (o[0] - u) * (int32_t)j / (int32_t)b, v + (o[1] - v) * (int32_t)j / (int32_t)b);
            u = o[0]; v = o[1];
        }
    }
}
/* HOME > VIS and MENU > SAVER: the whole screen in four bands (ui_draw.c), the frame's lines; a knob turned (OCT+):
 * its card a moment */
static const char *vis_card(uint32_t c, char *b)        /* HOME > VIS's cards' values (4: the recipe byte) */
{
    if (c == 2u)
        return str_nth(VIS_PN, vis_k[2] % VP_N);
    fmt_int(b, c > 3u ? vis_id[4] : vis_k[c & 3u]);
    return b;
}
static VNI void vis_saver(void)
{
    uint32_t c, now = fm1_ms, on = scrn.st != SCR_VIS && now - (vis_ov & ~15u) < 1300u, i, L = load_avg_q8 >> 8;
    uint32_t fresh = now - vg.seen > 250u, cap = L < 50u ? VIS_SEG : L < 70u ? (70u - L) * 10u : 0u;
    char b[8];
    vg.seen = now;
    if (!fresh && ((ui.frame & (L < 50u ? 1u : 3u)) || load_rl || !cap))
        return;                                         /* (every 2nd UI frame; the audio's load past 50 %: every 4th and
                                                         * fewer segments, past 70 % or the guard on: none) */
    CV_BANDS(240, 240, 0) {
        const int16_t *p = VIS_PT;
        uint16_t col = 0;
        if (!cv_band)
            vis_frame(fresh, cap ? cap : VIS_SEG / 4u);
        for (i = 0, c = 0; i < vg.np; i++, p += 2) {    /* (a break: the colour; else a line from the point before) */
            if (p[0] == VIS_UP)
                col = (uint16_t)p[1], c = 0;
            else if (c++) {                             /* (2 px: a second line beside it) */
                int32_t h = VABS(p[0] - p[-2]) > VABS(p[1] - p[-1]);
                cv_line(p[-2], p[-1], p[0], p[1], col);
                cv_line(p[-2] + !h, p[-1] + h, p[0] + !h, p[1] + h, col);
            }
        }
        if (on) {
            cv_text_in(0, 10, 240, &AF_S, str_nth(VIS_KN, vis_ov & 7u), T_THEME, 0);
            cv_text_in(0, 28, 240, &AF_L, vis_card(vis_ov & 7u, b), T_TEXT, 0);
        }
        cv_blit(0, 0);
    }
}
/* (ui_draw.c) the visual on the screen: MENU > SAVER, or HOME > VIS with nothing over it; back from it: the page whole */
static VNI int vis_screen(uint32_t counting)
{
    if (scrn.st == SCR_VIS || (home_vis() && !ui.menu && !dlg_on() && !name_on() && !ly_drawn() && !counting && !ui.uboot &&
                               !ui.segbox)) {
        vis_saver();
        vis_ov |= 8u;                                   /* (a static of its own here would move the merged globals the
                                                         * timer ISR reads: its code another) */
        return 1;
    }
    if (vis_ov & 8u) {
        vis_ov &= ~8u;
        ui.force = 1;
        ui.head_sig = ui.graph_sig = ui.foot_sig = ~0u;
    }
    return 0;
}
static VNI void vis_cols(char *val)   /* (ui_draw.c draw_columns) HOME > VIS's four cards */
{
    uint32_t c;
    for (c = 0; c < 4u; c++)
        draw_column(c, str_nth(VIS_KN, c), vis_card(c, val), c == 2u ? "" : "%", VAL(c), c == 2u ? vis_k[2] * 200 : vis_k[c] * 10,
                    c == 2u ? ICON_CUTOFF : ICON_MOD);
}
static VNI void vis_knob(uint32_t k, int32_t s)   /* (ui_input.c) HOME > VIS: a knob turned */
{
    vis_k[k & 3u] = (uint8_t)clamp(vis_k[k & 3u] + ((k & 3u) == 2u ? s : accel(EN_K1 + k, s, 100)), 0, (k & 3u) == 2u ? VP_N - 1 : 100);
    vis_ov = (fm1_ms & ~15u) | (k & 3u);                /* (its card over the full-screen visual a moment) */
}
