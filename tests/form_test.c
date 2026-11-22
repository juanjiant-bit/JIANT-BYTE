/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* FORM (BYTE 0.17, eng_form.c): one sound, plasticine. Every preset sounds, bounded; the drone alone continuous; each
 * DEST heard, every HIT GESTURE NOTE moving it; REST silent; the euclids ("--" none); the keys gestures; FREQ; TRIG;
 * the pages (MOD: KNOB 1 the modulator); the dice in range; the part stored and read back; 64 steps; power-on blank */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int ft_bad;
#define OK(c, ...) do { int ok_ = (c); printf("form: "); printf(__VA_ARGS__); printf("%*s%s\n", 4, "", ok_ ? "ok" : "FAIL"); ft_bad += !ok_; } while (0)
#define NS (FS * 4u)
static double ya[NS], yb[NS];
static void setup(unsigned pr)
{
    ui_power_on(); song.sel = 0; select_engine(ENGI_FORM); frame();
    for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, pr); TSEL->p[P_LRTRG] = 1; frames(64); song.master_q12 = 4096;
}
static void play(double *y)
{
    int32_t b[CTL * 2];
    unsigned n = 0;
    transport_req = 1;
    while (n < NS) { mix_block(b, CTL); for (unsigned i = 0; i < CTL && n < NS; i++) y[n++] = (b[2 * i] + b[2 * i + 1]) / 65536.0; }
    transport_req = 2; for (unsigned j = 0; j < FS / 4u / CTL; j++) mix_block(b, CTL);
}
static double rms(const double *y) { double e = 0; for (unsigned i = 0; i < NS; i++) e += y[i] * y[i]; return sqrt(e / NS); }
static double peak(const double *y) { double p = 0; for (unsigned i = 0; i < NS; i++) if (fabs(y[i]) > p) p = fabs(y[i]); return p; }
static double dlt(const double *a, const double *b) { double d = 0, e = 0; for (unsigned i = 0; i < NS; i++) d += (a[i] - b[i]) * (a[i] - b[i]), e += a[i] * a[i]; return sqrt(d / (e + 1e-12)); }
static void mods_off(void) { for (unsigned m = 0; m < 4u; m++) TSEL->p[FORM_MF[m][1]] = (int16_t)FORM_EUV(0, 0); }
int main(int argc, char **argv)
{
    (void)argv; if (argc > 1) return 0;                 /* (the old "checks" run: all of it here) */
    {   /* every preset sounds, bounded */
        int ok = 1;
        for (unsigned pr = 0; pr < FORM_N; pr++) {
            setup(pr); play(ya);
            double r = rms(ya), pk = peak(ya);
            if (!(r > 0.01 && pk < 1.0)) { printf("form: preset %s rms %.3f peak %.3f\n", FORM_PRESETS[pr].name, r, pk); ok = 0; }
        }
        OK(ok, "the %u presets sound, bounded", FORM_N);
    }
    {   /* the drone alone: continuous (no gaps), the modulators off */
        setup(0); mods_off(); play(ya);
        unsigned seg = NS / 64u, quiet = 0; double e[64] = {0};
        for (unsigned i = 0; i < NS; i++) e[i / seg % 64u] += ya[i] * ya[i];
        double em = 0; for (unsigned k = 1; k < 64u; k++) em += e[k] / 63.0;   /* (against the mean: the first slice is the onset) */
        for (unsigned k = 1; k < 64u; k++) quiet += e[k] < em * 0.2;
        OK(rms(ya) > 0.02 && quiet == 0u, "DRONE: continuous with the modulators off (%u quiet slices)", quiet);
    }
    {   /* each DEST heard: MOD 1 a TRI on it, a dense euclid, at full */
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E3] = 40; play(ya);   /* (an audio FREQ, some FM: RATIO heard) */
        for (unsigned d = 0; d < FD_N; d++) {
            setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E3] = 40; TSEL->p[P_E6] = (int16_t)FORM_DS(d, S_TRI); TSEL->p[P_E7] = FORM_EUV(9, 0); TSEL->p[P_CAOS] = 127; TSEL->p[P_ASWING] = 110; play(yb);
            OK(dlt(ya, yb) > 0.15 && peak(yb) < 1.0, "MOD 1 TRI > %s: heard (%.2f)", FORM_DEST[d], dlt(ya, yb));
        }
        unsigned bad = 0;
        for (unsigned s = 12; s < 48u; s++) {           /* every HIT, NOTE, GESTURE moves the one sound */
            setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E6] = (int16_t)FORM_DS(FD_PITCH, s); TSEL->p[P_E7] = FORM_EUV(5, 0); TSEL->p[P_CAOS] = 127; TSEL->p[P_ASWING] = 60; play(yb);
            if (!(dlt(ya, yb) > 0.05 && peak(yb) < 1.0)) { printf("form: shape %s %.3f\n", FORM_SHAPE[s], dlt(ya, yb)); bad++; }
        }
        OK(!bad, "every HIT, GESTURE and NOTE moves the sound, bounded (%u not)", bad);
    }
    {   /* REST at full, held: the silence */
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E6] = (int16_t)FORM_DS(FD_PITCH, S_REST); TSEL->p[P_E7] = FORM_EUV(9, 0); TSEL->p[P_CAOS] = 127; TSEL->p[P_ASWING] = 127; play(yb);
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; play(ya);
        double ha = 0, hb = 0; for (unsigned i = 1; i < NS; i++) { double da = ya[i] - ya[i - 1], db = yb[i] - yb[i - 1]; ha += da * da; hb += db * db; }
        OK(hb < ha * 0.25, "REST: the drone taken under hearing (%.3f of the drone's)", hb / (ha + 1e-12));
    }
    {   /* each modulator its own euclid: 3/8 strikes 3 of every 8 steps; "--" none */
        setup(0); mods_off(); TSEL->p[P_E7] = FORM_EUV(7, 0);
        unsigned hits = 0; for (unsigned s = 0; s < 64u; s++) hits += (unsigned)form_hit(TSEL->p, 0, s, 1u);
        unsigned fuse = 0; TSEL->p[P_E7] = FORM_EUV(1, 0); for (unsigned s = 0; s < 64u; s++) fuse += (unsigned)form_hit(TSEL->p, 0, s, 1u);
        unsigned none = 0; TSEL->p[P_E7] = FORM_EUV(0, 0); for (unsigned s = 0; s < 64u; s++) none += (unsigned)form_hit(TSEL->p, 0, s, 1u);
        OK(hits == 24u && fuse > 0u && fuse < 64u && !none, "EUCLID: 3/8 strikes %u of 64, FUSE %u, -- %u", hits, fuse, none);
    }
    {   /* the keys: a white a gesture into the chosen MOD, a black chooses it, a top key the root */
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; song.octave = 0; play(ya);
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; song.octave = 0; form_view[0] = 0; TSEL->p[P_E6] = (int16_t)FORM_DS(FD_FOLD, 0);
        form_key(TSEL, 1u, 100u, 1); form_key(TSEL, 1u, 0u, 0);       /* (F#3 tapped: MOD 1) */
        form_key(TSEL, 4u, 120u, 1);                                    /* (A3: TRI into MOD 1's FOLD, held) */
        play(yb); form_key(TSEL, 4u, 0u, 0);
        OK(dlt(ya, yb) > 0.15 && form_view[0] == 0, "KEYS: a white held moves the chosen MOD's DEST (%.2f)", dlt(ya, yb));
        form_key(TSEL, 3u, 100u, 1); form_key(TSEL, 3u, 0u, 0);
        OK(form_view[0] == 1 && form_key(TSEL, 24u, 100u, 1) == 48u + 7u, "KEYS: G#3 chooses MOD 2, F5 the root's fifth");
        form_view[0] = 0;
    }
    {   /* ROOT; TRIG: a key sounds it, stopped */
        setup(0); mods_off(); TSEL->p[P_ROOT] = 15; play(ya); TSEL->p[P_ROOT] = 22; play(yb);
        OK(dlt(ya, yb) > 0.3, "FREQ: the audio's, transposed (%.2f)", dlt(ya, yb));
        setup(0); mods_off(); TSEL->p[P_E2] = 127; TSEL->p[P_E3] = 0; TSEL->p[P_E4] = 0; TSEL->p[P_ROOT] = 6; play(ya);   /* a pulse at 1/16: a rhythm */
        { unsigned seg = FS * 60u / 120u / 4u, on = 0; double last = 0; for (unsigned i = 1; i < NS; i++) { double d = fabs(ya[i] - ya[i - 1]); if (d > 0.05 && i - last > seg / 2u) { on++; last = i; } }
          OK(on >= 20u && on <= 80u, "FREQ 1/16: the drone a rhythm, locked (%u edges in 4 s)", on); }
        int32_t b[CTL * 2]; double e = 0;
        TSEL->p[P_LRTRG] = 0; frames(48); trk_note_on(TSEL, 60, 100);
        for (unsigned j = 0; j < 200u; j++) { mix_block(b, CTL); for (unsigned i = 0; i < CTL * 2u; i++) e += (double)b[i] * b[i]; }
        trk_note_off(TSEL, 60);
        OK(e > 1e10, "TRIG: a key sounds it, stopped (%.2g)", e);
    }
    {   /* the pages: FORM (RUN ROOT RATE SEED), DRONE, MOD 1 .. 4 */
        setup(0); ui.home = 1; press(B_EDIT);
        int ok = cur_page()->scope == SC_FORM && cur_page()->id[0] == P_LRTRG && cur_page()->id[1] == P_ROOT;
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_E2 && cur_page()->id[3] == P_E5;
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_FRA;
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == ID_FV0;
        int16_t *vp; form_view[0] = 2; page_desc(cur_page(), 3, &vp); ok &= vp == &form_view[4];
        form_view_put(TSEL, 2, S_TRI); form_view_put(TSEL, 1, FD_NOISE); form_view_put(TSEL, 3, 4); ok &= form_vds(TSEL->p, 2) == FD_NOISE + 8u * S_TRI + 512u * 4u;
        form_view_put(TSEL, 2, S_KICK); ok &= (form_vds(TSEL->p, 2) & 511u) >> 3 == S_KICK;
        { unsigned bad2 = 0; for (unsigned sh2 = 0; sh2 < FORM_NSH; sh2++) for (unsigned d2 = 0; d2 < FD_N; d2++) for (unsigned r2 = 0; r2 < 7u; r2++) {   /* (every one stored -64 .. 127, read back) */
            int32_t a = FORM_DS(d2, sh2), b = FORM_EUV(24, r2); uint32_t want = (sh2 < 12u || (sh2 >= 24u && sh2 < 35u) ? d2 : 0u) + 8u * (sh2 == 49u ? 50u : sh2) + 512u * r2;
            TSEL->p[FORM_MF[3][0]] = (int16_t)a; TSEL->p[FORM_MF[3][1]] = (int16_t)b;
            bad2 += a < -64 || a > 127 || b < -64 || b > 127 || form_vds(TSEL->p, 3) != want; }
          ok &= !bad2; if (bad2) printf("form: %u packings wrong\n", bad2); }
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_CAOS; page_desc(cur_page(), 1, &vp); ok &= vp == &TSEL->p[FORM_MF[2][3]];
        form_view[0] = 0; frames(20);
        OK(ok, "EDIT: FORM, DRONE (FEEDBK), DRONE 2 (PALET), MOD (KNOB 1 the modulator), MOD AMT");
    }
    {   /* the dice in range */
        int ok = 1;
        for (unsigned k = 0; k < 40u; k++) {
            setup(0); form_dice(TSEL, k & 1u);
            for (unsigned m = 0; m < 4u; m++) for (unsigned f = 0; f < 4u; f++) { int v_ = TSEL->p[FORM_MF[m][f]]; if (v_ < -64 || v_ > 127) { ok = 0; printf("form: dice m%u f%u = %d\n", m, f, v_); } }
        }
        OK(ok, "40 throws of the dice in range");
    }
    {   /* stored and read back */
        static project_t q, r; static project_store_t st;
        setup(3); TSEL->p[P_ROOT] = 9; TSEL->p[P_CVOI] = 77; TSEL->p[P_KEY] = 99; TSEL->p[P_MORPH] = 11;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        for (unsigned m = 0; ok && m < 4u; m++) for (unsigned f = 0; f < 4u; f++) ok &= r.t[0].p[FORM_MF[m][f]] == q.t[0].p[FORM_MF[m][f]];
        ok &= r.t[0].p[P_ROOT] == 9 && r.t[0].p[P_LRTRG] == q.t[0].p[P_LRTRG];
        OK(ok, "the part stored and read back");
    }
    {   /* the pattern's LEN and RATE the sequencer's: 64 steps */
        setup(0);
        OK(TSEL->p[P_SLEN] == TP[P_SLEN].def, "PATTERN: LEN the sequencer's (%d)", TSEL->p[P_SLEN]);
        TSEL->p[P_SLEN] = 64; frame();
        OK(TSEL->p[P_SLEN] == 64, "PATTERN: 64 steps");
    }
    {   /* (BYTE 0.17, the owner: "start at zero") power-on with RESTORE LAST OFF: a new project, not song 1's rows */
        ui_power_on(); chain_config.row[0].slot = 2; chain_config.count = 3; proj_cur = 0; ui_prefs &= (uint8_t)~PREF_RESTORE_ON;
        song_boot_blank();
        chain_config_t empty; chain_defaults(&empty);
        OK(song_draft && proj_cur == PROJ_NO_SLOT && !memcmp(&chain_config, &empty, sizeof empty), "POWER-ON: a new project (draft), the saved songs untouched");
        song_draft = 0;
    }
    {   /* (BYTE) GLO > COMP PLAY: a white key plays its component while held; REC + PLAY: let go, it is in the lane */
        ui_power_on(); go_home(); frame();
        int ok = nav_put(SEC_GLO, "COMP PLAY") && gco_play();
        memset(glane, 0, sizeof glane); gl_touch();
        fm1_in.notes = 1u; frame();                     /* (F3: MUTE) */
        ok &= CO_ACT(gl_lv.comp) == CA_MUTE && gl_lv.mask == (1u << NTRK) - 1u;
        fm1_in.notes = 0; frame();
        ok &= !gl_lv.comp && !gl_on;                    /* (no REC: nothing written) */
        int32_t bb[CTL * 2];
        song.rec = 1u; transport_req = 1; for (unsigned j = 0; j < 40u; j++) mix_block(bb, CTL); frame();
        uint32_t g0 = clk_n;
        fm1_in.notes = 1u << 7; frame();                 /* (C4: RATCH x2, held) */
        for (unsigned j = 0; j < 2000u; j++) mix_block(bb, CTL);
        frame();
        ok &= CO_ACT(gl_lv.comp) == CA_RATCH;
        fm1_in.notes = 0; frame();
        ok &= !gl_lv.comp && gl_on && CO_ACT(glane[g0 % GL_N].comp) == CA_RATCH;
        transport_req = 2; for (unsigned j = 0; j < 40u; j++) mix_block(bb, CTL); song.rec = 0;
        OK(ok, "COMP PLAY: a key plays its component held, REC adds it to the lane (%u steps held)", (unsigned)glane[g0 % GL_N].len + 1u);
        memset(glane, 0, sizeof glane); gl_touch();
    }
    printf("form: %s\n", ft_bad ? "FAIL" : "all ok");
    return ft_bad != 0;
}
