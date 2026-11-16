/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE) LIFE as the formulas talking (life.c life_talk, life_brk): two parts, track 1 a WAVE, track 2 a MATH
 * FLOAT (and a BYTE), through the mix, each render in a fresh child (the FX's and the master's state would leak):
 *   TALK    track 2 sounds otherwise with track 1 sounding than alone (it hears it); TALK 0 BREAK 0: the same
 *   BREAK   with track 1 sounding, BREAK up breaks track 2 (another sound than TALK alone)
 *   LIFE 0  nothing of it: track 2 the same, track 1 sounding or not
 *   every render bounded
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/life_byte_test tests/life_byte_test.c -lm */
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
    printf("lifeb: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}
enum { NB = 160u, NS = NB * 2u * CTL };
/* track 2 a MATH (preset pr), track 1 a WAVE sounding or not; LIFE amt, TALK, BREAK, WEB 100 */
static void render(uint32_t pr, int src, int32_t amt, int32_t talk, int32_t brk, int32_t *m)
{
    int fd[2];
    pid_t pid;
    if (pipe(fd)) return;
    pid = fork();
    if (!pid) {
        static int32_t o[NS];
        uint32_t k, b;
        size_t off = 0;
        close(fd[0]);
        memset(trk, 0, sizeof trk);
        host_tracks_init();
        life_reset();
        host_preset(&trk[0], ENGI_WAVE, 1);              /* (FM KEYS) */
        host_preset(&trk[1], ENGI_LOFI, pr);
        for (k = 0; k < NTRK; k++) trk[k].p[P_DIST] = trk[k].p[P_CHOR] = trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
        life_v[LIFE_AMT] = (int16_t)amt;
        life_v[LIFE_FILT] = (int16_t)talk;
        life_v[LIFE_PIT] = (int16_t)brk;
        life_v[LIFE_FX] = 100;
        if (src) trk_note_on(&trk[0], 48, 110);
        trk_note_on(&trk[1], 60, 100);
        for (b = 0; b < NB; b++)
            mix_block(o + b * 2u * CTL, CTL);
        while (off < sizeof o) {
            ssize_t w = write(fd[1], (char *)o + off, sizeof o - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        _exit(0);
    }
    close(fd[1]);
    {
        size_t off = 0;
        while (off < NS * sizeof(int32_t)) {
            ssize_t r = read(fd[0], (char *)m + off, NS * sizeof(int32_t) - off);
            if (r <= 0) break;
            off += (size_t)r;
        }
    }
    close(fd[0]);
    waitpid(pid, 0, 0);
}
/* track 2 alone in the mix: the render with track 1 sounding, minus track 1's own (rendered by itself) */
static int32_t a[NS], b[NS], c[NS], d[NS], one[NS];
static double peak(const int32_t *x) { double p = 0; uint32_t i; for (i = 0; i < NS; i++) if (fabs(x[i] / 32768.0) > p) p = fabs(x[i] / 32768.0); return p; }
static double corr(const int32_t *x, const int32_t *y)
{
    double xy = 0, xx = 0, yy = 0;
    uint32_t i;
    for (i = 0; i < NS; i++) xy += (double)x[i] * y[i], xx += (double)x[i] * x[i], yy += (double)y[i] * y[i];
    return xx > 0 && yy > 0 ? xy / sqrt(xx * yy) : 0;
}
int main(int argc, char **argv)
{
    uint32_t pr, i;
    (void)argc;
    (void)argv;
    for (pr = 3; pr <= 3u; pr++) {                       /* (MATH KEYS: a FLOAT) */
        render(pr, 1, 100, 100, 0, a);                   /* TALK, track 1 sounding */
        render(pr, 0, 100, 100, 0, b);                   /* .. alone */
        render(pr, 1, 100, 0, 0, c);                     /* TALK 0 BREAK 0, track 1 sounding */
        render(pr, 0, 100, 0, 0, d);                     /* .. alone */
        /* track 1's own part out: the same WAVE rendered with nothing else is not available bit for bit (the mix's
         * dynamics): compare the sums directly, the differences tell */
        ok(memcmp(a, c, sizeof a) != 0 && corr(a, c) < 0.98, "TALK 100: the mix with track 1 sounding moves (track 2 hears it: %.3f)", corr(a, c));
        ok(!memcmp(b, d, sizeof b), "track 1 silent: TALK changes nothing (nothing to hear)");
        render(pr, 1, 100, 0, 100, one);                 /* BREAK alone */
        ok(memcmp(one, c, sizeof one) != 0 && corr(one, c) < 0.999, "BREAK 100: track 2 broken while track 1 sounds (%.3f)", corr(one, c));
        render(pr, 1, 0, 100, 100, a);                   /* LIFE 0 */
        ok(!memcmp(a, c, sizeof a), "LIFE 0: TALK and BREAK do nothing, bit for bit");
        ok(peak(one) <= 1.0 && peak(c) <= 1.0, "bounded (peaks %.3f %.3f)", peak(one), peak(c));
    }
    render(0, 1, 100, 100, 100, a);                      /* a BYTE part: hears and breaks too */
    render(0, 1, 100, 0, 0, b);
    ok(memcmp(a, b, sizeof a) != 0 && peak(a) <= 1.0, "BYTE: TALK and BREAK move it, bounded (peak %.3f)", peak(a));
    (void)i;
    printf(fails ? "lifeb: %d FAILED\n" : "lifeb: all ok\n", fails);
    return fails != 0;
}
