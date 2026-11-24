/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE, engopt) the engines' and the effects' cost a block, scenario by scenario, under callgrind: the average and the
 * worst block (the spikes: CPU FULL at a mean of 54 %), and the audio itself, for a before / after comparison.
 *   engopt_vg LIST                       the scenarios' names
 *   engopt_vg SCEN [out.raw]             render SCEN (its stereo int32 to out.raw)
 *   valgrind --tool=callgrind --collect-atstart=no engopt_vg SCEN - perblock
 *                                        a dump a block (tools: tests/engopt_vg.py) */
#define UI_TEST_NO_MAIN
#define HOST_LIFE LIFE_DEF
#include "ui_test.c"
#include <valgrind/callgrind.h>

#define WARM 64u
static int32_t vo[2 * CTL];
static uint32_t eo_blocks = 1024u;

static void eo_song(uint32_t genre, uint32_t seed, uint32_t form)
{
    gp.genre = (uint8_t)genre; gp.seed = (uint16_t)seed;
    gp.energy = 70; gp.form = (uint8_t)form;
    gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
    for (uint32_t k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    gen_go();
}
static void eo_fx(int rtype, int pit, int sends)
{
    song.g[G_RTYPE] = (int16_t)rtype;
    song.g[G_DPIT] = (int16_t)pit;
    song.g[G_DSPRY] = 0;
    if (song.g[G_DFDBK] < 50) song.g[G_DFDBK] = 60;
    if (song.g[G_CDEPTH] < 40) song.g[G_CDEPTH] = 60;
    if (song.g[G_RSIZE] < 60) song.g[G_RSIZE] = 90;
    if (sends)
        for (uint32_t k = 0; k < NTRK; k++) {
            track_t *t = &trk[k];
            if (t->p[P_REV] < 30) t->p[P_REV] = 50;
            if (t->p[P_DLY] < 20) t->p[P_DLY] = 40;
            if (t->p[P_CHOR] < 20) t->p[P_CHOR] = 40;
        }
}
static uint32_t eo_preset(uint32_t e, const char *n)
{
    for (uint32_t k = 0; k < ENGINES[e]->npresets; k++)
        if (str_eq(ENGINES[e]->presets[k].name, n)) return k;
    return 0;
}
static void eo_quiet(void)
{
    ui_power_on();
    for (uint32_t k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
}
static void eo_part(uint32_t k, uint32_t e, uint32_t pr)
{
    track_t *t = &trk[k];
    set_engine_of(t, e);
    t->engine = t->eng_req;
    apply_preset_to(t, pr);
    t->p[P_MUTE] = 0;
    for (uint32_t i = 0; i < 512u && (i < 64u || t->xf_on || t->eng_req != t->engine); i++)
        mix_block(vo, CTL);
}
static void eo_notes(uint32_t k, uint32_t nn)
{
    static const uint8_t N[8] = {48, 55, 60, 64, 67, 71, 74, 79};
    for (uint32_t i = 0; i < nn && i < 8u; i++)
        trk_note_on(&trk[k], N[i], 100);
}

/* the scenarios: name, setup */
typedef struct { char name[32]; uint32_t kind, a, b, c; } scen_t;
enum { SK_GEN, SK_WAVE, SK_DRUM, SK_MATH, SK_CLAY, SK_WAVEPLAY, SK_FXWALK };
static uint32_t eo_walk;
static scen_t SC[128];
static uint32_t nsc;
static void add(const char *n, uint32_t kind, uint32_t a, uint32_t b, uint32_t c)
{
    scen_t *s = &SC[nsc++];
    snprintf(s->name, sizeof s->name, "%s", n); s->kind = kind; s->a = a; s->b = b; s->c = c;
}
static void build_list(void)
{
    char n[32];
    add("gen/cloud.g3", SK_GEN, 3, 4, 7);    /* genre, rtype, pitch */
    add("gen/cloud.g6", SK_GEN, 6, 4, 12);
    add("gen/shimr.g3", SK_GEN, 3, 2, 0);
    add("gen/shimr.g8", SK_GEN, 8, 2, 0);
    add("gen/room.g1", SK_GEN, 1, 0, 0);
    add("gen/plain.g5", SK_GEN, 5, 0, 0x100);  /* GEN's own FX */
    for (uint32_t k = 0; k < ENG_WAVE.npresets; k++) {
        snprintf(n, sizeof n, "wave3/%s", ENG_WAVE.presets[k].name);
        add(n, SK_WAVE, k, 3, 0);
    }
    add("wave1/SAW PAD", SK_WAVE, eo_preset(ENGI_WAVE, "SAW PAD"), 1, 0);
    add("wave1/SUPERSAW", SK_WAVE, eo_preset(ENGI_WAVE, "SUPERSAW"), 1, 0);
    add("wave8/SUPERSAW", SK_WAVE, eo_preset(ENGI_WAVE, "SUPERSAW"), 8, 0);
    add("waveseq/MORPH PAD", SK_WAVEPLAY, eo_preset(ENGI_WAVE, "MORPH PAD"), 0, 0);
    add("waveseq/ACID BASS", SK_WAVEPLAY, eo_preset(ENGI_WAVE, "ACID BASS"), 0, 0);
    for (uint32_t k = 0; k < ENG_DRUM.npresets && k < 8u; k++) {
        snprintf(n, sizeof n, "drum/%s", ENG_DRUM.presets[k].name);
        add(n, SK_DRUM, k, 0, 0);
    }
    add("math/BYTE", SK_MATH, RW_BYTE, 4, 0);
    add("math/FLOAT", SK_MATH, RW_FLOAT, 4, 0);
    add("math/FUNC", SK_MATH, RW_FUNC, 4, 0);
    add("math/USER", SK_MATH, RW_USER, 4, 0);
    add("fx/walk", SK_FXWALK, 0, 0, 0);
    add("fx/walk.lofi", SK_FXWALK, 1, 0, 0);
    add("clay/WAVE", SK_CLAY, ENGI_WAVE, (uint32_t)-1, 0);
    add("clay/MATH.FLOAT", SK_CLAY, ENGI_LOFI, RW_FLOAT, 0);
    add("clay/DRUM", SK_CLAY, ENGI_DRUM, (uint32_t)-1, 0);
}
static void eo_tick(uint32_t b)                   /* (fx/walk) a change every 256 blocks */
{
    static const int16_t W[][8] = {   /* RTYPE DTIME DPIT DPING DTYPE CRATE DDRV RPRE */
        {4, DT_FREE + 20, 7, 0, 0, 40, 0, -6}, {4, DT_FREE + 30, 0, 0, 0, 40, 0, 6}, {2, DT_FREE + 30, 0, 1, 0, 100, 0, -6},
        {2, DT_FREE + 44, 0, 0, 0, 127, 50, 8}, {0, DT_FREE + 44, 5, 0, 0, 70, 0, 0}, {1, DT_FREE + 50, 0, 0, 2, 20, 0, 0},
        {3, DT_FREE + 25, 0, 0, 2, 64, 0, 3}, {4, DT_FREE + 25, -12, 0, 0, 90, 0, 12}, {0, DT_FREE + 10, 0, 1, 1, 120, 30, 0},
        {4, DT_FREE + 50, 12, 0, 0, 60, 0, -12}, {2, DT_FREE + 15, 3, 0, 0, 30, 0, 0}, {0, DT_FREE + 35, 0, 0, 0, 50, 0, 0}};
    if (!eo_walk || b % 256u) return;
    const int16_t *w = W[(b / 256u) % (sizeof W / sizeof W[0])];
    song.g[G_RTYPE] = w[0]; song.g[G_DTIME] = w[1]; song.g[G_DPIT] = w[2]; song.g[G_DPING] = w[3]; song.g[G_DTYPE] = w[4];
    song.g[G_CRATE] = w[5]; song.g[G_DDRV] = w[6]; song.g[G_RPRE] = w[7];
    song.g[G_RMOD] = (int16_t)((b / 256u) * 23u % 100u); song.g[G_RWIDE] = (int16_t)((b / 256u) * 37u % 128u);
    song.g[G_DSPRY] = (int16_t)((b / 512u) & 1u ? 40 : 0);
    if (eo_walk == 2u) sys_load = (uint8_t)((b / 768u) % 3u);
}
static void setup(const scen_t *s)
{
    uint32_t k;
    switch (s->kind) {
    case SK_GEN:
        ui_power_on();
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
        eo_song(s->a, 37u, 2);
        if (!(s->c & 0x100u)) eo_fx((int)s->b, (int)s->c, 1);
        eo_blocks = 4096u;
        transport_req = 1; ui_input();
        break;
    case SK_WAVE:
        eo_quiet();
        eo_part(0, ENGI_WAVE, s->a);
        trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127;
        eo_notes(0, s->b);
        break;
    case SK_WAVEPLAY:                              /* a pattern on the preset, the FX on */
        eo_quiet();
        eo_part(0, ENGI_WAVE, s->a);
        load_pat16(&trk[0], PATTERNS[4].note, PATTERNS[4].flags);
        eo_fx(4, 0, 1);
        transport_req = 1; ui_input();
        eo_blocks = 2048u;
        break;
    case SK_DRUM:
        eo_quiet();
        eo_part(1, ENGI_DRUM, s->a);
        for (k = 0; k < 16u; k++) {
            step_t *st = &trk[1].step[k];
            memset(st, 0, sizeof *st);
            st->time = ST_NOTE; st->vel = 100; st->hit = (uint8_t)(k & 1u ? 0x0Cu : 0xF3u);
        }
        trk[1].p[P_SLEN] = 16;
        song.g[G_BPM] = 140;
        transport_req = 1; ui_input();
        eo_blocks = 2048u;
        break;
    case SK_MATH:
        eo_quiet();
        eo_part(0, ENGI_LOFI, 0);
        trk[0].p[P_E1] = (int16_t)s->a;
        trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_SUS] = 127;
        for (k = 0; k < 16u; k++) frame();
        eo_notes(0, s->b);
        break;
    case SK_FXWALK:                               /* the FX's every branch in turn (eo_tick): a song into all three */
        ui_power_on();
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
        eo_song(4, 51u, 0);
        eo_fx(4, 7, 1);
        eo_walk = 1u + s->a;
        eo_blocks = 6144u;
        transport_req = 1; ui_input();
        break;
    case SK_CLAY:
        ui_power_on(); song.sel = 0; select_engine(s->a); frame();
        if (s->b != (uint32_t)-1) trk[0].p[P_E1] = (int16_t)s->b;
        for (k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
        clay_defaults(clay[0]); song.master_q12 = 4096; song.g[G_BPM] = 120; frames(8);
        rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;
        transport_req = 1;
        eo_blocks = 2048u;
        break;
    }
}
int main(int argc, char **argv)
{
    uint32_t i, b;
    int per = argc > 3 && !strcmp(argv[3], "perblock");
    FILE *fo = 0;
    vpol_on = 0;
    build_list();
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "LIST")) {
        for (i = 0; i < nsc; i++) printf("%s\n", SC[i].name);
        return 0;
    }
    for (i = 0; i < nsc && strcmp(SC[i].name, argv[1]); i++)
        ;
    if (i == nsc) { fprintf(stderr, "no scenario %s\n", argv[1]); return 2; }
    if (argc > 2 && strcmp(argv[2], "-")) fo = fopen(argv[2], "wb");
    setup(&SC[i]);
    song.master_q12 = 4096;
    for (b = 0; b < WARM; b++) {
        mix_block(vo, CTL);
        dw_poll();
        if (fo) fwrite(vo, sizeof vo, 1, fo);
    }
    {
        static uint8_t want[1u << 16];
        static uint32_t tsc[1u << 16];
        int tm = argc > 3 && !strcmp(argv[3], "time");
        if (argc > 3 && !strncmp(argv[3], "dump=", 5)) {
            const char *q = argv[3] + 5;
            while (*q) { uint32_t x = (uint32_t)strtoul(q, (char **)&q, 10); if (x < sizeof want) want[x] = 1; if (*q) q++; }
            per = 2;
        }
        CALLGRIND_ZERO_STATS;
        for (b = 0; b < eo_blocks; b++) {
            uint64_t t0 = 0;
            if (per == 2 && want[b]) CALLGRIND_ZERO_STATS;
            if (tm) t0 = __builtin_ia32_rdtsc();
            eo_tick(b);
            CALLGRIND_TOGGLE_COLLECT;
            mix_block(vo, CTL);
            CALLGRIND_TOGGLE_COLLECT;
            if (tm) tsc[b] = (uint32_t)(__builtin_ia32_rdtsc() - t0);
            if (per == 1 || (per == 2 && want[b])) { char nm[32]; snprintf(nm, sizeof nm, "b%u", b); CALLGRIND_DUMP_STATS_AT(nm); }
            if (fo) fwrite(vo, sizeof vo, 1, fo);
            if (b % (FS / 60u / CTL) == 0u) { frame(); dw_poll(); }   /* (the main loop: not counted) */
        }
        if (!per) CALLGRIND_DUMP_STATS_AT("total");
        if (tm) fwrite(tsc, 4, eo_blocks, stdout);
    }
    if (fo) fclose(fo);
    return 0;
}
