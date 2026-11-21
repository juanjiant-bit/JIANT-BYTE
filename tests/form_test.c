/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* FORM (BYTE 0.17, eng_form.c): every recipe compiled as A and as B; every preset sounding and bounded; MORPH at equal
 * power; each macro's stage heard above its middle; MOTION moving the sound; the drums on the 16ths; ROOT; TRIG; its
 * pages; the dice in range; the panel drawn; the part stored and read back; the sequencer's 64 steps */
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
    apply_preset_to(TSEL, pr); TSEL->p[P_LRTRG] = 1; frames(64); song.master_q12 = 4096;   /* (RUN PLAY, the transport's) */
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
int main(void)
{
    {   /* every recipe compiles, stateless (B's slot runs none), 64 instructions at most */
        int bad = 0;
        for (unsigned f = 1; f < FORM_NT; f++) {
            static fvm_cc_t cc; static fvm_ins_t ins[FVM_MAXINS]; static fvm_prog_t p; fvm_err_t e;
            const char *s = form_recipe(f, "");
            fvm_prog_init(&p, ins, FVM_MAXINS);
            if (fvm_compile(&cc, &p, s, (uint32_t)strlen(s), &e) || (f > 2u && p.nstate)) { printf("form: %s: error %u at %u\n", FORM_NAMES[f], e.code, e.pos); bad++; }
        }
        OK(!bad, "the %u recipes compile", FORM_NT - 1u);
    }
    {   /* every preset sounds, bounded */
        int ok = 1;
        for (unsigned pr = 0; pr < NELEM(FORM_PRESETS); pr++) {
            setup(pr); play(ya);
            double r = rms(ya), pk = peak(ya);
            if (!(r > 0.01 && pk < 1.0)) { printf("form: preset %s rms %.3f peak %.3f\n", FORM_PRESETS[pr].name, r, pk); ok = 0; }
        }
        OK(ok, "the %u presets sound, bounded", (unsigned)NELEM(FORM_PRESETS));
    }
    {   /* MORPH: A, B, between them at equal power */
        setup(1); TSEL->p[P_E0] = 3; TSEL->p[P_E1] = 6; TSEL->p[P_E2] = 0; TSEL->p[P_APROB] = 0; TSEL->p[P_DETUNE] = 0; frames(64); play(ya);
        double ra = rms(ya); TSEL->p[P_E2] = 127; play(yb); double rb = rms(yb), d = dlt(ya, yb);
        TSEL->p[P_E2] = 64; play(yb); double rm = rms(yb);
        OK(d > 0.3 && rm > 0.5 * (ra < rb ? ra : rb), "MORPH: A %.3f, B %.3f apart (%.2f), between %.3f", ra, rb, d, rm);
    }
    {   /* TIMBRE: each stage 0 out, heard turned up; SHAPE's knobs the recipe's, named by it; RATE the formula's time */
        static const unsigned ID[3] = {P_AGATE, P_ARATE, P_AORDER};
        static const char *const NM[3] = {"FOLD", "RING", "CRUSH"};
        for (unsigned k = 0; k < 3u; k++) {
            setup(1); TSEL->p[P_E0] = 3; TSEL->p[P_E2] = 0; TSEL->p[P_APROB] = 0; TSEL->p[P_DETUNE] = 0; TSEL->p[P_KEY] = 0;
            frames(64); play(ya); TSEL->p[ID[k]] = 127; play(yb);
            OK(dlt(ya, yb) > 0.2 && peak(yb) < 1.0, "%s: heard (%.2f)", NM[k], dlt(ya, yb));
        }
        setup(1); TSEL->p[P_E0] = 3; frames(64);
        OK(!strcmp(track_desc(TSEL, P_E4)->label, "VOWEL") && !strcmp(track_desc(TSEL, P_E7)->label, "SEED"), "SHAPE: GROWL's A B C D named (%s)", track_desc(TSEL, P_E4)->label);
        setup(1); TSEL->p[P_E0] = 10; TSEL->p[P_KEY] = 0; TSEL->p[P_DETUNE] = 0; TSEL->p[P_APROB] = 0; frames(64); play(ya);
        TSEL->p[P_MORPH] = 0; play(yb);
        OK(dlt(ya, yb) > 0.3, "RATE: x1/4 another time (%.2f)", dlt(ya, yb));
        setup(1); TSEL->p[P_E0] = 3; TSEL->p[P_KEY] = 0; TSEL->p[P_DETUNE] = 0; TSEL->p[P_FCUT] = 127; TSEL->p[P_FRES] = 0; frames(64); play(ya);
        TSEL->p[P_FCUT] = 40; play(yb);
        OK(rms(yb) < rms(ya) * 0.9, "FILTER: the part's own (%.3f -> %.3f)", rms(ya), rms(yb));
    }
    {   /* MOTION moves it */
        setup(1); TSEL->p[P_E0] = 3; TSEL->p[P_APROB] = 0; TSEL->p[P_DETUNE] = 0; frames(64); play(ya);
        TSEL->p[P_APROB] = 120; TSEL->p[P_ASWING] = 0; play(yb);
        OK(dlt(ya, yb) > 0.2, "MOTION: the curve moves the sound (%.2f)", dlt(ya, yb));
    }
    {   /* the drums: on, the 16ths' energy; off, none */
        setup(11); TSEL->p[P_DETUNE] = 0; frames(64); play(ya); double r0 = rms(ya);
        TSEL->p[P_DETUNE] = 127; TSEL->p[P_DXIMG] = 100; play(yb); double r1 = rms(yb);
        OK(r1 > r0 * 1.05 && dlt(ya, yb) > 0.2, "DRUMS: heard (%.3f -> %.3f)", r0, r1);
    }
    {   /* GATE: struck on euclids, rests between (the envelope's spread); 0 the formula as it is */
        setup(1); TSEL->p[P_E0] = 10; TSEL->p[P_DETUNE] = 0; TSEL->p[P_APROB] = 0; TSEL->p[P_KEY] = 0; frames(64); play(ya);
        TSEL->p[P_KEY] = 127; TSEL->p[P_DXIMG] = 40; play(yb);
        double ea[32] = {0}, eb[32] = {0}; unsigned seg = NS / 32u, quiet = 0;
        for (unsigned i = 0; i < NS; i++) { ea[i / seg % 32u] += ya[i] * ya[i]; eb[i / seg % 32u] += yb[i] * yb[i]; }
        for (unsigned k = 0; k < 32u; k++) quiet += eb[k] < ea[k] * 0.1;
        OK(quiet >= 4u && rms(yb) > 0.005, "GATE: rests between the hits (%u of 32 slices quiet)", quiet);
    }
    {   /* ROOT transposes; TRIG: a key sounds it stopped */
        setup(1); TSEL->p[P_E0] = 6; TSEL->p[P_DETUNE] = 0; TSEL->p[P_APROB] = 0; frames(64); play(ya);
        TSEL->p[P_E3] = 7; play(yb);
        OK(dlt(ya, yb) > 0.3, "ROOT: transposed (%.2f)", dlt(ya, yb));
        int32_t b[CTL * 2]; double e = 0;
        TSEL->p[P_LRTRG] = 0; frames(48); trk_note_on(TSEL, 60, 100);
        for (unsigned j = 0; j < 200u; j++) { mix_block(b, CTL); for (unsigned i = 0; i < CTL * 2u; i++) e += (double)b[i] * b[i]; }
        trk_note_off(TSEL, 60);
        OK(e > 1e10, "TRIG: a key sounds it, stopped (%.2g)", e);
    }
    {   /* the pages: FORM (RUN first), SHAPE, MOTION, PULSE */
        setup(0); ui.home = 1; press(B_EDIT);
        int ok = cur_page()->scope == SC_FORM && cur_page()->id[0] == P_LRTRG && cur_page()->id[1] == P_E3 && cur_page()->id[2] == P_E0;
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_E4;
        turn(EN_SELECT, 1); ok &= cur_page()->scope == SC_FORM && cur_page()->id[0] == P_AGATE;
        turn(EN_SELECT, 1); ok &= cur_page()->scope == SC_FORM && cur_page()->id[0] == P_E2 && cur_page()->id[1] == P_CAOS;
        turn(EN_SELECT, 1); ok &= cur_page()->scope == SC_FORM && cur_page()->id[0] == P_MORPH && cur_page()->id[2] == P_KEY && cur_page()->id[3] == P_DETUNE;
        frames(20);
        OK(ok, "EDIT: FORM (RUN ROOT A B), SHAPE, TIMBRE, MOTION, PULSE");
    }
    {   /* the dice in range */
        int ok = 1;
        for (unsigned k = 0; k < 40u; k++) {
            setup(0); form_dice(TSEL, k & 1u);
            ok &= TSEL->p[P_E0] >= 0 && TSEL->p[P_E0] < (int)FORM_NT && TSEL->p[P_E1] >= 0 && TSEL->p[P_E1] < (int)FORM_NT &&
                  TSEL->p[P_CAOS] <= 3 && TSEL->p[P_ASWING] <= 5 && TSEL->p[P_APROB] <= 127 && TSEL->p[P_DETUNE] <= 127 && TSEL->p[P_DXIMG] <= 127 && TSEL->p[P_AGATE] <= 127 && TSEL->p[P_AORDER] <= 127;
            for (unsigned j = 0; j < 4u; j++) ok &= TSEL->p[P_E4 + j] >= 0 && TSEL->p[P_E4 + j] <= 127;
        }
        OK(ok, "40 throws of the dice in range");
    }
    {   /* stored and read back */
        static project_t q, r; static project_store_t st;
        static const unsigned ID[] = {P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_LRTRG, P_CAOS, P_ASWING, P_APROB, P_DETUNE, P_DXIMG, P_KEY, P_AGATE, P_ARATE, P_AORDER, P_MORPH};
        setup(5); TSEL->p[P_E4] = 99; TSEL->p[P_E3] = -5; TSEL->p[P_CAOS] = 2; TSEL->p[P_AGATE] = 50; TSEL->p[P_MORPH] = 4; TSEL->p[P_DXIMG] = 3;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        for (unsigned k = 0; ok && k < NELEM(ID); k++) ok &= r.t[0].p[ID[k]] == q.t[0].p[ID[k]];
        OK(ok, "the part stored and read back");
    }
    {   /* the pattern's LEN and RATE the sequencer's: 64 steps, its own resolution */
        setup(0);
        OK(TSEL->p[P_SLEN] == TP[P_SLEN].def && param_desc_of(ENGI_FORM, P_SLEN) == &TP[P_SLEN], "PATTERN: LEN the sequencer's (%d)", TSEL->p[P_SLEN]);
        TSEL->p[P_SLEN] = 64; TSEL->p[P_SDIV] = 4; frame();
        OK(TSEL->p[P_SLEN] == 64, "PATTERN: 64 steps");
    }
    printf("form: %s\n", ft_bad ? "FAIL" : "all ok");
    return ft_bad != 0;
}
