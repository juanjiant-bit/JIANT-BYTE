/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT FM V10.8.41 .. 46 in BYTE) the generator's ports checked:
 * - JAZZ (the owner: "espantoso", then "the hats still too loud"): its hat lanes (the foot, the ride) under 50 at
 *   every MOOD, the snare's written comping there (GHOST hits off the beat), the bars not copies (steps that differ
 *   from the bar before: 3 in 10 at least on average; 2 in 10 before the comping)
 * - SONG's rows: none over ~24 s (bars * 240 / BPM, a tempo's 4 bars at least), no row a row before it again (section,
 *   mutes, sources, variations, macros), MY LOOP aside
 * - every melodic part moving (an LFO destination, AUTO PAN or an envelope on the filter or the shape)
 * - the melodic parts' filters (the owner: "the chords all filtered the same, the low end muddy"): BP and HP the most, at
 *   most one LP a piece, a BP part in the mids, an HP one under ~300 Hz, the bass's own */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t genre_of(const char *nm)
{
    uint32_t r;
    for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, nm)) return r;
    return 0;
}

int main(void)
{
    uint32_t bad = 0, jz = genre_of("JAZZ"), s, m, g;
    static const int8_t MOODS[4] = {0, 50, 25, -20};
    uint32_t loud = 0, noghost = 0, diff = 0, tot = 0, pieces = 0, hmax = 0;
    ui_power_on();
    for (m = 0; m < 4u; m++)
        for (s = 0; s < 8u; s++) {
            uint32_t dr, i, l, ghost = 0;
            const track_t *t;
            gp.genre = (uint8_t)jz; gp.form = 0; gp.seed = (uint16_t)(37u + s * 37u);   /* (tests/gen_grid.c's pieces) */
            for (i = 0; i < GP_N; i++) gp.amt[i] = 60;
            gp.energy = 50; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            grd.on = 0; grd.mood = MOODS[m];
            gen_make();
            dr = gs.trk_of[RL_DRUM];
            if (dr >= NTRK || !gs.act[RL_DRUM]) continue;
            t = &trk[dr]; pieces++;
            for (l = 3; l <= 4u; l++) {
                if ((uint32_t)t->p[P_LN0 + l] > hmax) hmax = (uint32_t)t->p[P_LN0 + l];
                loud += t->p[P_LN0 + l] >= 50;
            }
            for (i = 0; i < dlen(t); i++) {
                const step_t *x = &t->step[i];
                if (x->time == ST_NOTE && (x->hit & 2u) && step_lmode(x, 1) == LM_GHOST && (i & 1u)) ghost++;
                if (i >= 16u && !m) { tot++; diff += x->hit != t->step[i - 16u].hit || (x->time == ST_NOTE) != (t->step[i - 16u].time == ST_NOTE); }
            }
            noghost += !ghost;
        }
    printf("JAZZ: %u pieces, hat lanes max %u, %u %% of the steps differ from the bar before\n", pieces, hmax, tot ? diff * 100u / tot : 0u);
    bad += check("JAZZ: the hat lanes (the foot, the ride) under 50 at every MOOD", pieces >= 24u && !loud && hmax < 50u);
    bad += check("JAZZ: the snare's written comping, ghosts off the beat, every piece", !noghost);
    bad += check("JAZZ: the bars not copies (3 in 10 steps differ from the bar before)", tot && diff * 10u >= tot * 3u);
    {
        uint32_t rows = 0, same = 0, over = 0, still = 0, parts = 0, r, k, ft[8] = {0}, lp2 = 0, offz = 0;
        double mx = 0;
        for (g = 1; g <= NGENRE; g++)
            for (r = 0; r < 3u; r++) {
                uint32_t i, j;
                gp.genre = (uint8_t)g; gp.form = 2; gp.seed = (uint16_t)(300u + r * 91u);
                grd.on = 0; grd.mood = (int8_t)(r == 2u ? -30 : 0);
                gen_make();
                for (i = 0; i < chain_config.count; i++) {
                    double sec = chain_config.row[i].bars * 240.0 / song.g[G_BPM];
                    const scene_t *a = &song_idx.scene[song_cur][i];
                    rows++;
                    if (sec > mx) mx = sec;
                    over += sec > 25.0 && chain_config.row[i].bars > 4u;
                    for (j = 0; j < i; j++) {
                        const scene_t *b = &song_idx.scene[song_cur][j];
                        if (chain_config.row[j].slot == chain_config.row[i].slot && a->mute == b->mute && a->on == b->on &&
                            song_idx.tsrc[song_cur][i] == song_idx.tsrc[song_cur][j] && song_idx.tvar[song_cur][i] == song_idx.tvar[song_cur][j] &&
                            !memcmp(a->mac, b->mac, sizeof a->mac)) { same++; break; }
                    }
                }
                for (k = 0, j = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t role = gs.role[k], f = (uint32_t)t->p[P_FTYPE] & 7u;
                    if (k >= trk_live || role > RL_TEX || role == RL_DRUM || !gs.act[role]) continue;
                    parts++;
                    if (role != RL_BASS && !(GENRE[g - 1u].spec & GSP_FUGUE)) {   /* (FUGA's voices alone, open: gfugue) */
                        ft[f]++; j += f == FT_LP || f >= FT_LP24H;
                        offz += (f == FT_BP && (t->p[P_FCUT] < 62 || t->p[P_FCUT] > 78)) || (f == FT_HP && t->p[P_FCUT] > 42);
                    }
                    still += !t->p[P_LD_FLT] && !t->p[P_LD_SHP] && !t->p[P_LD_AMP] && !t->p[P_LD_PIT] && !t->p[P_APDEP];
                }
                lp2 += j > 1u;
            }
        printf("SONG: %u rows, the longest %.1f s, %u over ~24 s, %u a row before again; %u of %u melodic parts still\n", rows, mx, over, same, still, parts);
        bad += check("SONG: no row over ~24 s (four bars at least)", rows && !over);
        bad += check("SONG: no row a row before it again", !same);
        bad += check("every melodic part moving (an LFO, AUTO PAN)", parts && !still);
        printf("melodic filters (not the bass): LP %u HP %u BP %u COMB %u LP24 %u; pieces with two LP %u, out of their zone %u\n", ft[FT_LP], ft[FT_HP], ft[FT_BP],
               ft[FT_COMB] + ft[FT_COMBN], ft[FT_LP24H] + ft[FT_LP24], lp2, offz);
        bad += check("melodic filters: BP the most, then HP, LP the fewest, one LP a piece at most, BP in the mids, HP low",
                     ft[FT_BP] > ft[FT_HP] && ft[FT_HP] > ft[FT_LP] + ft[FT_LP24H] + ft[FT_LP24] && !lp2 && !offz);
    }
    clay_names(); lofi_names();                         /* (fmgen45, room: the names' tables written in the RAM) */
    bad += check("the names' tables in RAM: DRONE CLAY's SHAPE (the HITS first), EUCLID, RATE; MATH's B01 .. B48, F01 .. F48",
                 str_eq(CLAY_SHN[0], "KICK") && str_eq(CLAY_SHN[11], "CRASH") && str_eq(CLAY_SHN[12], "SINE") &&
                 str_eq(CLAY_SHN[54], "FM x7") && !CLAY_SHN[55] && str_eq(CLAY_EU[2], "1/1") && str_eq(CLAY_EU[25], "5/13") &&
                 !CLAY_EU[26] && str_eq(CLAY_RESN[1], "1/64") && str_eq(CLAY_RESN[25], "1/28") && !CLAY_RESN[26] &&
                 str_eq(N_BYTE[0], "B01") && str_eq(N_BYTE[47], "B48") && !N_BYTE[48] && str_eq(N_FLOAT[0], "F01") &&
                 str_eq(N_FLOAT[47], "F48") && !N_FLOAT[48]);
    printf(bad ? "gen-port: %u FAILED\n" : "gen-port: all ok\n", bad);
    return bad ? 1 : 0;
}
