# JIANT BYTE · VIS, the visual generator

Every song BYTE plays has its own visual, and another song makes another one. It is not a set of fixed modes: BYTE takes
the drawings it already has (its pages' pictures) and mixes them. A **recipe** comes from GEN's seed, the genre, the
energy and the engines the song plays; the recipe picks 2 or 3 **ingredients** and how they meet: one gives the
**content**, another the **skeleton** it is drawn on (DRUM-W's radar folded onto LIFE's sphere, the DELAY's tunnel on
Lissajous frames, CLAY's traces rolled round KEY's sphere), sometimes a third layer goes **over**, one or two
**deformations** bend it, and everything **reacts**: a drum hit pulses, the bass heats and swells, each chord change
turns the colours, each section changes one piece of the recipe; the song has a set of 8 recipes and its kicks and
snares cut between them ("Cuts" below). Same seed, same visual.

The approved mock-up is [`visgen-mock.html`](visgen-mock.html) (route A: built from BYTE's own drawings).

**VIS lite, in the release.** The first device visual (every page picture of the recipe drawn through a point map, a
third layer, mirror copies, heat cells, grain; every part's level followed in the audio path) made the FM-1's CPU
spike, and was taken out (`FELUCCA_VIS` 0). It is back as **VIS lite** (`FELUCCA_VIS` 1, the default,
`firmware/src/core.h`; 0 still leaves it out: no VIS tab, no SAVER row, WATCH's VIS bit answered 0, no VIS_SONG /
VIS_TICK, a song's `V` `S` block loaded and saved as ever): the same recipes, ids, vectors and cuts, drawn simply and
cheaply, and costing nothing when they are not on the screen ("The device" and "Zero cost" below).

Where it lives on the device: **HOME > VIS** (KNOB 1 MIX, 2 DEFORM, 3 PALETTE, 4 REACT; OCT+ another recipe for the same
song, OCT- the song's own) and **MENU > DISPLAY > SAVER** (OFF / 1 / 3 / 5 / 10 MIN, 3 the default): the visual on the
whole screen after that long with no input; the first press, key or turn only brings the page back.

JIANT Studio draws the same visuals from (1) BYTE over USB (the VIS_SONG and VIS_TICK pushes below), (2) the web
emulator (`window.jiantEmu.vis()`), (3) any MIDI device (Studio fills the same state itself). The **recipe** is the part
that must be identical everywhere: it is a small pure function, `firmware/src/vis_recipe.c` `vis_recipe()`, specified
here exactly, with test vectors. How each ingredient is drawn is each renderer's own (the device: each ingredient's
clean shape, below; Studio may draw the pages' real pictures).

## Ids (fixed for good)

**Ingredients** (`VI_*`):

| id | name | the page it comes from | the device's shape (VIS lite, `ui_vis.c` `VIS_SHP`) | can be skeleton | its carrier (VIS lite) | in the pool |
|---:|---|---|---|:---:|---|---|
| 0 | LIFE | HOME > LIFE's sphere and satellites (`ui_lzhome.c` `graph_eco`) | a ring, two satellites going round | yes | sphere | always |
| 1 | KEY | SCL > SCALE's sphere with the scale (`ui_lzpage.c` `graph_key`) | the scale's 7-gon in a ring, a small ring | yes | sphere (slower) | always |
| 2 | HARMONY | SCL > HARMONY's chord cards (`graph_harm`) | the chord's triangle in a ring, a small ring | | | always |
| 3 | LFO | LFO's satellite and wave (`ui_graph.c` `graph_lfo`) | a wave across, a satellite | yes | wave (slow) | always |
| 4 | SPLASH | the power-on signature | two Lissajous stars (3:2, 2:3) | | | always |
| 5 | RADAR | DRUM-W's PLAY radar (`ui_dx.c` `dw_play_panel`) | two rings, the sweep turning | yes | polar (turning) | DRUM-W |
| 6 | ORBIT | WAVE's phase orbit (`ui_byte.c` `hud_wave`) | the recipe's Lissajous | yes | swung along the Lissajous | WAVE |
| 7 | CLAY | DRONE CLAY's traces (`ui_lzedit.c` `graph_clay_lz`) | three coarse traces | yes | wave (in steps) | CLAY |
| 8 | TUNNEL | FX > DELAY's tunnel (`ui_lzfx.c` `fxg_delay`) | three squares turning | yes | polar (into the depth) | FX |
| 9 | CHORUS | FX > CHORUS's sines and orbit (`fxg_chorus`) | two waves, a small ring | yes | wave (fast) | FX |
| 10 | HIVE | HIVE's strings (`ui_engvis.c` `vx_hive`) | three strings, up | yes | wave (quick) | HIVE |
| 11 | FUNC | FUNC's score lanes (`ui_byte.c` `hud_func`) | three lanes of steps | | | FUNC |
| 12 | MATH | MATH's being (`ui_byte.c` `ub_draw`, small) | a ring and its eye in a square | yes | flat | MATH |

255 = none.

**Deformations** (`VD_*`): 0 TWIST, 1 FOLD, 2 MIRROR, 3 POLAR, 4 MELT, 5 SCAN.

**Blends** (`VB_*`): 0 THERM (thermal pulse: content and skeleton as heat cells), 1 SUM (colour sum: every layer a wire
in colour), 2 MIX (thermal + wire: the content heat, the others wires). (The device's VIS lite draws every blend as wires, and no
layer over: "The device" below.)

**Palettes** (`VP_*`): 0 AUTO (K3 only: the recipe's), 1 THERM, 2 HUD, 3 TRACKS, 4 ICE, 5 AMBER. On the device the
names are ASCII: `AUTO THERM HUD TRACKS ICE AMBER`.

**Families** (a genre's): 0 MACHINE, 1 HOUSE, 2 LATIN, 3 IDM, 4 SOFT, 5 HEAVY. **Moods** (energy): 0 CALM (< 40),
1 GROOVE (< 70), 2 PEAK.

**Engine mask** (bit: engine): 0 WAVE, 1 MATH (a MATH part that is not FUNC), 2 FUNC, 3 BANK (a FUNC part on the BANK
form), 4 DRUM-W, 5 HIVE, 6 CLAY (DRONE CLAY on), 7 FX (a part sending to the DELAY or the CHORUS). BANK has no
drawing of its own: it adds MELT to the deformations.

**Genres** (`gen_tables.c` GENRE, 0..26): 0 MINIMAL TECHNO, 1 HARD TECHNO, 2 HARD BASS, 3 HOUSE, 4 DEEP HOUSE, 5 TECH
HOUSE, 6 TRIBAL, 7 CUMBIA, 8 REGGAETON, 9 DEMBOW, 10 ELECTRO, 11 BREAKBEAT, 12 DRUM & BASS, 13 IDM GLITCH, 14 IDM
AMBIENT, 15 IDM WEIRD, 16 KRAUT, 17 EBM, 18 NEO CLASSIC, 19 FUGA, 20 ELECTRO METAL, 21 TECHNO ROCK, 22 DREAM POP,
23 PHONK, 24 BAILE FUNK, 25 JAZZ, 26 HIP HOP; 27 (or more) none (EXPLORE, a song not made by GEN).

## The recipe: `vis_recipe(seed, genre, energy, mask, rec)`

Inputs: `seed` (21 bits used: `seed & 0x1FFFFF`; the device's are GEN's 0..9999), `genre` (0..26, 27 none), `energy`
(0..100), `mask` (the engine mask, 8 bits), `rec` (the recipe byte, 0..255: 0 the song's own, OCT+ on HOME > VIS adds 1).
All arithmetic is unsigned 32-bit (JS: `>>> 0`, `Math.imul`); `%` is the unsigned remainder.

1. **The random stream.** `h = 2166136261`; for each `v` of `[seed & 0x1FFFFF, genre, energy, mask & 255, rec & 255]`:
   `h = (h ^ v) * 16777619; h ^= h >> 13`. The state `x = h || 1`. `rnd()`:
   `x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x`.
   `pick(w[n])`: `s = sum(w)`; `s == 0` → 255 (no `rnd()` call); else `r = rnd() % s` and the first `i` with
   `r < w[i]` (subtracting each `w[i]` passed).
   Then `energy = min(energy, 100)`; `fam = genre < 27 ? FAM[genre] : 1`; `mood = energy < 40 ? 0 : energy < 70 ? 1 : 2`.
2. **The pool.** `pool[i] = i < 5 || mask bit ENG_ING[i] ? W[i] : 0` with
   `W = [12,10,8,7,5,30,30,30,30,30,30,30,30]`, `ENG_ING = [-,-,-,-,-, 4,0,6,7,7,5,2,1]`
   (RADAR DRUM-W, ORBIT WAVE, CLAY CLAY, TUNNEL and CHORUS FX, HIVE HIVE, FUNC FUNC, MATH MATH).
3. `content = pick(pool)`.
4. **Skeletons.** `sk[i] = i != content && SKEL_OK bit i ? pool[i] * (FSK[fam][i] || 4) : 0`, `SKEL_OK = 0x17EB`
   (LIFE KEY LFO RADAR ORBIT CLAY TUNNEL CHORUS HIVE MATH). **Over:** `ov[i] = i != content && i != 4 ? pool[i] : 0`.
   `skel[0] = pick(sk)`.
5. **The layer over.** `over[0] = 255`; if `rnd() % 220 < 66 + energy`: `over[0] = pick(ov with ov[skel[0]] = 0)`.
6. `blend = pick(FBL[fam])`.
7. **Deformations.** `w = FDF[fam]` (6 weights); if mask bit 3 (BANK) and `w[MELT] < 3`: `w[MELT] = 3`.
   `nd = energy > 70 ? 2 : 1`; for each: `k = pick(w)` (255: stop), `w[k] = 0`, `def[0][i] = k`,
   `amt[0][i] = 45 + rnd() % 50`. Unused slots: 255 with amount 0.
8. `pal = FPAL[fam][rnd() % n]` (n: 3 for SOFT, else 2); `mix = 62 + rnd() % 34`;
   `react = min(100, 45 + floor(energy * 10 / 17))` (no `rnd()`); `lis = rnd() % 7` (the Lissajous a:b of the ORBIT
   skeleton: `[[1,2],[2,3],[3,4],[3,2],[1,3],[3,5],[2,5]]`).
9. **Sections B, C, D** (s = 1..3): copy section s-1's `skel over def amt`; `r = rnd() % 100`:
   - `r < 45`, another skeleton: `o = over[s] < 13 ? over[s] : skel[s]`; `pick(sk with sk[skel[s]] = 0 and sk[o] = 0)`;
     255 keeps it (sk is left as it was for the next section).
   - `r < 75`, a deformation more: the free ones `f` (ids 0..5 not in `def[s]`, in order), `k = f[rnd() % len(f)]`;
     if `def[s][1] != 255`: the first goes (`def[s][0] = def[s][1]`, its amount too); the new one in slot
     `def[s][0] == 255 ? 0 : 1`, `amt = 50 + rnd() % 45`.
   - else another layer over: `over[s] = pick(ov')` with `ov'[i] = i != content && i != 4 && i != over[s-1] &&
     i != skel[s] ? pool[i] : 0` (255: none). `ov` is now `ov'` for the sections after.

The tables (rows: the six families):

```
FAM[27]  = 0 0 0 1 1 1 1 2 2 2 0 3 3 3 4 3 1 0 4 4 5 5 4 5 2 4 5
FSK      LIFE KEY HARM LFO SPL RADAR ORBIT CLAY TUNNEL CHORUS HIVE FUNC MATH   (0 means 4)
MACHINE   10   0   0    0   0   30    0    10    30      0     0    0    0
HOUSE     30  20   0    0   0    0   20     0     0     10     0    0    0
LATIN     10   0   0   10   0   30    0     0     0      0    30    0    0
IDM        0   0   0    0   0   20   10    20     0      0     0    0   30
SOFT       0  30   0    0   0    0   20     0    10     20    10    0    0
HEAVY      0   0   0    0   0   20    0    10    30      0     0    0    0
FBL      THERM SUM MIX        FDF  TWIST FOLD MIRROR POLAR MELT SCAN     FPAL
MACHINE    3    0   1               2    0     0      1    0    3        THERM AMBER
HOUSE      1    1   3               2    0     2      2    0    0        THERM HUD
LATIN      0    3   1               1    2     3      0    0    0        TRACKS THERM
IDM        2    2   0               1    3     0      0    0    3        ICE THERM
SOFT       0    1   3               0    0     2      2    3    0        ICE AMBER HUD
HEAVY      3    0   1               2    0     0      0    2    2        AMBER THERM
```

### Test vectors

`tests/visgen_test.c` checks these against the C code (sections: skeleton, over, deformations with their amounts).

| seed | genre | energy | mask | rec | content | fam | mood | blend | pal | mix | react | lis | A | B | C | D |
|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|
| 421 | 4 | 70 | 0x11 | 0 | ORBIT | 1 | 2 | 2 | 2 | 67 | 86 | 1 | LIFE - MIRROR 78 | KEY - MIRROR 78 | LIFE - MIRROR 78 | RADAR - MIRROR 78 |
| 421 | 4 | 70 | 0x11 | 1 | RADAR | 1 | 2 | 2 | 1 | 70 | 86 | 5 | KEY - TWIST 46 | LIFE - TWIST 46 | ORBIT - TWIST 46 | ORBIT - TWIST 46 SCAN 69 |
| 422 | 4 | 70 | 0x11 | 0 | SPLASH | 1 | 2 | 2 | 1 | 75 | 86 | 1 | KEY - POLAR 86 | KEY RADAR POLAR 86 | KEY ORBIT POLAR 86 | KEY HARMONY POLAR 86 |
| 0 | 27 | 50 | 0x00 | 0 | HARMONY | 1 | 1 | 2 | 2 | 62 | 74 | 3 | KEY - TWIST 72 | LFO - TWIST 72 | LIFE - TWIST 72 | KEY - TWIST 72 |
| 9999 | 0 | 95 | 0x90 | 0 | LFO | 0 | 2 | 0 | 1 | 94 | 100 | 0 | TUNNEL CHORUS SCAN 47 TWIST 61 | RADAR CHORUS SCAN 47 TWIST 61 | TUNNEL CHORUS SCAN 47 TWIST 61 | RADAR CHORUS SCAN 47 TWIST 61 |
| 1234 | 13 | 85 | 0x15 | 0 | KEY | 3 | 2 | 0 | 4 | 76 | 95 | 0 | RADAR HARMONY FOLD 85 TWIST 47 | RADAR HARMONY TWIST 47 POLAR 53 | RADAR ORBIT TWIST 47 POLAR 53 | LIFE ORBIT TWIST 47 POLAR 53 |
| 77 | 22 | 30 | 0xA0 | 0 | LIFE | 4 | 0 | 1 | 2 | 72 | 62 | 4 | CHORUS HIVE MELT 55 | KEY HIVE MELT 55 | CHORUS HIVE MELT 55 | CHORUS TUNNEL MELT 55 |
| 5000 | 7 | 65 | 0x30 | 3 | RADAR | 2 | 1 | 1 | 3 | 66 | 83 | 3 | HIVE LFO MIRROR 71 | LIFE LFO MIRROR 71 | LIFE HIVE MIRROR 71 | LFO HIVE MIRROR 71 |
| 1 | 26 | 40 | 0x48 | 0 | CLAY | 5 | 1 | 0 | 1 | 91 | 68 | 6 | LFO - SCAN 49 | LFO - SCAN 49 POLAR 91 | LIFE - SCAN 49 POLAR 91 | LIFE HARMONY SCAN 49 POLAR 91 |
| 2097151 | 18 | 10 | 0xFF | 255 | MATH | 4 | 0 | 2 | 2 | 78 | 50 | 5 | HIVE FUNC MELT 81 | ORBIT FUNC MELT 81 | KEY FUNC MELT 81 | CHORUS FUNC MELT 81 |
| 600 | 11 | 72 | 0x5B | 2 | KEY | 3 | 2 | 0 | 4 | 93 | 87 | 5 | MATH - FOLD 88 MELT 90 | MATH - MELT 90 SCAN 91 | MATH RADAR MELT 90 SCAN 91 | MATH ORBIT MELT 90 SCAN 91 |
| 31337 | 24 | 55 | 0x31 | 0 | RADAR | 2 | 1 | 1 | 1 | 71 | 77 | 5 | HIVE ORBIT MIRROR 71 | LFO ORBIT MIRROR 71 | HIVE ORBIT MIRROR 71 | HIVE KEY MIRROR 71 |

## Cuts: the song's set of 8

The visual is not one recipe but the song's **set**: the recipes of the recipe bytes `rec + j` (j = 0..7, `& 255`;
`vis_recipe` itself unchanged, its vectors above). Which one is drawn moves with the music, like an editor cutting
between 8 generations, deterministically:

- `pos` (0..255) and the section `sec` (0..3, as VIS_SONG's byte 9; the saver adds one every 30 s): the recipe drawn is
  `j = (pos * (2 * sec + 1) + sec) & 7`, and of it section `sec`'s skeleton, over, deformations; its content, blend,
  palette (PALETTE AUTO: so a cut may turn the colours).
- A new section: `pos = 0` (a reshuffle: another odd step through the 8). A **kick** (VIS_TICK hits bit 0; the device
  also takes the loudest part's level jumping by 40 of 256 between two frames): `pos += 1`. A **snare** (hits bit 1),
  or a **timbre change** (hits bit 4: a part's engine, preset, or its filter CUT crossing a quarter of its range): `pos += 3`. Both
  in one frame: `+4`. A cut at most every 150 ms (a section's always).
- MIX and REACT start at the song's own recipe's (j = 0) whenever the song or its recipe byte changes; DEFORM 50.

## The device

- **HOME > VIS**, a tab after TRACKS: the visual on the whole screen (240 x 240: no header, tabs, cards or footer);
  the knobs MIX, DEFORM, PALETTE, REACT: a knob turned shows its name and value over the visual for 1.3 s (OCT+ /
  OCT-: RECIPE n); any page button leaves as ever. MIX and REACT start at the song's recipe's whenever the recipe
  changes; DEFORM 50; PALETTE AUTO. They are not saved. OCT+ another recipe (the recipe byte + 1, saved with the song: FUNC's block `V` `S`, an older song 0), OCT-
  back to 0.
- **MENU > DISPLAY > SAVER**: OFF 1 3 5 10 MIN (3 the default; editor id 33), engine 15's byte 12 bits 4-6 stored ^ 2
  (as JIANT FM V10.8.56). The whole screen, the page's recipe, in 4 bands of 60 rows.
- The song's identity for the recipe (`V` `S`: seed, genre, energy, rec) is set by GEN when it makes a piece; a song
  GEN did not make: seed from its tempo and its first part's key and length, genre 27, energy 50. The engine mask is read from the parts as they
  are.
- **Drawing (VIS lite).** One recipe draws ONE thing: its content's clean shape (the table above: 1 to 3 strokes, each
  one curve of 2 to 64 corners, a polygon's edges cut in pieces) on ONE carrier, its skeleton's (a sphere turning, polar
  round the turn, swung along the Lissajous, or the flat bent by a wave; MATH's and none: flat), MIX of it. No layer
  over (the recipe's `over` is not drawn), no second copy for MIRROR, no heat cells, no grain. At most ONE deformation,
  the section's first (`def[s][0]`), and only while it sounds (its amount x DEFORM x the sound, gliding): TWIST (the
  middle turned more, rippled), FOLD (back at +-k), MIRROR (the left half bent onto the right's, drawn once), POLAR
  (rings turning), MELT (each column of 3 px dropping its own), SCAN (on a snare, bands of 16 rows thrown aside). A
  sphere's far side is not drawn. **At most `VIS_SEG` = 200 segments a frame** (counted and capped as the points are
  worked out; the shapes ask 30 to 100), each a 2 px line in one colour of the palette's upper half (the chord, the
  stroke and the recipe drawn turning it there), every colour at least ~40 % luminance (a darker stop is lifted toward
  white; `tests/visgen_test.c` checks every pixel at 35 %). The camera: a zoom breathing (0.85x..1.15x) and pulsing (a
  kick, the level, each cut x REACT; at most ~+20 %), a turn drifting (its way the recipe's j; a kick throws it on), the
  centre drifting a little. The cuts (above) and the saver's 30 s scenes as ever.
- **At rest and with the sound.** The device follows one amount, the sound (0..1: the loudest part's level doubled, or a
  kick), held while it sounds and let go in ~0.7 s. Silent: the content's shape on its carrier as it is, no deformation,
  no pulse; only the carrier's turn (a sphere, the radar), the strokes' own slow turn or travel and the camera's breath,
  turn and drift move it: a still form turning. With the sound: the deformation, the zoom pulses, the shapes swelling
  (up to +25 %) and the carrier's wave growing come in with it, REACT setting the most; when it stops they ease back.
- **Its pace.** Drawn every 2nd UI frame (~30 a second; the frames between cost a few instructions), the points worked
  out once a frame (in 1.6 KB of the display cache's hashes, free while the visual fills the screen: the page after it
  is drawn whole) and each of the 4 bands drawing their lines. The audio's load (the guard's own measure, `voice.c`
  `load_avg_q8`, `load_rl`) thins it: past 50 % every 4th UI frame and fewer segments (200 down to 0 at 70 %), past 70 %
  or with the guard on not drawn at all (the last frame stays); the audio always wins. `tests/vis_cost_test.py` checks a
  VIS frame against every other page (the heaviest GEN song playing: instructions and the device code it runs; a VIS
  frame ~40 % of the heaviest page's instructions, ~7 % of its code).
- The recipe's code is in the Bluetooth segment (as JIANT FM's `vis_fig`): without the segment the visual draws the
  recipe of all zeros (LIFE on LIFE).

## Zero cost (the device's rules, for Studio too)

- Nothing of VIS runs unless the visual is on the screen (HOME > VIS with nothing over it, or MENU > SAVER's) or JIANT
  Studio watches it (WATCH bit 7, or the emulator's `web_vis_info` asked). Off the screen and unwatched: no frame, no
  level read, no polling (`tests/visgen_test.c` checks it: the song playing on another page, the visual's clocks still).
- Nothing of it is in the audio path. The parts' levels are their own peaks (`fx.c` `t->peak`: what the audio has
  always kept for the pages' meters), read and cleared by VIS when it looks (`ui_vis.c` `vis_levels`, at most every
  8 ms), a quick fall (~0.25 s) its own; the hits are DRUM-W's lanes as the UI sees them. The audio code is the same
  with VIS on and off: every audio function's size (`tools/audio_map.txt`) and the RAM code's place; the VIS code is
  placed past the sound (`tools/build.py` `vis_fns`); `tests/cpu_lines_test.py` scene 1101 (101 with HOME > VIS drawn
  between the blocks) must read the same lines as 101 and sound bit-exact.
- Studio's pushes: VIS_SONG / VIS_TICK only while WATCH bit 7 is set, the device looking 25 times a second (not each
  5 ms pass of the editor). While Studio watches and another page shows the parts' meters, both read the same peaks:
  a meter may miss a peak VIS read first (only then).

## USB (editor protocol 1.5)

WATCH (22) takes an optional second byte: its bit 0 is WATCH **bit 7, VIS** (SysEx bytes carry 7 bits). While set:

**VIS_SONG (98)**, pushed when VIS is first watched and whenever any field changes (at most once a UI frame):

| byte | field |
|---:|---|
| 0, 1, 2 | seed: bits 0-6, 7-13, 14-20 |
| 3, 4 | recipe byte: bits 0-6, bit 7 |
| 5 | genre (0..26, 27 none) |
| 6 | energy (0..100) |
| 7, 8 | engine mask: bits 0-6, bit 7 |
| 9 | section (0..3: A..D) |
| 10 | palette (K3: 0 AUTO, 1..5) |
| 11 | MIX (0..100) |
| 12 | DEFORM (0..100) |
| 13 | REACT (0..100) |

**VIS_TICK (99)**, 25 times a second while playing:

| byte | field |
|---:|---|
| 0..5 | level T1..T6 (0..127) |
| 6 | hits since the last tick: bit 0 KICK, 1 SNARE, 2 HAT, 3 PERC (DRUM-W's lanes: BD; SD CP; CH OH; TM RS CB), 4 TIMBRE (a timbre change, below) |
| 7 | chord root (0..11, C = 0; SCL > HARMONY's chord playing, else the key) |
| 8 | quality: 0 MAJ, 1 MIN, 2 DIM, 3 AUG, 4 SUS |
| 9 | step (0..63) |

JIANT FM uses 95..97 (PROJ_*); 98 and 99 are free in both firmwares.

## The web emulator

`web/emu/felucca_web.c` `web_vis_info()` returns a pointer to 21 `int32_t`: VIS_SONG's fields then VIS_TICK's, then
`playing`: `seed, recipe, genre, energy, engines, section, palette, mix, deform, react, level[6], hits, root, quality,
step, playing` (hits accumulate between two reads, bit 4 a timbre change since the last read; the levels are the parts'
peaks since the last read, however far apart the reads: nothing followed in the audio path). The page asks it with the worklet message `{type: "vis"}`;
`window.jiantEmu.vis()` resolves to `{seed, recipe, genre, energy, engines, section, palette, mix, deform, react,
levels, hits, root, quality, step, playing}`.
