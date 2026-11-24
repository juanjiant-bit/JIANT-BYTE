/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE) A part's own program text: MATH FUNC's formula (eng_lofi.c math_func_text, the NAME screen's FORMULA EDITOR,
 * projects' 'L' 'Y' blocks, the editor). It took the place of VOICE's lyric (sing.c lyr_txt, JIANT 0.8.5); VOICE, its
 * words and its singing (eng_formant.c, sing.c, sing_ext.c, felucca_lyrics.h's ES / EN / JA banks) are removed in BYTE's
 * flash audit. ASCII, 0-ended. The main loop writes (lyr_set), the audio reads; lyr_gen moves on at each write. In the
 * pool (zeroed at boot; LOWBUF is the main loop's and full) */
#define LYR_LEN 256u             /* 255 characters and the terminator */
static char lyr_txt[NTRK][LYR_LEN] __attribute__((section(".pool")));
static uint8_t lyr_gen[NTRK];
static void lyr_set(uint32_t k, const char *s)
{
    uint32_t i;
    char b[LYR_LEN];
    for (i = 0; s && s[i] && i + 1u < LYR_LEN; i++)
        b[i] = (char)(s[i] >= 32 && s[i] <= 126 ? s[i] : ' ');
    b[i] = 0;
    for (; i < LYR_LEN; i++)
        b[i] = 0;
    k %= NTRK;
    for (i = 0; i < LYR_LEN; i++)
        lyr_txt[k][i] = b[i];
    RING_PUBLISH();
    lyr_gen[k]++;
}
static const char *lyr_get(uint32_t k) { return lyr_txt[k % NTRK]; }

/* (V10.15) the Bluetooth segment checked at power-on (main.c: bt_hw_seg; the screen's box when it is missing) */
#if FELUCCA_BT
static volatile uint8_t ly_en_ok;
#else
static volatile uint8_t ly_en_ok = 1;                  /* (the host: one image) */
#endif
