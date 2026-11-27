/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT 0.9.4) Three engines draw their own creature where every other engine draws its being (ui_being.c): EDIT's
 * SOUND (ui_lzedit.c graph_sound_lz, on the left) and HOME's BEING (ui_lzhome.c, large in the middle); DRUM-X's kit
 * on EDIT > KIT and SOUND 1..4 (the whole panel), the colony staying on KIT 2, KIT 3 and X-MOD. Line art only,
 * antialiased (ui_organic.c og_line), in the beings' thermal colours (bg_col: heat_col; GREY and MONO their greys),
 * from small point tables (int8) and the synthesis state the UI already reads; nothing in the audio path. Each moves
 * only with its sound: in silence it rests (its time runs with the level, as the beings').
 *   VOICE   (0.9.9, "Tracto vivo") the vocal tract, close (the mouth and the neck; the crown off the panel): the head's
 *           outline a faint ghost, the tract a glowing tube of two walls from the lips over the palate and the velum
 *           down the pharynx to the glottis, the trachea under it. The tongue (the tube's floor) takes its height from
 *           F1 and its front / back from F2 (the vowel sounding: sing_view's formants with WORDS on, VOWEL / VOWEL2 /
 *           TALK without), the jaw opens down (and forward) with F1, the lips round (o u w sh) or close (p b m), the
 *           lower lip on the teeth (f v), the tongue's tip, blade or body on the place of the consonant (sing.c
 *           PHON[].place), hot where it closes; the velum drops for m n ng and a branch to the nose opens. The voicing
 *           sends glottal pulses up the tube (each a bulge, through the nose with m n ng), shakes the vocal folds (a V
 *           opening and shutting) and sends sound waves out of the mouth (the nose); turbulence at the constriction for
 *           the frication (s sh f x, the bursts) and at the glottis for the aspiration (h, BREATH); a damped nod at each
 *           vowel's onset (WORDS off: a note's), a slow sway and breath with the level.
 *   ANIMAL  a creature between a bird, an insect and a mammal: its outline, legs, wing, antennae / ears / crest are
 *           the same points in the three (VX_AN), mixed by weights that glide (a metamorphosis, ~0.6 s) to the MODE's
 *           animal (CMOS WHSTL a bird, PULSE WING an insect, GLOTT a mammal), leaned by SIZE past its presets' sizes
 *           (smaller: towards the insect; larger: the mammal) and stirred by CHAOS. With the call (each voice's syllable envelope, CMOS's
 *           chirp gate, WHSTL's syrinx amplitude): the beak / jaw opens and the throat (syrinx, larynx) swells, the
 *           wings buzz at the pulse rate (PULSE, WING), the legs stridulate, the ribs breathe (GLOTT).
 *   DRUM-X  the kit as a colony of small beings with eyes and a mouth, standing as a drum kit does: the kick large in
 *           the middle (its core hot), the snare flat on its stand, the toms on little legs, the hi-hat two shells (CH
 *           together, OH the top one lifted, its mouth open), the crash on a long stem and the cowbell a small head (BELL),
 *           the clap two little ones clapping together, the rim (RS) a spark on the snare's edge. A hit squashes its
 *           being (springing back), shuts its eyes and opens its mouth in a shout, as hard as the hit (the lane's own
 *           envelope: its velocity, its accent; it recovers with the sound's DECAY): a GHOST a small squash, a FLAM two
 *           (its hit to come a shadow), a ratchet repeated squashes. On EDIT > KIT (EDIT 1) the sound last struck named
 *           over it; on SOUND 1..4 the sound edited lit and named; HOME's BEING for a DRUM track. KIT 2, KIT 3 and
 *           X-MOD keep the colony of 0.5 (ui_being.c graph_colony: a being per sound, in two rows).
 * (JIANT 0.9.6) Six more, ported from the approved mock-up (the family's): line art in the same thermal colours (the JIANT
 * palette: DRUM-X's ramp, the mock-up's; the others as the beings), each moving only with its sound, the bichos' face
 * (vx_face) and outline (vx_blob) shared with DRUM-X's beings:
 *   ANALOG  CÉLULA: a cell whose membrane (three isotherms) is two cycles of the oscillators (WAVE, MORPH, DETUNE, MIX,
 *           NOISE, INT) through the filter (CUT on the note, RES) and DRIVE, in polar form; spikes where DRIVE clips it, a
 *           ring at CUT glowing and spreading with RES, cilia with NOISE, the two oscillators orbiting at DETUNE's beat
 *   FM6     COLONIA: the six operators as bichos wired as the algorithm (FM6_CELL, fm6_routes: carriers on the output's
 *           floor, modulators above), each as large and hot as its LEVEL times its envelope now (the sounding notes'),
 *           its spikes its RATIO, pulses down the wires, FB a curled tail; on EDIT, on OPERATOR / OPERATOR 2 (the
 *           operator picked in the accent, DETUNE a shimmer) and HOME. OP ENV: the envelope, its bicho riding it
 *   WHEEL   TUBÍCOLAS: nine tube worms, one a drawbar, as tall as its level (the digits under them), their crowns as
 *           many tentacles as the harmonic; the rotor a current bending them (its real phase and speed), PERC the
 *           worm it strikes, CLICK bubbles, DUST motes
 *   WEST    (V10.7.36, NOISE's ENJAMBRE before) LA ONDA PLEGADA: the folded wave as the engine folds it, brighter and
 *           further folded as the gate opens, the lamp glowing above it, DUST's motes, a face under it
 *   PHASE   CARACOL: a snail whose shell's chambers sit at the bent phase (WAVE / WAVE2 at DCW, crowding where it
 *           rushes), the foot's sole the wave, FB frays the spiral, SUB its outer whorl, the eyestalks pulling in on a
 *           note; the second line a little one following (RING a thread)
 *   LOFI    ORGANISMO DE CIRCUITO: a caterpillar whose two skins are the last 256 samples out (the scope's: nothing more
 *           from the audio), 8 segments the byte's bits (BYTE: the highest next to the head; the legs lift with their bit,
 *           the antennae the two lowest), lit by the wave's level elsewhere (FLOAT, PLS TRI SAW NOIS WRAM)
 * (JIANT 0.9.9-snd5) Two more, the same family's:
 *   STRING  TEJEDORA: a spider on its web, one thread a voice sounding, each thread the string itself (its delay
 *           line, one period of what the loop holds now: the real standing wave, ringing down as it decays); the
 *           spider sits at POS on the top thread and plucks with a front leg at each note (saws while BOW feeds it), a
 *           cocoon under the web as large as SIZE glows with BODY; BRIGHT warms the silk
 *   ADD     (MEDUSA, the jellyfish of its partials, until V10.7.13: gone for GEN's FORM MY LOOP; the being now)
 * Each takes a rectangle (vx_draw: EDIT's left panel, HOME's middle, or a whole panel) and scales to it.
 * State: vxs (in the pool: no global of the RAM region moves). MENU > ANIM OFF: they rest. */
#define VX_POOL __attribute__((section(".pool")))
#define VX_CODE __attribute__((noinline))                 /* (each creature its own function: its size on its own) */
#define VX_SEG BTAPP_FN                                  /* (V10.48) LOFI's, ANIMAL's and STRING's creatures in the Bluetooth
                                                         * segment (the app slot full): without it vx_draw says none (the
                                                         * being instead) */
#define VX_NE 9                                          /* the creatures: VOICE ANIMAL DRUM-X ANALOG FM6 WHEEL WEST PHASE LOFI
                                                         * ((0.9.9-snd5) STRING ADD: PHASE's and WHEEL's slots, below) */
typedef struct { int32_t cx, cy, s; } vxf_t;            /* a unit (Q4: 16) -> Q4 px: c + u * s / 256 */
static struct {
    uint32_t tm[VX_NE], fr[VX_NE];                       /* each one's time (it runs with its sound), the frame it ran */
    int32_t en[VX_NE];                                   /* its level now (0..256) */
    uint32_t mt[VX_NE], ms[VX_NE];                       /* (0.9.6) the mock-up's time (Q8 frames, 30 a second: with the sound), ms */
    uint32_t nage[VX_NE], nms[VX_NE];                    /* .. the newest note seen (voice_t.age), when it came (fm1_ms) */
    int16_t wv[256];                                     /* ANALOG's / PHASE's two cycles; LOFI's last 256 samples out (Q12) */
    uint32_t rph[2], bub[9], fhold;                      /* WHEEL: the rotors as last seen sounding, CLICK's bubbles; OP ENV's hold */
    int16_t bd[8];                                       /* LOFI: the byte's bits, slewed (0..256) */
    uint32_t crawl, cmt;                                 /* .. its crawl (16-bit turns), the time it was taken at */
    int16_t dal, frel;                                   /* lines drawn this much fainter (256 - alpha); OP ENV: the release from */
    int16_t art[11];                                     /* VOICE: the articulation, slewed (vx_voice) */
    int16_t js, jc, hs, hc, hk;                          /* .. the jaw's turn (sin, cos Q15) about the condyle; the head's
                                                          * about the neck (the nod and the sway), its breath (Q15, - 1) */
    int32_t nod, nodv;                                   /* .. the nod (Q15 rad) and its speed: a damped spring, struck at a
                                                          * vowel's onset (WORDS off: a note's) */
    uint32_t vmt, vph;                                   /* .. the mock-up's frames it has stepped; the sway's / breath's phase */
    uint16_t pw[2][4];                                   /* .. the glottal pulses (how far up the tract, Q16; 0 none), the
                                                          * waves leaving (their radius, Q4 units; 0 none) */
    uint8_t wh[4], pwn[2], pwi[2], pwk[2], vow;          /* .. the waves' heat; the pulses' / waves' nose bits, the next slot
                                                          * of each, the frames since the last; a vowel (1), its onset to come (2) */
    int16_t w[3];                                        /* ANIMAL: bird insect mammal weights (0..256) */
    uint8_t age[8], last;                                /* DRUM-X: each lane's age seen; the lane struck last */
    uint32_t hitm[8], tl[8], kms;                        /* .. its last hit (fm1_ms), its being's time; the ms drawn */
    uint8_t kc;                                          /* .. drawing it (its colours: vx_col) */
    uint8_t init;
} vxs VX_POOL;

/* DRUM-X's beings in the thermal ramp of the mock-up the kit was chosen from (cyan blue violet red orange yellow white:
 * the 0.6's, the JIANT palette); every other palette as the beings (bg_col) */
static const uint8_t VK_HP[8] = {0, 26, 51, 82, 118, 154, 200, 255};
static const uint16_t VK_HC[8] = {0x13D4u, 0x2DFCu, 0x4B5Fu, 0x8B7Fu, 0xFA49u, 0xFC43u, 0xFE87u, 0xFFFFu};
static uint16_t vx_col(int32_t h)
{
    uint32_t k = 0;
    if (h < -1)                                          /* (0.9.6: -2 the theme's: the operator picked, the mock-up's) */
        return T_THEME;
    if (h < 0)                                           /* (a muted sound: dim, as the colony's) */
        return T_DIM;
    h = clamp(h, 0, 255);
    if (!vxs.kc)
        return bg_col(h);
    while (k < 6u && h > VK_HP[k + 1u]) k++;
    return ux_mix(VK_HC[k], VK_HC[k + 1u], (h - VK_HP[k]) * 100 / (VK_HP[k + 1u] - VK_HP[k]));
}
/* (0.9.7) HOME's beings (ui_lzhome.c, variant A): the same ramp on JIANT, every other palette as the beings (bg_col) */
static uint16_t vk_heat(int32_t h)
{
    uint32_t k = 0;
    h = clamp(h, 0, 255);
    while (k < 6u && h > VK_HP[k + 1u]) k++;
    return ux_mix(VK_HC[k], VK_HC[k + 1u], (h - VK_HP[k]) * 100 / (VK_HP[k + 1u] - VK_HP[k]));
}
static void vx_line(const vxf_t *f, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h)
{
    int32_t X0 = f->cx + ((x0 * f->s) >> 8), Y0 = f->cy + ((y0 * f->s) >> 8);
    int32_t X1 = f->cx + ((x1 * f->s) >> 8), Y1 = f->cy + ((y1 * f->s) >> 8);
    og_line_a(X0, Y0, X1, Y1, vx_col(h), 256 - vxs.dal);
}
/* a smooth curve through n points (Q4 units, x y pairs): quadratic arcs from midpoint to midpoint, each point their
 * control; closed or open (from the first point to the last). vx_bez: its 1 + 3 e points into o (e: its arcs, returned) */
static uint32_t vx_bez(const int32_t *p, uint32_t n, int closed, int32_t *o)
{
    uint32_t i, k, e = closed ? n : n - 1u;
    for (i = 0; i < e; i++, o += 6) {
        const int32_t *a = &p[2u * (i ? i - 1u : (closed ? n - 1u : 0u))], *b = &p[2u * i], *c = &p[2u * ((i + 1u) % n)];
        int32_t x0 = (a[0] + b[0]) / 2, y0 = (a[1] + b[1]) / 2, x1 = (b[0] + c[0]) / 2, y1 = (b[1] + c[1]) / 2;
        if (!closed && i == 0u) { x0 = b[0]; y0 = b[1]; }
        if (!closed && i + 1u == e) { x1 = c[0]; y1 = c[1]; }
        if (i == 0u) { o[0] = x0; o[1] = y0; }
        for (k = 1; k <= 3u; k++) {
            int32_t t = (int32_t)k * 85, u = 256 - t;   /* (t / 256: thirds) */
            o[2u * k] = k == 3u ? x1 : (u * u * x0 + 2 * u * t * b[0] + t * t * x1) >> 16;
            o[2u * k + 1u] = k == 3u ? y1 : (u * u * y0 + 2 * u * t * b[1] + t * t * y1) >> 16;
        }
    }
    return e;
}
static void vx_curve(const vxf_t *f, const int32_t *p, uint32_t n, int closed, int32_t h)
{
    int32_t o[2 * (1 + 3 * 24)];                         /* (24 points closed at most: seq-v2's STRING draws 24, its
                                                         * silk; main's 16 overran the stack: p2, ui_render under ASan) */
    uint32_t i, e = 3u * vx_bez(p, n, closed, o);
    uint16_t c = vx_col(h);
    for (i = 0; i <= e; i++)                             /* (codex/anim: at the load's detail, ui_organic.c pl_pt) */
        pl_pt(i, e, f->cx + ((o[2u * i] * f->s) >> 8), f->cy + ((o[2u * i + 1u] * f->s) >> 8), c, 256 - vxs.dal);
}
/* an ellipse (Q4 units), n sides, its top at angle a0 .. a1 (16-bit turns) */
static void vx_arc(const vxf_t *f, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t a1, uint32_t n,
                   int32_t h)
{
    uint32_t i;
    int32_t px = 0, py = 0;
    for (i = 0; i <= n; i++) {
        uint32_t a = a0 + (uint32_t)(((uint64_t)(a1 - a0) * i) / n);
        int32_t x = cx + ((rx * og_cos(a)) >> 15), y = cy + ((ry * og_sin(a)) >> 15);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
static uint32_t vx_mt(uint32_t ms) { return ms / 25u * 192u + ms % 25u * 192u / 25u; }   /* (codex/anim) ms -> the mock-up's
                                                         * Q8 frames (30 a second), whole: steps of any size add up the same */
/* the level of track t now (0..256: its part's peak, slewed: a hit at once, a quick fall), k: which drawing; its time
 * runs with it (once a frame) */
static int32_t vx_level(const track_t *t, uint32_t k, int32_t en_home)
{
    uint32_t dt = anim_fr - vxs.fr[k], dm = fm1_ms - vxs.ms[k], st = (anim_fr >> 1) - (vxs.fr[k] >> 1);   /* (codex/anim: the
                                                         * clock's ticks, not the frames; st: their pairs) */
    int32_t v, j;
    if (!dt)
        return vxs.en[k];
    if (dt > 8u) dt = 8u;
    vxs.fr[k] = anim_fr;
    vxs.ms[k] = fm1_ms;
    if (dm > 250u) dm = 250u;                            /* (a pause: no jump; codex/anim: 4 frames a second still the time) */
    if (vxs.en[k])                                       /* (0.9.6) the mock-up's time: 30 frames a second while it sounds */
        vxs.mt[k] += vx_mt(fm1_ms) - vx_mt(fm1_ms - dm);
    if (ANIM_OFF())
        return vxs.en[k] = 0;                            /* (ANIM OFF: at rest) */
    if (en_home >= 0)
        v = en_home;                                     /* (HOME: its own, eco_step's) */
    else {
        v = ((track_t *)t)->peak * 256 / 7000;
        ((track_t *)t)->peak = 0;
        v = v > 256 ? 256 : v;
        for (dm = 0, j = vxs.en[k]; dm < st && dm < 8u; dm++)   /* (the fall: a step each two ticks, as at full pace) */
            j = v > j ? v : j + (v - j - 2) / 3;
        v = j;
    }
    v = v > 4 ? v : 0;                                   /* (HOME's level rests a little above 0: silence) */
    vxs.en[k] = v;
    if (v)
        vxs.tm[k] += dt * (uint32_t)(120 + v * 4);
    return v;
}

/* ------------------------------------------------------------------------------------------------ DRUM-X --- */
/* the kit as a colony of small beings (units ~124 x 90: x right, y down, the floor at 40), each drum one with eyes and
 * a mouth: the kick large in the middle, the snare flat on its stand, the toms on little legs, the hi-hat two shells,
 * the crash on a long stem, the cowbell a small head over the kick, the clap two little ones clapping */
enum { VK_KICK, VK_SNARE, VK_CLAP, VK_HATC, VK_HATO, VK_TOM, VK_RIM, VK_BELL };
/* 256 * 2^(-ms / half), Q8 (the squash's fall) */
static int32_t vx_fall(uint32_t ms, uint32_t half)
{
    uint32_t n = ms / half;
    int32_t v;
    if (n > 8u) return 0;
    v = 256 >> n;
    return v - (int32_t)((uint32_t)v * (ms % half) / (2u * half));
}
/* a being's outline (ui_being.c bg_outline's, its lobes 3, the spikes' sharpness 4), squashed: at (x, y), radius r
 * (Q4 units), x stretched by ax / 256, then scaled sx sy / 256 about x and its foot (by: y's place after the squash) */
typedef struct { int32_t x, y, by, ax, sx, sy, sp, len, wob, rip, lob, shp; } vxb_t;   /* (0.9.6: lob lobes, shp 4 or 1) */
static void vx_blob(const vxf_t *f, const vxb_t *b, int32_t r, int outer, uint32_t t, uint32_t nv, int32_t h)
{
    uint32_t i, lb = (uint32_t)b->lob;
    int32_t px = 0, py = 0;
    for (i = 0; i <= nv; i++) {
        uint32_t th = (i % nv) * 65536u / nv;
        int32_t rr = r + ((((r * b->wob) >> 8) * ((9175 * og_sin(lb * th + t) + 3932 * og_sin((lb + 2u) * th - 2u * t)) >> 15)) >> 15), x, y;
        if (outer) {
            int32_t c2 = og_sin(((uint32_t)b->sp * th >> 1) + 0x4000u), p = c2 > 0 ? c2 : 0;
            if (b->shp > 1) {
                p = (p * p) >> 15;
                p = (p * p) >> 15;                       /* (c2^4) */
            }
            rr += (((((r * b->len) >> 8) * p) >> 15) * 26) >> 4;
            rr += ((((r * b->rip) >> 8) * og_sin(th * 18u + 3u * t)) >> 15) / 14;
        }
        x = b->x + (((((rr * og_cos(th)) >> 15) * b->ax) >> 8) * b->sx >> 8);
        y = b->by + ((((rr * og_sin(th)) >> 15) * b->sy) >> 8);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
/* a small dot at (x, y) (Q4 units), radius r (Q4 px: under 1.5 px a 2 x 2 dot) */
static void vx_dot(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t h)
{
    uint16_t c = vx_col(h);
    int32_t X = f->cx + ((x * f->s) >> 8), Y = f->cy + ((y * f->s) >> 8);
    if (r >= 32) {
        og_node(X, Y, r >> 4, c);
        return;
    }
    X = (X - 8) >> 4;
    Y = (Y - 8) >> 4;
    for (r = 0; r < 4; r++)
        og_plot(X + (r & 1), Y + (r >> 1), c, 256);
}
/* the bichos' face (DRUM-X's beings, the 0.9.6 creatures): two eyes ex either side of x at ey (shut: short lines,
 * else dots of er Q4 px), the mouth at my: a line mw either side, or open (mo) an ellipse mx wide; their heats */
static void vx_face(const vxf_t *f, int32_t x, int32_t ey, int32_t ex, int32_t er, int shut, int32_t my, int32_t mw,
                    int32_t mo, int32_t mx, int32_t he, int32_t hm, int32_t hl)
{
    int32_t k;
    for (k = -1; k <= 1; k += 2) {
        if (shut)
            vx_line(f, x + k * ex - 22, ey, x + k * ex + 22, ey, he);
        else
            vx_dot(f, x + k * ex, ey, er, he);
    }
    if (mo)
        vx_arc(f, x, my + mo / 2, mx, mo, 0, 65536u, 12, hm);
    else
        vx_line(f, x - mw, my, x + mw, my, hl);
}
/* .. as the mock-up's bface: at (x, y) for a body of radius r (Q4 units), m its shout (0..256), shut its eyes; hot
 * warms the mouth (0..256) */
static void vx_bface(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t m, int shut, int32_t hot)
{
    int32_t ey = y - r / 10, mo = clamp(m, 0, 256) * r * 42 / 25600, er = ((r * f->s) >> 8) / 10;
    vx_face(f, x, ey, r * 36 / 100, er, shut, mo > 8 ? ey + r * 36 / 100 : ey + r * 34 / 100, r * 14 / 100,
            mo > 8 ? mo : 0, r * 16 / 100 + mo * 22 / 100, 243, 205 + hot / 5, 154);
}
/* a drum's being at (x, y), rx x ry (Q4 units): e its hit now (0..256: the lane's envelope), age since it (ms), t its
 * time (it runs with the sound), sp its spikes, len their length, legs (Q4 units, 0: none); lit 1 the sound edited, -1
 * muted. Struck it squashes (springing back with the hit's age), shuts its eyes and shouts (the mouth open) */
static void vx_bug(const vxf_t *f, int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t e, uint32_t age, uint32_t t,
                   int32_t sp, int32_t len, int32_t legs, int32_t lit)
{
    vxb_t b;
    int32_t sq = e ? (((e * og_cos(age * 229u)) >> 15) * vx_fall(age, 115u)) >> 8 : 0;   /* (22 rad/s, e^-6t) */
    int32_t k, ey, ex, mo, hb = lit > 0 ? 50 : 0, ho = lit < 0 ? -1 : 41 + e * 77 / 256 + hb;
    int32_t hi = lit < 0 ? -1 : 92 + e * 90 / 256 + hb, he = lit < 0 ? -1 : 243;
    uint32_t nv = (uint32_t)clamp(((rx + ry) * f->s >> 12) * 2, 20, 64);
    if (nv < (uint32_t)sp * 3u) nv = (uint32_t)sp * 3u;
    if (sys_load) nv = nv * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);   /* (LITE, ECO) */
    b.x = x;
    b.y = y;
    b.sx = 256 + sq * 90 / 256;
    b.sy = 256 - sq * 102 / 256;
    b.by = y + ry * (256 - b.sy) / 256;
    b.ax = rx * 256 / ry;
    b.sp = sp;
    b.len = len + e * 60 / 256;
    b.wob = 40 + e * 120 / 256;
    b.rip = e * 100 / 256;
    b.lob = 3;
    b.shp = 4;
    if (legs)
        for (k = -1; k <= 1; k += 2) {
            int32_t x1 = x + k * rx * 7 / 10, y1 = y + ry + legs * 6 / 10;
            vx_line(f, x + k * rx / 2, y + ry * 7 / 10, x1, y1, lit < 0 ? -1 : 56 + hb);
            vx_line(f, x1, y1, x + k * rx * 6 / 10, y + ry + legs, lit < 0 ? -1 : 56 + hb);
        }
    vx_blob(f, &b, ry, 1, t, nv, ho);                    /* the membrane (cold), the core */
    vx_blob(f, &b, ry * 66 / 100, 0, t, nv * 2u / 3u, hi);
    ey = b.by - ry * 15 * b.sy / 25600;
    ex = rx * 32 * b.sx / 25600;
    mo = e * ry * 4 / 2560;                              /* the eyes shut on a hard hit; the mouth a shout as loud as it */
    vx_face(f, x, ey, ex, ((ry * 12 / 100 * f->s) >> 12) << 4, e > 90, mo > 6 ? ey + ry * 32 * b.sy / 25600 : ey + ry * 3 / 10,
            ry * 15 / 100, mo > 6 ? mo : 0, ry * 18 / 100 + mo / 5, he, 205, lit < 0 ? -1 : 154);
}
/* the kit at (cx, cy) Q4 px, s: a unit's px (Q8); sel: the sound edited (lit), -1 none */
static void vx_kit(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home, int32_t sel)
{
    vxf_t f = {cx, cy, s};
    drum_lane_t *K = drum_kit_of(t);
    int32_t e[8], lt[8], en = vx_level(t, 2, en_home), i, k;
    uint32_t ag[8], now = fm1_ms, dt = now - vxs.kms;
    if (dt > 100u) dt = 100u;                            /* (a pause: no jump) */
    vxs.kms = now;
    vxs.kc = 1;
    for (i = 0; i < 8; i++) {                            /* each lane's hit now: its envelope (velocity, accent, decay) */
        int32_t a = K ? K[i].age : 255;
        lt[i] = dx_lane_muted((uint32_t)i) ? -1 : i == sel;
        e[i] = K && K[i].x.live && lt[i] >= 0 && !ANIM_OFF() ? clamp(K[i].x.ea >> 22, 0, 256) : 0;
        if (K && K[i].dm == 2u) e[i] = e[i] * 2 / 3;   /* (GHOST: a small squash) */
        if (K && a < vxs.age[i] && e[i]) {               /* a new hit (a FLAM's two, a ratchet's each) */
            vxs.hitm[i] = now;
            vxs.last = (uint8_t)i;
        }
        vxs.age[i] = (uint8_t)a;
        ag[i] = now - vxs.hitm[i] < 4000u ? now - vxs.hitm[i] : 4000u;
        if (en || e[i])                                  /* (its time runs with the sound: in silence it rests) */
            vxs.tl[i] += dt * (uint32_t)(60 * 256 + e[i] * 300) * 30u / 256000u;
    }
    vx_line(&f, -60 * 16, 40 * 16, 60 * 16, 40 * 16, 26);   /* the floor */
    /* the crash (BELL) on its stem */
    vx_line(&f, 46 * 16, -24 * 16, 48 * 16, 40 * 16, 41);
    vx_bug(&f, 44 * 16, -30 * 16, 16 * 16, 4 * 16, e[VK_BELL], ag[VK_BELL], vxs.tl[VK_BELL] + 3000u, 9, 40, 0, lt[VK_BELL]);
    /* the hi-hat: two shells on their stand, the top one lifting (OH: its mouth open), together on CH */
    {
        int32_t op = e[VK_HATO] > 5 ? 48 + e[VK_HATO] * 80 / 256 : e[VK_HATC] > 5 ? 0 : 24;
        vx_line(&f, -52 * 16, -2 * 16, -52 * 16, 40 * 16, 41);
        vx_bug(&f, -52 * 16, 0, 12 * 16, 54, e[VK_HATC] * 3 / 5, ag[VK_HATC], vxs.tl[VK_HATC], 12, 10, 0, lt[VK_HATC]);
        vx_bug(&f, -52 * 16, -5 * 16 - op, 12 * 16, 54, e[VK_HATC] > e[VK_HATO] ? e[VK_HATC] : e[VK_HATO],
               ag[VK_HATC] < ag[VK_HATO] ? ag[VK_HATC] : ag[VK_HATO], vxs.tl[VK_HATO] + 900u, 12, 10, 0,
               lt[VK_HATO] < 0 && lt[VK_HATC] < 0 ? -1 : lt[VK_HATO] > 0 || lt[VK_HATC] > 0);
    }
    /* the toms (TOM): two on little legs over the kick, the floor tom on longer ones */
    vx_bug(&f, -15 * 16, -14 * 16, 9 * 16, 7 * 16, e[VK_TOM], ag[VK_TOM], vxs.tl[VK_TOM] + 200u, 5, 20, 6 * 16, lt[VK_TOM]);
    vx_bug(&f, 15 * 16, -14 * 16, 9 * 16, 7 * 16, e[VK_TOM] * 3 / 4, ag[VK_TOM] + 30u, vxs.tl[VK_TOM] + 400u, 5, 20, 6 * 16, lt[VK_TOM]);
    vx_bug(&f, 40 * 16, 18 * 16, 11 * 16, 10 * 16, e[VK_TOM] / 2, ag[VK_TOM] + 60u, vxs.tl[VK_TOM] + 600u, 5, 20, 12 * 16, lt[VK_TOM]);
    /* the kick, large in the middle; the snare flat on its stand */
    vx_bug(&f, 0, 16 * 16, 22 * 16, 20 * 16, e[VK_KICK], ag[VK_KICK], vxs.tl[VK_KICK] + 1000u, 4, 14, 4 * 16, lt[VK_KICK]);
    vx_bug(&f, -34 * 16, 10 * 16, 13 * 16, 6 * 16, e[VK_SNARE], ag[VK_SNARE], vxs.tl[VK_SNARE] + 1400u, 14, 12, 22 * 16, lt[VK_SNARE]);
    /* the rim (RS): a spark on the snare's edge, its rays turning as it rings */
    if (e[VK_RIM] > 5 || lt[VK_RIM] > 0)
        for (k = 0; k < 6; k++) {
            uint32_t a = (uint32_t)k * 10923u + ag[VK_RIM] * 63u;
            int32_t r1 = 3 * 16 + e[VK_RIM] * 4 * 16 / 256;
            vx_line(&f, -47 * 16 + ((32 * og_cos(a)) >> 15), 8 * 16 + ((32 * og_sin(a)) >> 15), -47 * 16 + ((r1 * og_cos(a)) >> 15),
                    8 * 16 + ((r1 * og_sin(a)) >> 15), 154 + e[VK_RIM] * 102 / 256);
        }
    og_node(cx + ((-47 * 16 * s) >> 8), cy + ((8 * 16 * s) >> 8), 1, vx_col(lt[VK_RIM] < 0 ? -1 : 102 + e[VK_RIM] * 154 / 256));
    /* the cowbell (BELL): a small head over the kick */
    vx_bug(&f, 0, -30 * 16, 6 * 16, 5 * 16, e[VK_BELL], ag[VK_BELL], vxs.tl[VK_BELL] + 1800u, 3, 30, 0, lt[VK_BELL]);
    /* the clap: two little ones, apart, clapping together */
    {
        int32_t g = 8 * 16 - e[VK_CLAP] * 6 * 16 / 256;
        vx_bug(&f, -27 * 16 - g, -33 * 16, 4 * 16, 4 * 16, e[VK_CLAP], ag[VK_CLAP], vxs.tl[VK_CLAP] + 2200u, 5, 20, 0, lt[VK_CLAP]);
        vx_bug(&f, -27 * 16 + g, -33 * 16, 4 * 16, 4 * 16, e[VK_CLAP], ag[VK_CLAP], vxs.tl[VK_CLAP] + 2600u, 5, 20, 0, lt[VK_CLAP]);
    }
    /* a FLAM's hit to come: the being's shadow, doubled */
    for (i = 1; i < 8; i++)
        if (K && K[i].fl) {
            static const int8_t PX[8] = {0, -34, -27, -52, -52, -15, -47, 44}, PY[8] = {16, 10, -33, 0, -8, -14, 8, -30};
            vx_arc(&f, PX[i] * 16 + 2 * 16, PY[i] * 16 + 16, 6 * 16, 3 * 16, 0, 65536u, 12, 64);
        }
    vxs.kc = 0;
}

/* the squash of a hit (Q8): e (0..256) at age ms (22 rad/s, e^-6t) */
static int32_t vx_sq(int32_t e, uint32_t age)
{
    return (((e * og_cos(age * 229u)) >> 15) * vx_fall(age, 115u)) >> 8;
}
/* ms since track t's newest note (k: which creature saw it; 4000 at most); *note: that note */
static uint32_t vx_age(const track_t *t, uint32_t k, int32_t *note)
{
    uint32_t i, a = 0, n = 60;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > a) { a = t->v[i].age; n = t->v[i].note; }
    if (a > vxs.nage[k])
        vxs.nms[k] = fm1_ms;
    vxs.nage[k] = a;
    if (note) *note = (int32_t)n;
    return fm1_ms - vxs.nms[k] < 4000u ? fm1_ms - vxs.nms[k] : 4000u;
}
/* a polyline of n points (Q4 units: x y pairs), closed or not */
static void vx_poly(const vxf_t *f, const int32_t *p, uint32_t n, int closed, int32_t h)
{
    uint32_t i;
    for (i = 1; i < n; i++)
        vx_line(f, p[2u * i - 2u], p[2u * i - 1u], p[2u * i], p[2u * i + 1u], h);
    if (closed)
        vx_line(f, p[2u * n - 2u], p[2u * n - 1u], p[0], p[1], h);
}
/* a ring (Q4 units), its sides as many as its size asks */
static void vx_ring(const vxf_t *f, int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t h)
{
    uint32_t n = (uint32_t)clamp(((rx + ry) * f->s) >> 12, 8, 28);
    vx_arc(f, x, y, rx, ry, 0, 65536u, n, h);
}
/* a bicho's body (isoU): its outer outline (the spikes) of radius r, its inner one ri (0: none); g its features (spikes,
 * their length, wobble, ripple, lobes, sharpness, x stretch) and its place (x, by, squashed sx sy about it) */
static void vx_body(const vxf_t *f, const vxb_t *g, int32_t r, int32_t ri, uint32_t t, int32_t ho, int32_t hi, uint32_t no, uint32_t ni)
{
    if (sys_load) {                                      /* (LITE, ECO) */
        no = no * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);
        ni = ni * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);
    }
    vx_blob(f, g, r, 1, t, no, ho);
    if (ri) vx_blob(f, g, ri, 0, t, ni, hi);
}
/* text s centred on (x, its capitals' top y) Q4 px */
static uint8_t vx_small;                                 /* (V10.8) LARGE's strip: the creatures small, no labels */

/* --------------------------------------------------------------------------------------------- LOFI --- */
/* ORGANISMO DE CIRCUITO */
static VX_SEG void vx_lofi(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    int32_t a0 = A[2] * 256 / 124, b0 = (A[3] - 4) * 256 / 94, fl = p[P_E1] != RW_BYTE, new_fr = vxs.fr[8] != anim_fr;
    int32_t en = vx_level(t, 8, en_home), i, j, pk = 6000, sx[9], sy[9], bc[8], px = 0, py = 0, qx = 0, qy = 0, q[2 * 22], g = fl ? 38 : 27;
    uint32_t mt = vxs.mt[8], age = vx_age(t, 8, 0), w0 = scope_w - 256u, byte = 0, cr;
    int32_t sq = vx_sq(vx_fall(age, 154u), age);
    vxf_t f;
    f.s = (a0 < b0 ? a0 : b0) * 112 / 100;
    f.cx = (A[0] * 2 + A[2]) * 8 - ((32 * f.s) >> 8);
    f.cy = (A[1] * 2 + A[3]) * 8;
    vxs.kc = 1;
    for (i = 0; !cv_band && i < 256; i++) {              /* the last 256 samples out (the scope's): their peak (devcpu3:
                                                          * a panel's first band; the others draw what it took) */
        int32_t v = scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)];
        v = v < 0 ? -v : v;
        pk = v > pk ? v : pk;
    }
    for (i = 0, j = (4096 << 15) / pk; !cv_band && i < 256; i++)
        vxs.wv[i] = (int16_t)((scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)] * j) >> 15);
#define VH(i) vxs.wv[i]
    byte = (uint32_t)(VH(255) / 32 + 128) & 255u;       /* the byte now: BYTE's held sample, else the output's level */
    for (i = 0, j = 0; i < NVOICE; i++)
        if (!fl && t->v[i].active && t->v[i].age >= (uint32_t)j) { j = (int32_t)t->v[i].age; byte = (uint32_t)((t->v[i].s[0] + 32640) >> 8) & 255u; }
    for (i = 0; i < 8; i++) {                            /* (its bits slewed, once a frame) */
        if (new_fr)
            vxs.bd[i] = (int16_t)(vxs.bd[i] + (((byte >> i & 1u) ? en : 0) - vxs.bd[i]) * 141 / 256);
        bc[i] = clamp((vxs.bd[i] - 46) * 410 / 256, 0, 256);
    }
    vxs.crawl += ((mt - vxs.cmt) * (uint32_t)(307 + 4 * en) >> 8) * 10430u / 7680u;   /* the crawl */
    vxs.cmt = mt;
    cr = vxs.crawl;
    for (i = 0; i <= 8; i++) {                           /* the spine: the head, then 8 segments back */
        int32_t amp = (26 + (en * 54 >> 8)) * (77 + i * 179 / 8) / 256;
        sx[i] = i ? (27 * 16 - (i - 1) * 149) : 38 * 16;
        sy[i] = 6 * 16 + ((og_sin(cr * 22u / 10u - (uint32_t)i * 8344u) * amp) >> 15) - (i ? 0 : 64);
    }
#define VR(i) ((i) ? 147 - (i) * 8 : 176)
    vx_line(&f, -54 * 16, 33 * 16, 54 * 16, 33 * 16, 26);
    for (i = 1; i <= 8; i++) {                           /* legs: lifted where the bit is high */
        int32_t r = VR(i), b = bc[8 - i], sw = (og_sin(cr * 4u + (uint32_t)i * 13560u) * 22) >> 15, k;
        for (k = -1; k <= 1; k += 2) {
            q[0] = sx[i] + k * 29; q[1] = sy[i] + (r * 179 >> 8);
            q[2] = sx[i] + k * 45 + sw; q[3] = sy[i] + r + 38 - (b * 19 >> 8);
            q[4] = sx[i] + k * 35 + (sw * 333 >> 8); q[5] = sy[i] + r + 96 - (b * 48 >> 8);
            q[5] = q[5] < 33 * 16 ? q[5] : 33 * 16;
            vx_poly(&f, q, 3, 0, 51 + b / 5);
        }
    }
    for (j = 0; j <= 80; j++) {                          /* the two skins: the samples themselves along the body */
        int32_t u = 256 + j * 7 * 256 / 80, i0 = u >> 8 < 7 ? u >> 8 : 7, fu = u - i0 * 256;
        int32_t x = sx[i0] + (sx[i0 + 1] - sx[i0]) * fu / 256, y = sy[i0] + (sy[i0 + 1] - sy[i0]) * fu / 256;
        int32_t dx = sx[i0 + 1] - sx[i0], dy = sy[i0 + 1] - sy[i0], adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        int32_t L = (adx > ady ? adx + ady * 3 / 8 : ady + adx * 3 / 8) + 1, j5 = j * 16 / 5;
        int32_t r = (VR(i0) + (VR(i0 + 1) - VR(i0)) * fu / 256) * (256 - ((((j5 * j5 >> 8) * (j5 * j5 >> 8) >> 8) * j5 >> 8) * 141 >> 8)) / 256 + 26;   /* (thinner to the tail) */
        int32_t s1 = VH(j * 127 / 80), s2 = VH(128 + j * 127 / 80), d1 = r + ((s1 * g) >> 12), d2 = r + ((s2 * g) >> 12);
        int32_t tx = x + dy * d1 / L, ty = y - dx * d1 / L, bx = x - dy * d2 / L, by = y + dx * d2 / L;
        if (j) {
            vx_line(&f, px, py, tx, ty, 46 + (s1 < 0 ? -s1 : s1) * 130 / 4096 + (en * 40 >> 8));
            vx_line(&f, qx, qy, bx, by, 46 + (s2 < 0 ? -s2 : s2) * 130 / 4096 + (en * 40 >> 8));
        }
        px = tx; py = ty; qx = bx; qy = by;
    }
    q[0] = px; q[1] = py; q[2] = sx[8] - 96; q[3] = sy[8]; q[4] = qx; q[5] = qy;
    vx_curve(&f, q, 3, 0, 60);
    for (i = 8; i >= 1; i--) {                           /* the segments: rings lit by their bit (or the wave's level) */
        int32_t r = VR(i), v = VH(i * 30 + 6), m;
        v = fl ? clamp((v < 0 ? -v : v) * 115 / 100, 0, 4096) * 218 / 4096 + 13 : bc[8 - i];
        for (m = 0; m < 18; m++) {
            uint32_t a = (uint32_t)m * 65536u / 18u;
            int32_t w = 4096 + VH((i * 29 + m * 9) & 255) * 9 / 100;
            q[2 * m] = sx[i] + ((((og_cos(a) * r * 66 / 100) >> 15) * w) >> 12);
            q[2 * m + 1] = sy[i] + ((((og_sin(a) * r * 8 / 10) >> 15) * w) >> 12);
        }
        vx_poly(&f, q, 18, 1, 44 + (v * 200 >> 8));
        vx_ring(&f, sx[i], sy[i], (r * 77 >> 8), (r * 97 >> 8), 28 + (v * 150 >> 8));
        if (v > 115) vx_dot(&f, sx[i], sy[i], (v - 51) * f.s / 4267, 184 + (v * 72 >> 8));
    }
    for (i = -1; i <= 1; i += 2) {                       /* the antennae: tipped by the two lowest bits */
        int32_t b = bc[i < 0 ? 0 : 1], tx = sx[0] + 48 + i * 96 + (b * 32 >> 8);
        int32_t ty = sy[0] - 320 - (b * 64 >> 8) + ((og_sin(cr * 3u + (uint32_t)(i * 10430)) * 24) >> 15);
        q[0] = sx[0] + i * 48; q[1] = sy[0] - 144; q[2] = sx[0] + i * 80; q[3] = sy[0] - 240; q[4] = tx; q[5] = ty;
        vx_curve(&f, q, 3, 0, 77 + b / 5);
        vx_dot(&f, tx, ty, ((256 + (b * 307 >> 8)) * f.s * 6 / 10) >> 12, 128 + b / 2);
    }
    {   /* the head: a bicho shouting with the level, squashed by the note */
        uint32_t tt = mt * (uint32_t)(60 + (en * 260 >> 8)) >> 8;
        int32_t hx = sx[0], hy = sy[0], ssx = 256 + (sq * 77 >> 8), ssy = 256 - (sq * 77 >> 8);
        og_node(f.cx + ((hx * f.s) >> 8), f.cy + ((hy * f.s) >> 8), (10 * f.s) >> 8, T_SURF);
        vxb_t g = {hx, 0, hy, 288, ssx, ssy, 4, 8 + (en * 30 >> 8), 40 + (en * 100 >> 8), (en * 80 >> 8), 4, 1};
        vx_body(&f, &g, 168, 112, tt, 51 + (en * 77 >> 8), 102 + (en * 77 >> 8), 72, 40);
        vx_bface(&f, hx + 16, hy, 168, (en * 333 >> 8), age < 177u, 0);
    }
#undef VH
#undef VR
    vxs.kc = 0;
}

/* (BYTE 0.17) HIVE (eng_hive.c): its four strings as lines across, each vibrating as loud as it sounds (the higher, the
 * more waves), each pair linked (CROSS's lean), the echo a dot that grows with its clock and glows with what it holds,
 * the pair's tuning under them. Small (HOME): the four lines alone */
static void vx_hive(const track_t *t, const int32_t *A, int32_t en_home)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, k, i;
    const hv_st_t *s = hv_of(tr);
    int32_t x = A[0], y = A[1], w = A[2], h = A[3], big = w >= 160, lw = big ? w - 44 : w, on = s->magic == HV_MAGIC;
    uint16_t tc = TRK_COL[tr % 6u];
    (void)en_home;
    for (k = 0; k < HV_N; k++) {
        int32_t yc = y + h * (int32_t)(2u * k + 1u + (k >> 1)) / 10, amp = on ? clamp(s->lv[k], 0, 16383) * h >> 18 : 0;
        int32_t px = x, py = yc;
        uint32_t ph = anim_fr * (700u + 260u * k);
        for (i = 1; i <= 24u; i++) {                     /* a standing wave: its ends still, its belly the level */
            int32_t xx = x + (int32_t)i * lw / 24, yy = yc + ((((amp * og_sin(i * 1365u)) >> 15) *
                                                               og_sin(i * (3u + 2u * k) * 1365u + ph)) >> 15);
            cv_line_t(px, py, xx, yy, amp ? tc : T_DIM, big ? 2 : 1);
            px = xx; py = yy;
        }
        if (k & 1u)                                      /* the pair's link, leaning with CROSS */
            cv_line_t(x + lw / 2, yc - h / 5, x + lw / 2 + clamp(t->p[P_E1], 0, 127) / 8, yc, T_MID, 1);
    }
    if (big) {
        int32_t r = 2 + (int32_t)(on ? s->ew * 9u / HV_ES : 0u), e = on ? clamp(s->lv[HV_N], 0, 16383) >> 7 : 0;
        og_node((x + w - 22) * 16, (y + h / 2) * 16, r, e > 8 ? heat_col(40 + e) : T_LINE);
        hud_tag(x + w - text_w(&AF_X, "HIVE") - 4, y + 1, "HIVE", tr % 6u >= 4u ? 0xFFFFu : 0x0000u, tc);
        cv_text(x + 2, y + h - 11 - AF_X_CAP_Y, &AF_X, N_HVPAIR[clamp(t->p[P_E4], 0, 7)], T_TEXT);
    }
}

typedef struct { const engine_t *e; void (*f)(const track_t *, const int32_t *, int32_t); } vxe_t;
static const vxe_t VX_F[] = {{&ENG_HIVE, vx_hive}};   /* (BYTE, flash: MATH WAVE FUNC drawn by ub_draw before it: vx_lofi
                                                         * never reached, out of the segment) */   /* (JIANT BYTE:
                                                         * WEST PHASE STRING retired; WAVE drawn as MATH's) */   /* (0.9.9-snd5; V10.7.13+: ADD's gone, room for
                                                         * FORM MY LOOP: it draws the being) */
static int vx_draw(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    const engine_t *e = trk_engp(t);
    int32_t A[4] = {x, y, w, h}, cx = x * 16 + w * 8, cy = y * 16 + h * 8, s;
    if (ENG_IS_MATH(e) || e == &ENG_WAVE)               /* (JIANT BYTE) the formula and its being (ui_byte.c) */
        return ub_draw(t, A, en_home);
    if (trk_eng(t) == ENGI_DRUM) {                      /* (JIANT BYTE) the kit as eight beings (ui_byte.c) */
        (void)cx; (void)cy; (void)s;
        ub_kit(t, A, -1);
    }
    /* (JIANT BYTE: FM6 retired: its colony vx_fm6 gone) */
    else {                                               /* (0.9.9-snd5: the creatures drawn the same way, a table) */
        uint32_t i;
        for (i = 0; i < NELEM(VX_F) && VX_F[i].e != e; i++) {}
        if (i == NELEM(VX_F))
            return 0;
        VX_F[i].f(t, A, en_home);
    }
    return 1;
}
/* EDIT > KIT (lane -1) and SOUND 1..4 (a DRUM track): the kit large on the panel, KIT over it on the left; on the right
 * the sound last struck (KIT) or the sound edited, lit (SOUND: lane) */
static void vx_kit_page(const track_t *t, int32_t lane)
{
    char b[16];
    uint32_t l;
    {
        static const int32_t KA[4] = {4, 16, 232, 136};
        ub_kit(t, KA, lane);                             /* (JIANT BYTE: the eight beings) */
    }
    l = lane < 0 ? vxs.last & 7u : (uint32_t)lane & 7u;
    str_cpy(b, drum_lane_abbr(t, l), sizeof b);
    str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), drum_lane_name(t, l), sizeof b - str_len(b));
    cv_text(8, 6 - AF_X_CAP_Y, &AF_X, "KIT", T_THEME);
    cv_text_r(232, 6 - AF_X_CAP_Y, &AF_X, b, lane < 0 ? T_TEXT : T_THEME, T_SURF);
}
