#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(cpu3, the owner: "con 3 pistas ya se explota la CPU") the owner's project (tests/cpu3_test.c) under callgrind: each
core's instructions in every block of 64 samples, the mean and the worst, as the FM-1's time. The device: a block is
64 / 44100 s = 1451 us at 360 MHz, 522 k cycles a core. pi32v2 runs 0.6 .. 0.85 of an x86-64 instruction for one in the
hot functions (build/felucca.dis against gcc -O2: fvm_blk_op 0.72, rev_room 0.85, master_run 0.77, fvm_music 0.77), at
about one a cycle; R = 1 cycle an x86 instruction leaves a quarter of it for the stalls (ABOUT before c1fault: a FUNC
voice ~39 us a block, the buses 17 us, as this reads them). The three parts the owner plays (T1 DRUM-W, T2 FUNC, T4 MATH)
under 60 % of a core on either core, mean and worst block; T3 / T5 / T6 with them as well.
   python3 tests/cpu3_test.py BINARY        (run_byte_tests.sh, when valgrind is there)"""
import glob, os, re, subprocess, sys, tempfile

R = 1.0                      # FM-1 cycles an x86-64 instruction (see above)
MHZ = 360.0
BLOCK_US = 64 / 44100 * 1e6
LIMIT = 0.60

binary = sys.argv[1]
out = tempfile.mkdtemp(prefix="cpu3_")
per = {}                     # (mask, core) -> [instructions a block]
for core, start in ((0, "yes"), (1, "no")):
    prefix = os.path.join(out, f"c{core}.out")
    r = subprocess.run(["valgrind", "--tool=callgrind", f"--collect-atstart={start}",
                        f"--callgrind-out-file={prefix}", binary, "vg", str(core)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        print("cpu3: the run failed\n" + r.stderr[-2000:])
        sys.exit(1)
    for f in glob.glob(prefix + ".*"):
        txt = open(f, errors="replace").read()
        m = re.search(r"^desc: Trigger: Client Request: cpu3/([0-9a-f]{2})/\d+", txt, re.M)
        t = re.search(r"^(?:totals|summary): (\d+)", txt, re.M)
        if m:
            per.setdefault((m.group(1), core), []).append(int(t.group(1)) if t else 0)
if not per:
    print("cpu3: no blocks measured")
    sys.exit(1)
bad = 0
NAMES = {"0b": "T1 DRUM-W + T2 FUNC + T4 MATH", "0f": "  + T3 WAVE", "1b": "  + T5 WAVE", "2b": "  + T6 MATH"}
for mask in sorted({k[0] for k in per}):
    row = []
    for core in (0, 1):
        v = per.get((mask, core), [0])
        mean, worst = sum(v) / len(v), max(v)
        us_m, us_w = mean * R / MHZ, worst * R / MHZ
        row.append(f"core {core} {mean / 1000:5.1f} k / {worst / 1000:5.1f} k instr = {us_m:4.0f} / {us_w:4.0f} us "
                   f"({us_m / BLOCK_US * 100:3.0f} / {us_w / BLOCK_US * 100:3.0f} %)")
        if us_w > LIMIT * BLOCK_US:
            bad += 1
            row[-1] += "  OVER"
    print(f"cpu3: {NAMES.get(mask, mask):30s} " + "   ".join(row))
print("cpu3: " + ("FAILED: over 60 % of a core" if bad else "every core under 60 % of its block (mean / worst)"))
sys.exit(1 if bad else 0)
