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
    if (p[P_E1] == RW_USER) {
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
/* an INT being from the compiled formula: 11 x 11, its bits at time fr (a pose) */
static void ub_sprite(gq_work_t *w, int32_t x, int32_t y, int32_t px, uint32_t fr, uint16_t c, uint16_t e)
{
    uint32_t tq[11], gy;
    uint8_t v[11];
    for (gy = 0; gy < 11u; gy++)
        tq[gy] = (gy * 1237u + 4096u + fr * 3u) << 8;
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
    uint16_t bc = T_K(tr), ec = T_TEXT;
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
        cv_text(x + wd / 2 - (int32_t)str_len(nm) * 3, y + ht - 9 - AF_X_CAP_Y, &AF_X, nm, bc);
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
        cv_text(x + 42 - (int32_t)str_len(nm) * 3, y + ht - 10 - AF_X_CAP_Y, &AF_X, nm, bc);
        if (trk_engp(t) == &ENG_WAVE)
            ub_terrain(w, x + 92, by, wd - 92, bh, t->p[P_E1]);
        else
            ub_raster(w, x + wd - UB_COLS * 3, by + (bh - UB_ROWS * 3) / 2, anim_fr);
    }
    return 1;
}

/* DRUM-X's kit: its eight lanes as beings, 4 x 2 in A {x, y, w, h}: a formula lane its formula's (DX_FORMS), an
 * oscillator lane a bytebeat of its own (its lane's number in it). A hit squashes it (its envelope, as the colony's)
 * and frames it; the lane picked (sel, or the last struck) its name lit; a muted one dim */
static void ub_kit(const track_t *t, const int32_t *A, int32_t sel)
{
    static uint8_t age[8], last;
    drum_lane_t *K = drum_kit_of(t);
    gq_work_t *w = ub_ws();
    int32_t cw = A[2] / 4, ch = A[3] / 2, px = (cw - 12) / 9 < (ch - 16) / 9 ? (cw - 12) / 9 : (ch - 16) / 9;
    uint32_t l, fr = anim_fr / 3u;
    if (px < 2) px = 2;
    for (l = 0; l < 8u; l++) {
        int32_t x = A[0] + (int32_t)(l & 3u) * cw, y = A[1] + (int32_t)(l >> 2) * ch, sx = x + (cw - px * 9) / 2;
        int32_t e = K && K[l].x.live && !ANIM_OFF() ? clamp(K[l].x.ea >> 22, 0, 256) : 0, mut = dx_lane_muted(l);
        uint32_t f = dx_form[l], gy, sq = e > 120 ? 2u : e > 30 ? 1u : 0u;
        uint8_t v[9];
        uint16_t c = mut ? T_DIM : T_K(l), nc;
        if (K && K[l].age < age[l] && e) last = (uint8_t)l;
        age[l] = K ? (uint8_t)K[l].age : 255u;
        if (f && f <= DX_NFORM && ub_compile(w, DX_FORMS[f - 1u])) {
            uint32_t tq[9];
            for (gy = 0; gy < 9u; gy++)
                tq[gy] = (gy * 911u + 2048u + (e ? fr * 5u : 0u)) << 8;
            memset(w->st, 0, sizeof w->st);
            ub_run(w, tq, 9u, 0, v);
        } else
            for (gy = 0; gy < 9u; gy++) {               /* (an oscillator lane: t * (lane & t >> k), its own) */
                uint32_t tt = gy * 911u + 2048u + l * 7919u + (e ? fr * 5u : 0u);
                v[gy] = (uint8_t)(tt * ((l + 3u) & tt >> (5u + l % 4u)) | tt >> (l + 6u));
            }
        nc = (int32_t)l == (sel >= 0 ? sel : (int32_t)last) ? T_ACCENT : mut ? T_DIM : T_MID;
        if (cw >= 40)                                   /* (its name above it, its formula's beside; small: none) */
            cv_text(x + 4, y + 1 - AF_X_CAP_Y, &AF_X, drum_lane_abbr(t, l), nc);
        if (cw >= 56 && f && f <= DX_NFORM)
            cv_text(x + 20, y + 1 - AF_X_CAP_Y, &AF_X, N_DXFORM[f - 1u], T_DIM);
        ub_bits(v, 9u, sx, y + (cw >= 40 ? 11 : 2) + (int32_t)sq * px, px, e ? fr : 0u, sq, c, T_TEXT);
        if (e > 30)                                     /* (struck: framed, a corner each side) */
            cv_frame(sx - 2, y + (cw >= 40 ? 9 : 0), px * 9 + 4, px * 9 + 4, T_TEXT);
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
    uint16_t c = off ? T_DIM : ub_tcol(tr), e = off ? T_SURF : T_TEXT;
    const char *s = ub_text(t, &fix);
    gq_work_t *w = ub_ws();
    if (s && ub_compile(w, s)) {
        if (fix)
            ub_portrait(w, cx, cy, rad, fr, c, e);
        else
            ub_sprite(w, cx - px * 11 / 2, cy - px * 11 / 2, px, fr, c, e);
        return;
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

/* (JIANT BYTE) HOME's scene (MACROS, LIFE): a starfield drifting left (faster loud), the ground a horizon that is the
 * mix (the scope's last samples), its rows fading under it; the parts stand on it in a row (ui_lzhome.c places them:
 * ub_scene_at), each jumping with its own sound. gy: the horizon's row (returned by ub_scene) */
static int32_t ub_scene(int32_t bot, int32_t en, uint32_t still)
{
    uint32_t i, w0 = scope_w - 240u;
    int32_t gy = bot - 4, x, py = 0;
    uint32_t sp = still ? 0u : anim_fr * (1u + (uint32_t)en / 48u);
    for (i = 0; i < 48u; i++) {                         /* the stars: three depths */
        uint32_t h = i * 2654435761u, d = 1u + (h >> 30);
        int32_t sx = (int32_t)(((h >> 8) % 240u + 240u * 4u - sp * d / 4u % 240u) % 240u), sy = 6 + (int32_t)((h >> 16) % (uint32_t)(gy - 30));
        cv_rect(sx, sy, 1, 1, d == 1u ? T_DIM : d == 2u ? T_MID : T_TEXT);
    }
    for (x = 0; x < 240; x++) {                         /* the ground: the mix as the horizon */
        int32_t v = scope_buf[(w0 + (uint32_t)x) & (SCOPE_N - 1u)], y = gy + (v * 6 >> 15), k;
        if (x && (y - py > 1 || py - y > 1)) cv_rect(x, y < py ? y : py, 1, (y < py ? py - y : y - py) + 1, T_THEME);
        cv_rect(x, y, 1, 1, T_THEME);
        for (k = y + 3; k < bot + 14; k += 3)           /* (its rows fading under it) */
            cv_rect(x, k, 1, 1, ux_mix(T_SURF, T_THEME, 30 - (k - y) * 2 > 4 ? 30 - (k - y) * 2 : 4));
        py = y;
    }
    return gy;
}
/* where part c stands (px): its slot along the ground, its jump with its level (lv 0 .. 256) and its hits */
static void ub_scene_at(uint32_t c, int32_t gy, int32_t rad, int32_t lv, int32_t *x, int32_t *y)
{
    int32_t j = lv > 20 ? (sine_i((anim_fr * (6u + c) * 1500u + c * 9000u) << 16) * (lv / 24 + 2)) >> 15 : 0;
    *x = 20 + (int32_t)c * 40;
    *y = gy - rad - 4 - (j < 0 ? -j : j);
}
