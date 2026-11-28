/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE v1.1b2, editor protocol 1.6) JIANT Studio's additions through the editor's own handler over the USB ring (as
 * uipush_test.c): INFO 'D', the whole step (STEP_GET / STEP_SET / TRACK_STEP flag 2), a song row's scene, sources and
 * variations (SONG ops 4, 5), MOD AMT -128..127 (its stored byte, motion's clamp, the scaling). The project library
 * (95..97) is tests/backup_test.c's (it needs the simulated flash) */
#define FELUCCA_OTA 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint8_t uw[1u << 16];   /* the wire: what the device sent */
static uint32_t uw_n;
static void uw_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && uw_n < sizeof uw; i++)
            uw[uw_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { uw_drain(); fm1_ms++; }
static uint32_t flash_ok = 1;
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static int fl_erase4k(uint32_t off, uint32_t *took) { (void)off; *took = 0; return -1; }
static int fl_write(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_read(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
#include "../firmware/src/storage.c"
static uint8_t scr_on;
#include "../firmware/src/editor.c"

static int bad;
static void ok(const char *what, int c)
{
    printf("studio: %-78s %s\n", what, c ? "ok" : "FAIL");
    bad += !c;
}
/* a request; the reply's payload (after cmd) -> r, its length (0: no reply) */
static const uint8_t *r;
static uint32_t request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    ota_frame_done();
    uw_n = 0;
    sysex_byte(0xF0); sysex_byte(ED_HDR0); sysex_byte(ED_HDR1); sysex_byte(ED_HDR2); sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++) sysex_byte(a[i]);
    sysex_byte(0xF7);
    ed_service(); uw_drain();
    for (i = 0; i + 6u <= uw_n; i++)
        if (uw[i] == 0xF0 && uw[i + 1] == ED_HDR0 && uw[i + 4] == cmd) {
            uint32_t k = i + 5u;
            r = uw + k;
            while (k < uw_n && uw[k] != 0xF7) k++;
            return k - (i + 5u);
        }
    r = uw;
    return 0;
}
static void v14(uint8_t *p, int32_t v) { p[0] = (uint8_t)((v + 8192) & 127); p[1] = (uint8_t)((v + 8192) >> 7); }
static int32_t rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }

/* (1.6) the whole step */
static void whole_step(void)
{
    uint8_t a[64], b[64];
    uint32_t n, i;
    step_t *s = &TSEL->step[3], was;
    memset(s, 0, sizeof *s);
    s->n = 1; s->note[0] = 60; s->time = ST_NOTE; s->vel = 100;
    s->comp = CO_MAKE(CA_RATCH, CC_NM); s->carg = 0x8B; s->comp2 = CO_MAKE(CA_TRANS, CC_P50); s->carg2 = 0xE5;
    s->clen[0] = 3; s->clen[1] = 0; s->cnm[0] = CNM_MAKE(2u, 3u); s->cnm[1] = 0xF1;
    st_micro_set(s, -7);
    s->ctime = (uint8_t)(2u | 5u << 3);
    ST_EVERY_SET(s, EV_1_3);
    a[0] = 3; a[1] = 2;
    n = request(ED_STEP_GET, a, 2);
    ok("STEP_GET flag 2: index, 13 bytes, 8 lane modes, then 20 (42)", n == 42u && r[0] == 3u);
    ok("  MICRO + 32, LEN, DIV, EVERY", r[22] == 25u && r[23] == 5u && r[24] == 2u && r[25] == EV_1_3);
    ok("  the components as v14: comp carg comp2 carg2 clen0 clen1 cnm0 cnm1",
       rv(r + 26) == s->comp && rv(r + 28) == 0x8B && rv(r + 30) == s->comp2 && rv(r + 32) == 0xE5 && rv(r + 34) == 3 &&
       rv(r + 36) == 0 && rv(r + 38) == s->cnm[0] && rv(r + 40) == 0xF1);
    n = request(ED_STEP_GET, a, 1);
    ok("STEP_GET of before: 14 bytes as ever", n == 14u);
    a[1] = 1; n = request(ED_STEP_GET, a, 2);
    ok("STEP_GET flag 1: 22 bytes as ever", n == 22u);
    a[1] = 3; n = request(ED_STEP_GET, a, 2);
    ok("STEP_GET flag 3: no reply", n == 0u);

    /* a write of the whole step: the read's own bytes, changed */
    a[1] = 2; request(ED_STEP_GET, a, 2);
    memcpy(b, r, 42);
    b[22] = 32 + 31; b[23] = 0; b[24] = 5; b[25] = EV_FIRST;
    v14(b + 26, CO_MAKE(CA_OCTU, CC_FIRST)); v14(b + 28, 2); v14(b + 30, 0); v14(b + 32, 0);
    v14(b + 34, 31); v14(b + 36, CL_LEGACY); v14(b + 38, 0); v14(b + 40, 0x37);
    n = request(ED_STEP_SET, b, 42);
    ok("STEP_SET 42 bytes: the reply is the whole step (42)", n == 42u && !memcmp(r, b, 42));
    ok("  stored: MICRO +31, LEN 0, DIV 5, EVERY FIRST, the components", st_micro(s) == 31 && CT_LEN(s) == 0u &&
       CT_DIV(s) == 5u && ST_EVERY(s) == EV_FIRST && s->comp == CO_MAKE(CA_OCTU, CC_FIRST) && s->carg == 2u &&
       !s->comp2 && s->clen[0] == 31u && s->clen[1] == CL_LEGACY && s->cnm[1] == 0x37u && s->note[0] == 60u);
    was = *s;
    for (i = 0; i < 6u; i++) {                           /* each value out of its range: refused, nothing written */
        memcpy(a, b, 42);
        if (i == 0) a[22] = 64;
        if (i == 1) a[23] = 16;
        if (i == 2) a[24] = CT_NDIV;
        if (i == 3) a[25] = EV_N;
        if (i == 4) v14(a + 28, 256);
        if (i == 5) v14(a + 34, CL_MAX);
        n = request(ED_STEP_SET, a, 42);
        ok("STEP_SET with a value out of its range: no reply, the step as it was", !n && !memcmp(&was, s, sizeof was));
    }
    memcpy(a, b, 22); a[1] = 1; a[2] = 62;
    n = request(ED_STEP_SET, a, 22);
    ok("STEP_SET of before (22): the note, MICRO and the components kept", n == 22u && s->n == 1u && s->note[0] == 62u &&
       st_micro(s) == 31 && s->comp == was.comp && s->clen[1] == CL_LEGACY && s->ctime == was.ctime);

    /* TRACK_STEP: any track's */
    memset(&trk[1].step[7], 0, sizeof(step_t));
    a[0] = 1; a[1] = 7; a[2] = 2;
    n = request(ED_TRACK_STEP, a, 3);
    ok("TRACK_STEP flag 2: track, index, 13, 8, 20 (43)", n == 43u && r[0] == 1u && r[1] == 7u && r[23] == 32u);
    a[0] = 1; a[1] = 7; memcpy(a + 2, b + 1, 41);
    n = request(ED_TRACK_STEP, a, 43);
    ok("TRACK_STEP 43 bytes: written, the selection kept", n == 43u && st_micro(&trk[1].step[7]) == 31 &&
       trk[1].step[7].clen[0] == 31u && trk[1].step[7].cnm[1] == 0x37u && song.sel == 0u);
}

int main(void)
{
    uint32_t n, i;
    ui_power_on(); frames(50);
    usb.config = 1; ed_w.last_ms = fm1_ms;
    n = request(ED_INFO, 0, 0);
    for (i = 0; i + 3u < n && !(r[i] == 0x44 && r[i + 1] == 1 && r[i + 3] == 0x43); i++)
        ;
    ok("INFO: 44 01 caps right before 43 02 01 06", i + 7u <= n && r[i + 2] == ED_STUDIO_CAPS && r[i + 4] == 2 &&
       r[i + 5] == 1 && r[i + 6] == 6);
    whole_step();
    printf("studio: %s\n", bad ? "FAIL" : "all ok");
    return bad != 0;
}
