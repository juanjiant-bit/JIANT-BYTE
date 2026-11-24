#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(BYTE, engopt) tests/engopt_vg.c's scenarios: instructions a block (callgrind, exact), the worst block (the slowest
blocks of a dozen native runs, each then counted by callgrind), the audio (its hash; against a reference, the
difference in dB).
   python3 tests/engopt_vg.py BINARY OUTDIR [REFDIR] [PATTERN]"""
import concurrent.futures as cf, glob, hashlib, math, os, re, struct, subprocess, sys, tempfile
from array import array

binary, outdir = sys.argv[1], sys.argv[2]
ref = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] != "-" else None
pat = re.compile(sys.argv[4]) if len(sys.argv) > 4 else None
os.makedirs(outdir, exist_ok=True)
names = [n for n in subprocess.run([binary, "LIST"], capture_output=True, text=True).stdout.split("\n") if n]
if pat: names = [n for n in names if pat.search(n)]
RUNS = int(os.environ.get("RUNS", "12"))
TOP = 6


def cg(name, arg):
    d = tempfile.mkdtemp(prefix="eo_")
    subprocess.run(["valgrind", "--tool=callgrind", "--collect-atstart=no", "--dump-instr=no",
                    f"--callgrind-out-file={d}/cg", binary, name, "-", arg], capture_output=True)
    out = {}
    for f in glob.glob(d + "/cg*"):
        t = open(f, errors="replace").read()
        m = re.search(r"^desc: Trigger: Client Request: (\S+)", t, re.M)
        s = re.search(r"^(?:totals|summary): (\d+)", t, re.M)
        if m and s: out[m.group(1)] = int(s.group(1))
        os.remove(f)
    os.rmdir(d)
    return out


def one(name):
    safe = name.replace("/", "_").replace(" ", "_")
    raw = os.path.join(outdir, safe + ".raw")
    subprocess.run([binary, name, raw], check=True)
    mins = None
    for r in range(RUNS):
        b = subprocess.run([binary, name, "-", "time"], capture_output=True).stdout
        a = array("I", b)
        mins = list(a) if mins is None else [min(x, y) for x, y in zip(mins, a)]
    nb = len(mins)
    med = sorted(mins)[nb // 2]
    top = sorted(range(nb), key=lambda i: -mins[i])[:TOP]
    tot = cg(name, "-").get("total", 0)
    pk = cg(name, "dump=" + ",".join(map(str, top)))
    worst = max(pk.items(), key=lambda kv: kv[1]) if pk else ("-", 0)
    h = hashlib.sha1(open(raw, "rb").read()).hexdigest()[:10]
    diff = ""
    if ref:
        rr = os.path.join(ref, safe + ".raw")
        if os.path.exists(rr):
            A = array("i", open(raw, "rb").read()); B = array("i", open(rr, "rb").read())
            if A == B: diff = "exact"
            else:
                n = min(len(A), len(B))
                e = sum((A[i] - B[i]) ** 2 for i in range(n)); s = sum(B[i] ** 2 for i in range(n)) or 1
                diff = f"{10 * math.log10(e / s):.1f}dB" if e else "exact"
    return name, tot / nb, worst[1], worst[0], max(mins) / med, h, diff


with cf.ThreadPoolExecutor(4) as ex:
    res = list(ex.map(one, names))
with open(os.path.join(outdir, "table.txt"), "w") as fo:
    for r in res:
        line = f"{r[0]:24s} avg {r[1]:9.0f}  worst {r[2]:9d} ({r[3]:6s})  t-spike x{r[4]:4.1f}  {r[5]} {r[6]}"
        print(line); fo.write(line + "\n")
