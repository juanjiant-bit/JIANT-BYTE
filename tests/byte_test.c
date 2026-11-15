/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 2) MATH through the FVM, its CAOS and its KEY (eng_lofi.c), rendered as the device does
 * (track_render), the song's position advanced by hand (KEY ALTURA / FIJO read it: lofi_sync):
 *   CAOS 0     the formula as written (the golden renders, tests/regress.c, cover it bit for bit)
 *   CAOS up    a different sound at 32 64 96 127, every one bounded (no sample past full scale), the same twice
 *   KEY FIJO   C4 and G4 the same sound (the keys only start it): BYTE and FLOAT
 *   KEY ALTURA the rhythm the song's: C4 and G4's envelopes alike, more than LIBRE's; FLOAT's pitch the key's
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/byte_test tests/byte_test.c -lm && build/host/byte_test */
#include <stdarg.h>
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
    printf("byte: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

#define NS (FS * 2u)                                 /* two seconds */
static double ya[NS], yb[NS];

/* a MATH preset on track 0, dry, CAOS and KEY set; a note held for NS samples; the song's position running at 120 */
static void render(uint32_t preset, uint32_t note, int32_t caos, int32_t key, double *y)
{
    static int32_t blk[CTL];
    track_t *t = &trk[0];
    uint32_t got = 0, bl = (uint32_t)FS * 60u / 120u;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(t, ENGI_LOFI, preset);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_CAOS] = (int16_t)caos;
    t->p[P_KEY] = (int16_t)key;
    song.g[G_BPM] = 120;
    beat_pos = beat_n = bar_n = 0;
    trk_note_on(t, note, 100);
    while (got < NS) {
        uint32_t k;
        track_render(t, blk, CTL);
        for (k = 0; k < CTL && got < NS; k++)
            y[got++] = blk[k] / 32768.0;
        beat_pos += CTL;                             /* (the sequencer's clock, by hand) */
        if (beat_pos >= bl) {
            beat_pos -= bl;
            if (++beat_n >= 4u) {
                beat_n = 0;
                bar_n++;
            }
        }
    }
}
static double corr(const double *a, const double *b, uint32_t n)
{
    double ab = 0, aa = 0, bb = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        ab += a[i] * b[i];
        aa += a[i] * a[i];
        bb += b[i] * b[i];
    }
    return aa > 0 && bb > 0 ? ab / sqrt(aa * bb) : 0;
}
static double peak(const double *a, uint32_t n)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        if (fabs(a[i]) > p) p = fabs(a[i]);
    return p;
}
static double rms(const double *a, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++) s += a[i] * a[i];
    return sqrt(s / n);
}
/* the RMS envelope in 10 ms frames, its correlation between two renders (the mean taken out) */
static double env_corr(const double *a, const double *b)
{
    enum { FR = FS / 100u, NF = NS / FR };
    static double ea[NF], eb[NF];
    double ma = 0, mb = 0;
    uint32_t f, i;
    for (f = 0; f < NF; f++) {
        double sa = 0, sb = 0;
        for (i = 0; i < FR; i++) {
            sa += a[f * FR + i] * a[f * FR + i];
            sb += b[f * FR + i] * b[f * FR + i];
        }
        ea[f] = sqrt(sa / FR);
        eb[f] = sqrt(sb / FR);
        ma += ea[f];
        mb += eb[f];
    }
    for (f = 0; f < NF; f++) {
        ea[f] -= ma / NF;
        eb[f] -= mb / NF;
    }
    return corr(ea, eb, NF);
}
/* zero crossings a second, upwards (a pitch's rough measure) */
static double zc(const double *a, uint32_t n)
{
    uint32_t i, c = 0;
    for (i = 1; i < n; i++)
        c += a[i - 1] < 0 && a[i] >= 0;
    return c * (double)FS / n;
}

int main(int argc, char **argv)
{
    static double y0[NS];
    int32_t c;
    (void)argc;
    (void)argv;
    /* BYTE (preset 0, BYTEBEAT) */
    render(0, 60, 0, 0, y0);
    render(0, 60, 0, 0, ya);
    ok(!memcmp(y0, ya, sizeof ya), "BYTE CAOS 0: the same render twice (rms %.3f)", rms(y0, NS));
    for (c = 32; c <= 127; c += c == 96 ? 31 : 32) {
        double cr, pk;
        render(0, 60, c, 0, ya);
        render(0, 60, c, 0, yb);
        cr = corr(y0, ya, NS);
        pk = peak(ya, NS);
        ok(!memcmp(ya, yb, sizeof ya) && cr < 0.98 && pk <= 1.0 && rms(ya, NS) > 0.005,
           "BYTE CAOS %3d: another sound (correlation with CAOS 0 %.3f), bounded (peak %.3f), the same twice", c, cr, pk);
    }
    {
        double cl, cf, el, ea2;
        render(0, 60, 0, 16, ya);
        render(0, 67, 0, 16, yb);
        cf = corr(ya, yb, NS);
        ok(cf > 0.98, "BYTE KEY FIJO: C4 and G4 the same sound (correlation %.3f)", cf);
        render(0, 60, 0, 0, ya);
        render(0, 67, 0, 0, yb);
        cl = corr(ya, yb, NS);
        el = env_corr(ya, yb);
        ok(cl < 0.5, "BYTE KEY LIBRE: C4 and G4 apart (correlation %.3f), MATH as ever", cl);
        render(0, 60, 0, 5, ya);
        render(0, 67, 0, 5, yb);
        ea2 = env_corr(ya, yb);
        ok(ea2 > el + 0.1, "BYTE KEY ALT9: C4 and G4's rhythm alike (envelopes %.3f, LIBRE's %.3f)", ea2, el);
    }
    /* FLOAT (preset 3, MATH KEYS) */
    {
        double cf, z1, z2;
        render(3, 60, 0, 16, ya);
        render(3, 67, 0, 16, yb);
        cf = corr(ya, yb, NS);
        ok(cf > 0.98, "FLOAT KEY FIJO: C4 and G4 the same sound (correlation %.3f)", cf);
        render(3, 60, 0, 5, ya);
        render(3, 67, 0, 5, yb);
        z1 = zc(ya, NS);
        z2 = zc(yb, NS);
        ok(z2 > z1 * 1.3 && z2 < z1 * 1.7, "FLOAT KEY ALT9: the pitch the key's (%.0f / %.0f crossings a second: x%.2f)",
           z2, z1, z2 / z1);
        render(3, 60, 127, 0, ya);
        ok(peak(ya, NS) <= 1.0 && rms(ya, NS) > 0.005, "FLOAT CAOS 127: bounded (peak %.3f), sounding (rms %.3f)",
           peak(ya, NS), rms(ya, NS));
    }
    printf(fails ? "byte: %d FAILED\n" : "byte: all ok\n", fails);
    return fails != 0;
}
