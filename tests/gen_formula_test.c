/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE) the equations' generator (fvm_gen.c) and MATH's USER parts (engines.c math_poll_ws, eng_lofi.c):
 *   every seed     0..191 at the three CAOS bands: a formula of 63 characters at most that compiles and the judge lets
 *                  through (a few may fall back to their last try), the same text every time, most of them their own
 *   tonal          the tonal band mostly floatbeats (FIX), the wild one bytebeats (INT)
 *   USER           a MATH part on USER sounds its seed's formula, bounded; another seed another sound; the part's own
 *                  text (lyr_txt) played instead, as written
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/gen_formula_test tests/gen_formula_test.c -lm */
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
    printf("genf: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

static gq_work_t W;
#define NS (FS / 2u)
static double ya[NS], yb[NS];

static void user_part(uint32_t seed, uint32_t caos, const char *text, double *y)
{
    static int32_t blk[CTL];
    track_t *t = &trk[0];
    uint32_t got = 0, k;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(t, ENGI_LOFI, 0);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_E1] = RW_USER;
    t->p[P_E2] = (int16_t)seed;
    t->p[P_CAOS] = (int16_t)caos;
    memset(lyr_txt[0], 0, sizeof lyr_txt[0]);
    for (k = 0; text && text[k] && k < 63u; k++)
        lyr_txt[0][k] = text[k];
    math_poll_ws(&W);
    trk_note_on(t, 60, 100);
    while (got < NS) {
        track_render(t, blk, CTL);
        for (k = 0; k < CTL && got < NS; k++)
            y[got++] = blk[k] / 32768.0;
    }
}
static double rms(const double *a, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++) s += a[i] * a[i];
    return sqrt(s / n);
}
static double peak(const double *a, uint32_t n)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < n; i++) if (fabs(a[i]) > p) p = fabs(a[i]);
    return p;
}

int main(int argc, char **argv)
{
    static const uint8_t BAND[3] = {20, 64, 106};
    static char txt[3][192][64];
    uint32_t b, s;
    (void)argc;
    (void)argv;
    for (b = 0; b < 3u; b++) {
        uint32_t passed = 0, longest = 0, fix = 0, distinct = 0, again = 1;
        for (s = 0; s < 192u; s++) {
            char t2[64];
            uint32_t k = fvm_invent(&W, txt[b][s], s, BAND[b]), n = (uint32_t)strlen(txt[b][s]);
            passed += k < 12u;
            fix += W.pg.p.ty == FT_FIX;
            if (n > longest) longest = n;
            fvm_invent(&W, t2, s, BAND[b]);
            again &= !strcmp(t2, txt[b][s]);
        }
        for (s = 0; s < 192u; s++) {
            uint32_t j;
            for (j = 0; j < s && strcmp(txt[b][j], txt[b][s]); j++) {}
            distinct += j == s;
        }
        printf("genf: CAOS %3u e.g. %s | %s | %s\n", BAND[b], txt[b][0], txt[b][1], txt[b][2]);
        ok(passed >= 182u && longest <= 63u, "CAOS %3u: %u of 192 seeds judged playable at once, the longest %u characters",
           BAND[b], passed, longest);
        ok(again && distinct >= 150u, "CAOS %3u: the same text from the same seed; %u of 192 their own", BAND[b], distinct);
        if (b == 0u)
            ok(fix * 2u > 192u, "CAOS %3u (tonal): mostly floatbeats in tune (%u of 192 FIX)", BAND[b], fix);
        if (b == 2u)
            ok(fix == 0u, "CAOS %3u (wild): bytebeat trees (%u FIX)", BAND[b], fix);
    }
    user_part(7, 0, 0, ya);
    ok(rms(ya, NS) > 0.005 && peak(ya, NS) <= 1.0, "USER: seed 7 sounds (rms %.3f), bounded (peak %.3f): %s", rms(ya, NS),
       peak(ya, NS), math_txt[0]);
    user_part(8, 0, 0, yb);
    ok(memcmp(ya, yb, sizeof ya) != 0, "USER: seed 8 another sound: %s", math_txt[0]);
    user_part(7, 0, "t*(42&t>>10)", ya);
    ok(!strcmp(math_txt[0], "t*(42&t>>10)") && math_ty[0] == FT_INT && rms(ya, NS) > 0.005,
       "USER: the part's own text played as written (%s, rms %.3f)", math_txt[0], rms(ya, NS));
    user_part(7, 120, 0, ya);
    ok(rms(ya, NS) > 0.001 && peak(ya, NS) <= 1.0, "USER: CAOS 120 a wild one, bounded (peak %.3f): %s", peak(ya, NS), math_txt[0]);
    {   /* (phase 6) cross-mod: track 2 reads track 1's output (O1), a block late, through the mix; each pass in a fresh
         * child (fork, as tests/regress.c: the FX's and the master's state of a pass would leak into the next) */
        enum { NB = 150u };
        static int32_t mm[4][NB * 2u * CTL];
        static const char *const TX[2] = {"sin(w)", "sin(w+O1*8)"};
        uint32_t pass;
        for (pass = 0; pass < 4u; pass++) {
            int fd[2];
            pid_t pid;
            if (pipe(fd)) break;
            pid = fork();
            if (!pid) {
                static int32_t m[NB * 2u * CTL];
                uint32_t k, b, src = pass < 2u;      /* (passes 0 1: track 1 sounds; 2 3: silent) */
                size_t off = 0;
                close(fd[0]);
                memset(trk, 0, sizeof trk);
                host_tracks_init();
                host_preset(&trk[0], ENGI_WAVE, 0);  /* (track 1: a sine) */
                host_preset(&trk[1], ENGI_LOFI, 0);
                for (k = 0; k < NTRK; k++) trk[k].p[P_DIST] = trk[k].p[P_CHOR] = trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
                trk[1].p[P_E1] = RW_USER;
                memset(lyr_txt[1], 0, sizeof lyr_txt[1]);
                for (k = 0; TX[pass & 1u][k]; k++) lyr_txt[1][k] = TX[pass & 1u][k];
                math_poll_ws(&W);
                if (src) trk_note_on(&trk[0], 48, 100);
                trk_note_on(&trk[1], 60, 100);
                for (b = 0; b < NB; b++)
                    mix_block(m + b * 2u * CTL, CTL);
                while (off < sizeof m) {
                    ssize_t w = write(fd[1], (char *)m + off, sizeof m - off);
                    if (w <= 0) break;
                    off += (size_t)w;
                }
                _exit(0);
            }
            close(fd[1]);
            {
                size_t off = 0;
                while (off < sizeof mm[pass]) {
                    ssize_t r = read(fd[0], (char *)mm[pass] + off, sizeof mm[pass] - off);
                    if (r <= 0) break;
                    off += (size_t)r;
                }
            }
            close(fd[0]);
            waitpid(pid, 0, 0);
        }
        ok(memcmp(mm[0], mm[1], sizeof mm[0]) != 0, "cross-mod: track 2's sin(w+O1*8) moves with track 1 sounding");
        ok(!memcmp(mm[2], mm[3], sizeof mm[2]), "cross-mod: track 1 silent, O1 is 0: sin(w+O1*8) bit for bit sin(w)");
    }
    printf(fails ? "genf: %d FAILED\n" : "genf: all ok\n", fails);
    return fails != 0;
}
