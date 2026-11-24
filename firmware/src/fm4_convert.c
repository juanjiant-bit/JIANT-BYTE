/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DIGITAL (engine 1, four-operator FM): retired. Engine index 1 stays reserved (the stores name engines by index and
 * the table is append-only): ENG_GONE (engines.c), no presets, never offered (eng_ok). A DIGITAL sound that
 * arrives anyway -- a project, a user preset, the editor's PRESET / SET G_ENGSEL / UP_PUT, a backup, a stored DIGITAL
 * preset number -- becomes engine 12 (FM6's number, MATH's in BYTE: engines.c ENG_LIVE) with its values as they are
 * (ui.c fm4_apply, project.c). (BYTE's flash audit: JIANT FM's conversion to an FM6 patch, its algorithm tables and
 * DIGITAL's eight presets removed with FM6) */
#ifndef FM4_CONVERT
#define FM4_CONVERT FELUCCA_FM4
#endif

/* ------------------------------------------------------- conversion --- */
/* (BYTE: FM6 removed) a DIGITAL sound becomes engine 12 (FM6's number, which plays as MATH: engines.c ENG_LIVE) with its
 * values as they are and preset 0; no patch (the conversion to FM6 and DIGITAL's presets are gone) */
static uint32_t fm4_convert(int16_t *p)
{
    (void)p;
    return 0;
}
/* DIGITAL preset k's values: none (retired, see the top) */
static void fm4_preset_values(int16_t *p, uint32_t k)
{
    (void)p; (void)k;
}
