/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE punchfx, the owner: "some punch-in effects are not noticeable with the engines") the FX layer's punch-ins (pfx.c,
 * PSQ_NM's: OCT- OCT+ HALF WOB REP S8 S16 BITS MIR SCR, LOOP 3 5 7, CAOS MAX, LIFE MAX) on every engine, heard as the
 * device plays them (mix_block: pfx_block, the sequencer, the parts, the master): one part (a MATH BYTE / FLOAT / USER /
 * FUNC, a WAVE, a DRUM-W preset, the default and others; DRONE CLAY on a WAVE: only bounded and let go), a simple
 * sequence at 120 BPM (a melody of 1/16s and rests; a kit's groove), three bars; the key held over bar 2, let go for
 * bar 3, against the same render with no key. Each A-weighted (the ear's sensitivity), loudness as the power ^ 0.3:
 *   spec  the distance of bar 2's 1/16 steps, each its loudness in 20 bands of half an octave (rhythm, register, timbre;
 *         two noises the same)
 *   fine  the frames' loudness bin by bin (2048 points): a pitch a semitone off, a vibrato (bar 3's: its noise floor)
 *   env   1 - the correlation of the 1/32 loudness envelopes (the rhythm)
 *   cen   the spectral centroid's move, octaves
 *   rest  spec over bar 3 from its second beat (let go: back to normal), and the state left (nothing)
 * Each render from the same power-on state, its own process; GLO > PUNCH SEQ's rows too. PUNCHFX_TABLE=1 prints the
 * whole audit, every cell's numbers.
 *   cc -std=gnu99 -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/punchfx tests/punchfx_test.c -lm */
#include <stdarg.h>
#include <sys/mman.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int pq_fails;
#define BAR (4u * 22050u)                               /* 120 BPM: a bar 2 s */
#define NBAR 3u
#define NS (NBAR * BAR)
typedef struct { const char *name; uint32_t eng, preset; int mode; int clay; } pq_eng_t;   /* mode: -1 the preset's; RW_* */
enum { PQ_MEL, PQ_DRM };
static const pq_eng_t PQE[] = {
    {"MATH BYTE  BYTEBEAT", ENGI_LOFI, 0, -1, 0},
    {"MATH BYTE  BYTE TONE", ENGI_LOFI, 8, -1, 0},
    {"MATH FLOAT MATH KEYS", ENGI_LOFI, 3, -1, 0},
    {"MATH FLOAT MATH BASS", ENGI_LOFI, 4, -1, 0},
    {"MATH USER  seed 64", ENGI_LOFI, 0, RW_USER, 0},
    {"MATH USER  seed 120", ENGI_LOFI, 3, RW_USER, 0},
    {"FUNC SONG", ENGI_FUNC, 0, -1, 0},          /* (BYTE 0.17: FUNC its own engine; was MATH 12) */
    {"FUNC BEAT", ENGI_FUNC, 1, -1, 0},
    {"WAVE       INIT WAVE", ENGI_WAVE, 0, -1, 0},
    {"WAVE       SUB BASS", ENGI_WAVE, 4, -1, 0},
    {"WAVE       FM KEYS", ENGI_WAVE, 1, -1, 0},
    {"DRUM-W     ACOUSTIC", ENGI_DRUM, TRK_PR_DRUM, -1, 0},
    {"DRUM-W     909 CLUB", ENGI_DRUM, 1, -1, 0},
    {"CLAY       WAVE", ENGI_WAVE, 0, -1, 1},
};
#define PQ_NE (sizeof PQE / sizeof PQE[0])
static const struct { const char *n; int e; } PQF[] = {
    {"OCT-", PF_OCTD}, {"OCT+", PF_OCTU}, {"HALF", PF_HALF}, {"WOB", PF_WOB}, {"REP", PF_D16}, {"S8", PF_S8},
    {"S16", PF_S16}, {"BITS", PF_ATK}, {"MIR", PF_ARP}, {"SCR", PF_RND}, {"LOOP3", PF_LP3}, {"LOOP5", PF_LP5},
    {"LOOP7", PF_LP7}, {"CAOS", PF_RING}, {"LIFE", PF_LMAX},
};
#define PQ_NF (sizeof PQF / sizeof PQF[0])

static float *pq_out;                                   /* (shared with the renders' processes) */
static void pq_setup(const pq_eng_t *E)
{
    static const uint8_t MEL[16] = {48, 0, 55, 60, 0, 63, 60, 55, 50, 0, 58, 62, 0, 65, 62, 58};
    static gq_work_t w;
    track_t *t = &trk[0];
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    perf_held = perf_latched = 0; perf_kill = 0; scene_pfx = 0;
    song.g[G_BPM] = 120;
    if (E->eng == ENGI_DRUM)
        host_drums(t), host_preset(t, ENGI_DRUM, E->preset);
    else
        host_preset(t, E->eng, E->preset);
    if (E->mode == RW_USER) {
        t->p[P_E1] = RW_USER;
        t->p[P_E2] = E->preset ? 120 : 64;
    }
    for (i = 0; i < 4u * NTRK; i++)                     /* (USER, FUNC: the main loop's compile) */
        math_poll_ws(&w);
    t->p[P_DIST] = t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;   /* (the part itself: no tails over bar 3) */
    for (i = 0; i < 16u; i++) {
        uint8_t d[4], k = 0, n = MEL[i];
        if (E->eng == ENGI_DRUM) {
            if (i == 0u || i == 3u || i == 8u || i == 10u) d[k++] = 36;   /* (a groove, not a beat in four) */
            if ((i % 2u == 0u && i != 14u) || i == 7u) d[k++] = 42;
            if (i == 14u) d[k++] = 46;
            if (i == 4u || i == 12u || i == 15u) d[k++] = 38;
            put_step(t, i, k, d, k ? ST_NOTE : ST_REST, 0);
        } else
            put_step(t, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0);
    }
    if (E->clay) {
        clay_defaults(clay[0]);
        for (i = 0; i < NSTEP; i++)
            t->step[i].time = ST_REST;
    }
}
static int pq_psq = -1;                                /* (>= 0: no key; GLO > PUNCH SEQ's row 1 that effect, lit over bar 2) */
/* one render: the key e held over bar 2 (e < 0: none), into pq_out[slot * NS] */
static void pq_render(const pq_eng_t *E, int e, uint32_t slot)
{
    pid_t pid = fork();
    if (!pid) {
        int32_t o[2 * CTL];
        uint32_t t, i;
        float *y = pq_out + (size_t)slot * NS;
        pq_setup(E);
        psq_clear();
        transport_req = 1;
        for (t = 0; t < NS; t += CTL) {
            if (pq_psq >= 0 && t <= BAR && t + CTL > BAR)   /* (the row's steps all lit from bar 2's first: the rows' clock
                                                         * runs from PLAY, LEN 16 of 1/16: one bar) */
                psq.fx[0] = (uint8_t)pq_psq, psq.st[0] = 0xFFFFu, psq.len = 16, psq.res = 1;
            if (pq_psq >= 0 && t <= 2u * BAR && t + CTL > 2u * BAR)
                psq.st[0] = 0;
            if (e >= 0 && t <= BAR && t + CTL > BAR)
                perf_held |= PF_BIT((uint32_t)e);
            if (e >= 0 && t <= 2u * BAR && t + CTL > 2u * BAR)
                perf_held &= ~PF_BIT((uint32_t)e);
            mix_block(o, CTL);
            for (i = 0; i < CTL && t + i < NS; i++)
                y[t + i] = (float)(o[2 * i] + o[2 * i + 1]) / 65536.0f;
        }
        {   /* let go a bar since: nothing of it left (the pitch, the filter, the walk, the formulas' time, the loops) */
            uint32_t k, left = pt_ops | (uint32_t)pt_wob | (uint32_t)pt_cw | pt_lpn | lv.act | pfx_caos.on | pfx_life.on | fl_alone;
            for (k = 0; k < NTRK; k++)
                left |= (uint32_t)pt_vp[k] | trk[k].pfx | (uint32_t)trk[k].pfx_pit;
            _exit(left ? 3 : 0);
        }
    }
    {
        int st = 0;
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st)) {
            printf("punchfx: %s, key %d: let go, its state not all back (exit %d)    FAIL\n", E->name, e, WIFEXITED(st) ? WEXITSTATUS(st) : -1);
            pq_fails++;
        }
    }
}

#define FN 2048u
static double tw_c[FN / 2], tw_s[FN / 2], hann[FN], aw[FN / 2];
static double a_weight(double f)                        /* (IEC 61672 A-weighting, as power: the ear's sensitivity) */
{
    double f2 = f * f, r = 12194.0 * 12194.0 * f2 * f2 / ((f2 + 20.6 * 20.6) * sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0));
    return r * r * 1.2589;                              /* (+2.0 dB: 1 at 1 kHz) */
}
static uint8_t bnd[FN / 2];
static uint32_t band(uint32_t i);
static void fft(double *re, double *im, uint32_t n)      /* (n: FN) */
{
    uint32_t i, j, k, m;
    if (tw_c[0] == 0) {
        for (i = 0; i < FN / 2; i++) tw_c[i] = cos(-2 * M_PI * i / FN), tw_s[i] = sin(-2 * M_PI * i / FN), bnd[i] = (uint8_t)band(i),
                                     aw[i] = sqrt(a_weight(i * (double)FS / FN));
        for (i = 0; i < FN; i++) hann[i] = 0.5 - 0.5 * cos(2 * M_PI * i / FN);
    }
    for (i = 1, j = 0; i < n; i++) {
        uint32_t b = n >> 1;
        for (; j & b; b >>= 1) j ^= b;
        j |= b;
        if (i < j) { double a = re[i]; re[i] = re[j]; re[j] = a; a = im[i]; im[i] = im[j]; im[j] = a; }
    }
    for (m = 2; m <= n; m <<= 1) {
        uint32_t st = n / m;
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2; j++) {
                double c = tw_c[j * st], s = tw_s[j * st];
                double xr = re[k + j + m / 2] * c - im[k + j + m / 2] * s, xi = re[k + j + m / 2] * s + im[k + j + m / 2] * c;
                re[k + j + m / 2] = re[k + j] - xr; im[k + j + m / 2] = im[k + j] - xi;
                re[k + j] += xr; im[k + j] += xi;
            }
    }
}
static void mag(const float *y, double *M)               /* a frame's magnitudes (Hann) */
{
    static double re[FN], im[FN];
    uint32_t i;
    fft(re, im, 0);                                     /* (the tables) */
    for (i = 0; i < FN; i++) {
        re[i] = y[i] * hann[i];
        im[i] = 0;
    }
    fft(re, im, FN);
    for (i = 0; i < FN / 2; i++)
        M[i] = sqrt(re[i] * re[i] + im[i] * im[i]) * aw[i];   /* (A-weighted: what is heard, not a 20 Hz rumble) */
}
#define PQ_NB 20u                                       /* (half an octave a band from 20 Hz) */
static uint32_t band(uint32_t i)
{
    double x = 2 * log2(i * (double)FS / FN / 20);
    return x < 0 ? 0u : x >= PQ_NB ? PQ_NB - 1u : (uint32_t)x;
}
/* (each A-weighted) coarse: the distance of a's 1/16 steps from b's, each step its bands' loudness (the power over the step, ^ 0.3: two
 * noises the same, a rhythm, a register, a timbre apart); fine: the frames' loudness bin by bin (a pitch a semitone off,
 * a vibrato), as noise-blind as the sound is tonal (bar 3's fine distance: its floor) */
typedef struct { double coarse, fine, cen; } pq_d_t;
static pq_d_t pq_spec(const float *a, const float *b, uint32_t i0, uint32_t i1)
{
    static double A[FN / 2], B[FN / 2];
    double d = 0, e = 0, fd = 0, fe = 0, ca = 0, wa = 0, cb = 0, wb = 0;
    uint32_t s16 = BAR / 16u, o, f, k;
    pq_d_t r;
    for (o = i0; o + s16 <= i1; o += s16) {
        double PA[PQ_NB] = {0}, PB[PQ_NB] = {0};
        for (f = o; f + FN <= o + s16; f += FN / 4) {
            mag(a + f, A);
            mag(b + f, B);
            for (k = 1; k < FN / 2; k++) {
                PA[bnd[k]] += A[k] * A[k];
                PB[bnd[k]] += B[k] * B[k];
                double x = pow(A[k], 0.6), y = pow(B[k], 0.6);   /* (loudness: the power ^ 0.3, Stevens') */
                fd += (x - y) * (x - y);
                fe += y * y;
            }
        }
        for (k = 0; k < PQ_NB; k++) {
            double x = pow(PA[k], 0.3), y = pow(PB[k], 0.3);
            d += (x - y) * (x - y);
            e += y * y;
            ca += x * k; wa += x;
            cb += y * k; wb += y;
        }
    }
    r.cen = wa > 0 && wb > 0 ? (ca / wa - cb / wb) / 2 : 0;   /* (octaves) */
    r.coarse = e > 0 ? sqrt(d / e) : (d > 0 ? 9 : 0);
    r.fine = fe > 0 ? sqrt(fd / fe) : (fd > 0 ? 9 : 0);
    return r;
}
typedef struct { double spec, fine, env, cen, rest, frest, rms; } pq_m_t;
static pq_m_t pq_measure(const float *a, const float *b)
{
    pq_m_t m;
    pq_d_t h = pq_spec(a, b, BAR, 2u * BAR), l = pq_spec(a, b, 2u * BAR + BAR / 4u, 3u * BAR);   /* (bar 3 from its second beat) */
    uint32_t s32 = BAR / 32u, i, k;
    double ea[32], eb[32], ab = 0, aa = 0, bb = 0, ma = 0, mb = 0, r = 0;
    m.spec = h.coarse; m.fine = h.fine; m.cen = h.cen; m.rest = l.coarse; m.frest = l.fine;
    for (k = 0; k < 32u; k++) {
        double s = 0, t = 0;
        for (i = 0; i < s32; i++) {
            double x = a[BAR + k * s32 + i], y = b[BAR + k * s32 + i];
            s += x * x; t += y * y;
        }
        ea[k] = sqrt(s / s32); eb[k] = sqrt(t / s32);
        ma += ea[k] / 32; mb += eb[k] / 32;
        r += s;
    }
    for (k = 0; k < 32u; k++)
        ab += (ea[k] - ma) * (eb[k] - mb), aa += (ea[k] - ma) * (ea[k] - ma), bb += (eb[k] - mb) * (eb[k] - mb);
    m.env = aa > 0 && bb > 0 ? 1 - ab / sqrt(aa * bb) : (aa != bb ? 1 : 0);
    m.rms = sqrt(r / BAR);
    return m;
}
static double pq_peak(const float *a)
{
    double p = 0;
    uint32_t i;
    for (i = 0; i < NS; i++) {
        if (!(a[i] == a[i])) return 99;
        if (fabs(a[i]) > p) p = fabs(a[i]);
    }
    return p;
}

/* (the thresholds: heard clearly) over the bar held the steps' bands 0.35 apart, or the frames' bins 0.5 and twice
 * their floor (a pitch moved on a tonal sound); let go, bar 3's bands within 0.12 of normal (DRUM-W: 0.2, its noise
 * runs on from hit to hit, its phases elsewhere after any change) */
#define PQ_YES 0.35
#define PQ_FINE 0.5
#define PQ_WEAK 0.10
#define PQ_REST 0.12
#define PQ_REST_DRUM 0.20
static char pq_class(const pq_m_t *m)
{
    return m->spec >= PQ_YES || (m->fine >= PQ_FINE && m->fine >= 2 * m->frest) ? 'Y' : m->spec >= PQ_WEAK || m->fine >= PQ_WEAK ? 'w' : '-';
}
#ifndef PUNCHFX_NO_MAIN
int main(int argc, char **argv)
{
    uint32_t ei, fi, bad = 0, rbad = 0;
    int table = getenv("PUNCHFX_TABLE") != 0;
    static char row[PQ_NE][PQ_NF];
    (void)argc; (void)argv;
    pq_out = mmap(0, (PQ_NF + 1u) * NS * sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (pq_out == MAP_FAILED) return 2;
    for (ei = 0; ei < PQ_NE; ei++) {
        const pq_eng_t *E = &PQE[ei];
        const float *base = pq_out + (size_t)PQ_NF * NS;
        double pk;
        pq_render(E, -1, PQ_NF);
        pk = pq_peak(base);
        if (table)
            printf("punchfx: %-22s (rms %.3f peak %.2f)\n", E->name, pq_measure(base, base).rms, pk);
        if (pk < 0.005 || pk > 4) { printf("punchfx: %s: silent or broken without a key (peak %.3f)    FAIL\n", E->name, pk); bad++; }
        for (fi = 0; fi < PQ_NF; fi++) {
            const float *y = pq_out + (size_t)fi * NS;
            pq_m_t m;
            pq_render(E, PQF[fi].e, fi);
            m = pq_measure(y, base);
            pk = pq_peak(y);
            row[ei][fi] = pq_class(&m);
            if (table)
                printf("punchfx:   %-6s spec %5.2f fine %5.2f env %5.2f cen %+5.2f rest %5.2f/%4.2f peak %.2f  %s\n", PQF[fi].n,
                       m.spec, m.fine, m.env, m.cen, m.rest, m.frest, pk, row[ei][fi] == 'Y' ? "yes" : row[ei][fi] == 'w' ? "WEAK" : "NO");
            if (pk > 4 || pk != pk) { printf("punchfx: %s %s: out of bounds (peak %.2f)    FAIL\n", E->name, PQF[fi].n, pk); bad++; }
            if (!E->clay && row[ei][fi] != 'Y') {
                printf("punchfx: %s %s: not clearly heard (spec %.3f, fine %.3f, env %.3f)    FAIL\n", E->name, PQF[fi].n, m.spec, m.fine, m.env);
                bad++;
            }
            if (!E->clay && m.rest > (E->eng == ENGI_DRUM ? PQ_REST_DRUM : PQ_REST)) {
                printf("punchfx: %s %s: let go, not back to normal (bar 3: %.3f)    FAIL\n", E->name, PQF[fi].n, m.rest);
                rbad++;
            }
        }
    }
    {   /* GLO > PUNCH SEQ: a row holds the same bits (pfx.c psq_block): BITS, WOBBLE, HALF, CAOS from a row heard as
         * their keys are (WAVE INIT, FUNC SONG, DRUM-W) */
        static const struct { uint32_t e; int fx; } R[] = {{8, 7}, {8, 3}, {6, 2}, {6, 13}, {11, 9}, {11, 13}};
        uint32_t r, rb = 0;
        for (r = 0; r < sizeof R / sizeof R[0]; r++) {
            const float *base = pq_out + (size_t)PQ_NF * NS, *y = pq_out;
            pq_m_t m;
            pq_psq = -1;
            pq_render(&PQE[R[r].e], -1, PQ_NF);
            pq_psq = R[r].fx;
            pq_render(&PQE[R[r].e], -1, 0);
            pq_psq = -1;
            m = pq_measure(y, base);
            if (pq_class(&m) != 'Y' || m.rest > (PQE[R[r].e].eng == ENGI_DRUM ? PQ_REST_DRUM : PQ_REST)) {
                printf("punchfx: PUNCH SEQ %s on %s: spec %.3f fine %.3f rest %.3f    FAIL\n", str_nth(PSQ_NM, R[r].fx), PQE[R[r].e].name, m.spec, m.fine, m.rest);
                rb++;
            }
        }
        printf("punchfx: GLO > PUNCH SEQ's rows heard as the keys (BITS WOB on WAVE, HALF CAOS on FUNC, SCR CAOS on DRUM-W), let go: back    %s\n", rb ? "FAIL" : "ok");
        bad += rb;
    }
    printf("punchfx: %-22s", "");
    for (fi = 0; fi < PQ_NF; fi++) printf(" %-5.5s", PQF[fi].n);
    printf("\n");
    for (ei = 0; ei < PQ_NE; ei++) {
        printf("punchfx: %-22s", PQE[ei].name);
        for (fi = 0; fi < PQ_NF; fi++) printf(" %-5s", row[ei][fi] == 'Y' ? "yes" : row[ei][fi] == 'w' ? "weak" : "NO");
        printf("\n");
    }
    printf("punchfx: every punch-in heard on every engine (spec >= %.2f over the bar held)    %s\n", PQ_YES, bad ? "FAIL" : "ok");
    printf("punchfx: let go, every engine back to normal (bar 3 within %.2f)    %s\n", PQ_REST, rbad ? "FAIL" : "ok");
    pq_fails += (int)(bad + rbad);
    printf("punchfx: %s\n", pq_fails ? "FAIL" : "all ok");
    return pq_fails != 0;
}
#endif
