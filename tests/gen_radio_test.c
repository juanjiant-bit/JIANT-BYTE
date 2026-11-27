/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* GEN 4: RADIO and MOOD, as JIANT FM's (V10.8.39 .. 41, tests/gen_entry_test.c's cases) on BYTE's generator. MOOD MID
 * the pieces as before it, byte for byte (the references recorded from the generator without MOOD: GEN_MID=1 prints
 * them); RADIO: a loop's 32 bars (over 110 BPM) played, the next piece on the bar line and played on, twice; a song's
 * last row ended, the next song; off, a saved piece, MY LOOP: nothing; a STOP on the last bar: nothing (JIANT FM made
 * the next piece and played it); MOOD: RANDOM's tempo, a DANCE radio's, the drums in every GENRE mode; the page */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void blk(void) { static int32_t o[2 * CTL]; mix_block(o, CTL); frames(32); }   /* (transport_req: the next block) */
static void bars(uint32_t n)                            /* play on until n more bars started */
{
    static int32_t o[2 * CTL];
    uint32_t b0 = bar_n, i;
    for (i = 0; i < 200000u && bar_n - b0 < n; i++) { mix_block(o, CTL); if (!(i & 7u)) frame(); }
    frames(64);
}
static void to_bar(uint32_t m, uint32_t r)              /* play on until bar_n % m == r (a frame in it) */
{
    static int32_t o[2 * CTL];
    uint32_t i;
    for (i = 0; i < 400000u && bar_n % m != r; i++) { mix_block(o, CTL); if (!(i & 7u)) frame(); }
    for (i = 0; i < 16u; i++) mix_block(o, CTL);
    frames(64);
}
static uint32_t mid_hash(uint32_t md)                   /* 8 seeds of EXPLORE, RANDOM or HOUSE, every value made */
{
    uint32_t h = 2166136261u, hs = 0, r, i, k;
    for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
    for (r = 0; r < 8u; r++) {
        gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
        gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u);
#ifndef GEN_BASE
        grd.on = 0; grd.mood = 0;
#endif
        gen_make();
        for (k = 0; k < NTRK; k++) {
            int16_t fb = trk[k].p[P_E2];
#ifndef GEN_BASE
            if (trk[k].eng_req == ENGI_DRUM)              /* (FEEDBK: the kit's TONE, dw_ktone, what GEN set as WAVE before;
                                                         * its FEEDBK 0: hashed as the WAVE it was, every piece as recorded) */
                trk[k].p[P_E2] = (int16_t)(dw_ktone + 64 + fb);
#endif
            h = (h ^ trk[k].eng_req ^ (uint32_t)trk[k].preset << 8) * 16777619u;
            for (i = 0; i < sizeof trk[k].p + sizeof trk[k].step; i++)
                if (k != gs.trk_of[RL_BASS] || i / sizeof trk[k].p[0] != P_LEVEL)   /* (the bass's LEVEL the owner's "WAVE
                                                         * basses far too loud" moved: every other value as it was) */
                    h = (h ^ (i < sizeof trk[k].p ? ((const uint8_t *)trk[k].p)[i] : ((const uint8_t *)trk[k].step)[i - sizeof trk[k].p])) * 16777619u;
            trk[k].p[P_E2] = fb;
        }
        for (i = 0; i < sizeof song.g; i++) h = (h ^ ((const uint8_t *)song.g)[i]) * 16777619u;
        for (i = 0; i < sizeof dx_kit; i++) h = (h ^ ((const uint8_t *)dx_kit)[i]) * 16777619u;
        for (i = 0; i < sizeof dx_fmod; i++) h = (h ^ ((const uint8_t *)dx_fmod)[i]) * 16777619u;
    }
    return h;
}
int main(void)
{
    int bad = 0, ok, mid_ok = 1;
    uint32_t md;
    ui_power_on();
    {   /* MOOD MID: the pieces of the generator before MOOD (c68c3aac), byte for byte; a change to the generator records
         * them again (GEN_MID=1) */
        static const uint32_t MID[3] = {0x535e3d7au, 0x4933a57bu, 0xc33b4962u};   /* (fmgen45 recorded again, intended: JIANT
                                                         * FM V10.8.41 .. 46's JAZZ, rows, envelopes, LFOs, the parts' filter
                                                         * types; was 0x1c150710 0x6323ccaa 0x38b9eff5) (partstate, the owner: the kits
                                                         * shorter and less driven, WAVE quieter, the DJ FILTER rarer; the
                                                         * bass's LEVEL aside as ever) */
        for (md = 0; md < 3u; md++) {
            uint32_t h = mid_hash(md);
            if (getenv("GEN_MID")) printf("  MID[%u] 0x%08xu\n", md, h);
            mid_ok &= h == MID[md];
        }
#ifdef GEN_BASE
        return 0;
#endif
        ui_power_on();
    }
#ifndef GEN_BASE
    {   /* GEN 4: its knobs (KNOB 3 RADIO, KNOB 4 MOOD; 1 and 2 nothing), power-on OFF, MID */
        char v[16];
        ok = !grd.on && !grd.mood;
        gen_page_put(3); ok &= gen_page() == 3u;
        gen_value(3, 2, v); ok &= str_eq(v, "OFF");
        gen_value(3, 3, v); ok &= str_eq(v, "MID");
        gen_value(3, 0, v); ok &= !v[0];
        gen_knob(0, 1); gen_knob(1, 1); ok &= !grd.on && !grd.mood;
        gen_knob(2, 1); gen_value(3, 2, v); ok &= grd.on && str_eq(v, "ON");
        gen_knob(2, -1); ok &= !grd.on;
        for (md = 0; md < 12u; md++) gen_knob(3, -1);
        gen_value(3, 3, v); ok &= grd.mood == -50 && str_eq(v, "CALM");
        for (md = 0; md < 30u; md++) gen_knob(3, 1);
        gen_value(3, 3, v); ok &= grd.mood == 50 && str_eq(v, "DANCE");
        grd.mood = 0;
        gen_page_put(4); ok &= gen_page() == 0u;
        bad += check("GEN 4: KNOB 3 RADIO OFF / ON, KNOB 4 MOOD CALM .. MID .. DANCE (-50 .. 50), from OFF and MID", ok);
    }
    {
        uint32_t h0, r, n, sc = 0, sd = 0, rn;
        uint16_t sd0;
        gp.genre = 1; gp.form = 0; gp.sig = 0;          /* (MINIMAL TECHNO: over 110 BPM, 32 bars) */
        gen_press(); frames(64);
        h0 = dice_print(); sd0 = gp.seed;
        transport_req = 1; blk();
        bars(40);
        ok = song.playing && dice_print() == h0 && !grd.rn;       /* (off: nothing) */
        transport_req = 2; blk();
        grd.on = 1;
        transport_req = 1; blk();
        bars(30);
        ok &= song.playing && dice_print() == h0 && !grd.rn;      /* (its 31st bar: still the piece) */
        bars(3); blk();
        ok &= song.playing && grd.rn == 1u && gp.seed != sd0 && dice_print() != h0 && gp.genre == 1u && gs.gi == 0u;
        h0 = dice_print();
        for (n = 0; n < 40u && grd.rn < 2u; n++) bars(1);
        blk();
        ok &= song.playing && grd.rn == 2u && n >= 31u && dice_print() != h0;
        if (!ok) printf("  radio: playing %u rn %u seed %u (was %u)\n", (unsigned)song.playing, grd.rn, gp.seed, sd0);
        bad += check("RADIO: off nothing; on, the next piece (the GENRE's, a new seed) after the loop's 32 bars, playing on, twice", ok);
        /* JIANT FM's bug: a STOP right on the piece's last bar made the next piece and played it */
        to_bar(32u, 31u);
        h0 = dice_print(); rn = grd.rn;
        ok = song.playing && grd.arm;
        transport_req = 2; blk(); frames(200);
        ok &= !song.playing && !grd.arm && grd.rn == rn && dice_print() == h0;
        frames(500);
        ok &= !song.playing && dice_print() == h0;
        bad += check("RADIO: a STOP on the loop's last bar stops (no next piece, nothing playing)", ok);
        /* a saved piece (no draft) stays; MY LOOP stays */
        song_draft = 0;
        transport_req = 1; blk();
        bars(40);
        ok = song.playing && dice_print() == h0 && grd.rn == rn;
        transport_req = 2; blk();
        song_draft = 1;
        gp.form = 3;
        transport_req = 1; blk();
        bars(40);
        ok &= song.playing && dice_print() == h0 && grd.rn == rn;
        transport_req = 2; blk();
        bad += check("RADIO: a saved piece and MY LOOP stay as they are", ok);
        gp.form = 2; gp.sig = 0;                         /* (a song: its END the next piece's cue) */
        gen_press(); frames(64);
        ok = chain_config.count > 0u && !grd.rn;
        h0 = dice_print();
        if (live.mode) chain_play_ui(); else transport_req = 1;
        blk();
        for (n = 0; n < 400u && !grd.rn; n++) { bars(1); song_poll(); }   /* (the next row staged: the main loop's) */
        blk();
        ok &= grd.rn == 1u && song.playing && chain.running && dice_print() != h0;
        if (!ok) printf("  radio song: rows %u bars waited %u playing %u rn %u\n", chain_config.count, n, (unsigned)song.playing, grd.rn);
        bad += check("RADIO: a song's last row ended, the next song made and playing", ok);
        {   /* the song's last bar, STOP: the song stops, nothing made */
            for (n = 0; n < 400u && !(chain.running && chain.row + 1u >= chain.config.count && chain.remaining <= 1u); n++) { bars(1); song_poll(); }
            frames(64);
            sd0 = gp.seed; rn = grd.rn;
            ok = song.playing && grd.arm;
            transport_req = 2; blk(); song_poll(); frames(500); song_poll(); frames(64);
            ok &= !song.playing && !grd.arm && grd.rn == rn && gp.seed == sd0;   /* (the song's own music back: song_poll) */
            if (!ok) printf("  song stop: playing %u arm %u rn %u (%u) row %u/%u rem %u fin %u\n", (unsigned)song.playing, grd.arm, grd.rn, rn, chain.row, chain.config.count, chain.remaining, chain.fin);
            bad += check("RADIO: a STOP on a song's last bar stops (no next song)", ok);
        }
        transport_req = 2; blk();
        grd.on = 0; gp.form = 0; gp.genre = 0;
        for (r = 0; r < 24u; r++) {
            gp.seed = (uint16_t)(100u + r * 37u);
            grd.mood = -50; gen_make(); sc += (uint32_t)song.g[G_BPM];
            grd.mood = 50; gen_make(); sd += (uint32_t)song.g[G_BPM];
        }
        grd.mood = 0;
        ok = sd > sc + 24u * 20u;
        printf("  MOOD: RANDOM's mean tempo CALM %u, DANCE %u BPM\n", sc / 24u, sd / 24u);
        bad += check("MOOD: RANDOM's pieces over 20 BPM slower CALM than DANCE", ok);
        for (md = 0, sc = sd = 0; md < 2u; md++)        /* EXPLORE: the tempo with the mood */
            for (r = 0; r < 24u; r++) {
                gp.genre = GEN_EXPLORE; gp.seed = (uint16_t)(100u + r * 37u);
                grd.mood = md ? 50 : -50; gen_make();
                if (md) sd += (uint32_t)song.g[G_BPM]; else sc += (uint32_t)song.g[G_BPM];
            }
        grd.mood = 0; gp.genre = 0;
        printf("  MOOD: EXPLORE's mean tempo CALM %u, DANCE %u BPM\n", sc / 24u, sd / 24u);
        bad += check("MOOD: EXPLORE's pieces slow CALM (under 95 BPM), driving DANCE (over 125)", sc / 24u < 95u && sd / 24u > 125u);
        grd.on = 1; grd.mood = 50; gp.genre = 0;
        for (r = 0, ok = 1; r < 12u; r++) {
            int32_t b0 = song.g[G_BPM];
            gp.seed = (uint16_t)(900u + r * 53u);
            grd.rn = 1;
            gen_make();
            ok &= song.g[G_BPM] == clamp(b0, gs.g->bpm_lo, gs.g->bpm_hi);
        }
        grd.on = 0; grd.mood = 0; grd.rn = 0;
        bad += check("MOOD DANCE on the radio: each piece's tempo the nearest its genre has to the one before", ok);
        {   /* MOOD in every GENRE mode (EXPLORE, RANDOM, HOUSE), 24 seeds each. CALM -50: no sound of the kit but the
             * closed hat (every other lane's level 0), that one down, the kit shorter and its filter closing; -25: the
             * kick and the rest under MID's, still there. DANCE +50: every piece a kick on every beat (a half-time's, a
             * break's: the bar's one), its level whole, the drum track over MID's; +25: the bar's first and third */
            uint32_t hs = 0, i, l, okc = 1, okh = 1, okd = 1, okq = 1;
            for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
            for (md = 0; md < 3u; md++) {
                for (r = 0; r < 24u; r++) {
                    track_t *t;
                    int16_t lv0, e3, fc, ln[8];
                    uint32_t len, kb = 0, k2 = 0, k1 = 0, a0;
                    gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                    gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u); grd.on = 0;
                    grd.mood = 0; gen_make();
                    t = &trk[gs.trk_of[RL_DRUM] < NTRK ? gs.trk_of[RL_DRUM] : 0];
                    lv0 = t->p[P_LEVEL]; e3 = t->p[P_E3]; fc = t->p[P_FCUT]; a0 = gs.act[RL_DRUM];
                    for (l = 0; l < 8u; l++) ln[l] = t->p[P_LN0 + l];
                    if (gs.act[RL_DRUM]) {
                        grd.mood = -25; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                        okq &= !gs.act[RL_DRUM] || (t->p[P_LN0] < ln[0] && t->p[P_LN0] > 0 && (md == 1u || (t->p[P_LN1] < ln[1] && t->p[P_E3] < e3 && t->p[P_FCUT] < fc)));
                        grd.mood = -50; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                        if (gs.act[RL_DRUM]) {
                            for (l = 0; l < 8u; l++) okc &= l == DV_HATC ? t->p[P_LN0 + l] <= 80 : t->p[P_LN0 + l] == 0;
                            okc &= t->p[P_E3] < 40 && t->p[P_FCUT] <= 80 && t->p[P_LEVEL] < 100;
                        }
                    }
                    grd.mood = 50; gen_make();
                    ok = gs.act[RL_DRUM] && gs.trk_of[RL_DRUM] < NTRK;
                    t = &trk[ok ? gs.trk_of[RL_DRUM] : 0];
                    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                    for (i = 0, k1 = 0; i < len; i += 4u) { kb += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); if (!(i & 15u)) k1 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); }
                    okd &= ok && (ghalf() ? k1 == (len + 15u) / 16u : kb == (len + 3u) / 4u) && (md == 1u || !a0 || (t->p[P_LEVEL] < lv0 && t->p[P_E3] < e3));
                    grd.mood = 25; gen_make();
                    ok = gs.act[RL_DRUM] && gs.trk_of[RL_DRUM] < NTRK;
                    t = &trk[ok ? gs.trk_of[RL_DRUM] : 0];
                    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                    for (i = 0, k1 = 0; i < len; i += 8u) { k2 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); if (!(i & 15u)) k1 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); }
                    okh &= ok && (ghalf() ? k1 == (len + 15u) / 16u : k2 == (len + 7u) / 8u);
                }
            }
            grd.mood = 0; gp.genre = 0;
            bad += check("MOOD CALM -25: the kick, the snare, the kit's decay and filter under MID's, still there", okq);
            bad += check("MOOD CALM -50: every piece's kit silent but a low, short, dark closed hat (EXPLORE, RANDOM, HOUSE)", okc);
            bad += check("MOOD DANCE +25: every piece its drums, the kick on the bar's first and third beats (a half-time's, a break's: its one)", okh);
            bad += check("MOOD DANCE +50: a kick on every beat (a half-time's, a break's: the bar's one); (the owner) the drums under MID's, shorter", okd);
        }
        {   /* MOOD on the other genres too: every genre, DANCE drums in each piece (FUGA's, none of its own, too); (the
             * owner) NEO CLASSIC never: no drums in any mood, LOOP PARTS SONG, nor on the radio */
            uint32_t g, all = 1, neo = 0, nd = 0, f, r;
            int32_t mo;
            grd.mood = 50;
            for (g = 1; g <= NGENRE; g++) {
                if (str_eq(GENRE[g - 1u].name, "NEO CLASSIC")) { neo = g; continue; }
                gp.genre = (uint8_t)g; gp.seed = 77; gen_make(); all &= gs.act[RL_DRUM];
            }
            for (mo = -50; mo <= 50; mo += 25)
                for (f = 0; f < 3u; f++)
                    for (r = 0; r < 4u; r++) {
                        gp.genre = (uint8_t)neo; gp.form = (uint8_t)f; gp.seed = (uint16_t)(11u + r * 97u); grd.mood = (int8_t)mo;
                        grd.on = r & 1u; grd.rn = r & 1u;
                        gen_make(); nd += gs.act[RL_DRUM] || gs.gi != GI_NEOCL;
                    }
            grd.mood = 0; grd.on = 0; grd.rn = 0; gp.genre = 0; gp.form = 0;
            bad += check("MOOD DANCE: drums in a piece of every genre but NEO CLASSIC", (int)all && neo);
            bad += check("NEO CLASSIC: no drums in any mood (CALM .. DANCE), any form, on the radio", neo && !nd);
        }
    }
    {   /* (the owner: "CALM's pieces too long") CALM -50's PARTS and SONG pieces at most half as long (seconds) as
         * 41fbf469's (24 seeds, EXPLORE RANDOM HOUSE: 136 131 106 s, 272 250 202 s); a CALM radio's loop 8 bars (16
         * over 110 BPM) */
        static const uint16_t WAS[2][3] = {{136, 131, 106}, {272, 250, 202}};
        uint32_t f, hs = 0, r, okc = 1;
        for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
        for (f = 1; f <= 2u; f++)
            for (md = 0; md < 3u; md++) {
                double sec = 0;
                for (r = 0; r < 24u; r++) {
                    uint32_t j, b = 0;
                    gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                    gp.form = (uint8_t)f; gp.seed = (uint16_t)(100u + r * 37u); grd.on = 0; grd.mood = -50;
                    gen_make();
                    for (j = 0; j < chain_config.count; j++) b += chain_config.row[j].bars;
                    sec += b * 240.0 / song.g[G_BPM];
                }
                printf("  CALM %s: %.0f s (was %u)\n", f == 1u ? "PARTS" : "SONG", sec / 24.0, WAS[f - 1u][md]);
                okc &= sec / 24.0 <= WAS[f - 1u][md] / 2.0;
            }
        grd.mood = 0; gp.form = 0; gp.genre = 0;
        bad += check("MOOD CALM -50: PARTS and SONG pieces half as long as before at most (EXPLORE, RANDOM, HOUSE)", okc);
    }
    {   /* a CALM radio: its loop's 8 bars (16 over 110 BPM) */
        uint32_t n, r0, n0, nb;
        gp.genre = 1; gp.form = 0; gp.sig = 0; grd.mood = -50; grd.on = 0;
        gen_press(); frames(64);
        nb = song.g[G_BPM] > 110 ? 16u : 8u;
        grd.on = 1; transport_req = 1; blk();
        to_bar(nb, 0u);
        r0 = grd.rn;
        for (n = 0; n < 40u && grd.rn == r0; n++) bars(1);
        n0 = n; r0 = grd.rn;
        for (n = 0; n < 40u && grd.rn == r0; n++) bars(1);
        ok = n0 <= nb + 1u && n + 1u >= nb && n <= nb + 1u;
        if (!ok) printf("  calm radio: %u then %u bars (%u)\n", n0, n, nb);
        transport_req = 2; blk();
        grd.on = 0; grd.mood = 0; grd.rn = 0; gp.genre = 0;
        bad += check("MOOD CALM: the radio's loop half as long (8 bars, 16 over 110 BPM)", ok);
    }
    {   /* (the owner: "CALM's sounds short in, short out") CALM -50: the melodic parts' attacks and releases longer than
         * MID's, their reverb wetter, the chords never stabbed; (EXPLORE "weird") EXPLORE: the chords in every piece,
         * the arp and the lead in most, the chords and the texture swelling in most, the lines' delay, the pads' room,
         * the delay ping-pong in most */
        uint32_t hs = 0, r, k, mi, i;
        double atk[2] = {0, 0}, rel[2] = {0, 0}, rev[2] = {0, 0};
        uint32_t nm[2] = {0, 0}, stab = 0, ch = 0, ar = 0, ld = 0, sw = 0, nsw = 0, dl = 1, rv = 1, png = 0;
        for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
        for (mi = 0; mi < 2u; mi++)
            for (md = 0; md < 3u; md++)
                for (r = 0; r < 24u; r++) {
                    gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                    gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u); grd.on = 0; grd.mood = (int8_t)(mi ? -50 : 0);
                    gen_make();
                    if (mi) stab += GSTAB(gs.g->pads) && gs.act[RL_PAD];
                    for (k = 0; k < NTRK; k++) {
                        uint32_t ro = gs.role[k];
                        if (ro < RL_PAD || ro > RL_TEX ||   /* (RL_N: ui_layer.c's macro here) */
                             !gs.act[ro] || (ro == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
                        atk[mi] += trk[k].p[P_ATK]; rel[mi] += trk[k].p[P_REL]; rev[mi] += trk[k].p[P_REV]; nm[mi]++;
                    }
                    if (mi || md) continue;
                    ch += gs.act[RL_PAD]; ar += gs.act[RL_ARP]; ld += gs.act[RL_LEAD]; png += song.g[G_DPING] != 0;
                    for (i = RL_PAD; i <= RL_TEX; i++) {
                        track_t *t = gtr(i);
                        if (!t) continue;
                        if (i == RL_PAD || i == RL_TEX) { nsw++; sw += t->p[P_ATK] >= 50 && t->p[P_REL] >= 84; rv &= t->p[P_REV] >= 55; }
                        else if (i != RL_TEX) dl &= t->p[P_DLY] >= 40;
                    }
                }
        grd.mood = 0; gp.genre = 0;
        for (mi = 0; mi < 2u; mi++) { atk[mi] /= nm[mi] ? nm[mi] : 1u; rel[mi] /= nm[mi] ? nm[mi] : 1u; rev[mi] /= nm[mi] ? nm[mi] : 1u; }
        printf("  MOOD: melodic ATK MID %.0f CALM %.0f, REL %.0f %.0f, REV %.0f %.0f; CALM stabs %u\n", atk[0], atk[1], rel[0], rel[1], rev[0], rev[1], stab);
        bad += check("MOOD CALM -50: the melodic parts slower in, longer out, wetter; the chords never stabbed", atk[1] >= atk[0] + 20.0 && rel[1] >= rel[0] + 20.0 && rev[1] >= rev[0] + 10.0 && !stab);
        printf("  EXPLORE: chords %u arp %u lead %u of 24, swells %u of %u, ping-pong %u\n", ch, ar, ld, sw, nsw, png);
        bad += check("EXPLORE: chords always, arp and lead in most, swelling pads, the lines' delay and the pads' room, ping-pong", ch == 24u && ar >= 14u && ld >= 14u && sw * 2u >= nsw && dl && rv && png >= 14u);
    }
    {   /* (the owner: "WAVE basses far too loud") every genre's bass under its drums (their stems over 2 bars, 4 seeds:
         * the bass's RMS 3 dB under at least, its peak not over the drums') */
        uint32_t g, r, okb = 1;
        for (g = 0; g <= NGENRE; g++) {
            double sdb = 0, spk = 0;
            uint32_t n = 0;
            for (r = 0; r < 4u; r++) {
                uint32_t b, nb, i, p, bs, dr;
                double eb = 0, ed = 0, pb = 0, pd = 0;
                gp.genre = (uint8_t)(g == NGENRE ? GEN_EXPLORE : g + 1u);
                gp.form = 0; gp.seed = (uint16_t)(300u + r * 71u); grd.on = 0; grd.mood = 0;
                gen_make();
                bs = gs.trk_of[RL_BASS]; dr = gs.trk_of[RL_DRUM];
                if (!gs.act[RL_BASS] || !gs.act[RL_DRUM] || bs >= NTRK || dr >= NTRK) continue;
                nb = (uint32_t)(2u * 4u * 60u * FS / (uint32_t)song.g[G_BPM] / CTL);
                song.master_q12 = 4096; transport_req = 1;
                for (b = 0; b < nb; b++) {
                    int32_t o[2 * CTL];
                    if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                    fx_stems = 1; mix_block(o, CTL);
                    for (i = 0; i < CTL; i++) {
                        double vb = stem_buf[bs][i], vd = stem_buf[dr][i];
                        eb += vb * vb; ed += vd * vd;
                        if (fabs(vb) > pb) pb = fabs(vb);
                        if (fabs(vd) > pd) pd = fabs(vd);
                    }
                }
                fx_stems = 0; transport_req = 2;
                { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
                sdb += 10.0 * log10((eb + 1.0) / (ed + 1.0)); spk += 20.0 * log10((pb + 1.0) / (pd + 1.0)); n++;
                (void)p;
            }
            if (n && (sdb / n > -3.0 || spk / n > 0.0)) {
                okb = 0;
                printf("  bass over the drums: %s rms %+.1f dB peak %+.1f dB\n", g == NGENRE ? "EXPLORE" : GENRE[g].name, sdb / n, spk / n);
            }
        }
        bad += check("GEN: every genre's bass under its drums (RMS 3 dB under, the peak not over)", okb);
    }
    {   /* (the owner on the RADIO: DANCE's drums saturating in front, the synths barely there) MOOD DANCE +50, 8 RANDOM
         * pieces' stems over 4 bars: the melodic ones (every part but the drums) within 6 dB of the drums (their mean;
         * each within 10), the drums' stem never at the clip; the pump marked (DUCK 40 .. 60) */
        uint32_t r, i, b, k, nclip = 0, n = 0, duck = 1;
        double sum = 0, worst = 0;
        for (r = 0; r < 8u; r++) {
            double e[NTRK] = {0}, em = 0, db;
            uint32_t nb, dr;
            gp.genre = 0; gp.form = 0; gp.seed = (uint16_t)(300u + r * 71u); grd.on = 0; grd.mood = 50;
            gen_make();
            dr = gs.trk_of[RL_DRUM];
            if (!gs.act[RL_DRUM] || dr >= NTRK) continue;
            duck &= song.g[G_DUCK] >= 40 && song.g[G_DUCK] <= 60;
            nb = (uint32_t)(4u * 4u * 60u * FS / (uint32_t)song.g[G_BPM] / CTL);
            song.master_q12 = 4096; transport_req = 1;
            for (b = 0; b < nb; b++) {
                int32_t o[2 * CTL];
                if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                fx_stems = 1; mix_block(o, CTL);
                for (k = 0; k < NTRK; k++)
                    for (i = 0; i < CTL; i++) {
                        double v = stem_buf[k][i];
                        e[k] += v * v;
                        if (k == dr && fabs(v) >= 32767.0) nclip++;
                    }
            }
            fx_stems = 0; transport_req = 2;
            { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
            for (k = 0; k < NTRK; k++) if (k != dr) em += e[k];
            db = 10.0 * log10((em + 1.0) / (e[dr] + 1.0));
            sum += db; n++;
            if (db < worst) worst = db;
        }
        grd.mood = 0; gp.genre = 0;
        printf("  DANCE: the melodic stems %.1f dB from the drums (mean; worst %.1f), drum samples at the clip %u\n", n ? sum / n : -99.0, worst, nclip);
        bad += check("MOOD DANCE: the melodic parts within 6 dB of the drums, the drums never clipping, the pump marked",
                     n >= 6u && sum / n >= -6.0 && worst >= -10.0 && !nclip && duck);
    }
    {   /* (the owner: the DJ FILTER constant and long, "it kills the mood") 24 SONG pieces of RANDOM, MID and DANCE: a
         * third at most with a DJ FILTER record, each one short (a bar: its records within 16 steps) and small (under a
         * third of its range); CALM none */
        uint32_t r, s, i, md, with = 0, okl = 1, calm = 0;
        for (md = 0; md < 3u; md++)
            for (r = 0; r < 24u; r++) {
                uint32_t any = 0;
                gp.genre = 0; gp.form = 2; gp.seed = (uint16_t)(500u + r * 41u); grd.on = 0;
                grd.mood = (int8_t)(md == 0u ? 0 : md == 1u ? 50 : -50);
                gen_make();
                for (s = 0; s < 4u; s++) {
                    uint32_t lo = NSTEP, hi = 0;
                    if (!proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t))) continue;
                    for (i = 0; i < proj_scratch.motion.count && i < MOTION_MAX; i++) {
                        const motion_event_t *ev = &proj_scratch.motion.event[i];
                        if (MOTION_ID(ev) != MOT_DJF) continue;
                        any = 1;
                        if (MOTION_STEP(ev) < lo) lo = MOTION_STEP(ev);
                        if (MOTION_STEP(ev) > hi) hi = MOTION_STEP(ev);
                        okl &= MOTION_VAL(ev) > -22 && MOTION_VAL(ev) < 22 * 1 + 4;
                    }
                    okl &= lo == NSTEP || hi - lo < 16u;
                }
                if (md == 2u) calm += any; else with += any;
            }
        grd.mood = 0; gp.form = 0;
        printf("  DJ FILTER: %u of 48 MID and DANCE songs, CALM %u\n", with, calm);
        bad += check("GEN: the DJ FILTER a resource: a third of the pieces at most, one short small sweep, never CALM",
                     with <= 16u && okl && !calm);
    }
    bad += check("MOOD MID: EXPLORE's, RANDOM's and HOUSE's pieces as recorded (partstate), the bass's level aside, byte for byte", mid_ok);
#endif
    printf(bad ? "gen-radio: %d FAILED\n" : "gen-radio: all ok\n", bad);
    return bad != 0;
}
