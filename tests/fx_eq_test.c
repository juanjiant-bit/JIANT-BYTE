/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT BYTE, phase 7) the formula effects: DIST's TYPE 6.. (fx.c DIST_EQ, dist_eq), through the mix, each render in a
 * fresh child (the FX's and the master's state of one would leak into the next):
 *   every formula   compiles in 20 instructions, a FIX result
 *   DIST 0          out of the chain: the part bit for bit dry, whatever the TYPE
 *   each TYPE       at DIST 100 another sound, bounded, the same twice
 *   LPF             TONE down: a saw darker (its upper half's energy down)
 *   cc -O2 -Ibuild/gen -Ifirmware/src -o build/host/fx_eq_test tests/fx_eq_test.c -lm */
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
    printf("fxeq: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}

enum { NB = 120u, NS = NB * 2u * CTL };
/* track 1 a WAVE (preset pi: 0 SINE, A 4 a saw), DIST type / amount / tone, a note; the mix into m (a fresh child) */
static void render(uint32_t pi, int32_t wavef, int32_t type, int32_t dist, int32_t tone, int32_t *m)
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
        host_preset(&trk[0], ENGI_WAVE, pi);
        for (k = 0; k < NTRK; k++) trk[k].p[P_CHOR] = trk[k].p[P_DLY] = trk[k].p[P_REV] = 0;
        if (wavef >= 0) trk[0].p[P_E0] = (int16_t)wavef;
        trk[0].p[P_DTYPE] = (int16_t)type;
        trk[0].p[P_DIST] = (int16_t)dist;
        trk[0].p[P_DTONE] = (int16_t)tone;
        trk_note_on(&trk[0], 57, 100);
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
static double peak(const int32_t *a)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < NS; i++) if (fabs(a[i] / 32768.0) > p) p = fabs(a[i] / 32768.0);
    return p;
}
static double hf(const int32_t *a)                   /* the energy of its differences (the upper half, roughly) */
{
    double s = 0;
    uint32_t i;
    for (i = 2u; i < NS; i += 2u) { double d = (double)a[i] - a[i - 2u]; s += d * d; }
    return s;
}

int main(int argc, char **argv)
{
    static int32_t dry[NS], a[NS], b[NS];
    static fvm_cc_t cc;
    static fvm_prog_t pg;
    static fvm_ins_t pi[20];
    uint32_t f, bad = 0;
    (void)argc;
    (void)argv;
    fvm_prog_init(&pg, pi, 20u);
    for (f = 0; f < DIST_NEQ; f++) {
        fvm_err_t er;
        if (fvm_compile(&cc, &pg, DIST_EQ[f], (uint32_t)strlen(DIST_EQ[f]), &er) || pg.ty != FT_FIX) {
            printf("fxeq: %s: error %u at %u\n", DIST_EQ[f], er.code, er.pos);
            bad++;
        }
    }
    ok(!bad, "the %u effects compile in 20 instructions, FIX", (unsigned)DIST_NEQ);
    render(0, -1, 0, 0, 0, dry);
    bad = 0;
    for (f = 0; f < DIST_NEQ; f++) {
        render(0, -1, (int32_t)(DT_EQ0 + f), 0, 0, a);
        bad += memcmp(a, dry, sizeof a) != 0;
    }
    ok(!bad, "DIST 0: out of the chain, the part bit for bit dry with every formula TYPE (%u differ)", bad);
    bad = 0;
    for (f = 0; f < DIST_NEQ; f++) {
        int32_t tone = f == 6u || f == 7u ? -40 : 0;  /* (LPF, LFO.F: a cut that shows) */
        render(0, -1, (int32_t)(DT_EQ0 + f), 100, tone, a);
        render(0, -1, (int32_t)(DT_EQ0 + f), 100, tone, b);
        if (!memcmp(a, dry, sizeof a) || memcmp(a, b, sizeof a) || peak(a) > 1.0) {
            printf("fxeq: %s: same as dry %d, twice the same %d, peak %.3f\n", DIST_EQ[f], !memcmp(a, dry, sizeof a),
                   !memcmp(a, b, sizeof a), peak(a));
            bad++;
        }
    }
    ok(!bad, "every TYPE at DIST 100: another sound, the same twice, bounded");
    {
        double h0, h1;
        render(0, 4, 0, 0, 0, a);                    /* (A 4: SAWSQ at POS 0, a saw) */
        render(0, 4, (int32_t)(DT_EQ0 + 6u), 127, -60, b);
        h0 = hf(a);
        h1 = hf(b);
        ok(h1 < h0 * 0.5, "LPF with TONE down: a saw darker (its upper energy x%.2f)", h1 / h0);
    }
    printf(fails ? "fxeq: %d FAILED\n" : "fxeq: all ok\n", fails);
    return fails != 0;
}
