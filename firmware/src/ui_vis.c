/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (visgen, the owner: route A of the approved mock-up, docs/visgen-mock.html; docs/VISGEN.md) HOME > VIS and MENU >
 * SAVER: the song's visual made of BYTE's own pictures. The recipe (vis_recipe.c) says which: the content (drawn through
 * the skeleton's carrier, MIX of it), the skeleton itself faint, maybe a layer over; each ingredient is its page's own
 * drawing run with gfx.c's point map on (cv_vmap): its lines, dots and boxes come here (vis_line, vis_rect) and are
 * mapped (the source box to -1..1, the carrier, the section's morph, the deformations TWIST FOLD POLAR MELT, MIRROR
 * drawn again) onto the panel, as wires in the palette or as heat in 6 px cells with a memory (the display cache's last
 * free hashes, gfx.c dc_hash: no new RAM). SCAN shifts rows of 3. It reacts: a hit (DRUM-W's lanes, else the level
 * jumping) pulses the scale and the twist, the bass and the level heat and swell it, a chord change (SCL > HARMONY)
 * turns the colours, a section (A..D) morphs to its recipe in 1.4 s. Drawn in bands (the panel 2, the saver 4): what
 * moves is worked out once a frame, in the first band (vis_frame); the heat filled there too. Reads the UI's own
 * snapshots (track_env, dw_live, hm_*, live.cur, bar_n): nothing of the audio path. Included by ui_input.c (after
 * ui_dx.c: DRUM-W's radar) */
#define VABS(v) ((v) < 0 ? -(v) : (v))
#define VIS_HEAT ((uint8_t *)(dc_hash + DC_USE - 800u))   /* 1600 B: 40 x 40 cells (the panel's 40 x 24) */
static struct {
    vis_recipe_t r;
    uint32_t key, t, mt, last;                     /* the recipe's inputs; now (ms); the morph's start; a chord's */
    int16_t kick, snr, bass, lvl, plvl, sc, ee, mix, react, scan, hg, cmix;   /* 0..256 (sc Q10) */
    int16_t def[VD_N];                                  /* the frame's deformations, 256 = the recipe's at DEFORM 50 */
    int16_t sx, sy, sw, sh, W, H, cx, cy, R, asp;       /* the layer's source box; the strip; its centre, scale (asp Q10) */
    uint8_t sec, prev, chord, root, q, hacc, hst, dwp, mir, pal, n;   /* (hacc: the frame's hits; hst: Studio's) */
    uint8_t ly[5][4];                                   /* the frame's layers: ingredient, content?, alpha, heat? */
    uint8_t ing, cont, al, heat;                        /* the layer drawn */
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
    const uint16_t *p = VIS_RAMP[(vg.pal + 4u) % 5u];
    uint32_t s = v > 255u ? 255u : v, k = s * 3u >> 8;
    uint16_t c = ux_mix(p[k], p[k + 1u], (int32_t)((s * 3u & 255u) * 100u >> 8));
    return s < 40u ? ux_mix(0, c, (int32_t)s * 5 / 2) : c;
}
static int32_t vsin(uint32_t a) { return bg_sin(a); }   /* (a turn 65536, Q15) */
static uint32_t vph(uint32_t k) { return vg.t * k >> 4; }   /* the time's phase at k / 167 rad a second */

/* ------------------------------------------------------------------------------------------- the song --- */
static uint32_t vis_bits(uint32_t i)                    /* VE_* of part i */
{
    const track_t *t = &trk[i];
    uint32_t e = trk_eng(t);
    return (e == ENGI_WAVE ? 1u : e == ENGI_LOFI ? 2u : e == ENGI_FUNC ? (t->p[P_E7] == (int)FUNC_BANK ? 12u : 4u) :
            e == ENGI_DRUM ? 16u : e == ENGI_HIVE ? 32u : 0u) | (clay[i][CL_ON] ? 64u : 0u) | (t->p[P_DLY] > 0 || t->p[P_CHOR] > 0 ? 128u : 0u);
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
    o[0] = vis_id[2] ? (uint32_t)vis_id[0] | (uint32_t)vis_id[1] << 8 : h >> 11;
    o[1] = vis_id[2] ? vis_id[2] - 1u : VIS_GENRES;
    o[2] = vis_id[2] ? vis_id[3] : 50u;
    o[3] = vis_id[4];
}
static void vis_gen_set(uint32_t seed, uint32_t genre, uint32_t energy)   /* (gen.c: a piece made; its recipe byte 0) */
{
    vis_id[0] = (uint8_t)seed; vis_id[1] = (uint8_t)(seed >> 8);
    vis_id[2] = (uint8_t)((genre < VIS_GENRES ? genre : VIS_GENRES) + 1u);
    vis_id[3] = (uint8_t)(energy > 100u ? 100u : energy);
    vis_id[4] = 0;
}
static void vis_oct(int up)                             /* OCT+ another recipe, OCT- the song's own */
{
    char b[8];
    vis_id[4] = up ? (uint8_t)(vis_id[4] + 1u) : 0u;
    fmt_int(b, vis_id[4]);
    ui_say("VIS RECIPE ", b);                          /* (0: the song's own) */
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
    if (hm_n && song.playing) { i = ((bar_n * 4u + beat_n) >> (uint32_t)song.g[G_HLEN]) % hm_n; d = (uint32_t)(hm_dg[i] % 7 + 7) % 7u; x = hm_x[i]; }
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
    uint32_t k, now = 0, on, h;
    for (k = 0; k < 8u; k++) now |= (uint32_t)(dw_live[k] >> 15 & 1u) << k;
    on = now & ~(uint32_t)vg.dwp;
    vg.dwp = (uint8_t)now;
    h = (on & 1u) | (on & 6u ? 2u : 0u) | (on & 24u ? 4u : 0u) | (on & 224u ? 8u : 0u);
    vg.hacc |= (uint8_t)h;
    vg.hst |= (uint8_t)h;
}
/* JIANT Studio's state (docs/VISGEN.md): VIS_SONG's fields, VIS_TICK's, playing (21); the hits since the last call */
static void vis_state(int32_t *o)
{
    uint32_t s[4], i, m = vis_mask();
    vis_song(s);
    o[0] = (int32_t)s[0]; o[1] = (int32_t)s[3]; o[2] = (int32_t)s[1]; o[3] = (int32_t)s[2]; o[4] = (int32_t)m;
    o[5] = (int32_t)vis_sec(); o[6] = vis_k[2]; o[7] = vis_k[0]; o[8] = vis_k[1]; o[9] = vis_k[3];
    for (i = 0; i < 6u; i++) o[10u + i] = i < NTRK ? clamp(track_env[i] >> 8, 0, 127) : 0;
    vis_hits();
    o[16] = vg.hst; vg.hst = 0;
    (void)vis_chord();
    o[17] = vg.root; o[18] = vg.q; o[19] = (int32_t)(trk[0].seq_idx & 63u); o[20] = song.playing != 0;
}

/* ------------------------------------------------------------------------------------------- the map --- */
/* skeleton sk's carrier at (u, v) (-1024..1024): o[0], o[1] (y up), o[2] its nearness (0..1024) */
static void vis_car(uint32_t sk, int32_t u, int32_t v, int32_t *o)
{
    int32_t x = (u * vg.asp >> 10) * 943 >> 10, y = v * 922 >> 10, z = 1024, a, b;
    if (sk == VI_LIFE || sk == VI_KEY) {                /* a sphere turning, tilted (LIFE's, KEY's) */
        uint32_t lon = (uint32_t)(u * 32) + vph(sk ? 37u : 75u), lat = (uint32_t)(v * 15);
        int32_t cl = vsin(lat + 0x4000u), X = cl * vsin(lon) >> 15, Y = vsin(lat), Z = cl * vsin(lon + 0x4000u) >> 15;
        int32_t st = 12000, ct = 30490, R = 810 * vg.sc >> 10;   /* (tilted 0.37 rad; KEY's turns slower) */
        x = X * R >> 15;
        y = ((Y * ct - Z * st) >> 15) * R >> 15;
        z = (((Y * st + Z * ct) >> 15) + 32768) >> 6;
    } else if (sk == VI_RADAR) {                        /* polar: across the turn, up the radius */
        a = (u + 1024) * 32 + (int32_t)(vg.t >> 1);
        b = ((v + 1024) * 471 >> 10) * vg.sc >> 10;
        x = vsin((uint32_t)a) * b >> 15; y = vsin((uint32_t)a + 0x4000u) * b >> 15;
    } else if (sk == VI_ORBIT) {                        /* along a Lissajous, v its width */
        uint32_t q = (uint32_t)(u + 1024) * 32u, ph = vph(42u) + (uint32_t)(vg.bass * vg.react >> 4);
        b = 800 * vg.sc >> 10;
        x = (vsin(VIS_LIS[vg.r.lis][0] * q + ph) * b >> 15) * vg.asp >> 10;
        y = (vsin(VIS_LIS[vg.r.lis][1] * q) * b >> 15) + (v * 164 >> 10);
    } else if (sk == VI_TUNNEL) {                       /* a tunnel: round its ring, v into the depth (bending) */
        int32_t k = 1048576 / (1024 + ((v + 1024) * 1229 >> 10)), dr = 1024 - k;
        uint32_t q = (uint32_t)(u + 1024) * 32u;
        x = ((((vsin(q) >> 5) * vg.asp >> 10) * k >> 10) + (vsin(vph(50u)) * dr >> 17)) * vg.sc >> 10;
        y = (((vsin(q + 0x4000u) >> 5) * k >> 10) + (vsin(vph(38u) + 0x4000u) * dr >> 18)) * vg.sc >> 10;
        z = k;
    } else if (sk == VI_CHORUS || sk == VI_LFO || sk == VI_HIVE || sk == VI_CLAY) {   /* the flat bent by a wave: CHORUS's
                                                         * fast, LFO's slow, HIVE's quick, CLAY's in steps */
        if (sk == VI_CLAY) u &= ~255;
        y += vsin((uint32_t)(u * (int32_t)(sk * 9u)) - vph(sk * 40u)) * (120 + (vg.lvl * vg.react >> 6)) >> 15;
    } else if (sk == VI_MATH) {                         /* .. cells */
        x = (x + 4096) / 146 * 146 - 4088; y = (y + 4096) / 146 * 146 - 4088;
    }
    o[0] = x; o[1] = y; o[2] = z;
}
static void vis_deform(int32_t *o)                      /* TWIST FOLD POLAR (MELT, MIRROR, SCAN: on the screen) */
{
    int32_t tw = vg.def[VD_TWIST], fo = vg.def[VD_FOLD], po = vg.def[VD_POLAR], k;
    if (fo > 25) {                                      /* folded back at +-f (and up and down, past half) */
        int32_t f = 1126 - (874 * (fo > 256 ? 256 : fo) >> 8) + (vsin(vph(67u)) * 82 >> 15) + (vg.kick * vg.react >> 10);
        int32_t fy = f * (922 + (vsin(vph(45u)) * 205 >> 15)) >> 10;
        for (k = 0; k < 3; k++) {
            o[0] = o[0] > f ? 2 * f - o[0] : o[0] < -f ? -2 * f - o[0] : o[0];
            if (fo > 140) o[1] = o[1] > fy ? 2 * fy - o[1] : o[1] < -fy ? -2 * fy - o[1] : o[1];
        }
    }
    if (tw > 5 || po > 5) {                             /* turned about the middle, the more the nearer; rippled */
        int32_t r = (int32_t)isqrt32((uint32_t)(o[0] * o[0] + o[1] * o[1])), a, c, s, x, w = 1024 - r;
        a = tw * w >> 10;
        a = (a * ((vsin(vph(62u)) * 30 >> 15) + (vg.kick * vg.react >> 11))) >> 3;
        if (po > 5)
            a += po * vsin((uint32_t)(r * 92) - vph(334u)) >> 13;
        c = vsin((uint32_t)a + 0x4000u); s = vsin((uint32_t)a);
        x = (o[0] * c - o[1] * s) >> 15;
        o[1] = (o[0] * s + o[1] * c) >> 15;
        o[0] = x;
    }
}
/* a point of the layer's source into the strip: X, Y; its nearness (0..1024) */
static int32_t vis_pt(int32_t x, int32_t y, int32_t *X, int32_t *Y)
{
    int32_t u = (x - vg.sx) * 2048 / vg.sw - 1024, v = 1024 - (y - vg.sy) * 2048 / vg.sh, f[3], n[3], m;
    vis_car(VI_NONE, u, v, n);
    if (vg.cont) {                                      /* the content: on the skeleton, MIX of it (a new section's:
                                                         * through the flat, the old one's out, the new one's in) */
        vis_car(vg.r.skel[vg.ee < 128 ? vg.prev : vg.sec], u, v, f);
        for (m = 0; m < 3; m++) n[m] += (f[m] - n[m]) * vg.cmix >> 8;
    }
    vis_deform(n);
    *X = vg.cx + (n[0] * vg.R >> 10);
    *Y = vg.cy - (n[1] * vg.R >> 10);
    if (vg.def[VD_MELT] > 12)                           /* MELT: each column of 3 drops its own, with the bass */
        *Y += ((int32_t)((uint32_t)(*X >> 2) * 2654435761u >> 27) + 5) * vg.def[VD_MELT] * (154 + (vg.bass * vg.react >> 8)) >> 16;
    if (vg.scan > 12) {                                 /* SCAN (or a snare): rows of 3 shifted, a hit throws some far */
        uint32_t b = (uint32_t)*Y / 3u;
        *X += ((((b * 7u) ^ (trk[0].seq_idx * 11u)) & 31u) < 4u ? ((vg.kick * vg.react >> 8) + 77) * vg.scan * 30 >> 16 : vg.scan >> 6) *
              (b & 1u ? 1 : -1);
    }
    return n[2];
}
/* one mapped piece (x0, y0) .. (x1, y1) of the strip in colour c: as heat (band 0 only: the cells) or a wire, mirrored */
static uint32_t vis_lum(uint32_t c) { return ((c >> 11) * 616u + ((c >> 5) & 63u) * 600u + (c & 31u) * 232u) >> 8; }   /* 0..255 */
static void vis_put(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)
{
    uint32_t m, lum = vis_lum(c);
    int32_t n = (VABS(x1 - x0) > VABS(y1 - y0) ? VABS(x1 - x0) : VABS(y1 - y0)) / 3 + 1, i, inc = (int32_t)lum * a * vg.hg >> 20;
    for (m = 0; m <= vg.mir; m++) {
        if (m == 1u) { x0 = vg.W - 1 - x0; x1 = vg.W - 1 - x1; }   /* (MIRROR: across, then up and down too) */
        if (m == 2u) { y0 = vg.H - 1 - y0; y1 = vg.H - 1 - y1; }
        if (!vg.heat)
            cv_line(x0, y0, x1, y1, ux_mix(0, c, a * 100 >> 8));
        else                                            /* heat: every 3 px along it, its cell warmer */
            for (i = 0; i < n; i++) {
                int32_t x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
                if ((uint32_t)x < (uint32_t)vg.W && (uint32_t)y < (uint32_t)vg.H) {
                    uint8_t *h = &VIS_HEAT[(y / 6) * 40 + x / 6];
                    *h = (uint8_t)(*h + inc > 255 ? 255 : *h + inc);
                }
            }
    }
}
static uint16_t vis_ink(uint16_t c)                     /* a source colour's wire: its brightness along the palette, the
                                                         * chord turning it */
{
    return vis_col(64u + (vis_lum(c) + vg.chord * 53u) % 192u);
}
static void vis_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    const cv_vmap_t *k = cv_vmap;
    int32_t n = (VABS(x1 - x0) > VABS(y1 - y0) ? VABS(x1 - x0) : VABS(y1 - y0)) / 6 + 1, i, px, py, qx, qy, z, oy = cv_oy;
    uint16_t w = vg.heat ? c : vis_ink(c);
    if (c == ux.bg || c == ux.surf || (vg.heat && cv_band))
        return;
    if (n > 40) n = 40;
    cv_vmap = 0;
    cv_oy = 0;
    vis_pt(x0, y0, &px, &py);
    for (i = 1; i <= n; i++) {
        z = vis_pt(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, &qx, &qy);
        vis_put(px, py, qx, qy, w, z < 460 ? vg.al * 3 / 10 : vg.al);
        px = qx; py = qy;
    }
    cv_oy = oy;
    cv_vmap = k;
}
static void vis_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (h <= 6)                                         /* a small box, a rule: a line across its middle */
        vis_line(x, y + h / 2, x + w - 1, y + h / 2, c);
    else if (w <= 6)
        vis_line(x + w / 2, y, x + w / 2, y + h - 1, c);
    else if (h >= 12) {                                 /* a box: its outline (a tag's, smaller: left out) */
        vis_line(x, y, x + w - 1, y, c);
        vis_line(x + w - 1, y, x + w - 1, y + h - 1, c);
        vis_line(x + w - 1, y + h - 1, x, y + h - 1, c);
        vis_line(x, y + h - 1, x, y, c);
    }
}
static const cv_vmap_t VIS_MAP = {vis_line, vis_rect};

/* ------------------------------------------------------------------------------------------- the layers --- */
/* each ingredient's source box (x, y, w, h: where its picture lies in its page's panel) */
static const uint8_t VIS_BOX[VI_N][4] = {{12, 0, 150, 140}, {4, 4, 116, 116}, {4, 10, 232, 100}, {10, 0, 220, 100},
                                         {10, 0, 220, 140}, {14, 4, 112, 112}, {4, 22, 118, 118}, {0, 0, 240, 140},
                                         {0, 0, 240, 140}, {0, 0, 240, 140}, {0, 0, 200, 140}, {0, 14, 240, 126},
                                         {60, 10, 120, 120}};
static void vis_layer(uint32_t i, uint32_t heat)       /* layer i of the frame, its page's own picture through the map:
                                                         * its heat, or its wire (a heat layer's faint over its cells) */
{
    const uint8_t *L = vg.ly[i], *B = VIS_BOX[L[0] % VI_N];
    const track_t *t = vis_trk(L[0]);
    int32_t oy = cv_oy;
    vg.ing = L[0]; vg.cont = L[1]; vg.heat = (uint8_t)heat; vg.al = (uint8_t)(!heat && L[3] ? L[2] * 115 >> 8 : L[2]);
    vg.sx = B[0]; vg.sy = B[1]; vg.sw = B[2]; vg.sh = B[3];
    cv_oy = -30000;                                     /* (what draws straight into the canvas: nowhere) */
    cv_vmap = &VIS_MAP;
    switch (L[0]) {
    case VI_LIFE: graph_eco(1); break;
    case VI_KEY: if (bt_hw_seg()) graph_key(t, 0); break;
    case VI_HARMONY: if (bt_hw_seg()) graph_harm(t); break;
    case VI_LFO: graph_lfo(t, T_THEME); break;
    case VI_SPLASH: sig_pen((vg.t - vg.mt) >> 3); break;   /* (written anew each section, in 2.5 s) */
    case VI_RADAR: dw_play_panel(); break;
    case VI_ORBIT: case VI_FUNC: case VI_HIVE: (void)vx_draw(t, 0, 0, 240, 140, -1); break;   /* (hud_wave, hud_func, vx_hive) */
    case VI_CLAY: graph_clay_lz(t); break;
    case VI_TUNNEL: case VI_CHORUS: (L[0] == VI_TUNNEL ? fxg_delay : fxg_chorus)(0, 0, 240, 140); break;
    case VI_MATH: (void)vx_draw(t, 60, 10, 120, 120, -1); break;   /* (small: the being, ub_draw's) */
    }
    cv_vmap = 0;
    cv_oy = oy;
}
static void vis_add(uint32_t ing, uint32_t cont, int32_t al, uint32_t heat)
{
    if (ing < VI_N && al > 4 && vg.n < 5u) {
        uint8_t *L = vg.ly[vg.n++];
        L[0] = (uint8_t)ing; L[1] = (uint8_t)cont; L[2] = (uint8_t)(al > 255 ? 255 : al); L[3] = (uint8_t)heat;
    }
}
/* the frame (the first band): the song, the music, the recipe's state, the layers; the heat cooled and filled */
static void vis_frame(void)
{
    uint32_t s[4], i, k, e, key, cur, old, now = fm1_ms;
    int32_t ee, a1, dk;
    vis_song(s);
    k = vis_mask();
    key = ((s[0] * 31u + s[1]) * 131u + s[2]) * 257u + k * 7919u + s[3] * 104729u;
    if (key != vg.key || now - vg.last > 400u) {        /* a new song, recipe, or the page entered: from clean */
        if (key != vg.key) {
            if (bt_hw_seg())                            /* (the recipe: in the Bluetooth segment) */
                vis_recipe(&vg.r, s[0], s[1], s[2], k, s[3]);
            vis_k[0] = vg.r.mix; vis_k[1] = 50; vis_k[3] = vg.r.react;
        }
        vg.key = key;
        memset(VIS_HEAT, 0, 1600u);
        vg.sec = vg.prev = (uint8_t)vis_sec();
        vg.mt = now - 2000u;
    }
    vg.last = vg.t = now;
    vg.asp = (int16_t)(vg.H == 240 ? 1024 : 1755);     /* (the saver's square, the panel's 240 x 140) */
    vg.cx = 120; vg.cy = (int16_t)(vg.H >> 1); vg.R = (int16_t)(vg.H == 240 ? 114 : 66);
    for (i = 0, e = 0, k = 0; i < trk_live && i < NTRK; i++) {   /* the levels: the loudest, the mean */
        int32_t l = clamp(track_env[i] >> 7, 0, 256);
        e = (uint32_t)l > e ? (uint32_t)l : e;
        k += (uint32_t)l;
    }
    vg.lvl = (int16_t)e; vg.bass = (int16_t)(k / (trk_live ? trk_live : 1u));
    vis_hits();
    vg.kick = (int16_t)((vg.hacc & 1u) || (int32_t)e > vg.plvl + 40 ? 256 : vg.kick * 205 >> 8);
    vg.snr = (int16_t)(vg.hacc & 2u ? 256 : vg.snr * 200 >> 8);   /* (as JIANT FM's: a snare shifts bands, a glitch) */
    vg.hacc &= (uint8_t)~3u;
    vg.plvl = (int16_t)e;
    vg.chord = (uint8_t)vis_chord();
    i = vis_sec();
    if (vg.H == 240)                                    /* (the saver, as JIANT FM's: another scene every 30 s) */
        i = (i + now / 30000u) & 3u;
    if (i != vg.sec) { vg.prev = vg.sec; vg.sec = (uint8_t)i; vg.mt = now; }
    ee = (int32_t)((now - vg.mt) * 187u >> 10);   /* (1.4 s) */
    ee = ee > 256 ? 256 : ee;
    vg.ee = (int16_t)(ee * ee * (768 - 2 * ee) >> 16);  /* (smoothstep) */
    vg.mix = (int16_t)(vis_k[0] * 655 >> 8);       /* (0..100 to 0..256) */
    vg.cmix = (int16_t)(vg.r.skel[vg.prev] == vg.r.skel[vg.sec] ? vg.mix : vg.mix * VABS(2 * vg.ee - 256) >> 8);
    vg.react = (int16_t)(vis_k[3] * 655 >> 8);
    vg.pal = vis_k[2] ? vis_k[2] : vg.r.pal;
    dk = vis_k[1] * 655 >> 7;
    cur = vg.sec; old = vg.prev;
    for (i = 0; i < VD_N; i++) {                        /* the deformations: the old section's into the new one's */
        int32_t a = 0, b = 0;
        for (k = 0; k < 2u; k++) {
            if (vg.r.def[old][k] == i) a = vg.r.amt[old][k] * 655 >> 8;
            if (vg.r.def[cur][k] == i) b = vg.r.amt[cur][k] * 655 >> 8;
        }
        vg.def[i] = (int16_t)((a + ((b - a) * vg.ee >> 8)) * dk >> 8);
    }
    vg.scan = (int16_t)(vg.def[VD_SCAN] > vg.snr * vg.react >> 9 ? vg.def[VD_SCAN] : vg.snr * vg.react >> 9);
    vg.mir = (uint8_t)(vg.def[VD_MIRROR] > 154 ? 2u : vg.def[VD_MIRROR] > 25 ? 1u : 0u);
    vg.sc = (int16_t)(1024 + (vg.react * (vg.kick * 82 + vg.bass * 102) >> 16));
    vg.n = 0;                                           /* the layers: content, skeleton(s), over(s) */
    a1 = 51 + (154 * (256 - vg.mix) >> 8);
    vis_add(vg.r.content, 1, 255, vg.r.blend != VB_SUM);
    vis_add(vg.r.skel[cur], 0, vg.r.skel[old] != vg.r.skel[cur] ? a1 * vg.ee >> 8 : a1, vg.r.blend == VB_THERM);
    if (vg.r.skel[old] != vg.r.skel[cur]) vis_add(vg.r.skel[old], 0, a1 * (256 - vg.ee) >> 8, vg.r.blend == VB_THERM);
    vis_add(vg.r.over[cur], 0, vg.r.over[old] == vg.r.over[cur] ? 180 : 180 * vg.ee >> 8, vg.r.blend == VB_THERM);
    if (vg.r.over[old] != vg.r.over[cur]) vis_add(vg.r.over[old], 0, 180 * (256 - vg.ee) >> 8, vg.r.blend == VB_THERM);
    {   /* the heat: cooled, then the heat layers into it (once a frame) */
        uint32_t d = vg.r.blend == VB_THERM ? 212u : 200u, g = 96u + (uint32_t)(vg.bass * vg.react >> 9);
        uint8_t *h = VIS_HEAT;
        vg.hg = (int16_t)g;
        for (i = 0; i < 1600u; i++) h[i] = (uint8_t)(h[i] * d >> 8);
        for (i = 0; i < vg.n; i++)
            if (vg.ly[i][3])
                vis_layer(i, 1);
    }
}
/* the strip (vg.W x vg.H) in the band being drawn */
static void vis_draw(void)
{
    uint32_t i, r0 = (uint32_t)cv_by / 6u, r1 = ((uint32_t)cv_by + cv_bh + 5u) / 6u, c;
    if (!cv_band)
        vis_frame();
    cv_oy = 0;
    for (; r0 < r1; r0++)                               /* the heat cells */
        for (c = 0; c < 40u; c++) {
            uint32_t h = VIS_HEAT[r0 * 40u + c];
            if (h > 12u)
                cv_rect((int32_t)c * 6, (int32_t)r0 * 6, 5, 5, vis_col((h * h >> 8) + 20u));
        }
    for (i = 0; i < vg.n; i++)                          /* the wires */
        vis_layer(i, 0);
}
static void vis_panel(void)                             /* HOME > VIS: the panel (ui_lzhome.c graph_home_lz) */
{
    vg.W = 240; vg.H = (int16_t)graph_h();
    vis_draw();
}
static void vis_saver(void)                             /* MENU > SAVER: the whole screen, in four bands (ui_draw.c) */
{
    uint16_t surf = ux.surf;
    ux.surf = ux.bg;
    vg.W = 240; vg.H = 240;
    CV_BANDS(240, 240, 0) {
        vis_draw();
        cv_blit(0, 0);
    }
    ux.surf = surf;
}
static const char *vis_card(uint32_t c, char *b)        /* HOME > VIS's cards' values */
{
    if (c == 2u)
        return str_nth(VIS_PN, vis_k[2] % VP_N);
    fmt_int(b, vis_k[c & 3u]);
    return b;
}
