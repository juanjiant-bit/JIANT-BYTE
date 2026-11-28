/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (cpu3, the owner: "con 3 pistas ya se explota la CPU") the owner's project as JIANT Studio's analysis gives it, rebuilt
 * by name (T1 DRUM-W 808 BOOM, T2 FUNC MINIMAL, T3 WAVE LEG with its steps, T4 MATH FLOAT ALGO 108, T5 WAVE SAW PAD, T6
 * MATH LEG): the analysis names a MATH part's routes and every part's P_LRTRG / P_KEY with WAVE's labels (BLEND=1:
 * MATH's RTRG ON, DRUM-W's KEY MAP EXTENDED; FRAMES=0: MATH's TIME 0, DRUM-W's GAIN 0; T:A a MATH part's E1, CHIP).
 * Played as he plays it: the transport running, three parts (T1 T2 T4) and each of T3 T5 T6 with them, chords every
 * ~1/8 with the drum's pads, the macros, AT and MODW moving every block (M4 -> T:A, M2 -> V:POS, AT -> V:CUT ...).
 *   cpu3_test              the scenes run: the parts sound, nothing compiled in the audio, the hash of each
 *   cpu3_test vg <core>    under callgrind (tests/cpu3_test.py): a dump each block (mix_block alone) of core 0's
 *                          instructions (core1_run's work left out) or core 1's (core1_run's only) */
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#define MC_HOST_ASYNC
static int c3_mc_run(void (*f)(uint32_t), uint32_t a);
#define mc_run(f, a) c3_mc_run(f, a)
#define mc_wait() 0
#define HOST_LIFE LIFE_DEF
#include "ui_test.c"
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_TOGGLE_COLLECT
#define CALLGRIND_DUMP_STATS_AT(s) ((void)(s))
#define CALLGRIND_ZERO_STATS
#endif
static int c3_core = -1;                                 /* (vg: the core counted) */
static __attribute__((noinline)) void core1_run(void (*f)(uint32_t), uint32_t a) { f(a); __asm__ volatile(""); }
static int c3_mc_run(void (*f)(uint32_t), uint32_t a)
{
    if (!mc_host) return 0;
    mc_host_cpu = 1;
    if (c3_core >= 0) CALLGRIND_TOGGLE_COLLECT;
    core1_run(f, a);
    if (c3_core >= 0) CALLGRIND_TOGGLE_COLLECT;
    mc_host_cpu = 0;
    return 1;
}
static int32_t vo[2 * CTL];
static uint32_t bad, blk;
static void main_loop_poll(void) { math_poll(); dw_poll(); }
static int pid_by(track_t *t, const char *nm, uint32_t *used)
{
    uint32_t id, pass;
    for (pass = 0; pass < 2u; pass++)
        for (id = 0; id < P_COUNT; id++) {
            const param_desc_t *d;
            if ((pass == 0) != (id >= P_E0 && id <= P_E7)) continue;
            if ((used[id / 32u] >> (id % 32u)) & 1u) continue;
            d = track_desc(t, id);
            if (d && d->label && !strcmp(d->label, nm)) return (int)id;
        }
    return -1;
}
static int val_of(track_t *t, uint32_t id, const char *v)
{
    const param_desc_t *d = track_desc(t, id);
    int k;
    char *e;
    long n = strtol(v, &e, 10);
    if (!*e) return (int)n;
    if (id >= P_M1SRC && id <= P_M4AMT && (id - P_M1SRC) % 3u == 0u) {
        for (k = 0; k < MS_N; k++) if (!strcmp(PN_AT(N_MSRC, k), v)) return k;
    }
    if (id >= P_M1SRC && id <= P_M4AMT && (id - P_M1SRC) % 3u == 1u) {
        for (k = 0; k < MD_N; k++) {
            char b[32];
            if (!mod_dst_ok(t, k)) continue;
            snprintf(b, sizeof b, "%c:%s", mod_dst_scope(t, k), mod_dst_name_bt(t, k));
            if (!strcmp(b, v)) return k;
        }
        {   /* JIANT Studio names a MATH part's routes with WAVE's labels */
            static const char *const WL[8] = {"A", "POS", "WARP", "AMT", "B", "MIX", "DETUNE", "SUB"};
            if (!strcmp(v, "V:POS")) return MD_SHP;
            for (k = 0; k < 8; k++) if (v[0] == 'T' && !strcmp(v + 2, WL[k])) return MD_E1 + k;
        }
    }
    if (d && d->names)
        for (k = d->min; k <= d->max; k++) if (desc_nm(d, k) && !strcmp(desc_nm(d, k), v)) return k;
    if (d && d->label) {                                 /* the knob's names (lofi_desc ..) */
        for (k = d->min; k <= d->max; k++) {
            char b[32] = {0};
            { const char *u; param_format(d, k, b, &u); }
            if (!strcmp(b, v)) return k;
        }
    }
    fprintf(stderr, "cpu3: no value %s for %s\n", v, d ? d->label : "?"); bad++;
    return 0;
}
static void setp(track_t *t, const char *spec)
{
    char buf[1024], *tok, *sv;
    uint32_t used[(P_COUNT + 31u) / 32u] = {0};
    int ndec = 0;
    snprintf(buf, sizeof buf, "%s", spec);
    for (tok = strtok_r(buf, " ", &sv); tok; tok = strtok_r(0, " ", &sv)) {
        char *eq = strchr(tok, '=');
        int id;
        if (!eq) continue;
        *eq = 0;
        if (!strcmp(tok, "BLEND")) id = P_LRTRG;
        else if (!strcmp(tok, "FRAMES")) id = P_KEY;
        else if (!strcmp(tok, "DEC") && ndec++) id = P_E2DEC;
        else if (!strcmp(tok, "REL") && ndec > 1) id = P_E2REL;
        else id = pid_by(t, tok, used);
        if (id < 0) { fprintf(stderr, "cpu3: no parameter %s\n", tok); bad++; continue; }
        used[id / 32u] |= 1u << (id % 32u);
        t->p[id] = (int16_t)val_of(t, (uint32_t)id, eq + 1);
    }
}
static void part(uint32_t k, uint32_t e, uint32_t pr, uint32_t lvl, const char *spec)
{
    track_t *t = &trk[k];
    uint32_t i;
    set_engine_of(t, e);
    t->engine = t->eng_req;
    apply_preset_to(t, pr - 1u);
    if (lvl) t->p[P_LEVEL] = (int16_t)lvl;
    setp(t, spec);
    for (i = 0; i < 512u && (i < 64u || t->xf_on || t->eng_req != t->engine); i++) { mix_block(vo, CTL); main_loop_poll(); }
}
static void project(void)
{
    uint32_t k;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    part(0, ENGI_DRUM, 3, 104, "ATK=0 DEC=100 SUS=127 REL=100 REV=24 BLEND=1 FRAMES=0 MORPH=58 WAVE=54 DECAY=38 NOISE=59");
    part(1, ENGI_FUNC, 19, 104, "ATK=0 DEC=0 SUS=127 REL=24 DLY=30 REV=10 BLEND=1 FRAMES=0 BLIP=70 HATS=80 DRIVE=40 FORM=MINIMAL");
    part(2, ENGI_WAVE, 10, 0, "ATK=0 DEC=60 SUS=100 REL=15 DIST=25 DLY=20 POLY=LEG GLIDE=12 SRC1=ENV2 DEST1=V:POS AMT1=50 SRC2=ENV2 DEST2=T:F.CUT AMT2=40 DEC=76 REL=20 FRAMES=0 VEL=0 CUT=46 RES=100 A=TRISY POS=6 SUB=30");
    {   static const uint8_t ST[8][2] = {{1,60},{3,59},{5,59},{6,62},{9,59},{10,59},{14,64},{16,59}};
        track_t *t = &trk[2];
        for (k = 0; k < NSTEP; k++) { memset(&t->step[k], 0, sizeof t->step[k]); t->step[k].time = ST_REST; }
        for (k = 0; k < 8u; k++) { step_t *s = &t->step[ST[k][0] - 1u]; s->time = ST_NOTE; s->n = 1; s->note[0] = ST[k][1]; s->vel = 100; }
    }
    part(3, ENGI_LOFI, 4, 0, "ATK=0 DEC=75 SUS=40 REL=45 FILTER=20 SRC1=VEL DEST1=V:CUT AMT1=20 SRC2=M4 DEST2=T:A AMT2=44 SRC3=M2 DEST3=V:POS AMT3=38 SRC4=AT DEST4=V:CUT AMT4=20 BLEND=1 FRAMES=0 CUT=84 WAVE=FLOAT ALGO=108");
    part(4, ENGI_WAVE, 3, 0, "ATK=80 DEC=90 SUS=100 REL=95 RATE=12 SRC1=LFO DEST1=V:POS AMT1=20 SRC2=M1 DEST2=T:WARP AMT2=-24 SRC3=M3 DEST3=T:AMT AMT3=-38 SRC4=AT DEST4=V:CUT AMT4=20 FRAMES=0 A=SAWSQ POS=20 B=TRISY MIX=50 DETUNE=40");
    part(5, ENGI_LOFI, 1, 0, "ATK=0 DEC=80 SUS=110 REL=40 POLY=LEG SRC1=MODW DEST1=T:AMT AMT1=40 SRC2=AT DEST2=T:DETUNE AMT2=40 SRC3=M2 DEST3=T:A AMT3=-61 SRC4=AT DEST4=V:CUT AMT4=20 BLEND=1 FRAMES=0 ALGO=4");
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 4096;
}
static void unmute(uint32_t m) { uint32_t k; for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = !((m >> k) & 1u); }
static void move(void)                                   /* the matrix's sources: the macros, AT, MODW */
{
    uint32_t k, ph = blk++;
    for (k = 0; k < 4u; k++) macro_v[k] = (uint8_t)(64 + 50 * ((int32_t)((ph * (3 + k)) % 128u) - 64) / 64);
    for (k = 0; k < NTRK; k++) { trk[k].at = (uint8_t)((ph * 5u + k * 17u) % 128u); trk[k].mw = (uint8_t)((ph * 2u) % 128u); }
}
#define NBLK 256u
static uint32_t scene(uint32_t mask, int vg)             /* the hash of what it played */
{
    static const uint8_t CH[4][3] = {{48, 55, 60}, {50, 57, 62}, {45, 52, 57}, {43, 50, 55}};
    uint32_t b, i, q, h = 2166136261u, cnt0 = fvm_audio_compiles;
    char nm[24];
    project(); unmute(mask);
    transport_req = 1; ui_input();
    for (b = 0; b < 64u + NBLK; b++) {
        if (b % 43u == 0u) {
            const uint8_t *c = CH[(b / 43u) % 4u], *o = CH[(b / 43u + 3u) % 4u];
            for (i = 0; i < 3u; i++) { trk_note_off(&trk[1], o[i]); trk_note_off(&trk[3], o[i] + 12u); }
            for (i = 0; i < 3u; i++) { trk_note_on(&trk[1], c[i], 100); trk_note_on(&trk[3], c[i] + 12u, 90); }
            trk_note_on(&trk[0], 36u + (b / 43u) % 4u, 110);
            if ((mask >> 4) & 1u) for (i = 0; i < 3u; i++) { trk_note_off(&trk[4], o[i]); trk_note_on(&trk[4], c[i], 90); }
            if ((mask >> 5) & 1u) { trk_note_off(&trk[5], o[0]); trk_note_on(&trk[5], c[0], 90); }
        }
        move();
        snprintf(nm, sizeof nm, "cpu3/%02x/%03u", mask, b - 64u);
        if (vg) CALLGRIND_ZERO_STATS;                    /* (the block alone: its events, parts, buses, master) */
        mix_block(vo, CTL);
        if (vg && b >= 64u) CALLGRIND_DUMP_STATS_AT(nm);
        if (b & 1u)                                      /* (the main loop's, between the blocks) */
            main_loop_poll();
        for (q = 0; q < 2u * CTL; q++) h = (h ^ (uint32_t)vo[q]) * 16777619u;
    }
    if (fvm_audio_compiles != cnt0) { fprintf(stderr, "cpu3: %02x a formula compiled in the audio\n", mask); bad++; }
    return h;
}
static const uint8_t MASKS[4] = {0x0B, 0x0F, 0x1B, 0x2B};   /* T1 T2 T4; and T3, T5, T6 with them */
int main(int argc, char **argv)
{
    uint32_t s;
    mc_host = 1;
    if (argc > 2 && !strcmp(argv[1], "vg")) {
        c3_core = atoi(argv[2]) & 1;                     /* (cpu3_test.py: core 0 collected from the start, core 1
                                                         * not: core1_run toggles it either way) */
        for (s = 0; s < NELEM(MASKS); s++) scene(MASKS[s], 1);
        return 0;
    }
    for (s = 0; s < NELEM(MASKS); s++) {
        uint32_t k, sounding = 0, h = scene(MASKS[s], 0);
        for (k = 0; k < NTRK; k++) sounding += (MASKS[s] >> k & 1u) && trk[k].peak > 0;
        printf("cpu3: parts %02x hash %08x, %u of them sounding\n", MASKS[s], h, sounding);
        if (!sounding) bad++;
    }
    printf(bad ? "cpu3: FAILED\n" : "cpu3: the owner's project builds and plays\n");
    return bad != 0;
}
