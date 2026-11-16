#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""The UI palettes (run by tools/build.py generate()).

  gen_ui_palettes.py OUT.h [--report]

The family is seven palettes: JIANT (the default) and six of its own (SELVA CONTRAST BRASA ABISAL HUESO NEON), all
dark, all with the thermal ramp. A palette is twelve stored colours: BG (background), SURF (surface: dialogs, menu
rows, mixer strips), TEXT, THEME (identity colour: values, curves, gauges, selection), ACCENT (the one active thing:
hot knob, cursor, playhead, a sounding step), MID (labels, units, secondary text), DIM (inactive, empty steps,
disabled; the dividers), LINE (the faint gauge track), SEL (selection fill and gauge fill), TINT (a faint area),
REC and RAISE (a raised area on a surface: empty step stubs, guides, fader slots, button wells, chips).
JIANT's are five colours and the blends it has had since 0.8.9 (derive() here, the firmware's integer maths):
  MID = mix(BG, TEXT, 70 %)  DIM = 42 %  LINE = 18 %  SEL = mix(BG, THEME, 80 %)  TINT = 12 %  RAISE = mix(SURF, TEXT, 4 %)
the other six are stored as the owner set them. The firmware (src/gfx.c palette_set) derives only
  KEY  = mix(BG, TEXT, 78 %)    a keycap's fill (a mid-light cushion; its label INK, unavailable: a DIM fill)
  LANE, GRID                    the piano roll's rows and step lines
  INK  = black: text on a selection, a THEME / ACCENT / REC fill, a keycap
Outside the palette: the QR code's fixed black and white; the crash screen's fixed red.
Saved settings name a palette by a tagged id (PAL_TAG + n). n is not the index: it is the index the nearest palette
had in the firmware up to V10.7.18 (STORED_AS), so settings saved by that firmware land on the nearest of the family
(V10_TO_NEW) and that firmware reading newer settings shows its nearest one. Ids below 20 (the earliest firmware's 20
palettes) map by UI_PALETTE_MIGRATE.
--report prints the WCAG contrast of every pairing. The tool fails (exit 1) on any miss.
"""
import argparse
import sys
from pathlib import Path

# JIANT: five colours (8-bit RGB), the rest derived with the blends above (the palette as it has been since 0.8.9).
# A thermal camera (docs/TONIC-UI.md): black, white details, violet for what is selected, orange for the one active
# thing; every value, curve and hit as heat (ui_graph.c heat_col), in every palette of the family
JIANT = ("JIANT", (0, 0, 0), (10, 9, 14), (244, 241, 234), (184, 112, 255), (255, 122, 0))
RAISE_JIANT = 4                    # RAISE = mix(SURF, TEXT, 4 %): the mock-up's near-black card (#131218) on its panel
# The rest of the family (the owner's, 2026-11): every token as he set it (role -> hex), stored whole. INK is black in
# all seven. NEON's SEL is his #CC3284 a step lighter (#D43589: black INK on it 4.7, the rule 4.5; his 4.4)
FAMILY = {
    # (JIANT BYTE) the demoscene's machines, dark enough for the rules below: each in the place of a JIANT FM palette
    # (its stored id kept, so a setting saved by JIANT FM lands on one): C64 for SELVA, AMIGA BRASA, CGA ABISAL, GAMEBOY
    # HUESO, ATARI NEON
    "C64":      dict(bg="241A5C", surf="2C2170", raise_="352A80", line="4A3D9E", dim="8478DC", mid="B8B0F2", text="FFFFFF",
                     theme="9AE29B", sel="7CC47D", tint="2E2468", accent="EDF171", rec="FF7B7B"),
    "CONTRAST": dict(bg="000000", surf="000000", raise_="1C1C1C", line="8C8C8C", dim="A6A6A6", mid="E2E2E2", text="FFFFFF",
                     theme="D9B3FF", sel="D9B3FF", tint="26163D", accent="FFA640", rec="FF7A7A"),
    "AMIGA":    dict(bg="001229", surf="001A3A", raise_="00244D", line="0A3A6E", dim="5A84B0", mid="A6C2E2", text="FFFFFF",
                     theme="FF8800", sel="D06F00", tint="0A1E3A", accent="3FB6FF", rec="FF5A5A"),
    "CGA":      dict(bg="000000", surf="0A0A0A", raise_="141414", line="2A2A2A", dim="5A9A9A", mid="A4E0E0", text="FFFFFF",
                     theme="55FFFF", sel="33CCCC", tint="002424", accent="FF55FF", rec="FF5555"),
    "GAMEBOY":  dict(bg="081808", surf="0F240F", raise_="173317", line="294A29", dim="5F8A3A", mid="A6C64A", text="E8F8D0",
                     theme="9BBC0F", sel="7F9C0C", tint="102E10", accent="F8F8A0", rec="FF6060"),
    "ATARI":    dict(bg="000000", surf="0C0703", raise_="180F06", line="2E2010", dim="8E6E40", mid="D4B486", text="FCFCE0",
                     theme="F06818", sel="C4520F", tint="2A1004", accent="68A8FC", rec="FF4848"),
    # (JIANT BYTE) BYTE, the default: a phosphor screen's green on black, the demoscene's magenta for the one active thing
    "BYTE":     dict(bg="000000", surf="050A05", raise_="0C160C", line="1A2E1A", dim="4F8F5F", mid="8FD89F", text="D2FFD8",
                     theme="39FF6A", sel="2BBF50", tint="06200C", accent="FF2BD6", rec="FF4848"),
}
NAMES = ["JIANT"] + list(FAMILY)   # MENU > DISPLAY > COLOR's order; the index is not what is saved (STORED)
# the stored tokens, in the order of src/gfx.c ux's first fields (palette_set copies them)
TOKENS = ["bg", "surf", "text", "theme", "accent", "mid", "dim", "line", "sel", "tint", "rec", "raise_"]
# (0.8.9) each knob its colour (KNOB 1..4), on its card's value and gauge. JIANT: the 0.6 colours (its cards take the
# value's heat instead where there is one, ui_graph.c heat_on). The other six (the owner, 2026-11): four of the
# palette's own, apart from each other and from its ACCENT (the one active thing), 4.5 on BG, SURF and RAISE: its THEME
# first, then tints beside it; HUESO greys and its one orange
KNOBS = {
    "JIANT":    [(138, 108, 255), (255, 210, 60), (88, 116, 255), (255, 75, 75)],
    "C64":      ["9AE29B", "EDF171", "75CEC8", "FFB0B0"],      # (JIANT BYTE) light green, yellow, cyan, light red
    "CONTRAST": ["D9B3FF", "FFE08A", "9FD8FF", "FF9E9E"],      # lilac, pale yellow, pale blue, pale red
    "AMIGA":    ["FF8800", "3FB6FF", "FFFFFF", "FFD24A"],      # orange, blue, white, gold
    "CGA":      ["55FFFF", "FF55FF", "FFFFFF", "FFFF55"],      # cyan, magenta, white, yellow
    "GAMEBOY":  ["9BBC0F", "E8F8D0", "C4DC5A", "F8F8A0"],      # the four greens, lightest last
    "ATARI":    ["F06818", "FCFC54", "68A8FC", "5CD86C"],      # orange, yellow, blue, green
    "BYTE":     ["39FF6A", "FF2BD6", "2BE0FF", "FFE14A"],      # (JIANT BYTE) phosphor, magenta, cyan, yellow
}
# the 20 palettes of the earliest firmware (index order), then the 11 of the firmware up to V10.7.18 (stored
# PAL_TAG + index: GREY .. JIANT) -> the family
OLD = ["GREEN", "AMBER", "CYAN", "RED", "MONO", "VIOLET", "PINK", "ICE", "WARM", "OCEAN", "DUSK", "HI-CON",
       "LIGHT", "PAPER", "SKY", "MINT", "LILAC", "ROSE", "SAND", "L-HICON"]
OLD_TO_NEW = {"GREEN": "C64", "AMBER": "AMIGA", "CYAN": "CGA", "RED": "AMIGA", "MONO": "GAMEBOY", "VIOLET": "JIANT",
              "PINK": "AMIGA", "ICE": "CGA", "WARM": "AMIGA", "OCEAN": "CGA", "DUSK": "JIANT", "HI-CON": "CONTRAST",
              "LIGHT": "CONTRAST", "PAPER": "CONTRAST", "SKY": "CONTRAST", "MINT": "CONTRAST", "LILAC": "CONTRAST",
              "ROSE": "CONTRAST", "SAND": "CONTRAST", "L-HICON": "CONTRAST"}
V10 = ["GREY", "GREEN", "AMBER", "ICE", "VIOLET", "ROSE", "PAPER", "HI-CON", "NIGHT", "MONO", "JIANT"]
V10_TO_NEW = {"GREY": "GAMEBOY", "GREEN": "C64", "AMBER": "AMIGA", "ICE": "CGA", "VIOLET": "JIANT", "ROSE": "AMIGA",
              "PAPER": "CONTRAST", "HI-CON": "CONTRAST", "NIGHT": "GAMEBOY", "MONO": "GAMEBOY", "JIANT": "JIANT"}
# what a palette is saved as: the id of the V10 palette nearest to it, so the firmware up to V10.7.18 reading these
# settings shows that one (JIANT: JIANT); NEON a new id (that firmware: GREY, its fallback for an id it does not know)
STORED_AS = {"JIANT": "JIANT", "C64": "GREEN", "CONTRAST": "HI-CON", "AMIGA": "AMBER", "CGA": "ICE", "GAMEBOY": "GREY",
             "ATARI": None, "BYTE": None}
PAL_TAG = 64                       # stored id = PAL_TAG + n; below 20: an id of the earliest firmware
QR_LIGHT, QR_DARK, CRASH_BG, CRASH_INK = (255, 255, 255), (0, 0, 0), (160, 0, 0), (255, 255, 255)
PCT = {"MID": 70, "DIM": 42, "LINE": 18, "SEL": 80, "TINT": 12, "KEY": 78}


def near565(h):
    """a token of the family: the nearest RGB565 (a gray on the gray axis, G = 2 R)"""
    r, g, b = (int(h[i:i + 2], 16) for i in (0, 2, 4))
    r5, g6, b5 = (r * 31 + 127) // 255, (g * 63 + 127) // 255, (b * 31 + 127) // 255
    if r == g == b:
        g6 = r5 << 1
    return (r5 << 11) | (g6 << 5) | b5


def to565(c):
    r, g, b = c
    if r == g == b:                     # a gray stays on the RGB565 gray axis (G = 2 R)
        return ((r >> 3) << 11) | ((r >> 3) << 6) | (r >> 3)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def cdiv(a, b):
    return a // b if a >= 0 else -((-a) // b)


def mix(a, b, pct):
    """src/gfx.c ux_mix: a + (b - a) * pct / 100 per channel, rounded"""
    out = 0
    for sh, mask in ((11, 31), (5, 63), (0, 31)):
        x, y = (a >> sh) & mask, (b >> sh) & mask
        d = (y - x) * pct
        out |= (x + cdiv(d + (50 if d >= 0 else -50), 100)) << sh
    return out


def lum(c565):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = from565(c565)
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def derive(name):
    if name == "JIANT":
        bg, surf, text, theme, accent = (to565(c) for c in JIANT[1:])
        d = dict(bg=bg, surf=surf, text=text, theme=theme, accent=accent,
                 mid=mix(bg, text, PCT["MID"]), dim=mix(bg, text, PCT["DIM"]), line=mix(bg, text, PCT["LINE"]),
                 sel=mix(bg, theme, PCT["SEL"]), tint=mix(bg, theme, PCT["TINT"]), raise_=mix(surf, text, RAISE_JIANT),
                 rec=to565((255, 72, 72)))
    else:
        d = {k: near565(v) for k, v in FAMILY[name].items()}
    assert list(d) and set(d) == set(TOKENS)
    d["key"] = mix(d["bg"], d["text"], PCT["KEY"])
    d["ink"] = 0
    for i, k in enumerate(KNOBS[name]):
        d[f"k{i}"] = to565(k) if name == "JIANT" else near565(k)
    return name, d


# (foreground, background, minimum contrast, why)
CHECKS = [("text", "bg", 12.0, "body text"), ("text", "surf", 9.0, "body text on a surface"),
          ("mid", "bg", 5.0, "labels"), ("mid", "surf", 4.0, "labels on a surface"),
          ("dim", "bg", 2.2, "inactive (not read)"), ("line", "bg", 1.25, "1 px dividers"),
          ("theme", "bg", 4.5, "values, curves"), ("theme", "surf", 4.5, "values on a surface"),
          ("accent", "bg", 4.5, "hot value, cursor, playhead"), ("accent", "surf", 4.5, "hot on a surface"),
          ("ink", "sel", 4.5, "text on a selection"), ("ink", "theme", 4.5, "text on a THEME fill"),
          ("sel", "bg", 1.8, "gauge fill vs track"), ("rec", "bg", 3.0, "REC (graphic)"),
          ("text", "raise_", 7.0, "text on a RAISE chip or button"), ("ink", "rec", 4.5, "text on the REC chip"),
          ("ink", "key", 4.5, "a keycap's label"), ("key", "bg", 3.0, "a keycap on the background"),
          ("key", "surf", 2.5, "a keycap on a surface"), ("ink", "dim", 1.8, "an unavailable keycap's label"),
          ("ink", "accent", 4.5, "text on the ARM badge")] + \
         [(f"k{i}", b, 4.5, f"KNOB {i + 1}'s value") for i in range(4) for b in ("bg", "surf", "raise_")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    fails = []
    assert NAMES[0] == "JIANT" and len(OLD) == 20 and set(STORED_AS) == set(NAMES) == set(KNOBS)
    rows = [derive(n) for n in NAMES]
    for name, d in rows:
        for f, b, lim, why in CHECKS:
            c = contrast(d[f], d[b])
            if c < lim:
                fails.append(f"{name}: {f}/{b} {c:.2f} < {lim} ({why})")
    # what each is saved as (n of PAL_TAG + n), and what each n a firmware up to V10.7.18 saved reads as
    stored = [V10.index(STORED_AS[n]) if STORED_AS[n] else None for n in NAMES]
    for i, v in enumerate(stored):
        if v is None:
            stored[i] = max([len(V10) - 1] + [x for x in stored if x is not None]) + 1
    back = [NAMES.index(V10_TO_NEW[n]) for n in V10] + [0] * (max(stored) + 1 - len(V10))
    for i, v in enumerate(stored):
        assert v >= len(V10) or back[v] == i, f"{NAMES[i]} is saved as an id that reads as {NAMES[back[v]]}"
        back[v] = i
    assert len(set(stored)) == len(stored)
    if a.report:
        print(f"{'palette':8s} " + " ".join(f"{f}/{b}".ljust(12) for f, b, _, _ in CHECKS))
        for name, d in rows:
            print(f"{name:8s} " + " ".join(f"{contrast(d[f], d[b]):5.1f}".ljust(12) for f, b, _, _ in CHECKS))
        print("minimum  " + " ".join(f"{lim:<12.2f}" for _, _, lim, _ in CHECKS))
        for name, d in rows:                     # how near the RGB565 token is to what was set
            if name in FAMILY:
                off = {k: max(abs(x - int(FAMILY[name][k][i:i + 2], 16)) for x, i in zip(from565(d[k]), (0, 2, 4))) for k in TOKENS}
                print(f"{name:8s} RGB565 off by at most {max(off.values())} of 255 ({max(off, key=off.get)})")
    if a.out:
        out = ["/* generated by tools/gen_ui_palettes.py: the UI palettes (12 stored colours each: bg surf text theme accent",
               " * mid dim line sel tint rec raise, src/gfx.c ux's order) */",
               "#pragma once", "#include <stdint.h>", "",
               "static const ui_pal_t UI_PALETTES[] = {"]
        for name, d in rows:
            out.append(f'    {{"{name}", {{' + ", ".join(f"0x{d[k]:04x}" for k in TOKENS) + "}},")
        out += ["};", "/* (0.8.9) KNOB 1..4's colours, each palette its own */",
                "static const uint16_t UI_KNOBS[][4] = {"]
        out += ["    {" + ", ".join(f"0x{d[f'k{i}']:04x}" for i in range(4)) + f"}},   /* {name} */" for name, d in rows]
        out += ["};",
                f"#define UI_NPALETTES {len(NAMES)}u",
                "#define UI_JIANT_INDEX 0u          /* JIANT FM's */",
                f"#define UI_BYTE_INDEX {NAMES.index('BYTE')}u          /* (JIANT BYTE) BYTE: the default (src/panel.c) */",
                f"#define UI_PAL_TAG {PAL_TAG}u          /* stored id = UI_PAL_TAG + UI_PAL_STORED[index]; below 20: an old id */",
                "/* what each palette is saved as: the index its nearest one had up to V10.7.18 (" + " ".join(V10) + ") */",
                "static const uint8_t UI_PAL_STORED[] = {" + ", ".join(str(v) for v in stored) + "};",
                "/* and back: the palette of a saved id (V10.7.18's eleven -> the nearest of the family) */",
                "static const uint8_t UI_PAL_READ[] = {" + ", ".join(str(v) for v in back) + "};",
                f"#define UI_KEY_PCT {PCT['KEY']}",
                f"#define UI_QR_LIGHT 0x{to565(QR_LIGHT):04x}u", f"#define UI_QR_DARK 0x{to565(QR_DARK):04x}u",
                f"#define UI_CRASH_BG 0x{to565(CRASH_BG):04x}u", f"#define UI_CRASH_INK 0x{to565(CRASH_INK):04x}u", "",
                "/* the 20 palettes of the earliest firmware: " + " ".join(OLD) + " */",
                "static const uint8_t UI_PALETTE_MIGRATE[20] = {" +
                ", ".join(str(NAMES.index(OLD_TO_NEW[n])) for n in OLD) + "};", ""]
        Path(a.out).write_text("\n".join(out))
    for f in fails:
        print("FAIL", f)
    if fails:
        sys.exit(1)


if __name__ == "__main__":
    main()
