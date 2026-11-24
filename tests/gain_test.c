/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (gainall) a synth's GAIN (EDIT > MORE KNOB 4, core.h P_GAIN; voice.c part_gain), every engine's (MATH FUNC WAVE HIVE):
 *   0        nothing run: the part's render as it was (GAIN_HASH=1 prints each engine's mix hash, to set against the base)
 *   +6 dB    a quiet patch's RMS x2 (the mix, its LEVEL low: no limiter in the way)
 *   +12 dB   a loud patch into the soft knee: bounded (not 4x), the part's limiter after it, the mix under full scale
 *            and never flat-topped; turned at once, no step (the block's ramp)
 *   the page MORE's KNOB 4 on every synth ("+6 dB"), not a kit's; the matrix's GAIN; projects and user presets keep it,
 *            an older store's 127 in the slot (HUMAN GATE) loads 0 dB */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/wait.h>
#include <unistd.h>
static int bad;
#define OK(c, s) do { bad += check(s, c); } while (0)
static const uint32_t ENG[4] = {ENGI_LOFI, ENGI_FUNC, ENGI_WAVE, ENGI_HIVE};
static const char *const ENAME[4] = {"MATH", "FUNC", "WAVE", "HIVE"};

static void setup(uint32_t e, uint32_t preset, int32_t level)
{
    uint32_t k;
    int32_t o[CTL * 2];
    ui_power_on(); song.sel = 0; select_engine(e); frame();
    for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    apply_preset_to(TSEL, preset); math_poll(); frame(); song.master_q12 = 4096;
    TSEL->p[P_DIST] = TSEL->p[P_CHOR] = TSEL->p[P_DLY] = TSEL->p[P_REV] = 0;
    TSEL->p[P_LEVEL] = (int16_t)level;
    for (k = 0; k < 20u; k++) mix_block(o, CTL);
}
static int one_note;                                    /* (the quiet patch: one soft note, the part's own render) */
/* a chord held for ms through the mix: its peak, RMS (after the first 100 ms), samples at full scale, a hash */
static void play(uint32_t vel, uint32_t ms, int32_t *peak, double *rms, uint32_t *flat, uint32_t *hash)
{
    int32_t o[CTL * 2];
    double e = 0;
    uint32_t n = FS / 1000u * ms / CTL, j, i, cnt = 0, h = 2166136261u;
    *peak = 0; *flat = 0;
    trk_note_on(TSEL, 60, vel);
    if (!one_note) { trk_note_on(TSEL, 64, vel); trk_note_on(TSEL, 67, vel); }
    for (j = 0; j < n; j++) {
        if (one_note)                                   /* (the part's own render: before the master's leveler) */
        {
            mod_begin(TSEL);                            /* (the matrix's per-block values, as mix_part) */
            track_render(TSEL, o, CTL);
            memset(o + CTL, 0, CTL * sizeof o[0]);
            if (MOD.on) mod_end(TSEL);
        }
        else
            mix_block(o, CTL);
        for (i = 0; i < CTL * 2u; i++) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > *peak) *peak = v;
            *flat += v >= 32767;
            h = (h ^ (uint32_t)o[i]) * 16777619u;
            if (j >= FS / 10u / CTL) { e += (double)o[i] * o[i]; cnt++; }
        }
    }
    *rms = sqrt(e / (cnt ? cnt : 1u));
    *hash = h;
}
/* play in a child of this process (the same state each time: the RNGs, the clocks), GAIN g, the matrix on it or not */
typedef struct { int32_t peak; double rms; uint32_t flat, hash; } res_t;
static res_t play_fork(uint32_t vel, uint32_t ms, int32_t g, int mod)
{
    res_t r = {0, 0, 0, 0};
    int fd[2];
    pid_t c;
    if (pipe(fd)) exit(3);
    fflush(stdout);
    c = fork();
    if (!c) {
        close(fd[0]);
        TSEL->p[P_GAIN] = (int16_t)g;
        if (mod) { TSEL->p[P_M1SRC] = MS_M1; TSEL->p[P_M1DST] = MD_GAIN; TSEL->p[P_M1AMT] = 63; macro_v[0] = macro_e[0] = 127; }
        play(vel, ms, &r.peak, &r.rms, &r.flat, &r.hash);
        if (write(fd[1], &r, sizeof r) != (ssize_t)sizeof r) _exit(4);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r) exit(5);
    close(fd[0]);
    waitpid(c, 0, 0);
    return r;
}
/* the part's own render (track_render: the knee, before LEVEL and the part's limiter) of a chord: its peak */
static int32_t part_peak(uint32_t ms)
{
    static int32_t b[CTL];
    int32_t pk = 0;
    uint32_t j, i;
    trk_note_on(TSEL, 48, 127); trk_note_on(TSEL, 55, 127); trk_note_on(TSEL, 60, 127); trk_note_on(TSEL, 64, 127);
    for (j = 0; j < FS / 1000u * ms / CTL; j++) {
        track_render(TSEL, b, CTL);
        for (i = 0; i < CTL; i++) pk = b[i] < 0 && -b[i] > pk ? -b[i] : b[i] > pk ? b[i] : pk;
    }
    return pk;
}
int main(void)
{
    uint32_t k;
    int32_t pk0, pk12;
    char lab[96];
    const int dump = getenv("GAIN_HASH") != 0;
    /* GAIN 0: never run (no forced filter pass, no ramp state); the render as one that never had it (GAIN_HASH=1: the
     * hashes, to set against the base's) */
    for (k = 0; k < 4u; k++) {
        res_t a, b;
        setup(ENG[k], 0, 104);
        OK(pg_db[0] == 0 && TSEL->p[P_GAIN] == 0, "GAIN 0 by default, no ramp state");
        a = play_fork(100, 600, 0, 0);
        b = play_fork(100, 600, 0, 0);
        if (dump) printf("hash %s %08x\n", ENAME[k], a.hash);
        snprintf(lab, sizeof lab, "%s: GAIN 0 renders bit for bit the same (%08x)", ENAME[k], a.hash);
        OK(a.hash == b.hash && a.rms > 50.0, lab);
    }
    /* +6 dB: x2 on a quiet patch (a soft note, the part's render: under the knee; the master's leveler not in it) */
    for (k = 0; k < 4u; k++) {
        res_t a, b;
        setup(ENG[k], 0, 80);
        one_note = 1;
        a = play_fork(50, 800, 0, 0);
        b = play_fork(50, 800, 6, 0);
        one_note = 0;
        snprintf(lab, sizeof lab, "%s: +6 dB doubles a quiet patch's RMS (%.0f -> %.0f, x%.3f)", ENAME[k], a.rms, b.rms, b.rms / a.rms);
        OK(b.rms / a.rms > 1.98 && b.rms / a.rms < 2.01, lab);
    }
    /* +12 dB on a loud patch: into the knee, bounded; the mix under full scale, no flat tops */
    for (k = 0; k < 4u; k++) {
        res_t a;
        setup(ENG[k], 0, 127);
        pk0 = part_peak(500);
        setup(ENG[k], 0, 127);
        TSEL->p[P_GAIN] = 12;
        pk12 = part_peak(500);
        snprintf(lab, sizeof lab, "%s: +12 dB into the knee, the part bounded (peak %d -> %d, < 80000)", ENAME[k], pk0, pk12);
        OK(pk12 > pk0 && pk12 < 80000, lab);
        setup(ENG[k], 0, 127);
        a = play_fork(127, 800, 12, 0);
        snprintf(lab, sizeof lab, "%s: +12 dB, the mix under full scale, never flat-topped (peak %d, %u at 32767)", ENAME[k], a.peak, a.flat);
        OK(a.peak < 32767 && !a.flat, lab);
    }
    /* IMAGE on (its sides with the sum, through the knee): +12 dB still under full scale */
    {
        res_t a;
        setup(ENGI_WAVE, 0, 127);
        TSEL->p[P_VOICE] = V_POLY; TSEL->p[P_SIMG] = 0;
        a = play_fork(127, 800, 12, 0);
        snprintf(lab, sizeof lab, "WAVE, IMAGE 100 %%: +12 dB, the mix under full scale, never flat-topped (peak %d)", a.peak);
        OK(a.peak < 32767 && !a.flat && a.rms > 1000.0, lab);
    }
    /* turned while it plays: from 0 to +12 in a block, no step between two samples bigger than the sound's own */
    {
        static int32_t b[CTL];
        int32_t prev = 0, jump = 0, jump0 = 0;
        uint32_t j, i;
        setup(ENGI_WAVE, 0, 104);
        trk_note_on(TSEL, 60, 100);
        for (j = 0; j < 200u; j++) {
            if (j == 100u) TSEL->p[P_GAIN] = 12;
            track_render(TSEL, b, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t d = b[i] - prev;
                d = d < 0 ? -d : d;
                if (j > 20u && j < 100u && d > jump0) jump0 = d;
                if (j == 100u && d > jump) jump = d;
                prev = b[i];
            }
        }
        snprintf(lab, sizeof lab, "GAIN turned 0 -> +12 at once: ramped over the block (step %d, the sound's own %d)", jump, jump0);
        OK(jump < jump0 * 3, lab);
    }
    /* the page: MORE's KNOB 4 on every synth, "+6 dB"; not on a kit */
    for (k = 0; k < 4u; k++) {
        int16_t *v = 0;
        const param_desc_t *d;
        char val[8];
        const char *unit;
        setup(ENG[k], 0, 104);
        go_title("MORE");
        d = page_desc(cur_page(), 3, &v);
        TSEL->p[P_GAIN] = 6;
        param_format(d, 6, val, &unit);
        snprintf(lab, sizeof lab, "%s: EDIT > MORE KNOB 4 is GAIN, 0 .. +12 dB, shown %s %s", ENAME[k], val, unit);
        OK(page_visible(ui.page) && str_eq(cur_page()->title, "MORE") && d && v == &TSEL->p[P_GAIN] &&
           str_eq(d->label, "GAIN") && d->min == 0 && d->max == 12 && d->def == 0 && str_eq(val, "+6") && str_eq(unit, "dB") &&
           str_eq(knob_meaning("GAIN"), "LEVEL BOOST, NO DISTORTION"), lab);
        OK(mod_dst_ok(TSEL, MD_GAIN) && mod_param(MD_GAIN) == P_GAIN && str_eq(mod_dst_name_bt(TSEL, MD_GAIN), "GAIN"),
           "the matrix's DEST GAIN, a synth's");
    }
    {
        uint32_t i, any = 0;
        setup(ENGI_DRUM, 0, 104);
        for (i = 0; i < NPAGES; i++)
            any |= page_visible(i) && PAGES[i].id[3] == P_GAIN;
        OK(!any && !mod_dst_ok(TSEL, MD_GAIN), "a kit: no MORE GAIN (DRUM-W keeps its own, COLOR's), no DEST GAIN");
    }
    /* the matrix on it: MACRO 1 at full through GAIN, AMT 63 (+11 dB) */
    {
        res_t a, b;
        setup(ENGI_WAVE, 0, 80);
        one_note = 1;
        a = play_fork(50, 600, 0, 0);
        b = play_fork(50, 600, 0, 1);
        one_note = 0;
        snprintf(lab, sizeof lab, "MACRO 1 -> GAIN: louder (RMS %.0f -> %.0f)", a.rms, b.rms);
        OK(b.rms > a.rms * 3.0, lab);
    }
    /* saved and loaded: the project, a user preset; an older store's 127 there (HUMAN GATE): 0 dB, HUMAN as it was */
    {
        static project_t a, b;
        static project_store_t st;
        setup(ENGI_WAVE, 0, 104);
        TSEL->p[P_GAIN] = 7;
        trk[1].p[P_GAIN] = 0;
        project_capture(&a);
        OK(proj_pack(&st, &a) && proj_import(&b, st.raw, sizeof st.raw) && b.t[0].p[P_GAIN] == 7 && b.t[1].p[P_GAIN] == 0,
           "GAIN kept in the project (+7 dB, 0 dB)");
        TSEL->p[P_GAIN] = 0;
        project_restore_runtime(&b);
        OK(trk[0].p[P_GAIN] == 7 && trk[1].p[P_GAIN] == 0, "the project loaded: +7 dB back");
        b.t[0].p[P_GAIN] = 127; b.t[0].p[P_HVEL] = 127; b.sum = proj_sum(&b);
        project_restore_runtime(&b);
        OK(trk[0].p[P_GAIN] == 0 && trk[0].p[P_HVEL] == 127, "an older project's 127 there: 0 dB, HUMAN 0 % as it was");
        b.t[0].p[P_GAIN] = 100; b.sum = proj_sum(&b);
        project_restore_runtime(&b);
        OK(trk[0].p[P_GAIN] == 0 && trk[0].p[P_HVEL] == 100, "a 0.6.87 project's HUMAN GATE apart: folded into HUMAN, 0 dB");
        OK(param_store_def(ENGI_WAVE, P_GAIN) == 0 && param_store_def(ENGI_FORM, P_GAIN) == 127 &&
           param_fit(param_desc_of(ENGI_FORM, P_GAIN), 64) == 64, "the slot's stored default 0 dB (a FORM part's RATE B: 127, kept)");
    }
    {
        up_rec_t r;
        int16_t v[P_COUNT];
        setup(ENGI_HIVE, 0, 104);
        TSEL->p[P_GAIN] = 9;
        up_store(0, "GAIN9");
        TSEL->p[P_GAIN] = 0;
        OK(up_load(0) == 0 && TSEL->p[P_GAIN] == 9, "a user preset keeps GAIN (+9 dB)");
        r = *up_rec(0);
        up_set_value(&r, P_GAIN, 127);
        up_values(&r, v);
        OK(v[P_GAIN] == 0, "an older user preset's 127 there: 0 dB");
        apply_preset_to(TSEL, 1);
        OK(TSEL->p[P_GAIN] == 0, "a factory preset: 0 dB (the sound's, not the track's)");
    }
    printf("gain: %s\n", bad ? "FAIL" : "all ok");
    return !!bad;
}
