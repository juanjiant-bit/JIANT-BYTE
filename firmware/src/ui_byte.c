/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE) the formula on the screen and the beings born from it: MATH's and WAVE's SOUND panel (ui_engvis.c
 * vx_draw), HOME's strip of the parts (small).
 *   the text     the part's formula as it plays (MATH: its bank's BYTE or FLOAT, or its USER text; WAVE: A's), in the
 *                pixel face, wrapped (the u of an unsigned number left out: it is the machine's, not the reader's)
 *   the being    born from the formula, run here: an INT one (bytebeat) a sprite of its bits mirrored, 11 x 11, a
 *                new pose as its time moves; a FIX one (floatbeat) its phase portrait (f(t) against f(t + d)) with its
 *                wave hanging under it. Its name from the formula's text. It sways with the part's level
 *   MATH         the raster: f(t) along t, a row 256 of t, a cell 3 x 3 px, the newest row at the foot (a waterfall:
 *                the rows move up a row a frame, ~8 kHz of t a second)
 *   WAVE         the table as a wire terrain: A's cycle at 12 POS from 0 (back) to 127 (front), POS's own row lit
 * Every formula compiled here a frame (a few thousand cycles) in the project scratch (project.c math_poll's work
 * space: the main loop's, one job at a time), run in blocks of FVM_BLK: no RAM of its own but the name's */

#define UB_ROWS 30                                      /* the raster's rows (3 px) */
#define UB_COLS 40                                      /* .. and columns */

static gq_work_t *proj_ws(void);                        /* (project.c: the project scratch, math_poll's work space) */
static gq_work_t *ub_ws(void) { return proj_ws(); }

/* the formula part t plays (0: none), its type in *fix */
static const char *ub_text(const track_t *t, uint32_t *fix)
{
    const int16_t *p = t->p;
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    if (trk_engp(t) == &ENG_WAVE) {
        *fix = 1;
        return WAVE_BANK[(uint32_t)clamp(p[P_E0], 0, 127) % WAVE_NF];
    }
    if (trk_engp(t) != &ENG_LOFI)
        return 0;
    if(p[P_E1]==RW_FUNC){*fix=1;return math_func_text(t,lyr_txt[tr]);}
    if (p[P_E1] >= RW_USER) {
        *fix = math_ty[tr] == FT_FIX;
        return math_txt[tr][0] ? math_txt[tr] : 0;
    }
    *fix = p[P_E1] == RW_FLOAT;
    return *fix ? FVM_FLOAT_BANK[((uint32_t)p[P_E2] >> 2) % FVM_NFLOAT] : FVM_BYTE_BANK[((uint32_t)p[P_E2] >> 2) % FVM_NBYTE];
}
/* compiled into the work space: 0 when it does not compile */
static int ub_compile(gq_work_t *w, const char *s)
{
    uint32_t n = 0;
    fvm_err_t e;
    while (s[n]) n++;
    return !fvm_compile(&w->cc, FVM_FULL(&w->pg), s, n, &e);
}
/* f at n times (tq: Q8, the judge's mapping: t = tq >> 8, w its phase) -> 0 .. 255 each (INT: its byte; FIX: -1 .. 1) */
static void ub_run(gq_work_t *w, const uint32_t *tq, uint32_t n, uint32_t a, uint8_t *o)
{
    uint32_t T[FVM_BLK], W[FVM_BLK], y[FVM_BLK], i, j;
    for (i = 0; i < n; i += FVM_BLK) {
        uint32_t m = n - i < FVM_BLK ? n - i : FVM_BLK;
        fvm_env_t en = {{T, W, W, T, W}, {0}, w->st};
        en.u[FU_A] = 5u;
        en.u[FU_AF] = a;
        for (j = 0; j < m; j++) {
            T[j] = tq[i + j] >> 8;
            W[j] = tq[i + j] << 3;
        }
        fvm_run(&w->pg.p, &en, &w->sc, m, y);
        for (j = 0; j < m; j++)
            o[i + j] = w->pg.p.ty == FT_INT ? (uint8_t)y[j] : (uint8_t)clamp(((int32_t)y[j] >> 9) + 128, 0, 255);
    }
}
/* a name from the text: two or three syllables of its hash */
static void ub_name(const char *s, char *b)
{
    static const char C[] = "BDGKLMNPRSTXZ", V[] = "AEIOU";
    uint32_t h = 2166136261u, k, n = 0;
    for (k = 0; s[k]; k++) h = (h ^ (uint8_t)s[k]) * 16777619u;
    for (k = 0; k < 2u + (h >> 30 & 1u); k++) {
        b[n++] = C[(h >> (k * 7u)) % 13u];
        b[n++] = V[(h >> (k * 7u + 4u)) % 5u];
    }
    b[n] = 0;
}
/* the text wrapped into lines of cols characters from (x, y), at most rows lines (the last cut with an ellipsis) */
static void ub_text_draw(int32_t x, int32_t y, const char *s, uint32_t cols, uint32_t rows, uint16_t c)
{
    char b[48];
    uint32_t r, n;
    if (cols > sizeof b - 2u) cols = sizeof b - 2u;
    for (r = 0; r < rows && *s; r++) {
        for (n = 0; *s && n < cols; s++) {
            if (*s == 'u' && n && b[n - 1] >= '0' && b[n - 1] <= '9') continue;   /* (127u: 127) */
            if (*s == ' ') continue;                    /* (the bank's spaces: the pixel face reads without them) */
            b[n++] = *s;
        }
        if (r == rows - 1u && *s && n)
            b[n - 1] = ELLIPSIS;
        b[n] = 0;
        cv_text(x, y + (int32_t)r * 9 - AF_F_CAP_Y, &AF_F, b, c);
    }
}
/* Large readable formula: the existing lower-case bitmap font, exactly 2x pixels.
 * At 18 columns four rows contain the entire 63-character editable expression. */
static void ub_formula_large(const char *s)
{
    uint32_t n, gx, gy;
    for (n = 0; s[n] && n < 72u; n++) {
        aag_t g = ag(&AF_F, glyph(&AF_F, (uint8_t)s[n]));
        const uint8_t *d = AF_F.data + g.off;
        int32_t x = 12 + (int32_t)(n % 18u) * 12, y = 26 + (int32_t)(n / 18u) * 18;
        for (gy = 0; gy < g.bh; gy++) for (gx = 0; gx < g.bw; gx++) {
            uint32_t k = gy * g.bw + gx, v = (d[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            if (v) cv_rect(x + ((int32_t)gx + g.bx) * 2, y + ((int32_t)gy + g.by - AF_F_CAP_Y) * 2, 2, 2, T_TEXT);
        }
    }
}
/* a being of bits: n x n cells of px each from (x, y) (n odd, 7 .. 11), row gy's bits from v[gy], mirrored; squash
 * (0 .. 3) rows pressed out of its top (a hit); fr the pose (its feet); c its body, e its eyes */
static void ub_bits(const uint8_t *v, uint32_t n, int32_t x, int32_t y, int32_t px, uint32_t fr, uint32_t squash,
                    uint16_t c, uint16_t e)
{
    uint32_t gy, gx, half = (n + 1u) / 2u, ey = n / 3u;
    for (gy = squash; gy < n; gy++) {
        uint32_t b = v[gy] ^ (v[gy] >> 3) ^ (gy * 37u);
        int32_t yy = y + (int32_t)gy * px;
        for (gx = 0; gx < half; gx++) {
            uint32_t on = (b >> (gx + (gy & 1u))) & 1u, cx = half - 1u - gx;
            int32_t row = (int32_t)gy - (int32_t)n / 2;
            if (gx == half - 1u && gy >= 2u && gy + 2u < n) on = 1;   /* (the spine: one body, never two halves) */
            if ((row < 0 ? -row : row) + (int32_t)cx * 7 / 10 > (int32_t)n / 2 + 1) on = 0;   /* (round) */
            if (gy == n - 1u) on = on && (gx + fr) % 3u == 0u;   /* (the feet step) */
            if (on) {
                cv_rect(x + (int32_t)gx * px, yy, px, px, c);
                cv_rect(x + (int32_t)(n - 1u - gx) * px, yy, px, px, c);
            }
        }
    }
    if (ey >= squash) {                                 /* the eyes */
        cv_rect(x + (int32_t)(half - 3u) * px, y + (int32_t)ey * px, px, px, e);
        cv_rect(x + (int32_t)(n - half + 2u) * px, y + (int32_t)ey * px, px, px, e);
    }
}
/* Stable formula fingerprint: sampling positions stay fixed; only the feet
 * animate. A new formula changes the body, a new frame keeps its identity. */
static void ub_sprite(gq_work_t *w, int32_t x, int32_t y, int32_t px, uint32_t fr, uint16_t c, uint16_t e)
{
    uint32_t tq[11], gy;
    uint8_t v[11];
    for (gy = 0; gy < 11u; gy++)
        tq[gy] = (gy * 1237u + 4096u) << 8;
    memset(w->st, 0, sizeof w->st);
    ub_run(w, tq, 11u, 0, v);
    ub_bits(v, 11u, x, y, px, fr, 0, c, e);
}
/* a FIX being: its phase portrait about (cx, cy), r px, then its wave hanging under it as tentacles */
static void ub_portrait(gq_work_t *w, int32_t cx, int32_t cy, int32_t r, uint32_t fr, uint16_t c, uint16_t e)
{
    enum { N = 96u };
    uint32_t tq[N + 8u], i;
    uint8_t v[N + 8u];
    for (i = 0; i < N + 8u; i++)                        /* (a cycle and a little: w = tq << 3, 0 .. a turn) */
        tq[i] = i * 8192u / N + fr * 24u;
    memset(w->st, 0, sizeof w->st);
    ub_run(w, tq, N + 8u, 0, v);
    for (i = 0; i < N; i++) {
        int32_t x = cx + ((int32_t)v[i] - 128) * r / 128, y = cy + ((int32_t)v[(i + N / 4u) % N] - 128) * r * 3 / 4 / 128;
        cv_pset(x, y, i & 1u ? c : ux_mix(c, T_TEXT, 40));
    }
    for (i = 0; i < 5u; i++) {                          /* the tentacles: the wave's own values, swaying */
        int32_t x0 = cx - r * 6 / 10 + (int32_t)i * r * 3 / 10, k;
        for (k = 0; k < r; k += 2) {
            int32_t sw = ((int32_t)v[(uint32_t)(k * 3 + (int32_t)i * 17 + (int32_t)fr) % N] - 128) * 3 / 128;
            cv_pset(x0 + sw, cy + r * 6 / 10 + k, ux_mix(c, T_BG, k * 100 / r));
        }
    }
    cv_rect(cx - r * 3 / 10 - 1, cy - 2, 2, 2, e);
    cv_rect(cx + r * 3 / 10 - 1, cy - 2, 2, 2, e);
}
/* MATH's raster from (x, y): UB_COLS x UB_ROWS cells of 3 px, t a row 256, the rows climbing with the frames */
static void ub_raster(gq_work_t *w, int32_t x, int32_t y, uint32_t fr)
{
    uint32_t tq[UB_COLS], r, c, t0 = fr * 256u;
    uint8_t v[UB_COLS];
    uint16_t sh[8];
    for (c = 0; c < 8u; c++)
        sh[c] = c < 6u ? ux_mix(T_BG, T_THEME, (int32_t)(c * 20u)) : ux_mix(T_THEME, T_TEXT, (int32_t)((c - 5u) * 30u));
    for (r = 0; r < UB_ROWS; r++) {
        for (c = 0; c < UB_COLS; c++)
            tq[c] = (t0 + r * 256u + c * 256u / UB_COLS) << 8;
        memset(w->st, 0, sizeof w->st);
        ub_run(w, tq, UB_COLS, 0, v);
        for (c = 0; c < UB_COLS; c++)
            if (v[c] >= 32u)
                cv_rect(x + (int32_t)c * 3, y + (int32_t)r * 3, 3, 3, sh[v[c] >> 5]);
    }
    cv_rect(x, y + UB_ROWS * 3 - 1, UB_COLS * 3, 1, T_TEXT);   /* t, now: the foot */
}
/* WAVE's table as a wire terrain from (x, y), w x h px: 12 rows of POS (back to front), POS's own lit */
static void ub_terrain(gq_work_t *w, int32_t x, int32_t y, int32_t wd, int32_t ht, int32_t pos)
{
    enum { NR = 12u, NP = 48u };
    uint32_t tq[NP], r, i, sel = (uint32_t)clamp(pos, 0, 127) * (NR - 1u) / 127u;
    uint8_t v[NP];
    for (r = 0; r < NR; r++) {
        int32_t y0 = y + 14 + (int32_t)r * (ht - 30) / (int32_t)(NR - 1u), x0 = x + (int32_t)(NR - 1u - r) * 2, px = 0, py = 0;
        uint16_t c = r == sel ? T_TEXT : ux_mix(T_BG, T_THEME, 25 + (int32_t)(r * 60u / NR));
        for (i = 0; i < NP; i++)
            tq[i] = i * 8192u / NP;
        memset(w->st, 0, sizeof w->st);
        ub_run(w, tq, NP, r * 65535u / (NR - 1u), v);
        for (i = 0; i < NP; i++) {
            int32_t xx = x0 + (int32_t)i * (wd - 26) / (int32_t)(NP - 1u), yy = y0 - ((int32_t)v[i] - 128) * 14 / 128;
            if (i) cv_line(px, py, xx, yy, c);
            px = xx; py = yy;
        }
    }
}
/* the panel at A {x, y, w, h} (px), en_home HOME's level (-1: the part's own): 0 when t plays no formula */
static int ub_draw(const track_t *t, const int32_t *A, int32_t en_home)
{
    uint32_t fix = 0, fr = anim_fr, tr = (uint32_t)(t - trk) % NTRK;
    const char *s = ub_text(t, &fix);
    gq_work_t *w = ub_ws();
    int32_t lv = en_home >= 0 ? en_home : track_env[tr] >> 7, x = A[0], y = A[1], wd = A[2], ht = A[3], bob;
    char nm[8];
    uint16_t bc = heat_col(48 + clamp(lv, 0, 256) * 200 / 256), ec = T_TEXT;
    if (!s || !ub_compile(w, s))
        return 0;
    ub_name(s, nm);
    if (lv < 8) fr = fr / 8u;                           /* (silent: it rests, a pose now and then) */
    bob = lv > 8 && (anim_fr >> 2 & 1u) ? 2 : 0;
    if (wd < 160) {                                     /* small (HOME, SCOPE beside it): the being and its name */
        int32_t px = (wd < ht ? wd : ht) / 14 < 2 ? 2 : (wd < ht ? wd : ht) / 14;
        if (fix)
            ub_portrait(w, x + wd / 2, y + ht / 2 - 4 - bob, px * 5, fr, bc, ec);
        else
            ub_sprite(w, x + wd / 2 - px * 11 / 2, y + ht / 2 - px * 6 - bob, px, fr, bc, ec);
        cv_text_in(x, y + ht - 10 - AF_X_CAP_Y, wd, &AF_X, nm, bc, T_BG);
        return 1;
    }
    {                                                   /* t now, the newest voice's, in hex (the time the formula reads) */
        static const char HX[] = "0123456789ABCDEF";
        char b[10] = {'t', '='};
        uint32_t i, tt = 0, age = 0, k;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].age >= age) { age = t->v[i].age; tt = (uint32_t)t->v[i].s[2]; }
        for (k = 0; k < 6u; k++) b[2u + k] = HX[(tt >> (20u - 4u * k)) & 15u];
        b[8] = 0;
        cv_text_r(x + wd - 2, y + ht - 10 - AF_F_CAP_Y, &AF_F, b, age ? T_THEME : T_DIM, T_BG);
    }
    ub_text_draw(x + 2, y + 2, s, (uint32_t)(wd - 4) / 6u, 3u, T_TEXT);
    {
        int32_t by = y + 32, bh = ht - 32;
        if (fix)
            ub_portrait(w, x + 42, by + bh / 2 - 8 - bob, 30, fr, bc, ec);
        else
            ub_sprite(w, x + 8, by + bh / 2 - 36 - bob, 6, fr, bc, ec);
        cv_text_in(x, y + ht - 10 - AF_X_CAP_Y, 84, &AF_X, nm, bc, T_BG);
        if (trk_engp(t) == &ENG_WAVE)
            ub_terrain(w, x + 92, by - 6, wd - 92, bh - 4, t->p[P_E1]);
        else
            ub_raster(w, x + wd - UB_COLS * 3, by + (bh - UB_ROWS * 3) / 2 - 7, anim_fr);   /* (t= under it) */
    }
    return 1;
}

/* Pixel instruments follow the original JIANT FM kit layout and actual drum silhouettes. */
static const uint32_t UB_DRUM[8][17] = {
    {0x0u, 0xfe0u, 0x1ff0u, 0x3838u, 0x739cu, 0xeeeeu, 0xc826u, 0xd836u, 0xd016u, 0xd836u, 0xd826u, 0xfeeeu, 0x739cu, 0x3838u, 0x3ff4u, 0x4fe2u, 0x8002u},
    {0x0u, 0xfe0u, 0x600cu, 0xc006u, 0xe00eu, 0x9ff2u, 0x9112u, 0xbffau, 0xf11eu, 0xc006u, 0x600cu, 0x1ff0u, 0x1008u, 0x1008u, 0x1008u, 0x1008u, 0x2008u},
    {0x100u, 0x100u, 0x1110u, 0x1550u, 0x5454u, 0x5454u, 0x5454u, 0x5c74u, 0x5454u, 0x56d4u, 0x5654u, 0x2248u, 0x1250u, 0xa60u, 0x440u, 0x0u, 0x0u},
    {0x100u, 0x100u, 0xfe0u, 0x610cu, 0xc106u, 0x6fecu, 0x6fecu, 0xc106u, 0x610cu, 0xfe0u, 0x100u, 0x100u, 0x100u, 0x380u, 0x540u, 0x920u, 0x1010u},
    {0xfe0u, 0x610cu, 0xc106u, 0x610cu, 0xfe0u, 0xfe0u, 0x610cu, 0xc106u, 0x610cu, 0xfe0u, 0x100u, 0x100u, 0x100u, 0x380u, 0x540u, 0x920u, 0x1010u},
    {0x0u, 0xfe0u, 0x600cu, 0xc006u, 0xe00eu, 0x9ff2u, 0x9112u, 0x9112u, 0x9112u, 0x9112u, 0x9ff2u, 0xf11eu, 0xc006u, 0x600cu, 0x1ff0u, 0x1008u, 0x2008u},
    {0x0u, 0x4000u, 0x200cu, 0x1014u, 0x828u, 0x450u, 0x2a0u, 0xfe0u, 0x7abcu, 0xe54eu, 0xca26u, 0xf41eu, 0x783cu, 0x1fe0u, 0x0u, 0x0u, 0x0u},
    {0x100u, 0x100u, 0x7c0u, 0x420u, 0x420u, 0x810u, 0x810u, 0x1010u, 0x1008u, 0x1008u, 0x2004u, 0x2004u, 0x7ffcu, 0x2008u, 0x3ff8u, 0x0u, 0x0u},
};
static void ub_drum_icon(uint32_t lane, int32_t cx, int32_t y, int32_t px, int32_t hit, uint16_t c)
{
    uint32_t gy, gx; int32_t x = cx - px * 17 / 2;
    y += hit > 100 ? px : 0;
    for (gy = 0; gy < 17u; gy++) for (gx = 0; gx < 17u; gx++)
        if ((UB_DRUM[lane & 7u][gy] >> gx) & 1u)
            cv_rect(x + (int32_t)gx * px, y + (int32_t)gy * px, px, px,
                    gy < 5u ? ux_mix(c, T_TEXT, 35) : c);
}
static void ub_kit(const track_t *t, const int32_t *A, int32_t sel)
{
    /* centers and top edges in a 120 x 72 stage; kick painted last */
    static const uint8_t PX[8] = {60, 30, 25, 10, 108, 86, 31, 60};
    static const uint8_t PY[8] = {36, 38, 0, 20, 1, 32, 38, 0};
    static const uint8_t ORDER[8] = {3,4,2,7,5,1,6,0};
    drum_lane_t *K = drum_kit_of(t);
    uint32_t k, lane; int32_t p = A[2] >= 180 ? 2 : 1;
    for (k = 0; k < 8u; k++) {
        int32_t e, x, y, sz; uint16_t c;
        lane = ORDER[k];
        e = K && K[lane].x.live && !ANIM_OFF() ? clamp(K[lane].x.ea >> 22, 0, 256) : 0;
        x = A[0] + PX[lane] * A[2] / 120; y = A[1] + PY[lane] * A[3] / 72;
        sz = lane == 0u && p == 2 ? 3 : p;
        c = dx_lane_muted(lane) ? T_DIM : heat_col(70 + (int32_t)lane * 11 + e * 95 / 256);
        if ((int32_t)lane == sel) c = T_ACCENT;
        if (lane == 6u && !e && sel != 6) continue; /* rim is a strike on the snare */
        ub_drum_icon(lane, x, y, sz, e, c);
        if (lane == 5u) { /* the two mounted toms over the bass drum */
            ub_drum_icon(5u, A[0] + A[2] * 44 / 120, A[1] + A[3] * 16 / 72, p, e, c);
            ub_drum_icon(5u, A[0] + A[2] * 70 / 120, A[1] + A[3] * 16 / 72, p, e, c);
        }
    }
}

/* a part's colour among the six (the palette's four knobs, then its ACCENT and its TEXT) */
static uint16_t ub_tcol(uint32_t tr) { return tr < 4u ? T_K(tr) : tr == 4u ? T_ACCENT : T_TEXT; }
/* HOME: part t's being about (cx, cy) px, about rad px across its half, its level lv (0 .. 256); off: muted, asleep
 * (dim, still). A formula part's from its formula; a kit's from its first lane's bytebeat */
static void ub_being_at(const track_t *t, int32_t cx, int32_t cy, int32_t rad, int32_t lv, int off)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, fix = 0, fr = off || lv < 8 ? anim_fr / 16u : anim_fr / 3u, gy;
    int32_t px = rad * 2 / 11 < 2 ? 2 : rad * 2 / 11;
    uint16_t c = off ? T_DIM : heat_col(48 + clamp(lv, 0, 256) * 200 / 256), e = off ? T_SURF : T_TEXT;
    const char *s = ub_text(t, &fix);
    gq_work_t *w = ub_ws();
    if (s && ub_compile(w, s)) {
        if (fix)
            ub_portrait(w, cx, cy, rad, fr, c, e);
        else
            ub_sprite(w, cx - px * 11 / 2, cy - px * 11 / 2, px, fr, c, e);
        return;
    }
    if (trk_eng(t) == ENGI_DRUM) {
        int32_t A[4] = {cx - rad, cy - rad, rad * 2, rad * 2};
        ub_kit(t, A, -1); return;
    }
    {                                                   /* (a kit, or a part with no formula: a bytebeat of its own) */
        uint8_t v[11];
        for (gy = 0; gy < 11u; gy++) {
            uint32_t tt = gy * 911u + 2048u + tr * 7919u + fr * 5u;
            v[gy] = (uint8_t)(tt * (3u & tt >> 5) | tt >> (tr + 6u));
        }
        ub_bits(v, 11u, cx - px * 11 / 2, cy - px * 11 / 2, px, fr, lv > 200 ? 1u : 0u, c, e);
    }
}
/* HOME's ground: a plasma in blocks of 6 px under the beings, its swirl quickened by the mix's level (en 0 .. 256),
 * four shades of THEME on SURF, faint */
static void ub_plasma(int32_t bot, int32_t en, uint32_t still)
{
    int32_t x, y;
    uint32_t f = still ? 0u : anim_fr * (2u + (uint32_t)en / 64u);
    uint16_t sh[4];
    for (x = 0; x < 4; x++)
        sh[x] = ux_mix(T_SURF, T_THEME, 2 + x * 3);
    for (y = 0; y < bot + 12; y += 6)
        for (x = 0; x < 240; x += 6) {
            int32_t v = sine_i((uint32_t)(x * 300 + (int32_t)f * 211) << 16) + sine_i((uint32_t)(y * 410 - (int32_t)f * 157) << 16) +
                        sine_i((uint32_t)((x + y) * 190 + (int32_t)f * 97) << 16);   /* (three waves: -98301 .. 98301) */
            v = (v + 98304) * 4 / 196608;
            if (v > 0)
                cv_rect(x, y, 6, 6, sh[v > 3 ? 3 : v]);
        }
}

/* HOME's space: three depths of stars, with no ground or fixed track slots.
 * Motion belongs to eco_at / LIFE, the same physics as JIANT FM. */
static void ub_scene(int32_t bot, int32_t en, uint32_t still)
{
    uint32_t i, sp = still ? 0u : anim_fr * (1u + (uint32_t)en / 48u);
    for (i = 0; i < 48u; i++) {
        uint32_t h = i * 2654435761u, d = 1u + (h >> 30);
        int32_t sx = (int32_t)(((h >> 8) % 240u + 960u - sp * d / 4u % 240u) % 240u);
        int32_t sy = 14 + (int32_t)((h >> 16) % (uint32_t)(bot > 28 ? bot - 28 : 1));
        cv_rect(sx, sy, 1, 1, ux_mix(T_SURF, heat_col(40 + (int32_t)d * 40), 25 + (int32_t)d * 8));
    }
}
