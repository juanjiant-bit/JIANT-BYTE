/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (devcpu2) the scenes of the lines model (tests/cpu_lines_test.py): the FM-1's time follows the 32-byte lines of the
 * flash the sound reads each block (code and constants), core by core, more than its instructions. Run under valgrind's
 * lackey: each block of the scene between two stores to cl_mark, core 1's work (mc_job) between two to cl_c1, each part
 * (fx.c mix_part_t) between two to cl_part[k], the host's own main loop (engines.c fvm_host_serve) between two to cl_ex.
 *   cpu_lines_test SCENE NBLK      (SCENE hex: 101 the heaviest GEN song, s123, CLOUD, delay and chorus; 102 .. 105 other
 *                                   GEN songs; 0f the owner's project, its four parts; 200 its parts idle; 300 asleep)
 * CL_COST="c0,..,c5,fx" (us): each part's cost as the FM-1 measures it (mc_tk, mc_fx_tk), for the device's split */
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#define MC_HOST_ASYNC
static int cl_mc_run(void (*f)(uint32_t), uint32_t a);
#define mc_run(f, a) cl_mc_run(f, a)
#define mc_wait() 0
#define HOST_LIFE LIFE_DEF
volatile int cl_mark, cl_c1, cl_ex, cl_part[8];
#define CPU_LINES_PART(i, v) (cl_part[(i) & 7u] = (v))
#define CPU_LINES_EX(v) (cl_ex = (v))
#include "ui_test.c"
static __attribute__((noinline)) void core1_run(void (*f)(uint32_t), uint32_t a) { f(a); __asm__ volatile(""); }
static int cl_mc_run(void (*f)(uint32_t), uint32_t a)
{
    if (!mc_host) return 0;
    mc_host_cpu = 1;
    cl_c1 = 1;
    core1_run(f, a);
    cl_c1 = 2;
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
        for (k = 0; k < MS_N; k++) if (!strcmp(N_MSRC[k], v)) return k;
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
        for (k = d->min; k <= d->max; k++) if (d->names[k] && !strcmp(d->names[k], v)) return k;
    if (d && d->label) {
        for (k = d->min; k <= d->max; k++) {
            char b[32] = {0};
            { const char *u; param_format(d, k, b, &u); }
            if (!strcmp(b, v)) return k;
        }
    }
    fprintf(stderr, "cpu_lines: no value %s for %s\n", v, d ? d->label : "?"); bad++;
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
        if (!strcmp(tok, "DEC") && ndec++) id = P_E2DEC;
        else if (!strcmp(tok, "REL") && ndec > 1) id = P_E2REL;
        else id = pid_by(t, tok, used);
        if (id < 0) { fprintf(stderr, "cpu_lines: no parameter %s\n", tok); bad++; continue; }
        used[id / 32u] |= 1u << (id % 32u);
        t->p[id] = (int16_t)val_of(t, (uint32_t)id, eq + 1);
    }
}
static void part(uint32_t k, uint32_t e, uint32_t pr, const char *spec)
{
    track_t *t = &trk[k];
    uint32_t i;
    set_engine_of(t, e);
    t->engine = t->eng_req;
    apply_preset_to(t, pr - 1u);
    setp(t, spec);
    for (i = 0; i < 512u && (i < 64u || t->xf_on || t->eng_req != t->engine); i++) { mix_block(vo, CTL); main_loop_poll(); }
}
static void steps(uint32_t k, const uint8_t (*st)[2], uint32_t n)
{
    track_t *t = &trk[k]; uint32_t i;
    for (i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }
    for (i = 0; i < n; i++) { step_t *s = &t->step[st[i][0] - 1u]; s->time = ST_NOTE; s->n = 1; s->note[0] = st[i][1]; s->vel = 100; }
}
/* the owner's project as the FM-1 played it for MENU > ABOUT (f22f004f: NOW 83 %, PEAK 132 %, FX 212 us, FUNC 581 ..
 * 610, DRUM-W 422 .. 509, WAVE 98 .. 136, MATH 12): T1 FUNC AMBIENT, T2 DRUM-W, T3 WAVE with its steps, T4 MATH FLOAT
 * ALGO 108 with its steps, six parts live */
static void project(void)
{
    uint32_t k;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    part(0, ENGI_FUNC, 18, "FORM=AMBIENT RUN=8 CHORUS=50 DLY=30 REV=70 CUT=100 FM=34 DRIFT=40 SHINE=28 ATK=0 DEC=0 SUS=127");
    part(1, ENGI_DRUM, 6, "SRC1=M2 DEST1=T:WAVE AMT1=36 SRC2=M4 DEST2=T:DLY AMT2=55 MORPH=56 WAVE=56 DECAY=62 DEC=100 SUS=127 REL=100");
    part(2, ENGI_WAVE, 19, "ATK=80 DEC=100 SUS=110 REL=110 RATE=30 CHORUS=50 DLY=60 REV=100 SRC1=ENV2 DEST1=V:POS AMT1=50 SRC2=LFO DEST2=T:AMT AMT2=20 ATK=110 SUS=127 REL=110 IMAGE=80 A=SINTR POS=40 WARP=MIRROR AMT=20 DETUNE=30");
    {   static const uint8_t ST[7][2] = {{1,60},{4,64},{6,60},{8,59},{11,59},{13,62},{15,59}}; steps(2, ST, 7); }
    part(3, ENGI_LOFI, 4, "FILTER=20 SRC1=VEL DEST1=V:CUT AMT1=20 SRC2=M4 DEST2=T:WAVE AMT2=-62 SRC3=M2 DEST3=T:REV AMT3=-42 SRC4=AT DEST4=V:CUT AMT4=20 CUT=84 WAVE=FLOAT ALGO=108");
    {   static const uint8_t ST[9][2] = {{1,64},{3,64},{5,60},{7,60},{9,60},{10,60},{12,64},{14,64},{16,60}}; steps(3, ST, 9); }
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 4096;
}
static void unmute(uint32_t m) { uint32_t k; for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = !((m >> k) & 1u); }
static void move(void)                                   /* the matrix's sources moving: the macros, AT, MODW */
{
    uint32_t k, ph = blk++;
    for (k = 0; k < 4u; k++) macro_v[k] = (uint8_t)(64 + 50 * ((int32_t)((ph * (3 + k)) % 128u) - 64) / 64);
    for (k = 0; k < NTRK; k++) { trk[k].at = (uint8_t)((ph * 5u + k * 17u) % 128u); trk[k].mw = (uint8_t)((ph * 2u) % 128u); }
}
static uint32_t cl_cost[NTRK + 1u], cl_cost_on;
static void cost_in(void)                                /* (the device's split: what each part took, as it measures it) */
{
    uint32_t k;
    if (!cl_cost_on) return;
    for (k = 0; k < NTRK; k++) mc_tk[k] = cl_cost[k];
    mc_fx_tk = cl_cost[NTRK];
}
static uint32_t scene(uint32_t mask, uint32_t nblk)
{
    uint32_t b, q, h = 2166136261u, vis = (mask >> 12) & 1u;   /* (VIS lite) 1xxx: the scene with HOME > VIS on the
                                                         * screen, the UI drawn between the blocks (~60 frames a second:
                                                         * one each 11 blocks), outside the blocks the model reads */
    blk = 0;
    mask &= 0xFFFu;
    if (mask == 0x200u) { project(); unmute(0x0Fu); }   /* the parts idle: on, no notes, stopped */
    else if (mask == 0x300u) { project(); unmute(0x0Fu); cpu_pause = 0x3F; }   /* .. asleep: the block without them */
    else if (mask >= 0x100u) {                           /* GEN songs: EXPLORE s123 (relief_test.c's heaviest), with CLOUD,
                                                         * the delay and the chorus (101); EXPLORE s7 DANCE; genre 7 s42;
                                                         * HIP HOP s2024 DANCE; EXPLORE s999 at full energy */
        static const struct { uint8_t g; uint16_t s; uint8_t e; int8_t m; } GS[6] = {
            {0, 123, 85, 0}, {0, 123, 85, 0}, {0, 7, 90, 50}, {7, 42, 85, 0}, {NGENRE, 2024, 85, 50}, {0, 999, 100, 0}};
        uint32_t k, gi = (mask & 0xFFu) < 6u ? mask & 0xFFu : 0u;
        ui_power_on();
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        gp.genre = GS[gi].g ? GS[gi].g : GEN_EXPLORE; gp.seed = GS[gi].s; gp.energy = GS[gi].e; gp.form = 0;
        grd.mood = GS[gi].m;
        gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        life_v[LIFE_AMT] = 100;
        gen_go();
        for (k = 0; k < 64u; k++) { mix_block(vo, CTL); math_poll(); dw_poll(); }
        if (mask == 0x101u) { song.g[G_RTYPE] = 4; song.g[G_DFDBK] = 70; song.g[G_DTIME] = DT_FREE + 40;
            for (k = 0; k < NTRK; k++) { trk[k].p[P_DLY] = 70; trk[k].p[P_REV] = 80; trk[k].p[P_CHOR] = 70; } }
        song.master_q12 = 4096;
    } else { project(); unmute(mask); }
    if (getenv("CL_OTT")) {                              /* (BYTE ott) every track's OTT at this DEPTH (the worst case: 100) */
        uint32_t k;
        for (k = 0; k < NTRK; k++) ott_v[k][OT_DEPTH] = (int16_t)atoi(getenv("CL_OTT"));
    }
    if (mask < 0x200u) { transport_req = 1; ui_input(); }
#if FELUCCA_VIS
    if (vis) { nav_go(SEC_HOME, HE_VIS, 0); ui.force = 1; }
#else
    (void)vis;
#endif
    for (b = 0; b < 64u + nblk; b++) {
        if (mask < 0x100u && b == 0u && (mask & 1u)) trk_note_on(&trk[0], 60, 100);   /* (FUNC's chord held, the kit's */
        if (mask < 0x100u && b % 43u == 0u && (mask & 2u)) trk_note_on(&trk[1], 36u, 110);   /* hits ~1/8; WAVE and MATH:
                                                         * their steps) */
        move();
        cost_in();
        if (b >= 64u) cl_mark = 1;
        mix_block(vo, CTL);
        if (b >= 64u) cl_mark = 2;
        if (b & 1u) main_loop_poll();
        if (vis && b % 11u == 0u) { fm1_ms += 16u; ui_draw(); }
        for (q = 0; q < 2u * CTL; q++) h = (h ^ (uint32_t)vo[q]) * 16777619u;
    }
    return h;
}
int main(int argc, char **argv)
{
    uint32_t m = argc > 1 ? strtoul(argv[1], 0, 16) : 0x101u, nblk = argc > 2 ? strtoul(argv[2], 0, 10) : 16u, h, k;
    mc_host = 1;                                         /* (two cores: core 1's job at once, between cl_c1's stores) */
    if (getenv("CL_COST")) {
        char *p = getenv("CL_COST");
        cl_cost_on = 1;
        for (k = 0; k <= NTRK && *p; k++) { cl_cost[k] = strtoul(p, &p, 10); if (*p == ',') p++; }
    }
    h = scene(m, nblk);
    printf("cpu_lines: scene %03x hash %08x parts sounding", m, h);
    for (k = 0; k < NTRK; k++) printf(" %u", trk[k].peak > 0);
    printf(" bad %u\n", bad);
    return bad != 0;
}
