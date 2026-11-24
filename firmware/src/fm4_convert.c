/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DIGITAL (engine 1, four-operator FM): retired. Engine index 1 stays reserved (the stores name engines by index and
 * the table is append-only): ENG_FM4_GONE below, no presets, never offered (engines.c eng_ok). A DIGITAL sound that
 * arrives anyway -- a project, a user preset, the editor's PRESET / SET G_ENGSEL / UP_PUT, a backup, a stored DIGITAL
 * preset number -- becomes engine 12 (FM6's number, MATH's in BYTE: engines.c ENG_LIVE) with its values as they are
 * (ui.c fm4_apply, project.c). (BYTE's flash audit: JIANT FM's conversion to an FM6 patch, its algorithm tables and
 * DIGITAL's eight presets removed with FM6) */
#ifndef FM4_CONVERT
#define FM4_CONVERT FELUCCA_FM4
#endif

/* EDIT: the ranges are the stores' (a value of engine 1 is clamped to them before it converts) */
#define FM4_EDIT(l0, l1, l2, l3, l4, l5, l6, f0, f1, n0, n1) {                                              \
        {l0, f0, 0, 7, 0, n0}, {l1, f0, 0, 14, 1, n1}, {l2, f0, 0, 14, 1, n1}, {l3, f0, 0, 14, 1, n1}, \
        {l4, F_PCT, 0, 127, 60, 0}, {l5, f1, 0, 127, 60, 0}, {l6, F_PCT, 0, 127, 0, 0},               \
        {"-", F_INT, 0, 0, 0, 0}}

/* engine 1 without the DSP: never selectable, no presets, silent (a track never keeps it: fm4_convert) */
static void fm4_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void fm4_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_FM4_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = FM4_EDIT("-", "-", "-", "-", "-", "-", "-", F_INT, F_INT, 0, 0),
    .note_on = fm4_gone_note_on,
    .render = fm4_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};

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
