/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* FM6 (JIANT FM's six-operator engine, msfa's core): retired in BYTE, removed in BYTE's flash audit with everything
 * that only served it: its synthesis (fm6_core.c), the eight factory patches (tools/gen_fm6_patches.py,
 * felucca_fm6.h), the tracks' patches, SLOT (F1..F8, OWN, the voice bank B1..B32: fm6_vbank.c), the operator pages
 * (ui_fm6op.c), the editor's FM6 commands (editor_fm6.c) and the user presets' patches (up_fm6.c). Engine 12 stays
 * reserved (the stores name engines by index): a sound of it plays as MATH (engines.c ENG_LIVE). The stores keep their
 * layouts: a project's patch block (project.c, FUN8 and later) is read and skipped, written empty; a backup's FM6
 * objects (editor_backup.c ids 8 and 9) are accepted and ignored, listed empty. */
#define ENGI_FM6 12u             /* engines.c ENGINES[] (append-only) */
#define FM6_PACKED 128u          /* a project's per-track patch record (kept in the formats) */

static uint8_t rel_hurry[NTRK];                  /* (voice.c, codex/voices: a part's released voices hurried) */
static volatile uint8_t load_tight;              /* (voice.c: the guard tripped) */
#define HURRY_LVL 3277                           /* (voice.c) */
