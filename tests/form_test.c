/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* FORM 3 (eng_form.c): every formula of the bank compiled in FORMULA B's 32 instructions; every preset sounding and
 * bounded; each control heard (MORPH, DEFORM A, DEFORM B, RATE, SWING, PROGRESS, CHORD RATE, NOTES, OCTAVE); the
 * progression's chords changing in time; its pages; the dice in range; the panel drawn; the part stored and read back */
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
    apply_preset_to(TSEL, pr); frame(); song.master_q12 = 4096;
}
static void play(double *y)
{
    int32_t b[CTL * 2];
    unsigned n = 0;
    press(B_PLAY);
    while (n < NS) { mix_block(b, CTL); for (unsigned i = 0; i < CTL && n < NS; i++) y[n++] = (b[2 * i] + b[2 * i + 1]) / 65536.0; }
    press(B_PLAY);
}
static double rms(const double *y) { double e = 0; for (unsigned i = 0; i < NS; i++) e += y[i] * y[i]; return sqrt(e / NS); }
static double peak(const double *y) { double p = 0; for (unsigned i = 0; i < NS; i++) if (fabs(y[i]) > p) p = fabs(y[i]); return p; }
static double dlt(const double *a, const double *b) { double d = 0, e = 0; for (unsigned i = 0; i < NS; i++) d += (a[i] - b[i]) * (a[i] - b[i]), e += a[i] * a[i]; return sqrt(d / (e + 1e-12)); }
int main(void)
{
    {   /* the bank */
        static fvm_cc_t cc; static fvm_ins_t ins[32]; static fvm_prog_t p;
        unsigned bad = 0;
        for (unsigned f = 0; f < FORM_N; f++) {
            fvm_prog_init(&p, ins, 32); fvm_err_t e = {0};
            if (fvm_compile(&cc, &p, FORM_BANK[f], (uint32_t)strlen(FORM_BANK[f]), &e) || p.nstate > 2u) { printf("form: %s: error %u at %u\n", FORM_NAMES[f], e.code, e.pos); bad++; }
        }
        OK(!bad, "the %u formulas compile in 32 instructions, 2 states at most (and FORM 1's %u states)", (unsigned)FORM_N, (unsigned)F1_N);
    }
    for (unsigned pr = 0; pr < NELEM(FORM_PRESETS); pr++) {   /* every preset: sounding, bounded */
        setup(pr); play(ya);
        OK(rms(ya) > 0.02 && peak(ya) < 1.0, "%-10s sounds, bounded: rms %.3f peak %.3f", FORM_PRESETS[pr].name, rms(ya), peak(ya));
    }
    {   /* each control heard */
        static const struct { const char *nm; unsigned id; int16_t v; } C[] = {
            {"MORPH", P_E2, 127}, {"SWING", P_E3, 70}, {"DEF A", P_E4, 127}, {"OCT A", P_E6, 3}, {"RATE A", P_SDIV, 2},
            {"PROG", P_AGATE, 4}, {"LENGTH", P_ARATE, 0}, {"CHORD", P_CHRD, 7}, {"VOICING", P_CVOI, 5}, {"HITS", P_KEY, 5}};
        for (unsigned k = 0; k < NELEM(C); k++) {
            setup(0); play(ya);
            setup(0); TSEL->p[C[k].id] = C[k].v; play(yb);
            OK(dlt(ya, yb) > 0.1, "%s changes the sound (%.3f)", C[k].nm, dlt(ya, yb));
        }
        static const struct { const char *nm; unsigned id; int16_t v; } CB[] = {{"DEF B", P_E5, 0}, {"OCT B", P_E7, 3}, {"RATE B", P_SLEN, 3}};
        for (unsigned k = 0; k < NELEM(CB); k++) {
            setup(0); TSEL->p[P_E2] = 127; play(ya);
            setup(0); TSEL->p[P_E2] = 127; TSEL->p[CB[k].id] = CB[k].v; play(yb);
            OK(dlt(ya, yb) > 0.1, "%s changes B (%.3f)", CB[k].nm, dlt(ya, yb));
        }
        setup(0); TSEL->p[P_KEY] = 5; play(ya);
        setup(0); TSEL->p[P_KEY] = 5; TSEL->p[P_DETUNE] = 3; play(yb);
        OK(dlt(ya, yb) > 0.1, "ROTATE moves the hits (%.3f)", dlt(ya, yb));
    }
    {   /* the chords change in time: the progression's first and second chord differ in pitch */
        form_note_t a, b;
        setup(3); TSEL->p[P_AGATE] = 0; TSEL->p[P_ARATE] = 2;
        form_harmony(TSEL, 0u, 0u, &a); form_harmony(TSEL, 4u << 16, 16u, &b);
        OK(a.chord == 0u && b.chord == 1u && a.note != b.note, "a bar on, the next chord (%d -> %d)", a.note, b.note);
        char r[8]; form_scale_t sc; form_scale(TSEL, &sc); form_roman(&sc, 0, r);
        OK(r[0] == 'i' || r[0] == 'I', "the first chord's numeral %s", r);
    }
    {   /* the pages */
        setup(0); ui.home = 1; press(B_EDIT);
        int ok = cur_page()->id[0] == P_E0;
        turn(EN_SELECT, 1); ok &= cur_page()->id[0] == P_E4;
        turn(EN_SELECT, 1); ok &= cur_page()->scope == SC_FORM && cur_page()->id[0] == P_AGATE;
        turn(EN_SELECT, 1); ok &= cur_page()->scope == SC_FORM && cur_page()->id[0] == P_SDIV;
        frames(20);
        OK(ok, "EDIT: FORM, DEFORM, HARMONY, PULSE");
    }
    {   /* the dice in range */
        int ok = 1;
        for (unsigned k = 0; k < 40u; k++) {
            setup(0); form_dice(TSEL, k & 1u);
            ok &= TSEL->p[P_E0] >= 0 && TSEL->p[P_E0] < (int)FORM_NT && TSEL->p[P_E1] >= 0 && TSEL->p[P_E1] < (int)FORM_NT &&
                  TSEL->p[P_AGATE] < (int)FORM_NPROG && TSEL->p[P_ARATE] <= 4 && TSEL->p[P_AORDER] <= 6 && TSEL->p[P_KEY] >= 1 && TSEL->p[P_KEY] <= 16 && TSEL->p[P_DETUNE] <= 15 && TSEL->p[P_CHRD] <= 11 && TSEL->p[P_CVOI] <= 7;
        }
        OK(ok, "40 throws of the dice in range");
    }
    {   /* stored and read back */
        static project_t q, r; static project_store_t st;
        static const unsigned ID[] = {P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_AGATE, P_ARATE, P_AORDER, P_LRTRG, P_SDIV, P_SLEN, P_KEY, P_DETUNE, P_CHRD, P_CVOI};
        setup(5); TSEL->p[P_AGATE] = 7; TSEL->p[P_E4] = 99; TSEL->p[P_KEY] = 7; TSEL->p[P_CHRD] = 4;
        project_capture(&q);
        int ok = proj_pack(&st, &q) && proj_import(&r, st.raw, (int)sizeof st.raw);
        for (unsigned k = 0; ok && k < NELEM(ID); k++) ok &= r.t[0].p[ID[k]] == q.t[0].p[ID[k]];
        OK(ok, "the part stored and read back");
    }
    printf("form: %s\n", ft_bad ? "FAIL" : "all ok");
    return ft_bad != 0;
}
