/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE) DRONE CLAY, a track's sequencer (the owner: FORM out as an engine; "the voice the track has, a drone,
 * endless timbres"). Each audio block, before the parts (fx.c mix_block, after the macros): for every track with it
 * ON and running (RUN SONG: while the transport plays; FREE: always), FORM's composer (eng_form.c) on the track's
 * settings: the euclids strike gestures on the STEP, the gestures summed into what each modulator adds now (clay_v,
 * mod.c mod_begin moves its DEST with it); a NOTE gesture (ARP, CHORD ..) a real note on the voice, on the scale,
 * two octaves over the drone; the drone itself, a very low note (C0 .. C3 on the track's ROOT) held, or struck again
 * every RATE. Off, stopped, muted: its notes end. Included by seq.c (its notes, the scale) */
static void ui_message(const char *s);                  /* (ui.c) */
static uint32_t clay_free;                              /* (FREE stopped: its own beat, Q16) */
static int16_t clay_sp[CLAY_SPN];                        /* (the audio's FORM slots: clay_fill) */
/* (main.c, at power-on: RAMTEXT's room is not zeroed) */
/* (BYTE, the owner: the keys, as FORM's) the keys' state: the MOD chosen, the drone's degree (a root key's), the last key's
 * event for its card (seq: changes per event; kind 1 a gesture, 2 a MOD, 3 a mode, 4 the root; a, b its values);
 * written by the audio's keys (core 0), read by the UI */
static uint8_t clay_km[NTRK] __attribute__((section(".eu_keep")));
static int8_t clay_kr[NTRK] __attribute__((section(".eu_keep")));
static struct { uint8_t seq, tr, kind, a, b, held; } clay_ke __attribute__((section(".eu_keep")));
static uint32_t clay_b0[NTRK] __attribute__((section(".eu_keep"))), clay_pq[NTRK] __attribute__((section(".eu_keep")));   /* (a punch-in's restart: the time it began; what was held) */
static void clay_boot(void)
{
    memset(clay_km, 0, sizeof clay_km); memset(clay_kr, 0, sizeof clay_kr); memset(&clay_ke, 0, sizeof clay_ke);
    memset(clay, 0, sizeof clay); memset(clay_v, 0, sizeof clay_v); memset(clay_dn, 0, sizeof clay_dn);
    memset(clay_nn, 0, sizeof clay_nn); memset(clay_lr, 0, sizeof clay_lr);
    memset(clay_b0, 0, sizeof clay_b0); memset(clay_pq, 0, sizeof clay_pq);
}
static void clay_stop(uint32_t tr)
{
    track_t *t = &trk[tr];
    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
    if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
    clay_dn[tr] = clay_nn[tr] = 0;
    clay_v[tr][0] = clay_v[tr][1] = clay_v[tr][2] = clay_v[tr][3] = 0;
    form_lit[tr] = 0;
}
static int clay_klive(const form_st_t *st)            /* a key's gesture alive (the keys play while stopped too) */
{
    uint32_t i;
    for (i = 0; i < FORM_G; i++)
        if ((st->g[i].fl & 8u) && st->g[i].key)
            return 1;
    return 0;
}
static __attribute__((noinline)) void clay_tick(uint32_t n)
{
    static const uint32_t PER[8] = {0, 16u << 16, 8u << 16, 4u << 16, 2u << 16, 1u << 16, 1u << 15, 1u << 14};
    uint32_t s16 = div_samples(2), bps = beat_samples(), b, tr, k;
    if (song.playing && clk_pos != CLK_START)
        b = ((uint32_t)clk_n << 16) + ((uint32_t)clk_pos << 16) / (s16 ? s16 : 1u) >> 2;
    else
        b = clay_free += (n << 16) / (bps | 1u);
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        const int16_t *c = clay[tr];
        form_st_t *st = &form_st[tr];
        uint32_t end = b, seed = (uint32_t)clamp(c[CL_SEED], 0, 127) * 977u + 1u, root, dn;
        int32_t D[FV_N] = {0};
        if (!c[CL_ON] || tr >= trk_live || t->p[P_MUTE] || (!song.playing && c[CL_RUN] != 1 && !clay_klive(st))) {
            if (clay_dn[tr] | clay_nn[tr] | form_lit[tr]) clay_stop(tr);
            continue;
        }
        {   /* (the owner) a punch-in on the time or the pitch: the composition from its first hit, the moment it goes in;
             * let go, back on the transport's grid (never out of phase) */
            uint32_t pq = (pt_ops & PT_TIME_OPS) | (uint32_t)(t->pfx & (PFX_HALF | PFX_LOOP | PFX_REP)) << 12 | (t->pfx_pit ? 1u << 20 : 0u);
            if (pq != clay_pq[tr]) {
                clay_b0[tr] = pq ? b : 0u;
                for (k = 0; k < 4u; k++) st->lastm[k] = -1;
                clay_pq[tr] = pq;
            }
            end = b - clay_b0[tr];
        }
        clay_fill(c, clay_sp);
        if (pt_ops & PT_TIME_OPS)                       /* (the white punch-ins: the composition's time, as FORM's) */
            end = pt_time(end >> 4, 0u) << 4;
        st->clk += (n << 18) / (bps ? bps : 1u);
        for (k = 0; k < 4u; k++) {                     /* each modulator on the STEP: its euclid strikes a gesture */
            int32_t ds = (int32_t)form_vds_x(clay_sp, FORM_MC, k), dec = clamp(clay_sp[FORM_MC[k][3]], 0, 127);
            uint32_t s1 = end >> (16 - form_mexpo(clay_sp, ds));
            if ((int32_t)s1 == st->lastm[k]) continue;
            st->lastm[k] = (int32_t)s1;
            form_lit[tr] = (uint8_t)(form_lit[tr] & ~(1u << k));
            if (form_hit(clay_sp, k, s1, seed)) {
                form_spawn(st, (uint32_t)((ds & 511) >> 3), k, (s1 & 3u) ? 90u : 127u, 0u, 64u + (uint32_t)dec * 32u, 0u);
                form_lit[tr] |= (uint8_t)(1u << k);
            }
        }
        form_eval(st, clay_sp, D, t->at);
        for (k = 0; k < 4u; k++)
            clay_v[tr][k] = (int16_t)clamp(st->mv[k] * 256, -32767, 32767);
        root = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        dn = 12u * (uint32_t)(clamp(c[CL_OCT], 0, 3) + 1) + root + (uint32_t)clay_kr[tr];   /* the drone: C0 .. C3 on the ROOT
                                                         * (+ a root key's degree) */
        {
            uint32_t per = PER[clamp(c[CL_RATE], 0, 7)], cnt = per ? end / per : 0u, j, busy = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (the owner: the sequencers live with it) one voice: a step's or a
                                                         * key's note takes it, the drone back after */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (busy) {
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                clay_dn[tr] = 0;
            } else if (clay_dn[tr] != dn || (per && cnt != clay_lr[tr])) {   /* new, moved, or its RATE's strike */
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                seq_note_on(t, dn, 110u);
                clay_dn[tr] = (uint8_t)dn;
            }
            clay_lr[tr] = cnt;
        }
        {   /* a NOTE gesture alive: its note on the voice, on the scale, two octaves over the drone */
            int32_t sem = D[FV_ESUM] > 0 ? D[FV_SEM] * 32767 / (D[FV_ESUM] > 32767 ? D[FV_ESUM] : 32767) : 0;
            uint32_t nn = 0, j, busy = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (one voice: a step's or a key's note before a gesture's) */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (D[FV_ESUM] > 3000 && !busy)
                nn = (uint32_t)clamp(scale_snap(t, (int32_t)dn + 24 + (sem * 12 + (sem < 0 ? -2048 : 2048)) / 4096), 12, 108);
            if (nn != clay_nn[tr]) {
                if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
                if (nn && nn != dn) seq_note_on(t, nn, 100u);
                clay_nn[tr] = (uint8_t)(nn != dn ? nn : 0u);
            }
        }
        if (form_scope_tr == tr) {                      /* (the panel's OUT: its voices' loudest envelope, a block a point) */
            int32_t e = 0;
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active && (int32_t)(t->v[k].env >> 9) > e) e = (int32_t)(t->v[k].env >> 9);
            form_scope[form_sc_i & 63u] = (int16_t)clamp(e, 0, 32767);
            form_sc_i = (uint8_t)((form_sc_i + 1u) & 63u);
        }
    }
}

/* the pages' knobs (params.c page_desc: ids ID_CL0 + CL_*): the selected track's settings, written in place (ui_input.c
 * edit_param, then clay_put) */
static const char *const CL_ONN[] = {"OFF", "ON", 0};
static const char *const CL_RATEN[] = {"HOLD", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", 0};
static const char *const CL_RUNN[] = {"SONG", "FREE", 0};
static const char *const CL_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const param_desc_t *clay_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    static const char *nm[CLAY_ND + 1] __attribute__((section(".eu_keep"))), *on[5] __attribute__((section(".eu_keep")));   /* (DEST's: the sound's knobs by name) */
    static char octn[4][4] __attribute__((section(".eu_keep")));
    static param_desc_t d __attribute__((section(".eu_keep")));
    static const char *const BASE[CLAY_ND] = {"OFF", "PITCH", "CUTOFF", "SHAPE", "VOLUME", "PAN", "DIST", "CHORUS", "DELAY",
                                              "REVERB", "LFO RATE", "VIBRATO", 0, 0, 0, 0, 0, 0, 0, 0, "VOICE CUT", "LEVEL", "LIFE"};
    static const char *const DECN[4] = {"DECAY 1", "DECAY 2", "DECAY 3", "DECAY 4"};
    static const char *const EN[8] = {"E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    f %= CLAY_NF;
    *valp = &clay[tr][f];
    d.def = 0; d.names = 0; d.fmt = F_ENUM; d.min = 0;
    if (f == CL_ON) { d.label = "CLAY"; d.max = 1; d.names = CL_ONN; }
    else if (f == CL_OCT) {                              /* (the drone's note: its octave on the track's ROOT) */
        uint32_t r = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        for (i = 0; i < 4u; i++) {
            str_cpy(octn[i], CL_NN[r], 4);
            octn[i][str_len(octn[i])] = (char)('0' + i);
            octn[i][str_len(CL_NN[r]) + 1u] = 0;
            on[i] = octn[i];
        }
        on[4] = 0;
        d.label = "DRONE"; d.max = 3; d.def = 1; d.names = on;
    } else if (f == CL_RATE) { d.label = "RATE"; d.max = 7; d.names = CL_RATEN; }
    else if (f == CL_RUN) { d.label = "RUN"; d.max = 1; d.names = CL_RUNN; }
    else if (f < CL_DEC) {
        uint32_t k = (f - CL_M0) & 3u;
        if (k == 0u) {                                   /* DEST: the sound's knobs by their own names */
            for (i = 0; i < (uint32_t)CLAY_ND; i++) nm[i] = BASE[i];
            for (i = 0; i < 8u; i++) {
                const param_desc_t *pd = track_desc(t, P_E0 + i);
                nm[12u + i] = pd && pd->label && pd->label[0] && pd->label[0] != '-' ? pd->label : EN[i];
            }
            nm[CLAY_ND] = 0;
            d.label = "DEST"; d.max = CLAY_ND - 1; d.names = nm;
        } else if (k == 1u) {                            /* SHAPE: the HITS first (the owner) */
            d.label = "SHAPE"; d.max = FORM_NSH - 1; d.names = CLAY_SHN;
        } else if (k == 2u) { d.label = "EUCLID"; d.max = FORM_NEU - 1; d.names = FORM_EU; }
        else { d.label = "AMOUNT"; d.fmt = F_PCT; d.max = 127; d.def = 100; }
    } else if (f < CL_STEP) { d.label = DECN[f - CL_DEC]; d.fmt = F_PCT; d.max = 127; }
    else if (f == CL_STEP) { d.label = "STEP"; d.max = 3; d.def = 2; d.names = FORM_RATE; }
    else { d.label = "SEED"; d.fmt = F_INT; d.max = 127; }
    return &d;
}
/* a knob of the pages turned (its value already in): ON from a blank one, its first sound (clay_defaults) */
static void clay_put(uint32_t idx, int32_t was, int32_t v)
{
    uint32_t tr = idx / CLAY_NF % NTRK, f = idx % CLAY_NF, m, any = 0;
    if (f != CL_ON || !v || was)
        return;
    for (m = 0; m < 4u; m++) any |= clay[tr][CL_MF(m, 3u)] != 0;
    if (!any)
        clay_defaults(clay[tr]);
    ui_message("DRONE CLAY ON");
}
/* (a FORM part of an older project or user preset: its modulators into DRONE CLAY, ON; the caller makes it WAVE) */
static void clay_from_form(uint32_t tr, const int16_t *p)
{
    static const uint8_t DM[FD_N] = {1u, 13u, 14u, 3u, 3u, 4u, 15u};   /* PITCH, FM E2, FOLD E3, NOISE SHAPE, SHAPE, LEVEL VOLUME, RATIO E4 */
    int16_t *c = clay[tr % NTRK];
    uint32_t m;
    for (m = 0; m < CLAY_NF; m++) c[m] = 0;
    c[CL_ON] = 1; c[CL_OCT] = 1; c[CL_RUN] = p[P_LRTRG] == 2;
    c[CL_STEP] = (int16_t)clamp(p[P_E0], 0, 3); c[CL_SEED] = (int16_t)clamp(p[P_E1], 0, 127);
    for (m = 0; m < 4u; m++) {
        uint32_t ds = form_vds(p, m), s = (ds & 511u) >> 3, g = s / 12u;
        c[CL_MF(m, 0u)] = (int16_t)(g == FPG_NOTE ? 0u : g == FPG_HIT ? 2u : g == FPG_FM ? 13u : DM[ds & 7u]);
        c[CL_MF(m, 1u)] = (int16_t)clay_sh_rank(s);
        c[CL_MF(m, 2u)] = (int16_t)((uint32_t)clamp(p[FORM_MF[m][1]] + 64, 0, 174) % 25u);
        c[CL_MF(m, 3u)] = (int16_t)clamp(p[FORM_MF[m][2]], 0, 127);
        c[CL_DEC + m] = (int16_t)clamp(p[FORM_MF[m][3]], 0, 127);
    }
}

/* (BYTE, the owner: "what each key does", as FORM's keys) DRONE CLAY ON: the keys are its gestures, not notes (seq.c
 * key_on / key_off). The whites the shapes of the OCT's page (0 the HITS, +1 the TABLES, +2 the GESTURES, -1 the NOTES,
 * -2 FM) into the chosen MOD, their velocity its amount; the blacks MOD 1..4 (tapped: chosen; held: the whites into it),
 * LATCH x2 /2 INV; the last seven the drone's degree. Each a card (clay_card) */
static const uint8_t CLAY_KMAP[27] = {0, 16, 1, 17, 2, 18, 3, 4, 19, 5, 20, 6, 7, 21, 8, 22, 9, 23, 10, 11, 32, 33, 34, 35, 36, 37, 38};
static const uint8_t CLAY_KPG[5] = {FPG_FM, FPG_NOTE, FPG_HIT, FPG_TAB, FPG_GES};   /* (OCT -2 .. +2) */
static const uint8_t CLAY_DEG[7] = {0, 2, 3, 5, 7, 8, 10};
static void clay_kev(uint32_t tr, uint32_t kind, uint32_t a, uint32_t b, uint32_t held)
{
    clay_ke.tr = (uint8_t)tr; clay_ke.kind = (uint8_t)kind; clay_ke.a = (uint8_t)a; clay_ke.b = (uint8_t)b;
    clay_ke.held = (uint8_t)held;
    clay_ke.seq++;
}
static void clay_key(track_t *t, uint32_t k, uint32_t vel, int on)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, c = CLAY_KMAP[k % 27u], i;
    form_st_t *st = &form_st[tr];
    if (c >= 32u) {                                     /* the drone's degree */
        if (on) { clay_kr[tr] = (int8_t)CLAY_DEG[c - 32u]; clay_kev(tr, 4u, c - 32u, 0u, 1u); }
        return;
    }
    if (c >= 16u) {
        uint32_t b = c - 16u;
        if (b < 4u) {                                   /* MOD 1..4: held, the whites into it; tapped, chosen */
            if (on) { st->hmod = (int8_t)(b + 1u); st->hdown = st->clk; clay_kev(tr, 2u, b, 1u, 1u); }
            else {
                if (st->clk - st->hdown < (3u << 15)) clay_km[tr] = (uint8_t)b;
                st->hmod = 0;
                clay_kev(tr, 2u, b, 0u, 0u);
            }
            return;
        }
        {
            uint8_t bit = (uint8_t)(1u << (b - 4u));
            if (on) st->modes |= bit; else st->modes &= (uint8_t)~bit;
            if (on) clay_kev(tr, 3u, b - 4u, 1u, 1u);
        }
        return;
    }
    if (on) {
        uint32_t sh = (uint32_t)CLAY_KPG[clamp(song.octave, -2, 2) + 2] * 12u + c, m = st->hmod > 0 ? (uint32_t)st->hmod - 1u : clay_km[tr] & 3u;
        if (sh >= FORM_NSH || sh == 49u) return;          /* (FM x1: none) */
        for (i = 0; i < FORM_G; i++)
            if ((st->g[i].fl & 10u) == 10u && st->g[i].key == k + 1u) { st->g[i].fl &= (uint8_t)~3u; return; }   /* (a latched one: let go) */
        form_spawn(st, sh, m, vel, 1u | ((st->modes & 1u) ? 2u : 0u), 0u, k + 1u);
        clay_kev(tr, 1u, sh, m, 1u);
    } else {
        for (i = 0; i < FORM_G; i++)
            if ((st->g[i].fl & 8u) && st->g[i].key == k + 1u && !(st->g[i].fl & 2u)) st->g[i].fl &= (uint8_t)~1u;
        if (clay_ke.tr == tr) clay_ke.held = 0;
    }
}
/* seq.c key_on / key_off: op 1 down, 0 up; 1 if DRONE CLAY took the key */
static __attribute__((noinline)) int clay_kb(uint32_t k, track_t *t, int op)
{
    if (!clay_on((uint32_t)(t - trk) % NTRK) || drum_track(t))
        return 0;
    if (op == 1) { clay_key(t, k, KB_VEL(k), 1); kb_on[k] = 1; kb_sn[k] = 0; }
    else { kb_on[k] = 0; clay_key(t, k, 0, 0); }
    return 1;
}
static const char *const CLAY_HOW[55]={"SWAYS DEST|EVERY 4 STEPS","DEST JUMPS TO|RANDOM EACH STEP","DEST WANDERS|SLOWLY, RANDOM","DEST WOBBLES|AND SETTLES","DEST CLIMBS 4|STAIRS, 16 STEPS","DEST BOUNCES|FASTER, SMALLER","DEST RISES SLOW|THEN FAST","DEST FALLS FAST|THEN SLOW","DEST RANDOM|EVERY HALF STEP","DEST RAMPS UP|OVER 64 STEPS","DEST OPENS AT|RANDOM, 1 IN 4","DEST GATES|IN TRIPLETS","DEST UP, FALLS|IN HALF A STEP","DEST SNAPS UP|SHORT FALL","DEST TICKS UP|VERY SHORT","DEST UP, RINGS|HALF A STEP","DEST FLUTTERS UP|THEN FALLS","DEST CLICKS UP|VERY SHORT","DEST UP, FALLS|IN 1/4 STEP","DEST ZAPS UP|1/4 STEP FALL","DEST BLIPS UP|VERY SHORT","DEST UP, FADES|OVER A STEP","DEST SWELLS UP|OVER 8 STEPS","DEST UP, LONG|SLOW FADE","DEST RISES|EVERY 4 STEPS","DEST FALLS|EVERY 4 STEPS","DEST UP AND DOWN|EVERY 4 STEPS","DEST ON AND OFF|EVERY 2 STEPS","DEST RISES IN 4|STEPS, STAYS","DEST JUMPS UP|THEN FADES","DEST BUILDS SLOW|OVER 16 STEPS","DEST ON AND OFF|EVERY HALF STEP","DEST STUTTERS|FASTER, LOUDER","PUSHES DEST DOWN|WHILE HELD","PUSHES DEST UP|WHILE HELD","PULLS DEST DOWN|WHILE HELD","NOTES 0 3 7 12|DEST STEPS UP","NOTES 12 7 3 0|DEST STEPS DOWN","ARP UP AND DOWN|DEST FOLLOWS","RANDOM NOTES|DEST JUMPS","MINOR STRUM|DEST TREMBLES","MAJOR STRUM|DEST TREMBLES","SUS4 STRUM|DEST TREMBLES","MINOR 7TH STRUM|DEST TREMBLES","CLUSTER STRUM|DEST SHIVERS","AN OCTAVE NOTE|DEST UP, FULL","A FIFTH UP NOTE|DEST UP 7/12","NOTE GLIDES DOWN|DEST SLIDES DOWN","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES","HOLDS DEST UP|THEN FADES"};
static uint8_t clay_cs __attribute__((section(".eu_keep"))), clay_cv __attribute__((section(".eu_keep")));
static uint32_t clay_cms __attribute__((section(".eu_keep")));
/* (the UI's: ui_input.c clay_card_poll) */
static int clay_card_on(void) { return clay_cv; }
static const char *const CLAY_MODE[4] = {"LATCH", "x2 FASTER", "/2 SLOWER", "INVERTED"};
/* the card's words for the last key: line a (what), line b (how); 0 none */
static int clay_card_text(char *a, char *b)
{
    const track_t *t = &trk[clay_ke.tr % NTRK];
    int16_t *dv;
    a[0] = b[0] = 0;
    if (clay_ke.kind == 1u) {                           /* a gesture: its shape, MOD, where it goes; how */
        uint32_t sh = clay_ke.a % FORM_NSH, m = clay_ke.b & 3u, j;
        const param_desc_t *dd = clay_desc(t, CL_MF(m, 0u), &dv);
        str_cpy(a, FORM_SHAPE[sh], 24);
        str_cpy(a + str_len(a), " \xB7 M", 24 - str_len(a));
        j = str_len(a); a[j] = (char)('1' + m); a[j + 1u] = 0;
        str_cpy(a + str_len(a), " \xB7 ", 24 - str_len(a));
        str_cpy(a + str_len(a), sh / 12u == FPG_NOTE ? "NOTES" : dd && dd->names ? dd->names[clamp(clay[t - trk][CL_MF(m, 0u)], dd->min, dd->max)] : "-", 24 - str_len(a));
        {
            const char *h = CLAY_HOW[sh], *p = h;
            uint32_t n = 0;
            while (*p && *p != '|' && n < 39u) b[n++] = *p++;
            if (*p == '|' && n < 38u) { b[n++] = ' '; p++; while (*p && n < 39u) b[n++] = *p++; }
            b[n] = 0;
        }
    } else if (clay_ke.kind == 2u) {
        str_cpy(a, "MOD ", 24); a[4] = (char)('1' + (clay_ke.a & 3u)); a[5] = 0;
        str_cpy(b, clay_ke.b ? "HELD: THE WHITES GO INTO IT" : "CHOSEN: THE WHITES GO INTO IT", 40);
    } else if (clay_ke.kind == 3u) {
        str_cpy(a, CLAY_MODE[clay_ke.a & 3u], 24);
        str_cpy(b, "WHILE HELD: THE NEXT GESTURES", 40);
    } else if (clay_ke.kind == 4u) {
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        str_cpy(a, "DRONE ", 24);
        str_cpy(a + 6, NN[((uint32_t)clamp(t->p[P_ROOT], 0, 11) + CLAY_DEG[clay_ke.a % 7u]) % 12u], 18);
        str_cpy(b, "THE DRONE ON THE SCALE'S DEGREE", 40);
    } else
        return 0;
    return 1;
}
