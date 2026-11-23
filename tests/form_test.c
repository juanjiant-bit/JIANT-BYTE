/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DRONE CLAY (BYTE: FORM's composer as a track's sequencer, eng_form.c clay.c): its drone sounds low on the track's
 * voice, held or struck at RATE; a modulator moves its DEST (the sound differs); a NOTE gesture plays real notes; OFF
 * silent; the pages' knobs; the settings stored and read back; a FORM part of an older project loads as WAVE with its
 * modulators in DRONE CLAY. Then the power-on and COMP PLAY checks kept from FORM's test */
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
        if (clay_v[0][0] > vmax) vmax = clay_v[0][0];
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
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_DEC] = 40; play(ya);
        setup(); mods_off(); trk[0].p[P_FCUT] = 40; clay[0][CL_MF(0, 0u)] = 0; clay[0][CL_MF(0, 1u)] = 24; clay[0][CL_MF(0, 2u)] = 2;
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_DEC] = 40; play(yb);
        OK(dlt(ya, yb) > 0.05, "a modulator on CUTOFF moves the sound (%.3f of it, vmax %d, rms %.4f %.4f)", dlt(ya, yb), vmax, rms(ya), rms(yb));
        setup(); mods_off(); trk[0].p[P_FCUT] = 40; clay[0][CL_MF(0, 0u)] = 12; clay[0][CL_MF(0, 1u)] = 12; clay[0][CL_MF(0, 2u)] = 2;
        clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_DEC] = 40; play(ya);
        OK(dlt(ya, yb) > 0.2, "a modulator on the sound's KNOB 1 moves it (%.3f of it)", dlt(ya, yb));
    }
    {   /* a NOTE gesture: real notes on the voice */
        setup(); mods_off(); clay[0][CL_MF(1, 0u)] = 0; clay[0][CL_MF(1, 1u)] = 36; clay[0][CL_MF(1, 2u)] = 2;
        clay[0][CL_MF(1, 3u)] = 110; clay[0][CL_DEC + 1] = 30; play(ya);
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
        ok &= d && d->names && !strcmp(d->names[0], "KICK");   /* (the owner: the HITS first) */
        OK(ok, "SEQ > DRONE CLAY: ON from blank its first sound, SHAPE's first a HIT (%s)", d && d->names ? d->names[0] : "-");
    }
    {   /* stored and read back */
        static project_t q, r; static project_store_t st;
        setup(); clay[0][CL_SEED] = 77; clay[0][CL_MF(2, 0u)] = 15;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        for (unsigned f = 0; ok && f < CLAY_NF; f++) ok &= r.clay[0][f] == (uint8_t)clay[0][f];
        ok &= !r.clay[1][CL_ON];
        OK(ok, "the settings stored and read back");
    }
    {   /* a FORM part of an older project: WAVE, its modulators in DRONE CLAY */
        static project_t q, r; static project_store_t st;
        setup(); memset(clay, 0, sizeof clay);
        project_capture(&q); q.t[0].engine = ENGI_FORM;
        q.t[0].p[FORM_MF[0][0]] = (int16_t)FORM_DS(FD_PITCH, S_KICK); q.t[0].p[FORM_MF[0][1]] = (int16_t)FORM_EUV(2, 0);
        q.t[0].p[FORM_MF[0][2]] = 110; q.t[0].p[FORM_MF[0][3]] = 9;
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw) && !project_restore_runtime(&r);
        ok &= trk[0].eng_req == ENGI_WAVE && clay[0][CL_ON] && clay_sh(clay[0][CL_MF(0, 1u)]) == S_KICK &&
              clay[0][CL_MF(0, 2u)] == 2 && clay[0][CL_MF(0, 3u)] == 110 && clay[0][CL_DEC] == 9;
        OK(ok, "a FORM part loads as WAVE with its modulators in DRONE CLAY (%u %d %d %d %d %d)", trk[0].eng_req, clay[0][CL_ON], (int)clay_sh(clay[0][CL_MF(0, 1u)]), clay[0][CL_MF(0, 2u)], clay[0][CL_MF(0, 3u)], clay[0][CL_DEC]);
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
