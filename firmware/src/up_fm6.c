/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE: FM6 removed) The user presets' FM6 patches (1.0.3 .. BYTE 0.16: storage.c OBJ_UPFM6, which also held a VOICE
 * preset's lyric) and the FM6 patch bank of 1.0..1.0.2 (OBJ_FM6BANK) are no longer read or written; their sectors are
 * left as they are. Their layouts stay for the backups (editor_backup.c ids 8 and 9: an archive's are accepted, by size,
 * and ignored; listed empty). Included by upreset.c. */
typedef struct { uint8_t raw[3472]; } fm6_bank_t;   /* the retired bank's size (an older backup's id 8) */
#define UPF_PK 112u                              /* 128 x 7 bits */
typedef struct {
    uint32_t tag;
    uint8_t pk[UPF_PK];
} upf_ent_t;
typedef struct {
    uint32_t magic;
    uint16_t ver, nslot;
    uint32_t used;
    uint32_t rsv;
    upf_ent_t e[UP_SLOTS];
} upf_t;
_Static_assert(sizeof(upf_t) == 3728u, "user preset FM6 patches layout (an older backup's id 9)");
