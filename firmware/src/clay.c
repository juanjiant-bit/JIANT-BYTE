/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE) DRONE CLAY, a track's sequencer (the owner: FORM out as an engine; "the voice the track has, a drone,
 * endless timbres"). Each audio block, before the parts (fx.c mix_block, after the macros): for every track with it
 * ON and running (RUN SONG: while the transport plays; FREE: always), FORM's composer (eng_form.c) on the track's
 * settings: each modulator's euclid strikes a gesture on its own RATE's steps (as FORM's MOD RATE), on the bar's grid;
 * the recorded loop (the keys with REC armed) strikes its own on the 1/16; the gestures summed into what each
 * modulator adds now (clay_vk, mod.c mod_begin moves its DEST with it), a HIT also the voice as a drum (st->hb: its
 * pitch, noise, CUTOFF, the drone ducked between hits; a DRUM track's own pad); a NOTE gesture (ARP, CHORD ..) a real
 * note on the voice, on the scale, two octaves over the drone; the drone itself, a very low note (C0 .. C3 on the
 * track's ROOT, PITCH up or down) held, or struck again every RATE. Off, stopped, muted: its notes end. Included by
 * seq.c (its notes, the scale) */
static void ui_message(const char *s);                  /* (ui.c) */
static uint32_t clay_free;                              /* (FREE stopped: its own beat, Q16) */
/* (BYTE, the owner: the keys, as FORM's) the keys' state: the MOD chosen (the pages' MOD too), the drone's degree (a
 * root key's), the last key's event for its card (seq: changes per event; kind 1 a gesture, 2 a MOD, 3 a mode, 4 the
 * root; a, b its values); written by the audio's keys (core 0) and the pages, read by the UI */
static int16_t clay_km[NTRK] __attribute__((section(".eu_keep")));
static int8_t clay_kr[NTRK] __attribute__((section(".eu_keep")));
static struct { uint8_t seq, tr, kind, a, b, held; } clay_ke __attribute__((section(".eu_keep")));
static uint32_t clay_b0[NTRK] __attribute__((section(".eu_keep"))), clay_pq[NTRK] __attribute__((section(".eu_keep")));   /* (a punch-in's restart: the time it began; what was held) */
static uint32_t clay_rng __attribute__((section(".pool")));   /* (zeroed: clay_tick's noise) */
/* (main.c, at power-on: RAMTEXT's room is not zeroed) */
static void clay_boot(void)
{
    memset(clay_km, 0, sizeof clay_km); memset(clay_kr, 0, sizeof clay_kr); memset(&clay_ke, 0, sizeof clay_ke);
    memset(clay, 0, sizeof clay); memset(clay_dn, 0, sizeof clay_dn);
    memset(clay_nn, 0, sizeof clay_nn); memset(clay_lr, 0, sizeof clay_lr);
    memset(clay_b0, 0, sizeof clay_b0); memset(clay_pq, 0, sizeof clay_pq);
    memset(clay_rec, 0, sizeof clay_rec);
}
static void clay_stop(uint32_t tr)
{
    track_t *t = &trk[tr];
    form_st_t *st = &form_st[tr];
    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
    if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
    if (st->pn) seq_note_off(t, st->pn);
    clay_dn[tr] = clay_nn[tr] = st->pn = st->rt = 0;
    st->mv[0] = st->mv[1] = st->mv[2] = st->mv[3] = 0;
    st->hb.on = 0;
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
/* (a review: the degree keys ignored the scale) degree d of the track's SCALE over its ROOT, in semitones: its d-th
 * note (a scale of fewer than seven: on into the octave above) */
static int32_t clay_degree(const track_t *t, uint32_t d)
{
    uint32_t mask = scale_mask(t) & 0xFFFu, n = 0, i, k;
    if (!mask) mask = 0xFFFu;
    for (i = 0; i < 12u; i++) n += (mask >> i) & 1u;
    k = d % n;
    for (i = 0; i < 12u; i++)
        if (((mask >> i) & 1u) && !k--) break;
    return (int32_t)(i + 12u * (d / n));
}
/* a HIT struck (sh FORM's shape, a HIT's): a DRUM track's own pad (its GM note, let go a few blocks on); a voice that
 * does not sustain (SUS under 3/4: a pluck, any engine's) the drone struck again with it */
static void clay_hit_strike(uint32_t tr, track_t *t, uint32_t sh, uint32_t vel)
{
    form_st_t *st = &form_st[tr];
    if (drum_track(t)) {
        if (st->pn) seq_note_off(t, st->pn);
        st->pn = CLAY_HIT[sh % 12u].gm;
        st->pt = 8u;
        seq_note_on(t, st->pn, clamp(vel, 1, 127));
    } else if (t->p[P_SUS] < 96 && vel > st->rt)
        st->rt = (uint8_t)vel;
}
/* modulator m strikes its SHAPE (a euclid's, the loop's): a gesture held for its DECAY, then let go */
static void clay_strike(uint32_t tr, track_t *t, form_st_t *st, uint32_t m, uint32_t sh, uint32_t vel)
{
    form_spawn(st, sh, m, vel, 0u, 64u + (uint32_t)clamp(clay[tr][CL_MF(m, 4u)], 0, 127) * 32u, 0u);
    form_lit[tr] |= (uint8_t)(1u << m);
    if (sh / 12u == FPG_HIT) clay_hit_strike(tr, t, sh, vel);
}
/* (the owner: "the keyboard to RECORD the modulators") a key's gesture or degree into the loop, REC armed and the
 * transport playing: on the nearest 1/16 (over a struck one), as a step's note is recorded */
static void clay_rec_put(uint32_t tr, const track_t *t, uint32_t code)
{
    form_st_t *st = &form_st[tr];
    uint32_t p = div_samples(2), s;
    if (!rec_on(t) || clk_pos == CLK_START)
        return;
    s = clk_n + (clk_pos * 2u >= p ? 1u : 0u);
    clay_rec[tr][s % CLAY_RN] = (uint8_t)code;
    if (s != clk_n)                                     /* (ahead: played now, not again when the loop gets there) */
        st->rk = (uint8_t)(s % CLAY_RN + 1u);
}
/* the recorded loop on the transport's 1/16, two bars round */
static void clay_play(uint32_t tr, track_t *t, form_st_t *st)
{
    uint32_t s = clk_n, j = s % CLAY_RN, ev, i, rm = 0;
    if (s == st->rp)
        return;
    st->rp = s;
    for (i = 0; i < CLAY_RN; i++)
        if (clay_rec[tr][i] >= 8u) rm |= 1u << ((clay_rec[tr][i] - 8u) / 55u & 3u);
    st->rm = (uint8_t)rm;
    if (st->rk == j + 1u) {
        st->rk = 0;
        return;
    }
    ev = clay_rec[tr][j];
    if (!ev)
        return;
    if (ev < 8u) {
        clay_kr[tr] = (int8_t)clay_degree(t, (ev - 1u) % 7u);
        return;
    }
    ev -= 8u;
    clay_strike(tr, t, st, ev / 55u & 3u, ev % 55u, (j & 3u) ? 100u : 120u);
}
static int func_kb(uint32_t k, track_t *t, int op);   /* (funcplay.c: the selected FUNC part's keys, after DRONE CLAY's) */
static __attribute__((noinline)) void clay_tick(uint32_t n)
{
    uint32_t s16 = div_samples(2), bps = beat_samples(), b, tr, k, playing = song.playing && clk_pos != CLK_START;
    if (playing)                                        /* (Q16 beats from PLAY: the bar's start; wraps at 16384 bars) */
        b = ((clk_n & 0x3FFFFu) << 14) + (clk_pos << 14) / (s16 ? s16 : 1u);
    else
        b = clay_free += (n << 16) / (bps | 1u);
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        const clay_t *c = clay[tr];
        form_st_t *st = &form_st[tr];
        uint32_t end = b, seed = (uint32_t)clamp(c[CL_SEED], 0, 127) * 977u + 1u, root, dn, drum = drum_track(t), duck = 0;
        clay_out_t o;
        if (st->pt && !--st->pt && st->pn) {   /* (a DRUM track's pad let go) */
            seq_note_off(t, st->pn);
            st->pn = 0;
        }
        if (!playing)
            st->rp = ~0u;
        if (!c[CL_ON] || tr >= trk_live || t->p[P_MUTE] || (!song.playing && !(c[CL_RUN] & 1) && !clay_klive(st))) {
            if (clay_dn[tr] | clay_nn[tr] | st->pn | form_lit[tr] | st->hb.on) clay_stop(tr);
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
        if (pt_ops & PT_TIME_OPS)                       /* (the white punch-ins: the composition's time, as FORM's) */
            end = pt_time(end >> 4, 0u) << 4;
        st->clk += (n << 18) / (bps ? bps : 1u);
        if (playing)                                    /* the recorded loop */
            clay_play(tr, t, st);
        for (k = 0; k < 4u; k++) {                     /* each modulator on its own RATE's steps: its euclid strikes */
            uint32_t eu = (uint32_t)clamp(c[CL_MF(k, 2u)], 0, CLAY_NEU - 1), s1 = clay_step(end, (uint32_t)clamp(c[CL_MF(k, 5u)], 0, CLAY_NRES - 1));
            if (clay_is_hit(c, k) && c[CL_MF(k, 3u)] > 0 && (eu || (playing && ((st->rm >> k) & 1u))) && c[CL_MF(k, 3u)] > (int32_t)duck)
                duck = (uint32_t)c[CL_MF(k, 3u)];      /* (a HIT on the beat: the drone ducked by its AMOUNT between) */
            if ((int16_t)s1 == st->lastm[k]) continue;
            st->lastm[k] = (int16_t)s1;
            form_lit[tr] = (uint8_t)(form_lit[tr] & ~(1u << k));
            if (c[CL_MF(k, 3u)] > 0 && (c[CL_MF(k, 0u)] || clay_sh(c[CL_MF(k, 1u)]) / 12u == FPG_HIT || clay_sh(c[CL_MF(k, 1u)]) / 12u == FPG_NOTE) && form_hit(eu, k, s1, seed))   /* (AMOUNT 0, or
                                                         * no DEST but a HIT's or a NOTE's: nothing struck, the drone clean) */
                clay_strike(tr, t, st, k, clay_sh(c[CL_MF(k, 1u)]), (s1 & 3u) ? 90u : 127u);
        }
        clay_eval(st, c, &o, t->at);
        root = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        dn = (uint32_t)clamp(12 * (clamp(c[CL_OCT], 0, 3) + 1) + (int32_t)root + (hm_n && !hm_rootkey(tr) ? hm_semi(t) : clay_kr[tr]) +
                             clamp(c[CL_PIT], -24, 36), 1, 127);   /* (BYTE: SCL > HARMONY's root, unless a root key is held) */
                                                         /* the drone: C0 .. C3 on the ROOT (+ a root key's degree), PITCH */
        if (drum)                                       /* (a DRUM track: its pads, no drone) */
            st->hb.on = 0;
        else {                                          /* the HITs' bundle (mod.c clay_mod) */
            int32_t dq = (int32_t)duck * 258, hg = clamp(o.hg, 0, 32767), pit = 0;
            if (o.ha) {                                 /* (the loudest's pitch, back to the drone's as it fades) */
                int32_t w = o.ha * 3 > 32767 ? 32767 : o.ha * 3;
                pit = (o.hp - (int32_t)dn * 16) * w >> 15;
            }
            if (o.hn > 0) {                             /* (the noise: the pitch thrown each block) */
                int32_t nz = clamp(o.hn >> 11, 0, 30 * 16);
                clay_rng = clay_rng * 1664525u + 1013904223u;
                pit += ((int32_t)(clay_rng >> 24) - 128) * nz >> 7;
            }
            st->hb.pit = (int16_t)clamp(pit, -2047, 2047);
            st->hb.gain = (int16_t)clamp(32767 - dq + (dq * hg >> 15), 0, 32767);
            st->hb.cut = (int16_t)((clamp(o.hc, -1966080, 1966080) >> 6) * 5 >> 3);   /* (% x Q15 -> Q15 of the range; no
                                                         * constant in the chip ROM's addresses: build.py) */
            st->hb.on = (int16_t)(duck || o.ha);
        }
        {
            uint32_t r = (uint32_t)clamp(c[CL_RATE], 0, CLAY_NRES), cnt = r ? clay_step(end, r - 1u) : 0u, j, busy = 0, rt = st->rt;
            st->rt = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (the owner: the sequencers live with it) one voice: a step's or a
                                                         * key's note takes it, the drone back after */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (drum) {                                 /* (a DRUM track: the drone its kick pad, struck each RATE step,
                                                         * HOLD each bar; DRONE, PITCH and the degree its tuning) */
                uint32_t c2 = clay_step(end, r ? r - 1u : 6u);
                if (!clay_dn[tr] || (uint16_t)c2 != clay_lr[tr]) {
                    int8_t dp = t->dpit;
                    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                    t->dpit = (int8_t)clamp((int32_t)dn - 24, -48, 48);
                    seq_note_on(t, 36u, 110u);
                    t->dpit = dp;
                    clay_dn[tr] = 36u;
                }
                cnt = c2;
            } else if (busy) {
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                clay_dn[tr] = 0;
            } else if (clay_dn[tr] != dn || (r && ((uint16_t)cnt != clay_lr[tr] || rt))) {   /* new, moved, its RATE's strike, a HIT's
                                                         * (HOLD: held, never struck again) */
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                seq_note_on(t, dn, rt ? rt : 110u);
                clay_dn[tr] = (uint8_t)dn;
            }
            clay_lr[tr] = (uint16_t)cnt;
        }
        {   /* a NOTE gesture alive: its note on the voice, on the scale, two octaves over the drone */
            int32_t sem = o.esum > 0 ? o.sem * 32767 / (o.esum > 32767 ? o.esum : 32767) : 0;
            uint32_t nn = 0, j, busy = drum;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (one voice: a step's or a key's note before a gesture's) */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (o.esum > 3000 && !busy)
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
 * edit_param, then clay_put); MOD (CL_SEL) the modulator the pages' others edit (clay_page_desc) */
static const char *const CL_ONN[] = {"OFF", "ON", 0};
static const char *const CL_RUNN[] = {"SONG", "FREE", 0};
static const char *const CL_KEYN[] = {"ON", "OFF", 0};
static const char *const CL_MODN[] = {"1", "2", "3", "4", 0};
#define CL_NN N_NOTE                                   /* (eumel, flash: params.c's) */
static const param_desc_t *clay_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    static const char *nm[CLAY_ND + 1] __attribute__((section(".eu_keep"))), *on[5] __attribute__((section(".lowbuf")));   /* (DEST's: the sound's knobs by name) */
    static char octn[4][4] __attribute__((section(".lowbuf")));
    static param_desc_t d __attribute__((section(".lowbuf")));
    static const char *const BASE[CLAY_ND] = {"OFF", "PITCH", "CUTOFF", "SHAPE", "VOLUME", "PAN", "DIST", "CHORUS", "DELAY",
                                              "REVERB", "LFO RATE", "VIBRATO", 0, 0, 0, 0, 0, 0, 0, 0, "VOICE CUT", "LEVEL", "LIFE"};
    static const char *const EN[8] = {"E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    d.def = 0; d.names = 0; d.fmt = F_ENUM; d.min = 0;
    if (f == CL_SEL) {                                   /* (the owner: one MOD, scrolled, as the sequences are) */
        *valp = &clay_km[tr];
        d.label = "MOD"; d.max = 3; d.names = CL_MODN;
        return &d;
    }
    if (f == CL_KEYS) {                                  /* (as JIANT FM: RUN's bit 1, 0 ON) */
        clay_ui[CLAY_NF] = (int16_t)((clay[tr][CL_RUN] >> 1) & 1);
        *valp = &clay_ui[CLAY_NF];
        d.label = "KEYS"; d.max = 1; d.names = CL_KEYN;
        return &d;
    }
    f %= CLAY_NF;
    clay_ui[f] = (int16_t)(f == CL_RUN ? clay[tr][f] & 1 : clay[tr][f]);                           /* (its int16 view: ui_input.c edit_param writes it, clay_put back) */
    *valp = &clay_ui[f];
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
    } else if (f == CL_RATE) { d.label = "RATE"; d.max = CLAY_NRES; d.names = CLAY_RESN; }
    else if (f == CL_RUN) { d.label = "RUN"; d.max = 1; d.names = CL_RUNN; }
    else if (f == CL_PIT) { d.label = "PITCH"; d.fmt = F_SEMI; d.min = -24; d.max = 36; }   /* (FORM's FREQ: -24 .. +36) */
    else if (f == CL_SEED) { d.label = "SEED"; d.fmt = F_INT; d.max = 127; }
    else {
        uint32_t k = (f - CL_M0) % 6u;
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
        } else if (k == 2u) { d.label = "EUCLID"; d.max = CLAY_NEU - 1; d.names = CLAY_EU; }
        else if (k == 3u) { d.label = "AMOUNT"; d.fmt = F_PCT; d.max = 127; d.def = 100; }
        else if (k == 4u) { d.label = "DECAY"; d.fmt = F_PCT; d.max = 127; }
        else { d.label = "RATE"; d.max = CLAY_NRES - 1; d.def = 2; d.names = CLAY_RESN + 1; }   /* (its own, as FORM's MOD RATE) */
    }
    return &d;
}
/* (params.c page_desc) the pages' knobs: MOD 1's fields stand for the chosen modulator's (as FORM's MOD page) */
static const param_desc_t *clay_page_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    if (f >= CL_M0 && f < CL_M0 + 6u)
        f = CL_MF((uint32_t)clay_km[(uint32_t)(t - trk) % NTRK] & 3u, f - CL_M0);
    return clay_desc(t, f, valp);
}
/* a knob of the pages turned (its view's value already in): into the track's, ON from a blank one its first sound
 * (clay_defaults) */
static void clay_put(uint32_t tr, uint32_t f, int32_t was, int32_t v)
{
    uint32_t m, any = 0;
    tr %= NTRK;
    if (f >= CLAY_NF || f == CL_RUN) {                  /* (RUN bit 0, KEYS bit 1: the other kept) */
        uint32_t b = f == CL_RUN ? 1u : 2u;
        clay[tr][CL_RUN] = (clay_t)((clay[tr][CL_RUN] & ~b) | (v ? b : 0u));
        return;
    }
    clay[tr][f] = (clay_t)clamp(v, -128, 127);
    if (f != CL_ON || !v || was)
        return;
    for (m = 0; m < 4u; m++) any |= clay[tr][CL_MF(m, 3u)] != 0;
    if (!any)
        clay_defaults(clay[tr]);
    ui_message("DRONE CLAY ON");
}
/* (BYTE) the stored settings (the project's): PITCH biased; a first DRONE CLAY's ('C' 'Y', 26: ON OCT RATE RUN, MOD 1..4
 * DEST SHAPE EUCLID AMOUNT, their DECAYs, STEP, SEED) as today's: its RATE and EUCLID in the new orders, its STEP every
 * modulator's RATE */
#define CLAY_V1 26u                                    /* (the first layout's count) */
static const clay_t clay_v2_def[CLAY_NF] = {[CL_MF(0, 5u)] = 2, [CL_MF(1, 5u)] = 2, [CL_MF(2, 5u)] = 2, [CL_MF(3, 5u)] = 2};   /* (a shorter block's rest) */
static void clay_pack(const clay_t *c, uint8_t *b)
{
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) b[i] = (uint8_t)clamp(c[i] + (i == CL_PIT ? CL_PITB : 0), 0, 127);
}
static void clay_unpack(const uint8_t *b, clay_t *c)
{
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) c[i] = (clay_t)((int32_t)(b[i] & 127u) - (i == CL_PIT ? CL_PITB : 0));
}
static void clay_from_v1(const uint8_t *o, uint8_t *b)
{
    static const uint8_t RATE1[8] = {0, 9, 8, 7, 6, 5, 4, 3}, STEP1[4] = {4, 3, 2, 1};
    uint32_t m;
    memset(b, 0, CLAY_NF);
    b[CL_ON] = o[0] & 1u; b[CL_OCT] = (uint8_t)(o[1] & 3u); b[CL_RATE] = RATE1[o[2] & 7u]; b[CL_RUN] = o[3] & 1u;
    b[CL_PIT] = CL_PITB; b[CL_SEED] = o[25] & 127u;
    for (m = 0; m < 4u; m++) {
        b[CL_MF(m, 0u)] = o[4u + 4u * m] & 127u; b[CL_MF(m, 1u)] = o[5u + 4u * m] & 127u;
        b[CL_MF(m, 2u)] = CLAY_EU_OLD[o[6u + 4u * m] % 25u]; b[CL_MF(m, 3u)] = o[7u + 4u * m] & 127u;
        b[CL_MF(m, 4u)] = o[20u + m] & 127u; b[CL_MF(m, 5u)] = STEP1[o[24] & 3u];
    }
}
/* (a FORM part of an older project or user preset: its modulators into DRONE CLAY, ON; the caller makes it WAVE) */
static void clay_from_form(uint32_t tr, const int16_t *p)
{
    static const uint8_t DM[FD_N] = {1u, 13u, 14u, 3u, 3u, 4u, 15u};   /* PITCH, FM E2, FOLD E3, NOISE SHAPE, SHAPE, LEVEL VOLUME, RATIO E4 */
    static const uint8_t RT[7] = {0, 6, 5, 4, 3, 2, 1}, STEP1[4] = {4, 3, 2, 1};   /* (FORM's MOD RATE: FORM 1/1 .. 1/32) */
    clay_t *c = clay[tr % NTRK];
    uint32_t m;
    for (m = 0; m < CLAY_NF; m++) c[m] = 0;
    c[CL_ON] = 1; c[CL_OCT] = 1; c[CL_RUN] = p[P_LRTRG] == 2;
    c[CL_SEED] = (int16_t)clamp(p[P_E1], 0, 127);
    for (m = 0; m < 4u; m++) {
        uint32_t ds = form_vds(p, m), s = (ds & 511u) >> 3, g = s / 12u, r = (ds >> 9) % 7u;
        c[CL_MF(m, 0u)] = (int16_t)(g == FPG_NOTE ? 0u : g == FPG_HIT ? 2u : g == FPG_FM ? 13u : DM[ds & 7u]);
        c[CL_MF(m, 1u)] = (int16_t)clay_sh_rank(s);
        c[CL_MF(m, 2u)] = (int16_t)CLAY_EU_OLD[(uint32_t)clamp(p[FORM_MF[m][1]] + 64, 0, 174) % 25u];
        c[CL_MF(m, 3u)] = (int16_t)clamp(p[FORM_MF[m][2]], 0, 127);
        c[CL_MF(m, 4u)] = (int16_t)clamp(p[FORM_MF[m][3]], 0, 127);
        c[CL_MF(m, 5u)] = (int16_t)(r ? RT[r] : STEP1[clamp(p[P_E0], 0, 3)]);
    }
}

/* (BYTE, the owner: "what each key does", as FORM's keys) DRONE CLAY ON: the keys are its gestures, not notes (seq.c
 * key_on / key_off). The whites the shapes of the OCT's page (0 the HITS, +1 the TABLES, +2 the GESTURES, -1 the NOTES,
 * -2 FM) into the chosen MOD, their velocity its amount; the blacks MOD 1..4 (tapped: chosen; held: the whites into it),
 * LATCH x2 /2 INV; the last seven the drone's degree. Each a card (clay_card). REC armed, the transport playing: the
 * gestures and the degrees recorded into the loop (clay_rec_put) */
static const uint8_t CLAY_KMAP[27] = {0, 16, 1, 17, 2, 18, 3, 4, 19, 5, 20, 6, 7, 21, 8, 22, 9, 23, 10, 11, 32, 33, 34, 35, 36, 37, 38};
static const uint8_t CLAY_KPG[5] = {FPG_FM, FPG_NOTE, FPG_HIT, FPG_TAB, FPG_GES};   /* (OCT -2 .. +2) */
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
        if (on) {
            clay_kr[tr] = (int8_t)clay_degree(t, c - 32u);
            clay_kev(tr, 4u, c - 32u, 0u, 1u);
            clay_rec_put(tr, t, 1u + c - 32u);
        }
        return;
    }
    if (c >= 16u) {
        uint32_t b = c - 16u;
        if (b < 4u) {                                   /* MOD 1..4: held, the whites into it; tapped, chosen */
            if (on) { st->hmod = (int8_t)(b + 1u); st->hdown = st->clk; clay_kev(tr, 2u, b, 1u, 1u); }
            else {
                if (st->clk - st->hdown < (3u << 15)) clay_km[tr] = (int16_t)b;
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
        uint32_t pg = CLAY_KPG[clamp(song.octave, -2, 2) + 2], inv = pg == FPG_FM && c >= 7u, sh = pg * 12u + (inv ? c - 7u : c),
                 m = st->hmod > 0 ? (uint32_t)st->hmod - 1u : (uint32_t)clay_km[tr] & 3u;
        /* (a review: at OCT -2 five whites did nothing) FM's seven, then the upper five its first five inverted */
        for (i = 0; i < FORM_G; i++)
            if ((st->g[i].fl & 10u) == 10u && st->g[i].key == k + 1u) { st->g[i].fl &= (uint8_t)~3u; return; }   /* (a latched one: let go) */
        i = form_spawn(st, sh, m, vel, 1u | ((st->modes & 1u) ? 2u : 0u), 0u, k + 1u);
        if (inv) st->g[i].fl |= 4u;
        if (sh / 12u == FPG_HIT) clay_hit_strike(tr, t, sh, vel);
        clay_kev(tr, 1u, sh, m, 1u);
        clay_rec_put(tr, t, 8u + 55u * m + sh);
    } else {
        for (i = 0; i < FORM_G; i++)
            if ((st->g[i].fl & 8u) && st->g[i].key == k + 1u && !(st->g[i].fl & 2u)) st->g[i].fl &= (uint8_t)~1u;
        if (clay_ke.tr == tr) clay_ke.held = 0;
    }
}
/* seq.c key_on / key_off: op 1 down, 0 up; 1 if DRONE CLAY took the key. KEYS OFF (RUN's bit 1): the keys notes (none recorded into
 * the loop); a key up as it went down (a note: its kb_sn) */
static __attribute__((noinline)) int clay_kb(uint32_t k, track_t *t, int op)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    if (!clay_on(tr) || drum_track(t) || (op ? (clay[tr][CL_RUN] & 2) != 0 : kb_sn[k] != 0))
        return func_kb(k, t, op);                       /* (funcplay: then FUNC's, the selected FUNC part's) */
    if (op == 1) { clay_key(t, k, KB_VEL(k), 1); kb_on[k] = 1; kb_sn[k] = 0; }
    else { kb_on[k] = 0; clay_key(t, k, 0, 0); }
    return 1;
}
static const char CLAY_HOW[]="SWAYS DEST|EVERY 4 STEPS\0DEST JUMPS TO|RANDOM EACH STEP\0DEST WANDERS|SLOWLY, RANDOM\0DEST WOBBLES|AND SETTLES\0DEST CLIMBS 4|STAIRS, 16 STEPS\0DEST BOUNCES|FASTER, SMALLER\0DEST RISES SLOW|THEN FAST\0DEST FALLS FAST|THEN SLOW\0DEST RANDOM|EVERY HALF STEP\0DEST RAMPS UP|OVER 64 STEPS\0DEST OPENS AT|RANDOM, 1 IN 4\0DEST GATES|IN TRIPLETS\0A KICK: PITCH|FALLS, LOUD HIT\0A SNARE: BODY|AND RATTLE\0A HAT: HIGH|SHORT NOISE\0AN OPEN HAT|LONGER NOISE\0A CLAP: NOISE|IN BURSTS\0A RIM: HIGH|VERY SHORT\0A TOM: PITCH|FALLS, ROUND\0A ZAP: PITCH|DIVES FAST\0A BIT HAT|HARSH, SHORT\0NOISE FADES|OVER A STEP\0NOISE SWELLS|UP, 8 STEPS\0A CRASH: NOISE|LONG FADE\0DEST RISES|EVERY 4 STEPS\0DEST FALLS|EVERY 4 STEPS\0DEST UP AND DOWN|EVERY 4 STEPS\0DEST ON AND OFF|EVERY 2 STEPS\0DEST RISES IN 4|STEPS, STAYS\0DEST JUMPS UP|THEN FADES\0DEST BUILDS SLOW|OVER 16 STEPS\0DEST ON AND OFF|EVERY HALF STEP\0DEST STUTTERS|FASTER, LOUDER\0PUSHES DEST DOWN|WHILE HELD\0PUSHES DEST UP|WHILE HELD\0PULLS DEST DOWN|WHILE HELD\0NOTES 0 3 7 12|DEST STEPS UP\0NOTES 12 7 3 0|DEST STEPS DOWN\0ARP UP AND DOWN|DEST FOLLOWS\0RANDOM NOTES|DEST JUMPS\0MINOR STRUM|DEST TREMBLES\0MAJOR STRUM|DEST TREMBLES\0SUS4 STRUM|DEST TREMBLES\0MINOR 7TH STRUM|DEST TREMBLES\0CLUSTER STRUM|DEST SHIVERS\0AN OCTAVE NOTE|DEST UP, FULL\0A FIFTH UP NOTE|DEST UP 7/12\0NOTE GLIDES DOWN|DEST SLIDES DOWN\0DEST SWINGS|ONCE IN 2 STEPS\0DEST SWINGS|ONCE A STEP\0DEST SWINGS|1.5 A STEP\0DEST SWINGS|TWICE A STEP\0DEST SWINGS|3 TIMES A STEP\0DEST SWINGS|5 TIMES A STEP\0DEST SWINGS|7 TIMES A STEP";
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
        str_cpy(a, str_nth(FORM_SHAPE, sh), 24);
        str_cpy(a + str_len(a), " \xB7 M", 24 - str_len(a));
        j = str_len(a); a[j] = (char)('1' + m); a[j + 1u] = 0;
        str_cpy(a + str_len(a), " \xB7 ", 24 - str_len(a));
        str_cpy(a + str_len(a), sh / 12u == FPG_NOTE ? "NOTES" : dd && dd->names ? dd->names[clamp(clay[t - trk][CL_MF(m, 0u)], dd->min, dd->max)] : "-", 24 - str_len(a));
        {
            const char *h = str_nth(CLAY_HOW, sh), *p = h;
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
        const char *const *NN = N_NOTE;              /* (params.c's) */
        str_cpy(a, "DRONE ", 24);
        str_cpy(a + 6, NN[((uint32_t)clamp(t->p[P_ROOT], 0, 11) + (uint32_t)clay_degree(t, clay_ke.a % 7u)) % 12u], 18);
        str_cpy(b, "THE DRONE ON THE SCALE'S DEGREE", 40);
    } else
        return 0;
    return 1;
}
