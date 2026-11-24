/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The contiguous ten white keys retain their recorded lane codes.
 * G5 adds a live ensemble profile. The audio block resolves held and recorded
 * keys before LIFE snapshots the active formulas for both render cores. */
static uint8_t pfx_in;                /* a repeat's own note-on (pfx_note lets it through) */
static uint8_t pfx_any;               /* a track has something running (or to put back) */
static uint32_t pfx_blk;              /* blocks: the notes of one block were struck together */
static uint32_t pfx_seed = 0x9E3779B9u;
static struct { int16_t caos[NTRK]; uint8_t on; } pfx_caos;   /* (JIANT BYTE) CAOS MAX: the parts' CAOS, kept a block */
static struct { int16_t keep[4]; uint8_t on; } pfx_life __attribute__((section(".pool")));   /* (V10.7.36) LIFE MAX: the
                                                         * user's LIFE FILTER PITCH FX while it holds them at 100 */

/* (0.6.72) a repeat holds the track: trk_note_on drops its notes (seq.c MIDI OUT sends none of them either) */
static int pfx_holds(const track_t *t) { return !pfx_in && (t->pfx & PFX_REP); }

/* voice.c trk_note_on, before it starts a note: 1 = not played (a repeat holds the track). Remembers the notes
 * struck together; RANDOM draws the note's offset */
static int pfx_note(track_t *t, uint32_t note, uint32_t vel)
{
    if (pfx_in)
        return 0;
    if (t->pfx & PFX_REP)
        return 1;
    if (t->pfx_lblk != pfx_blk) {
        t->pfx_lblk = pfx_blk;
        t->pfx_lnn = 0;
    }
    if (t->pfx_lnn < 4u) {
        t->pfx_ln[t->pfx_lnn] = (uint8_t)note;
        t->pfx_lv[t->pfx_lnn++] = (uint8_t)vel;
    }
    t->pfx_rpit = 0;
    if (t->pfx & PFX_RND) {
        static const int8_t R[8] = {0, 0, 12, -12, 7, -5, 0, 12};
        pfx_seed = pfx_seed * 1664525u + 1013904223u;
        t->pfx_rpit = (int16_t)(16 * (t->engine == ENGI_DRUM ? (int32_t)(pfx_seed >> 29) - 3 : R[pfx_seed >> 29]));
    }
    return 0;
}

/* the notes a repeat sounds end */
static void pfx_hush(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->pfx_snn; i++)
        seq_note_off(t, t->pfx_sn[i]);
    t->pfx_snn = 0;
}

/* a repeat: the notes again (STUTTER), or the next of them (ARP) */
static __attribute__((noinline)) void pfx_fire(track_t *t, int arp)
{
    uint32_t i, n = t->pfx_lnn;
    if (!n && !(arp && t->engine == ENGI_DRUM))         /* (a drum fill needs no notes struck) */
        return;
    if (!n) {                                           /* (0.6.28) .. but a kit with steps: a cleared DRUM track stays
                                                         * silent (the lane's ARP made random fills on it) */
        for (i = 0; i < NSTEP && !(t->step[i].time == ST_NOTE && (t->step[i].n || t->step[i].hit)); i++) {}
        if (i == NSTEP) return;
    }
    pfx_hush(t);
    pfx_in = 1;
    t->pfx_rpit = 0;
    if (arp && t->engine == ENGI_DRUM) {               /* (JIANT 0.4) a fill: each press its own (pfx_seed runs on) */
        static const uint8_t FILL[16] = {38, 38, 40, 38, 50, 48, 47, 45, 43, 41, 42, 42, 46, 39, 37, 36};
        uint32_t r, note;
        pfx_seed = pfx_seed * 1664525u + 1013904223u;
        r = pfx_seed >> 24;
        if (r < 40u) {                                  /* (a rest now and then) */
            pfx_in = 0;
            return;
        }
        note = r < 150u ? FILL[(pfx_seed >> 16) & 3u]  /* the snare, most */
             : r < 215u ? FILL[4u + ((t->pfx_si + ((pfx_seed >> 16) & 1u)) % 6u)]   /* toms, mostly downwards */
             : FILL[10u + ((pfx_seed >> 16) % 6u)];
        t->pfx_si++;
        seq_note_on(t, note, 60u + ((pfx_seed >> 8) & 63u) + (r & 1u ? 0u : 4u));
        t->pfx_sn[0] = (uint8_t)note;
        t->pfx_snn = 1;
    } else if (arp) {
        uint32_t k = t->pfx_si++ % (t->engine == ENGI_DRUM ? n : 3u * n);
        uint32_t note = t->pfx_ln[k % n] + 12u * (k / n);
        note = note > 127u ? 127u : note;
        seq_note_on(t, note, t->pfx_lv[k % n]);
        t->pfx_sn[0] = (uint8_t)note;
        t->pfx_snn = 1;
    } else {
        for (i = 0; i < n; i++) {
            seq_note_on(t, t->pfx_ln[i], t->pfx_lv[i]);
            t->pfx_sn[i] = t->pfx_ln[i];
        }
        t->pfx_snn = (uint8_t)n;
    }
    pfx_in = 0;
}

/* the attacks ATK+ took out (pfx_end puts them back) */
static int16_t pfx_keep[NTRK];
static uint32_t pfx_qon;            /* (JIANT) the effects in (quantized to the 1/16) */

/* (BYTE) GLO > PUNCH SEQ (ui.c psq_*): four rows, each one punch-in (PSQ_FX, the keys' own) on up to 16 steps of RES,
 * free to overlap. While the transport plays, a row's step lit at the playhead holds its punch-in on every track as if
 * its key were held (pfx_block: OR'd with the keys, after their 1/16 quantize; a key held live keeps its effect on
 * whatever the rows say, nothing restarts under it); the step off, or STOP: let go. The state in the pool (.bss is
 * full; the pool got back more than this from project_t, GLO > COMPS' 80 bytes out), set at power-on (main.c
 * psq_boot, before the project's load); written on core 0 only (the pages, a load, the audio's psq_at psq_now) */
static const uint8_t PSQ_FX[PSQ_NFX] = {PF_OCTD, PF_OCTU, PF_HALF, PF_WOB, PF_D16, PF_S8, PF_S16, PF_ATK, PF_ARP, PF_RND,
                                        PF_LP3, PF_LP5, PF_LP7, PF_RING, PF_LMAX};
static const char *const PSQ_NM[PSQ_NFX] = {"OCT-", "OCT+", "HALF", "WOB", "REP", "S8", "S16", "BITS", "MIR", "SCR",
                                            "LOOP3", "LOOP5", "LOOP7", "CAOS", "LIFE"};
typedef struct { uint16_t st[PSQ_ROWS]; uint8_t fx[PSQ_ROWS], len, res; } psq_t;   /* st bit s: step s lit; len 4..16;
                                                         * res 0 1/32, 1 1/16, 2 1/8, 3 1/4 */
_Static_assert(sizeof(psq_t) == PSQ_BYTES, "PUNCH SEQ as stored (project.c 'P' 'Q')");
static psq_t psq __attribute__((section(".pool")));
static volatile uint8_t psq_at __attribute__((section(".pool")));    /* the step at the playhead (0xFF: stopped) */
static volatile uint32_t psq_now __attribute__((section(".pool")));  /* the PF bits the rows hold now */
static uint8_t psq_row __attribute__((section(".pool")));            /* the row the page edits */
static void psq_fix(psq_t *q)                           /* a stored one in range */
{
    static const uint8_t D[PSQ_BYTES] = PSQ_DEF_BYTES;
    uint32_t r;
    for (r = 0; r < PSQ_ROWS; r++)
        if (q->fx[r] >= PSQ_NFX) q->fx[r] = D[2u * PSQ_ROWS + r];
    q->len = (uint8_t)clamp(q->len, 4, 16);
    q->res = q->res > 3u ? 1u : q->res;
}
static void psq_clear(void)                             /* empty (CLEAR ALL, CLEAR PROJECT, an older project) */
{
    static const uint8_t D[PSQ_BYTES] = PSQ_DEF_BYTES;
    memcpy(&psq, D, sizeof psq);
}
static void psq_boot(void) { psq_clear(); psq_at = 0xFF; psq_now = 0; psq_row = 0; }
/* the PF bits the rows hold this block: the step the block ends in (as the keys' quantize: a step starting inside the
 * block holds it whole), on the transport's 1/32s (before events_block clk_pos is the last block's start) */
static __attribute__((noinline)) uint32_t psq_block(uint32_t n)
{
    uint32_t s16, p32, st, r, m = 0;
    if (!song.playing) {
        psq_at = 0xFF;
        psq_now = 0;
        return 0;
    }
    s16 = div_samples(2);
    s16 = s16 ? s16 : 1u;
    p32 = clk_pos == CLK_START ? (n - 1u) * 2u / s16 : clk_n * 2u + (clk_pos + 2u * n - 1u) * 2u / s16;
    st = (p32 >> (psq.res & 3u)) % (uint32_t)clamp(psq.len, 4, 16);
    for (r = 0; r < PSQ_ROWS; r++)
        if ((psq.st[r] >> st) & 1u)
            m |= PF_BIT(PSQ_FX[psq.fx[r] % PSQ_NFX]);
    psq_at = (uint8_t)st;
    psq_now = m;
    return m;
}

/* (BYTE punchfx, the owner: "some punch-in effects are not noticeable with the engines") what a formula's time cannot
 * carry, on every engine, each block from pt_ops (pfx_block): WOBBLE the pitch swayed +-1.5 semitones and MATH's filter
 * (the FILTER page's CUT, 80 steps down at the most) closed and opened once a 1/8 of the transport (from the bar: in
 * phase with it); CAOS MAX the pitch thrown on each 1/16 (an octave, a fifth .. or not; the same throws at the same
 * place of the song) on WAVE, DRUM-W and FUNC (MATH's other WAVEs: its CAOS bends their time). BITS: fx.c pfx_bits,
 * from pt_ops. pt_vp: voice.c and eng_drum.c add it to the voices' pitch a block, never to a note number (MIDI OUT, CLAY's
 * restart: pfx_pit, OCT's). Stopped: on the macros' free beat. A few dozen instructions a block; idle a test */
static __attribute__((noinline)) void pfx_sound(void)
{
    static const int8_t CJ[8] = {0, 12, -12, 7, -5, 19, -7, 24};   /* (CAOS MAX's throws, semitones) */
    uint32_t k, o = pt_ops, bt, s16;
    int32_t w = 0, cj = 0;
    if (!(o & (1u << PT_WOB | 1u << PT_CAOS))) {
        if (pt_wob | pt_cw | pt_vp[0] | pt_vp[1] | pt_vp[2] | pt_vp[3] | pt_vp[4] | pt_vp[5]) {
            memset(pt_vp, 0, sizeof pt_vp);
            pt_wob = pt_cw = 0;
        }
        return;
    }
    s16 = div_samples(2);
    if (song.playing && clk_pos != CLK_START)           /* (the transport's beat, Q16, as mac_tick's) */
        bt = ((uint32_t)clk_n << 16) + ((uint32_t)clk_pos << 16) / (s16 ? s16 : 1u) >> 2;
    else
        bt = mac_free;                                  /* (stopped: the macros' free beat, mod.c, a block old) */
    if (o & (1u << PT_WOB)) {
        int32_t sn = sine_i(bt << 17);
        w = (sn * 24) >> 15;
        pt_cw = (int16_t)(((sn - 32767) * 40) >> 7);    /* (the filter from where it is to 80 steps of CUT down, Q8) */
    } else
        pt_cw = 0;
    if (o & (1u << PT_CAOS))
        cj = CJ[fvm_hash((bt >> 14) * 0x9E3779B1u + 7u) & 7u] * 16;
    pt_wob = (int16_t)w;
    for (k = 0; k < NTRK; k++) {
        uint32_t e = eng_idx(trk[k].engine);
        int thrown = e == ENGI_WAVE || e == ENGI_DRUM || (e == ENGI_LOFI && trk[k].p[P_E1] == RW_FUNC);
        pt_vp[k] = (int16_t)(w + (thrown ? cj : 0));
    }
}

/* each block, before the events (fx.c mix_block). held: the effects held (PF_* bits) */
static __attribute__((noinline)) void pfx_block(uint32_t n, uint32_t held)
{
    uint32_t k, b = beat_samples(), nw, sq = psq_block(n);   /* (BYTE) PUNCH SEQ's */
    uint32_t caos = (held | sq) & PF_BIT(PF_RING), lmax = (held | sq) & PF_BIT(PF_LMAX);
    pfx_blk++;
    if (caos) {                                         /* (JIANT BYTE) CAOS MAX: every formula part at CAOS 127 for this
                                                         * block (pfx_end: back) */
        for (k = 0; k < NTRK; k++) {
            uint32_t e = eng_idx(trk[k].engine);
            pfx_caos.caos[k] = trk[k].p[P_CAOS];
            if (e == ENGI_LOFI || e == ENGI_WAVE)
                trk[k].p[P_CAOS] = 127;
        }
        pfx_caos.on = 1;
        held &= ~PF_BIT(PF_RING);
    }
    if (lmax) {                                         /* (V10.7.36) LIFE MAX: 100 % for this block (pfx_end: back) */
        for (k = 0; k < 4u; k++) {
            pfx_life.keep[k] = life_v[k];
            life_v[k] = 100;
        }
        pfx_life.on = 1;
        held &= ~PF_BIT(PF_LMAX);
    }
    fl_alone = lmax ? 1u : 0u;                          /* (punchfx) a part with no other sounding: its own donor */
    pfx_qon &= held;                                    /* (JIANT) let go: at once; pressed: on the next 1/16 */
    nw = held & ~pfx_qon;
    if (nw) {
        uint32_t s16 = div_samples(2), ph = clk_pos == CLK_START ? 0u : (clk_pos + n) % s16;   /* (the transport's 1/16:
                                                         * before events_block clk_pos is the last block's start) */
        if (!song.playing || !ph || s16 - ph < n)
            pfx_qon |= nw;
    }
    held = pfx_qon | (sq & ~(PF_BIT(PF_RING) | PF_BIT(PF_LMAX)));   /* (PUNCH SEQ's on the step: not quantized again) */
    pt_ops = (held & PF_FORMULA) >> PF_OCTD | (caos ? 1u << PT_CAOS : 0u);   /* (BYTE 0.16) the white keys: the formulas'
                                                         * time (formula_perf.c); (punchfx) CAOS MAX held */
    {   /* (punchfx) LOOP 3 / 5 / 7 on FUNC's own time (formula_perf.c pt_beat_x): from the 1/16 playing as it goes in, its
         * last one (the steps': pfx_lps below), in the formula's P (eng_lofi.c lofi_sync, a block old as the steps') */
        uint32_t lpn = (held >> PF_LP3) & 1u ? 3u : (held >> PF_LP5) & 1u ? 5u : (held >> PF_LP7) & 1u ? 7u : 0u, r;
        if (lpn && song.playing) {
            if (!pt_lpn)
                pt_lp0 = ((lofi_sync(&r) * 8u) & ~16383u) - (lpn - 1u) * 16384u;
            pt_lpn = (uint8_t)lpn;
            pt_ops |= 1u << PT_LOOP;
        } else
            pt_lpn = 0;
    }
    pfx_sound();                                        /* (punchfx) WOBBLE, CAOS MAX on every engine */
    formula_punch = 0;
    held &= ~(PF_FORMULA & ~(PF_BIT(PF_OCTD) | PF_BIT(PF_OCTU)));   /* (G5: COMB again, perform.c's, not the exchange) */

    if (!held && !pfx_any && !nw)
        return;
    pfx_any = 0;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        int drum = t->engine == ENGI_DRUM;
        uint32_t on = held;                             /* (V10.7.36: every track) */
        uint32_t old = t->pfx, f, rep, per = 0;
        rep = on & (PF_BIT(PF_S16) | PF_BIT(PF_S8) | PF_BIT(PF_ARP));
        per = (rep & (PF_BIT(PF_S16) | PF_BIT(PF_ARP))) ? b / 4u : b / 2u;   /* (JIANT 0.4: 1/8, 1/16; ARP 1/16) */
        t->pfx_pit = (int16_t)(((on >> PF_OCTU) & 1u) * 192 - ((on >> PF_OCTD) & 1u) * 192);
        uint32_t lpn = (on >> PF_LP3) & 1u ? 3u : (on >> PF_LP5) & 1u ? 5u : (on >> PF_LP7) & 1u ? 7u : 0u;   /* (snd2) LOOP
                                                         * 3 / 5 / 7 (the shortest held wins) */
        f = ((on >> PF_RND) & 1u ? PFX_RND : 0u) | ((on >> PF_HALF) & 1u ? PFX_HALF : 0u) | (rep ? PFX_REP : 0u) |
            ((on >> PF_ATK) & 1u ? PFX_ATK : 0u) |
            (lpn && song.playing ? PFX_LOOP : 0u);
        /* (p2) 1/2 TEMPO and LOOP are LIVE's RATE /2 and a LOOP (seq_live.c live_q): its ghost keeps where the track
         * would be, let go it is back there */
        if ((f & PFX_LOOP) && !(old & PFX_LOOP)) {      /* (snd2) LOOP N: the last N steps, the one playing the last of
                                                         * them; its next step is the first again (not on the bar: a 3
                                                         * or a 5 against the 4 of the bar) */
            uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
            t->pfx_lps = (uint16_t)(((uint32_t)t->seq_idx % len + lpn * len - (lpn - 1u)) % len);
        }
        if (lpn)
            t->pfx_lpn = (uint8_t)lpn;
        if (rep && !(old & PFX_REP)) {
            t->pfx_si = 0;
            t->pfx_sc = per;                            /* (the first repeat now) */
        } else if (!rep && (old & PFX_REP)) {
            pfx_hush(t);
        }
        t->pfx = (uint8_t)f;
        if (rep) {
            t->pfx_sc += n;
            if (t->pfx_sc >= per) {
                t->pfx_sc = t->pfx_sc - per < per ? t->pfx_sc - per : 0u;
                pfx_fire(t, rep == PF_BIT(PF_ARP));
            } else if (t->pfx_snn && t->pfx_sc >= per / 2u) {
                pfx_hush(t);                            /* gated half */
            }
        }
        if ((f & PFX_ATK) && !drum) {                   /* (JIANT) ATK+: the attack slow (DRUM: eng_drum.c) */
            pfx_keep[k] = t->p[P_ATK];
            t->p[P_ATK] = (int16_t)(t->p[P_ATK] + (127 - t->p[P_ATK]) * 2 / 3);
        }
        if (f || t->pfx_pit)
            pfx_any = 1;
    }
}

/* after the parts: what ATK+ changed, back; (V10.7.36) LIFE MAX's. (punchfx) Each block: idle, its three tests */
static __attribute__((noinline)) void pfx_end(void)
{
    formula_punch = 0;
    pt_ops = 0;
    uint32_t k;
    if (!(pfx_any | pfx_life.on | pfx_caos.on))
        return;
    if (pfx_life.on) {
        for (k = 0; k < 4u; k++)
            life_v[k] = pfx_life.keep[k];
        pfx_life.on = 0;
    }
    if (pfx_caos.on) {                                  /* (JIANT BYTE) CAOS MAX's, back */
        for (k = 0; k < NTRK; k++)
            trk[k].p[P_CAOS] = pfx_caos.caos[k];
        pfx_caos.on = 0;
    }
    for (k = 0; k < NTRK; k++)
        if ((trk[k].pfx & PFX_ATK) && trk[k].engine != ENGI_DRUM)
            trk[k].p[P_ATK] = pfx_keep[k];
}
