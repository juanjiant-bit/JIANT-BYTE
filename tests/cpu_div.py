#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(BYTE 0.16) the audio path's cost as the FM-1 feels it: tests/cpu_vg.c under callgrind, each instruction counted, the
divisions (x86 div / idiv) weighted as the FM-1's pi32v2 pays them (DIV_W cycles: a divide there is iterative, the
host's one instruction). cpu_vg.py counts every instruction as one, so a part dividing a few times a sample looked as
cheap as one that does not and cost about twice as much on the device (the owner's measures: FORM 60 %, DRUM-W 33 %).
   python3 tests/cpu_div.py BINARY [SECTION-PREFIX ...]
Prints, per section: instructions a sample, divisions a sample, and the weighted cost (instructions + DIV_W x divs)."""
import os, re, subprocess, sys, tempfile, glob

DIV_W = 24
SAMPLES = None
binary = sys.argv[1]
want = sys.argv[2:]
src = open(os.path.join(os.path.dirname(__file__), "cpu_vg.py")).read()
SAMPLES = int(re.search(r"^SAMPLES\s*=\s*(\d+)", src, re.M).group(1)) if re.search(r"^SAMPLES\s*=\s*(\d+)", src, re.M) else 64 * 64

dis = subprocess.run(["objdump", "-d", "--no-show-raw-insn", binary], capture_output=True, text=True).stdout
divs = set()
for line in dis.splitlines():
    m = re.match(r"\s*([0-9a-f]+):\s+(\S+)", line)
    if m and re.fullmatch(r"i?div[bwlq]?", m.group(2)):
        divs.add(int(m.group(1), 16))

out = tempfile.mkdtemp(prefix="cpu_div_")
prefix = os.path.join(out, "cg.out")
r = subprocess.run(["valgrind", "--tool=callgrind", "--collect-atstart=yes", "--instr-atstart=no", "--dump-instr=yes",
                    "--compress-pos=no", "--compress-strings=no", f"--callgrind-out-file={prefix}", binary],
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
if r.returncode:
    print("cpu_div: the run failed\n" + r.stderr[-2000:]); sys.exit(1)
rows = []
for f in glob.glob(prefix + "*"):
    txt = open(f, errors="replace").read()
    m = re.search(r"^desc: Trigger: Client Request: (\S+)", txt, re.M)
    if not m or m.group(1).startswith("peak/"):
        continue
    name = m.group(1)
    if want and not any(name.startswith(w) for w in want):
        continue
    ir = nd = 0
    skip = False
    for line in txt.splitlines():
        if line.startswith("calls="):
            skip = True                                  # the next line: a call's inclusive cost, not this one's
            continue
        mm = re.match(r"(0x[0-9a-f]+)\s+(?:\d+\s+)?(\d+)\s*$", line)
        if not mm:
            continue
        c = int(mm.group(2))
        if skip:
            skip = False
            continue
        ir += c
        if int(mm.group(1), 16) in divs:
            nd += c
    rows.append((name, ir / SAMPLES, nd / SAMPLES))
for name, ir, nd in sorted(rows):
    print(f"cpu_div: {name:<22} {ir:7.0f} instr  {nd:5.1f} div  weighted {ir + DIV_W * nd:7.0f}  (divisions {100 * DIV_W * nd / (ir + DIV_W * nd):4.1f} %)")
