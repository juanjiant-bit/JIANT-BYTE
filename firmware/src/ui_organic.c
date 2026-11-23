/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): JIANT FM's organic line art, drawn by code (docs/TONIC-UI.md). */
/* Everything is drawn into the graph canvas (gfx.c cv_px) with integer maths: no stored artwork. Positions in
 * 1/16 px (Q4) so curves move smoothly between pixels; lines antialiased (two pixels across, blended over what
 * is there). Angles are 16-bit turns (0x10000 = 360 degrees), through dsp.c's sine table.
 *   og_line     an antialiased line (Q4 ends)
 *   og_quad, og_cubic   Bezier curves, in short lines
 *   og_path     a closed outline of cubic segments from a small table of points (int8), morphed between two such
 *               tables (A and B: the same shape count), scaled about an anchor (inner copies: the plates' vessels)
 *               and mirrored (one table draws both sides of a symmetric organ)
 *   og_ring, og_node    a circle; a dot with a soft rim
 *   og_dash     a dashed line (the plates' axes and leaders); og_label a letter in a circle (their references)
 *   og_stipple  dots scattered in an ellipse (their stippled shading), the same dots every frame
 *   og_col      the five JIANT colours: cream, coral, teal, mustard, mint (on another palette: blends of its own)
 * The look is the anatomical wall chart (1960s, white line on black): thin cream outlines, colour only in details
 * (red tips, yellow and cyan vessels), cyan reference letters, dashed axes. */
#define OG_Q 16                                          /* Q4 */
enum { OG_CREAM, OG_CORAL, OG_TEAL, OG_MUSTARD, OG_MINT };

static int og_neutral(uint16_t c) { return (c >> 11) == (c & 31u) && ((c >> 5) & 63u) >> 1 == (c >> 11); }
/* c over the canvas pixel at (x, y) with coverage a (0..256) */
static inline void og_plot(int32_t x, int32_t y, uint16_t c, int32_t a)
{
    uint16_t *p, o;
    uint32_t r, g, b;
    y += cv_oy;
    if (a <= 0 || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 256) {
        *p = swap16(c);
        return;
    }
    o = swap16(*p);
    r = (o >> 11) + ((((int32_t)(c >> 11) - (int32_t)(o >> 11)) * a) >> 8);
    if (og_neutral(o) && og_neutral(c)) {                /* grey over grey stays grey */
        *p = swap16((uint16_t)(r << 11 | r << 6 | r));
        return;
    }
    g = ((o >> 5) & 63u) + ((((int32_t)((c >> 5) & 63u) - (int32_t)((o >> 5) & 63u)) * a) >> 8);
    b = (o & 31u) + ((((int32_t)(c & 31u) - (int32_t)(o & 31u)) * a) >> 8);
    *p = swap16((uint16_t)(r << 11 | g << 5 | b));
}
/* an antialiased line between Q4 points, at strength a (0..256) */
static void og_line_a(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)
{
    int32_t dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, t, g, p, e, f;
    if (adx >= ady) {                                    /* along x: y in Q8, two pixels per column */
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        g = adx ? ((y1 - y0) << 8) / (x1 - x0) : 0;      /* dy / dx, Q8 */
        e = x1 >> 4;
        for (p = x0 >> 4; p <= e && p - (x0 >> 4) < 480; p++) {
            int32_t y = (y0 << 4) + g * (p - (x0 >> 4)) - 128;   /* (pixel centres) */
            f = y & 255;
            og_plot(p, y >> 8, c, ((256 - f) * a) >> 8);
            og_plot(p, (y >> 8) + 1, c, (f * a) >> 8);
        }
    } else {
        if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
        g = ((x1 - x0) << 8) / (y1 - y0);
        e = y1 >> 4;
        for (p = y0 >> 4; p <= e && p - (y0 >> 4) < 480; p++) {
            int32_t x = (x0 << 4) + g * (p - (y0 >> 4)) - 128;
            f = x & 255;
            og_plot(x >> 8, p, c, ((256 - f) * a) >> 8);
            og_plot((x >> 8) + 1, p, c, (f * a) >> 8);
        }
    }
}
static void og_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { og_line_a(x0, y0, x1, y1, c, 256); }
/* (codex/anim) a polyline's points 0..n, one at a time, at the pictures' level of detail (ui.c ui_lod: 0 whole .. 256
 * half): each odd point glides onto the chord between its two neighbours as the detail falls, and at half is left out,
 * half the lines (a geomorph, as a 3D engine blends a mesh's levels: the outline the same, nothing pops). At 0 the
 * lines drawn are the ones drawn before it. pl_skip: an odd point the caller need not even work out */
#define pl_skip(i, n) (((i) & 1u) && (i) < (n) && ui_lod >= 256u)
static void pl_pt(uint32_t i, uint32_t n, int32_t x, int32_t y, uint16_t c, int32_t a)
{
    anim_st_t *q = &anim_st;
    int32_t m = q->lod;
    if ((i & 1u) && i < n && m) {                       /* (an odd point: held for the next) */
        q->ox = x;
        q->oy = y;
        return;
    }
    if (i && !(i & 1u) && m && m < 256) {               /* (an even one after it: the held one on its way to the chord) */
        q->ox += (((q->px + x) >> 1) - q->ox) * m >> 8;
        q->oy += (((q->py + y) >> 1) - q->oy) * m >> 8;
        og_line_a(q->px, q->py, q->ox, q->oy, c, a);
        q->px = q->ox;
        q->py = q->oy;
    }
    if (i)
        og_line_a(q->px, q->py, x, y, c, a);
    q->px = x;
    q->py = y;
}


/* cos / sin of a 16-bit angle, Q15 */
static int32_t og_cos(uint32_t ang) { return sine_i((ang + 0x4000u) << 16); }
static int32_t og_sin(uint32_t ang) { return sine_i(ang << 16); }


/* A shape: n cubic segments, a start point then three points a segment, x y as int8 (units, y down), closed by its
 * last point = its start. og_path draws it morphed m (0 A .. 256 B), each unit u / 16 px (Q4 per unit: u), about
 * the anchor (ax, ay) (units) scaled by k (Q8: 256 the shape itself) and mirrored (mx -1) at (cx, cy) (Q4) */
typedef struct {
    int16_t cx, cy, u, mx;                               /* where, units -> Q4, -1 mirrors */
    int16_t ax, ay, k;                                   /* scaled about (ax, ay) by k / 256 */
} og_xf_t;

/* a filled dot of radius r px (pixel centre (cx, cy) in Q4) with an antialiased rim */
static void og_node(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x0 = cx >> 4, y0 = cy >> 4, i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++) {
            int32_t d = i * i + j * j - r * r;          /* inside: d <= 0; the rim: 0 .. 2r */
            if (d <= -r)
                og_plot(x0 + i, y0 + j, c, 256);
            else if (d < r)
                og_plot(x0 + i, y0 + j, c, 128 - d * 128 / r);
        }
}

/* a shape from assets/ui-shapes (tools/gen_ui_shapes.py: build/gen/ui_shapes.h) */
typedef struct {
    uint8_t nseg, mirror;
    const int8_t *a, *b;
} og_shape_t;
#include "ui_shapes.h"
#define og_mix ux_mix
/* a plate (og_ill's bytes) alive: glow[k] (-256..256, 0 none) takes role k toward white (+) or into the surface (-);
 * lit[i] (0..256) lights the i-th dot of role dot_role (a hit, a voice: its own size up to 2 px more); sway (Q4 px)
 * bends every point sideways by its height, a wave of 64 units at phase ph (16 bits a turn); u Q4 per unit */

/* an illustration (assets/ui-shapes, an SVG without ids: tools/gen_ui_shapes.py) at (cx, cy) (Q4), u Q4 per unit;
 * col[k] the colour of role k (tools/gen_ui_shapes.py colour_class: 0 cream, 1 red, 2 teal, 3 cyan, 4 green, 5 yellow,
 * 6 orange, 7 blue / purple); parts are polylines (kind 2), dots (1) or cubic outlines (0); glow[k] (0..256) lights role k: its lines toward
 * cream, its dots a pixel larger (a hit, a value: the caller's) */
