/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Charles Vestal (X0X)
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The design (the device clock driven by the audio the worklet asks for, the exports below, the saved
 * flash handed back before boot) follows X0X's browser build, web/emu/x0x_web.c of charlesvestal/fm1-x0x
 * (GPL-3.0), a Felucca fork. */
/* Felucca in the browser: the firmware's sources, as the host tests build them (tests/hostsim.c: libc,
 * engines, voices, FX, sequencer; then the UI, main.c, storage and projects, in src/felucca.c's order),
 * compiled to WebAssembly and run in an AudioWorklet against a simulated FM-1:
 *   - the device clock: TIMER4 ticks (24 MHz) and fm1_ms advance with the audio the worklet asks for; the
 *     audio ISR (audio.c fm1_alnk0_irq, the real one) renders each I2S half (HALF_FRAMES) as the DMA would
 *     ask for it, and the main loop (main.c's, below in web_frame) runs every 15 ms, with ui_input on every
 *     millisecond in between as main.c's wait loop calls it;
 *   - the panel: buttons and encoders by their printed labels (panel.c's table maps them to the matrix),
 *     the keys as fm1_in.notes, the LEDs read back from fm1_led / fm1_led_dim through the key matrix;
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order);
 *   - the flash: a 1 MiB NOR image behind storage.c's hooks (settings, projects, user presets, the user
 *     sample slots read through it as XIP); the page keeps its storage sectors and gives them back at boot.
 * Not here: USB (MIDI, audio, the editor, updates), TRS MIDI, the serial console, the battery (full), the
 * crash / UBOOT paths. Nothing of the firmware changes for this file.
 *   web/emu/build.sh  ->  build/emu/{felucca.wasm, worklet.js, index.html} */
#include <stdint.h>

#define FELUCCA_WEB 1
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                    /* as src/felucca.c: MENU > USB SERIAL is there (nothing behind it) */
#endif
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "WEB"
#endif

static uint8_t nor[0x100000];            /* the SPI NOR (erased: 0xFF) */
/* (studio) JIANT Studio records the emulator's MIDI OUT (usb.c midi_out_event queues it without a USB host) */
static unsigned char web_mo_tap;
#define USB_MIDI_TAP web_mo_tap
#define main hostsim_main
#define HOST_OWN_TICKS 1                    /* (fm1_ticks below) */
#include "../../tests/hostsim.c"
#undef main

/* ------------------------------------------------------------ the clock --- */
#define FM1_TICKS_PER_US 24u             /* TIMER4: 24 MHz */
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ------------------------------------------------------ audio (ALNK0) --- */
static uint32_t web_half;                /* the half the next render fills */
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
static void fm1_audio_stop(void) {}
static void fm1_audio_unmute(void) {}
void isr_alnk0(void) {}
/* (studio) the stems live, as MENU > USB STEMS sends them (fx.c stem_buf: the six parts mono, before the master;
 * stem_fx: the FX return L R), each sample at its place in the output (stem_ring by the sample's number), for JIANT
 * Studio's recorder: web_stems(1) on, web_render puts them in web_stem_out() beside the stereo */
#define STEM_CH 8u
#define STEM_RING 4096u
static float stem_ring[STEM_RING][STEM_CH], stem_out[STEM_CH][1024];
static uint32_t stem_w, stem_from;
static uint8_t web_stems_on;
static void web_stem_tap(uint32_t n)
{
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        float *s = stem_ring[(stem_w + i) % STEM_RING];
        for (k = 0; k < 6u; k++)
            s[k] = (float)stem_buf[k][i] / 32768.0f;
        s[6] = (float)stem_fx[0][i] / 32768.0f;
        s[7] = (float)stem_fx[1][i] / 32768.0f;
    }
    stem_w += n;
}
#define WEB_STEM_TAP(n) web_stem_tap(n)
#include "../../firmware/src/audio.c"

/* ------------------------------------------------------------ the panel --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h: key id at (column, row bit) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL], fm1_led_hi[FM1_NCOL];
static uint32_t web_dim_lo;
static void fm1_led_dim_level(uint32_t lo) { web_dim_lo = lo; }
/* the power-on LED sweep (hal/fm1_input.h fm1_led_anim_start, the picture hal/fm1_led_anim.h): its frame from the
 * clock (a scan frame 1.1 ms), the page draws each LED's level (web_anim_levels); at its end, or on a key or a button
 * down, the last picture (the glow everywhere, or dark) and the UI's LEDs from the next frame, as the scan does */
#include "../../firmware/hal/fm1_led_anim.h"
static uint32_t web_anim_t0, web_anim_glow, web_anim_run;
static uint32_t web_anim_frame(void) { return (web_ticks - web_anim_t0) / (1100u * FM1_TICKS_PER_US); }
static void fm1_led_anim_start(uint32_t glow)
{
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
    memset(fm1_led_mid, 0, sizeof fm1_led_mid);
    memset(fm1_led_hi, 0, sizeof fm1_led_hi);
    web_anim_t0 = web_ticks;
    web_anim_glow = glow != 0u;
    web_anim_run = 1;
}
static int fm1_led_anim_on(void)
{
    if (web_anim_run && (web_anim_frame() >= FM1_ANIM_FRAMES || fm1_in.notes || fm1_in.buttons)) {
        uint32_t c, r;
        web_anim_run = 0;
        for (c = 0; c < FM1_NCOL; c++)
            for (r = 1; r < 5u; r++)
                if (web_anim_glow && FM1_KEYMAP[r][c] >= 0)
                    fm1_led_dim[c] |= (uint8_t)(1u << r);
    }
    return (int)web_anim_run;
}
static uint32_t web_pressed, web_released, web_notes_pressed;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released)
        *released = web_released;
    web_pressed = web_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = web_notes_pressed; web_notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = web_enc_steps[e % 7u]; web_enc_steps[e % 7u] = 0; return s; }
static void fm1_input_init(void) {}
static void fm1_input_tick(void) {}
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}

/* ----------------------------------------------------------- the screen --- */
static uint16_t web_fb[240 * 240];       /* RGB565 as the panel gets it (high byte first) */
static uint32_t web_draws;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = sw;
    web_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s)              /* MENU > SCREEN OFF (lcd.c): off, the page dark; on: redrawn before */
{
    if (!s) {
        memset(web_fb, 0, sizeof web_fb);
        web_draws++;
    }
}
static void lcd_wake_now(void) {}
static void lcd_init(void) {}
static void lcd_panel(uint32_t m) { (void)m; }   /* (MENU > DISPLAY > PANEL: the browser has no panel) */

#include "../../firmware/src/gfx.c"
#include "../../firmware/src/panel.c"
#include "../../firmware/src/ui.c"
#include "../../firmware/src/menu_items.c"
#include "../../firmware/src/icons.c"
#include "../../firmware/src/ui_graph.c"
#include "../../firmware/src/ui_draw.c"
#include "../../firmware/src/ui_menu.c"
#include "../../firmware/src/ui_input.c"
#include "../../firmware/src/ui_layer.c"

/* ------------------------------------------------------------ the flash --- */
static uint32_t web_flash_writes;
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off >= sizeof nor || 4096u > sizeof nor - off)
        return -8;
    memset(nor + (off & ~4095u), 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)       /* NOR: a write only clears bits */
{
    const uint8_t *s = src;
    uint32_t i;
    if (off >= sizeof nor || n > sizeof nor - off)
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }             /* the FM-1's part (project.c) */
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../../firmware/src/storage.c"
#include "../../firmware/src/upreset.c"
#include "../../firmware/src/project.c"

/* ------------------------------------------------- main.c and its HAL --- */
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets, sp, usp, ssp, other_pc, other, cnum, bus[8], app; } fm1_crash_t;   /* (0.9.9-stab: as hal/fm1_irq.h's) */
#define FM1_CRASH_MAGIC 0x43525348u
static fm1_crash_t fm1_crash;                     /* (none: main.c boot_leds runs the sweep) */
static struct { uint8_t p3_rst, wdt_con; uint32_t rst_src; } fm1_boot;
uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];
/* (0.6.80) the RAM code and the low buffers (app.ld, JIANT 0.6.78): none here; each start the same array as its end,
 * so main.c's copy and clear loops do nothing */
#define _hot_start _hot_end
#define _low_start _low_end
uint32_t _hot_end[1], _hot_load[1], _low_end[1];
char _etext[1];                                   /* (V10.6z5: main.c unpacks the RAM code from there; nothing here) */
static uint32_t web_master_adc = 700;   /* MASTER, 0..1023 as the ADC reads it */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? (int32_t)web_master_adc : 620; }
static void fm1_adc_init(void) {}
static void fm1_timer5_ack(void) {}
static void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)isr; (void)prio; }
void isr_timer5(void) {}
static void fm1_reboot(void) {}
static void fm1_core1_stop(void) {}               /* (0.6.71: no second core here, FELUCCA_DUAL_CORE 0) */
static void fm1_enter_uboot(void) {}
static void fm1_guard_lock_top(void) {}
static void fm1_irq_enable_all(void) {}
static void fm1_irq_init(void) {}
static void fm1_time_init(void) {}
static void fm1_reset_reason(void) {}
static void fm1_wdt_arm(uint32_t v) { (void)v; }
static void fm1_mailbox_clear(void) {}
static void fm1_rails_boot(void) {}                /* (0.9.9-stab: the core supplies, hal/fm1_sys.h; none here) */
static uint8_t fm1_power_ready;                     /* (V10.11: the stock supply setup, hal/fm1_power.h; none here) */
static int fm1_power_init(void) { return 1; }
static void fm1_guard_enable(uint32_t m) { (void)m; }
enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
#if FELUCCA_CDC
static void cdc_task(void) {}
#endif
/* main.c's calls into the USB driver (registers): nothing here (usb.c's own functions stay as they are) */
#define usb_poll() ((void)0)
#define usb_start() ((void)0)
#define usb_retry(ms) ((void)(ms))
#define usb_detach() ((void)0)
#include "../../firmware/src/master.c"
#include "../../firmware/src/main.c"
#undef usb_poll
#undef usb_start
#undef usb_retry
#undef usb_detach

/* --------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame;

static uint32_t web_seed_v = 1u;          /* (JIANT 0.4) the page's random seed before web_boot (worklet.js), else 1 */
/* main.c fm1_main's boot, up to the main loop (no USB, no UART, no panel setup) */
static void web_power_on(void)
{
    psq_boot();                           /* (main.c: PUNCH SEQ's state, before the project's load) */
    persist_boot();
    settings_init();
    prefs_update();                       /* main.c (JIANT 0.6.44): FX LATCH ON by default (0.6.58: here too) */
    usb_serial_apply();
    lcd_init();
    spl_anim = 1;                         /* main.c (JIANT 0.5): the signature written */
    draw_splash();                        /* main.c: the splash (ui_draw.c) */
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    felucca_dbg.magic = DBG_MAGIC;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    felucca_init();
    master_boot();                        /* main.c (#137): the pot's level from the first block */
    kb_boot_hold = 1;                     /* main.c (#137): the keys silent under the splash */
    audio_init();
    boot_leds();                          /* main.c: the power-on LED sweep, from the scan's start */
    autosave_boot(boot_clean);            /* main.c: the last session's music (1.2, project.c) */
    if (!(ui_prefs & PREF_RESTORE_ON))
        song_boot_blank();                    /* (BYTE 0.17) RESTORE LAST OFF: a new project, not song 1 */
    session_dice(web_seed_v);             /* main.c (JIANT 0.4): this session's macros (the page's seed: web_seed) */
    web_boot_ms = fm1_ms + 30u + SPL_WRITE + SPL_HOLD;   /* main.c: 30 ms + the splash before the first frame */
    web_booted = 1;
}

/* one pass of main.c fm1_main's loop (what the browser has: no USB, editor, UBOOT, console) */
static void web_frame(void)
{
    int32_t b = fm1_adc_read(FM1_ADC_BATT);
    if (b > 0)
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    master_poll();
    felucca_dbg.ui_frames++;
    ui_input();
    settings_poll();
    autosave_poll();
    song_poll();
    ui_leds();
    ui_draw();
}

/* output ring, stereo float, filled by web_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

/* one millisecond of the device: the clock, the audio halves due by now, the main loop */
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += FS;
    if (web_stems_on && !fx_stems)                        /* (studio) the stems on while Studio records them (the menu's */
        fx_stems = 1;                                     /* USB setting sets it back each frame) */
    while (web_samples_due >= 1000u * HALF_FRAMES) {      /* an I2S half: the audio ISR renders it */
        uint32_t k;
        const int32_t *o = &abuf[web_half * HALF_WORDS];
        web_samples_due -= 1000u * HALF_FRAMES;
        fm1_alnk0_irq();
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            ring[i] = (float)o[2u * k] / 8388608.0f;
            ring[i + 1] = (float)o[2u * k + 1u] / 8388608.0f;
        }
        web_half ^= 1u;
    }
    if (web_booted && (int32_t)(fm1_ms - web_boot_ms) < 0 && !(fm1_ms % 20u))   /* main.c: the signature written */
        splash_write(SPL_WRITE + SPL_HOLD - (web_boot_ms - fm1_ms));
    if (!web_booted || (int32_t)(fm1_ms - web_boot_ms) < 0)
        return;
    if (web_boot_ms) {                                    /* main.c: the splash goes, the loop starts */
        web_boot_ms = 0;
        kb_boot_hold = 0;
        lcd_fill(0, 0, 240, 240, T_BG);
        web_last_frame = fm1_ms - 15u;
    }
    if (fm1_ms - web_last_frame >= 15u) {                /* main.c: a frame, then ui_input until 15 ms are over */
        web_last_frame = fm1_ms;
        web_frame();
    } else {
        ui_input();                                       /* main.c's wait loop */
    }
}

#define EXPORT __attribute__((used, visibility("default")))
#undef __attribute__

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t c, at = ring_r, i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
        if (web_stems_on)                                 /* (studio) its stems: silent before they were on */
            for (c = 0; c < STEM_CH; c++)
                stem_out[c][k] = (int32_t)(at - stem_from) >= 0 ? stem_ring[at % STEM_RING][c] : 0.0f;
    }
}
/* (studio) the stems: on (their samples from the next one rendered) or off; web_stem_out(c) a channel's n samples */
EXPORT void web_stems(uint32_t on)
{
    web_stems_on = (uint8_t)(on != 0u);
    if (web_stems_on) {
        stem_w = stem_from = ring_w;                      /* (between halves: the next half's first sample) */
        if (!fx_stems)
            fx_stems = 1;
    }
}
EXPORT float *web_stem_out(uint32_t c) { return stem_out[c < STEM_CH ? c : 0]; }
/* (studio) MIDI OUT as the FM-1 sends it over USB (MENU > CONTROL > MIDI OUT: KEYS, CLOCK, ALL), read by Studio:
 * web_midi_tap(1) on; web_midi_take() the next USB-MIDI packet (0: none) */
EXPORT void web_midi_tap(uint32_t on) { web_mo_tap = (unsigned char)(on != 0u); mo_r = mo_w; }
EXPORT uint32_t web_midi_take(void)
{
    uint32_t p;
    if (mo_r == mo_w)
        return 0;
    p = midi_out_q[mo_r % MQ];
    mo_r++;
    return p;
}

/* the flash: erased, then the page's saved sectors (web_nor), then boot */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
EXPORT void web_seed(uint32_t s) { web_seed_v = s; }   /* (JIANT 0.4) the session's macros: a seed of the page's */
EXPORT void web_boot(void)
{
    if (!web_booted)
        web_power_on();
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 */
static uint32_t web_btn_labels, web_key_mask;
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    web_btn_labels = labels;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    web_pressed |= m & ~old;
    web_released |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    web_key_mask = mask;
    web_notes_pressed |= mask & ~old;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_enc_steps[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
/* a channel message from Web MIDI, as a USB-MIDI packet arrives (usb.c midi_enqueue): 0 = the ring was full */
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 1;
    return (uint32_t)midi_enqueue((status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}

/* the LEDs: bit b = the button labelled b lit (bit 14: PLAY's green), keys bit k; the dim glow and the breath alike */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}
EXPORT uint32_t web_lit_buttons(void)
{
    return led_buttons(fm1_led) | ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) << 14;
}
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT uint32_t web_dim_level(void) { return web_dim_lo; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_breath); }   /* #119: dark .. ~60 % of lit */
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_breath); }
EXPORT uint32_t web_mid_keys(void) { return led_keys(fm1_led_mid); }   /* the DRUM grid's beats: a steady mid level */
/* the power-on sweep running: each LED's level now (hal/fm1_led_anim.h: 0..64 /64 of lit, | 0x80 the glow under
 * it), 0..13 the buttons by label, 14..40 the keys F3..G5; 0 when it is not */
EXPORT uint8_t *web_anim_levels(void)
{
    static uint8_t lv[FM1_ANIM_NBTN + FM1_ANIM_NKEY];
    uint32_t f = web_anim_frame(), i;
    if (!fm1_led_anim_on())
        return 0;
    for (i = 0; i < FM1_ANIM_NBTN + FM1_ANIM_NKEY; i++)
        lv[i] = (uint8_t)fm1_anim_level(f, i < FM1_ANIM_NBTN ? (i < NB ? panel.btn[i] : i) : i, web_anim_glow);
    return lv;
}

/* (0.6.91) the song generator from the page or a bench (web/emu/gen_bench.mjs): a genre (0 RANDOM, 1 + the genre's
 * index, 0.8.6: web_gen_count() + 1 EXPLORE), a seed 0..9999, an energy 0..100, a form (0 LOOP, 1 PARTS, 2 SONG); the
 * piece's name after */
EXPORT void web_gen(uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form)
{
    gp.genre = (uint8_t)(genre % (NGENRE + 2u));
    gp.seed = (uint16_t)(seed % 10000u);
    gp.energy = (uint8_t)(energy > 100u ? 100u : energy);
    gp.form = (uint8_t)(form % 3u);
    gen_go();
    gp.sig = gen_sig();
}
EXPORT const char *web_gen_last(void) { return gp.last; }
EXPORT uint32_t web_gen_count(void) { return NGENRE; }
EXPORT const char *web_gen_name(uint32_t i) { return GENRE[i % NGENRE].name; }

static uint32_t web_rg_salt;                            /* (v1.1b2: web_gen_cmd 5 / 6's last salt, web_regen) */
/* (studio) JIANT Studio's RADIO page: the generator's state as eight numbers (v1.1b2: nine, [8] the salt web_gen_cmd 5
 * or 6 used last), a static the page reads: radio 0/1, the
 * genre (-1 EXPLORE, -2 RANDOM, else web_gen_genre's index), the seed, MOOD -50..50, ENERGY 0..100, FORM (0 LOOP,
 * 1 PARTS, 2 SONG, 3 MY LOOP), the piece's number (gen.c web_gen_n: every piece made, OCT+'s, RADIO's, the page's),
 * playing */
EXPORT int32_t *web_gen_info(void)
{
    static int32_t o[9];
    o[8] = (int32_t)web_rg_salt;                        /* (v1.1b2) the salt web_gen_cmd 5 / 6 used last */
    o[0] = grd.on != 0;
    o[1] = gp.genre == GEN_EXPLORE ? -1 : gp.genre ? (int32_t)((gp.genre - 1u) % NGENRE) : -2;
    o[2] = gp.seed;
    o[3] = grd.mood;
    o[4] = gp.energy;
    o[5] = gp.form;
    o[6] = (int32_t)web_gen_n;
    o[7] = song.playing || chain_busy() || seq_counting();
    return o;
}
#if FELUCCA_VIS
/* (visgen) JIANT Studio's VIS: VIS_SONG's fields, VIS_TICK's, playing (docs/VISGEN.md; ui_vis.c vis_state); without
 * it (FELUCCA_VIS 0) the worklet answers "This engine has no VIS." */
EXPORT int32_t *web_vis_info(void)
{
    static int32_t o[21];
    vis_state(o);
    return o;
}
#endif
EXPORT const char *web_gen_genre(uint32_t i) { return i < NGENRE ? GENRE[i].name : 0; }

/* (v1.1b2) JIANT Studio: the generator's whole state, what makes the piece (gen.c gen_sig and what gen_make reads beside
 * it), so a project made by GEN is made again identically from it: web_gen_state() the 19 numbers as they are (a static
 * the page reads, and writes for web_gen_set_state):
 *   [0] 1 (the layout)  [1] genre (0 RANDOM, 1 + the genre's index, web_gen_count() + 1 EXPLORE)  [2] seed 0..9999
 *   [3] energy 0..100  [4] form 0 LOOP 1 PARTS 2 SONG  [5..9] the parts' amounts DRUMS BASS CHORDS ARP LEAD (0..100,
 *   255 the genre's)  [10] complex 0..100  [11] swing 0..100 (255 the genre's)  [12] human 0..100  [13] mood -50..50
 *   [14] the tracks live (4 or 6: MENU TRACKS)  [15] a RADIO piece's tempo before it (the one MOOD DANCE kept, gen.c
 *   gmood_bpm), -1 none  [16] BPM LOCK (MENU) 0/1  [17] the tempo (G_BPM)  [18] RADIO on 0/1 */
#define WEB_GEN_NS 19u
_Static_assert(GP_N == 5u, "web_gen_state: five amounts");
/* (v1.1b4) after the 19, the regenerations since the piece (web_gen_cmd 5 / 6), so a restore replays them: [19] how many
 * (0..WEB_RG_LOG; -1 more than that: the first WEB_RG_LOG kept, not replayed), then a triple each, [20 + 3 i] the kind
 * (5 a track, 6 a section), [21 + 3 i] its index, [22 + 3 i] its salt. web_gen_state_len() the whole length (44) */
#define WEB_RG_LOG 8u
#define WEB_GEN_NL (WEB_GEN_NS + 1u + 3u * WEB_RG_LOG)
static int32_t web_gst[WEB_GEN_NL];
static int32_t web_rg_log[1u + 3u * WEB_RG_LOG];        /* [0] the count (-1 full), then the triples */
static uint32_t web_rg_at = ~0u;                        /* web_gen_n after the last regeneration logged (another: a new piece) */
EXPORT uint32_t web_gen_state_len(void) { return WEB_GEN_NL; }
static void web_rg_note(uint32_t cmd, uint32_t which, uint32_t salt)   /* a regeneration done: into the log */
{
    int32_t n = web_rg_at == web_gen_n - 1u ? web_rg_log[0] : 0;   /* (its own gen_go counted one: the piece before) */
    if (n >= 0 && (uint32_t)n < WEB_RG_LOG) {
        web_rg_log[1 + 3 * n] = (int32_t)cmd; web_rg_log[2 + 3 * n] = (int32_t)which; web_rg_log[3 + 3 * n] = (int32_t)salt;
        web_rg_log[0] = n + 1;
    } else
        web_rg_log[0] = -1;
    web_rg_at = web_gen_n;
}
EXPORT int32_t *web_gen_state(void)
{
    uint32_t k;
    web_gst[0] = 1;
    web_gst[1] = gp.genre; web_gst[2] = gp.seed; web_gst[3] = gp.energy; web_gst[4] = gp.form;
    for (k = 0; k < GP_N; k++) web_gst[5 + k] = gp.amt[k];
    web_gst[10] = gp.complex; web_gst[11] = gp.swing; web_gst[12] = gp.human; web_gst[13] = grd.mood;
    web_gst[14] = (int32_t)trk_live;
    web_gst[15] = grd.rn && grd.mood > 10 ? grd.bpm0 : -1;
    web_gst[16] = (ui_prefs & PREF_BPM_LOCK) != 0;
    web_gst[17] = song.g[G_BPM];
    web_gst[18] = grd.on != 0;
    if (web_rg_at != web_gen_n) web_rg_log[0] = 0;     /* (a piece made since: no regeneration of it) */
    memcpy(web_gst + WEB_GEN_NS, web_rg_log, sizeof web_rg_log);
    return web_gst;
}
static uint32_t web_regen(uint32_t cmd, uint32_t which, uint32_t salt);
static int web_gen_amt_ok(int32_t v) { return (v >= 0 && v <= 100) || v == (int32_t)GA_AUTO; }
/* web_gen_state()'s numbers (written there by the page) into the generator; regen 1: the piece made from them (as
 * web_gen: identical to the one they were read after, its tempo too), (v1.1b4) then the regenerations of its log [19..]
 * replayed in order (identical to the piece the state was read from); 0 the settings alone. 0 done, 1 a number out of
 * its range (nothing changed), 2 made without its regenerations (the log was full, [19] -1). The log is consumed: a
 * page writing only the first 19 sets [19] 0 (worklet.js does) */
EXPORT uint32_t web_gen_set_state(uint32_t regen)
{
    const int32_t *s = web_gst;
    uint32_t k, lock = ui_prefs & PREF_BPM_LOCK;
    if (s[0] != 1 || s[1] < 0 || s[1] > (int32_t)NGENRE + 1 || s[2] < 0 || s[2] > 9999 || s[3] < 0 || s[3] > 100 ||
        s[4] < 0 || s[4] > 2 || !web_gen_amt_ok(s[10]) || s[10] == (int32_t)GA_AUTO || !web_gen_amt_ok(s[11]) ||
        !web_gen_amt_ok(s[12]) || s[12] == (int32_t)GA_AUTO || s[13] < -50 || s[13] > 50 || (s[14] != 4 && s[14] != NTRK) ||
        s[15] < -1 || s[15] > 300 || (uint32_t)s[16] > 1u || s[17] < 20 || s[17] > 300 || (uint32_t)s[18] > 1u)
        return 1;
    for (k = 0; k < GP_N; k++)
        if (!web_gen_amt_ok(s[5 + k])) return 1;
    if (s[19] < -1 || s[19] > (int32_t)WEB_RG_LOG) return 1;   /* (v1.1b4) the log: each entry checked first */
    for (k = 0; s[19] > 0 && k < (uint32_t)s[19]; k++) {
        const int32_t *e = s + WEB_GEN_NS + 1u + 3u * k;
        if ((e[0] != 5 && e[0] != 6) || e[1] < 0 || e[1] > (e[0] == 5 ? (int32_t)s[14] - 1 : 3) || e[2] <= 0 || e[2] > 0x0FFFFFFF)
            return 1;
    }
    gp.genre = (uint8_t)s[1]; gp.seed = (uint16_t)s[2]; gp.energy = (uint8_t)s[3]; gp.form = (uint8_t)s[4];
    for (k = 0; k < GP_N; k++) gp.amt[k] = (uint8_t)s[5 + k];
    gp.complex = (uint8_t)s[10]; gp.swing = (uint8_t)s[11]; gp.human = (uint8_t)s[12];
    grd.mood = (int8_t)s[13];
    trks_put(s[14] == NTRK);
    grd.on = (uint8_t)s[18];
    if (regen) {
        gsr.thrown = 0;                                 /* (the seed as given: never thrown again) */
        if (s[15] >= 0) { song.g[G_BPM] = (int16_t)s[15]; grd.on = 1; web_gen_rn = 1; }   /* (a RADIO piece: its tempo kept) */
        else if (s[16]) song.g[G_BPM] = (int16_t)s[17];
        if (s[16]) ui_prefs |= PREF_BPM_LOCK; else ui_prefs &= ~PREF_BPM_LOCK;
        gen_go();
        ui_prefs = (ui_prefs & ~PREF_BPM_LOCK) | lock;  /* (the player's MENU as it was) */
        grd.on = (uint8_t)s[18];
        web_gen_rn = 0;
        if (s[19] > 0) {                                /* (v1.1b4) the regenerations replayed, in their order */
            int32_t log[1u + 3u * WEB_RG_LOG];
            memcpy(log, s + WEB_GEN_NS, sizeof log);
            for (k = 0; k < (uint32_t)log[0]; k++)
                (void)web_regen((uint32_t)log[1 + 3 * k], (uint32_t)log[2 + 3 * k], (uint32_t)log[3 + 3 * k]);
        }
    }
    k = regen && s[19] < 0;                             /* (a log past WEB_RG_LOG: the piece alone, rc 2) */
    memset(web_gst + WEB_GEN_NS, 0, sizeof web_rg_log); /* (consumed: a 19-number write after it reads no log) */
    gp.sig = gen_sig();
    ui.force = 1;
    return k ? 2u : 0u;
}

/* (v1.1b2) web_gen_cmd 5 / 6: the piece made again from GEN's settings with a salt in the parts' streams (gen.c
 * web_gen_salt: the key, the progression, the rows and the tempo as they were), then of it only one track (5) or one
 * section (6) kept over what was there (the player's edits of the rest kept) */
static project_store_t web_rg_slot[4];
static project_t web_rg_old, web_rg_new;
static uint32_t web_rg_n;
static void web_rg_track(project_t *o, const project_t *n, uint32_t k)   /* track k of n into o */
{
    motion_store_t m = o->motion;
    uint32_t i, c = 0, dr = o->t[k].engine == ENGI_DRUM || n->t[k].engine == ENGI_DRUM;
    o->t[k] = n->t[k];
    memcpy(o->fm6[k], n->fm6[k], sizeof o->fm6[k]);
    o->dnz[k] = n->dnz[k]; o->cru[k] = n->cru[k]; o->life[LIFE_N + k] = n->life[LIFE_N + k];
    o->voice_limit[k] = n->voice_limit[k]; o->fit[k] = n->fit[k]; o->fpx[k] = n->fpx[k]; o->fbl[k] = n->fbl[k];
    memcpy(o->clay[k], n->clay[k], sizeof o->clay[k]); memcpy(o->clay_rec[k], n->clay_rec[k], sizeof o->clay_rec[k]);
    memcpy(o->fugue[k], n->fugue[k], sizeof o->fugue[k]); memcpy(o->lyr[k], n->lyr[k], sizeof o->lyr[k]);
    memcpy(o->fmo[k], n->fmo[k], sizeof o->fmo[k]); memcpy(o->fxr[k], n->fxr[k], sizeof o->fxr[k]);
    if (dr) {                                           /* (a DRUM part: its kit, the project's one kit) */
        memcpy(o->dx, n->dx, sizeof o->dx); o->dx_mute = n->dx_mute; memcpy(o->dx_mot, n->dx_mot, sizeof o->dx_mot);
        o->dx_tone = n->dx_tone; o->dx_hands = n->dx_hands; memcpy(o->dx_snd, n->dx_snd, sizeof o->dx_snd);
        memcpy(o->dx_lay, n->dx_lay, sizeof o->dx_lay); memcpy(o->dx_form, n->dx_form, sizeof o->dx_form);
        memcpy(o->dw, n->dw, sizeof o->dw); memcpy(o->dx_text, n->dx_text, sizeof o->dx_text);
        memcpy(o->dx_fmod, n->dx_fmod, sizeof o->dx_fmod); o->dwt = n->dwt;
    }
    for (i = 0; i < m.count; i++)                       /* its motion records the new ones, the others' as they were */
        if (MOTION_TRK(&m.event[i]) != k) o->motion.event[c++] = m.event[i];
    for (i = 0; i < n->motion.count && c < MOTION_MAX; i++)
        if (MOTION_TRK(&n->motion.event[i]) == k) o->motion.event[c++] = n->motion.event[i];
    o->motion.count = (uint8_t)c;
    o->motion.on = (uint8_t)((m.on & ~(1u << k)) | (n->motion.on & (1u << k)));
    o->sum = proj_sum(o);
}
/* (5) track tr (0..NTRK-1), (6) section sec (1..3; 0 A: the whole piece again, every section new): 0 done */
static uint32_t web_regen(uint32_t cmd, uint32_t which, uint32_t salt)
{
    static chain_config_t cc;
    static scene_t sc[CHAIN_ROWS];
    static uint32_t ts[CHAIN_ROWS], tv[CHAIN_ROWS];
    static int16_t g[G_COUNT];
    uint32_t x, used = 0, all = cmd == 6u && !which;
    if (cmd == 5u ? which >= trk_live : which > 3u) return 1;
    if (live.cur >= 0 && live.cur < 4) section_store((uint32_t)live.cur);   /* (the music as it is now: its part's) */
    for (x = 0; x < 4u; x++) used |= (uint32_t)project_used(x) << x;
    memcpy(web_rg_slot, proj_slot, sizeof web_rg_slot);
    cc = chain_config; memcpy(g, song.g, sizeof g);
    memcpy(sc, song_idx.scene[song_cur], sizeof sc); memcpy(ts, song_idx.tsrc[song_cur], sizeof ts);
    memcpy(tv, song_idx.tvar[song_cur], sizeof tv);
    web_rg_salt = salt ? salt : ++web_rg_n * 0x2F6Bu + 1u;
    web_gen_salt = web_rg_salt; web_gen_ssec = cmd == 5u || all ? 0xFFu : (uint8_t)which;
    gsr.thrown = 0;
    gen_go();
    web_gen_salt = 0; web_gen_ssec = 0xFFu;
    if (!all) {
        for (x = 0; x < 4u; x++) {
            if (cmd == 6u) {                            /* (6: every other section as it was) */
                if (x != which) memcpy(&proj_slot[x], &web_rg_slot[x], sizeof proj_slot[x]);
                continue;
            }
            if (!((used >> x) & 1u) || !proj_import(&web_rg_new, &proj_slot[x], sizeof proj_slot[x]) ||
                !proj_import(&web_rg_old, &web_rg_slot[x], sizeof web_rg_slot[x])) {
                memcpy(&proj_slot[x], &web_rg_slot[x], sizeof proj_slot[x]);   /* (no such part before, or none now) */
                continue;
            }
            web_rg_track(&web_rg_old, &web_rg_new, which);
            if (!proj_pack(&proj_slot[x], &web_rg_old)) memcpy(&proj_slot[x], &web_rg_slot[x], sizeof proj_slot[x]);
        }
        chain_config = cc;
        memcpy(song_idx.scene[song_cur], sc, sizeof sc); memcpy(song_idx.tsrc[song_cur], ts, sizeof ts);
        memcpy(song_idx.tvar[song_cur], tv, sizeof tv);
        if (project_used(0)) section_load(0);           /* (A live, as GEN leaves it) */
        memcpy(song.g, g, sizeof g);                    /* (the song's tempo and globals as they were) */
    } else
        song.g[G_BPM] = g[G_BPM];                       /* (A: everything new, the tempo kept) */
    web_rg_note(cmd, which, web_rg_salt);               /* (v1.1b4) the state's log: a restore replays it */
    proj_wire_gen++;
    ui.force = 1;
    return 0;
}
/* 0 RADIO OFF / ON (arg); 1 the next piece now (RADIO on: its advance, a new seed and on playing; else OCT+ on GEN);
 * 2 GENRE = arg (-1 EXPLORE, -2 RANDOM, else the genre's index); 3 MOOD = arg (-50..50); 4 PLAY (arg 1) / STOP (0);
 * (v1.1b2) 5 track arg & 15 made again (arg >> 4: the salt, 0 a new one; web_gen_info()[8] the one used), the rest as
 * it is; 6 section arg & 15 made again (1 B .. 3 D: the others as they are; 0 A: the whole piece, its sections all new),
 * the salt likewise. 5 and 6 keep the key and the tempo (and 5 / 6 B..D the rows and the song's globals) */
EXPORT void web_gen_cmd(uint32_t cmd, int32_t arg)
{
    if (cmd == 0u) grd.on = arg != 0;
    else if (cmd == 1u) {
        if (grd.on) { gen_new_seed(); grd.go = (uint8_t)transport_busy(); }
        gen_press();
    } else if (cmd == 2u) {
        if (arg == -1) gp.genre = (uint8_t)GEN_EXPLORE;
        else if (arg == -2) gp.genre = 0;
        else if (arg >= 0 && (uint32_t)arg < NGENRE) gp.genre = (uint8_t)(arg + 1);
    } else if (cmd == 3u) grd.mood = (int8_t)clamp(arg, -50, 50);
    else if (cmd == 4u) {
        int busy = transport_busy();
        if (!arg && busy) transport_req = 2;
        else if (arg && !busy) { if (live.mode) chain_play_ui(); else transport_req = 1; }
    } else if (cmd == 5u || cmd == 6u)
        (void)web_regen(cmd, (uint32_t)arg & 15u, (uint32_t)arg >> 4);
    ui.force = 1;
}

/* for the test and the bench (web/emu/emu_test.mjs), a heavy song straight into the tracks (as hostsim's
 * renders set them): T1 WAVE SAW PAD, T2 MATH FLOAT PAD, T3 MATH BYTEBEAT, 4-note chords on all 16
 * steps, held; T4 DRUM, kick snare and hats. PLAY is the page's (or the test's) to press */
EXPORT void web_test_heavy(void)
{
    static const uint8_t ENG[3][2] = {{ENGI_WAVE, 3}, {ENGI_LOFI, 5}, {ENGI_LOFI, 0}};
    static const uint8_t CH[4][4] = {{48, 55, 60, 64}, {45, 52, 57, 60}, {41, 48, 53, 57}, {43, 50, 55, 59}};
    uint32_t p, i;
    for (p = 0; p < 3u; p++) {
        track_t *t = &trk[p];
        host_preset(t, ENG[p][0], ENG[p][1]);
        t->p[P_VOICE] = V_POLY;
        t->p[P_SUS] = 127;
        for (i = 0; i < 16u; i++) {
            uint8_t n[4], k;
            for (k = 0; k < 4u; k++)
                n[k] = (uint8_t)(CH[i / 4u][k] + 12u * p);
            put_step(t, i, 4, n, ST_NOTE, 0);
        }
    }
    host_drums(&trk[3]);
    dx_kit_load(DX_NKIT);                              /* FLOAT KIT, all three modifiers in the native/WASM check */
    str_cpy(dx_text[0], "sin(w+sin(w*2)*A*2)", DX_TEXT); dx_form[0] = DX_USER; dx_fid[0] = 0;
    dx_fmod[0][DFM_RATE] = 12;
    dx_fmod[1][DFM_BEND] = 90;
    dx_fmod[2][DFM_CRUSH] = 8;
    trk[3].p[P_E5] = 96; trk[3].p[P_E6] = 80; /* formula MOD ENV + FOLD */
    trk[0].p[P_DTYPE] = 11; trk[0].p[P_DIST] = 48; trk[0].p[P_DTONE] = -32; dnz_v[0] = 36;
    perf_held |= PF_BIT(PF_ATK) | PF_BIT(PF_WOB); /* formula FOLD + phase warp */
    life_v[LIFE_AMT] = life_v[LIFE_NUMBERS] = life_v[LIFE_FRAGMENTS] = life_v[LIFE_LINK] = 100;
    for (i = 0; i < 16u; i++) {
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        n[k++] = i % 2u ? 42 : 46;
        put_step(&trk[3], i, k, n, ST_NOTE, 0);
    }
    ui.force = 1;
}

EXPORT uint32_t web_test_user_preset(uint32_t i) { return (uint32_t)up_used(i); }

EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return song.playing; }
EXPORT uint32_t web_sample_rate(void) { return FS; }

/* (0.7.1) stems from the browser (web/emu/stems.js): a backup file's objects into this device, as the editor's RESTORE
 * writes them (editor_backup.c ed_bk_commit, not built here): the songs' sections and index into the flash, the user
 * presets, the live music last into RAM; then a song or the live section rendered with parts left out of the mix (fx.c
 * mix_skip: the part still runs, only its sound and sends are left out), each part alone in turn */
EXPORT uint8_t *web_bk_buf(void) { return proj_wire_u.raw; }
EXPORT uint32_t web_bk_max(void) { return (uint32_t)sizeof proj_wire_u.raw; }
EXPORT uint32_t web_bk_put(uint32_t id, uint32_t len)   /* the bytes in web_bk_buf(): 0 = done, 1 unknown, 2 bad, 4 flash */
{
    uint8_t *raw = proj_wire_u.raw;
    if (len > sizeof proj_wire_u.raw) return 2;
    if (id == 0u || (id >= 40u && id < 40u + NSONG * 4u)) {   /* the live music; song s's section k (song 1: the slots) */
        uint32_t s = (id - 40u) / 4u, k = (id - 40u) % 4u;
        if (!len) return 0;
        if (!proj_import(&proj_scratch, raw, (int)len)) return 2;
        proj_bound(&proj_scratch);
        if (id == 0u) return project_restore_runtime(&proj_scratch) ? 2u : 0u;
        if (!proj_pack((project_store_t *)raw, &proj_scratch)) return 2;
        if (!s)                                        /* song 1's sections live in RAM too (the slots) */
            memcpy(&proj_slot[k], raw, sizeof proj_slot[0]);
        return st_save(s ? OBJ_SONG1 + (s - 1u) * 4u + k : OBJ_PROJECT0 + k, raw, sizeof(project_store_t)) ? 4u : 0u;
    }
    if (id == 10u) {                                   /* the song index: the rows and scenes, into RAM too */
        if (len != sizeof song_idx || ((const song_index_t *)raw)->magic != SONG_MAGIC) return 2;
        memcpy(&song_idx, raw, sizeof song_idx);
        song_idx.cur = 0;
        chain_config = song_idx.rows[song_cur];        /* (the rows now, as editor_backup.c) */
        return st_save(OBJ_SONGIDX, raw, len) ? 4u : 0u;
    }
    if (id == 6u || id == 7u) return !len ? 0u : st_save(OBJ_UPRESET0 + id - 6u, raw, len) ? 4u : 0u;   /* user presets */
    return id == 1u || id == 8u || id == 9u || (id >= 2u && id <= 5u) ? 0u : 1u;   /* (settings, the retired bank and
                                                         * FM6 patches (BYTE: FM6 removed), the current song's
                                                         * sections: the songs' ids carry them) */
}
EXPORT uint32_t web_bk_len(uint32_t id)   /* the object as it is now, into web_bk_buf(): its length, 0 none */
{
    uint8_t *raw = proj_wire_u.raw;
    if (id == 0u) {                                    /* the live music, packed as a section is stored */
        project_capture(&proj_scratch);
        return proj_pack((project_store_t *)raw, &proj_scratch) ? (uint32_t)sizeof(project_store_t) : 0u;
    }
    if (id == 10u) {                                   /* the index with this song's rows as they are now (song_flush) */
        song_idx.rows[song_cur] = chain_config;
        memcpy(raw, &song_idx, sizeof song_idx);
        return (uint32_t)sizeof song_idx;
    }
    if (id >= 40u && id < 40u + NSONG * 4u) {
        uint32_t s = (id - 40u) / 4u, k = (id - 40u) % 4u;
        int n;
        if (!s) {                                      /* song 1's sections: the RAM slots (as editor_backup.c ids 2..5) */
            if (!project_used(k)) return 0;
            memcpy(raw, &proj_slot[k], sizeof proj_slot[0]);
            return (uint32_t)sizeof proj_slot[0];
        }
        n = st_load(OBJ_SONG1 + (s - 1u) * 4u + k, raw, (uint32_t)sizeof proj_wire_u.raw);
        return n > 0 ? (uint32_t)n : 0u;
    }
    return 0;
}
EXPORT void web_mix_skip(uint32_t mask) { mix_skip = (uint8_t)mask; }
EXPORT uint32_t web_song_select(uint32_t n) { return (uint32_t)song_switch(n, 1); }   /* 0 = done (stopped, n < 8) */
EXPORT uint32_t web_song_cur(void) { return song_cur; }
EXPORT uint32_t web_song_rows(void) { return chain_config.count; }
EXPORT uint32_t web_song_bars(void)                    /* the rows' bars, once round (stems.js: a LOOP song's length) */
{
    uint32_t i, n = 0;
    for (i = 0; i < chain_config.count && i < CHAIN_ROWS; i++) n += chain_config.row[i].bars ? chain_config.row[i].bars : 1u;
    return n;
}
EXPORT uint32_t web_song_end(void) { return SONG_END(&chain_config); }   /* 0 once, 2..8 times, 255 round and round */
EXPORT uint32_t web_song_play(void) { return chain_prepare(); }   /* 0 = the song starts (song_main.c's codes) */
EXPORT void web_play(uint32_t on) { transport_req = (uint8_t)(on ? 1u : 2u); }
EXPORT uint32_t web_beat_samples(void) { return beat_samples(); }
EXPORT uint32_t web_trk_live(void) { return trk_live; }

/* FUNC text bridge: same compiler/storage as the instrument, never JS eval. */
static char web_func_text[256];static uint32_t web_func_pos;
EXPORT char *web_func_buffer(void){return web_func_text;}
EXPORT uint32_t web_func_error_pos(void){return web_func_pos;}
EXPORT const char *web_func_get(uint32_t track){track%=NTRK;return math_func_text(&trk[track],lyr_get(track));}
EXPORT uint32_t web_func_apply(uint32_t track){
    if(track>=NTRK)return FE_ARGS;
    web_func_text[255]=0;gq_work_t*w=proj_ws();fvm_err_t err={0,0};
    uint32_t rc=fvm_compile(&w->cc,FVM_FULL(&w->pg),web_func_text,str_len(web_func_text),&err);
    if(!rc&&(!w->pg.p.func||w->pg.p.nstate>2u)){rc=FE_STATE;err.pos=0;}
    web_func_pos=err.pos;if(rc)return rc;
    track_t*t=&trk[track];song.sel=(uint8_t)track;
    if(t->eng_req!=ENGI_FUNC){set_engine_of(t,ENGI_FUNC);t->p[P_E0]=4;t->p[P_E2]=80;t->p[P_E3]=0;t->p[P_E4]=64;t->p[P_E5]=64;}   /* (BYTE 0.17: FUNC's own engine) */
    t->p[P_E1]=RW_FUNC;t->p[P_E7]=0;lyr_set(track,web_func_text);math_hash[track]=0;math_poll();ui.force=1;return 0;
}

EXPORT uint32_t web_func_mode(uint32_t track,int32_t mode){track%=NTRK;track_t*t=&trk[track];if(mode>=0){t->p[P_E0]=mode?5:4;if(mode)t->p[P_SUS]=127;ui.force=1;}return !func_on(t)||func_song(t);}
#if FELUCCA_WEB_EDITOR                                   /* (studio) the whole web editor into this emulator: web/emu/build.sh's
                                                          * felucca.wasm only */
#include "editor-api.h"
#endif
