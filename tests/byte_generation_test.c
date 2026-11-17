/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The song generator must use the engines it actually exposes, preserve tonal
 * formula identities, and produce reproducible songs after unrelated edits. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void make_byte_song(uint32_t genre, uint32_t seed, uint32_t form)
{
    gp.genre = (uint8_t)genre; gp.seed = (uint16_t)seed;
    gp.energy = 60; gp.form = (uint8_t)form;
    gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
    for (uint32_t k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    gen_go();
}

static uint32_t byte_song_hash(void)
{
    uint32_t h = proj_hash(song.g, sizeof song.g);
    for (uint32_t k = 0; k < NTRK; k++) {
        h = (h ^ proj_hash(trk[k].p, sizeof trk[k].p)) * 16777619u;
        h = (h ^ proj_hash(trk[k].step, sizeof trk[k].step)) * 16777619u;
        h = (h ^ trk[k].eng_req ^ ((uint32_t)trk[k].preset << 8)) * 16777619u;
    }
    return h ^ proj_hash(dx_kit, sizeof dx_kit) ^ proj_hash(dx_form, sizeof dx_form) ^ proj_hash(dx_fmod, sizeof dx_fmod);
}

int main(void)
{
    uint32_t songs = 0, seen = 0, bytes = 0, floats = 0;
    int bad = 0, native = 1, reproducible = 1, varied = 1, bounded = 1, tonal = 1;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    for (uint32_t g = 0; g <= NGENRE + 1u; g++) {
        uint32_t genre = g > NGENRE ? GEN_EXPLORE : g;
        uint32_t previous = 0;
        for (uint32_t seed = 1; seed <= 3u; seed++) {
            make_byte_song(genre, seed * 37u, 0);
            uint32_t hash = byte_song_hash();
            if (seed > 1u) varied &= hash != previous;
            previous = hash;
            for (uint32_t k = 0; k < NTRK; k++) {
                track_t *t = &trk[k];
                uint32_t e = t->eng_req;
                native &= e == ENGI_WAVE || e == ENGI_LOFI || e == ENGI_DRUM;
                seen |= 1u << e;
                for (uint32_t i = 0; i < 8u; i++) {
                    const param_desc_t *d = track_desc(t, P_E0 + i);
                    bounded &= t->p[P_E0 + i] >= d->min && t->p[P_E0 + i] <= d->max;
                }
                if (e == ENGI_LOFI) {
                    bytes += t->p[P_E1] == RW_BYTE;
                    floats += t->p[P_E1] == RW_FLOAT;
                    /* ALGO points to an exact factory formula, never a random
                     * neighbouring formula from a generic knob perturbation. */
                    tonal &= t->p[P_E2] == LOFI_PRESETS[t->preset].e[2];
                }
            }
            /* A previous sound must not leak into the same seed. */
            trk[2].p[P_E2] = 127; trk[2].p[P_CAOS] = 127;
            trk[3].p[P_FCUT] = 0;
            memset(dx_form, 0, sizeof dx_form);
            memset(dx_fmod, 7, sizeof dx_fmod);
            make_byte_song(genre, seed * 37u, 0);
            reproducible &= hash == byte_song_hash();
            songs++;
        }
    }
    bad += check("BYTE GEN: all genres and seeds use only DRUM / MATH / WAVE", native);
    bad += check("BYTE GEN: both melodic engines, bytebeats and floatbeats are selected",
                 seen == ((1u << ENGI_WAVE) | (1u << ENGI_LOFI) | (1u << ENGI_DRUM)) && bytes && floats);
    bad += check("BYTE GEN: every engine parameter stays in its real descriptor range", bounded);
    bad += check("BYTE GEN: melodic algorithms retain their curated preset identity", tonal);
    bad += check("BYTE GEN: same settings reproduce the song after unrelated edits", reproducible);
    bad += check("BYTE GEN: different seeds produce different songs in every genre", varied);
    printf("BYTE GEN: %u songs, %u bytebeat and %u floatbeat tracks\n", songs, bytes, floats);
    return bad != 0;
}
