/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MASTER (#137): the pot read before the audio starts, so the first block plays at its level (it was 2048, about
 * 70 % of the knob, until the main loop's first read after the splash, then a 1/8-per-frame glide from mid-scale:
 * the keys, live from the audio ISR at once, sounded loud under the splash with the knob turned down).
 * master_knob: the pot x16, smoothed per UI frame (master_poll) */
static int32_t master_knob = 512 * 16;
static uint8_t mf_on;                                   /* (main.c: 1 at power-on; the fade below) */
static uint32_t mf_ms;
static void master_poll(void)
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER);
    if (a >= 0) {
        master_knob += (a * 16 - master_knob) / 8;
        song.master_q12 = master_of_pot((uint32_t)(master_knob / 16));
    }
    if (mf_on) {                                        /* (BYTE, as JIANT FM: the power-on click) silent until the audio has
                                                         * played ~100 ms of zeros (35 halves), the output on, the level up
                                                         * from 0 over 128 ms */
        if (mf_on == 1u) {
            if (audio_halves >= 35u) {
                fm1_audio_unmute();
                mf_on = 2u;
                mf_ms = fm1_ms;
            }
            song.master_q12 = 0;
        } else {
            uint32_t e = fm1_ms - mf_ms;
            if (e >= 128u)
                mf_on = 0;
            else
                song.master_q12 = song.master_q12 * e >> 7;
        }
    }
}
/* power-on (main.c), before audio_init: the knob as it stands, no glide */
static void master_boot(void)
{
    uint32_t i;
    (void)fm1_adc_read(FM1_ADC_MASTER);                 /* (the first conversion after the pin setup: dropped) */
    for (i = 0; i < 3u; i++) {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0) {
            master_knob = a * 16;
            song.master_q12 = master_of_pot((uint32_t)a);
            return;
        }
    }                                                   /* (no reading: felucca_init's level, the loop glides on) */
}
