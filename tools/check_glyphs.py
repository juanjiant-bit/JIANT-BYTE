#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(BYTE 0.17) every character the firmware's text can hold has a glyph in the faces that draw it.

  check_glyphs.py UI_FONTS.H SRC_OR_DIR ...

Run by tools/build.py generate() after the fonts are made (tools/gen_aa_font.py): the C string and character literals
of the sources (comments left out) and of the generated headers, decoded as the compiler does. S and M (the labels,
values and names) hold printable ASCII 32..95, lowercase folding to capitals, and their extras; F (formulas) 32..126.
A byte of 127 or more in a literal must be one S and M both have (today the middle dot '\\xb7' and the ellipsis,
gfx.c ELLIPSIS '\\x85'), or it would draw as '?' (two of them for a UTF-8 character such as an arrow or a times
sign: write the Latin-1 escape, and add the glyph to gen_aa_font.py UI_EXTRAS). Bytes under 32
are the texts' own markup (ui_menu.c's kind octets, line breaks) and pass. L, the sparse face, has its own check of
the strings drawn in it (tests/ui_test.c test_large_face, tests/ui_render.c lg_hook).
"""
import re
import sys
from pathlib import Path

# literals that are numbers, not text: (the file's name, the literal's bytes)
DATA = {("gen.c", b"\xff\xfd\t\xfe\x00")}   # gen_lead's figure offsets (int8_t)
ESC = {"n": 10, "t": 9, "r": 13, "0": 0, "a": 7, "b": 8, "f": 12, "v": 11, "\\": 92, "'": 39, '"': 34, "?": 63}


def faces(path):
    """{name: (first, last, set of extra codes)} of a generated ui_fonts.h"""
    t = Path(path).read_text()
    out = {}
    for m in re.finditer(r"static const aafont_t AF_(\w+) = \{(\d+), (\d+), (\d+), (\d+), (\d+),", t):
        n, first, last, nex = m.group(1), int(m.group(4)), int(m.group(5)), int(m.group(6))
        ex = re.search(r"AF_%s_EX\[\d+\] = \{([^}]*)\}" % ("S" if n == "X" else n), t)
        out[n] = (first, last, {int(x) for x in ex.group(1).split(",")[:nex] if x.strip()} if ex else set())
    return out


def has(face, c):
    first, last, ex = face
    if 97 <= c <= 122 and last < 97:
        c -= 32
    return first <= c <= last or c in ex


def literals(src):
    """(line, bytes) of every string or character literal of a C source, comments skipped"""
    i, n, line = 0, len(src), 1
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
        if src.startswith("//", i):
            i = src.find("\n", i)
            i = n if i < 0 else i
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            line += src.count("\n", i, j)
            i = j
            continue
        if c in "\"'":
            j, out = i + 1, bytearray()
            while j < n and src[j] != c and src[j] != "\n":
                if src[j] == "\\" and j + 1 < n:
                    e = src[j + 1]
                    if e == "x":
                        m = re.match(r"[0-9a-fA-F]+", src[j + 2:])
                        out.append(int(m.group(0), 16) & 255 if m else 0)
                        j += 2 + (len(m.group(0)) if m else 0)
                        continue
                    if e in "01234567":
                        m = re.match(r"[0-7]{1,3}", src[j + 1:])
                        out.append(int(m.group(0), 8) & 255)
                        j += 1 + len(m.group(0))
                        continue
                    if e == "\n":
                        line += 1
                    out.append(ESC.get(e, ord(e) & 255))
                    j += 2
                    continue
                out += src[j].encode("utf-8", "surrogateescape")
                j += 1
            yield line, bytes(out)
            i = j + 1
            continue
        i += 1


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    fc = faces(sys.argv[1])
    need = [fc[n] for n in ("S", "M") if n in fc]
    if len(need) != 2:
        raise SystemExit(f"check_glyphs: no S and M faces in {sys.argv[1]}")
    files = []
    for a in sys.argv[2:]:
        p = Path(a)
        files += sorted(p.rglob("*.[ch]")) if p.is_dir() else [p]
    bad = []
    for f in files:
        for line, b in literals(f.read_text(encoding="utf-8", errors="surrogateescape")):
            if (f.name, b) in DATA:
                continue
            miss = sorted({c for c in b if c >= 127 and not all(has(face, c) for face in need)})
            if miss:
                bad.append(f"{f}:{line}: {', '.join('0x%02x' % c for c in miss)} in {b[:40]!r}")
    if bad:
        print("check_glyphs: characters the S / M faces lack (tools/gen_aa_font.py UI_EXTRAS; they would draw as '?'):")
        print("\n".join("  " + x for x in bad[:40]))
        raise SystemExit(1)
    print(f"check_glyphs: {len(files)} files, every literal's character in S and M "
          f"(extras {sorted(fc['S'][2])})")


if __name__ == "__main__":
    main()
