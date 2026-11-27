/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (visgen) HOME > VIS and MENU > SAVER (firmware/src/vis_recipe.c, ui_vis.c) on the web emulator's build, natively:
 *   - the recipe's test vectors: docs/VISGEN.md's table, row by row, as vis_recipe makes them (JIANT Studio's spec);
 *   - different seeds make different recipes (of 400 neighbours, almost every one differs; many distinct);
 *   - the same seed the same frames: a song's VIS drawn, another recipe (OCT+), the song's own again: its frames the same
 *     as the first time, pixel for pixel; another seed's not; the knobs' MIX REACT follow the recipe;
 *   - the saver: after SAVER's minutes with no input, the whole screen; the first key brings the page back and plays no
 *     note (swallowed); SAVER OFF never;
 *   - the project keeps the song's VIS ('V' 'S'): packed, cleared, unpacked; VIS_SONG / VIS_TICK's state (vis_state);
 *   - the emulator's web_vis_info() (JIANT Studio's jiantEmu.vis()) off the VIS page, LIFE off: playing after PLAY, the parts' levels moving (followed while VIS is read: mod.c vis_fol). */
#define FELUCCA_WEB_EDITOR 1
#define FELUCCA_VIS 1                                   /* (the visual is out of the default build: core.h) */
#include "../web/emu/felucca_web.c"
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>

static unsigned nfail;
static void check(const char *what, int ok) { printf("%-64s %s\n", what, ok ? "ok" : "FAIL"); nfail += !ok; }
static void run_ms(uint32_t ms) { while (ms--) { web_ms(); ring_r = ring_w; } }
static uint32_t fb_hash(uint32_t y0, uint32_t y1)
{
    uint32_t h = 2166136261u, i;
    for (i = y0 * 240u; i < y1 * 240u; i++) h = (h ^ web_fb[i]) * 16777619u;
    return h;
}
static const char *const IN[] = {"LIFE", "KEY", "HARMONY", "LFO", "SPLASH", "RADAR", "ORBIT", "CLAY", "TUNNEL", "CHORUS", "HIVE",
                                 "FUNC", "MATH"};
static const char *const DN[] = {"TWIST", "FOLD", "MIRROR", "POLAR", "MELT", "SCAN"};
static const char *vnm(uint8_t v) { return v == 255 ? "-" : IN[v % 13u]; }
static void row(char *o, size_t n, uint32_t seed, uint32_t g, uint32_t e, uint32_t m, uint32_t rec)
{
    vis_recipe_t R;
    int s, k = 0;
    vis_recipe(&R, seed, g, e, m, rec);
    k += snprintf(o + k, n - (size_t)k, "| %u | %u | %u | 0x%02X | %u | %s | %u | %u | %u | %u | %u | %u | %u |", seed, g, e, m, rec,
                  vnm(R.content), R.fam, R.mood, R.blend, R.pal, R.mix, R.react, R.lis);
    for (s = 0; s < 4; s++) {
        k += snprintf(o + k, n - (size_t)k, " %s %s", vnm(R.skel[s]), vnm(R.over[s]));
        if (R.def[s][0] != 255) k += snprintf(o + k, n - (size_t)k, " %s %u", DN[R.def[s][0] % 6u], R.amt[s][0]);
        if (R.def[s][1] != 255) k += snprintf(o + k, n - (size_t)k, " %s %u", DN[R.def[s][1] % 6u], R.amt[s][1]);
        k += snprintf(o + k, n - (size_t)k, " |");
    }
}
static void vectors(void)
{
    FILE *f = fopen("docs/VISGEN.md", "r");
    char line[512], mine[512];
    unsigned n = 0, bad = 0;
    if (!f) { check("docs/VISGEN.md found", 0); return; }
    while (fgets(line, sizeof line, f)) {
        unsigned s, g, e, m, r;
        if (sscanf(line, "| %u | %u | %u | 0x%x | %u |", &s, &g, &e, &m, &r) != 5) continue;
        line[strcspn(line, "\n")] = 0;
        row(mine, sizeof mine, s, g, e, m, r);
        if (strcmp(line, mine)) { bad++; printf("  doc  %s\n  code %s\n", line, mine); }
        n++;
    }
    fclose(f);
    check("docs/VISGEN.md: 12 test vectors, each as vis_recipe makes it", n == 12u && !bad);
}
static void seeds(void)
{
    uint32_t s, diff = 0, uniq = 0, k;
    static uint32_t seen[400];
    for (s = 0; s < 400u; s++) {
        vis_recipe_t a, b;
        uint32_t h = 2166136261u;
        const uint8_t *p = (const uint8_t *)&a;
        vis_recipe(&a, 1000u + s, 4, 70, 0x91, 0);
        vis_recipe(&b, 1001u + s, 4, 70, 0x91, 0);
        diff += memcmp(&a, &b, sizeof a) != 0;
        for (k = 0; k < sizeof a; k++) h = (h ^ p[k]) * 16777619u;
        for (k = 0; k < uniq && seen[k] != h; k++) {}
        if (k == uniq) seen[uniq++] = h;
    }
    check("neighbouring seeds: different recipes (400 of 400)", diff == 400u);
    check("400 seeds: 400 distinct recipes", uniq == 400u);
    {
        vis_recipe_t a, b;
        vis_recipe(&a, 421, 4, 70, 0x11, 0);
        vis_recipe(&b, 421, 4, 70, 0x11, 0);
        check("the same inputs: the same recipe", !memcmp(&a, &b, sizeof a));
        vis_recipe(&b, 421, 4, 70, 0x11, 1);
        check("OCT+ (the recipe byte): another recipe", memcmp(&a, &b, sizeof a) != 0);
    }
}
/* HOME > VIS in a child (fork) from the same moment: first seed s0 recipe r0 for 600 ms, then seed s1 recipe r1 from
 * clean for 12 frames, their panel's rows hashed: what the parent gets */
static void vis_set(uint32_t seed, uint32_t rec)
{
    vis_id[0] = (uint8_t)seed; vis_id[1] = (uint8_t)(seed >> 8); vis_id[2] = 4u + 1u; vis_id[3] = 70; vis_id[4] = (uint8_t)rec;
}
static uint32_t branch(uint32_t s0, uint32_t r0, uint32_t s1, uint32_t r1)
{
    int fd[2];
    uint32_t h = 0, i;
    pid_t p;
    assert(!pipe(fd));
    fflush(stdout);
    if (!(p = fork())) {
        vis_set(s0, r0);
        run_ms(600);
        vis_set(s1, r1);
        vg.key = 0;                                     /* (the visual from clean, as a song's start) */
        for (i = 0; i < 12u; i++) {
            run_ms(40);
            h = h * 31u + fb_hash(84, 224);
        }
        assert(write(fd[1], &h, sizeof h) == sizeof h);
        _exit(0);
    }
    assert(read(fd[0], &h, sizeof h) == sizeof h);
    waitpid(p, 0, 0);
    close(fd[0]); close(fd[1]);
    return h;
}
static void frames(void)
{
    uint32_t a, b, c, d;
    nav_go(SEC_HOME, HE_VIS, 0);
    ui.force = 1;
    run_ms(200);
    check("HOME > VIS: its page", home_vis() && !strcmp(nav_page_name(), "VIS"));
    vis_set(4242, 0);
    run_ms(100);
    {
        vis_recipe_t r0;                                /* (the song's own recipe, the set's first) */
        vis_recipe(&r0, 4242, 4, 70, vis_mask(), 0);
        check("the knobs: MIX and REACT the song's recipe's, DEFORM 50", vis_k[0] == r0.mix && vis_k[3] == r0.react && vis_k[1] == 50);
    }
    a = branch(4242, 0, 4242, 0);
    b = branch(4242, 1, 4242, 0);                       /* (OCT+ first, then back to the song's own) */
    c = branch(4242, 0, 4242, 1);
    d = branch(4242, 0, 777, 0);
    check("the same seed: the same frames (after another recipe first)", a == b);
    check("OCT+'s recipe: other frames", a != c);
    check("another seed: other frames", a != d);
}
/* HOME > VIS: the whole screen (no header, tabs, cards, footer); a knob turned: its card a moment; away: the page whole */
static void fullscreen(void)
{
    uint32_t a, b, c;
    nav_go(SEC_HOME, HE_VIS, 0);
    ui.force = 1;
    run_ms(1600);
    a = fb_hash(0, 84);
    run_ms(200);
    check("HOME > VIS: the visual over the whole screen (the header's, tabs', cards' rows moving)", a != fb_hash(0, 84));
    c = vis_k[0];
    web_enc(EN_K1, 3);                                  /* MIX turned: its card */
    run_ms(60);
    b = fb_hash(0, 60);
    check(".. a knob turned: MIX up, its card shown over it", vis_k[0] > c && (vis_ov & 7u) == 0u && fm1_ms - (vis_ov & ~15u) < 1300u);
    run_ms(1500);
    check(".. and gone after a moment", fm1_ms - (vis_ov & ~15u) >= 1300u);
    nav_go(SEC_HOME, HE_MACROS, 0);
    run_ms(200);
    c = fb_hash(0, 34);
    nav_go(SEC_HOME, HE_MACROS, 0);
    ui.force = 1;
    run_ms(200);
    check(".. away (MACROS): the page drawn whole again", c == fb_hash(0, 34) && b != c);
}
static void saver(void)
{
    uint32_t page, sv, i;
    nav_go(SEC_HOME, HE_MACROS, 0);
    ui.force = 1;
    menu_put(MI_SAVER, 1);                              /* 1 MIN */
    check("MENU > SAVER: 1 MIN", menu_get(MI_SAVER) == 1u && !strcmp(menu_vname(MI_SAVER, 1), "1 MIN"));
    run_ms(200);
    page = fb_hash(0, 84);
    run_ms(61000);
    sv = fb_hash(0, 84);
    check("after a minute idle: the saver, the whole screen", scrn.st == SCR_VIS && sv != page && kb_asleep);
    web_keys(1u << 5);                                   /* a key: only the page back */
    run_ms(30);
    check("the first key: the page back", scrn.st == SCR_ON);
    for (i = 0; i < NVOICE; i++)
        if (trk[song.sel].v[i].active) break;
    check(".. and no note played (swallowed)", i == NVOICE);
    web_keys(0);
    run_ms(600);
    check(".. then input again", !kb_asleep && !scrn.eat);
    menu_put(MI_SAVER, 0);
    run_ms(70000);
    check("SAVER OFF: never", scrn.st == SCR_ON);
    menu_put(MI_SAVER, 2);
    check("SAVER: 3 MIN the default (0 stored)", ((favorites.factory[15][12] >> 4) & 7u) == 0u);
}
static void project(void)
{
    int32_t o[21];
    vis_id[0] = 0x34; vis_id[1] = 0x12; vis_id[2] = 9; vis_id[3] = 66; vis_id[4] = 7;
    project_capture(&proj_scratch);
    check("the project keeps the song's VIS", !memcmp(proj_scratch.vis, vis_id, 5));
    vis_state(o);
    check("VIS_SONG's state: seed, recipe, genre, energy", o[0] == 0x1234 && o[1] == 7 && o[2] == 8 && o[3] == 66);
    check(".. the knobs, the section; VIS_TICK's levels in range", o[6] == vis_k[2] && o[7] == vis_k[0] && o[5] >= 0 && o[5] < 4 &&
          o[10] >= 0 && o[10] < 128 && o[17] < 12 && o[18] < 5 && o[19] < 64);
}
static int sx(const uint8_t *b, uint32_t n)             /* a request to the editor; its replies and pushes drained */
{
    memcpy(web_sysex_buf(), b, n);
    return (int)web_sysex(n);
}
static void usb_pushes(void)
{
    static const uint8_t W[] = {0xF0, 0x7D, 0x46, 0x4C, 22, 1, 1, 0xF7}, P[] = {0xF0, 0x7D, 0x46, 0x4C, 25, 0xF7};
    uint32_t n, song = 0, tick = 0, wa = 0, i, k;
    int32_t o[21];
    uint8_t m[64];
    sx(W, sizeof W);
    for (i = 0; i < 400u; i++) {
        if (i % 50u == 0u) sx(P, sizeof P);
        run_ms(1);
        web_editor_poll();
        while ((n = web_sysex_out())) {
            memcpy(m, web_sysex_msg(), n < 64u ? n : 64u);
            if (m[4] == 22) wa = n == 8u && (m[6] & 1u);
            if (m[4] == 98 && n == 20u) {
                vis_state(o);
                song += (uint32_t)(m[5] | m[6] << 7 | m[7] << 14) == (uint32_t)o[0] && m[10] == o[2] && m[11] == o[3] &&
                        m[14] == o[5] && m[16] == o[7] && m[18] == o[9];
            }
            if (m[4] == 99 && n == 16u) {
                for (k = 0; k < 6u && m[5 + k] < 128u; k++) {}
                tick += k == 6u && m[12] < 12u && m[13] < 5u && m[14] < 64u;
            }
        }
    }
    check("USB: WATCH bit 7 (its second byte) taken", wa);
    check("USB: VIS_SONG (98) first, its fields as vis_state's", song == 1u);
    check("USB: VIS_TICK (99) ~25 a second while playing", tick >= 8u && tick <= 11u);
}
static void web_levels(void)
{
    uint32_t i, k, mx = 0;
    int32_t *o;
    web_play(0);
    run_ms(300);
    nav_go(SEC_HOME, HE_MACROS, 0);
    life_v[LIFE_AMT] = 0;
    web_gen_cmd(4, 1);                                  /* (Studio's gen(4, 1): PLAY, taken by the next UI pass) */
    run_ms(30);
    o = web_vis_info();
    check("web_vis_info: playing after PLAY, as web_gen_info", o[20] == web_gen_info()[7] && o[20]);
    for (i = 0; i < 40u; i++) {
        run_ms(40);
        o = web_vis_info();
        for (k = 0; k < 6u; k++) mx = (uint32_t)o[10 + k] > mx ? (uint32_t)o[10 + k] : mx;
    }
    check("web_vis_info: the levels move off the VIS page, LIFE off", mx >= 16u && mx < 128u);
    for (i = 0, mx = 0; i < 6u; i++) {                  /* (Studio's: a read a second, far past vis_fol's 0.37 s) */
        run_ms(1000);
        o = web_vis_info();
        for (k = 0; k < 6u; k++) mx = (uint32_t)o[10 + k] > mx ? (uint32_t)o[10 + k] : mx;
    }
    check("web_vis_info: the levels read once a second, not 0", mx >= 8u && mx < 128u);
    {   /* hits bit 4: a timbre change (a part's preset, its CUT a quarter of its range) once, the next read none */
        int32_t h0, h1, h2;
        run_ms(40);
        h0 = web_vis_info()[16];
        trk[0].p[P_FCUT] = (int16_t)((trk[0].p[P_FCUT] + 64) & 127);
        run_ms(1);
        h1 = web_vis_info()[16];
        run_ms(1);
        h2 = web_vis_info()[16];
        check("web_vis_info: hits bit 4 on a timbre change, once", !(h0 & 16) && (h1 & 16) && !(h2 & 16));
    }
}
int main(void)
{
    web_nor_erase();
    web_boot();
    run_ms(2700);
    vectors();
    seeds();
    web_gen(5, 421, 70, 0);
    run_ms(200);
    check("GEN: the piece's VIS (seed, genre, energy; recipe 0)",
          (vis_id[0] | vis_id[1] << 8) == 421 && vis_id[2] == 5 && vis_id[3] == 70 && vis_id[4] == 0);
    web_levels();
    web_play(1);
    run_ms(2000);
    frames();
    usb_pushes();
    fullscreen();
    saver();
    project();
    printf("visgen: %s\n", nfail ? "FAILED" : "all ok");
    return nfail != 0;
}
