#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(JIANT BYTE, phase 1) firmware/src/fvm_bank.c: the factory formulas as text for the FVM (firmware/src/fvm.c).

  tools/gen_fvm_bank.py            writes firmware/src/fvm_bank.c

BYTE: MATH's 48 bytebeats (JIANT FM, firmware/src/eng_lofi.c lofi_byte) read out of its C, as written (the FVM takes C's
syntax and its u suffixes; tests/fvm_test.c checks them bit for bit). FLOAT: MATH's 48 floatbeats (lofi_float),
rewritten in the FVM's FIX: MATH's S(x * k) (x the time in Q8, a turn every 32 t) is sin(w * k) (w the note's phase in
turns, Q16) through the same sine table; the shorthands below are expanded here (the language has no names of its
own): E the step's envelope (1 at a step's start, 0 at its end, a step 2048 t), R(i) the just ratios (1 9/8 5/4 4/3 3/2
5/3 15/8 2, in sixteenths), N the formula's own hash noise (-0.5 .. 0.5). MATH's kicks add (k + 40) / 2^19 of a
turn a sample: ph() takes Hz, so (k + 40) x FS / 2^19 (0.0841140747 Hz each). tests/fvm_test.c compares them with MATH's.

(funcbank) FUNC's BANK (eng_lofi.c FUNC_BANK, the FORM 3 formula bank brought back, curated): BANK below, in families
(keys and pads, basses, leads and FM, textures, drums, bytebeats), each a name, its level (func-level: x / 128, measured
by tests/funcbank_test.c) and its formula in BANK's variables (eng_lofi.c func_bank_run): w w3 w5 wd the chord's root,
third, fifth and the root a little sharp (turns), T the 1/16's age, s the 1/16s, c the bar's 1/16 (0 .. 15), P the beats,
t P x 4096, D the chord's age, b its degree, A DEF, n the root's note.

(funcbank, flash) all the texts in one, packed by pairs (byte-pair encoding): FVM_BPE's 160 codes (1 .. 31, 127 .. 255;
the texts are ASCII) each two of the others or of the text's own characters, found here as the most frequent pair
each time; fvm_bank_to expands one formula (a few hundred operations) into the caller's buffer."""
import pathlib, re

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOFI = ROOT / "firmware/src/eng_lofi.c"
OUT = ROOT / "firmware/src/fvm_bank.c"

E = "(1-(t&2047)/2048.)"
N = "(fix((t*2654435761>>17)-16384)/32768)"
def R(i):
    return f'("@BDEHKNP"[{i}]-48)'

FLOAT = [
    f"sin(w*(((t>>11^t>>13)&7)+1))*{E}",
    "sin(w+sin(w*3)*(t>>10&15)*3/16)",
    f"sin(w*{R('0x36364689>>((t>>11&7)*4)&7')}/16)*{E}",
    "sin(w*(1+(t>>12&3)))*sin(w*((t>>9&7)+1))",
    "((t>>11&1?0.:sin(ph(fix(((t&2047)<1024?(1024-(t&2047))*(1024-(t&2047))>>6:0)+40)*0.0841140747))*(1024-min(t&2047,1024))/1024)+sin(w/2)/2)/2",
    f"sin(w*((t>>9&t>>11&7)|1))*{E}",
    "sat(sin(w*(1+(t>>11&1)))*(1+(t>>11&7)*3)/2)",
    f"sin(w*{R('(t>>11)*3+(t>>13)&7')}*(1+(t>>11&1))/16)",
    '(sin(w*("23456853"[t>>9&7]-48)/2)+sin(w*("23456853"[t>>11&7]-48)/4))/2',
    f"((t>>11&1?{N}:0.)*{E}+sin(w))/2",
    f"sin(w+sin(w*((t>>12&7)+1))*{E}*2)",
    "sin(w+sin(t/2048.)*6/16)",
    "sin(sin(w)*(2+(t>>11&7)))",
    "abs(sin(w*(1+(t>>11&1))))-abs(sin(w*(24+(t>>11&6)*4)/16))",
    "sin(floor(w*8192/(1<<4+(t>>11&7)))*(1<<4+(t>>11&7))/8192)",
    f"sin(w*((t>>8)%7+1))*{E}",
    f"(sin(w*(1+(t>>11&1)))*{E}+sin(w*(24+(t>>11&2)*8)/16)*max(1-fix(t%3072*10)/32768,0.))/2",
    f"sin(w*(64+(t>>6&255))/64)*{E}",
    "((t>>10&1?sin(w*(1+(t>>11)%3)):0.)+sin(w+t/2048.))/2",
    "(sin(w)+((t>>11&3)>0?sin(w*3)/3:0.)+((t>>11&3)>1?sin(w*5)/5:0.)+((t>>11&3)>2?sin(w*7)/7:0.))*3/4",
    "sin(w*(2+((t>>11)*5&7))/2)",
    f"((t>>11&3)==0?sin(ph(fix(((t&2047)<768?(768-(t&2047))*(768-(t&2047))>>6:0)+30)*0.0841140747))*(768-min(t&2047,768))/768:0.)/2"
    f"+((t>>(8+(t>>11&3))&1)?{N}*(1024-(t&1023))/4096:0.)/2+sin(w/2)/8",
    "sin(w*(1+(t>>9&t>>7&7))+fix(t>>9&t>>7&255)/16)",
    "sin(w*2+sin(w*5)*(t>>11&3)*8/16)",
    f"sin(w*(1+(t>>11&3)))*abs(sin(w*(1+(t>>11&3))))*{E}",
    f"sin(w-fix(t&1<<(t>>11&7))/32)*{E}",
    f"sin(w*(34-(t>>11&15))/32)*{E}",
    f"(sin(w)+sin(w*((t>>13&3)+2))*{E})/2",
    "sin(w+sin(w*2)*sin(t/8192.)*2)",
    "sin(w*(16+(t>>7&63))/16)",
    f"sin(w*{R('((t>>11)+1)*2654435761>>29')}/16)*{E}",
    f"(sin(w*{R('t>>12&7')}/16)+sin(w*{R('t>>12&7')}*20/256)+sin(w*{R('t>>12&7')}*24/256))/4"
    f"+(((t>>11)*5&7)<5?{N}*(512-(t&511))/2048*{E}:0.)/2",
    f"sin(w*((t>>11)%5+1))*{E}",
    f"sin(w+sin(w*7)*{E})",
    f"(sin(w)+sin(w*3/2)+sin(w*{R('t>>11&7')}/16))/3",
    f"(t>>11&3)!=3?sin(w*{R('(t>>11)*5&7')}*2/16)*{E}:0.",
    "sin(w+sin(w/2)*(t>>9&15)*2/16)",
    "sin(w)>0?sin(w):sin(w)/4",
    "sin(w*(1+(t>>10&1)*2))*sin(t/4096.)",
    f"sin(w*(8+(t>>8&7))/8)*{E}",
    f"(sin(w*2)*{E}+sin(w*3)*(1-{E}))/2",
    "sin(w+sin(w*5)*(t>>11&7)*4/16)",
    f"(t&2047)<1024?sin(w*{R('t>>13&7')}/16)*{E}:0.",
    "sat((sin(w)*3+sin(w*2)*2)/2)",
    "sin(w*((t>>9^t>>12)&7|1))",
    f"sin(w*(32+(t>>5&31))/32)*{E}",
    "sin(w)*sin(w*(1+(t>>11&3)))+sin(w/2)/4",
    "sin(w+sin(w*3)*sin(t/4096.))",
]

def byte_forms():
    src = LOFI.read_text()
    start = src.index("uint32_t lofi_byte(")
    body = src[start:src.index("\n}\n", start)]          # (lofi_byte's body alone: lofi_mut's cases come after)
    forms = {}
    for m in re.finditer(r"(case (\d+)|default): return (.*?);\s*(/\*.*?\*/)?\s*$", body, re.M):
        forms[int(m.group(2)) if m.group(2) else "default"] = m.group(3).strip()
    return [forms.get(i, forms["default"]) for i in range(48)]


# (funcbank) FUNC's BANK: name, level (x / 128), formula; in families, the journey FORM A / FORM B turn through
FB = [
    # keys, pads, chords: the chord's three tones (w w3 w5)
    ("PAD", 80, "(sin(w*2)+sin(w3*2)+sin(w5*2)+sin(w)*A)*.27"),
    ("SAW CHORD", 74, "lp(saw(w*2)+saw(w3*2)+saw(w5*2),.04+A*.4)*.33"),
    ("SUPERSAW", 84, "lp(saw(w*2)+saw(wd*2)+saw(w3*2)*.7+saw(w5*2)*.7,.1+A*.6)*.28"),
    ("BRASS", 73, "lp(saw(w*2)+saw(w5*2)+saw(w3*4)*.6,.03+min(P%4,1)*(.08+A*.4))*.38"),
    ("ORGAN", 81, "(sin(w*2)+sin(w*4)*.5+sin(w*6)*.3*A+sin(w3*2)*.4+sin(w5*2)*.4)*.38"),
    ("KEYS", 116, "(sin(w*4)+sin(w*8)*.3*A+sin(w3*4)*.5+sin(w5*4)*.5)*(1-P*2%1*.7)*.4"),
    ("STRUM", 112, "(sin(w*2)+sin(w3*2)*(P%1>.04+A*.1)+sin(w5*2)*(P%1>.08+A*.2))*(1-P%1*.8)*.38"),
    ("STAB", 129, "sat(lp(saw(w*2)+saw(w3*2)+saw(w5*2),.06+A*.4)*max(1-(P+.5)%1*2.5,0)*1.2)"),
    ("PUMP PAD", 118, "lp(saw(w*2)+saw(w3*2)+saw(w5*2),.08+A*.2)*.36*(1-(1-P%1)*(1-P%1))"),
    ("VOX", 68, "(sin(w*2)*sin(w*(6+floor(A*2.99)*2))+sin(w3*2)*sin(w3*(6+floor(A*2.99)*2))*.6)*.55"),
    ("ARP", 58, "sin((c%4==1?w5:c%4==3?w3*2:w)*4)*(1-T*(.7-A*.6))*.7"),
    # basses: the root (w), an octave under the chords
    ("SUB", 49, "(sin(w)+tri(w*2)*A*.5)*.75"),
    ("FM BASS", 39, "sin(w+sin(w*2)*(1-T)*(.15+A*.6))*.85"),
    ("ACID", 34, "sat(lp(saw(w),.02+(1-T)*(1-T)*(.05+A*.6))*4)*.9"),
    ("REESE", 75, "lp(saw(w)+saw(wd),.04+A*.3)*.5"),
    ("WOBBLE", 70, "lp(saw(w)+sqr(w),.01+abs(sin(P*(1+floor(A*4))))*.3)*.8"),
    ("GROWL", 51, "sin(w*(1+floor(A*3.99))+sin(w)*.6*abs(sin(P*2)))*.85"),
    ("DUB", 40, "sat(lp(saw(w)+saw(wd)+sin(w),.01+abs(sin(P*3))*(.15+A*.4))*2)*.9"),
    ("WALK", 79, "lp(saw(c%8==3?w3:c%8==6?w5:w),.04+(1-T)*(1-T)*(.05+A*.4))*(1-T*.5)*.95"),
    ("KICK BASS", 103, "sat(sin(T*10-(1-T)*(1-T)*6)*(1-T)*(c%4==0)*1.3+saw(w)*(c%4>1)*(.4+A*.6))"),
    # leads and FM: the root's partials
    ("PLUCK", 102, "sat(saw(w*2)*(1-T)*(1+A*4))*(28013>>c&1)*.8"),
    ("FM BELL", 124, "sin(w*4+sin(w*8)*(1-T)*(.05+A*.5))*(1-(T+s%3)*.33)*(1-(T+s%3)*.33)*.85"),
    ("NESTED FM", 141, "sin(w*2+sin(w*3+sin(w*5)*A*.3)*.25)*(1-T*.7)*(s*5%8<5)*.85"),
    ("PWM", 30, "(sqr(w*2)-sqr(w*2+.5+tri(P/4)*.4*A))*.45"),
    ("SYNC", 45, "tri(w*(2+A*4+(1-P%1)*(1-P%1)))*.85"),
    ("FOLD", 33, "sin(sin(w*2)*(.3+A*1.5+(1-T)*.5))*.85"),
    ("FORMANT", 51, "sin(w*2)*sin(w*2*(2+floor((1+tri(P/4))*A*3)))*.85"),
    ("RING", 63, "sin(w*4)*sin(w*(2+floor(A*3.99)*2))*(1-T*.5)*.9"),
    ("NIBBLE", 77, "lp(sqr(w*2),.1+A*.4)*((s*3)%8<5)*(1-T*.7)*.85"),
    # textures
    ("CRUSH CHD", 72, "floor((sin(w*2)+sin(w3*2)+sin(w5*2))*(2+A*6))/(2+A*6)*.3"),
    ("RISER", 82, "(rnd(P)*D*A+sin(w*2*(1+D)))*D*.6"),
    # drums: the 1/16s (T c s), the noise rnd(P)
    ("KICK", 125, "sin(P%1*40-(1-P%1*2)*(1-P%1*2)*(6+A*10))*max(1-P%1*2,0)"),
    ("SNARE", 129, "sat((rnd(P)+sin(P*120)*.5)*(1-(P+1)%2*2)*(1-(P+1)%2*2)*((P+1)%2<.5)*(1.2+A))"),
    ("HATS", 141, "hp(rnd(P),.5)*(1-T*(.6+(c%4!=2)*.4))*(.7+A*.5)"),
    ("CLAP", 130, "sat(rnd(P)*((T<.08)+(T>.12)*(T<.2)+(T>.25)*(1-T*.7))*(c%8==4)*(1.4+A))"),
    ("BEAT", 129, "sat((sin(T*10-(1-T)*4)*(c%4==0)+rnd(P)*((c%8==4)*.8+(c%2)*A*.6))*(1-T*.8)*1.3)"),
    ("BREAK", 134, "sat((sin(T*10-(1-T)*4)*(2313>>c&1)+rnd(P)*((4112>>c&1)*.8+A*.3))*(1-T*.8)*1.3)"),
    ("PERC", 141, "sat(sin(w*4*(1+(1-T)*2))*(1-T)*((s*5+A*8)%8<3)*1.4)"),
    ("RUMBLE", 129, "sat(lp(sin(T*10-(1-T)*(1-T)*4)*(c%4==0)+rnd(P)*.3,.03+A*.1)*(1-T)*2)"),
    ("TOMS", 137, "sat(sin(w*2*(1+(1-T)*.5))*(1-T*.7)*(1170>>c&1)*1.5)"),
    # bytebeats: on the root's phase (its byte a cycle: in tune), the beats' bits (t)
    ("BYTE", 54, "lp(fix(int(w*256)*(t>>11&3|1)&(t>>8|t>>11)&255)/128-1,.15+A*.6)"),
    ("BITS", 47, "lp(fix((int(w*256)^(t>>7))&255)/128-1,.15+A*.7)"),
    ("XOR", 76, "lp(fix((int(w*256)*(b+1)&int(w5*256)|t>>8)&255)/128-1,.2+A*.6)"),
    ("GLITCH", 52, "lp(fix(int(w*256)*(s%7+1)>>(c%4)&255)/128-1,.15+A*.6)"),
    ("BITGROOVE", 62, "lp(fix(int(w*256)*((t>>11)%4+1)&t>>5&255)/128-1,.2+A*.5)"),
]
# (funcbank, flash) the pairs: codes 1 .. 31 and 127 .. 255 (the texts' own characters 32 .. 126)
CODES = list(range(1, 32)) + list(range(127, 256))
def bpe(texts):
    from collections import Counter
    seqs = [list(t.encode("ascii")) for t in texts]
    merges = []
    for code in CODES:
        cnt = Counter()
        for q in seqs:
            for a, b in zip(q, q[1:]):
                cnt[(a, b)] += 1
        if not cnt:
            break
        pair, n = max(cnt.items(), key=lambda kv: (kv[1], -kv[0][0], -kv[0][1]))   # (deterministic)
        if n < 3:                                    # (a pair costs two bytes of the table)
            break
        merges.append(pair)
        for i, q in enumerate(seqs):
            o, j = [], 0
            while j < len(q):
                if j + 1 < len(q) and (q[j], q[j + 1]) == pair:
                    o.append(code); j += 2
                else:
                    o.append(q[j]); j += 1
            seqs[i] = o
    return merges, seqs
def unbpe(q, merges):
    out = []
    def x(c):
        if c < 32 or c >= 127:
            k = c - 1 if c < 32 else c - 96
            x(merges[k][0]); x(merges[k][1])
        else:
            out.append(chr(c))
    for c in q:
        x(c)
    return "".join(out)
def cbytes(q):
    o, prev = [], ""
    for c in q:
        ch = chr(c)
        if c < 32 or c >= 127 or ch in '"\\' or (ch == "?" and prev == "?"):
            o.append("\\%03o" % c)
        else:
            o.append(ch)
        prev = ch
    return "".join(o)
def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def main():
    byte = byte_forms()
    assert len(FLOAT) == 48, len(FLOAT)
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton "
                "Instruments\n * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */\n"
                "/* (JIANT BYTE) the factory formulas, as text for the FVM (fvm.c). Generated by tools/gen_fvm_bank.py: "
                "do not edit.\n * BYTE: MATH's bytebeats as written (eng_lofi.c); FLOAT: MATH's floatbeats in the FVM's "
                "FIX. */\n")
        f.write("#ifndef FVM_BANK_C\n#define FVM_BANK_C\n")
        f.write(f"#define FVM_NBYTE {len(byte)}u\n#define FVM_NFLOAT {len(FLOAT)}u\n")
        # (partstate) one text, each formula ended by its 0: no pointer each (384 B of the image); (funcbank, flash) packed
        # by pairs (FVM_BPE), FUNC's BANK after BYTE's and FLOAT's
        allf = [("B", s) for s in byte] + [("F", s) for s in FLOAT] + [("U", s) for (_, _, s) in FB]
        assert all(re.fullmatch(r"[A-Z0-9 ]{1,9}", nm) for nm, _, _ in FB), "BANK's names: the S face's, 9 at most"
        merges, seqs = bpe([s for _, s in allf])
        for (k, s), q in zip(allf, seqs):
            assert unbpe(q, merges) == s and len(s) <= 255, s   # (fvm_bank_to's buffers: 256)
        raw = sum(len(s) + 1 for _, s in allf)
        z = sum(len(q) + 1 for q in seqs)
        f.write(f"#define FB_N {len(FB)}u                  /* (funcbank) FUNC's BANK (eng_lofi.c FUNC_BANK) */\n")
        f.write(f"#define FB_NPITCH {[n for n, _, _ in FB].index('KICK')}u            /* (the pitched first: keys, basses, leads, textures) */\n")
        f.write(f"/* (funcbank, flash) {len(allf)} texts, {raw} B, packed to {z} B and {2 * len(merges)} B of pairs: code c (1 .. 31, "
                "127 .. 255) is FVM_BPE[c < 32 ? c - 1 : c - 96]'s two, each a character or a code again */\n")
        f.write(f"static const uint8_t FVM_BPE[{len(merges)}][2] = {{\n")
        for r in range(0, len(merges), 12):
            f.write("    " + " ".join(f"{{{a}, {b}}}," for a, b in merges[r:r + 12]) + "\n")
        f.write("};\n")
        f.write("static const char FVM_BANK_Z[] =\n")
        for i, ((k, s), q) in enumerate(zip(allf, seqs)):
            n = i + 1 if k == "B" else i + 1 - len(byte) if k == "F" else i + 1 - 2 * len(byte)
            last = i + 1 == len(allf)
            nm = f"{k}{n:02d}" if k != "U" else FB[n - 1][0]
            f.write(f"    /* {nm} */ \"{cbytes(q)}{'' if last else chr(92) + '0'}\"{';' if last else ''}\n")
        f.write("/* (funcbank) BANK's names (str_nth) and levels (x / 128: tests/funcbank_test.c) */\n")
        f.write("static const char FB_NAMES[] = " + "".join(cstr(nm)[:-1] + ("\\0\"" if i + 1 < len(FB) else "\"") for i, (nm, _, _) in enumerate(FB)) + ";\n")
        f.write(f"static const uint8_t FB_LV[FB_N] = {{{', '.join(str(lv) for _, lv, _ in FB)}}};\n")
        f.write("/* code c's text into o, past it returned (a pair's two, each again) */\n"
                "static char *fvm_unz1(char *o, uint32_t c)\n{\n"
                "    if (c - 1u < 31u || c >= 127u) {\n"
                "        const uint8_t *q = FVM_BPE[c < 32u ? c - 1u : c - 96u];\n"
                "        return fvm_unz1(fvm_unz1(o, q[0]), q[1]);\n    }\n"
                "    *o = (char)c;\n    return o + 1;\n}\n"
                "/* formula i of the whole (BYTE's 0 .., FLOAT's FVM_NBYTE .., BANK's 2 x FVM_NBYTE ..) into o (256 B), its length */\n"
                "static uint32_t fvm_bank_to(uint32_t i, char *o)\n{\n"
                "    const uint8_t *s = (const uint8_t *)FVM_BANK_Z;\n    char *e = o;\n"
                "    for (; i; i--)\n        while (*s++) {}\n"
                "    while (*s)\n        e = fvm_unz1(e, *s++);\n"
                "    *e = 0;\n    return (uint32_t)(e - o);\n}\n"
                "/* the BYTE (flt 0) or FLOAT (flt 1) bank's formula i: the main loop's (its buffer, one at a time; the audio's\n"
                " * fvm_bank_to its own) */\n"
                "static char fvm_tx[256] __attribute__((section(\".bss.funcbank\")));   /* (out of the merged statics) */\n"
                "static const char *fvm_bank(uint32_t flt, uint32_t i)\n{\n"
                "    fvm_bank_to(i % FVM_NBYTE + (flt ? FVM_NBYTE : 0u), fvm_tx);\n    return fvm_tx;\n}\n")
        f.write("#endif\n")
    print(f"fvm bank: {len(byte)} BYTE, {len(FLOAT)} FLOAT, {len(FB)} BANK -> {OUT}")

if __name__ == "__main__":
    main()
