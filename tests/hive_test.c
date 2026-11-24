/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE 0.17) HIVE (eng_hive.c), the four strings with feedback, rendered as the device does:
 *   every preset    sounds, bounded (no sample past full scale, no runaway), its drone held on with HOLD
 *   FEEDBK          low: a pluck that dies while the key is held; high: the strings on by themselves after the touch
 *   CROSS           moves the spectrum (the partner's length modulation); PAIR / two keys: the second pair on the second
 *   the rows        the COMB line and the kit row given back clean (the engine changed, the voice ended)
 *   low notes       the strings at 11 kHz / an octave up, bounded; the CPU of a block against WAVE's one voice
 *   ECHO (hive2)    the strings' own level kept (the echo on top: no wall of its loop through the strings), a repeat
 *                   at TIME's delay over TIME's range, no runaway at a low FEEDBK
 *   the new E1..E8  STIFF BODY POS DRIVE each move the spectrum, the level kept; each moved by the matrix's LFO with
 *                   no zipper (the output's roughest step against the knob still)
 *   HIVE_WAV_DIR=dir writes each preset's render there (5 s: the note 3 s, then let go)
 *   cc -O2 -Ibuild/gen -Ifirmware/src -Itests -o build/host/hive_test tests/hive_test.c -lm && build/host/hive_test */
#include <stdarg.h>
#if defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>                          /* (HIVE_VG: the instruction counts, under callgrind) */
#define HIVE_HAS_VG 1
#endif
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void ok(int c, const char *fmt, ...)
{
    va_list ap;
    char b[300];
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("hive: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

#define SECS 5u
#define NS (FS * SECS)
static double ya[NS], yb[NS];

static void setup(uint32_t engine, uint32_t preset)
{
    track_t *t = &trk[0];
    memset(trk, 0, sizeof trk);
    memset(drum_kit, 0, sizeof drum_kit);                /* (no state from the render before) */
    host_tracks_init();
    host_preset(t, engine, preset);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
}
/* notes held from 0 for `on` samples (k notes, the second 0.5 s after the first), then let go; n samples in all */
static void play(const uint8_t *notes, uint32_t k, uint32_t on, double *y, uint32_t n)
{
    static int32_t blk[CTL];
    uint32_t got = 0, j, played = 0;
    while (got < n) {
        uint32_t i;
        for (j = played; j < k && got >= j * (FS / 2u); j++, played++)
            trk_note_on(&trk[0], notes[j], 100);
        if (got >= on && got < on + CTL)
            for (j = 0; j < k; j++)
                trk_note_off(&trk[0], notes[j]);
        track_render(&trk[0], blk, CTL);
        for (i = 0; i < CTL && got < n; i++)
            y[got++] = blk[i] / 32768.0;
    }
}
static double peak(const double *a, uint32_t n)
{
    double p = 0;
    for (uint32_t i = 0; i < n; i++) if (fabs(a[i]) > p) p = fabs(a[i]);
    return p;
}
static double rms(const double *a, uint32_t n)
{
    double s = 0;
    for (uint32_t i = 0; i < n; i++) s += a[i] * a[i];
    return n ? sqrt(s / n) : 0;
}
static int finite_all(const double *a, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) if (!isfinite(a[i])) return 0;
    return 1;
}
static double goertzel(const double *a, uint32_t n, double hz)   /* the power at hz */
{
    double w = 2 * M_PI * hz / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (uint32_t i = 0; i < n; i++) {
        double s0 = a[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)n * n);
}
/* a spectrum: the power in 96 bands (a sixth of an octave each from 40 Hz), normalised */
static void spectrum(const double *a, uint32_t n, double *S)
{
    double t = 0;
    for (uint32_t b = 0; b < 96u; b++) {
        double f0 = 40.0 * pow(2.0, b / 6.0), p = 0;
        for (uint32_t k = 0; k < 4u; k++) p += goertzel(a, n, f0 * pow(2.0, k / 24.0));
        S[b] = p;
        t += p * p;
    }
    t = sqrt(t);
    for (uint32_t b = 0; b < 96u; b++) S[b] = t > 0 ? S[b] / t : 0;
}
static double spec_sim(const double *a, const double *b, uint32_t n)
{
    double A[96], B[96], d = 0;
    spectrum(a, n, A);
    spectrum(b, n, B);
    for (uint32_t k = 0; k < 96u; k++) d += A[k] * B[k];
    return d;
}
static double centroid(const double *a, uint32_t n)
{
    double A[96], s = 0, w = 0;
    spectrum(a, n, A);
    for (uint32_t k = 0; k < 96u; k++) { s += A[k] * k; w += A[k]; }
    return w > 0 ? s / w : 0;
}
static void wav_write(const char *dir, const char *name, const double *y, uint32_t n)
{
    char path[512], nm[64];
    uint32_t i, k = 0;
    FILE *f;
    for (i = 0; name[i] && k < sizeof nm - 1u; i++)
        nm[k++] = (char)(name[i] == ' ' ? '_' : name[i] == '+' ? 'y' : name[i]);
    nm[k] = 0;
    snprintf(path, sizeof path, "%s/hive_%s.wav", dir, nm);
    f = fopen(path, "wb");
    if (!f) { printf("hive: cannot write %s\n", path); return; }
    {
        uint32_t data = n * 2u, v;
        uint16_t s;
        fwrite("RIFF", 1, 4, f); v = 36u + data; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
        v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
        v = FS; fwrite(&v, 4, 1, f); v = FS * 2u; fwrite(&v, 4, 1, f); s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
        for (i = 0; i < n; i++) {
            double x = y[i] * 2.0;                       /* (the part's level before the master: +6 dB to hear it) */
            int16_t q = (int16_t)(x > 1 ? 32767 : x < -1 ? -32767 : x * 32767);
            fwrite(&q, 2, 1, f);
        }
    }
    fclose(f);
    printf("hive: wrote %s\n", path);
}
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static double cpu_us(uint32_t engine, uint32_t preset, uint8_t note)   /* host microseconds a block, one part, one key */
{
    static int32_t blk[CTL];
    uint32_t b, nb = getenv("HIVE_VG") ? 1000u : 20000u;
    double t0;
    setup(engine, preset);
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < 200u; b++)
        track_render(&trk[0], blk, CTL);
#ifdef HIVE_HAS_VG
    CALLGRIND_ZERO_STATS;                                /* (valgrind --tool=callgrind --instr-atstart=no: a section a call,
                                                          * its instructions / 1000 a block) */
    CALLGRIND_START_INSTRUMENTATION;
#endif
    t0 = now_s();
    for (b = 0; b < nb; b++)
        track_render(&trk[0], blk, CTL);
#ifdef HIVE_HAS_VG
    {
        char nm[48];
        snprintf(nm, sizeof nm, "%s.%u.%u", ENGINES[engine]->name, preset, note);
        CALLGRIND_DUMP_STATS_AT(nm);
        CALLGRIND_STOP_INSTRUMENTATION;
    }
#endif
    return (now_s() - t0) * 1e6 / nb;
}

int main(void)
{
    const char *dir = getenv("HIVE_WAV_DIR");
    uint32_t pr, n = ENG_HIVE.npresets;
    static const uint8_t C3[1] = {48}, DYAD[2] = {48, 55};
    ok(ENGINES[ENGI_HIVE] == &ENG_HIVE && eng_ok(ENGI_HIVE) && eng_idx(ENGI_HIVE) == ENGI_HIVE && n >= 6u,
       "HIVE is engine %u, selectable, %u presets", ENGI_HIVE, n);
    {
        uint32_t k, seen = 0;
        for (k = 0; k < NENG_SHOWN; k++) seen |= eng_vis(k) == ENGI_HIVE;
        ok(seen, "HIVE in the engines' order (PRESETS, EDIT, the editor)");
    }
    for (pr = 0; pr < n; pr++) {
        const preset_t *p = &ENG_HIVE.presets[pr];
        double r0, r1, pk;
        setup(ENGI_HIVE, pr);
        play(C3, 1, 3u * FS, ya, NS);
        pk = peak(ya, NS);
        r0 = rms(ya + FS / 4u, FS);                      /* held: 0.25 .. 1.25 s */
        r1 = rms(ya + 4u * FS, FS);                      /* let go 1 s before */
        ok(finite_all(ya, NS) && pk < 0.99 && r0 > 0.01, "%-15s sounds: rms %.3f, peak %.3f, bounded", p->name, r0, pk);
        if (trk[0].p[P_HVHOLD])
            ok(r1 > 0.3 * r0, "%-15s HOLD: the drone on after the key (rms %.3f of %.3f held)", p->name, r1, r0);
        else
            ok(r1 < 0.5 * r0, "%-15s no HOLD: let go, it fades (rms %.3f of %.3f held)", p->name, r1, r0);
        if (dir)
            wav_write(dir, p->name, ya, NS);
    }
    {   /* FEEDBK low: a pluck that dies under the held key; high: on by itself (HOLD off, the key held) */
        double e0, e1, s0, s1;
        setup(ENGI_HIVE, 3);                             /* (KOTO) */
        trk[0].p[P_E0] = 20; trk[0].p[P_HVHOLD] = 0;
        play(C3, 1, NS, ya, 3u * FS);
        e0 = rms(ya + FS / 20u, FS / 10u);
        e1 = rms(ya + 2u * FS, FS / 2u);
        ok(e0 > 0.01 && e1 < 0.05 * e0, "FEEDBK 20: a pluck that dies, the key held (rms %.4f -> %.5f)", e0, e1);
        setup(ENGI_HIVE, 3);
        trk[0].p[P_E0] = 122; trk[0].p[P_HVHOLD] = 0;
        play(C3, 1, NS, ya, 3u * FS);
        s0 = rms(ya + FS / 20u, FS / 10u);
        s1 = rms(ya + 2u * FS, FS / 2u);
        ok(s1 > 0.3 * s0 && peak(ya, 3u * FS) < 0.99, "FEEDBK 122: self-oscillating, on after the touch (rms %.3f -> %.3f)", s0, s1);
    }
    {   /* CROSS moves the spectrum */
        double sim, c0, c1;
        setup(ENGI_HIVE, 0);
        trk[0].p[P_E0] = 118; trk[0].p[P_E1] = 0;
        play(C3, 1, NS, ya, NS);
        setup(ENGI_HIVE, 0);
        trk[0].p[P_E0] = 118; trk[0].p[P_E1] = 127;
        play(C3, 1, NS, yb, NS);
        sim = spec_sim(ya + 2u * FS, yb + 2u * FS, 2u * FS);
        c0 = centroid(ya + 2u * FS, 2u * FS);
        c1 = centroid(yb + 2u * FS, 2u * FS);
        ok(sim < 0.9 && peak(yb, NS) < 0.99, "CROSS 0 -> 127 changes the spectrum (similarity %.3f, centroid band %.1f -> %.1f)", sim, c0, c1);
    }
    {   /* PAIR / two keys: the second key the second pair's root */
        double g1, g2;
        setup(ENGI_HIVE, 0);
        play(C3, 1, NS, ya, 3u * FS);
        setup(ENGI_HIVE, 0);
        play(DYAD, 2, NS, yb, 3u * FS);
        g1 = goertzel(ya + 2u * FS, FS, 196.0) / goertzel(ya + 2u * FS, FS, 130.8);   /* G3 (the second key, 55) over C3 */
        g2 = goertzel(yb + 2u * FS, FS, 196.0) / goertzel(yb + 2u * FS, FS, 130.8);
        ok(g2 > 2.0 * g1, "two keys: the second pair on the second (G3 over C3 %.2f -> %.2f)", g1, g2);
    }
    {   /* (hive2) ECHO: the strings' level kept, a repeat at the delay, no runaway */
#define KOTO0() (trk[0].p[P_E3] = trk[0].p[P_E4] = trk[0].p[P_E6] = 0)   /* (the bare strings) */
        static const uint8_t TM[5] = {1, 4, 6, 8, 12};
        static const double TD[5] = {0.06, 0.28, 0.65, 0.25, 1.0};   /* (1/8 and 1/2 at 120 BPM) */
        uint32_t k, bad = 0;
        double worst = 0, wl = 99;
        for (k = 0; k < 5u; k++) {                       /* a short pluck (KOTO, FEEDBK 20): before the repeat, the same */
            double d0, d1, r0, r1, tap;
            setup(ENGI_HIVE, 3); trk[0].p[P_E0] = 20; trk[0].p[P_E2] = 0; trk[0].p[P_HVTIME] = TM[k]; KOTO0();
            play(C3, 1, NS, ya, 2u * FS);
            setup(ENGI_HIVE, 3); trk[0].p[P_E0] = 20; trk[0].p[P_E2] = 127; trk[0].p[P_HVTIME] = TM[k]; KOTO0();
            play(C3, 1, NS, yb, 2u * FS);
            d0 = rms(ya, (uint32_t)(TD[k] * 0.9 * FS)); d1 = rms(yb, (uint32_t)(TD[k] * 0.9 * FS));
            r0 = rms(ya + (uint32_t)(TD[k] * FS), FS * 3u / 100u); r1 = rms(yb + (uint32_t)(TD[k] * FS), FS * 3u / 100u);
            tap = 20 * log10(sqrt(fmax(r1 * r1 - r0 * r0, 1e-12)) / rms(ya, FS * 3u / 100u));
            printf("hive: ECHO 127 TIME %-5s: dry %+.2f dB before the repeat, the repeat %.1f dB under the touch\n",
                   N_HVTIME[TM[k]], 20 * log10(d1 / d0), tap);
            bad |= fabs(20 * log10(d1 / d0)) > 1.0 || tap < -9.0;
            if (tap < wl) wl = tap;
        }
        ok(!bad, "ECHO 127: the dry pluck within 1 dB before its repeat, the repeat at TIME (60MS .. 1/2) %.1f dB at worst", wl);
        bad = 0;
        for (k = 0; k < ENG_HIVE.npresets; k++) {        /* each preset's own: the level with ECHO 127 not under ECHO 0's */
            double a, b;
            setup(ENGI_HIVE, k); trk[0].p[P_E2] = 0;
            play(C3, 1, NS, ya, 3u * FS);
            setup(ENGI_HIVE, k); trk[0].p[P_E2] = 127;
            play(C3, 1, NS, yb, 3u * FS);
            a = rms(ya + FS * 3u / 2u, FS * 3u / 2u); b = rms(yb + FS * 3u / 2u, FS * 3u / 2u);
            if (20 * log10(b / a) < -1.0) bad++;
            if (20 * log10(b / a) > worst) worst = 20 * log10(b / a);
        }
        ok(!bad && worst < 15.0, "ECHO 0 -> 127 on every preset: never quieter than 1 dB, at most %+.1f dB (no wall)", worst);
        setup(ENGI_HIVE, 1); trk[0].p[P_E0] = 70; trk[0].p[P_E2] = 127;   /* (BATIDO: was 0.59, the echo's loop saturated) */
        play(C3, 1, NS, ya, 3u * FS);
        ok(rms(ya + 2u * FS, FS) < 0.1, "FEEDBK 70 ECHO 127: the repeats die away, no loop through the strings (rms %.3f)",
           rms(ya + 2u * FS, FS));
    }
    {   /* (hive2) STIFF BODY POS DRIVE: the spectrum moved, the level kept; the matrix's LFO on them smooth */
        static const uint8_t EK[4] = {3, 4, 6, 7}, PR[4] = {3, 3, 3, 0}, EV[4] = {110, 90, 127, 110};
        static const char *const EN[4] = {"STIFF", "BODY", "POS", "DRIVE"};
        uint32_t k, b;
        for (k = 0; k < 4u; k++) {
            double A[96], Bs[96], dev = 0, lv;
            setup(ENGI_HIVE, PR[k]); trk[0].p[P_E2] = 0; trk[0].p[P_E0 + EK[k]] = 0;
            play(C3, 1, NS, ya, 3u * FS);
            setup(ENGI_HIVE, PR[k]); trk[0].p[P_E2] = 0; trk[0].p[P_E0 + EK[k]] = EV[k];
            play(C3, 1, NS, yb, 3u * FS);
            spectrum(ya + FS, 2u * FS, A);
            spectrum(yb + FS, 2u * FS, Bs);
            for (b = 0; b < 96u; b++)
                if (fmax(A[b], Bs[b]) > 1e-5 && fabs(10 * log10((A[b] + 1e-12) / (Bs[b] + 1e-12))) > dev)
                    dev = fabs(10 * log10((A[b] + 1e-12) / (Bs[b] + 1e-12)));
            lv = 20 * log10(rms(yb + FS / 4u, 2u * FS) / rms(ya + FS / 4u, 2u * FS));
            ok(dev > 6.0 && fabs(lv) < 7.0 && peak(yb, 3u * FS) < 0.99, "%-6s %3u: the spectrum moved (a band %.0f dB), the level %+.1f dB",
               EN[k], EV[k], dev, lv);
        }
        for (k = 0; k < 6u; k++) {                       /* the matrix's LFO (fast, deep) on each: its roughest step */
            static const uint8_t MK[6] = {3, 4, 6, 7, 1, 5};
            static int32_t blk[CTL];
            double d2[2];
            uint32_t r, got, i;
            for (r = 0; r < 2u; r++) {
                track_t *t = &trk[0];
                setup(ENGI_HIVE, 0); t->p[P_E2] = 0; t->p[P_E0 + MK[k]] = 64;
                if (r) { t->p[P_M1SRC] = PS_LFO; t->p[P_M1DST] = PD_E(MK[k]); t->p[P_M1AMT] = 60; t->p[P_LRATE] = 60; }   /* (~10 Hz, 0 .. 127) */
                trk_note_on(t, 48, 100);
                d2[r] = 0;
                for (got = 0; got < 2u * FS; got += CTL) {
                    mod_begin(t);
                    track_render(t, blk, CTL);
                    mod_end(t);
                    for (i = 0; i < CTL; i++) ya[got + i] = blk[i] / 32768.0;
                }
                for (i = FS / 2u; i < 2u * FS; i++) {   /* (the second difference: a step, a click) */
                    double d = fabs(ya[i] - 2 * ya[i - 1] + ya[i - 2]);
                    if (d > d2[r]) d2[r] = d;
                }
            }
            ok(d2[1] < 2.5 * d2[0] + 1e-4, "the matrix's LFO on %-6s: smooth (its roughest step %.4f, still %.4f)",
               ENG_HIVE.edit[MK[k]].label, d2[1], d2[0]);
        }
    }
    {   /* low notes: 11 kHz, octaves up where nothing fits; high: bounded */
        static const uint8_t LO[1] = {24}, HI[1] = {96};
        hv_st_t *s;
        setup(ENGI_HIVE, 4);                             /* (ABISMO: SUB, the second pair an octave down) */
        play(LO, 1, NS, ya, 2u * FS);
        s = hv_of(0);
        ok(s->sh == 2u && finite_all(ya, 2u * FS) && peak(ya, 2u * FS) < 0.99 && rms(ya + FS, FS) > 0.005,
           "C1 on SUB: the strings at 11 kHz (sh %u), bounded, sounding (rms %.3f)", s->sh, rms(ya + FS, FS));
        setup(ENGI_HIVE, 0);
        play(HI, 1, NS, ya, 2u * FS);
        ok(hv_of(0)->sh == 1u && peak(ya, 2u * FS) < 0.99 && rms(ya + FS, FS) > 0.005, "C7: at 22 kHz, bounded (rms %.3f)", rms(ya + FS, FS));
    }
    {   /* the rows back to their owners: the voice ended, the engine changed */
        static int32_t blk[CTL];
        uint32_t k, dirty = 0, b;
        const uint8_t *kd = (const uint8_t *)drum_kit[0];
        setup(ENGI_HIVE, 3);                             /* (KOTO: no HOLD) */
        trk[0].p[P_REL] = 0;
        play(C3, 1, FS / 4u, ya, FS);
        for (b = 0; b < 4u * FS / CTL; b++) track_render(&trk[0], blk, CTL);
        for (k = 0; k < sizeof drum_kit[0]; k++) dirty |= kd[k];
        for (k = 0; k < COMB_LEN; k++) dirty |= (uint16_t)comb_buf[0][k];
        ok(!dirty && !trk[0].v[0].active, "the voice ended: the kit row and the COMB line cleared");
        setup(ENGI_HIVE, 0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 100u; b++) { events_block(CTL); track_render(&trk[0], blk, CTL); }
        trk[0].eng_req = ENGI_DRUM;
        for (b = 0; b < 40u; b++) { events_block(CTL); track_render(&trk[0], blk, CTL); }
        dirty = 0;
        for (k = 0; k < sizeof drum_kit[0]; k++) dirty |= kd[k] && k >= 4u;
        for (k = 0; k < COMB_LEN; k++) dirty |= (uint16_t)comb_buf[0][k];
        ok(trk[0].engine == ENGI_DRUM && !dirty, "HIVE -> DRUM with the drone on: the rows clear when it switched");
    }
    {   /* HOLD: STOP (all off) lets the drone go */
        static int32_t blk[CTL];
        uint32_t b;
        setup(ENGI_HIVE, 0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 50u; b++) track_render(&trk[0], blk, CTL);
        trk_note_off(&trk[0], 48);
        for (b = 0; b < 50u; b++) track_render(&trk[0], blk, CTL);
        ok(trk[0].v[0].active && trk[0].v[0].gate, "HOLD: the key let go, the drone held (gate on)");
        trk[0].p[P_HVHOLD] = 0;
        for (b = 0; b < 2000u; b++) track_render(&trk[0], blk, CTL);
        ok(!trk[0].v[0].active, "HOLD turned OFF with no key down: the drone lets go, ends");
        setup(ENGI_HIVE, 0);                             /* STOP and MUTE let a held drone go, as DRONE's */
        song.playing = 1;
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 50u; b++) track_render(&trk[0], blk, CTL);
        trk_note_off(&trk[0], 48);
        song.playing = 0;
        for (b = 0; b < 2000u; b++) track_render(&trk[0], blk, CTL);
        ok(!trk[0].v[0].active, "HOLD: STOP lets the drone go");
        setup(ENGI_HIVE, 0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 50u; b++) track_render(&trk[0], blk, CTL);
        trk_note_off(&trk[0], 48);
        trk[0].p[P_MUTE] = 1;
        for (b = 0; b < 2000u; b++) track_render(&trk[0], blk, CTL);
        ok(!trk[0].v[0].active, "HOLD: MUTE lets the drone go");
        setup(ENGI_HIVE, 0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 50u; b++) track_render(&trk[0], blk, CTL);
        trk_note_off(&trk[0], 48);
        trk_note_on(&trk[0], 48, 100);                   /* (the note alone again) */
        trk_note_off(&trk[0], 48);
        for (b = 0; b < 2000u; b++) track_render(&trk[0], blk, CTL);
        ok(!trk[0].v[0].active, "HOLD: the droning note struck alone again lets it go");
    }
    {
        double h1 = cpu_us(ENGI_HIVE, 0, 48), h2 = cpu_us(ENGI_HIVE, 4, 24), h3 = cpu_us(ENGI_HIVE, 5, 96);
        double w1 = cpu_us(ENGI_WAVE, 0, 48), w2 = cpu_us(ENGI_WAVE, 2, 48);
        printf("hive: host CPU a block of %u, one key: HIVE DRONE QUINTA C3 %.2f us, ABISMO C1 %.2f us, AULLIDO C7 (22 kHz) %.2f us;"
               " WAVE INIT %.2f us, SAW PAD %.2f us\n", CTL, h1, h2, h3, w1, w2);
        ok(h1 < 1.5 * w2, "HIVE's block no dearer than WAVE SAW PAD's on the host, within the timing's noise (%.2f / %.2f us)", h1, w2);
    }
    printf(fails ? "hive: %d FAILED\n" : "hive: all ok\n", fails);
    return fails != 0;
}
