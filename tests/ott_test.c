/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (BYTE ott) FX > OTT, a track's two-band upward / downward compressor (fx.c ott_block; params.c ott_v): a quiet band
 * comes up, a loud one goes down, each band on its own; DEPTH 0 the track's sound bit for bit (not run), and back to it
 * after DEPTH goes down; silence stays silence; no value out of range on any input; the overload's relief eases it
 * (never cuts the sound); kept with a project ('O' 'T') and a user preset, an older one loading with it off; the page
 * (FX > TRACK's fourth). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <math.h>
static int bad;
static void ck(const char *what, int ok) { printf("ott: %-100s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static double run_sine(uint32_t ti, double amp, double hz, uint32_t nblk, double *out_rms, int32_t *mx)
{
    static int32_t b[CTL];
    double ein = 0, eout = 0;
    uint32_t k, i;
    *mx = 0;
    for (k = 0; k < nblk; k++) {
        for (i = 0; i < CTL; i++) b[i] = (int32_t)lrint(amp * sin(2 * M_PI * hz * (k * CTL + i) / 44100.0));
        if (k >= nblk / 2) for (i = 0; i < CTL; i++) ein += (double)b[i] * b[i];
        if (ott_on(ti)) ott_block(ti, b, CTL);
        if (k >= nblk / 2) for (i = 0; i < CTL; i++) { eout += (double)b[i] * b[i]; if (abs(b[i]) > *mx) *mx = abs(b[i]); }
    }
    *out_rms = sqrt(eout / (nblk / 2 * CTL));
    return 10 * log10((eout + 1) / (ein + 1));          /* (the gain, dB) */
}
static void ott_set(uint32_t ti, int d, int tm, int ud, int xo)
{
    ott_v[ti][OT_DEPTH] = (int16_t)d; ott_v[ti][OT_TIME] = (int16_t)tm; ott_v[ti][OT_UPDN] = (int16_t)ud; ott_v[ti][OT_XO] = (int16_t)xo;
}
static uint32_t part_hash(uint32_t nblk, int depth_until)
{
    track_t *t = &trk[0];
    uint32_t h = 2166136261u, b, i;
    ui_power_on();
    host_preset(t, 0, 0);
    memset(ott_s, 0, sizeof ott_s);
    ott_defaults();
    rng_state = 99;
    for (b = 0; b < nblk; b++) {
        if (b == 2) trk_note_on(t, 48, 110);
        ott_v[0][OT_DEPTH] = (int16_t)((int)b < depth_until ? 100 : 0);
        memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
        memset(send_c, 0, sizeof send_c); memset(send_d, 0, sizeof send_d); memset(send_r, 0, sizeof send_r);
        mix_part_m(t, CTL);
        if (b >= 60) for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)mix_l[i]) * 16777619u;
    }
    return h;
}
int main(void)
{
    double r, g;
    int32_t mx;
    uint32_t i, k;
    ui_power_on();
    ck("power-on: every track's OTT off at its defaults (DEPTH 0, TIME 56, UP/DN 0, X-OVER 37), not run",
       ott_v[0][OT_DEPTH] == 0 && ott_v[5][OT_TIME] == 56 && ott_v[3][OT_UPDN] == 0 && ott_v[2][OT_XO] == 37 && !ott_on(0) && !ott_on(5));
    {   /* 1. a quiet band up, a loud one down (DEPTH 100), each band its own; UP/DN takes from one side */
        double q_lo, q_hi, l_lo, l_hi, q_none, l_none, q_mix;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        q_lo = run_sine(0, 1300, 80, 400, &r, &mx);          /* -40 dB of full scale (2^17), under X-OVER */
        memset(ott_s, 0, sizeof ott_s);
        q_hi = run_sine(0, 1300, 3000, 400, &r, &mx);        /* .. over it */
        memset(ott_s, 0, sizeof ott_s);
        l_lo = run_sine(0, 120000, 80, 400, &r, &mx);        /* -0.8 dB */
        memset(ott_s, 0, sizeof ott_s);
        l_hi = run_sine(0, 120000, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 63, 37);                          /* UP/DN +: no downward */
        l_none = run_sine(0, 120000, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, -64, 37);                         /* UP/DN -: no upward */
        q_none = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 50, 50, 0, 37);                            /* half the DEPTH: about half the gain */
        q_mix = run_sine(0, 1300, 3000, 400, &r, &mx);
        printf("ott: quiet low %+.1f dB, quiet high %+.1f, loud low %+.1f, loud high %+.1f; UP/DN +63 loud %+.1f, -64 quiet %+.1f; DEPTH 50 quiet %+.1f\n",
               q_lo, q_hi, l_lo, l_hi, l_none, q_none, q_mix);
        ck("1. DEPTH 100 (v1.1b5: brutal): a quiet band up (+28 .. +40 dB), a -0.8 dB one down (-1 .. -15 dB), low and high alike",
           q_lo > 28 && q_lo < 40 && q_hi > 28 && q_hi < 40 && l_lo < -1 && l_lo > -15 && l_hi < -1 && l_hi > -15);
        ck("   UP/DN +63: no downward, makeup nor drive (a loud band within 0.5 dB); -64: no upward, the makeup and the drive (+12 .. +16 dB); DEPTH 50 a quarter or so",
           fabs(l_none) < 0.5 && q_none > 12 && q_none < 16 && q_mix > q_hi * 0.12 && q_mix < q_hi * 0.4);
        {   /* (bigtext) at DEPTH 100 a -40 dB sine and a -6 dB one come out within 6 dB of each other, none past full scale;
             * (v1.1b5) a -46 dB one and the -6 dB one within 3 dB; DEPTH 60 still gentle (the -40 dB one up 4 .. 14 dB) */
            double lq, ll, rq, rl, l46, r46, d60;
            int32_t m2;
            memset(ott_s, 0, sizeof ott_s);
            ott_set(0, 100, 56, 0, 37);
            (void)run_sine(0, 1311, 1000, 400, &rq, &mx);     /* -40 dB of full scale (2^17) */
            memset(ott_s, 0, sizeof ott_s);
            (void)run_sine(0, 65700, 1000, 400, &rl, &m2);    /* -6 dB */
            lq = 20 * log10(rq * M_SQRT2 / 131072); ll = 20 * log10(rl * M_SQRT2 / 131072);
            printf("ott: DEPTH 100, 1 kHz: -40 dB in -> %+.1f dB out, -6 dB in -> %+.1f dB out (peaks %d %d)\n", lq, ll, mx, m2);
            ck("   DEPTH 100: -40 dB and -6 dB sines out within 6 dB of each other, neither past full scale",
               fabs(lq - ll) < 6 && mx <= 131072 && m2 <= 131072);
            memset(ott_s, 0, sizeof ott_s);
            (void)run_sine(0, 657, 1000, 400, &r46, &mx);     /* -46 dB */
            l46 = 20 * log10(r46 * M_SQRT2 / 131072);
            memset(ott_s, 0, sizeof ott_s);
            ott_set(0, 60, 56, 0, 37);
            d60 = run_sine(0, 1311, 1000, 400, &r, &m2);
            printf("ott: DEPTH 100, 1 kHz: -46 dB in -> %+.1f dB out (peak %d); DEPTH 60: -40 dB %+.1f dB\n", l46, mx, d60);
            ck("   (v1.1b5) DEPTH 100: -46 dB and -6 dB sines out within 3 dB (smashed), no peak past full scale; DEPTH 60 +4 .. +14 dB",
               fabs(l46 - ll) < 3 && mx <= 131072 && d60 > 4 && d60 < 14);
        }
    }
    {   /* 2. a quiet low under a loud high: the low comes up, the high goes down (the bands apart) */
        static int32_t b[CTL];
        double elo = 0, elo0 = 0;
        uint32_t n;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        for (n = 0; n < 400; n++) {
            for (i = 0; i < CTL; i++) {
                double tt = (n * CTL + i) / 44100.0;
                b[i] = (int32_t)lrint(1300 * sin(2 * M_PI * 60 * tt) + 100000 * sin(2 * M_PI * 4000 * tt));
            }
            ott_block(0, b, CTL);
        }
        (void)elo; (void)elo0;
        ck("2. a quiet low under a loud high: LOW's gain up, HIGH's down (each band its own level)",
           ott_s[0].g[0] > 0 && ott_s[0].g[1] < -50);
    }
    {   /* 3. silence stays silence; anything in, nothing out of range (the integers' path: no NaN, no denormal) */
        static int32_t b[CTL];
        int ok = 1;
        uint32_t n, rs = 12345u;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(1, 100, 30, 0, 15);
        for (n = 0; n < 300; n++) {
            memset(b, 0, sizeof b);
            ott_block(1, b, CTL);
            for (i = 0; i < CTL; i++) ok &= b[i] == 0;
        }
        ck("3. DEPTH 100, silence in: silence out (no hiss brought up), its gain within the bounds", ok && ott_s[1].g[0] <= OT_UPMAX && ott_s[1].g[0] >= 0);
        for (k = 0; k < 4u; k++) {
            ott_set(1, 100, k ? 30 + (int)k * 23 : 30, k & 1 ? 63 : -64, k ? 100 : 15);
            for (n = 0; n < 500; n++) {
                for (i = 0; i < CTL; i++) {
                    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
                    b[i] = n % 50 < 25 ? (int32_t)rs >> (n % 7 == 0 ? 8 : 14) : (int32_t)(rs & 15u) - 8;   /* (bursts of noise past
                                                         * full scale, near-silence between) */
                }
                ott_block(1, b, CTL);
                for (i = 0; i < CTL; i++) ok &= b[i] > -(1 << 25) && b[i] < (1 << 25);
                ok &= ott_s[1].g[0] >= -1276 && ott_s[1].g[0] <= OT_UPMAX && ott_s[1].g[1] >= -1276 && ott_s[1].g[1] <= OT_UPMAX;
                ok &= ott_s[1].lv[0] >= 0 && ott_s[1].lv[1] >= 0;
            }
        }
        ck("   noise bursts past full scale and near-silence, every setting: every sample and gain within range", ok);
    }
    {   /* 4. DEPTH 0: not run, bit for bit; after DEPTH 100 then 0: back to the same sound within a block, then not run */
        uint32_t h0 = part_hash(160, 0), h1 = part_hash(160, 40), h2 = part_hash(160, 1000), h3 = part_hash(160, 0);
        int quiet;
        quiet = !ott_on(0);
        ck("4. DEPTH 0 bit-exact (not run); DEPTH 100 for 40 blocks then 0: back to the dry part bit for bit", h0 == h3 && h0 == h1 && quiet);
        ck("   DEPTH 100 throughout: the part changed", h2 != h0);
        {   /* the fade: gains back at 1 after one block, the state cleared */
            static int32_t b[CTL];
            memset(ott_s, 0, sizeof ott_s);
            ott_set(2, 100, 50, 0, 37);
            for (k = 0; k < 100u; k++) {
                for (i = 0; i < CTL; i++) b[i] = (int32_t)lrint(1300 * sin(2 * M_PI * 200 * (k * CTL + i) / 44100.0));
                ott_block(2, b, CTL);
            }
            g = ott_s[2].g[0];
            ott_v[2][OT_DEPTH] = 0;
            ott_block(2, b, CTL);
            ck("   DEPTH to 0: a block's ramp to unity, then out (ott_on 0, the state cleared)",
               g > 0 && !ott_on(2) && !ott_s[2].lv[0] && !ott_s[2].lp);
        }
    }
    {   /* 5. the overload's relief: half the effect tripped, none at RL_WAVE1 (the sound itself as it is) */
        double full, half, none;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        load_rl = 0;
        full = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        load_rl = RL_TIGHT;
        half = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        load_rl = RL_WAVE1;
        none = run_sine(0, 1300, 3000, 400, &r, &mx);
        load_rl = 0;
        printf("ott: relief: none %+.1f dB, tripped %+.1f, RL_WAVE1 %+.2f (the level the sound had: %.0f rms)\n", full, half, none, r);
        ck("5. relief: tripped about half the gain; from RL_WAVE1 none (the sound as it was, never cut)",
           half > full * 0.35 && half < full * 0.7 && fabs(none) < 0.01 && r > 800);
    }
    {   /* 6. kept with a project: 'O' 'T' when one is on; without it (every one of before): off */
        static project_store_t st;
        int ok;
        uint32_t j, found = 0;
        ui_power_on();
        ott_set(2, 70, 40, -20, 60);
        ott_set(4, 0, 90, 10, 20);
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        for (j = 68; j + 1 < PROJ_STORE_SIZE; j++) found |= st.raw[j] == 'O' && st.raw[j + 1] == 'T' && st.raw[j + 2 + 2 * OT_N] == 70;
        ui_power_on();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("6. a project keeps each track's OTT ('O' 'T'): T3 70 40 -20 60, T5's knobs at DEPTH 0, the others off",
           ok && found && ott_v[2][OT_DEPTH] == 70 && ott_v[2][OT_TIME] == 40 && ott_v[2][OT_UPDN] == -20 && ott_v[2][OT_XO] == 60 &&
           ott_v[4][OT_DEPTH] == 0 && ott_v[4][OT_TIME] == 90 && ott_v[0][OT_DEPTH] == 0 && ott_v[0][OT_TIME] == 56);
        ok = proj_packed_size(&proj_scratch) >= 2u + sizeof proj_scratch.ott;
        ui_power_on();                                    /* a project of before: no 'O' 'T' */
        project_capture(&proj_scratch);
        ok &= proj_pack(&st, &proj_scratch);
        for (j = 68, found = 0; j + 1 < PROJ_STORE_SIZE; j++) found |= st.raw[j] == 'O' && st.raw[j + 1] == 'T';
        ott_set(1, 100, 40, 0, 50);
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project with no OTT on writes no block; loading one (as every project of before): OTT off, the defaults",
           ok && !found && ott_v[1][OT_DEPTH] == 0 && ott_v[1][OT_TIME] == 56 && ott_v[1][OT_XO] == 37);
    }
    {   /* 6b. (v1.1b5, as JIANT FM) DEPTH 0 everywhere, a TIME UP/DN X-OVER moved: the block written, kept */
        static project_store_t st;
        int ok;
        uint32_t j, found = 0;
        ui_power_on();
        ott_set(5, 0, 33, -40, 90);
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        for (j = 68; j + 1 < PROJ_STORE_SIZE; j++) found |= st.raw[j] == 'O' && st.raw[j + 1] == 'T';
        ui_power_on();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("6b. DEPTH 0, T6's TIME UP/DN X-OVER moved (33 -40 90): 'O' 'T' written, they load back, the others the defaults",
           ok && found && ott_v[5][OT_DEPTH] == 0 && ott_v[5][OT_TIME] == 33 && ott_v[5][OT_UPDN] == -40 && ott_v[5][OT_XO] == 90 &&
           ott_v[0][OT_TIME] == 56 && ott_v[0][OT_XO] == 37);
    }
    {   /* 7. a user preset keeps the track's OTT; one of before loads it off */
        int ok;
        ui_power_on();
        song.sel = 3;
        ott_set(3, 55, 70, 30, 80);
        ok = (up_store(9, "OTT TEST") | 3) == 3;
        ott_set(3, 0, 64, 0, 37);
        ok &= up_load(9) == 0 && ott_v[3][OT_DEPTH] == 55 && ott_v[3][OT_TIME] == 70 && ott_v[3][OT_UPDN] == 30 && ott_v[3][OT_XO] == 80;
        {
            up_rec_t rr = *up_rec(9);
            rr.packed[UP_OT_AT] = 0;                    /* (a record of before) */
            *up_rec(10) = rr;
        }
        ok &= up_load(10) == 0 && ott_v[3][OT_DEPTH] == 0 && ott_v[3][OT_TIME] == 56;
        ck("7. a user preset keeps the track's OTT (55 70 +30 80); one of before loads with it off", ok);
        memset(up_rec(9), 0, sizeof(up_rec_t)); memset(up_rec(10), 0, sizeof(up_rec_t));
        song.sel = 0;
    }
    {   /* 8. the page: FX > TRACK's fourth, DEPTH TIME UP/DN X-OVER (their formats), drawn; HOME's tag */
        int16_t *vp;
        const param_desc_t *d;
        char v[12];
        const char *u;
        int ok;
        ui_power_on(); go_home(); frame(); press(B_FX); frame();
        turn(EN_SELECT, 3); frame();
        ok = str_eq(cur_page()->title, "OTT") && fx_level() == FXL_TRACK && fx_trk_k(ui.page) == 3;
        d = page_desc(cur_page(), 0, &vp);
        ok &= d && str_eq(d->label, "DEPTH") && vp == &ott_v[song.sel % NTRK][OT_DEPTH];
        turn(EN_K1, 5); frame();
        ok &= ott_v[song.sel % NTRK][OT_DEPTH] > 0 && ott_on(song.sel % NTRK);
        d = page_desc(cur_page(), 3, &vp);
        param_format(d, *vp, v, &u);
        ok &= str_eq(d->label, "X-OVER") && str_eq(v, "187") && str_eq(u, "Hz");
        d = page_desc(cur_page(), 1, &vp);
        param_format(d, *vp, v, &u);
        ok &= str_eq(d->label, "TIME") && str_eq(u, "ms");
        ok &= str_eq(page_desc(cur_page(), 2, &vp)->label, "UP/DN");
        ck("8. FX > TRACK's fourth page OTT: DEPTH (KNOB 1 turns it on) TIME (ms) UP/DN X-OVER (187 Hz)", ok);
        ott_v[song.sel % NTRK][OT_DEPTH] = 0;
    }
    {   /* 9. (v1.1b5) GEN: the electronic genres (gen.c GOTT_G, by their place: the names here) the drums and the bass
         * DEPTH 40 .. 60, a lead / arp / texture 60 .. 80 or off, the chords off, from the seed; every other genre none */
        static const char *const EL[] = {"MINIMAL TECHNO", "HARD TECHNO", "HARD BASS", "HOUSE", "DEEP HOUSE", "TECH HOUSE",
                                         "TRIBAL", "ELECTRO", "BREAKBEAT", "DRUM & BASS", "IDM GLITCH", "EBM", "PHONK"};
        uint32_t m = 0, e, g, k, i, ok, seen = 0, d1[NTRK], ng = 0;
        for (e = 0; e < sizeof EL / sizeof EL[0]; e++)
            for (g = 0; g < NGENRE; g++) if (str_eq(GENRE[g].name, EL[e])) m |= 1u << g;
        ok = m == GOTT_G;
        ui_power_on();
        for (g = 0; g < NGENRE * 2u; g++) {
            uint32_t on = 0, gi = g % NGENRE;
            gp.genre = (uint8_t)(gi + 1u); gp.form = 0; gp.seed = (uint16_t)(300u + g * 7u);
            for (i = 0; i < GP_N; i++) gp.amt[i] = GA_AUTO;
            gp.energy = 60; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30; grd.on = 0; grd.mood = 0;
            gen_make();
            for (k = 0; k < NTRK; k++) {
                int32_t d = ott_v[k][OT_DEPTH], r = gs.role[k];
                on |= d != 0;
                if (!((m >> gi) & 1u)) ok &= d == 0 && ott_v[k][OT_TIME] == 56 && ott_v[k][OT_XO] == 37;
                else if (d) ok &= r == RL_DRUM || r == RL_BASS ? d >= 40 && d <= 60 : r != RL_PAD && d >= 60 && d <= 80;
                if (d) seen |= r > RL_BASS ? 2u : 1u;
            }
            if ((m >> gi) & 1u) { ok &= on != 0; ng++; }
            if (!g) for (k = 0; k < NTRK; k++) d1[k] = (uint32_t)(ott_v[k][OT_DEPTH] << 8 | ott_v[k][OT_XO]);
        }
        gp.genre = 1; gp.seed = 300; gen_make();         /* (MINIMAL TECHNO 0300 again, after the others) */
        for (k = 0; k < NTRK; k++) ok &= d1[k] == (uint32_t)(ott_v[k][OT_DEPTH] << 8 | ott_v[k][OT_XO]);
        ck("9. GEN: 13 electronic genres' drums / bass DEPTH 40..60, a lead / arp / FX 60..80 or off, none elsewhere; by the seed",
           ok && seen == 3u && ng == 26u);
    }
    {   /* 10. (v1.1b5 rel15) RESOUND (GLO + A4, gen.c gen_rs_prep / gen_rs_apply): the notes kept, new sounds, OTT as GEN
         * gives it on the tracks it resounds (an electronic genre's: on; any other's: off), the same seed the same sounds and
         * OTT, GEN's own state (seed, streams) as it was; GLO + G4 the sounds and OTT back */
        uint32_t k, ok = 1, sig[NTRK], on = 0, e0[NTRK];
        uint16_t seed0;
        int16_t o1[NTRK][OT_N];
        ui_power_on();
        gp.genre = 3u + 1u; gp.form = 0; gp.seed = 421; gp.energy = 60; grd.on = 0; grd.mood = 0;   /* (HOUSE: GOTT_G's) */
        gen_make();
        song_draft = 1;
        for (k = 0; k < NTRK; k++) { sig[k] = steps_sig(&trk[k]); e0[k] = trk[k].eng_req << 8 | trk[k].preset; ott_put(k, 0); }
        seed0 = gp.seed;
        undo_mark();
        ok &= gen_rs_prep(1234) && rs_gi == 3;
        gen_rs_apply();
        for (k = 0; k < NTRK; k++) {
            ok &= steps_sig(&trk[k]) == sig[k];
            if (rs_role[k] < RL_TEX + 1u && (rs_role[k] == RL_DRUM || rs_role[k] == RL_BASS)) ok &= ott_v[k][OT_DEPTH] >= 40 && ott_v[k][OT_DEPTH] <= 60;
            on |= ott_v[k][OT_DEPTH] != 0;
        }
        ok &= on && gp.seed == seed0;
        memcpy(o1, ott_v, sizeof o1);
        (void)gen_rs_prep(1234); gen_rs_apply();          /* (from the new sounds: the same OTT, from the seed alone) */
        ok &= !memcmp(o1, ott_v, sizeof o1);
        ok &= !undo_do();
        for (k = 0; k < NTRK; k++) ok &= ott_v[k][OT_DEPTH] == 0 && (uint32_t)(trk[k].eng_req << 8 | trk[k].preset) == e0[k];
        for (k = 0; k < NTRK; k++) ott_v[k][OT_DEPTH] = 30;   /* (a non-electronic genre: RESOUND turns its tracks' OTT off) */
        gs.gi = 13u; while ((GOTT_G >> gs.gi) & 1u) gs.gi++;
        (void)gen_rs_prep(77); gen_rs_apply();
        for (k = 0; k < NTRK; k++) ok &= rs_role[k] < RL_TEX + 1u ? ott_v[k][OT_DEPTH] == 0 : ott_v[k][OT_DEPTH] == 30;
        ck("10. RESOUND: notes kept, OTT as GEN's (HOUSE on, another genre off, untouched tracks kept), seeded; UNDO back", ok);
        for (k = 0; k < NTRK; k++) ott_put(k, 0);
    }
    printf("ott: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
