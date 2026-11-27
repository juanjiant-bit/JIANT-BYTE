/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (loopq) MATH's LOOP (P_E7) as a musical division while TIME is SYNC (eng_lofi.c lofi_time):
 *   SYNC LOOP 1 BAR   the sound repeats exactly every bar, in phase with the transport: two notes started at different
 *                     times the same loop on the same bars (from their first bar line on)
 *   UNSYNC / FIJO     bit for bit as before (the hashes of the renders before loopq), the tone zone too
 *   the knob card     SYNC: OFF 4 BAR .. 1/32, then the tone zone's % as before; UNSYNC: the % as before
 * The tempo 125: a beat 21168 samples, a bar a whole number of blocks (the song's clock read once a block) */
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
    printf("loopq: %-100s %s\n", b, c ? "ok" : "FAIL");
    if (!c)
        fails++;
}
#define BPM 125u
#define BL ((uint32_t)FS * 60u / BPM)                /* (21168) */
#define BAR (BL * 4u)
#define NB 5u                                        /* five bars of the song */
static int32_t ys[NB * BAR];                        /* indexed by the song's position (0: bar 0's first sample) */

/* a MATH preset on track 0, dry, the envelope held; the song from bar 0, the note at block `at`, held to bar NB */
static void render(uint32_t preset, int32_t key, int32_t loop, int32_t mot, uint32_t at, int32_t *y)
{
    static int32_t blk[CTL];
    track_t *t = &trk[0];
    uint32_t pos = 0, k;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(t, ENGI_LOFI, preset);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    t->p[P_ATK] = 0; t->p[P_DEC] = 0; t->p[P_SUS] = 127;
    t->p[P_KEY] = (int16_t)key;
    t->p[P_E7] = (int16_t)loop;
    t->p[P_E4] = (int16_t)mot;
    song.g[G_BPM] = BPM;
    beat_pos = beat_n = bar_n = 0;
    track_render(t, blk, CTL);                       /* (the formula asked for: the main loop compiles it) */
    memset(y, 0, sizeof ys);
    for (pos = 0; pos + CTL <= NB * BAR; pos += CTL) {
        if (pos == at * CTL)
            trk_note_on(t, 60, 100);
        memset(blk, 0, sizeof blk);
        track_render(t, blk, CTL);
        for (k = 0; k < CTL; k++)
            y[pos + k] = blk[k];
        beat_pos += CTL;                             /* (the sequencer's clock, by hand) */
        if (beat_pos >= BL) {
            beat_pos -= BL;
            if (++beat_n >= 4u) {
                beat_n = 0;
                bar_n++;
            }
        }
    }
}
static uint32_t hash(const int32_t *y, uint32_t n)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < n; i++)
        h = (h ^ (uint32_t)y[i]) * 16777619u;
    return h;
}
static double rms(const int32_t *y, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++) s += (double)y[i] * y[i];
    return sqrt(s / n) / 32768.0;
}
/* the renders before loopq (UNSYNC, FIJO; SYNC with LOOP off or in the tone zone): their hashes */
static const struct { uint8_t preset, key, loop, mot; uint32_t h; } GOLD[] = {
    {0, 0, 0, 0, 0xBB4F17EEu},
    {0, 0, 3, 0, 0xBB4F17EEu},
    {0, 0, 40, 9, 0xDEC7A69Eu},
    {0, 0, 80, 0, 0xBE20511Bu},
    {0, 2, 3, 0, 0xC6A978FFu},
    {0, 2, 80, 6, 0x9895BCC7u},
    {0, 1, 0, 0, 0x4F29697Au},
    {0, 1, 80, 0, 0x310A6ADEu},
    {0, 1, 40, 6, 0x336DBB5Cu},
    {3, 0, 0, 0, 0x4857DF89u},
    {3, 0, 5, 0, 0x4857DF89u},
    {3, 0, 127, 0, 0x7CAE8BF9u},
    {3, 2, 5, 0, 0xE7237AF6u},
    {3, 1, 127, 0, 0x7CAE8BF9u},
    {3, 1, 0, 9, 0x2D371ADEu},
    {8, 0, 80, 0, 0x5167CC34u},
    {8, 1, 80, 0, 0xF6D94E39u}
};

int main(int argc, char **argv)
{
    static int32_t ya[NB * BAR];
    uint32_t i, dump = argc > 1;
    (void)argv;
    for (i = 0; i < NELEM(GOLD); i++) {
        uint32_t h;
        render(GOLD[i].preset, GOLD[i].key, GOLD[i].loop, GOLD[i].mot, 37u, ys);
        h = hash(ys, NB * BAR);
        if (dump)
            printf("    {%u, %u, %u, %u, 0x%08Xu},\n", GOLD[i].preset, GOLD[i].key, GOLD[i].loop, GOLD[i].mot, h);
        else
            ok(h == GOLD[i].h && rms(ys, NB * BAR) > 0.002, "preset %u TIME %u LOOP %3u MOTION %2u: bit for bit as before (%08X, rms %.3f)",
               GOLD[i].preset, GOLD[i].key, GOLD[i].loop, GOLD[i].mot, h, rms(ys, NB * BAR));
    }
    if (dump)
        return 0;
    {   /* SYNC LOOP 1 BAR: every bar the same, and two notes (bar 0 + 0.3 beat, bar 1 + 2.7 beats) the same loop */
        uint32_t pa = BL * 3u / 10u / CTL, pb = (BAR + BL * 27u / 10u) / CTL, b, same = 1, diff = 0;
        uint32_t pr[] = {0, 3}, q;
        for (q = 0; q < 2u; q++) {
            render(pr[q], 1, 3, 0, pa, ya);
            render(pr[q], 1, 3, 0, pb, ys);
            same = 1;
            for (b = 2; b + 1 < NB; b++)
                same &= !memcmp(ya + b * BAR, ya + (b + 1u) * BAR, BAR * sizeof *ya);
            ok(same && rms(ya + 2u * BAR, BAR) > 0.005, "preset %u SYNC LOOP 1 BAR: bars 2 3 4 the same, sample for sample "
               "(rms %.3f)", pr[q], rms(ya + 2u * BAR, BAR));
            ok(!memcmp(ya + 2u * BAR + CTL, ys + 2u * BAR + CTL, ((NB - 2u) * BAR - CTL) * sizeof *ya),
               "preset %u SYNC LOOP 1 BAR: a note at 0.3 beat and one at bar 1 + 2.7 the same loop on the bar (from "
               "bar 2, past its first block: the smoothing's tail of the bar before)", pr[q]);
            render(pr[q], 1, 0, 0, pa, ys);          /* (LOOP off: the bars differ) */
            for (b = 2, diff = 0; b + 1 < NB; b++)
                diff += !!memcmp(ys + b * BAR, ys + (b + 1u) * BAR, BAR * sizeof *ys);
            ok(diff > 0, "preset %u SYNC LOOP OFF: the bars not a loop (%u of 2 differ)", pr[q], diff);
        }
        {   /* 1/4: every beat alike (a beat is not a whole number of blocks: the song's clock read once a block,
             * a step of t apart here and there), every bar the same */
            double cmin = 1;
            render(0, 1, 5, 0, pa, ya);
            for (b = 8; b + 1 < NB * 4u; b++) {
                double best = -1;
                int32_t lag;
                for (lag = -2; lag <= 2; lag++) {           /* (within a sample of the half rate) */
                    double ab = 0, aa = 0, bb = 0, c;
                    for (i = 4; i + 4 < BL; i++) {
                        double x = ya[b * BL + i], y = ya[(b + 1u) * BL + i + lag];
                        ab += x * y, aa += x * x, bb += y * y;
                    }
                    c = ab / sqrt(aa * bb + 1e-9);
                    if (c > best) best = c;
                }
                if (best < cmin) cmin = best;
            }
            ok(cmin > 0.95 && !memcmp(ya + 2u * BAR, ya + 3u * BAR, BAR * sizeof *ya),
               "SYNC LOOP 1/4: every beat alike (within a sample: correlation >= %.4f), every bar the same", cmin);
        }
        render(0, 1, 2, 9, pa, ya);                  /* 2 BAR, MOTION 1/4: bar 2 as bar 4, not as bar 3 */
        ok(!memcmp(ya + 2u * BAR + CTL, ya + 4u * BAR + CTL, (BAR - CTL) * sizeof *ya) &&
           memcmp(ya + 2u * BAR, ya + 3u * BAR, BAR * sizeof *ya), "SYNC LOOP 2 BAR (MOTION 1/4): two bars a loop, not one");
    }
    {   /* the knob card */
        static const char *const W[] = {"OFF", "4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
        track_t *t = &trk[0];
        char val[8], ref[8];
        const char *u, *ur;
        int32_t v, good = 1;
        host_preset(t, ENGI_LOFI, 0);
        t->p[P_KEY] = 1;
        for (v = 0; v <= 8; v++) {
            param_format(track_desc(t, P_E7), v, val, &u);
            good &= !strcmp(val, W[v]) && !*u;
        }
        ok(good, "SYNC: LOOP 0 .. 8 OFF, 4 BAR .. 1/32 on the card");
        for (v = 9, good = 1; v <= 127; v++) {
            param_format(track_desc(t, P_E7), v, val, &u);
            param_format(&ENG_LOFI.edit[7], v, ref, &ur);
            good &= !strcmp(val, ref) && !strcmp(u, ur);
        }
        ok(good, "SYNC: LOOP 9 .. 127 the tone zone's %% as before");
        t->p[P_KEY] = 0;
        for (v = 0, good = 1; v <= 127; v++) {
            param_format(track_desc(t, P_E7), v, val, &u);
            param_format(&ENG_LOFI.edit[7], v, ref, &ur);
            good &= !strcmp(val, ref) && !strcmp(u, ur);
        }
        ok(good, "UNSYNC: LOOP as before (%%)");
    }
    printf(fails ? "loopq: %d FAILED\n" : "loopq: all ok\n", fails);
    return fails != 0;
}
