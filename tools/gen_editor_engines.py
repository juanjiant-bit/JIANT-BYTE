#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(JIANT BYTE) web/editor.html's engine tables from the firmware itself: FORM, DRUM, WAVE and MATH as the device has
them (their EDIT 1 / EDIT 2 descriptors, value names and factory presets with their MODS), the engine numbers
(engines.c ENGINES[], ENG_LIVE, ENGINE_ORDER), FORM's own labels of common parameters (eng_form.c form_common_desc) and
its pages (params.c PAGES, SC_FORM).

The firmware's initializers are full of macros (FP / FP2, MODS, FX, TAG, WAVE_NF ..), so nothing is parsed: a small C
program is built with the host compiler against the firmware's sources the way the host tests build them
(tests/hostsim.c) and prints the tables as JSON; this script writes them as JavaScript between the markers

    /*BYTE-ENGINES-BEGIN*/ ... /*BYTE-ENGINES-END*/

of web/editor.html (the site build copies editor.html alone: web/make_site.py). Needs build/gen (one ./tools/build.py
run, or the host tests' tables).
    tools/gen_editor_engines.py            rewrite web/editor.html
    tools/gen_editor_engines.py --check    exit 1 when web/editor.html is not up to date
    tools/gen_editor_engines.py --json     print the tables
"""
import json, os, re, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HTML = ROOT / "web" / "editor.html"
BEGIN, END = "/*BYTE-ENGINES-BEGIN*/", "/*BYTE-ENGINES-END*/"

DUMP_C = r"""
#define main hostsim_main
#include "tests/hostsim.c"
#undef main
static void js(const char *s)
{
    putchar('"');
    for (; s && *s; s++) { if (*s == '"' || *s == '\\') putchar('\\'); putchar(*s); }
    putchar('"');
}
static void desc(const param_desc_t *d)
{
    int32_t i;
    printf("{\"label\":"); js(d->label);
    printf(",\"fmt\":%u,\"min\":%d,\"max\":%d,\"def\":%d,\"unit\":", (unsigned)desc_fmt(d), d->min, d->max, d->def);
    js(desc_unit(d) ? desc_unit(d) : "");
    printf(",\"names\":");
    if (d->names) {
        putchar('[');
        for (i = d->min; i <= d->max; i++) { if (i > d->min) putchar(','); js(desc_nm(d, i)); }   /* (bigtext: packed lists) */
        putchar(']');
    } else printf("null");
    putchar('}');
}
int main(void)
{
    uint32_t e, k, i;
    printf("{\"nengines\":%u,\"pcount\":%u,\"pe0\":%u,\"gcount\":%u,", (unsigned)NENGINES, (unsigned)P_COUNT, (unsigned)P_E0,
           (unsigned)G_COUNT);
    printf("\"names\":[");
    for (e = 0; e < NENGINES; e++) { if (e) putchar(','); js(ENGINES[e]->name); }
    printf("],\"live\":[");
    for (e = 0; e < NENGINES; e++) printf("%s%u", e ? "," : "", (unsigned)ENG_LIVE[e]);
    printf("],\"order\":[");
    for (e = 0; e < NENG_SHOWN; e++) printf("%s%u", e ? "," : "", (unsigned)ENGINE_ORDER[e]);
    printf("],\"engines\":{");
    for (e = 0; e < NENG_SHOWN; e++) {
        const engine_t *en = ENGINES[ENGINE_ORDER[e]];
        printf("%s\"%u\":{\"name\":", e ? "," : "", (unsigned)ENGINE_ORDER[e]); js(en->name);
        printf(",\"titles\":["); js(en->page_title[0]); putchar(','); js(en->page_title[1]);
        printf("],\"edit\":[");
        for (k = 0; k < 8; k++) { if (k) putchar(','); desc(&en->edit[k]); }
        printf("],\"presets\":[");
        for (i = 0; i < en->npresets; i++) {
            const preset_t *p = &en->presets[i];
            printf("%s{\"name\":", i ? "," : ""); js(p->name);
            printf(",\"e\":[");
            for (k = 0; k < 8; k++) printf("%s%d", k ? "," : "", p->e[k]);
            printf("],\"env\":[%u,%u,%u,%u],\"fenv\":%d,\"mono\":%u,\"fx\":[%u,%u,%u,%u],\"pat\":%u,\"xp\":%u,\"mod\":[",
                   p->env[0], p->env[1], p->env[2], p->env[3], p->fenv, p->mono, p->fx[0], p->fx[1], p->fx[2], p->fx[3],
                   p->pat, p->xp);
            for (k = 0; p->mod && p->mod[k] >= 0; k += 2) printf("%s[%d,%d]", k ? "," : "", p->mod[k], p->mod[k + 1]);
            printf("]}");
        }
        printf("]}");
    }
    printf("},\"form_common\":{");               /* (BYTE: FORM retired, its number WAVE's: none of its own) */
    printf("},\"form_pages\":[");
    printf("],\"engsel\":");
    desc(&GP[G_ENGSEL]);
    printf(",\"ids\":{\"P_ED_FLT\":%u,\"P_LRATE\":%u,\"P_LWAVE\":%u,\"P_LD_PIT\":%u,\"P_LD_FLT\":%u,\"P_DIST\":%u,"
           "\"P_VOICE\":%u,\"P_TVEL\":%u,\"P_LRTRG\":%u,\"P_DKEYMODE\":%u,\"V_LEGATO\":%u,\"V_POLY\":%u,\"PT_BASS\":%u}",
           P_ED_FLT, P_LRATE, P_LWAVE, P_LD_PIT, P_LD_FLT, P_DIST, P_VOICE, P_TVEL, P_LRTRG, P_DKEYMODE, V_LEGATO, V_POLY,
           PT_BASS);
    printf(",\"trk_def\":[");
    for (i = 0; i < NPART; i++) printf("%s[%u,%u,%u]", i ? "," : "", TRK_DEF[i][0], TRK_DEF[i][1], TRK_DEF[i][2]);
    printf("]}\n");
    return 0;
}
"""


def dump():
    gen = ROOT / "build" / "gen"
    if not (gen / "felucca_tables.h").exists():
        sys.exit("gen_editor_engines: no build/gen/felucca_tables.h: run ./tools/build.py once first")
    cc = os.environ.get("CC", "cc")
    with tempfile.TemporaryDirectory() as d:
        src, exe = Path(d) / "editor_engines_dump.c", Path(d) / "editor_engines_dump"
        src.write_text(DUMP_C)
        subprocess.run([cc, "-O1", "-w", "-std=gnu99", f"-I{ROOT}", f"-I{gen}", f"-I{ROOT / 'firmware' / 'src'}",
                        "-o", str(exe), str(src), "-lm"], check=True, cwd=ROOT)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def js_desc(d):
    """a descriptor as the editor's D(label, fmt, min, max, def, names, unit) (F_* numbers are the editor's F.*)"""
    args = [json.dumps(d["label"]), str(d["fmt"]), str(d["min"]), str(d["max"]), str(d["def"])]
    if d["names"] or d["unit"]:
        args.append(json.dumps(d["names"]) if d["names"] else "null")
    if d["unit"]:
        args.append(json.dumps(d["unit"]))
    return f"D({', '.join(args)})"


def js_preset(p):
    """P(name, e, env, mono, pat, xp, mod, fenv, fx): the editor's preset record (fx: the sends stored + 1, 0 none)"""
    return (f"P({json.dumps(p['name'])}, {json.dumps(p['e'])}, {json.dumps(p['env'])}, {p['mono']}, {p['pat']}, "
            f"{p['xp']}, {json.dumps(p['mod'])}, {p['fenv']}, {json.dumps(p['fx'])})")


def render(t):
    L = [BEGIN + " /* (JIANT BYTE) generated by tools/gen_editor_engines.py from firmware/src: do not edit by hand */",
         "/* the firmware's layout these tables are for (core.h P_COUNT, P_E0, G_COUNT, NENGINES) */",
         f"const BYTE_LAYOUT = {{ nengines: {t['nengines']}, pcount: {t['pcount']}, pe0: {t['pe0']}, gcount: {t['gcount']} }};",
         "/* every engine number's name as INFO sends it (engines.c ENGINES[]: a retired number plays as another engine,",
         "   ENG_LIVE), the engine each number plays on, and the order the device shows them in (ENGINE_ORDER) */",
         f"const BYTE_ENG_NAMES = {json.dumps(t['names'])};",
         f"const BYTE_ENG_LIVE = {json.dumps(t['live'])};",
         f"const BYTE_ENG_ORDER = {json.dumps(t['order'])};",
         "/* the engines: their EDIT 1 / EDIT 2 titles and descriptors, their factory presets (preset_t: e[8], env, mono (bit 0;",
         "   bits 1..4 the sound type), pat, xp, the MODS as [parameter, value] pairs, fenv, fx: the sends + 1, 0 = default) */",
         "const BYTE_ENG_DEFS = (D, P) => ({"]
    for n, e in t["engines"].items():
        L.append(f"  {n}: {{ name: {json.dumps(e['name'])}, titles: {json.dumps(e['titles'])}, edit: [")
        L += [f"      {js_desc(d)}," for d in e["edit"]]
        L.append("    ], presets: [")
        L += [f"      {js_preset(p)}," for p in e["presets"]]
        L.append("    ] },")
    L.append("});")
    L.append("/* FORM's own descriptors of common parameters (eng_form.c form_common_desc), by parameter id */")
    L.append("const BYTE_FORM_COMMON = (D) => ({")
    L += [f"  {i}: {js_desc(d)}," for i, d in t["form_common"].items()]
    L.append("});")
    L.append("/* FORM's pages (params.c PAGES, scope SC_FORM): title, parameter ids */")
    L.append(f"const BYTE_FORM_PAGES = {json.dumps(t['form_pages'])};")
    L.append("/* GLOBAL's ENG (G_ENGSEL): the engine numbers' names; the parameter ids the mock's preset load sets (core.h);")
    L.append("   the parts' sounds at power-on: engine, preset, pattern (engines.c TRK_DEF) */")
    L.append(f"const BYTE_ENGSEL = {json.dumps(t['engsel']['names'])};")
    L.append(f"const BYTE_IDS = {json.dumps(t['ids'])};")
    L.append(f"const BYTE_TRK_DEF = {json.dumps(t['trk_def'])};")
    L.append(END)
    return "\n".join(L)


def main():
    t = dump()
    if "--json" in sys.argv:
        print(json.dumps(t, indent=1))
        return
    html = HTML.read_text()
    i, j = html.find(BEGIN), html.find(END)
    if i < 0 or j < i:
        sys.exit(f"gen_editor_engines: no {BEGIN} .. {END} in {HTML}")
    new = html[:i] + render(t) + html[j + len(END):]
    if "--check" in sys.argv:
        if new != html:
            sys.exit("gen_editor_engines: web/editor.html is not up to date: run tools/gen_editor_engines.py")
        print("gen_editor_engines: web/editor.html up to date")
        return
    if new != html:
        HTML.write_text(new)
    print(f"gen_editor_engines: {HTML.relative_to(ROOT)}: " +
          ", ".join(f"{e['name']} ({len(e['presets'])} presets)" for e in t["engines"].values()))


if __name__ == "__main__":
    main()
