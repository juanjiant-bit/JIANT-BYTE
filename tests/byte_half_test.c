/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT BYTE, byte-cpu-light) BYTE's engines at half the rate (voice.c voice_half) against the full one, under 4 kHz:
 * JIANT FM's voices_probe halfq (V10.7.56) for BYTE's engines. Well: about -19 dB; near 0 or over: the engine does not
 * follow the doubled increments (another pitch, another tempo).
 *   byte_half_test [LIM] [ENGINE]   (every engine with .half, or ENGINE forced to the generic path to try it) */
static unsigned vsoft_host = 0;
#define VSOFT vsoft_host
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/wait.h>
#include <unistd.h>
#define WAV_MAX (44100 * 8)
typedef struct { const char *name; uint8_t e, pr, nn, mono; uint8_t note[8]; uint16_t gate, gap; int16_t rel; } phrase_t;
static void force_half(uint32_t p)                      /* part p's voices at half the rate from their next block */
{
    uint32_t i;
    if (getenv("QUART")) vh_quart = 0xFF;               /* (V10.7.56: FM6 at a quarter instead) */
    for (i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && !VH_ST(vh_c[p * NVOICE + i]))
            vh_c[p * NVOICE + i] = VH_IN;
}

static uint32_t phrase_render(const phrase_t *ph, uint32_t mode, int32_t *buf, uint32_t cap)
{
    uint32_t b, k, n = 0, per = (ph->gate + ph->gap) * FS / 1000u / CTL, on = ph->gate * FS / 1000u / CTL;
    uint32_t total = per * ph->nn + FS * 2u / CTL;
    host_tracks_init();
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(vh_c, 0, sizeof vh_c);
    host_preset(&trk[0], ph->e, ph->pr);
    trk[0].p[P_VOICE] = ph->mono ? V_LEGATO : V_POLY;
    if (ph->rel >= 0) trk[0].p[P_REL] = ph->rel;
    for (b = 0; b < total && n + CTL <= cap; b++) {
        k = b / per;
        if (k < ph->nn && b % per == 0u) {
            if (ph->mono) trk_note_on(&trk[0], ph->note[k], 100);
            else trk_note_on(&trk[0], ph->note[k], 100), k && ph->gap < 300u ? trk_note_off(&trk[0], ph->note[k - 1u]) : (void)0;
        }
        if (k < ph->nn && b % per == on) trk_note_off(&trk[0], ph->note[k]);
        if (mode == 1u || (mode == 2u && (b / 256u) & 1u))
            force_half(0);
        else if (mode == 2u)
            for (k = 0; k < NVOICE; k++)                 /* back to the full rate */
                if (VH_ST(vh_c[k]) == VH_ON) vh_c[k] += VH_OUT - VH_ON;
                else if (VH_ST(vh_c[k]) != VH_OUT) vh_c[k] = VH_OFF;
        track_render(&trk[0], buf + n, CTL);
        n += CTL;
    }
    return n;
}

static uint32_t phrase_fork(const phrase_t *ph, uint32_t mode, int32_t *buf, uint32_t cap)
{
    int fd[2];
    uint32_t n = 0, got = 0;
    pid_t pid;
    if (pipe(fd)) return 0;
    pid = fork();
    if (!pid) {
        close(fd[0]);
        n = phrase_render(ph, mode, buf, cap);
        if (write(fd[1], &n, sizeof n) != sizeof n) _exit(1);
        while (got < n * sizeof *buf) {
            ssize_t w = write(fd[1], (char *)buf + got, n * sizeof *buf - got);
            if (w <= 0) _exit(1);
            got += (uint32_t)w;
        }
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &n, sizeof n) != sizeof n) n = 0;
    while (got < n * sizeof *buf) {
        ssize_t r = read(fd[0], (char *)buf + got, n * sizeof *buf - got);
        if (r <= 0) break;
        got += (uint32_t)r;
    }
    close(fd[0]);
    waitpid(pid, 0, 0);
    return got / sizeof *buf;
}

static void resample_ref(const int32_t *x, int32_t *y, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n; i += 2u) {
        y[i] = i ? (x[i - 1u] + x[i + 1u]) >> 1 : x[0];
        y[i + 1u] = x[i + 1u];
    }
}

static double rms_db(const int32_t *a, const int32_t *b, uint32_t n, int lp)   /* a - b against a, dB; lp: under ~4 kHz */
{
    double sa = 0, sd = 0, ya = 0, yb = 0, za = 0, zb = 0, k = 1.0 - exp(-2.0 * M_PI * 4000.0 / FS);
    uint32_t i;
    for (i = 0; i < n; i++) {
        double x = a[i], y = b ? b[i] : 0;
        if (lp) { ya += k * (x - ya); za += k * (ya - za); x = za; yb += k * (y - yb); zb += k * (yb - zb); y = zb; }
        sa += x * x; sd += (x - y) * (x - y);
    }
    return sa > 0 && sd > 0 ? 10.0 * log10(sd / sa) : -200.0;
}

int main(int argc, char **argv)
{
    static int32_t full[WAV_MAX], half[WAV_MAX], ref[WAV_MAX];
    double lim = argc > 1 ? atof(argv[1]) : -12.0;
    uint32_t e, pr, k, n, bad = 0;
    for (e = 0; e < NENGINES; e++) {
        engine_t *en = (engine_t *)ENGINES[e];
        double worst = -999, wref = -999;
        const char *wn = "";
        uint32_t seen;
        for (seen = 0; seen < e && ENGINES[seen] != en; seen++) {}
        if (seen < e) continue;                         /* (a retired number playing as another: once) */
        if (argc > 2 && !strcmp(argv[2], en->name) && !en->half) en->half = 1;   /* (try the generic path) */
        if (!en->half || e == ENGI_DRUM || !en->npresets) continue;
        for (pr = 0; pr < en->npresets; pr++) {
            for (k = 0; k < 2u; k++) {
                phrase_t ph = {en->presets[pr].name, (uint8_t)e, (uint8_t)pr, 4, (uint8_t)k, {48, 55, 60, 64}, 500, 200, -1};
                double d, r;
                if (k) { ph.note[0] = 60; ph.note[1] = 67; ph.note[2] = 72; ph.note[3] = 64; }
                n = phrase_fork(&ph, 0, full, FS * 4u);
                phrase_fork(&ph, 1, half, FS * 4u);
                resample_ref(full, ref, n);
                d = rms_db(full, half, n, 1);
                r = rms_db(ref, half, n, 1);
                if (r > wref) wref = r;
                if (d > worst) { worst = d; wn = en->presets[pr].name; }
                if (d > lim) { printf("  %-8s %-14s %s half vs full under 4 kHz %6.1f dB (vs the interpolation %6.1f)\n", en->name, en->presets[pr].name, k ? "line " : "chord", d, r); bad++; }
            }
        }
        printf("%-8s worst %6.1f dB (%s), the engine's own part worst %6.1f dB\n", en->name, worst, wn, wref);
    }
    return bad != 0;
}
