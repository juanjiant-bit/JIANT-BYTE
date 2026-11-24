/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (1.4) The UI push (editor.c ED_UI, 43; web/EDITOR_PROTOCOL.md): with WATCH bit 5 a page change, a held layer and a
 * button go out as one coalesced push a UI frame (section, page, layer, button, KNOB 1..4's labels); without it nothing.
 * The UI of ui_test.c (its frames: input, then the drawing) with the editor's handler over the USB ring of usb.c */
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
    printf("uipush: %-78s %s\n", what, c ? "ok" : "FAIL");
    bad += !c;
}
static void request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    ota_frame_done();
    sysex_byte(0xF0); sysex_byte(ED_HDR0); sysex_byte(ED_HDR1); sysex_byte(ED_HDR2); sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++) sysex_byte(a[i]);
    sysex_byte(0xF7);
    ed_service(); uw_drain();
}
/* frames as main.c runs them: the editor's service between them (every 5 ms: three passes a 16 ms frame) */
static void ef(uint32_t k)
{
    uint32_t i, j;
    for (i = 0; i < k; i++) {
        frame();
        for (j = 0; j < 3u; j++) { fm1_ms += 5u; ed_w.last_ms = fm1_ms; ed_service(); uw_drain(); }
    }
}
static void ebtn(uint32_t label, int down)
{
    if (down) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
    else fm1_in.buttons &= ~(1u << panel.btn[label]);
}
/* the UI pushes on the wire from position from: count, the last one's start */
static uint32_t ui_pushes(uint32_t from, uint32_t *last)
{
    uint32_t i, c = 0;
    for (i = from; i + 5u <= uw_n; i++)
        if (uw[i] == 0xF0 && uw[i + 1] == ED_HDR0 && uw[i + 2] == ED_HDR1 && uw[i + 3] == ED_HDR2 && uw[i + 4] == ED_UI) {
            c++;
            *last = i;
        }
    return c;
}
typedef struct { uint32_t sec, ly, btn, end; char page[24], lbl[4][16]; } uip_t;
static uint32_t rd_str(uint32_t p, char *s, uint32_t max)
{
    uint32_t k = 0;
    while (uw[p] && uw[p] != 0xF7 && k + 1u < max) s[k++] = (char)uw[p++];
    s[k] = 0;
    return uw[p] == 0 ? p + 1u : p;
}
static int parse(uint32_t p, uip_t *u)
{
    uint32_t c;
    p += 5u;
    u->sec = uw[p++];
    p = rd_str(p, u->page, sizeof u->page);
    u->ly = uw[p++];
    u->btn = uw[p++];
    for (c = 0; c < 4u; c++)
        p = rd_str(p, u->lbl[c], sizeof u->lbl[c]);
    u->end = p;
    return uw[p] == 0xF7;
}
static void label_of(uint32_t c, char *b)                /* KNOB c's label as its card drew it */
{
    uint32_t i;
    for (i = 0; ui.col[c][i] && ui.col[c][i] != '|' && i < 11u; i++) b[i] = ui.col[c][i];
    b[i] = 0;
}

int main(void)
{
    uint32_t at, last = 0, n;
    uip_t u;
    uint8_t a[1];
    ui_power_on(); frames(200);
    usb.config = 1; ed_w.last_ms = fm1_ms;

    /* WATCH without bit 5: nothing */
    a[0] = 1 | 4 | 8 | 16; uw_n = 0; request(ED_WATCH, a, 1);
    ok("WATCH 29: the reply without bit 5 (29)", uw_n == 7u && uw[5] == 29u);
    at = uw_n;
    ebtn(B_EDIT, 1); ef(2); ebtn(B_EDIT, 0); ef(6);
    ok("without bit 5: a page change and a button push no UI", ui_pushes(at, &last) == 0u && cur_page()->fam == FAM_EDIT);

    /* WATCH 63: bit 5 taken, the screen as it is first */
    a[0] = 63; uw_n = 0; request(ED_WATCH, a, 1);
    ok("WATCH 63: the reply says bit 5 (63)", uw_n == 7u && uw[5] == 63u);
    at = uw_n; ef(3);
    n = ui_pushes(at, &last);
    ok("bit 5 asked for: one push, the screen as it is", n == 1u && parse(last, &u) && u.sec == SEC_EDIT &&
       str_eq(u.page, nav_page_name()) && u.ly == 0u && u.btn == 127u);
    at = uw_n; ef(10);
    ok("nothing changes: no push", ui_pushes(at, &last) == 0u);

    /* a page change (SEQ pressed and let go in the same frame): one push, after the cards were drawn */
    at = uw_n;
    ebtn(B_SEQ, 1); frame(); ebtn(B_SEQ, 0); ef(4);
    n = ui_pushes(at, &last);
    {
        char l0[16], l1[16], l2[16], l3[16];
        label_of(0, l0); label_of(1, l1); label_of(2, l2); label_of(3, l3);
        ok("SEQ: one coalesced push", n == 1u);
        ok("its fields: SEQ's section, the page's name, no layer, SEQ let go", parse(last, &u) && u.sec == SEC_SEQ &&
           str_eq(u.page, nav_page_name()) && u.ly == 0u && u.btn == (B_SEQ | 64u));
        ok("its labels: the cards' as drawn", str_eq(u.lbl[0], l0) && str_eq(u.lbl[1], l1) && str_eq(u.lbl[2], l2) &&
           str_eq(u.lbl[3], l3) && (l0[0] || l1[0] || l2[0] || l3[0]));
        printf("uipush: (SEC %u \"%s\" layer %u button %u: \"%s\" \"%s\" \"%s\" \"%s\", %u bytes)\n", (unsigned)u.sec, u.page,
               (unsigned)u.ly, (unsigned)u.btn, u.lbl[0], u.lbl[1], u.lbl[2], u.lbl[3], (unsigned)(u.end + 1u - last));
    }

    /* a button pressed (held): its push; let go: another */
    at = uw_n; ebtn(B_FX, 1); ef(3);
    n = ui_pushes(at, &last);
    ok("FX held: a push with FX pressed and its layer held", n >= 1u && parse(last, &u) && u.btn == B_FX &&
       u.ly == LAYER_FX && ui.ly == LAYER_FX);
    at = uw_n; ebtn(B_FX, 0); ef(4);
    n = ui_pushes(at, &last);
    ok("FX let go: one push, FX | 64, no layer", n == 1u && parse(last, &u) && u.btn == (B_FX | 64u) && u.ly == 0u);

    /* the same page, a button tapped: a push for the button (each press or release counts, the same state again too) */
    at = uw_n; ebtn(B_PLAY, 1); frame(); ebtn(B_PLAY, 0); ef(4);
    n = ui_pushes(at, &last);
    ok("PLAY tapped: one push, PLAY let go", n == 1u && parse(last, &u) && u.btn == (B_PLAY | 64u));
    ebtn(B_PLAY, 1); frame(); ebtn(B_PLAY, 0); ef(4);

    /* WATCH 31 again (bit 5 off): nothing more */
    a[0] = 31; uw_n = 0; request(ED_WATCH, a, 1);
    at = uw_n; ebtn(B_EDIT, 1); ef(2); ebtn(B_EDIT, 0); ef(6);
    ok("WATCH 31 after 63: no UI push", ui_pushes(at, &last) == 0u);
    printf("uipush: %s\n", bad ? "FAIL" : "all ok");
    return bad != 0;
}
