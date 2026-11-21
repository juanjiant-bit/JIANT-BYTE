/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* FORM (BYTE 0.17, eng_form.c): the drone entity. Every preset sounds, bounded; the drone never stops by itself; each
 * modulator's euclid strikes on its own steps; each DEST heard (UP DIVE NOISE FM FOLD CRUSH SHAPE CHORD JUMP); DIVE's
 * bottom silent; ROOT; TRIG a key; the pages (RUN first); the dice in range; the part stored and read back; the
 * sequencer's 64 steps its own */
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
static void mods_off(void) { for (unsigned m = 0; m < 4u; m++) TSEL->p[FORM_MF[m][0]] = 0; }
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
        for (unsigned k = 1; k < 64u; k++) quiet += e[k] < e[0] * 0.2;
        OK(rms(ya) > 0.02 && quiet == 0u, "DRONE: continuous with the modulators off (%u quiet slices)", quiet);
    }
    {   /* each DEST heard, through MOD 1 at full, a dense euclid */
        static const char *const NM[FD_N] = {"OFF", "UP", "DIVE", "NOISE", "FM", "FOLD", "CRUSH", "SHAPE", "CHORD", "JUMP"};
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; play(ya);   /* (at an audio FREQ: the timbre's dests heard as timbre) */
        for (unsigned d = 1; d < FD_N; d++) {
            setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E6] = (int16_t)d; TSEL->p[P_E7] = 3; TSEL->p[P_CAOS] = 127; TSEL->p[P_ASWING] = 110; play(yb);
            OK(dlt(ya, yb) > 0.15 && peak(yb) < 1.0, "MOD 1 > %s: heard (%.2f)", NM[d], dlt(ya, yb));
        }
    }
    {   /* DIVE at full, held: the silence */
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; TSEL->p[P_E6] = FD_DIVE; TSEL->p[P_E7] = 3; TSEL->p[P_CAOS] = 127; TSEL->p[P_ASWING] = 127; play(yb);
        setup(0); mods_off(); TSEL->p[P_ROOT] = 12; play(ya);
        double ha = 0, hb = 0; for (unsigned i = 1; i < NS; i++) { double da = ya[i] - ya[i - 1], db = yb[i] - yb[i - 1]; ha += da * da; hb += db * db; }   /* (heard: the slope's energy, the sub-audible out) */
        OK(hb < ha * 0.25, "DIVE: the drone taken under hearing (%.3f of the drone's)", hb / (ha + 1e-12));
    }
    {   /* each modulator its own euclid: 3/8 strikes 3 of every 8 steps */
        setup(0); mods_off(); TSEL->p[P_E6] = FD_UP; TSEL->p[P_E7] = 6;
        unsigned hits = 0; for (unsigned s = 0; s < 64u; s++) hits += (unsigned)form_hit(TSEL->p, 0, s, 1u);
        unsigned fuse = 0; TSEL->p[P_E7] = 0; for (unsigned s = 0; s < 64u; s++) fuse += (unsigned)form_hit(TSEL->p, 0, s, 1u);
        OK(hits == 24u && fuse > 0u && fuse < 64u, "EUCLID: 3/8 strikes %u of 64, FUSE %u", hits, fuse);
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
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_E2;
        for (unsigned m = 0; m < 4u; m++) { turn(EN_SELECT, 1); ok &= cur_page()->id[0] == FORM_MF[m][0] && cur_page()->id[3] == FORM_MF[m][3]; }
        frames(20);
        OK(ok, "EDIT: FORM (RUN FREQ RATE SEED), DRONE, MOD 1 .. 4");
    }
    {   /* the dice in range */
        int ok = 1;
        for (unsigned k = 0; k < 40u; k++) {
            setup(0); form_dice(TSEL, k & 1u);
            for (unsigned m = 0; m < 4u; m++) ok &= TSEL->p[FORM_MF[m][0]] < FD_N && TSEL->p[FORM_MF[m][1]] < (int)FORM_NEU && TSEL->p[FORM_MF[m][2]] <= 127 && TSEL->p[FORM_MF[m][3]] <= 127;
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
    printf("form: %s\n", ft_bad ? "FAIL" : "all ok");
    return ft_bad != 0;
}
