/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (visgen, the owner: route A of the approved mock-up, docs/visgen-mock.html; docs/VISGEN.md) HOME > VIS and MENU >
 * SAVER: the song's visual made of BYTE's own pictures, the whole screen (HOME > VIS: a knob turned, OCT+, its card a
 * moment over it). The song's SET of 8 recipes (vis_recipe.c: the recipe byte and the 7 after it): a kick cuts to the
 * next, a snare (or a timbre change: a part's engine, preset, a big filter move) jumps, a section reshuffles
 * (docs/VISGEN.md "Cuts"), as an editor cutting between 8 generations. The recipe drawn says which: the content (drawn
 * through the skeleton's carrier, MIX of it), the skeleton itself faint, maybe a layer over; each ingredient is its
 * page's own drawing run with gfx.c's point map on (cv_vmap): its lines, dots and boxes come here (vis_line, vis_rect)
 * and are mapped (the source box to -1..1, the carrier, the deformations TWIST FOLD POLAR MELT, a camera turning,
 * zooming and drifting, MIRROR drawn again) onto the screen, as wires in the palette or as heat in 6 px cells with a
 * memory (the display cache's last free hashes, gfx.c dc_hash: no new RAM). Depth: the grain (scanlines of dim dots, as
 * dense as REACT and the level) behind, then the heat, then the wires in two passes, the dark and faint first, the
 * bright last. It reacts: a hit pulses the zoom and the turn, the bass and the level heat and swell it, a snare throws
 * bands of 3 rows aside (SCAN: further), a chord change turns the colours. Drawn in 4 bands: what moves is worked out
 * once a frame, in the first (vis_frame); the heat filled there too. Reads the UI's own snapshots (track_env, dw_live,
 * hm_*, live.cur, bar_n): nothing of the audio path but vis_fol (mod.c: every part's level followed while VIS looks).
 * Included by ui_input.c (after ui_dx.c: DRUM-W's radar) */
#define VABS(v) ((v) < 0 ? -(v) : (v))
#define VIS_HEAT ((uint8_t *)(dc_hash + DC_USE - 800u))   /* 1600 B: 40 x 40 cells */
static struct {
    vis_recipe_t r;                                /* the set's recipe drawn (worked out on a cut: the BT segment's) */
    uint32_t key;                                  /* the set's inputs */
    uint32_t t, last, ct, tim;                     /* (from here on cleared with the set) now (ms); the last cut; the
                                                    * timbre's signature */
    int32_t kick, snr, bass, lvl, plvl, sc, rot, zp, mix, react, scan, hg, a1;   /* 0..256 (sc Q10; rot a turn 65536) */
    int32_t def[VD_N];                             /* the deformations now (gliding to the recipe's), 256 = DEFORM 50 */
    int32_t sx, sy, sw, sh, cx, cy, R;             /* the layer's source box; the camera; its scale */
    uint8_t ri, rr, pos, sec, blend, chord, root, q, hacc, hst, dwp, mir, pal, pass;   /* (hacc: the frame's hits;
                                                    * hst: Studio's) */
    uint8_t ly[3];                                 /* the frame's layers: skeleton, over, content */
    uint8_t cont, al, heat;                        /* the layer drawn */
} vg __attribute__((section(".pool")));
/* the five palettes' ramps (docs/VISGEN.md VP_*: THERM HUD TRACKS ICE AMBER), four stops each */
static const uint16_t VIS_RAMP[5][4] = {
    {RGB(57, 40, 172), RGB(255, 64, 74), RGB(255, 190, 90), RGB(255, 255, 255)},
    {RGB(41, 60, 255), RGB(58, 208, 255), RGB(238, 242, 90), RGB(255, 255, 255)},
    {RGB(164, 93, 255), RGB(255, 56, 180), RGB(255, 137, 41), RGB(255, 210, 57)},
    {RGB(10, 20, 90), RGB(41, 60, 255), RGB(58, 208, 255), RGB(255, 255, 255)},
    {RGB(60, 15, 0), RGB(255, 120, 20), RGB(255, 200, 60), RGB(255, 245, 190)}};
static uint16_t vis_col(uint32_t v)                     /* 0..255 along the palette, the low end into black */
{
    const uint16_t *p = VIS_RAMP[vg.pal];
    uint32_t s = v > 255u ? 255u : v, k = s * 3u >> 8;
    uint16_t c = ux_mix(p[k], p[k + 1u], (int32_t)((s * 3u & 255u) * 100u >> 8));
    return s < 40u ? ux_mix(0, c, (int32_t)s * 5 / 2) : c;
}
#define vsin bg_sin                                     /* (a turn 65536, Q15) */
static uint32_t vph(uint32_t k) { return vg.t * k >> 4; }   /* the time's phase at k / 167 rad a second */

/* ------------------------------------------------------------------------------------------- the song --- */
/* an engine's VE_* bit (engines.c's numbers: LOFI MATH, DRUM DRUM-W, WAVE, HIVE, FUNC) */
static const uint8_t VIS_EB[NENGINES] = {[ENGI_LOFI] = 2, [ENGI_DRUM] = 16, [ENGI_WAVE] = 1, [ENGI_HIVE] = 32, [ENGI_FUNC] = 4};
static uint32_t vis_bits(uint32_t i)                    /* VE_* of part i */
{
    const track_t *t = &trk[i];
    uint32_t b = VIS_EB[trk_eng(t)];
    if (b == 4u && t->p[P_E7] == (int)FUNC_BANK) b = 12u;
    return b | (clay[i][CL_ON] ? 64u : 0u) | (t->p[P_DLY] > 0 || t->p[P_CHOR] > 0 ? 128u : 0u);
}
static uint32_t vis_mask(void)                          /* VE_* of the parts playing */
{
    uint32_t i, m = 0;
    for (i = 0; i < trk_live && i < NTRK; i++) m |= vis_bits(i);
    return m;
}
/* the song's seed, genre, energy, recipe byte (o[4]); a song GEN did not make: its tempo's and first part's, none, 50 */
static void vis_song(uint32_t *o)
{
    uint32_t h = ((uint32_t)song.g[G_BPM] * 131u + (uint32_t)trk[0].p[P_ROOT] * 17u + (uint32_t)trk[0].p[P_SLEN]) * 2654435761u;
    int g = vis_id[2] != 0;
    o[0] = g ? (uint32_t)vis_id[0] | (uint32_t)vis_id[1] << 8 : h >> 11;
    o[1] = g ? vis_id[2] - 1u : VIS_GENRES;
    o[2] = g ? vis_id[3] : 50u;
    o[3] = vis_id[4];
}
static void vis_gen_set(uint32_t seed, uint32_t genre, uint32_t energy)   /* (gen.c: a piece made; its recipe byte 0) */
{
    vis_id[0] = (uint8_t)seed; vis_id[1] = (uint8_t)(seed >> 8);
    vis_id[2] = (uint8_t)((genre < VIS_GENRES ? genre : VIS_GENRES) + 1u);
    vis_id[3] = (uint8_t)(energy > 100u ? 100u : energy);
    vis_id[4] = 0;
}
static void vis_oct(int up)                             /* OCT+ another recipe, OCT- the song's own (0) */
{
    vis_id[4] = up ? (uint8_t)(vis_id[4] + 1u) : 0u;
    vis_ov = (fm1_ms & ~15u) | 4u;                      /* (RECIPE n over the visual a moment) */
}
static const track_t *vis_trk(uint32_t ing)             /* the part an ingredient draws (its engine's first) */
{
    uint32_t i;
    for (i = 0; i < trk_live && i < NTRK; i++)
        if (ing < VI_RADAR ? !drum_track(&trk[i]) : (vis_bits(i) >> VIS_ENG_ING[ing]) & 1u)
            return &trk[i];
    return TSEL;
}
/* the chord now (SCL > HARMONY's playing, else the key's): its root (0..11), quality (0 MAJ 1 MIN 2 DIM 3 AUG 4 SUS);
 * its index */
static uint32_t vis_chord(void)
{
    const track_t *t = vis_trk(VI_KEY);
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
static uint32_t vis_sec(void) { return live.cur >= 0 ? (uint32_t)live.cur & 3u : song.playing ? (bar_n >> 3) & 3u : 0u; }
static void vis_hits(void)                              /* DRUM-W's lanes struck since the last look: vg.hacc */
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
/* JIANT Studio's state (docs/VISGEN.md): VIS_SONG's fields, VIS_TICK's, playing (21); the hits since the last call */
static void vis_state(int32_t *o)
{
    uint32_t s[4], i;
    vis_song(s);
    o[0] = (int32_t)s[0]; o[1] = (int32_t)s[3]; o[2] = (int32_t)s[1]; o[3] = (int32_t)s[2]; o[4] = (int32_t)vis_mask();
    o[5] = (int32_t)vis_sec(); o[6] = vis_k[2]; o[7] = vis_k[0]; o[8] = vis_k[1]; o[9] = vis_k[3];
    for (i = 0; i < 6u; i++) o[10u + i] = i < NTRK ? clamp(track_env[i] >> 8, 0, 127) : 0;
    vis_hits();
    o[16] = vg.hst; vg.hst = 0;
    (void)vis_chord();
    o[17] = vg.root; o[18] = vg.q; o[19] = (int32_t)(trk[0].seq_idx & 63u);
    o[20] = song.playing || chain_busy() || seq_counting();   /* (as web_gen_info's) */
    vis_fol = 255;                                      /* (the levels followed a while: mod.c) */
}

/* ------------------------------------------------------------------------------------------- the map --- */
/* skeleton sk's carrier at (u, v) (-1024..1024): o[0], o[1] (y up), o[2] its nearness (0..1024) */
static void vis_car(uint32_t sk, int32_t u, int32_t v, int32_t *o)
{
    int32_t x = u * 943 >> 10, y = v * 922 >> 10, z = 1024, k;
    uint32_t q = (uint32_t)(u + 1024) * 32u;           /* (u across a turn) */
    if (sk <= VI_KEY) {                                 /* a sphere turning, tilted 0.37 rad (LIFE's, KEY's slower) */
        uint32_t lon = q + vph(sk ? 37u : 75u), lat = (uint32_t)(v * 15);
        int32_t cl = vsin(lat + 0x4000u), Y = vsin(lat), Z = cl * vsin(lon + 0x4000u) >> 15, R = 810 * vg.sc >> 10;
        x = (cl * vsin(lon) >> 15) * R >> 15;
        y = ((Y * 30490 - Z * 12000) >> 15) * R >> 15;
        z = (((Y * 12000 + Z * 30490) >> 15) + 32768) >> 6;
    } else if (sk == VI_RADAR || sk == VI_TUNNEL) {     /* polar: round the turn, v up the radius (RADAR's turning), or
                                                         * into the depth (TUNNEL's, bending) */
        k = sk == VI_RADAR ? (v + 1024) * 471 >> 10 : 1048576 / (1024 + ((v + 1024) * 1229 >> 10));
        if (sk == VI_RADAR) q += vg.t >> 1;
        x = vsin(q) * k >> 15;
        y = vsin(q + 0x4000u) * k >> 15;
        if (sk == VI_TUNNEL) {
            z = k; k = 1024 - k;
            x += vsin(vph(50u)) * k >> 17;
            y += vsin(vph(38u) + 0x4000u) * k >> 18;
        }
        x = x * vg.sc >> 10; y = y * vg.sc >> 10;
    } else if (sk == VI_ORBIT) {                        /* along a Lissajous, v its width */
        k = 800 * vg.sc >> 10;
        x = vsin(VIS_LIS[vg.r.lis][0] * q + vph(42u) + (uint32_t)(vg.bass * vg.react >> 4)) * k >> 15;
        y = (vsin(VIS_LIS[vg.r.lis][1] * q) * k >> 15) + (v * 164 >> 10);
    } else if (sk == VI_MATH) {                         /* .. cells */
        x = (x + 4096) / 146 * 146 - 4088; y = (y + 4096) / 146 * 146 - 4088;
    } else if ((1u << sk) & (1u << VI_CHORUS | 1u << VI_LFO | 1u << VI_HIVE | 1u << VI_CLAY)) {   /* the flat bent by a
                                                         * wave: CHORUS's fast, LFO's slow, HIVE's quick, CLAY's in steps */
        if (sk == VI_CLAY) u &= ~255;
        y += vsin((uint32_t)(u * (int32_t)(sk * 9u)) - vph(sk * 40u)) * (120 + (vg.lvl * vg.react >> 6)) >> 15;
    }
    o[0] = x; o[1] = y; o[2] = z;
}
static int32_t vis_fold(int32_t v, int32_t f)          /* v folded back at +-f (three times at most) */
{
    uint32_t k;
    for (k = 0; k < 3u; k++) v = v > f ? 2 * f - v : v < -f ? -2 * f - v : v;
    return v;
}
/* a point of the layer's source onto the screen: X, Y; its nearness (0..1024) */
static int32_t vis_pt(int32_t x, int32_t y, int32_t *X, int32_t *Y)
{
    int32_t u = (x - vg.sx) * 2048 / vg.sw - 1024, v = 1024 - (y - vg.sy) * 2048 / vg.sh, f[3], n[3], m, r, a, c, s;
    int32_t fo = vg.def[VD_FOLD];
    vis_car(VI_NONE, u, v, n);
    if (vg.cont) {                                      /* the content: on the skeleton, MIX of it */
        vis_car(vg.ly[0], u, v, f);
        for (m = 0; m < 3; m++) n[m] += (f[m] - n[m]) * vg.mix >> 8;
    }
    if (fo > 25) {                                      /* FOLD: back at +-f (and up and down, past half) */
        m = 1126 - (874 * (fo > 256 ? 256 : fo) >> 8) + (vsin(vph(67u)) * 82 >> 15) + (vg.kick * vg.react >> 10);
        n[0] = vis_fold(n[0], m);
        if (fo > 140) n[1] = vis_fold(n[1], m);
    }
    /* the camera's turn, TWIST (the nearer the middle the more, rippled) and POLAR (rings turning) */
    r = (int32_t)isqrt32((uint32_t)(n[0] * n[0] + n[1] * n[1]));
    a = (vg.def[VD_TWIST] * (1024 - r) >> 10) * ((vsin(vph(62u)) * 30 >> 15) + (vg.kick * vg.react >> 11)) >> 3;
    a += vg.rot + (vg.def[VD_POLAR] * vsin((uint32_t)(r * 92) - vph(334u)) >> 13);
    c = vsin((uint32_t)a + 0x4000u); s = vsin((uint32_t)a);
    *X = vg.cx + (((n[0] * c - n[1] * s) >> 15) * vg.R >> 10);
    *Y = vg.cy - (((n[0] * s + n[1] * c) >> 15) * vg.R >> 10);
    if (vg.def[VD_MELT] > 12)                           /* MELT: each column of 3 drops its own, with the bass */
        *Y += ((int32_t)((uint32_t)(*X >> 2) * 2654435761u >> 27) + 5) * vg.def[VD_MELT] * (154 + (vg.bass * vg.react >> 8)) >> 16;
    if (vg.scan > 12) {                                 /* a snare (SCAN: further): rows of 3 shifted, some far */
        uint32_t b = (uint32_t)*Y / 3u;
        *X += ((((b * 7u) ^ (trk[0].seq_idx * 11u)) & 31u) < 4u ? vg.scan * 30 >> 8 : vg.scan >> 5) * (b & 1u ? 1 : -1);
    }
    return n[2];
}
/* one mapped piece (x0, y0) .. (x1, y1) in colour c: as heat (band 0 only: its end's cell) or a wire, mirrored */
static uint32_t vis_lum(uint32_t c) { return ((c >> 11) * 616u + ((c >> 5) & 63u) * 600u + (c & 31u) * 232u) >> 8; }   /* 0..255 */
static void vis_put(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)
{
    uint32_t m;
    for (m = 0; m <= vg.mir; m++) {
        if (m == 1u) { x0 = 239 - x0; x1 = 239 - x1; }   /* (MIRROR: across, then up and down too) */
        if (m == 2u) { y0 = 239 - y0; y1 = 239 - y1; }
        if (!vg.heat)
            cv_line(x0, y0, x1, y1, ux_mix(0, c, a * 100 >> 8));
        else if ((uint32_t)x1 < 240u && (uint32_t)y1 < 240u) {   /* heat: its cell warmer */
            uint8_t *h = &VIS_HEAT[(y1 / 6) * 40 + x1 / 6];
            int32_t v = *h + ((int32_t)vis_lum(c) * a * vg.hg >> 19);
            *h = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}
static void vis_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    const cv_vmap_t *k = cv_vmap;
    int32_t n = (VABS(x1 - x0) > VABS(y1 - y0) ? VABS(x1 - x0) : VABS(y1 - y0)) / 6 + 1, i, px, py, qx, qy, z, oy = cv_oy;
    uint32_t b = 64u + (vis_lum(c) + vg.chord * 53u) % 192u;   /* (its brightness along the palette, the chord turning it) */
    if (c == ux.bg || c == ux.surf || (vg.heat ? cv_band != 0 : (b * vg.al >= 128u * 256u) != vg.pass))
        return;                                         /* (a wire: the dark and faint pass first, the bright last) */
    if (!vg.heat)
        c = vis_col(b);
    if (n > 40) n = 40;
    cv_vmap = 0;
    cv_oy = 0;
    vis_pt(x0, y0, &px, &py);
    for (i = 1; i <= n; i++) {
        z = vis_pt(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, &qx, &qy);
        vis_put(px, py, qx, qy, c, z < 460 ? vg.al * 3 / 10 : vg.al);
        px = qx; py = qy;
    }
    cv_oy = oy;
    cv_vmap = k;
}
static void vis_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w - 1, y1 = y + h - 1;
    if (h <= 6)                                         /* a small box, a rule: a line across its middle */
        vis_line(x, y + h / 2, x1, y + h / 2, c);
    else if (w <= 6)
        vis_line(x + w / 2, y, x + w / 2, y1, c);
    else if (h >= 12) {                                 /* a box: its outline (a tag's, smaller: left out) */
        vis_line(x, y, x1, y, c);
        vis_line(x1, y, x1, y1, c);
        vis_line(x1, y1, x, y1, c);
        vis_line(x, y1, x, y, c);
    }
}
static const cv_vmap_t VIS_MAP = {vis_line, vis_rect};

/* ------------------------------------------------------------------------------------------- the layers --- */
/* each ingredient's source box (x, y, w, h: where its picture lies in its page's panel) */
static const uint8_t VIS_BOX[VI_N][4] = {{12, 0, 150, 140}, {4, 4, 116, 116}, {4, 10, 232, 100}, {10, 0, 220, 100},
                                         {12, 0, 150, 140}, {14, 4, 112, 112}, {4, 22, 118, 118}, {0, 0, 240, 140},
                                         {0, 0, 240, 140}, {0, 0, 240, 140}, {0, 0, 200, 140}, {0, 14, 240, 126},
                                         {60, 10, 120, 120}};
static void vis_layer(uint32_t i, uint32_t heat)       /* layer i of the frame (0 the skeleton, 1 over, 2 the content),
                                                         * its page's own picture through the map: its heat, or its wire
                                                         * (a heat layer's faint over its cells) */
{
    uint32_t g = vg.ly[i], hl = i == 2u ? vg.blend != VB_SUM : vg.blend == VB_THERM;
    const uint8_t *B = VIS_BOX[g % VI_N];
    const track_t *t = vis_trk(g);
    int32_t oy = cv_oy, al = i == 2u ? 255 : i ? 180 : vg.a1;
    if (g >= VI_N || (heat && !hl))
        return;
    vg.cont = i == 2u; vg.heat = (uint8_t)heat; vg.al = (uint8_t)(!heat && hl ? al * 115 >> 8 : al);
    vg.sx = B[0]; vg.sy = B[1]; vg.sw = B[2]; vg.sh = B[3];
    cv_oy = -30000;                                     /* (what draws straight into the canvas: nowhere) */
    cv_vmap = &VIS_MAP;
    switch (g) {
    case VI_LIFE: case VI_SPLASH: graph_eco(1); break;  /* (SPLASH: the power-on signature's table, 0.6 KB, left out of
                                                         * the image: LIFE's beings for it) */
    case VI_KEY: if (bt_hw_seg()) graph_key(t, 0); break;
    case VI_HARMONY: if (bt_hw_seg()) graph_harm(t); break;
    case VI_LFO: graph_lfo(t, T_THEME); break;
    case VI_RADAR: dw_play_panel(); break;
    case VI_ORBIT: case VI_FUNC: case VI_HIVE: (void)vx_draw(t, 0, 0, 240, 140, -1); break;   /* (hud_wave, hud_func, vx_hive) */
    case VI_CLAY: graph_clay_lz(t); break;
    case VI_TUNNEL: case VI_CHORUS: (g == VI_TUNNEL ? fxg_delay : fxg_chorus)(0, 0, 240, 140); break;
    case VI_MATH: (void)vx_draw(t, 60, 10, 120, 120, -1); break;   /* (small: the being, ub_draw's) */
    }
    cv_vmap = 0;
    cv_oy = oy;
}
/* the frame (the first band): the song's set, the music, the cuts, the camera, the layers; the heat cooled and filled */
static void vis_frame(void)
{
    uint32_t s[4], i, e, k, key = 2166136261u, now = fm1_ms, st = 0, hit;
    const vis_recipe_t *R = &vg.r;
    vis_song(s);
    for (i = 0; i < 4u; i++) key = (key ^ s[i]) * 16777619u;
    key ^= vis_mask();
    if (key != vg.key || now - vg.last > 400u) {        /* a new song, recipe, or the page entered: from clean */
        if (key != vg.key) {                            /* (the knobs: the song's own recipe's) */
            if (bt_hw_seg())                            /* (the recipe: in the Bluetooth segment) */
                vis_recipe(&vg.r, s[0], s[1], s[2], vis_mask(), s[3]);
            vis_k[0] = R->mix; vis_k[1] = 50; vis_k[3] = R->react;
        }
        vg.key = key;
        memset(VIS_HEAT, 0, 1600u);
        memset(&vg.t, 0, (uint32_t)((uint8_t *)(&vg + 1) - (uint8_t *)&vg.t));   /* (the camera, the cuts, the hits) */
        vg.sec = vg.rr = 255;
    }
    vg.last = vg.t = now;
    vis_fol = 255;                                      /* (every part's level followed while shown: mod.c) */
    for (i = 0, e = 0, k = 0; i < trk_live && i < NTRK; i++) {   /* the levels: the loudest, the mean; the timbre */
        int32_t l = clamp(track_env[i] >> 7, 0, 256);
        e = (uint32_t)l > e ? (uint32_t)l : e;
        k += (uint32_t)l;
        st = st * 31u + trk[i].eng_req * 7u + trk[i].preset + ((uint32_t)trk[i].p[P_FCUT] >> 5) * 131u;
    }
    vg.lvl = (int32_t)e; vg.bass = (int32_t)(k / (trk_live ? trk_live : 1u));
    vis_hits();
    hit = vg.hacc & 3u;
    vg.hacc = (uint8_t)(vg.hacc & ~3u);
    if ((int32_t)e > vg.plvl + 40) hit |= 1u;           /* (a kick: DRUM-W's BD, else the level jumping) */
    vg.plvl = (int32_t)e;
    vg.kick = hit & 1u ? 256 : vg.kick * 205 >> 8;
    vg.snr = hit & 2u ? 256 : vg.snr * 200 >> 8;
    vg.chord = (uint8_t)vis_chord();
    if (st != vg.tim) hit |= 2u;                        /* (the timbre changing: as a snare) */
    i = vis_sec();
    if (scrn.st == SCR_VIS)                             /* (the saver, as JIANT FM's: another scene every 30 s) */
        i = (i + now / 30000u) & 3u;
    if (i != vg.sec) { vg.sec = (uint8_t)i; vg.pos = 0; hit = 4u; }   /* (a section: reshuffled) */
    if (hit && (hit > 3u || now - vg.ct >= 150u)) {     /* the cuts: a kick the next, a snare a jump */
        vg.pos = (uint8_t)(vg.pos + (hit & 1u) + (hit & 2u ? 3u : 0u));
        vg.ct = now; vg.zp = 256; vg.tim = st;
    }
    vg.ri = (uint8_t)((vg.pos * (2u * vg.sec + 1u) + vg.sec) & 7u);
    if (vg.ri != vg.rr) {                               /* a cut: that recipe of the set */
        vg.rr = vg.ri;
        if (bt_hw_seg())
            vis_recipe(&vg.r, s[0], s[1], s[2], vis_mask(), s[3] + vg.ri);
    }
    vg.ly[0] = R->skel[vg.sec]; vg.ly[1] = R->over[vg.sec]; vg.ly[2] = R->content; vg.blend = R->blend;
    vg.mix = vis_k[0] * 655 >> 8;                       /* (0..100 to 0..256) */
    vg.react = vis_k[3] * 655 >> 8;
    vg.pal = (uint8_t)(((vis_k[2] ? vis_k[2] : R->pal) + 4u) % 5u);
    vg.a1 = 51 + (154 * (256 - vg.mix) >> 8);
    for (i = 0; i < VD_N; i++) {                        /* the deformations: gliding to the recipe's */
        int32_t b = 0;
        for (k = 0; k < 2u; k++)
            if (R->def[vg.sec][k] == i) b = R->amt[vg.sec][k] * vis_k[1] * 13 >> 8;   /* (100 at DEFORM 50: 256) */
        vg.def[i] += (b - vg.def[i]) >> 2;
    }
    vg.scan = vg.snr * (vg.react + vg.def[VD_SCAN]) >> 9;
    vg.mir = (uint8_t)(vg.def[VD_MIRROR] > 154 ? 2u : vg.def[VD_MIRROR] > 25 ? 1u : 0u);
    vg.zp = vg.zp * 215 >> 8;
    /* the camera: a zoom breathing and pulsing (a cut, a kick, the bass), a turn drifting (a kick throws it on), a drift */
    vg.sc = 1024 + (vsin(vph(11u)) * 205 >> 15) + (vg.react * (vg.kick * 160 + vg.bass * 150 + vg.zp * 200) >> 16);
    vg.rot += (40 + (vg.kick * vg.react >> 7)) * (vg.ri & 1u ? 1 : -1);
    vg.R = 114 * vg.sc >> 10;
    vg.cx = 120 + (vsin(vph(13u)) * 12 >> 15);
    vg.cy = 120 + (vsin(vph(17u) + 0x4000u) * 8 >> 15);
    vg.hg = 96 + (vg.bass * vg.react >> 9);             /* the heat: cooled, then the heat layers into it */
    for (i = 0; i < 1600u; i++) VIS_HEAT[i] = (uint8_t)(VIS_HEAT[i] * (vg.blend == VB_THERM ? 212u : 200u) >> 8);
    for (i = 0; i < 3u; i++)
        vis_layer(i, 1);
}
/* HOME > VIS and MENU > SAVER: the whole screen in four bands (ui_draw.c): the grain, the heat cells, the wires (dark,
 * then bright); a knob turned (OCT+): its card a moment */
static const char *vis_card(uint32_t c, char *b)        /* HOME > VIS's cards' values (4: the recipe byte) */
{
    if (c == 2u)
        return str_nth(VIS_PN, vis_k[2] % VP_N);
    fmt_int(b, c > 3u ? vis_id[4] : vis_k[c & 3u]);
    return b;
}
static void vis_saver(void)
{
    uint16_t surf = ux.surf;
    uint32_t c = vis_ov & 7u, on = scrn.st != SCR_VIS && fm1_ms - (vis_ov & ~15u) < 1300u, i, y, r, h;
    char b[8];
    ux.surf = ux.bg;
    CV_BANDS(240, 240, 0) {
        if (!cv_band)
            vis_frame();
        cv_oy = 0;
        for (y = (uint32_t)cv_by; y < (uint32_t)cv_by + cv_bh; y += 2u)   /* the grain: every other row, dim dots */
            for (i = (uint32_t)(vg.react * (32 + vg.lvl)) >> 12; i--;) {
                r = (y * 40503u + i * 2654435761u + (vg.t / 80u) * 7919u) * 2654435761u;
                cv_rect((int32_t)((r >> 8) % 240u), (int32_t)y, 1, 1, vis_col(24u + (r >> 27)));
            }
        for (i = (uint32_t)cv_by / 6u * 40u; i < ((uint32_t)cv_by + cv_bh + 5u) / 6u * 40u; i++)   /* the heat cells */
            if ((h = VIS_HEAT[i]) > 12u)
                cv_rect((int32_t)(i % 40u) * 6, (int32_t)(i / 40u) * 6, 5, 5, vis_col((h * h >> 8) + 20u));
        for (vg.pass = 0; vg.pass < 2u; vg.pass++)      /* the wires */
            for (i = 0; i < 3u; i++)
                vis_layer(i, 0);
        if (on) {
            cv_text_in(0, 10, 240, &AF_S, str_nth(VIS_KN, c), T_THEME, 0);
            cv_text_in(0, 28, 240, &AF_L, vis_card(c, b), T_TEXT, 0);
        }
        cv_blit(0, 0);
    }
    ux.surf = surf;
}
