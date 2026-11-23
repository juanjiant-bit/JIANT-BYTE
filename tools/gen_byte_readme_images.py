#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT BYTE's README images, from the firmware's own screens: tools/byte_shots.c (the emulator's build, natively)
# plays a GEN song, opens the pages and saves them; this script puts them together.
#   python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"   (build/gen, once)
#   python3 tools/gen_byte_readme_images.py [OUT]     (needs cc and PIL; OUT: docs)
# Makes docs/byte-hero.png (eight screens), docs/byte-boot.png (the ORBIT power-on), docs/byte-drone-clay.png
# (DRONE CLAY's pages and a key's card) and docs/byte-graphs.png (CHORD, SCALE, ENV, PUNCH SEQ).
import re, subprocess, sys
from pathlib import Path
from PIL import Image
R = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else R / 'docs'
B = R / 'build/byte_shots'
B.mkdir(parents=True, exist_ok=True)
VERSION = re.search(r'#define FELUCCA_VERSION "([^"]+)"', (R / 'firmware/src/felucca.c').read_text()).group(1)
EXE = B / 'byte_shots'
subprocess.run(['cc', '-O2', '-ffp-contract=off', '-w', f'-DFELUCCA_VERSION="{VERSION}"', '-I' + str(R / 'build/gen'),
                '-I' + str(R / 'firmware/src'), '-I' + str(R / 'web/emu'), '-o', str(EXE), str(R / 'tools/byte_shots.c'), '-lm'],
               check=True)

# the power-on, then a GEN song (DEEP HOUSE, seed 421, LOOP) playing, page by page. T1 MATH, T2 DRUM-W, T3 WAVE (its
# CHORD, then DRONE CLAY), T4 WAVE
SCRIPT = f'''
ms 600
shot {B}/boot1.ppm
ms 400
shot {B}/boot2.ppm
ms 350
shot {B}/boot3.ppm
ms 210
shot {B}/boot4.ppm
ms 1000
gen 5 421 70 0
ms 200
play
ms 2500
nav HOME MACROS
ms 1500
shot {B}/home.ppm
nav HOME LIFE
ms 1500
shot {B}/life.ppm
sel 0
nav EDIT EDIT 1
ms 1500
shot {B}/math.ppm
sel 1
nav EDIT PLAY
ms 1500
shot {B}/drumw.ppm
sel 2
nav EDIT EDIT 1
ms 1500
shot {B}/wave.ppm
nav FX DELAY
ms 1500
shot {B}/delay.ppm
nav GLO MIXER
ms 1500
shot {B}/mixer.ppm
voice 2 0
chord 2 3
nav SCL CHORD
ms 500
keys 512
ms 700
shot {B}/chord.ppm
keys 0
ms 300
nav SCL SCALE
keys 512
ms 600
shot {B}/scale.ppm
keys 0
nav ENV ENV
keys 512
ms 300
keys 0
ms 200
keys 128
ms 300
keys 0
ms 200
keys 2048
ms 400
shot {B}/env.ppm
keys 0
chord 2 0
clay 2
nav SEQ DRONE CLAY
ms 3000
shot {B}/clay.ppm
nav SEQ CLAY MOD 1
ms 2000
shot {B}/claymod.ppm
nav SEQ DRONE CLAY
keys 1
ms 150
shot {B}/claycard.ppm
keys 0
nav GLO PUNCH SEQ
ms 300
'''
def tap(k):
    return f'keys {1 << k}\nms 40\nkeys 0\nms 40\n'
# PUNCH SEQ: the white keys the steps of the row the black keys 1..4 pick, KNOB 2 its effect
SCRIPT += tap(1) + 'enc 4 -2\nms 100\n' + ''.join(tap(k) for k in (0, 7, 14, 19))
SCRIPT += tap(3) + 'enc 4 3\nms 100\n' + ''.join(tap(k) for k in (4, 6, 16, 18, 24))
SCRIPT += tap(5) + 'enc 4 6\nms 100\n' + ''.join(tap(k) for k in (11, 12, 21, 23, 26))
SCRIPT += f'ms 800\nshot {B}/punch.ppm\n'
subprocess.run([str(EXE)], input=SCRIPT, text=True, check=True, stderr=subprocess.DEVNULL)


def shot(n):
    return Image.open(B / f'{n}.ppm').convert('RGB')


def grid(names, cols, scale, m, path):
    rows = (len(names) + cols - 1) // cols
    s = 240 * scale
    im = Image.new('RGB', (m + cols * (s + m), m + rows * (s + m)), (0, 0, 0))
    for i, n in enumerate(names):
        im.paste(shot(n).resize((s, s), Image.NEAREST), (m + (i % cols) * (s + m), m + (i // cols) * (s + m)))
    im.save(path, optimize=True)
    print(path, im.size)


grid(['boot4', 'home', 'math', 'wave', 'drumw', 'clay', 'chord', 'delay'], 4, 2, 16, OUT / 'byte-hero.png')
grid(['boot1', 'boot2', 'boot3', 'boot4'], 4, 1, 8, OUT / 'byte-boot.png')
grid(['clay', 'claymod', 'claycard'], 3, 2, 12, OUT / 'byte-drone-clay.png')
grid(['chord', 'scale', 'env', 'punch'], 4, 1, 8, OUT / 'byte-graphs.png')
