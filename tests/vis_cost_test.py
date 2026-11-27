#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(VIS lite) the UI's cost a frame on HOME > VIS (and MENU > SAVER's visual) against every other page: tests/vis_cost_test.c
under valgrind's callgrind, the heaviest GEN song playing (cpu_lines' 101), each page's frames counted (ui_draw only).
Two measures a frame: its instructions (the host's, built -O2 -fno-inline: the source's functions) and the code it runs,
as the device's bytes (tools/audio_map.txt: each function it ran, its size on the FM-1; what the shared 32 KiB I-cache
holds for it). VIS's frame (its every 2nd UI frame, the frames it skips nearly free) must cost no more than the heaviest
page's, in both; its segments a frame at most VIS_SEG (ui_vis.c). Fails otherwise.
   python3 tests/vis_cost_test.py      (run_byte_tests.sh, with valgrind)"""
import collections, os, re, shutil, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRAMES = 6                      # (vis_cost_test.c FRAMES)


def sh(*a, **k):
    return subprocess.run(a, capture_output=True, text=True, **k)


def devsizes():
    d = {}
    for ln in open(os.path.join(ROOT, "tools", "audio_map.txt")):
        if not ln.startswith("#"):
            p = ln.split()
            d[p[0]] = int(p[1])
    return d


def dumps(out):
    """{trigger: (Ir, {functions run})} of each callgrind dump"""
    res = {}
    for f in sorted(os.listdir(out)):
        if not re.fullmatch(r"cg\.out\.\d+", f):
            continue
        names, fn, skip, run, trig, ir = {}, None, False, set(), None, 0
        for ln in open(os.path.join(out, f), errors="replace"):
            m = re.match(r"desc: Trigger: Client Request: (.*)", ln)
            if m: trig = m.group(1).strip(); continue
            m = re.match(r"summary: (\d+)", ln)
            if m: ir = int(m.group(1)); continue
            m = re.match(r"(c?fn)=\((\d+)\)(?: (.*))?", ln)
            if m:
                if m.group(3): names[m.group(2)] = m.group(3).strip()
                if m.group(1) == "fn": fn = names.get(m.group(2))
                continue
            if ln.startswith("calls="): skip = True; continue
            if re.match(r"[+\-*0-9]", ln):
                if skip: skip = False; continue
                p = ln.split()
                if fn and len(p) > 1 and int(p[1]): run.add(fn)
        if trig:
            res[trig] = (ir, run)
    return res


def main():
    out = tempfile.mkdtemp(prefix="vis_cost_")
    exe = os.path.join(out, "vis_cost")
    r = sh(os.environ.get("CC", "cc"), "-std=gnu99", "-O2", "-fno-inline", "-w", "-Ibuild/gen", "-Ifirmware/src", "-Itests",
           "-o", exe, "tests/vis_cost_test.c", "-lm", cwd=ROOT)
    if r.returncode:
        sys.exit("vis_cost: the build failed\n" + r.stderr[-3000:])
    r = sh("valgrind", "--tool=callgrind", "--collect-atstart=no", "--dump-line=no",
           f"--callgrind-out-file={out}/cg.out", exe, cwd=ROOT)
    print(r.stdout.strip())
    if r.returncode:
        sys.exit("vis_cost: the run failed (or VIS's segments a frame out of range)\n" + r.stderr[-2000:])
    dev = devsizes()
    def code(run):
        return sum(dev.get(re.sub(r"\.(constprop|part|isra|cold)\.\d+", "", f), 0) for f in run)
    pages, vis = [], []
    for trig, (ir, run) in dumps(out).items():
        m = re.match(r"VIS ([\w-]+) (\d+)", trig)
        if m:
            vis.append((m.group(1), ir / max(1, int(m.group(2))), code(run), int(m.group(2))))
        elif trig.startswith("PAGE "):
            pages.append((trig[5:], ir / FRAMES, code(run)))
    if not pages or not vis:
        sys.exit("vis_cost: no pages measured")
    hi = max(pages, key=lambda p: p[1])
    hc = max(pages, key=lambda p: p[2])
    bad = 0
    print(f"vis_cost: {len(pages)} pages; the heaviest frame {hi[0]}: {hi[1] / 1000:.0f} k instructions; the most code "
          f"{hc[0]}: {hc[2]} B of the device's")
    for k, i, c, n in vis:
        ok = i <= hi[1] and c <= hc[2]
        bad += not ok
        print(f"vis_cost: VIS ({k}) a frame drawn ({n} of {FRAMES}): {i / 1000:.0f} k instructions ({i / hi[1] * 100:.0f} % of "
              f"the heaviest page's), {c} B of code ({c / hc[2] * 100:.0f} %)" + ("" if ok else "  OVER"))
    print("vis_cost: " + ("FAILED: VIS's frame over the heaviest page's" if bad else
                          "VIS's frame within the heaviest page's (instructions and code)"))
    if os.environ.get("VC_KEEP"):
        print("vis_cost: the dumps in", out)
    else:
        shutil.rmtree(out, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
