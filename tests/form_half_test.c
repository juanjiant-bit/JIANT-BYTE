/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT BYTE, byte-cpu-light) FORM at half the rate (eng_form.c .half 2, voice.c voice_half) against the full one:
 * FORM plays the transport's song, so byte_half_test's phrases cannot reach it. Its song played twice (states A, A to B
 * at DENSITY 127 with a chord), once with every block of its voice at half, compared under 4 kHz (as byte_half_test).
 * Well: about -15 dB or less; near 0: the half rate is another tempo or another pitch.
 *   form_half_test [LIM] */
#define FORM_HALF 0u                                    /* (the full rate to compare against) */
#define UI_TEST_NO_MAIN
#include "ui_test.c"
#include <sys/wait.h>
#include <unistd.h>
#define NB 600u                                         /* blocks: ~0.9 s */
static int32_t vo[2 * CTL];
static void play(uint32_t ab, uint32_t half, int32_t *buf)
{
    uint32_t k, b, q;
    ui_power_on();
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    track_t *t = &trk[0];
    set_engine_of(t, ENGI_FORM); t->engine = t->eng_req; apply_preset_to(t, 0);
    t->p[P_MUTE] = 0; t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    for (k = 0; k < 64u; k++) mix_block(vo, CTL);
    t->p[P_E0] = (int16_t)(getenv("SA") ? atoi(getenv("SA")) : 2); t->p[P_E1] = (int16_t)(getenv("SB") ? atoi(getenv("SB")) : 7); t->p[P_E2] = (int16_t)(ab ? 64 : 0); t->p[P_E5] = 127; t->p[P_CHRD] = 3; t->p[P_KEY] = 127;
    memset(vh_c, 0, sizeof vh_c);
    transport_req = 1; ui_input();
    for (b = 0; b < NB; b++) {
        if (half)
            for (q = 0; q < NVOICE; q++)
                if (t->v[q].active && !VH_ST(vh_c[q])) vh_c[q] = VH_IN;
        mix_block(vo, CTL);
        for (q = 0; q < CTL; q++) buf[b * CTL + q] = mix_l[q];     /* (the host master is 0: the parts' sum) */
    }
}
static void play_fork(uint32_t ab, uint32_t half, int32_t *buf)
{
    int fd[2]; size_t got = 0, n = NB * CTL * sizeof *buf;
    if (pipe(fd)) exit(2);
    if (!fork()) { close(fd[0]); play(ab, half, buf); while (got < n) { ssize_t w = write(fd[1], (char *)buf + got, n - got); if (w <= 0) _exit(1); got += (size_t)w; } _exit(0); }
    close(fd[1]);
    while (got < n) { ssize_t r = read(fd[0], (char *)buf + got, n - got); if (r <= 0) break; got += (size_t)r; }
    close(fd[0]); wait(0);
}
static double rms_db(const int32_t *a, const int32_t *b, uint32_t n)
{
    double sa = 0, sd = 0, ya = 0, yb = 0, za = 0, zb = 0, k = 1.0 - exp(-2.0 * M_PI * 4000.0 / FS);
    for (uint32_t i = 0; i < n; i++) {
        ya += k * (a[i] - ya); za += k * (ya - za); yb += k * (b[i] - yb); zb += k * (yb - zb);
        sa += za * za; sd += (za - zb) * (za - zb);
    }
    return sa > 0 ? 10.0 * log10((sd + 1e-9) / sa) : 0.0;
}
int main(int argc, char **argv)
{
    static int32_t full[NB * CTL], half[NB * CTL];
    double lim = argc > 1 ? atof(argv[1]) : -15.0;
    int bad = 0;
    for (uint32_t ab = 0; ab < 2u; ab++) {
        double e = 0, d;
        play_fork(ab, 0, full); play_fork(ab, 1, half);
        for (uint32_t i = 0; i < NB * CTL; i++) e += (double)full[i] * full[i];
        d = rms_db(full, half, NB * CTL);
        printf("FORM %-6s half vs full under 4 kHz %6.1f dB (level %.0f)\n", ab ? "A to B" : "A", d, sqrt(e / (NB * CTL)));
        if (e == 0 || d > lim) bad = 1;
    }
    return bad;
}
