/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (funcplay, the owner: "FUNC to play, not a music box") a FUNC part played live, its state in eng_lofi.c fp_t. Every
 * change on the formula's own time, P (Q16 beats; a bar 4 beats, 2^18), so it stays in tempo and on the bar, and comes
 * back as written when let go; nothing kept in the formula (the recipes have no state).
 *   GROOVE   (every recipe, from outside it) DENSITY: its music calls' hits and notes thinned or retriggered each 1/16
 *            and eu()'s steps taken or added, by a hash of the step (fvm_music.c fu_music, fvm_func_run.c fu_eu);
 *            SWING: the odd 1/16s later (P bent inside each 1/8); COMPLEX: a share of the steps read 3 or 13 steps on
 *            (broken, three against four; the same every 4 bars); RANGE: a share of the notes (the calls', and w's
 *            per step) an octave down or up
 *   SECTION  at the next bar line: INTRO (no drums, thinner, CUT down), GROOVE (as written), BUILD (denser bar by bar,
 *            its riser the CUT opening over four bars), DROP (denser), BREAK (no drums, CUT down)
 *   FILL     held, from the next 1/16 to the 1/16 after the key is up: ROLL (each 1/16 its first 1/32, then 1/64),
 *            STUTTER (the 1/8 from its start again), REVERSE (back from its start), DOUBLE (twice as fast from its start)
 *   MUTATE   held, as the fills (from the next 1/16, a tap one 1/16 at least; at the 1/16 after the key is up the formula
 *            as written again, nothing kept): NOTES (a new seed d, the notes moved 0 7 5 12), RHYTHM (the bar's offbeat
 *            1/16s reordered), SWAP (layer A's and B's parts; one layer: the bar's halves), FREEZE (the bar playing when
 *            pressed, again); with a fill held, both. RESET: GROOVE, the mutes and the root back
 *   LAYERS   a second recipe B (its preset's knobs) on the same P, BLEND between them (A .. both at 3/4 .. B), B
 *            muted by its key; two parts at once (fp_b). (PARTS, a part from each layer: out for the flash's room)
 *   ROOT     the last seven keys: the scale's degree, at the next bar
 * The keys (seq.c key_on / key_off: after DRONE CLAY's, the selected FUNC part's alone, KEYS ON and RUN PLAY playing or
 * FREE; a layer button held, SCENE, the DRUM grid first, as ever): whites 1..5 the sections, 6..9 the fills, blacks 1..5
 * the mutations, 6..8 the parts' mutes (DRUMS NOTES B), the last seven the root. (The keys recorded into a loop of
 * four bars: out for the flash's room, kept on branch funcplay-loop) */

/* ---- the render's side (eng_lofi.c lofi_func_render) */
static int func_live(const track_t *t)                 /* anything to do: else the formula as before, bit for bit */
{
    const uint8_t *f = &fp_st[(uint32_t)(t - trk) % NTRK].sec;
    const int16_t *p = t->p;
    uint32_t k, any = (uint32_t)(p[P_FDEN] | p[P_FRNG] | p[P_FSWG] | p[P_FFB] | (p[P_FCPX] ^ 127));
    for (k = 0; k < 10u; k++) any |= f[k];
    any |= fp_st[(uint32_t)(t - trk) % NTRK].mreq;     /* (a mutation pressed: on at the next 1/16) */
    return any != 0;
}
/* the block's: COMPLEX's and RANGE's step (its pick), SWING's two slopes */
typedef struct { uint32_t ck, co, rc, one, seed, S, r1, r2, cpx; int32_t ro, rng; } fp_q_t;
/* real P r -> the formula's */
static __attribute__((noinline)) uint32_t func_tp(const fp_t *f, fp_q_t *q, uint32_t r)
{
    uint32_t x = r, d, b0;
    if (f->mut & FM_FREEZE) x = (f->frz << 18) | (x & 0x3FFFFu);
    if (f->fill && r >= f->fs && (!f->fe || r < f->fe)) {
        d = r - f->fs;
        b0 = f->mut & FM_FREEZE ? (f->frz << 18) | (f->fs & 0x3FFFFu) : f->fs;
        x = f->fill == FF_ROLL ? (x & ~0x3FFFu) | (x & (d < 0x20000u ? 0x1FFFu : 0xFFFu)) : f->fill == FF_STUT ? b0 + (d & 0x7FFFu) :
            f->fill == FF_REV ? (b0 > d ? b0 - d : 0u) : b0 + 2u * d;
    }
    if (f->mut & FM_RHYTHM) x = (x & ~0x3C000u) | ((x >> 14) * 5u & 15u) << 14;
    if ((f->mut & FM_SWAP) && q->one) x ^= 0x20000u;
    if (q->cpx) {
        if (x >> 14 != q->ck) {
            uint32_t h = fvm_hash((x >> 14 & 63u) * 0x9E3779B1u ^ q->seed);
            q->ck = x >> 14;
            q->co = (h & 127u) < q->cpx ? (h >> 8 & 1u ? 3u : 13u) : 0u;
        }
        x = (x & ~0x3C000u) | (((x >> 14) + q->co) & 15u) << 14;
    }
    if (q->S) {
        d = x & 0x7FFFu;
        d = d < 0x4000u + q->S ? d * q->r1 >> 16 : 0x4000u + ((d - 0x4000u - q->S) * q->r2 >> 16);
        x = (x & ~0x7FFFu) | d;
    }
    return x & 0x7FFFFFFFu;
}
static const int8_t FS_DEN[5] = {0, -10, 0, 20, -16}, FS_CUT[5] = {0, -24, -24, 0, -14};
/* the block of FUNC part t: its time through the section, the fill, the groove; A (and B) run on it */
static __attribute__((noinline)) uint32_t func_run(track_t *t, voice_t *v, const fvm_prog_t *pa, fvm_env_t *en, uint32_t n,
                                               uint32_t *Y, uint32_t *W, uint32_t *P)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, s = n, sm = n, a, b, i, k, L, Yb[LOFI_N], fb = (uint32_t)clamp(t->p[P_FFB], 0, 20), fk,
             x = (uint32_t)fp_kp[tr][0] & 127u, gp, PB[LOFI_N], WB[LOFI_N];
    fp_t *f = &fp_st[tr];
    uint32_t r0 = (f->act & 7u) ^ 3u, r1 = (f->act >> 3 & 7u) ^ 3u;   /* (func-rate) the RATEs, B OCT as playing */
    int32_t ob = (int32_t)((f->act >> 6 & 7u) ^ 2u) - 2, tb = fp_kp[tr][FK_BTONE];
    const fvm_prog_t *pb = 0;
    fp_q_t q;
    fu_cx_t cx[1];
    if (P[0] + (1u << 18) < f->lastP) {                 /* (the time went back: PLAY again) the lines from here */
        f->qbar = f->fs = f->fe = f->fill = 0;
        f->sbar = f->frz = P[0] >> 18;
        f->mpf = 0; f->me = 0;                          /* (the mutations' lines from here too) */
    }
    f->lastP = P[n - 1u];
    if (f->sreq | f->dreq) {                            /* a section, a root asked: at the next bar line */
        if (!f->qbar) f->qbar = (P[0] >> 18) + 1u;
        for (s = 0; s < n && P[s] >> 18 < f->qbar; s++) {}
    }
    fk = f->fkey;                                       /* the fill held */
    if (fk && fk != f->fill) {                          /* from the next 1/16 */
        f->fill = (uint8_t)fk;
        f->fs = (P[0] + 0x3FFFu) & ~0x3FFFu;
        f->fe = 0;
    } else if (!fk && f->fill && !f->fe)                /* let go: to the 1/16 after (one at least) */
        f->fe = (((P[0] > f->fs ? P[0] : f->fs) >> 14) + 1u) << 14;
    if (f->fill && f->fe && P[0] >= f->fe) f->fill = 0;
    {   /* (momentary MUTATE) the mutations pressed: on from the next 1/16; let go: off at the 1/16 after (one at least) */
        uint32_t nx = (P[0] + 0x3FFFu) & ~0x3FFFu, te = ~0u;
        f->mreq &= (uint8_t)~f->mut;                    /* (pressed again while still on: on it stays) */
        if ((f->mreq & ~f->mut) && !(f->mpf & 1u)) { f->ms = nx; f->mpf |= 1u; }
        if (f->mut & ~(f->mkey | f->mreq)) {
            if (!(f->mpf & 2u)) { f->mr = nx > f->me ? nx : f->me; f->mpf |= 2u; }
        } else f->mpf &= (uint8_t)~2u;
        if (f->mpf & 1u) te = f->ms;
        if ((f->mpf & 2u) && f->mr < te) te = f->mr;
        for (sm = 0; te != ~0u && sm < n && P[sm] < te; sm++) {}
    }
    for (k = 0; k < 2u && fb; k++)                      /* layer B: its slot (func_b_poll) */
        if (fp_b[k].tr == tr + 1u && fp_b[k].form == fb && fp_b[k].p.n && !(f->mute & 4u)) pb = &fp_b[k].p;
    uint32_t la = func_lv(func_form(t)), lb = pb ? func_lv(fb) : 0u, lm = la;   /* (func-level) each its FORM's level,
                                                         * under the louder's while B sounds (after: lofi_func_render) */
    for (a = 0; a < n; a = b) {
        uint32_t sec, bars, g[2], m, ra = P[a], xx;
        if (a == s) {                                   /* the bar line: the section, the root */
            if (f->sreq) { f->sec = (uint8_t)(f->sreq - 1u); f->sbar = f->qbar; f->sreq = 0; }
            if (f->dreq) { f->deg = (uint8_t)(f->dreq - 1u); f->semi = f->dsemi; f->dreq = 0; }
            f->qbar = 0;
        }
        if (a == sm) {                                  /* (momentary MUTATE) a 1/16's line: the mutations on, off */
            if ((f->mpf & 1u) && ra >= f->ms) {
                if (f->mreq & FM_FREEZE) f->frz = (f->ms - (f->ms != 0u)) >> 18;   /* (FREEZE: the bar it was pressed in) */
                f->mut |= f->mreq; f->mreq = 0; f->me = f->ms + 0x4000u; f->mpf &= (uint8_t)~1u;
            }
            if ((f->mpf & 2u) && ra >= f->mr) { f->mut &= f->mkey | f->mreq; f->mpf &= (uint8_t)~2u; }
        }
        b = n;
        if (s > a && s < b) b = s;
        if (sm > a && sm < b) b = sm;
        xx = (f->mut & FM_SWAP) && pb ? 127u - x : x;
        sec = f->sec % 5u;
        bars = (ra >> 18) - f->sbar;
        bars = sec == FS_BUILD ? (bars < 3u ? bars : 3u) : 0u;
        f->cut = (int8_t)(FS_CUT[sec] + (int32_t)bars * 8);
        memset(&q, 0, sizeof q);
        q.one = !pb;
        q.seed = en->u[FU_D] * 0x85EBCA77u + 0x1234u;
        q.cpx = 127u - (uint32_t)clamp(t->p[P_FCPX], 0, 127);
        q.S = (uint32_t)clamp(t->p[P_FSWG], 0, 127) * 47u;
        q.r1 = (0x4000u << 16) / (0x4000u + q.S);
        q.r2 = (0x4000u << 16) / (0x4000u - q.S);
        q.rng = clamp(t->p[P_FRNG], -64, 64);
        q.ck = q.rc = ~0u;
        m = (sec == FS_INTRO || sec == FS_BREAK) | (f->mute & 3u);
        cx[0].dens = (int8_t)clamp(t->p[P_FDEN] + FS_DEN[sec] + f->gden + (int32_t)bars * 14, -64, 64);
        cx[0].range = (int8_t)q.rng;
        cx[0].notes = (uint8_t)(f->mut & FM_NOTES);
        cx[0].seed = q.seed;
        cx[0].mask = (uint8_t)m;
        for (i = a; i < b; i++) {
            P[i] = func_tp(f, &q, P[i]);
            if (q.rng) {                                /* RANGE on w: the step's octave */
                if (P[i] >> 14 != q.rc) {
                    int32_t h = (int32_t)(fvm_hash((P[i] >> 14) * 0x27D4EB2Fu ^ q.seed) & 127u);
                    q.rc = P[i] >> 14;
                    q.ro = h < 2 * q.rng ? 1 : h < -2 * q.rng ? -1 : 0;
                }
                W[i] = q.ro > 0 ? W[i] << 1 : q.ro < 0 ? W[i] >> 1 : W[i];
            }
        }
        if (pb && r1 != r0) fn_rate(P + a, PB + a, b - a, r1);   /* (func-rate) B on its own RATE, */
        if (pb && ob) for (i = a; i < b; i++) WB[i] = ob > 0 ? W[i] << ob : W[i] >> -ob;   /* its octaves */
        if (r0 != 3u) fn_rate(P + a, P + a, b - a, r0);   /* A on its own */
        gp = v->ph[1] ? v->ph[1] - 1u : xx << 8;        /* (func-smooth) BLEND (Q8) gliding, an eighth a block */
        {   int32_t dx = (int32_t)(xx << 8) - (int32_t)gp; gp = dx >> 3 ? gp + (uint32_t)(dx >> 3) : xx << 8; v->ph[1] = gp + 1u; }
        g[0] = gp < 43u * 256u || !pb ? 32768u : ((127u << 8) - gp) * 387u >> 8;   /* BLEND: A full to a third, both 3/4
                                                         * in the middle */
        lm = gp && lb > la ? lb : la;
        g[0] = g[0] * la / lm;
        g[1] = !pb ? 0u : func_brel(t, fb, gp > 84u * 256u ? 32768u : gp * 387u >> 8, lm);   /* (func-level) at its level, */
        g[1] -= (g[1] >> 2) * (g[0] >> 1) >> 14;        /* under A as much as A plays (-2.5 dB, the owner: "B too strong") */
        for (L = 0; L < 2u; L++) {                      /* A, then B (its preset's knobs) on the same time */
            fvm_env_t e = *en;
            uint32_t *o = L ? Yb : Y;
            if (!g[L]) { if (!L) memset(Y + a, 0, (b - a) * sizeof *Y); continue; }
            if (L) {
                for (k = 0; k < 4u; k++) {
                    uint32_t c = (uint32_t)FUNC_PRESETS[fb - 1u].e[2u + k] & 127u;
                    e.u[FU_A + k] = c * 2u; e.u[FU_AF + k] = c * 65536u / 127u;
                }
                xm_env(&e, pb);
            } else if (f->mut & FM_NOTES) e.u[FU_D] ^= 0x5Au;
            for (k = 0; k < FA_N; k++) e.arr[k] = e.arr[k] ? e.arr[k] + a : 0;
            if (L && r1 != r0) e.arr[FA_BEAT] = PB + a;
            if (L && ob) e.arr[FA_W] = WB + a;
            e.fx = cx;
            e.u[FU_NOTE] = (uint32_t)clamp((int32_t)e.u[FU_NOTE] + (hm_n && func_song(t) && !hm_rootkey(tr) ?
                                           (!L && func_form(t) == FUNC_BANK ? 0 : hm_semi(t)) : f->semi), 0, 127);   /*
                                                         * (funcbank: BANK its own chord, func_bank_chord) */
            if (L) lofi_run(pb, &e, &lofi_work[MC_CPU() & 1u], b - a, o + a);
            else func_arun(t, v, pa, &e, b - a, o + a);
            if (L && tb)                                /* (func-rate) B TONE: a tilt about ~800 Hz, 0 as it is */
                for (i = a; i < b; i++) {
                    int32_t y = (int32_t)Yb[i] >> 1;
                    f->bty += (y - f->bty) * 6700 >> 15;
                    Yb[i] = (uint32_t)((f->bty + ((y - f->bty) * (64 + tb) >> 6)) * 2);
                }
        }
        if (pb || g[0] != 32768u)
            for (i = a; i < b; i++)
                Y[i] = (uint32_t)((((int32_t)Y[i] >> 1) * (int32_t)g[0] >> 14) + (g[1] ? ((int32_t)Yb[i] >> 1) * (int32_t)(g[1] >> 1) >> 13 : 0));
    }
    return lm;
}
/* ---- the keys' (audio, core 0) */
static int32_t clay_degree(const track_t *t, uint32_t d);
/* event ev (1 .. 20) on part tr: 1..5 a section (+1: GROOVE INTRO BUILD DROP BREAK), 6..12 the root's degree, 13..16 a
 * mutation (NOTES RHYTHM SWAP FREEZE) pressed (op 1) or let go (0): held, momentary; 17 RESET, 18..20 a part muted
 * (DRUMS NOTES B), toggled */
static void func_mkey(uint32_t tr, uint32_t ev, int op)
{
    fp_t *f = &fp_st[tr];
    uint8_t m = (uint8_t)(1u << (ev - 13u));
    if (!op) { f->mkey &= (uint8_t)~m; return; }
    f->mkey |= m; f->mreq |= m;
}
static void func_apply(uint32_t tr, uint32_t ev)
{
    fp_t *f = &fp_st[tr];
    if (ev <= 5u) f->sreq = (uint8_t)ev;
    else if (ev <= 12u) { f->dreq = (uint8_t)(ev - 5u); f->dsemi = (int8_t)clay_degree(&trk[tr], ev - 6u); }
    else if (ev == 17u) { f->mut = f->mute = f->mreq = f->mpf = 0; f->sreq = f->dreq = 1; f->dsemi = 0; }
    else if (ev <= 16u) func_mkey(tr, ev, 1);
    else f->mute ^= (uint8_t)(1u << (ev - 18u));
}
static const uint8_t FUNC_SKEY[5] = {2, 1, 3, 4, 5};    /* whites 1..5: INTRO GROOVE BUILD DROP BREAK */
static uint8_t fk_last;                                 /* (the UI's, ui_layer.c fk_map; not fp_t, not .eu_keep) the last */
static uint32_t fk_ms;                                  /* key FUNC took (1 + k), when a key went down or up (fm1_ms) */
/* seq.c key_on / key_off (clay.c clay_kb, DRONE CLAY's first): op 1 down, 0 up; 1 if FUNC took the key: the selected
 * FUNC part's, KEYS ON, running by itself (RUN PLAY playing, FREE); its up the one it took down */
static int func_kb(uint32_t k, track_t *t, int op)
{
    uint32_t tr = song.sel % NTRK, c = CLAY_KMAP[k % 27u], ev;
    fp_t *f = &fp_st[tr];
    if (t != &trk[tr] || !func_on(t) || (op ? fp_kp[tr][1] || !((func_song(t) && song.playing) || func_free(t)) : kb_sn[k] != 0))
        return 0;
    kb_on[k] = (uint8_t)op;
    kb_sn[k] = 0;
    if (op) fk_last = (uint8_t)(k + 1u);
    fk_ms = fm1_ms;
    if (c - 5u < 4u) {                                  /* whites 6..9: a fill, held */
        if (op) f->fkey = (uint8_t)(c - 4u);
        else if (f->fkey == c - 4u) f->fkey = 0;
        return 1;
    }
    ev = c < 5u ? FUNC_SKEY[c] : c >= 32u ? c - 26u : c - 16u < 8u ? c - 3u : 0u;
    if (ev - 13u < 4u) func_mkey(tr, ev, op);           /* blacks 1..4: a mutation, held */
    else if (op && ev) func_apply(tr, ev);
    return 1;
}
/* (funcplay) GEN's hook (none yet): part tr's FUNC to section s (0 GROOVE 1 INTRO 2 BUILD 3 DROP 4 BREAK) at its next
 * bar, its density moved by dens (-64 .. 64) on top of DENSITY's */
static void func_gen_set(uint32_t tr, uint32_t s, int32_t dens)
{
    fp_t *f = &fp_st[tr % NTRK];
    f->sreq = (uint8_t)(s % 5u + 1u);
    f->gden = (int8_t)clamp(dens, -64, 64);
}
/* ---- the main loop's (engines.c math_poll_ws): layer B compiled into a slot of fp_b, published whole */
static void func_b_poll(gq_work_t *w)
{
    uint32_t k = 0, tr, n;
    for (tr = 0; tr < NTRK && k < 2u; tr++) {           /* (the first two FUNC parts with a B, in order) */
        int32_t fb = trk[tr].p[P_FFB];
        const char *s;
        if (trk[tr].eng_req != ENGI_FUNC || fb <= 0 || fb >= (int32_t)FUNC_BANK) continue;
        if (fp_b[k].tr != tr + 1u || fp_b[k].form != (uint32_t)fb) {
            for (s = func_form_text(fb), n = 0; s[n]; n++) {}
            if (fvm_compile(&w->cc, FVM_FULL(&w->pg), s, n, 0) || w->pg.p.nstate) continue;
            MATH_PUBLISH_LOCK();
            fp_b[k].tr = 0;
            memcpy(fp_bins[k], w->pg.ins, w->pg.p.n * sizeof *fp_bins[k]);
            fp_b[k].p = w->pg.p;
            fp_b[k].p.ins = fp_bins[k];
            fp_b[k].form = (uint8_t)fb;
            fp_b[k].tr = (uint8_t)(tr + 1u);
            MATH_PUBLISH_UNLOCK();
        }
        k++;
    }
    for (; k < 2u; k++) fp_b[k].tr = 0;                 /* (a slot let go) */
}

/* ---- (funcbank) BANK: FORM 3's formulas (fvm_bank.c FB_*) on the chord. Their variables, made here a block at a time
 * from FUNC's time P (its RATE, the GROOVE's swing and fills on it already) and the chord:
 *   w w3 w5   the chord's root, third and fifth (turns; their phases the voice's, never reset at a change: fb_ph),
 *             the root in the second octave (C2 .. B2 and up the progression), wd the root 1/128 sharp
 *   T         the 1/16's age (0 .. 1)    s c d  the 1/16s, the bar's 1/16 (0 .. 15), the bars (s c d constant in a run:
 *             the block cut at each 1/16)    P  the beats    t  P x 4096 (bytebeat's time, ~8 kHz at 120 BPM)
 *   D         the chord's age (0 .. 1 over LENGTH)    b  its degree    A  DEF    n  the root's note
 * The chord: SCL > HARMONY's (its LENGTH: 4 BAR a chord each four bars; EXTENS: the third suspended, an add9's, as
 * hm_pick reads it) on PLAY or FREE while the song plays, else the root's own in the part's SCALE (the key's tonic, a
 * key played its note's). MORPH (gliding as the knobs do: fn_knob) between A's and B's samples, equal power, each at
 * its level (FB_LV); B only while it is heard, A only while it is. A's state v->s[6], B's v->s[7] (one each). A is
 * the part's program (math_poll), B its second (wave_prg_b, asked for as it changes: eng_lofi.c func_act; compiled by
 * the main loop, engines.c fvm_serve) */
static const uint16_t FB_SEMI[13] = {4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732, 8192};   /*
                                                         * 2^(k/12), Q12 */
static uint32_t fb_ph[NTRK * NVOICE][4] __attribute__((section(".bss.funcbank")));   /* each voice's root's, third's, fifth's,
                                                         * the sharp root's phase */
#define FB_SZ __attribute__((noinline, minsize))       /* (flash: once a block, not a sample) */
static FB_SZ int32_t fb_dia(const track_t *t, int32_t x, int32_t d) { return hm_dia(t, x, d) - x; }
/* part t's chord as BANK hears it, for note n (the voice's, 60 its root): its root's note (returned), the third and
 * the fifth over it (semitones) and its degree */
static FB_SZ int32_t func_bank_chord(const track_t *t, int32_t n, int32_t *th, int32_t *fi, int32_t *dg)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    int32_t r, a[2], iv[5];
    *dg = 0;
    if (hm_on(t) && (func_song(t) || func_free(t)) && !hm_rootkey(tr)) {   /* SCL > HARMONY's chord now */
        uint32_t i = hm_now();
        int32_t t0 = 48 + clamp(t->p[P_ROOT], 0, 11), d0 = t0 + fb_dia(t, t0, hm_dg[i]);
        (void)hm_pick(t, i, a, th, iv, &r);
        *fi = (int32_t)hm_cls(i, (uint32_t)clamp(fb_dia(t, d0, 4), 0, 11));   /* (its fifth: the scale's, onto its quality) */
        *dg = hm_dg[i];
        r += n - 72;                                    /* (an octave under the key's: C2 .. ; ROOT, the keys' moves on it) */
    } else {                                            /* the root's own chord in the part's scale */
        r = n - 24 + ((func_song(t) || func_free(t)) && !func_pitch[tr] ? clamp(t->p[P_ROOT], 0, 11) : 0);   /* (no
                                                         * key: the key's tonic, as HARMONY's: C2 .. B2) */
        *th = fb_dia(t, r, 2);
        *fi = fb_dia(t, r, 4);
    }
    *th = clamp(*th, 2, 5);
    *fi = clamp(*fi, 6, 8);
    return clamp(r, 12, 96);
}
/* the block's set-up: the chord into e (b n D A), the phases' steps d[4], the gains g[2] (0: that side not run) */
static FB_SZ void fb_setup(const track_t *t, const fvm_env_t *en, fvm_env_t *e, uint32_t *d, int32_t *g, uint32_t fa, uint32_t fb, int hb)
{
    int32_t th, fi, dg, r = func_bank_chord(t, (int32_t)en->u[FU_NOTE], &th, &fi, &dg);
    uint32_t inc = pitch_inc((uint32_t)r * 16u) << VH_SH(), mo = en->u[FU_CF] << 14, h = (uint32_t)clamp(song.g[G_HLEN], 0, 4);
    *e = *en;
    d[0] = inc; d[1] = (uint32_t)(umul64(inc, FB_SEMI[th]) >> 12); d[2] = (uint32_t)(umul64(inc, FB_SEMI[fi]) >> 12); d[3] = inc + (inc >> 7);
    g[0] = (fa == fb ? 32767 : sine_i(mo + (1u << 30))) * FB_LV[fa] >> 7;   /* (MORPH, equal power: cos, sin) */
    g[1] = fa == fb || !hb ? 0 : sine_i(mo) * FB_LV[fb] >> 7;
    e->u[FU_AF] = en->u[FU_DF];                         /* A: DEF */
    e->u[FU_A] = en->u[FU_D];
    e->u[FU_B] = (uint32_t)dg;
    e->u[FU_NOTE] = (uint32_t)r;
    e->u[FU_DF] = (en->arr[FA_BEAT][0] & ((1u << (16u + h)) - 1u)) >> h;   /* D: the chord's age */
}
#define FB_ROW (FVM_DEPTH - 6u)                         /* (c1fault) a BANK formula's stack at most (FB_*: up to 6, ARP's;
                                                         * tests/c1stack_test.c) */
static __attribute__((noinline)) void func_bank_run(track_t *t, voice_t *v, const fvm_prog_t *pa, const fvm_env_t *en, uint32_t n, uint32_t *Y)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, a, b, i, k, m, d[4], YB[FVM_BLK], fb = func_bank_f(t, 1);
    uint32_t (*X)[FVM_BLK] = lofi_work[MC_CPU() & 1u].s.s + FB_ROW;   /* (c1fault) its variables in the machine's
                                                         * stack above the formulas' own (FB_ROW): 768 B less of core 1's */
    uint32_t *ph = fb_ph[tr * NVOICE + (uint32_t)(v - t->v) % NVOICE];
    int32_t g[2];
    const fvm_prog_t *pb = wave_id_b[tr] == (0x8000u | fb) && wave_prg_b[tr].n && wave_prg_b[tr].depth <= FB_ROW ? &wave_prg_b[tr] : 0,
                     *pr[2] = {pa->depth <= FB_ROW ? pa : &fvm_mute, pb};
    const uint32_t *P = en->arr[FA_BEAT];
    fvm_env_t e;
    static const uint8_t ARR[6] = {FA_W, FA_XF, FA_IN, FA_X, FA_T, FA_TS};   /* w w3 w5 wd, t T */
    fb_setup(t, en, &e, d, g, func_bank_f(t, 0), fb, pb != 0);
    for (k = 0; k < 6u; k++) e.arr[ARR[k]] = X[k];
    for (a = 0; a < n; a = b) {                         /* a run: one 1/16, FVM_BLK at most */
        uint32_t st = P[a] >> 14;
        for (b = a + 1u; b < n && b - a < FVM_BLK && P[b] >> 14 == st; b++) {}
        m = b - a;
        for (k = 0; k < 4u; k++) for (i = 0; i < m; i++) { X[k][i] = ph[k] >> 16; ph[k] += d[k]; }
        for (i = 0; i < m; i++) { X[4][i] = P[a + i] >> 4; X[5][i] = (P[a + i] << 2) & 0xFFFFu; }
        e.arr[FA_BEAT] = P + a;
        e.u[FU_STEP] = st; e.u[FU_C] = st & 15u; e.u[FU_D] = st >> 4;
        for (k = 0; k < 2u; k++) {                      /* A (its state v->s[6]), B (v->s[7]) */
            uint32_t *o = k ? YB : Y + a;
            if (g[k]) { e.state = &v->s[6u + k]; lofi_run(pr[k], &e, &lofi_work[MC_CPU() & 1u], m, o); }
            else memset(o, 0, m * sizeof *o);
        }
        for (i = 0; i < m; i++)
            Y[a + i] = (uint32_t)((((int32_t)Y[a + i] >> 2) * g[0] + ((int32_t)YB[i] >> 2) * g[1]) >> 13);
#ifdef FUNCBANK_PEAK
        for (i = 0; i < m; i++) { int32_t y = (int32_t)Y[a + i]; if (y < 0) y = -y; if (y > FUNCBANK_PEAK) FUNCBANK_PEAK = y; }
#endif
    }
}
