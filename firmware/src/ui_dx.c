/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): EDIT > SOUND and SOUND 2, a sound of the DRUM-X kit (docs/TONIC-DRUMX.md). */
/* Shown in the EDIT family on a DRUM track (ui.c page_visible). They edit the section's kit
 * (drumx_voice.c dx_kit, saved with the section: project.c FUNA):
 *   SOUND    LANE (the sound: BD .. CB; a key struck on the track picks its lane too), SIDE (patch A or B), PTCH, PMOD
 *   SOUND 2  DCAY, NOIS, COLR, MODE (the oscillator's wave, the noise filter, SNAP: 32 combinations; V10.7.8 the
 *            filter GR, the guiro's scrape: PMOD its SCRAPE, the stroke's length)
 *   SOUND 3  (JIANT 0.4) LANE again, PMOD's mode (DECAY LONG NOISE SINE), DRIVE (OFF / ON), the wave alone
 * The values are the side's; the MORPH plays between the two (EDIT 2 MRPH). The panel shows the kit's specimen
 * with the sound's organ singled out (ui_plates.c graph_p_drum: the iris, its node lit). */
static uint8_t dx_ui_seen = 0xFFu;                   /* the last key note followed */
#define DX_LANE_ABBR (DRUM_AB + 1)                       /* (eumel, flash: eng_drum.c's table) */

/* a key struck on the track (seq.c key_on: last_note) picks the sound it plays */
static void dx_follow(void)
{
    if (last_note != dx_ui_seen) {
        if (dx_ui_seen != 0xFFu)                     /* (the first look only takes note of it) */
            dx_ui_lane = (uint8_t)drum_note_lane(TSEL,last_note);
        dx_ui_seen = last_note;
    }
}


/* (JIANT 0.6.54) the pages, the whole kit first, then a sound: KIT (EDIT 1), KIT 2 (EDIT 2), KIT 3 (TONE's BP and FEEDBK,
 * IMAGE: dx_kit3_*), X-MOD, FILTER, LEVELS; then SOUND 1..4, KNOB 1 the sound on each:
 *   SOUND    LANE SIDE PITCH PMOD          SOUND 3  LANE MODE PM.TYPE DRIVE
 *   SOUND 2  LANE DECAY NOISE COLOR        SOUND 4  LANE LAYER FEEDBK SEND (the bus and how much on one knob) */
static void dw_knob(uint32_t pn, uint32_t k, int32_t s);
static void dw_cards(uint32_t pn);
static void dx_lane_turn(int32_t s) { dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (s > 0 ? 1 : -1), 0, 7); }
/* KNOB k (0..3) turned s on a SOUND page or X-MOD */
static __attribute__((noinline)) void dx_knob(const page_t *pg, uint32_t k, int32_t s)
{
    uint32_t pn = dx_pagen(pg);
    if (pn >= 20u || !BYTE_DX_OLD) { dw_knob(pn, k, s); return; }   /* (DRUM-W's own pages; KIT LFO in no chain) */
    if (pn == 4u) {                                     /* (JIANT 0.5.1) KIT LFO: the kit's, not a sound's */
        static const uint8_t MX[4] = {8, 127, 4, 127};
        int32_t v0 = dx_mot[k & 3u];
        dx_mot[k & 3u] = (uint8_t)clamp(v0 + (MX[k & 3u] > 8 ? accel(EN_K1 + k, s, 127) : (s > 0 ? 1 : -1)), 0, MX[k & 3u]);
    }
}
/* KIT 3 (the whole kit): KNOB 1 BP, 2 FEEDBK (TONE's, dx_tone), (0.7.9) 3 HANDS (the CLAP's, dx_hands), (0.9.9-snd1)
 * 4 GLUE (the bus's, eng_drum.c dx_glue: KIT 3's free knob) */
static __attribute__((unused)) void dx_kit3_knob(uint32_t k, int32_t s)   /* (KIT 3, KIT 4: in no chain, ui.c BYTE_DX_OLD) */
{
    if(cur_page()->title[4]=='4') {                    /* KIT 4: KNOB 1 the KEY MAP (V10.6o: KNOB 1's ACOUST gone; codex/
                                                         * coherence: was KNOB 2) */
        if(!k) TSEL->p[P_DKEYMODE]=(int16_t)clamp(TSEL->p[P_DKEYMODE]+(s>0?1:-1),0,9);   /* (V10.6r) 2..9: OCT -1 a lane's chromatic run */
        return;
    }
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    if (k == 0u) bp = (uint32_t)clamp((int32_t)bp + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 2u) dx_hands = (uint8_t)clamp((int32_t)dx_hands + (s > 0 ? 1 : -1), 0, 15);
    else dx_glue = (uint8_t)clamp((int32_t)dx_glue + (s > 0 ? 1 : -1), 0, 15);
    dx_tone = (uint8_t)(bp | fb << 4);
}
static __attribute__((noinline, unused)) void dx_kit3_cards(void)
{
    if(cur_page()->title[4]=='4') {
        empty_column(1);
        hot_unit_column(0,"KEY MAP",drum_ext(TSEL)?"EXT":"CLASS",!drum_ext(TSEL)?"":drum_chrom(TSEL)<0?"K/T":drum_lane_abbr(TSEL,(uint32_t)drum_chrom(TSEL)&7u));
        empty_column(2);
        empty_column(3);
        return;
    }
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    char val[12];
    if (bp) {                                           /* (its centre, as the filter sets it) */
        uint32_t hz = CUTOFF_HZ[78u + bp * 2u];
        if (hz < 1000u) fmt_int(val, (int32_t)hz);
        else fmt_fix(val, (int32_t)(hz / 100u), 1);
    } else
        str_cpy(val, "OFF", sizeof val);
    draw_column(0, "METAL", val, bp ? (CUTOFF_HZ[78u + bp * 2u] < 1000u ? "Hz" : "kHz") : "", bp ? VAL(0u) : T_DIM,
                (int32_t)bp * 1000 / 15, ICON_TONE);
    (void)fb;
    empty_column(1);                                    /* (DRUM-W: FEEDBK gone with DRUM-X's oscillator) */
    fmt_int(val, (int32_t)dx_hands);                    /* (0.7.9) the CLAP's HANDS */
    gauge_column(2, "HANDS", dx_hands ? val : "OFF", dx_hands ? VAL(2u) : T_DIM, (int32_t)dx_hands * 1000 / 15);
    fmt_int(val, (int32_t)dx_glue);                     /* (0.9.9-snd1) the bus's GLUE */
    gauge_column(3, "GLUE", dx_glue ? val : "OFF", dx_glue ? VAL(3u) : T_DIM, (int32_t)dx_glue * 1000 / 15);
}

/* the four cards of a SOUND page or X-MOD */
static void dx_cards(const page_t *pg)
{
    uint32_t pn = dx_pagen(pg);
    char val[12];
    dx_follow();
    if (pn >= 20u || !BYTE_DX_OLD) { dw_cards(pn); return; }   /* (DRUM-W's own pages; KIT LFO in no chain) */
    {                                                   /* (JIANT 0.5.1) KIT LFO */
        static const char *const RT[9] = {"OFF", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
        static const char *const SH[5] = {"SINE", "TRI", "SAW", "RAMP", "S&H"};
        gauge_column(0, "RATE", RT[dx_mot[XM_RATE] % 9u], dx_mot[XM_RATE] ? VAL(0u) : T_DIM, dx_mot[XM_RATE] * 125);
        fmt_int(val, dx_mot[XM_DPTH]);
        gauge_column(1, "DEPTH", val, dx_mot[XM_DPTH] ? VAL(1u) : T_DIM, dx_mot[XM_DPTH] * 1000 / 127);
        hot_gauge_column(2, "SHAPE", SH[dx_mot[XM_SHPE] % 5u], dx_mot[XM_SHPE] * 250);
        fmt_int(val, dx_mot[XM_RAND]);
        gauge_column(3, "RAND", val, dx_mot[XM_RAND] ? VAL(3u) : T_DIM, dx_mot[XM_RAND] * 1000 / 127);
    }
}
/* what the cards and the panel show changed */
static uint32_t dx_sig(void)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint32_t h = (uint32_t)dx_ui_lane * 31u + dx_ui_side * 977u + L->mode * 131u, i;
    h ^= (uint32_t)dx_mot[0] << 24 ^ (uint32_t)dx_mot[1] << 16 ^ (uint32_t)dx_mot[2] << 8 ^ dx_mot[3];   /* (X-MOD) */
    h = (h ^ (uint32_t)TSEL->p[P_DKEYMODE] << 24 ^ dx_tone ^ (uint32_t)dx_hands << 8 ^ (uint32_t)dx_glue << 12) * 2654435761u;   /* (TONE, HANDS, GLUE) */
    for (i = 0; i < DV_NLANE; i++)                      /* (0.6.48) the SENDs, (0.6.49) the LAYERs */
        h = (h ^ dx_snd[i] ^ (uint32_t)dx_lay[i] << 8 ^ (uint32_t)dx_form[i] << 16) * 16777619u;   /* (JIANT BYTE: FORM) */
    for (i = 0; i < sizeof dx_fmod; i++) h = (h ^ ((const uint8_t *)dx_fmod)[i]) * 16777619u;
    for (i = 0; i < DXP_N; i++)
        h = (h ^ (uint32_t)L->a[i] << 8 ^ L->b[i]) * 16777619u;
    for (i = 0; i < sizeof(dw_lane_t); i++)                /* (DRUM-W) the pad's tables and motion */
        h = (h ^ ((const uint8_t *)&dw_kit[dx_ui_lane & 7u])[i]) * 16777619u;
    return h;
}



/* (V10.6z6) EDIT > SWARM, SWARM 2 on a SWARM track (eng_input.c sw_sv): KNOB 1 SIDE (FAR / NEAR, as SOUND's A / B), then
 * the side's values: RATE MOVE TONE, NOISE MOD; SWARM 2's KNOB 4 NOW, where the morph is (0 FAR .. 128 NEAR), named by
 * what gives it (PROX, AIR, SELF: none). In the Bluetooth segment (its pages are a SWARM track's; without the segment
 * they show nothing and turn nothing) */
static uint8_t sw_ui_side;
static BTAPP_FN void sw_ui_knob_bt(const page_t *pg, uint32_t k, int32_t s)
{
    uint32_t q = pg->title[5] == ' ' ? 3u : 0u;
    if (k == 0u)
        sw_ui_side = s > 0;
    else if (k + q - 1u < SWS_N)
        sw_sv_put(TSEL, sw_ui_side, k + q - 1u, sw_sv(TSEL, sw_ui_side, k + q - 1u) + accel(EN_K1 + k, s, 127));
}
static BTAPP_FN void sw_ui_cards_bt(const page_t *pg)
{
    static const char *const LB[8] = {"RATE", "MOVE", "TONE", "NOISE", "MOD", "SELF", "AIR", "PROX"};
    uint32_t q = pg->title[5] == ' ' ? 3u : 0u, c, i;
    char val[8];
    hot_gauge_column(0, "SIDE", sw_ui_side ? "NEAR" : "FAR", sw_ui_side ? 1000 : 0);
    for (c = 1; c < 4u; c++) {                          /* (RATE as the note, 1/1 .. 1/32; NOW the morph, by its source) */
        int32_t v = (i = c + q - 1u) < SWS_N ? sw_sv(TSEL, sw_ui_side, i) : sm.mq;
        val[0] = '1', val[1] = '/';
        fmt_int(val + (i ? 0 : 2), i ? v : 1 << ((uint32_t)v * 6u >> 7));
        hot_gauge_column(c, LB[i < SWS_N ? i : 5u + sm.src % 3u], val, v * 1000 / 128);
    }
}
static void sw_ui_knob(const page_t *pg, uint32_t k, int32_t s) { if (bt_hw_seg()) sw_ui_knob_bt(pg, k, s); }

/* (DRUM-W) the panel of WAVE A .. NOISE: pad A's table as its four frames stacked in depth, the frame it plays now lit
 * where it is (the hit's place, live, or its POS at rest) and the SCAN's way under it; B's frame beside it with the
 * STRUCT joining them; the eight pads below, the one edited framed, the ones sounding lit */
static void dw_wave_line(const int16_t *m, int32_t x, int32_t y, int32_t w, int32_t a, uint16_t c, int32_t th)
{
    int32_t k, px = x, py = y - (m[0] * a >> 15);
    for (k = 1; k <= (int32_t)DW_N; k++) {
        int32_t nx = x + k * w / (int32_t)DW_N, ny = y - (m[k] * a >> 15);
        if (th > 1) cv_line_t(px, py, nx, ny, c, th);
        else cv_line(px, py, nx, ny, c);
        px = nx; py = ny;
    }
}
/* (DRUM-W) MOD ENV's or MOD LFO's shape in a box (x, y, 66 x 34), lit to where the hit is */
static void dw_mod_box(const dw_lane_t *D, int lfo, int32_t x, int32_t y, int32_t on)
{
    int32_t k, px = 0, py = 0, w = 64, h = 30, idle = lfo ? !D->lrate || D->ldep == 64 : D->eamt == 64;   /* (shown whole, dim when it does nothing) */
    cv_rect(x, y, 66, 34, T_SURF);
    for (k = 0; k <= w; k++) {
        int32_t vq;
        if (lfo) {                                       /* two turns of the shape */
            uint32_t ph = (uint32_t)k * (0x80000000u / (uint32_t)w) * 4u;
            switch (D->lshp) {
            case DWL_SINE: vq = sine_i(ph); break;
            case DWL_SQR: vq = ph < 0x80000000u ? 32767 : -32767; break;
            case DWL_RAMP: vq = (int32_t)(ph >> 16) - 32768; break;
            case DWL_SH: vq = (int32_t)(fvm_hash((ph >> 29) * 977u + 7u + (uint32_t)(k >= w / 2) * 131u) >> 17) - 32768; break;
            default: vq = (int32_t)(ph >> 15); vq = vq < 65536 ? vq - 32768 : 98303 - vq; break;
            }
            vq = D->ldep < 64 ? -vq : vq;
        } else {                                         /* ATTACK then DECAY, their times on a log scale */
            int32_t ta = D->eatk * w / 254, td = w - ta, e;
            if (k < ta) e = k * 32767 / (ta ? ta : 1);
            else { int32_t x2 = (k - ta) * 65535 / (td ? td : 1); x2 = x2 * 127 / (D->edec + 16); x2 = x2 > 65535 ? 65535 : x2; e = (int32_t)(((65535u - (uint32_t)x2) * (65535u - (uint32_t)x2)) >> 17); }
            vq = D->eamt < 64 ? -e : e;
        }
        {
            int32_t nx = x + 1 + k, ny = y + 17 - (vq * h / 2 >> 15);
            if (k) cv_line(px, py, nx, ny, idle ? T_DIM : heat_col(on ? 230 : 160));
            px = nx; py = ny;
        }
    }
    cv_line(x + 1, y + 17, x + 65, y + 17, ux_mix(T_SURF, T_DIM, 60));
}
static const char DW_PADS[] = "KICK\0" "SNARE\0" "CLAP\0" "HAT C\0" "HAT O\0" "TOM\0" "RIM\0" "BELL";   /* (v1.1b5: packed, str_nth) */
#define DW_PAD(k) str_nth(DW_PADS, (unsigned)(k))
static const char DW_LPS[] = "FEEDBK\0" "MORPH\0" "DECAY\0" "STRIKE\0" "ATTACK\0" "FOLD\0" "GAIN\0" "CUTOFF";   /* (PLAY's, COLOR's:
                                                         * the kit's, DW_PL DW_CO) */
#define DW_LP(k) str_nth(DW_LPS, (unsigned)(k))
static const uint8_t DW_PL[4] = {P_E2, P_E0, P_E3, P_E5}, DW_CO[4] = {P_E1, P_E6, P_E7, P_FCUT};
static void dw_panel(void)
{
    uint32_t ln = dx_ui_lane & 7u, k, pn = dx_pagen(cur_page());
    const dw_lane_t *D = &dw_kit[ln];
    int16_t m[DW_N + 1u];
    int32_t top = (int32_t)(DW_FR - 1u) * 256, lv = dw_live[ln], on = (lv & 0x8000) != 0, p0 = D->pos * top / 127;
    int32_t sp = on ? (lv & 0x7FFF) : p0;
    int32_t se = D->edst == DWD_POSA || D->edst == DWD_POS ? clamp(p0 + ((int32_t)D->eamt - 64) * 12, 0, top) : p0;
    int32_t x0 = 8, y0 = 88, dx = 10, dy = 12, w = 104, a = 12;
    cv_text(6, 4 - AF_S_CAP_Y, &AF_S, DW_PAD(ln), T_ACCENT);
    cv_text(60, 4 - AF_S_CAP_Y, &AF_S, N_WAVEF[D->ta & 15u], T_TEXT);
    for (k = DW_FR; k-- > 0;) {                          /* the frames, the far one first */
        int32_t ox = x0 + (int32_t)k * dx, oy = y0 - (int32_t)k * dy;
        dw_frame(m, D->ta & 15u, (int32_t)k * 256);
        dw_wave_line(m, ox, oy, w, a, ux_mix(T_BG, heat_col(60 + (int32_t)k * 40), 55), 1);   /* (thermal: the far frames hotter) */
    }
    {                                                    /* the ENV's way on POS, then where it plays */
        int32_t xa = x0 + p0 * dx / 256, ya = y0 - p0 * dy / 256, xb = x0 + se * dx / 256, yb = y0 - se * dy / 256;
        int32_t xs = x0 + sp * dx / 256, ys = y0 - sp * dy / 256;
        cv_line_t(xa + w + 6, ya, xb + w + 6, yb, ux_mix(T_BG, T_THEME, 50), 2);
        dw_frame(m, D->ta & 15u, sp);
        dw_wave_line(m, xs, ys, w, a + 2, heat_col(on ? 235 : 170), 2);
        cv_rect(xs + w + 4, ys - 2, 5, 5, heat_col(on ? 245 : 170));
    }
    if (pn == 23u) {                                    /* MOVE: the modulators */
        cv_text(160, 4 - AF_S_CAP_Y, &AF_S, N_DWD[D->edst % DWD_N], T_ACCENT);
        dw_mod_box(D, 0, 160, 16, on);
        cv_text(160, 54 - AF_S_CAP_Y, &AF_S, N_DWD[D->ldst % DWD_N], T_ACCENT);
        dw_mod_box(D, 1, 160, 66, on);
    } else if (D->bamt) {                                /* B, at its place, by the STRUCT */
        int32_t bp = clamp(D->bpos * top / 127 + (on ? sp - p0 : 0), 0, top);
        cv_text(160, 22 - AF_S_CAP_Y, &AF_S, N_DWS[D->str % DWS_N], T_ACCENT);
        cv_text(160, 36 - AF_S_CAP_Y, &AF_S, N_WAVEF[D->tb & 15u], T_TEXT);
        cv_rect(160, 48, 74, 34, T_SURF);
        dw_frame(m, D->tb & 15u, bp);
        dw_wave_line(m, 165, 65, 64, 15 * D->bamt / 127 + 2, heat_col(90 + D->bamt), 1);
    } else
        cv_text(160, 36 - AF_S_CAP_Y, &AF_S, "B OFF", T_DIM);
    for (k = 0; k < 8u; k++) {                           /* the pads */
        int32_t px = 6 + (int32_t)k * 29, s2 = (dw_live[k] & 0x8000) != 0;
        cv_rect(px, 104, 26, 14, s2 ? heat_col(200) : T_SURF);
        if (k == ln) cv_frame(px, 104, 26, 14, T_ACCENT);
        cv_text(px + 4, 107 - AF_S_CAP_Y, &AF_S, DX_LANE_ABBR[k], k == ln ? T_TEXT : T_DIM);
    }
}

/* ---- (DRUM-W, its own UI) eight pages, four knobs each, the pad by its key or KNOB 1:
 *   PLAY   FEEDBK MORPH DECAY STRIKE (the kit, to play)   BODY   PAD PITCH SWEEP DECAY
 *   WAVE   PAD TABLE A POS TABLE B                        NOISE  PAD NOISE CUTOFF TYPE
 *   SHAPE  PAD STRUCT AMOUNT RATIO                        COLOR  ATTACK FOLD GAIN CUTOFF (the kit's)
 *   MOVE   PAD ENV DECAY LFO                              MIX    PAD LEVEL STEREO METAL
 * BODY and NOISE edit the patch sounding: A with MORPH under the middle, B over it */
enum { DWP_PLAY = 20, DWP_WAVE, DWP_SHAPE, DWP_MOVE, DWP_BODY, DWP_NOISE, DWP_COLOR, DWP_MIX };
static const uint8_t DW_TYPE[10][2] = {{DWN_WHITE, DXF_HP}, {DWN_WHITE, DXF_BP}, {DWN_WHITE, DXF_LP}, {DWN_PINK, DXF_BP}, {DWN_PINK, DXF_LP},
                                       {DWN_BROWN, DXF_LP}, {DWN_VIOLET, DXF_HP}, {DWN_VIOLET, DXF_BP}, {DWN_RESON, DXF_BP}, {DWN_RESON, DXF_HP}};
static const char *const N_DWTY[] = {"WHT HP", "WHT BP", "WHT LP", "PNK BP", "PNK LP", "BRN LP", "VIO HP", "VIO BP", "RES BP", "RES HP"};
static uint32_t dw_side(void) { return TSEL->p[P_E0] >= 64; }   /* (the patch sounding) */
static uint32_t dw_type_ix(const dx_lane_t *L, const dw_lane_t *D)
{
    uint32_t f = (L->mode >> 2) & 3u, k;
    for (k = 0; k < 10u; k++) if (DW_TYPE[k][0] == D->noise && DW_TYPE[k][1] == (f > 2u ? DXF_BP : f)) return k;
    return 0;
}
static void dw_knob(uint32_t pn, uint32_t k, int32_t s)
{
    dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    dw_lane_t *D = &dw_kit[dx_ui_lane & 7u];
    uint8_t *v = dw_side() ? L->b : L->a;
    int32_t st = s > 0 ? 1 : -1, ac = accel(EN_K1 + k, s, 127);
    int16_t *p = TSEL->p;
    if (pn == DWP_PLAY || pn == DWP_COLOR) {            /* (the kit's: the engine's own) */
        uint32_t id = (pn == DWP_PLAY ? DW_PL : DW_CO)[k & 3u];
        p[id] = (int16_t)clamp(p[id] + ac, 0, 127);
        return;
    }
    if (k == 0u) { dx_lane_turn(s); return; }           /* KNOB 1: the pad */
    switch (pn * 4u + k) {
    case DWP_WAVE * 4u + 1u: D->ta = (uint8_t)clamp((int32_t)D->ta + st, 0, 15); break;
    case DWP_WAVE * 4u + 2u: D->pos = (uint8_t)clamp((int32_t)D->pos + ac, 0, 127); break;
    case DWP_WAVE * 4u + 3u: D->tb = (uint8_t)clamp((int32_t)D->tb + st, 0, 15); break;
    case DWP_SHAPE * 4u + 1u: D->str = (uint8_t)clamp((int32_t)D->str + st, 0, DWS_N - 1); break;
    case DWP_SHAPE * 4u + 2u: D->bamt = (uint8_t)clamp((int32_t)D->bamt + ac, 0, 127); break;
    case DWP_SHAPE * 4u + 3u: D->ratio = (uint8_t)clamp((int32_t)D->ratio + st, 0, (int32_t)DW_NRAT - 1); break;
    case DWP_MOVE * 4u + 1u: D->eamt = (uint8_t)clamp((int32_t)D->eamt + ac, 0, 127); break;
    case DWP_MOVE * 4u + 2u: D->edec = (uint8_t)clamp((int32_t)D->edec + ac, 0, 127); break;
    case DWP_MOVE * 4u + 3u:                            /* LFO: its depth (a rate given the first time) */
        D->ldep = (uint8_t)clamp((int32_t)D->ldep + ac, 0, 127);
        if (!D->lrate && D->ldep != 64u) D->lrate = 60;
        break;
    case DWP_BODY * 4u + 1u: v[DXP_PITCH] = (uint8_t)clamp(v[DXP_PITCH] + ac, 0, 127); break;
    case DWP_BODY * 4u + 2u: v[DXP_PMOD] = (uint8_t)clamp(v[DXP_PMOD] + ac, 0, 127); break;
    case DWP_BODY * 4u + 3u: v[DXP_DECAY] = (uint8_t)clamp(v[DXP_DECAY] + ac, 0, 127); break;
    case DWP_NOISE * 4u + 1u: v[DXP_NOISE] = (uint8_t)clamp(v[DXP_NOISE] + ac, 0, 127); break;
    case DWP_NOISE * 4u + 2u: v[DXP_COLOR] = (uint8_t)clamp(v[DXP_COLOR] + ac, 0, 127); break;
    case DWP_NOISE * 4u + 3u: {
        uint32_t ix = (uint32_t)clamp((int32_t)dw_type_ix(L, D) + st, 0, 9);
        D->noise = DW_TYPE[ix][0];
        L->mode = (uint8_t)((L->mode & ~0x0Cu) | (uint32_t)DW_TYPE[ix][1] << 2);
        break;
    }
    case DWP_MIX * 4u + 1u: p[P_LN0 + (dx_ui_lane & 7u)] = (int16_t)clamp(p[P_LN0 + (dx_ui_lane & 7u)] + ac, 0, 127); break;
    case DWP_MIX * 4u + 2u: p[P_DXIMG] = (int16_t)clamp(p[P_DXIMG] + ac, 0, 127); break;
    case DWP_MIX * 4u + 3u: dx_tone = (uint8_t)((dx_tone & 0xF0u) | (uint32_t)clamp((int32_t)DX_TONE_BP(dx_tone) + st, 0, 15)); break;
    }
}
static void dw_cards(uint32_t pn)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    const dw_lane_t *D = &dw_kit[dx_ui_lane & 7u];
    const uint8_t *v = dw_side() ? L->b : L->a;
    const int16_t *p = TSEL->p;
    char val[12], ln[8];
    uint32_t sd = (pn == DWP_BODY || pn == DWP_NOISE);
    if (pn == DWP_PLAY || pn == DWP_COLOR) {
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            uint32_t id = (pn == DWP_PLAY ? DW_PL : DW_CO)[c];
            fmt_int(val, p[id]);
            hot_gauge_column(c, DW_LP(c + (pn == DWP_PLAY ? 0u : 4u)), val, p[id] * 1000 / 127);
        }
        return;
    }
    str_cpy(ln, DX_LANE_ABBR[dx_ui_lane & 7u], sizeof ln);
    if (sd) { ln[2] = ' '; ln[3] = dw_side() ? 'B' : 'A'; ln[4] = 0; }   /* (BODY, NOISE: the patch edited) */
    hot_gauge_column(0, "PAD", ln, (int32_t)(dx_ui_lane & 7u) * 1000 / 7);
    switch (pn) {
    case DWP_WAVE:
        hot_gauge_column(1, "TABLE A", N_WAVEF[D->ta & 15u], (int32_t)(D->ta & 15u) * 1000 / 15);
        fmt_int(val, D->pos); hot_gauge_column(2, "POS", val, D->pos * 1000 / 127);
        hot_gauge_column(3, "TABLE B", N_WAVEF[D->tb & 15u], (int32_t)(D->tb & 15u) * 1000 / 15);
        break;
    case DWP_SHAPE:
        hot_gauge_column(1, "STRUCT", N_DWS[D->str % DWS_N], (int32_t)(D->str % DWS_N) * 250);
        fmt_int(val, D->bamt); gauge_column(2, "AMOUNT", D->bamt ? val : "OFF", D->bamt ? VAL(2u) : T_DIM, D->bamt * 1000 / 127);
        hot_gauge_column(3, "RATIO", N_DWR[D->ratio % DW_NRAT], (int32_t)(D->ratio % DW_NRAT) * 1000 / (int32_t)(DW_NRAT - 1u));
        break;
    case DWP_MOVE:
        fmt_int(val, (int32_t)D->eamt - 64); hot_gauge_column(1, "ENV", val, D->eamt * 1000 / 127);
        fmt_int(val, D->edec); hot_gauge_column(2, "DECAY", val, D->edec * 1000 / 127);
        fmt_int(val, (int32_t)D->ldep - 64); hot_gauge_column(3, "LFO", D->lrate ? val : "OFF", D->ldep * 1000 / 127);
        break;
    case DWP_BODY:
        fmt_int(val, v[DXP_PITCH]); hot_gauge_column(1, "PITCH", val, v[DXP_PITCH] * 1000 / 127);
        fmt_int(val, v[DXP_PMOD]); hot_gauge_column(2, "SWEEP", val, v[DXP_PMOD] * 1000 / 127);
        fmt_int(val, v[DXP_DECAY]); hot_gauge_column(3, "DECAY", val, v[DXP_DECAY] * 1000 / 127);
        break;
    case DWP_NOISE:
        fmt_int(val, v[DXP_NOISE]); hot_gauge_column(1, "NOISE", val, v[DXP_NOISE] * 1000 / 127);
        fmt_int(val, v[DXP_COLOR]); hot_gauge_column(2, "CUTOFF", val, v[DXP_COLOR] * 1000 / 127);
        hot_gauge_column(3, "TYPE", N_DWTY[dw_type_ix(L, D)], (int32_t)dw_type_ix(L, D) * 111);
        break;
    default:                                            /* MIX */
        fmt_int(val, p[P_LN0 + (dx_ui_lane & 7u)]); hot_gauge_column(1, "LEVEL", val, p[P_LN0 + (dx_ui_lane & 7u)] * 1000 / 127);
        fmt_int(val, p[P_DXIMG]); hot_gauge_column(2, "STEREO", val, p[P_DXIMG] * 1000 / 127);
        fmt_int(val, (int32_t)DX_TONE_BP(dx_tone)); gauge_column(3, "METAL", DX_TONE_BP(dx_tone) ? val : "OFF", DX_TONE_BP(dx_tone) ? VAL(3u) : T_DIM, (int32_t)DX_TONE_BP(dx_tone) * 1000 / 15);
        break;
    }
}
/* the pad's frame playing (or at its POS), A (and B by the STRUCT when b): m[DW_N + 1] */
static void dw_frame_now(uint32_t ln, int16_t *m)
{
    const dw_lane_t *D = &dw_kit[ln];
    int32_t top = (int32_t)(DW_FR - 1u) * 256, lv = dw_live[ln];
    dw_frame(m, D->ta & 15u, (lv & 0x8000) ? (lv & 0x7FFF) : D->pos * top / 127);
}
/* (BYTE v1.1b5, the owner's mock-up drumw.html A "RADAR VIVO") PLAY: the kit a live radar, the whole page (ui.c
 * dw_full). Each pad a ray in its colour, stretched by its glow (a hit: half and its velocity, by its LEVEL; ~0.9 a 15 ms tick
 * after), a dot at its tip, a ring from the middle out as it fades, its name at the rim lit while it glows; the tips
 * joined, filled in the loudest pad's colour, outlined white; the kick swells it all and tints the ground in the
 * loudest's colour, the snare a short white flash; the whole turning slowly. The hits: drum_kit's ages (eng_drum.c,
 * read here: nothing added to the audio); the four knobs as tags at its foot */
static const uint16_t DW_PC[8] = {0xF9D6, 0xFC47, 0xEF8B, 0x7FF3, 0x3E9F, 0x2B7F, 0xA47F, 0xFACB};
static uint8_t dw_e[8], dw_ag[8], dw_tk, dw_ld;
static uint16_t dw_bg;
static uint16_t dw_glow(void)                         /* (once a tick: the same tick's bands, the ground, the same) */
{
    const drum_lane_t *L = drum_kit[song.sel % NTRK];
    uint32_t k, i, e, m = 0, n = (uint8_t)(anim_fr - dw_tk);
    if (!n)
        return dw_bg;
    dw_tk = (uint8_t)anim_fr;
    for (k = 0; k < 8u; k++, L++) {
        uint32_t h = (128u + L->x.sv) * (uint32_t)(TSEL->p[P_LN0 + k] + 128) >> 8;
        for (e = dw_e[k], i = n < 40u ? n : 40u; i--;) e = e * 230u >> 8;
        if (L->age < dw_ag[k] && h > e) e = h;
        dw_ag[k] = L->age; dw_e[k] = (uint8_t)e;
        if (e > dw_e[m]) m = k;
    }
    dw_ld = (uint8_t)m;
    return dw_bg = ux_mix(ux_mix(T_BG, DW_PC[m], 10 + (dw_e[0] * 35 >> 8)), 0xFFFFu, dw_e[1] * dw_e[1] * 60 >> 16);   /* (the
                                                         * snare's flash: its glow squared, ~0.8 a tick) */
}
static void dw_thick(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t th)   /* across its run */
{
    int32_t t, h = (x1 - x0) * (x1 - x0) > (y1 - y0) * (y1 - y0);
    for (t = -th / 2; t < th - th / 2; t++)
        cv_line(x0 + (h ? 0 : t), y0 + (h ? t : 0), x1 + (h ? 0 : t), y1 + (h ? t : 0), c);
}
static void dw_round(int32_t r, uint16_t c, int32_t th)   /* a circle round the radar's middle (120, 88) */
{
    int32_t px = 0, py = 0;
    uint32_t i;
    for (i = 0; i <= 32u; i++) {
        int32_t x = 120 + (r * og_cos(i * 2048u) >> 15), y = 88 + (r * og_sin(i * 2048u) >> 15);
        if (i) dw_thick(px, py, x, y, c, th);
        px = x; py = y;
    }
}
static void dw_tri(const int32_t *p, uint16_t c)     /* p: three points' x, y; the band's rows only */
{
    int32_t y, e, y0 = p[1], y1 = p[1];
    for (e = 3; e < 6; e += 2) { y0 = p[e] < y0 ? p[e] : y0; y1 = p[e] > y1 ? p[e] : y1; }
    y0 = y0 < cv_by ? cv_by : y0;
    y1 = y1 >= cv_by + (int32_t)cv_bh ? cv_by + (int32_t)cv_bh - 1 : y1;
    for (y = y0; y <= y1; y++) {
        int32_t lo = 999, hi = -999;
        for (e = 0; e < 6; e += 2) {
            int32_t ax = p[e], ay = p[e + 1], bx = p[(e + 2) % 6], by = p[(e + 3) % 6], x;
            if ((y < ay && y < by) || (y > ay && y > by)) continue;
            x = ay == by ? ax : ax + (y - ay) * (bx - ax) / (by - ay);
            lo = x < lo ? x : lo; hi = x > hi ? x : hi;
            if (ay == by) { lo = bx < lo ? bx : lo; hi = bx > hi ? bx : hi; }
        }
        if (hi >= lo) cv_rect(lo, y, hi - lo + 1, 1, c);
    }
}
static void dw_play_panel(void)
{
    int32_t R = 74 + (dw_e[0] * 22 >> 8), tx[9], ty[9], k, t[6] = {120, 88};
    uint32_t rot = (uint32_t)anim_fr * 47u;
    uint16_t bg = dw_glow();
    char v[8];
    for (k = 1; k <= 3; k++)                             /* the range */
        dw_round(R * k / 3, ux_mix(bg, T_TEXT, 12), 1);
    for (k = 0; k < 8; k++) {
        uint32_t a = (uint32_t)k * 8192u - 16384u + rot;
        int32_t l = (R * (64 + (dw_e[k] * 270 >> 8)) >> 8) + 4;
        if (dw_e[k] > 20u)                               /* a ring out from the middle as it fades */
            dw_round(20 + (255 - dw_e[k]) * 2 / 3, ux_mix(bg, DW_PC[k], dw_e[k] * 100 >> 8), 2 + dw_e[k] / 64);
        tx[k] = 120 + (l * og_cos(a) >> 15); ty[k] = 88 + (l * og_sin(a) >> 15);   /* its tip */
    }
    tx[8] = tx[0]; ty[8] = ty[0];
    for (k = 0; k < 8; k++) {                            /* the tips joined, filled in the loudest's colour */
        t[2] = tx[k]; t[3] = ty[k]; t[4] = tx[k + 1]; t[5] = ty[k + 1];
        dw_tri(t, ux_mix(bg, DW_PC[dw_ld], 35 + (dw_e[dw_ld] * 40 >> 8)));
    }
    for (k = 0; k < 8; k++) {                            /* the rays, their dots */
        uint16_t c = ux_mix(bg, DW_PC[k], 50 + (dw_e[k] * 50 >> 8));
        int32_t r = 2 + (dw_e[k] * 6 >> 8);       /* (cv_rrect: r <= 8) */
        dw_thick(120, 88, tx[k], ty[k], c, 2 + dw_e[k] / 28);
        cv_rrect(tx[k] - r, ty[k] - r, 2 * r + 1, 2 * r + 1, r, c, bg);
    }
    for (k = 0; k < 8; k++)                              /* the outline, white; the names over it */
        dw_thick(tx[k], ty[k], tx[k + 1], ty[k + 1], 0xFFFFu, 2);
    for (k = 0; k < 8; k++) {                            /* the names at the rim: a tag in its colour while it glows */
        uint32_t a = (uint32_t)k * 8192u - 16384u + rot, on = dw_e[k] > 76u;
        const char *nm = DW_PAD(k);
        hud_tag(118 + (88 * og_cos(a) >> 15) - text_w(&AF_X, nm) / 2, 83 + (80 * og_sin(a) >> 15), nm,
                on ? 0x0000u : ux_mix(bg, T_TEXT, 35), on ? DW_PC[k] : bg);
    }
    for (k = 0; k < 4; k++) {                            /* the knobs: FEEDBK MORPH DECAY STRIKE */
        int32_t x = 4 + k * 59, hot = ui.hot_t && ui.hot_col == (uint32_t)k;
        hud_tag(x, 176, DW_LP(k), hot ? 0x0000u : T_TEXT, hot ? TRK_COL[song.sel % 6u] : 0x29FFu);
        fmt_int(v, TSEL->p[DW_PL[k]]);
        cv_text_on(x + 2, 190 - AF_S_CAP_Y, &AF_S, v, T_TEXT, bg);
    }
}
/* SHAPE: one turn of A and B joined by the STRUCT, as the voice joins them (B at its RATIO) */
static void dw_shape_box(const dw_lane_t *D, int32_t x, int32_t y)
{
    int16_t ma[DW_N + 1u], mb[DW_N + 1u];
    int32_t top = (int32_t)(DW_FR - 1u) * 256, g = D->bamt * 258, i, px = 0, py = 0;
    uint32_t rq = DW_RAT[D->ratio % DW_NRAT];
    dw_frame_now((uint32_t)(D - dw_kit), ma);
    dw_frame(mb, D->tb & 15u, D->bpos * top / 127);
    cv_rect(x, y, 74, 64, T_SURF);
    for (i = 0; i <= 70; i++) {
        uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / 70u), p2 = (uint32_t)(((uint64_t)ph * rq) >> 12);
        int32_t a = DW_RD(ma, ph), b = DW_RD(mb, p2), o, bg = mulq15(b, g), z;
        switch (D->str) {
        case DWS_FM: o = DW_RD(ma, ph + ((uint32_t)bg << 17)); break;
        case DWS_RING: o = a + mulq15(clamp(mulq15(a, b) * 2, -32767, 32767) - a, g); break;
        case DWS_SYNC: o = a + mulq15(DW_RD(mb, (uint32_t)(((uint64_t)ph * rq) >> 12) & 0xFFFFFFFFu) - a, g); break;
        case DWS_FOLD: z = clamp(a + bg * 2, -5 * 16384, 5 * 16384);
            z = z > 16384 ? 32768 - z : z < -16384 ? -32768 - z : z; z = z > 16384 ? 32768 - z : z < -16384 ? -32768 - z : z; o = z * 2; break;
        default: o = a + bg; break;
        }
        o = clamp(o, -32767, 32767);
        { int32_t nx = x + 2 + i, ny = y + 32 - o * 28 / 32768; if (i) cv_line_t(px, py, nx, ny, heat_col(140 + D->bamt), 2); px = nx; py = ny; }
    }
}
/* BODY: the hit's pitch over 400 ms (its SWEEP falling to PITCH) hot, its level under it */
static void dw_body_panel(const dx_lane_t *L, const uint8_t *v)
{
    int32_t i, px = 0, py = 0, qx = 0, qy = 0;
    uint32_t ta = dx_tau(v[DXP_DECAY]), tp = clamp((int32_t)(ta / ((dx_ui_lane & 7u) ? 8u : 6u)), 1000, (dx_ui_lane & 7u) ? 25000 : 40000);
    (void)L;
    cv_text(6, 4 - AF_S_CAP_Y, &AF_S, DW_PAD(dx_ui_lane & 7u), heat_col(220));
    cv_text(150, 4 - AF_S_CAP_Y, &AF_S, "PITCH  LEVEL", T_DIM);
    cv_rect(6, 16, 228, 84, T_SURF);
    for (i = 0; i <= 226; i++) {
        uint32_t us = (uint32_t)i * 400000u / 226u;
        int32_t ep = (int32_t)(dx_pow2((uint32_t)clamp(4096 * 16 - (int32_t)((uint64_t)us * 4096u * 1443u / 1000u / (tp | 1u)), 0, 4096 * 16)) >> 16);   /* e^(-t/tp), Q16 */
        int32_t ea = (int32_t)(dx_pow2((uint32_t)clamp(4096 * 16 - (int32_t)((uint64_t)us * 4096u * 1443u / 1000u / (ta | 1u)), 0, 4096 * 16)) >> 16);
        int32_t semi = v[DXP_PITCH] * 12 / 16 + ((v[DXP_PMOD] * 6 * ep) >> 16) / 16;   /* (semitones above the bottom) */
        int32_t ny = 96 - clamp(semi, 0, 120) * 76 / 120, ly = 96 - (ea * 70 >> 16);
        if (i) { cv_line_t(px, py, 7 + i, ny, heat_col(120 + (ep >> 9)), 2); cv_line(qx, qy, 7 + i, ly, ux_mix(T_SURF, T_DIM, 70)); }
        px = 7 + i; py = ny; qx = 7 + i; qy = ly;
    }
}
/* NOISE: the filter's shape at its CUTOFF over the band (LP, BP, HP), the colour's tilt under it */
static void dw_noise_panel(const dx_lane_t *L, const dw_lane_t *D, const uint8_t *v)
{
    int32_t i, px = 0, py = 0, f = (L->mode >> 2) & 3u, c = v[DXP_COLOR] * 226 / 127;
    static const int8_t TILT[5] = {0, -10, -20, 12, 0};
    cv_text(6, 4 - AF_S_CAP_Y, &AF_S, N_DWN[D->noise % 5u], heat_col(220));
    cv_rect(6, 16, 228, 84, T_SURF);
    for (i = 0; i <= 226; i++) {
        int32_t d = (i - c) * 3, r = f == DXF_LP ? (d < 0 ? 0 : d) : f == DXF_HP ? (d > 0 ? 0 : -d) : (d < 0 ? -d : d) * (D->noise == DWN_RESON ? 1 : 2) / 2;
        int32_t ny = 30 + clamp(r, 0, 60) + TILT[D->noise % 5u] * (i - 113) / 226 - (D->noise == DWN_RESON && i > c - 6 && i < c + 6 ? 14 : 0);
        ny = clamp(ny, 18, 98);
        if (i) cv_line_t(px, py, 7 + i, ny, heat_col(230 - clamp(r, 0, 60) * 2), 2);
        px = 7 + i; py = ny;
    }
    cv_line(7 + c, 18, 7 + c, 98, heat_col(200));
}
/* COLOR: the kit's FOLD and GAIN (into the bus's knee) as the curve a hit goes through (in -> out) */
static void dw_color_panel(void)
{
    int32_t i, px = 0, py = 0, fold = TSEL->p[P_E6], drv = TSEL->p[P_E7];
    cv_text(6, 4 - AF_S_CAP_Y, &AF_S, "FOLD > GAIN", heat_col(220));
    cv_rect(62, 14, 116, 88, T_SURF);
    cv_line(62, 58, 178, 58, ux_mix(T_SURF, T_DIM, 60));
    for (i = 0; i <= 112; i++) {
        int32_t x = (i - 56) * 585, g = 16 + ((fold * fold * 5) >> 10), z = clamp((x * g) >> 4, -5 * 16384, 5 * 16384), a;
        for (a = 0; a < 3; a++) z = z > 16384 ? 32768 - z : z < -16384 ? -32768 - z : z;
        if (drv) z = softclip(z * (128 + drv * 3) >> 7);   /* (x1 .. x4 into the knee) */
        { int32_t nx = 64 + i, ny = 58 - z * 40 / 32768; if (i) cv_line_t(px, py, nx, ny, heat_col(100 + fold + drv / 2), 2); px = nx; py = ny; }
    }
}
/* MIX: the eight levels hot as they are, the pad edited framed, the STEREO's spread under them */
static void dw_mix_panel(void)
{
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        int32_t x = 6 + (int32_t)k * 29, lv = TSEL->p[P_LN0 + k], h = lv * 80 / 127, on = (dw_live[k] & 0x8000) != 0;
        cv_rect(x, 12, 24, 82, T_SURF);
        cv_rect(x + 2, 92 - h, 20, h, heat_col(on ? 245 : 60 + lv));
        if (k == (dx_ui_lane & 7u)) cv_frame(x, 12, 24, 82, heat_col(220));
        cv_text(x + 4, 98 - AF_S_CAP_Y, &AF_S, DX_LANE_ABBR[k], k == (dx_ui_lane & 7u) ? T_TEXT : T_DIM);
    }
}
static void dw_panel2(uint32_t pn)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    const dw_lane_t *D = &dw_kit[dx_ui_lane & 7u];
    const uint8_t *v = dw_side() ? L->b : L->a;
    switch (pn) {
    case DWP_PLAY: dw_play_panel(); break;
    case DWP_BODY: dw_body_panel(L, v); break;
    case DWP_NOISE: dw_noise_panel(L, D, v); break;
    case DWP_COLOR: dw_color_panel(); break;
    case DWP_MIX: dw_mix_panel(); break;
    default:
        dw_panel();                                     /* WAVE SHAPE MOVE: the table in depth */
        if (pn == DWP_SHAPE) { cv_rect(160, 14, 76, 84, T_BG); cv_text(160, 22 - AF_S_CAP_Y, &AF_S, N_DWS[D->str % DWS_N], heat_col(220)); dw_shape_box(D, 160, 32); }
        break;
    }
}
