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
    if (!ENG_IS_MATH(trk_engp(t)))
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
/* ---- (BYTE, the owner's references: the census, the flight recorder) the engines' panels as instruments ---- */
#define HUD_BL 0x29FFu
#define HUD_YE 0xEF8Bu
#define HUD_N 68u
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c);   /* (ui_graph.c) */
static uint16_t vk_heat(int32_t v);                     /* (ui_engvis.c) */
static uint8_t hud_q[8][HUD_N / 2u] __attribute__((section(".pool")));   /* (the traces' history: 8 lanes, swept, 4 bits a point) */
static uint32_t hud_get(uint32_t k, uint32_t i) { return (uint32_t)(hud_q[k][i >> 1] >> ((i & 1u) * 4u)) & 15u; }
static void hud_put(uint32_t k, uint32_t i, uint32_t v)
{
    uint8_t *q = &hud_q[k][i >> 1];
    *q = (uint8_t)((*q & ~(15u << ((i & 1u) * 4u))) | ((v > 15u ? 15u : v) << ((i & 1u) * 4u)));
}
static uint8_t hud_x, hud_who = 0xFF;
static uint32_t hud_fr;
static int hud_tick(uint32_t who)                       /* the sweep a step on (1: a new frame); a new part: cleared */
{
    if (hud_who != who) { memset(hud_q, 0, sizeof hud_q); hud_who = (uint8_t)who; hud_x = 0; }
    if (anim_fr == hud_fr) return 0;
    hud_fr = anim_fr; hud_x = (uint8_t)((hud_x + 1u) % HUD_N);
    return 1;
}
static int32_t hud_tag(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg)
{
    int32_t w = text_w(&AF_X, s) + 4;
    cv_rect(x, y, w, 11, bg);
    cv_text_on(x + 2, y + 1 - AF_X_CAP_Y, &AF_X, s, fg, bg);
    return w;
}
static void hud_lane(int32_t x, int32_t y, int32_t w, int32_t h)   /* a lane's frame: its rules, a dotted middle, ticks */
{
    int32_t i;
    cv_rect(x, y, w, 1, T_SURF); cv_rect(x, y + h - 1, w, 1, T_SURF);
    for (i = x; i < x + w; i += 3) cv_rect(i, y + h / 2, 1, 1, T_LINE);
    for (i = x; i < x + w; i += w / 6) cv_rect(i, y + h - 4, 1, 3, T_DIM);
}
static void hud_cursor(int32_t x, int32_t y, int32_t h, int32_t v)   /* the yellow cursor, its value (0 .. 99) beside */
{
    char q[4];
    cv_rect(x, y, 1, h, HUD_YE); cv_rect(x - 2, y, 5, 1, HUD_YE); cv_rect(x - 1, y + 1, 3, 1, HUD_YE);
    v = clamp(v, 0, 99); q[0] = (char)('0' + v / 10); q[1] = (char)('0' + v % 10); q[2] = 0;
    cv_text_on(x + 3, y + h - 11 - AF_X_CAP_Y, &AF_X, q, HUD_YE, T_BG);
}
/* lane k's history in (x, y) w x h, 0 .. 255 bottom .. top, the sweep's gap; ticks where it rose (a hit) */
static void hud_trace(uint32_t k, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int32_t th, int ticks)
{
    uint32_t i;
    int32_t px = 0, py = 0;
    for (i = 0; i < HUD_N; i++) {
        int32_t xx = x + (int32_t)i * w / (int32_t)HUD_N, yy = y + h - 2 - (int32_t)hud_get(k, i) * (h - 4) / 15;
        if (i && (i <= hud_x || i > hud_x + 3u)) cv_line_t(px, py, xx, yy, c, th);
        if (ticks && i && hud_get(k, i) > hud_get(k, i - 1u) + 3u) cv_rect(xx, y + h - 4, 2, 3, T_TEXT);
        px = xx; py = yy;
    }
}
/* MATH: CENSUS (the IPv4 map's): 256 of its bytes along a Hilbert curve, each its heat (a hard curve: the low ones
 * near black), the window running with t; a cell whose top bit turned since the last frame flashes white; a scan head
 * running it. Beside (the owner: OSCILLOSCOPE) the wave those bytes make, its least and most */
static void hud_hil(uint32_t d, int32_t *x, int32_t *y)   /* d along a 16 x 16 Hilbert curve */
{
    uint32_t s, rx, ry, t = d;
    int32_t X = 0, Y = 0, k;
    for (s = 1; s < 16u; s <<= 1) {
        rx = 1u & (t >> 1); ry = 1u & (t ^ rx);
        if (!ry) { if (rx) { X = (int32_t)s - 1 - X; Y = (int32_t)s - 1 - Y; } k = X; X = Y; Y = k; }
        X += (int32_t)(s * rx); Y += (int32_t)(s * ry); t >>= 2;
    }
    *x = X; *y = Y;
}
static uint8_t hud_pb[32];                              /* (each cell's top bit, the frame before: the flashes) */
static void hud_census(gq_work_t *w, int32_t x, int32_t y, uint32_t t0, uint16_t tc)
{
    uint32_t tq[64], i, j, hd = anim_fr % 256u, mn = 255, mx = 0;
    uint8_t v[256];
    int32_t cx, cy, px = 0, py = 0;
    char b[12];
    memset(w->st, 0, sizeof w->st);
    for (i = 0; i < 256u; i += 64u) {
        for (j = 0; j < 64u; j++) tq[j] = (t0 + (i + j) * 8u) << 8;
        ub_run(w, tq, 64u, 0, v + i);
    }
    for (i = 0; i < 256u; i++) {
        uint32_t q = v[i], hi = q >> 7, was = (hud_pb[i >> 3] >> (i & 7u)) & 1u;
        hud_hil(i, &cx, &cy);
        cv_rect(x + cx * 6, y + cy * 6, 5, 5, hi != was ? T_TEXT : q < 24u ? T_SURF : vk_heat(24 + (int32_t)(q * q / 255u) * 231 / 255));
        hud_pb[i >> 3] = (uint8_t)((hud_pb[i >> 3] & ~(1u << (i & 7u))) | hi << (i & 7u));
        if (q < mn) mn = q;
        if (q > mx) mx = q;
    }
    hud_hil(hd, &cx, &cy);                              /* the scan head */
    cv_rect(x + cx * 6 - 2, y + cy * 6 - 2, 9, 1, T_TEXT); cv_rect(x + cx * 6 - 2, y + cy * 6 + 6, 9, 1, T_TEXT);
    cv_rect(x + cx * 6 - 2, y + cy * 6 - 2, 1, 9, T_TEXT); cv_rect(x + cx * 6 + 6, y + cy * 6 - 2, 1, 9, T_TEXT);
    {   /* beside: the oscilloscope */
        int32_t rx = x + 104, rw = 128, top = y + 14, h = 70;
        hud_tag(rx, y, "OUT", 0x0000, tc);
        hud_lane(rx, top, rw, h);
        for (i = 0; i < 256u; i++) {                    /* (every byte: two a pixel) */
            int32_t xx = rx + (int32_t)(i >> 1), yy = top + h - 2 - (int32_t)v[i] * (h - 4) / 255;
            if (i) cv_line(px, py, xx, yy, tc);
            px = xx; py = yy;
        }
        hud_cursor(rx + (int32_t)(hd >> 1), top, h, (int32_t)v[hd] * 99 / 255);
        str_cpy(b, "MIN ", sizeof b); fmt_int(b + 4, (int32_t)mn);
        cv_text_on(rx, top + h + 4 - AF_X_CAP_Y, &AF_X, b, T_MID, T_BG);
        str_cpy(b, "MAX ", sizeof b); fmt_int(b + 4, (int32_t)mx);
        cv_text_on(rx + 64, top + h + 4 - AF_X_CAP_Y, &AF_X, b, T_MID, T_BG);
    }
}
/* WAVE (the owner: PHASE ORBIT): its cycle at POS against itself a quarter turn on, an orbit (a Lissajous) that FOLD,
 * WARP and the rest bend; POS's neighbours faint; a satellite running it. Beside: POS, and the mix's scope */
static void hud_wave(const track_t *t, gq_work_t *w, int32_t x, int32_t y, int32_t wd, int32_t ht, uint16_t tc)
{
    enum { NP = 64u };
    uint32_t tq[NP], i, k, pos = (uint32_t)clamp(t->p[P_E1], 0, 127), si = anim_fr % NP;
    uint8_t v[NP];
    int32_t R = ht / 2 - 4, cx = x + R + 8, cy = y + ht / 2, rx = x + 2 * R + 22;
    char b[12];
    (void)wd;
    for (i = 0; i < 48u; i += 2u)                       /* its range, dotted; the axes */
        cv_pset(cx + (R * og_cos(i * 1365u) >> 15), cy + (R * og_sin(i * 1365u) >> 15), T_DIM);
    cv_rect(cx - R - 4, cy, 2 * R + 9, 1, T_SURF);
    cv_rect(cx, cy - R - 4, 1, 2 * R + 9, T_SURF);
    for (k = 0; k < 3u; k++) {                          /* POS's neighbours, then its own */
        int32_t pp = clamp((int32_t)pos + (k == 0u ? -24 : k == 1u ? 24 : 0), 0, 127), px = 0, py = 0;
        for (i = 0; i < NP; i++) tq[i] = i * 8192u / NP;
        memset(w->st, 0, sizeof w->st);
        ub_run(w, tq, NP, (uint32_t)pp * 516u, v);
        for (i = 0; i <= NP; i++) {
            int32_t xx = cx + ((int32_t)v[i % NP] - 128) * R / 128, yy = cy - ((int32_t)v[(i + NP / 4u) % NP] - 128) * R / 128;
            if (i) cv_line_t(px, py, xx, yy, k == 2u ? tc : ux_mix(T_BG, HUD_BL, 60), k == 2u ? 2 : 1);
            px = xx; py = yy;
        }
        if (k == 2u) {                                  /* the satellite */
            int32_t sx = cx + ((int32_t)v[si] - 128) * R / 128, sy = cy - ((int32_t)v[(si + NP / 4u) % NP] - 128) * R / 128;
            cv_rect(sx - 2, sy - 2, 5, 5, HUD_YE);
        }
    }
    hud_tag(rx, y + 2, "PHASE ORBIT", 0x0000, tc);
    str_cpy(b, "POS ", sizeof b); fmt_int(b + 4, (int32_t)pos * 100 / 127); str_cpy(b + str_len(b), "%", 2);
    cv_text_on(rx, y + 20 - AF_X_CAP_Y, &AF_X, b, HUD_YE, T_BG);
    cv_text_on(rx, y + 32 - AF_X_CAP_Y, &AF_X, "X  THE CYCLE", T_MID, T_BG);
    cv_text_on(rx, y + 43 - AF_X_CAP_Y, &AF_X, "Y  +90 DEG", T_MID, T_BG);
    hud_lane(rx, y + ht - 34, x + wd - rx, 30);
    cv_text_on(rx, y + ht - 46 - AF_X_CAP_Y, &AF_X, "MIX", T_MID, T_BG);
    scope_at(rx, x + wd - rx, y + ht - 19, 12, T_TEXT);
}
/* (func-tame, the owner: "I want a bit more idea of what is going on") FUNC: its program read at a moment of the song,
 * on the UI's side (the work space's copy, as CENSUS): one sample, the sounds not made, what it asks for kept: kick()
 * snare() hat()'s age, eu()'s answer (the recipes' action), the first note()'s or chord()'s pitch, the first table's
 * pick. The audio is not looked into; the same text, the same knobs, the same P */
typedef struct { uint32_t age[3], eu[3], note, tab; uint8_t neu, has; } fpeek_t;
static void func_peek(const fvm_prog_t *p, const uint32_t *u, uint32_t P, uint32_t tq, fpeek_t *o)
{
    uint32_t stack[FVM_DEPTH], local[10] = {0}, sp = 0, pc, zero = 0, T = (uint32_t)((uint64_t)tq * 65536u / FS);
    const uint32_t *ar[FA_N] = {0};
    ar[FA_T] = &tq; ar[FA_W] = &zero; ar[FA_TS] = &T; ar[FA_BEAT] = &P;
    o->neu = 0; o->has = 0;
    for (pc = 0; pc < p->n; pc++) {
        const fvm_ins_t *in = &p->ins[pc];
        uint32_t op = in->op, b = in->k, a;
        if (op == FO_SAVE) { local[b % 10u] = stack[--sp]; continue; }
        if (in->md == FM_ARR) b = b < FA_N && ar[b] ? ar[b][0] : 0;
        else if (in->md == FM_UNI) b = u[b];
        else if (in->md == FM_LOCAL) b = local[b % 10u];
        else if (in->md == FM_STATE) b = 0;
        if (op == FO_PUSH) { if (sp < FVM_DEPTH) stack[sp++] = b; continue; }
        if ((FO_ISBIN(op) || op == FO_LP || op == FO_HP) && in->md == FM_STK) b = stack[--sp];
        if (!sp) return;
        a = stack[sp - 1u];
        if (FO_ISBIN(op)) {
            stack[sp - 1u] = fvm_op2(op, a, b);
            if (op == FO_EU && o->neu < 3u) o->eu[o->neu++] = stack[sp - 1u] & 1u;
        } else if (FO_ISUN(op)) stack[sp - 1u] = fvm_op1(op, a);
        else if (op == FO_MUSIC) {
            if (in->k < 2u) { sp -= 2u; if (!(o->has & 8u)) o->note = stack[sp - 1u]; o->has |= 8u; }
            else { o->age[in->k - 2u] = a; o->has |= 1u << (in->k - 2u); }
            stack[sp - 1u] = 0;
        } else if (op == FO_SEL) { uint32_t y = stack[--sp], x = stack[--sp]; stack[sp - 1u] = stack[sp - 1u] ? x : y; }
        else if (op == FO_TAB) { stack[sp - 1u] = p->pool[(in->k >> 8) + a % (in->k & 255u)]; if (!(o->has & 16u)) { o->tab = stack[sp - 1u]; o->has |= 16u; } }
        else if (op == FO_LP || op == FO_HP || op == FO_PH) stack[sp - 1u] = 0;
    }
}
/* (funcplay, the owner's mock-up B, "the map on a touch") FUNC's keys (funcplay.c func_kb, by clay.c CLAY_KMAP): each
 * one's name and what it does (the fills and the mutations while held), in order (the whites 0..11, the blacks 16..23, the root's 32..38; 9..11 none) */
static const char FK_T[] = "INTRO\0NO DRUMS\0GROOVE\0AS WRITTEN\0BUILD\0RISING\0DROP\0DENSER\0BREAK\0NO DRUMS\0"
    "ROLL\0RATCHETS\0STUT\0REPEAT 1/8\0REV\0BACKWARDS\0DBL\0DOUBLE TIME\0\0\0\0\0\0\0"
    "NOTES\0NEW WHILE HELD\0RHYTHM\0NEW WHILE HELD\0SWAP\0A FOR B WHILE HELD\0FREEZE\0BAR AGAIN, HELD\0RESET\0ALL BACK\0"
    "DRUMS\0MUTE\0NOTES\0MUTE\0B\0MUTE\0I\0\0II\0\0III\0\0IV\0\0V\0\0VI\0\0VII";   /* (the root's: "ROOT", fk_map) */
/* the groups' colours: sections, fills, (none), mutations, mutes, the root */
static const uint16_t FK_COL[6] = {HUD_YE, 0xF9D6u, 0, 0x3E9Fu, 0x7FF3u, 0xA47Fu};
/* key k's name (by CLAY_KMAP; its words after it), *g its group (ui_layer.c fk_kf, fk_map) */
static const char *fk_txt(uint32_t k, uint32_t *g)
{
    uint32_t c = CLAY_KMAP[k];                         /* (k < 27) */
    c = c < 12u ? c : c < 24u ? c - 4u : c - 12u;
    *g = (c >= 5u) + (c >= 9u) + (c >= 12u) + (c >= 17u) + (c >= 20u);
    return str_nth(FK_T, c * 2u);
}
/* ui_layer.c: the keyboard drawn, each key by a pk_kf_t (its fill; its name and ink); FUNC's keys on it */
typedef uint16_t (*pk_kf_t)(uint32_t a, int blk, uint32_t i, char *nm, uint16_t *ink);
static int fk_map(const track_t *t, int32_t x, int32_t y, int32_t wd, int32_t ht);
/* the bar now: 16 steps (the one playing outlined), a lane for each drum the program calls and each eu() action, the
 * note (or the table's pick: the recipes' w * x), the four knobs by their FORM's names */
static void hud_func(const track_t *t, gq_work_t *w, int32_t x, int32_t y, int32_t wd, int32_t ht, uint16_t tc)
{
    static const char *const LN[6] = {"KICK", "SNARE", "HAT", "ACT", "ACT2", "ACT3"};
    uint32_t u[FU_N] = {0}, i, k, pos = 0, live = 0, lanes = 0, hit[6] = {0}, bps = (uint32_t)FS * 60u / (uint32_t)clamp(song.g[G_BPM], 20, 300);
    uint32_t form = func_form(t);
    int32_t lx = x + 34, cw = (wd - 36) / 16, row = 12, yy;
    fpeek_t pk = {{0}, {0}, 0, 0, 0, 0}, now = pk;
    char b[24];
    if (cw < 4 || fk_map(t, x, y, wd, ht)) return;      /* (funcplay) a key held: the keys' map */
    for (k = 0; k < 4u; k++) { uint32_t a = (uint32_t)clamp(t->p[P_E2 + k], 0, 127); u[FU_A + k] = a * 2u; u[FU_AF + k] = a * 65536u / 127u; }
    u[FU_NOTE] = (uint32_t)clamp(60 + t->p[P_E6], 0, 127); u[FU_VEL] = 100; u[FU_BPS] = bps;
    if (func_bar(t)) { pos = func_clock_end; live = 1; }
    else for (i = 0; i < NVOICE; i++)
        if (t->v[i].active) { pos = (uint32_t)(((uint64_t)(uint32_t)t->v[i].s[2] << 16) / bps); u[FU_NOTE] = t->v[i].note; live = 1; }
    pos &= 0x7fffffffu;
    for (k = 0; k < 16u; k++) {                         /* a step's start (and a hair): what begins there */
        uint32_t P = (pos & ~0x3FFFFu) + k * 16384u + 64u;
        func_peek(&w->pg.p, u, P, (uint32_t)((uint64_t)P * bps >> 16), &pk);
        for (i = 0; i < 3u; i++) if ((pk.has >> i & 1u) && pk.age[i] < 1024u) hit[i] |= 1u << k;
        for (i = 0; i < pk.neu; i++) if (pk.eu[i]) hit[3u + i] |= 1u << k;
        lanes |= (pk.has & 7u) | ((1u << pk.neu) - 1u) << 3;
    }
    func_peek(&w->pg.p, u, pos, (uint32_t)((uint64_t)pos * bps >> 16), &now);
    b[0] = 0; str_cpy(b, "BAR ", sizeof b); fmt_int(b + 4, (int32_t)(pos >> 18) + 1);
    str_cpy(b + str_len(b), " STEP ", sizeof b - str_len(b)); fmt_int(b + str_len(b), (int32_t)(pos >> 14 & 15u) + 1);
    hud_tag(x, y, N_FUNC_FORM[form], 0x0000, tc);       /* (BYTE 0.17) the FORM by its name, its RUN at the right */
    cv_text_on(x + text_w(&AF_X, N_FUNC_FORM[form]) + 8, y + 2 - AF_X_CAP_Y, &AF_X, b, live ? T_TEXT : T_DIM, T_BG);
    cv_text_r(x + wd, y + 2 - AF_X_CAP_Y, &AF_X, N_FUNC_RUN[clamp(t->p[P_E0], 4, 6)], live ? HUD_YE : T_MID, T_BG);
    yy = y + 14;
    for (k = 0; k < 16u; k++)                           /* the steps: the beats a little brighter */
        cv_rect(lx + (int32_t)k * cw, yy, cw - 1, 3, k % 4u ? T_SURF : T_MID);
    yy += 5;
    for (i = 0; i < 6u && yy + row <= y + ht - 34; i++) {
        if (!(lanes >> i & 1u)) continue;
        cv_text_on(x, yy + 1 - AF_X_CAP_Y, &AF_X, form == 13u && i >= 3u ? LN[i - 3u] : LN[i], T_MID, T_BG);   /* (DRUMS: its
                                                         * three eu() its kick, snare and hats) */
        for (k = 0; k < 16u; k++)
            cv_rect(lx + (int32_t)k * cw, yy, cw - 1, row - 2, hit[i] >> k & 1u ? tc : T_SURF);
        yy += row;
    }
    if (live) {                                         /* the step playing */
        int32_t cx = lx + (int32_t)(pos >> 14 & 15u) * cw;
        cv_rect(cx - 1, y + 12, cw + 1, 1, HUD_YE); cv_rect(cx - 1, yy, cw + 1, 1, HUD_YE);
        cv_rect(cx - 1, y + 12, 1, yy - y - 12, HUD_YE); cv_rect(cx + cw - 1, y + 12, 1, yy - y - 12, HUD_YE);
    }
    {   /* (funcplay) the part played live: its SECTION (lit when not GROOVE), the one asked, the FILL held, the mutations
         * and the mutes (a letter each, a dot when off: NOTES RHYTHM SWAP FREEZE, DRUMS NOTES B), layer B */
        const fp_t *fs = &fp_st[(uint32_t)(t - trk) % NTRK];   /* (the names: FK_T's, INTRO and GROOVE swapped) */
        uint32_t sc = fs->sec % 5u, rq = (fs->sreq - 1u) % 5u;
        int32_t ry = y + ht - 32, sx = x + hud_tag(x, ry, str_nth(FK_T, (sc ^ (sc < 2u)) * 2u), 0x0000, fs->sec ? HUD_YE : T_MID) + 4;
        char m[40];
        uint32_t n = 0;
        if (fs->sreq) { m[n++] = '>'; str_cpy(m + n, str_nth(FK_T, (rq ^ (rq < 2u)) * 2u), 8); n = str_len(m); m[n++] = ' '; }
        str_cpy(m + n, str_nth(FK_T, fs->fill % 5u ? fs->fill % 5u * 2u + 8u : 18u), 6); n = str_len(m); m[n++] = ' ';
        for (k = 0; k < 7u; k++) m[n++] = (k < 4u ? fs->mut >> k : fs->mute >> (k - 4u)) & 1u ? "NRSFDNB"[k] : '\xb7';
        m[n++] = ' ';
        str_cpy(m + n, N_FUNC_FB[clamp(t->p[P_FFB], 0, 20)] + (t->p[P_FFB] > 0 ? 0 : 3), 12);   /* (B OFF: none) */
        cv_text_on(sx, ry + 1 - AF_X_CAP_Y, &AF_X, m, fs->fill | fs->mut | fs->mute | fs->sreq ? HUD_YE : T_MID, T_BG);
    }
    yy = y + ht - 20;
    if (now.has & 8u) { str_cpy(b, "NOTE ", sizeof b); note_name(b + 5, clamp((int32_t)now.note, 0, 127)); }
    else if ((now.has & 16u) && (0x80E68u >> form & 1u)) {   /* (GROWL ACID REESE JUST PLUCK NIBBLE MINIMAL: w * x / 4, the table's
                                                         * pick a harmonic, as a ratio) */
        int32_t q = ((int32_t)now.tab - 48) * 25;
        str_cpy(b, "TONE X", sizeof b); fmt_int(b + 6, q / 100); str_cpy(b + str_len(b), ".", 2); fmt_int(b + str_len(b), q % 100 / 10);
        fmt_int(b + str_len(b), q % 10);
    } else { str_cpy(b, "KEY ", sizeof b); note_name(b + 4, u[FU_NOTE]); }
    cv_text_on(x, yy - AF_X_CAP_Y, &AF_X, b, live ? HUD_YE : T_MID, T_BG);
    for (k = 0; k < 4u; k++) {                          /* the knobs by their names, as on their cards */
        str_cpy(b, FUNC_KN[form][k], sizeof b); str_cpy(b + str_len(b), " ", 2);
        fmt_int(b + str_len(b), clamp(t->p[P_E2 + k], 0, 127) * 100 / 127);
        cv_text_on(x + (int32_t)(1u + k % 2u) * (wd / 3), yy + (int32_t)(k / 2u) * 10 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_BG);
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
        cv_text_r(x + wd - 2, y + 13 - AF_F_CAP_Y, &AF_F, b, age ? T_THEME : T_DIM, T_BG);
    }
    ub_text_draw(x + 2, y + 2, s, (uint32_t)(wd - 120) / 6u, 2u, T_TEXT);
    {   /* (BYTE) its name in its colour, at the top right; under: CENSUS (MATH) or TELEMETRY (WAVE) */
        uint16_t tc = TRK_COL[tr % 6u];
        const char *en = trk_engp(t)->name;              /* (the owner: its engine, clear; was a made-up name) */
        int32_t tw = text_w(&AF_X, en) + 4;
        hud_tag(x + wd - tw, y + 1, en, tr % 6u >= 4u ? 0xFFFFu : 0x0000u, tc);
        (void)ec; (void)bob; (void)bc;
        if (trk_engp(t) == &ENG_WAVE)
            hud_wave(t, w, x, y + 22, wd, ht - 22, tc);
        else {
            uint32_t i, tt = 0, age = 0;
            for (i = 0; i < NVOICE; i++)
                if (t->v[i].active && t->v[i].age >= age) { age = t->v[i].age; tt = (uint32_t)t->v[i].s[2]; }
            if (t->p[P_E1] == RW_FUNC)                  /* (func-tame) FUNC: the bar, its lanes, the note, the knobs */
                hud_func(t, w, x + 2, y + 20, wd - 4, ht - 20, tc);
            else
                hud_census(w, x + 2, y + 20, age ? tt : anim_fr * 64u, tc);
        }
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
    if (A[2] >= 160 && K) {                             /* (BYTE: TELEMETRY) the eight sounds a trace each, swept */
        uint32_t tr = (uint32_t)(t - trk) % NTRK, nw = hud_tick(0x40u | tr);
        int32_t lh = (A[3] - 2) / 8, lx = A[0] + 34, lw = A[2] - 36, cx = lx + (int32_t)hud_x * lw / (int32_t)HUD_N;
        for (lane = 0; lane < 8u; lane++) {
            int32_t e = K[lane].x.live && !ANIM_OFF() ? clamp(K[lane].x.ea >> 22, 0, 255) : 0, ly = A[1] + (int32_t)lane * lh;
            int mu = dx_lane_muted(lane), on = (int32_t)lane == sel;
            if (nw) hud_put(lane, hud_x, (uint32_t)e >> 4);
            hud_lane(lx, ly, lw, lh - 1);
            hud_tag(A[0], ly + (lh - 12) / 2, drum_lane_abbr(t, lane), e > 100 ? 0x0000 : mu ? T_DIM : on ? 0x0000 : T_INK,
                    e > 100 ? HUD_YE : mu ? T_SURF : on ? T_INK : HUD_BL);
            hud_trace(lane, lx, ly, lw, lh - 1, mu ? T_DIM : on ? T_INK : lane == 0u ? TRK_COL[tr % 6u] : HUD_BL, on ? 2 : 1, 0);
        }
        cv_rect(cx, A[1], 1, lh * 8 - 1, HUD_YE);
        cv_rect(cx - 2, A[1], 5, 1, HUD_YE);
        return;
    }
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

