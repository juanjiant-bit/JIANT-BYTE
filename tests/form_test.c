/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DRONE CLAY (BYTE: FORM's composer as a track's sequencer, eng_form.c clay.c): its drone sounds low on the track's
 * voice, held or struck at RATE; a modulator moves its DEST (the sound differs); a NOTE gesture plays real notes; OFF
 * silent; the pages' knobs; the settings stored and read back; a FORM part of an older project loads as WAVE with its
 * modulators in DRONE CLAY. Then the power-on check kept from FORM's test */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int ft_bad;
#define OK(c, ...) do { int ok_ = (c); printf("form: "); printf(__VA_ARGS__); printf("%*s%s\n", 4, "", ok_ ? "ok" : "FAIL"); ft_bad += !ok_; } while (0)
#define NS (FS * 3u)
static double ya[NS], yb[NS];
static void setup(void)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_WAVE); frame();
    for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    clay_defaults(clay[0]); song.master_q12 = 4096; frames(8);
}
static unsigned others, strikes; static int vmax;
static void play(double *y)
{
    int32_t b[CTL * 2];
    unsigned n = 0, prev = 0;
    others = strikes = 0; vmax = 0;
    transport_req = 1;
    while (n < NS) {
        mix_block(b, CTL);
        for (unsigned i = 0; i < CTL && n < NS; i++) y[n++] = (b[2 * i] + b[2 * i + 1]) / 65536.0;
        for (unsigned v = 0; v < NVOICE; v++) {
            const voice_t *vv = &trk[0].v[v];
            if (vv->active && vv->gate && vv->note != clay_dn[0]) others++;
        }
        if (clay_vk(0, 0) > vmax) vmax = clay_vk(0, 0);
        if (clay_lr[0] != prev) strikes++;
        prev = clay_lr[0];
    }
    transport_req = 2; for (unsigned j = 0; j < FS / 4u / CTL; j++) mix_block(b, CTL);
}
static double rms(const double *y) { double e = 0; for (unsigned i = 0; i < NS; i++) e += y[i] * y[i]; return sqrt(e / NS); }
static double dlt(const double *a, const double *b) { double d = 0, e = 0; for (unsigned i = 0; i < NS; i++) d += (a[i] - b[i]) * (a[i] - b[i]), e += a[i] * a[i]; return sqrt(d / (e + 1e-12)); }
static void mods_off(void) { for (unsigned m = 0; m < 4u; m++) clay[0][CL_MF(m, 3u)] = 0; }
int main(int argc, char **argv)
{
    (void)argv; if (argc > 1) return 0;
    {   /* the drone: low, on the ROOT, held */
        setup(); mods_off(); trk[0].p[P_ROOT] = 2; play(ya);
        OK(clay_dn[0] == 0 && rms(ya) > 0.005, "the drone sounds (rms %.3f), held while playing, its note let go at STOP", rms(ya));
        setup(); mods_off(); trk[0].p[P_ROOT] = 2;
        int32_t b[CTL * 2]; transport_req = 1; for (unsigned j = 0; j < 200u; j++) mix_block(b, CTL);
        OK(clay_dn[0] == 26u, "the drone D1 (%u): C1 on the ROOT, OCT 1", clay_dn[0]);
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
    }
    {   /* RATE: HOLD one strike, 1/4 one a beat */
        setup(); mods_off(); play(ya); unsigned h = strikes;
        setup(); mods_off(); clay[0][CL_RATE] = 5; play(ya);
        OK(h <= 1u && strikes >= 4u, "RATE: HOLD struck %u time(s), 1/4 %u times in 3 s", h, strikes);
    }
    {   /* a modulator moves its DEST: the sound differs */
        setup(); mods_off(); trk[0].p[P_FCUT] = 40; clay[0][CL_MF(0, 0u)] = 2; clay[0][CL_MF(0, 1u)] = 24; clay[0][CL_MF(0, 2u)] = 2;
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_MF(0, 4u)] = 40; play(ya);
        setup(); mods_off(); trk[0].p[P_FCUT] = 40; clay[0][CL_MF(0, 0u)] = 0; clay[0][CL_MF(0, 1u)] = 24; clay[0][CL_MF(0, 2u)] = 2;
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_MF(0, 4u)] = 40; play(yb);
        OK(dlt(ya, yb) > 0.05, "a modulator on CUTOFF moves the sound (%.3f of it, vmax %d, rms %.4f %.4f)", dlt(ya, yb), vmax, rms(ya), rms(yb));
        setup(); mods_off(); trk[0].p[P_FCUT] = 40; clay[0][CL_MF(0, 0u)] = 12; clay[0][CL_MF(0, 1u)] = 12; clay[0][CL_MF(0, 2u)] = 2;
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_MF(0, 4u)] = 40; play(ya);
        OK(dlt(ya, yb) > 0.2, "a modulator on the sound's KNOB 1 moves it (%.3f of it)", dlt(ya, yb));
    }
    {   /* a NOTE gesture: real notes on the voice */
        setup(); mods_off(); clay[0][CL_MF(1, 0u)] = 0; clay[0][CL_MF(1, 1u)] = 36; clay[0][CL_MF(1, 2u)] = 2;
        clay[0][CL_MF(1, 3u)] = 110; clay[0][CL_MF(1, 4u)] = 30; play(ya);
        OK(others > 0u, "ARP UP plays notes over the drone (%u blocks of them)", others);
    }
    {   /* OFF: silent */
        setup(); clay[0][CL_ON] = 0; play(ya);
        OK(rms(ya) < 1e-4, "OFF: nothing sounds (rms %.5f)", rms(ya));
    }
    {   /* the pages: SEQ > DRONE CLAY, KNOB 1 ON from blank: its first sound */
        ui_power_on(); go_home(); frame(); memset(clay, 0, sizeof clay);
        int ok = nav_put(SEC_SEQ, "DRONE CLAY") && cur_page()->graph == GR_CLAY;
        turn(EN_K1, 1); frames(2);
        ok &= clay[song.sel][CL_ON] == 1 && clay[song.sel][CL_MF(0, 3u)] != 0;
        int16_t *vp; const param_desc_t *d = clay_desc(TSEL, CL_MF(0, 1u), &vp);
        ok &= d && d->names && !strcmp(desc_nm(d, 0), "KICK");   /* (the owner: the HITS first) */
        OK(ok, "SEQ > DRONE CLAY: ON from blank its first sound, SHAPE's first a HIT (%s)", d && d->names ? desc_nm(d, 0) : "-");
    }
    {   /* stored and read back */
        static project_t q, r; static project_store_t st;
        setup(); clay[0][CL_SEED] = 77; clay[0][CL_MF(2, 0u)] = 15; clay[0][CL_PIT] = -19; clay[0][CL_MF(3, 5u)] = 22;
        clay_rec[0][5] = 8u + 55u * 2u + 13u; clay_rec[0][31] = 3u;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        uint8_t pk[CLAY_NF]; clay_pack(clay[0], pk);
        for (unsigned f = 0; ok && f < CLAY_NF; f++) ok &= r.clay[0][f] == pk[f];
        for (unsigned f = 0; ok && f < CLAY_RN; f++) ok &= r.clay_rec[0][f] == clay_rec[0][f];
        ok &= !r.clay_rec[1][0];
        ok &= !r.clay[1][CL_ON];
        OK(ok, "the settings (PITCH -19, a RATE 1/20) and the recorded loop stored and read back");
    }
    {   /* a FORM part of an older project: WAVE, its modulators in DRONE CLAY */
        static project_t q, r; static project_store_t st;
        setup(); memset(clay, 0, sizeof clay);
        project_capture(&q); q.t[0].engine = ENGI_FORM;
        q.t[0].p[FORM_MF[0][0]] = (int16_t)FORM_DS(FD_PITCH, S_KICK); q.t[0].p[FORM_MF[0][1]] = (int16_t)FORM_EUV(2, 0);
        q.t[0].p[FORM_MF[0][2]] = 110; q.t[0].p[FORM_MF[0][3]] = 9;
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw) && !project_restore_runtime(&r);
        ok &= trk[0].eng_req == ENGI_WAVE && clay[0][CL_ON] && clay_sh(clay[0][CL_MF(0, 1u)]) == S_KICK &&
              clay[0][CL_MF(0, 2u)] == 3 && clay[0][CL_MF(0, 3u)] == 110 && clay[0][CL_MF(0, 4u)] == 9;
        OK(ok, "a FORM part loads as WAVE with its modulators in DRONE CLAY (%u %d %d %d %d %d)", trk[0].eng_req, clay[0][CL_ON], (int)clay_sh(clay[0][CL_MF(0, 1u)]), clay[0][CL_MF(0, 2u)], clay[0][CL_MF(0, 3u)], clay[0][CL_MF(0, 4u)]);
    }
    {   /* the resolutions: straight, dotted, triplets, polyrhythms on the bar's grid (clay_step) */
        static const unsigned R[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 11, 16, 19, 22};
        static const unsigned N[] = {256, 128, 64, 32, 16, 8, 4, 2, 1, 24, 48, 20, 28};   /* (steps in 4 bars) */
        int ok = 1;
        for (unsigned i = 0; i < sizeof R / sizeof R[0]; i++) {
            unsigned cnt = 0, last = ~0u;
            for (uint32_t e = 0; e < 4u * 262144u; e += 512u) { uint32_t s1 = clay_step(e, R[i]); cnt += s1 != last; last = s1; }
            ok &= cnt == N[i] && clay_step(0, R[i]) == 0 && clay_step(262144u, R[i]) == (CLAY_RES[R[i]].num > CLAY_RES[R[i]].den ? CLAY_RES[R[i]].spb : R[i] == 6u);
            if (cnt != N[i]) printf("  %s: %u steps in 4 bars, not %u\n", str_nth(CLAY_RESS, R[i] + 1), cnt, N[i]);
        }
        OK(ok && !strcmp(str_nth(CLAY_RESS, 1), "1/64") && !strcmp(str_nth(CLAY_RESS, 9), "4BAR"), "the resolutions: 1/64 .. 4 BAR, dotted, triplets, 1/5 .. on the bar's first step");
    }
    {   /* each modulator its own RATE, the drone's RATE not theirs */
        unsigned st[2][2];
        for (unsigned v = 0; v < 2u; v++) {
            setup(); mods_off(); clay[0][CL_RATE] = v ? 3 : 5;   /* (the drone: 1/16, 1/4) */
            clay[0][CL_MF(0, 2u)] = 2; clay[0][CL_MF(0, 5u)] = 1; clay[0][CL_MF(1, 2u)] = 2; clay[0][CL_MF(1, 5u)] = 4;
            int32_t b[CTL * 2]; int32_t l0 = -9, l1 = -9; st[v][0] = st[v][1] = 0;
            transport_req = 1; mix_block(b, CTL);
            while (clk_n < 32u) { mix_block(b, CTL); st[v][0] += form_st[0].lastm[0] != l0; st[v][1] += form_st[0].lastm[1] != l1; l0 = form_st[0].lastm[0]; l1 = form_st[0].lastm[1]; }
            transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        }
        OK(st[0][0] >= 63u && st[0][0] <= 65u && st[0][1] >= 7u && st[0][1] <= 9u && st[0][0] == st[1][0] && st[0][1] == st[1][1],
           "MOD 1 at 1/32 %u steps, MOD 2 at 1/4 %u in two bars, whatever the drone's RATE (%u %u)", st[0][0], st[0][1], st[1][0], st[1][1]);
    }
    {   /* PITCH: the drone up and down */
        int32_t b[CTL * 2]; unsigned up, dn;
        setup(); mods_off(); trk[0].p[P_ROOT] = 0; clay[0][CL_PIT] = 12; transport_req = 1; for (unsigned j = 0; j < 100u; j++) mix_block(b, CTL); up = clay_dn[0];
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        setup(); mods_off(); trk[0].p[P_ROOT] = 0; clay[0][CL_OCT] = 3; clay[0][CL_PIT] = -24; transport_req = 1; for (unsigned j = 0; j < 100u; j++) mix_block(b, CTL); dn = clay_dn[0];
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        OK(up == 36u && dn == 24u, "PITCH +12: the drone C2 (%u); C3 at -24: C1 (%u)", up, dn);
    }
    {   /* a HIT on any engine: the drone ducked between, the hit loud; a kick low, a hat bright */
        static const uint32_t ENG[2] = {ENGI_WAVE, ENGI_LOFI};
        static const char *const EN[2] = {"WAVE", "MATH"};
        for (unsigned e = 0; e < 2u; e++) {
            double zc[2] = {0, 0}, gate = 0;
            for (unsigned h = 0; h < 2u; h++) {
                ui_power_on(); song.sel = 0; select_engine(ENG[e]); frame();
                for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
                clay_defaults(clay[0]); song.master_q12 = 4096; song.g[G_BPM] = 120; frames(8);
                mods_off(); clay[0][CL_MF(0, 1u)] = h ? 2 : 0; clay[0][CL_MF(0, 0u)] = 0; clay[0][CL_MF(0, 2u)] = 3;
                clay[0][CL_MF(0, 3u)] = 120; clay[0][CL_MF(0, 5u)] = 2; play(ya);
                unsigned beat = FS / 2u, nb = 0; double on = 0, off = 0; unsigned zcn = 0;
                for (unsigned i = beat; i + beat <= NS; i += beat, nb++) {
                    for (unsigned j = 0; j < FS / 40u; j++) on += ya[i + j] * ya[i + j];
                    for (unsigned j = beat - FS / 20u; j < beat - FS / 40u; j++) off += ya[i + j] * ya[i + j];
                    for (unsigned j = 1; j < FS / 40u; j++) zcn += (ya[i + j] >= 0) != (ya[i + j - 1] >= 0);
                }
                zc[h] = (double)zcn / nb;
                if (!h) gate = sqrt(on / (off + 1e-12));
            }
            OK(gate > 3.0 && zc[1] > zc[0] * 2.0, "%s: a KICK on the beat (%.1f x louder than between), a HAT brighter (%.0f against the kick's %.0f crossings)", EN[e], gate, zc[1], zc[0]);
        }
    }
    {   /* a HIT on a DRUM track: the kit's own pad */
        ui_power_on(); song.sel = 0; select_engine(ENGI_DRUM); frame();
        for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
        clay_defaults(clay[0]); song.master_q12 = 4096; frames(8);
        mods_off(); clay[0][CL_MF(0, 1u)] = 1; clay[0][CL_MF(0, 2u)] = 3; clay[0][CL_MF(0, 3u)] = 120;
        int32_t b[CTL * 2]; unsigned sn = 0, drone = 0;
        transport_req = 1;
        for (unsigned j = 0; j < 3000u; j++) { mix_block(b, CTL); for (unsigned v = 0; v < NVOICE; v++) sn += trk[0].v[v].active && trk[0].v[v].note == 38u; drone |= clay_dn[0]; }
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        OK(sn > 0u && drone == 36u, "DRUM-W: a SNARE plays the kit's snare (%u blocks), the drone its kick (%u)", sn, drone);
    }
    {   /* FM's seven differ (each a ratio of its step) */
        int vv[3][64];
        for (unsigned f = 0; f < 3u; f++) {
            setup(); mods_off(); clay[0][CL_MF(0, 1u)] = (int16_t)(48 + f * 3); clay[0][CL_MF(0, 2u)] = 6; clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_MF(0, 5u)] = 4;
            int32_t b[CTL * 2]; transport_req = 1;
            for (unsigned j = 0; j < 64u * 8u; j++) { mix_block(b, CTL); if (!(j & 7u)) vv[f][j / 8u] = clay_vk(0, 0); }
            transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        }
        double d01 = 0, d02 = 0; for (unsigned i = 0; i < 64u; i++) d01 += fabs((double)vv[0][i] - vv[1][i]), d02 += fabs((double)vv[0][i] - vv[2][i]);
        OK(d01 > 64 * 3000.0 && d02 > 64 * 3000.0, "FM x1/2, x2, x7 move DEST each its own way (%.0f %.0f)", d01 / 64, d02 / 64);
    }
    {   /* the keys: every white acts on every OCT page; the degrees on the track's scale; MOD scrolled on the page */
        setup(); int ok = 1;
        for (int oc = -2; oc <= 2; oc++) {
            song.octave = oc;
            for (unsigned k = 0; k < 27u; k++) {
                if (CLAY_KMAP[k] >= 16u) continue;
                memset(form_st[0].g, 0, sizeof form_st[0].g);
                clay_kb(k, &trk[0], 1); clay_kb(k, &trk[0], 0);
                unsigned alive = 0; for (unsigned g = 0; g < FORM_G; g++) alive |= form_st[0].g[g].fl & 8u;
                if (!alive) { ok = 0; printf("  OCT %d key %u: nothing\n", oc, k); }
            }
        }
        song.octave = 0;
        OK(ok, "every white key strikes a gesture on every OCT page (-2 .. +2)");
        trk[0].p[P_SCALE] = 1; int32_t maj = clay_degree(&trk[0], 2), maj6 = clay_degree(&trk[0], 6);
        trk[0].p[P_SCALE] = 2; int32_t mn = clay_degree(&trk[0], 2);
        trk[0].p[P_SCALE] = 5; int32_t pen = clay_degree(&trk[0], 5);
        OK(maj == 4 && maj6 == 11 && mn == 3 && pen == 12, "the degree keys on the scale: MAJ 3rd %d, 7th %d; MIN 3rd %d; PEN 6th %d", maj, maj6, mn, pen);
        ui_power_on(); go_home(); frame(); clay_defaults(clay[song.sel]);
        ok = nav_put(SEC_SEQ, "CLAY MOD") && cur_page()->graph == GR_CLAY;
        turn(EN_K1, 1); frames(2); turn(EN_K1, 1); frames(2);
        int16_t was = clay[song.sel][CL_MF(2, 3u)], was0 = clay[song.sel][CL_MF(0, 3u)];
        turn(EN_K4, -3); frames(2);
        ok &= clay_km[song.sel] == 2 && clay[song.sel][CL_MF(2, 3u)] < was && clay[song.sel][CL_MF(0, 3u)] == was0;
        OK(ok, "CLAY MOD: KNOB 1 scrolls to MOD 3 (%d), KNOB 4 its AMOUNT (%d -> %d)", clay_km[song.sel], was, clay[song.sel][CL_MF(2, 3u)]);
    }
    {   /* (FM V10.8.30's) every AMOUNT 0, the EUCLIDs on, RATE HOLD, a plucked voice: the drone clean, one note-on in
         * four bars (no HIT re-strikes it) */
        setup(); mods_off(); trk[0].p[P_SUS] = 20;
        for (unsigned m = 0; m < 4u; m++) clay[0][CL_MF(m, 2u)] = 3;
        int32_t b[CTL * 2]; unsigned ons = 0;
        transport_req = 1;
        for (unsigned j = 0; j < 8u * FS / CTL; j++) {   /* (8 s: four bars at 120; the voices started: the only track's) */
            uint32_t a = vage; mix_block(b, CTL); ons += vage != a;
        }
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        OK(ons == 1u, "AMOUNT 0 everywhere, EUCLIDs on, HOLD: the drone struck %u time(s) in four bars", ons);
    }
    {   /* KEYS (CLAY TIME, RUN's bit 1 as JIANT FM): ON the keys CLAY's gestures, OFF notes while the drone sounds, no
         * gesture; its knob, RUN's kept */
        int ok = 1; unsigned w = white(2);
        setup(); mods_off(); song.octave = 0; memset(form_st[0].g, 0, sizeof form_st[0].g);
        int32_t b[CTL * 2]; transport_req = 1; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        clay[0][CL_RUN] |= 2;
        key_down(w); mix_block(b, CTL); mix_block(b, CTL);
        unsigned alive = 0, notes = 0;
        for (unsigned g = 0; g < FORM_G; g++) alive |= form_st[0].g[g].fl & 8u;
        for (unsigned v = 0; v < NVOICE; v++) notes += trk[0].v[v].active && trk[0].v[v].gate && trk[0].v[v].note != clay_dn[0];
        key_up(w); mix_block(b, CTL);
        unsigned held = 0; for (unsigned v = 0; v < NVOICE; v++) held += trk[0].v[v].active && trk[0].v[v].gate && trk[0].v[v].note != clay_dn[0];
        ok &= !alive && notes >= 1u && !held && clay_dn[0];
        clay[0][CL_RUN] &= ~2;
        key_down(w); mix_block(b, CTL);
        unsigned alive2 = 0; for (unsigned g = 0; g < FORM_G; g++) alive2 |= form_st[0].g[g].fl & 8u;
        key_up(w); mix_block(b, CTL);
        ok &= alive2 != 0u;
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        ui_power_on(); go_home(); frame(); clay_defaults(clay[song.sel]); clay[song.sel][CL_RUN] = 1;
        ok &= nav_put(SEC_SEQ, "CLAY TIME") && cur_page()->id[2] == ID_CL0 + CL_KEYS;
        int16_t *vp; const param_desc_t *d = clay_desc(TSEL, CL_KEYS, &vp);
        ok &= d && !strcmp(d->label, "KEYS") && d->names && !strcmp(desc_nm(d, *vp), "ON");
        turn(EN_K3, 1); frames(2);
        ok &= clay[song.sel][CL_RUN] == 3;            /* (KEYS OFF: RUN's bit 1, FREE kept) */
        turn(EN_K1, -1); frames(2);
        ok &= clay[song.sel][CL_RUN] == 2;            /* (RUN SONG: KEYS kept) */
        uint8_t pk[CLAY_NF]; clay_pack(clay[song.sel], pk); ok &= pk[CL_RUN] == 2u;
        clay_defaults(clay[song.sel]);
        OK(ok, "KEYS OFF: a key a note (%u), no gesture (%u), let go (%u); ON: a gesture (%u)", notes, alive, held, alive2);
    }
    {   /* REC: the keys' gestures into the loop, played back round */
        setup(); mods_off(); song.octave = 0; memset(clay_rec, 0, sizeof clay_rec); clay_km[0] = 0;
        for (unsigned m = 0; m < 4u; m++) clay[0][CL_MF(m, 2u)] = 0;   /* (no euclid: the loop alone) */
        clay[0][CL_MF(0, 3u)] = 100;
        int32_t b[CTL * 2]; transport_req = 1; mix_block(b, CTL); mix_block(b, CTL);
        while (clk_n < 5u) { mix_block(b, CTL); song.rec = 1u; }
        int rok = rec_on(&trk[0]);
        clay_kb(0, &trk[0], 1); mix_block(b, CTL); clay_kb(0, &trk[0], 0);   /* (key 0: white 1, KICK into MOD 1) */
        unsigned at = 99; for (unsigned i = 0; i < CLAY_RN; i++) if (clay_rec[0][i]) at = i;
        song.rec = 0;
        unsigned hits = 0; uint32_t was = 0;
        while (clk_n < 5u + 64u) {                      /* (two rounds more) */
            uint32_t ps = clk_n; mix_block(b, CTL);
            if (clk_n != ps) { for (unsigned g = 0; g < FORM_G; g++) if ((form_st[0].g[g].fl & 8u) && form_st[0].g[g].t0 != was && !form_st[0].g[g].key && form_st[0].g[g].sh == S_KICK) { hits++; was = form_st[0].g[g].t0; } }
        }
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(b, CTL);
        OK((at == 5u || at == 6u) && clay_rec[0][at] == 8u + S_KICK && hits == 2u, "REC (armed %d): a KICK key at step %u recorded (%u), played back each round (%u times in two)", rok, at, at < CLAY_RN ? clay_rec[0][at] : 0u, hits);
    }
    {   /* a first DRONE CLAY's block (26 settings) as today's */
        uint8_t o[26] = {1, 2, 5, 1, 2, 0, 2, 100, 0, 36, 7, 90, 13, 26, 21, 60, 3, 2, 22, 70, 10, 14, 30, 10, 3, 7}, nb[CLAY_NF];
        clay_from_v1(o, nb);
        OK(nb[CL_ON] && nb[CL_OCT] == 2 && nb[CL_RATE] == 5 && nb[CL_RUN] && nb[CL_PIT] == CL_PITB && nb[CL_SEED] == 7 && nb[CL_MF(0, 2u)] == 3 &&
           nb[CL_MF(2, 2u)] == 11 && nb[CL_MF(3, 2u)] == 12 && nb[CL_MF(1, 4u)] == 14 && nb[CL_MF(3, 5u)] == 1, "a first layout's block: RATE 1/4, EUCLIDs 1/4 3/16 5/16, STEP 1/32 every RATE");
    }
    {   /* (BYTE 0.17, the owner: "start at zero") power-on with RESTORE LAST OFF: a new project, not song 1's rows */
        ui_power_on(); chain_config.row[0].slot = 2; chain_config.count = 3; proj_cur = 0; ui_prefs &= (uint8_t)~PREF_RESTORE_ON;
        song_boot_blank();
        chain_config_t empty; chain_defaults(&empty);
        OK(song_draft && proj_cur == PROJ_NO_SLOT && !memcmp(&chain_config, &empty, sizeof empty), "POWER-ON: a new project (draft), the saved songs untouched");
        song_draft = 0;
    }
    printf("form: %s\n", ft_bad ? "FAIL" : "all ok");
    return ft_bad != 0;
}
