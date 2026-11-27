#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# BYTE's host checks, independent of the JieLi SDK and retired-engine tests.
set -eu
cd "$(dirname "$0")/.."
OUT=build/byte-check
mkdir -p "$OUT"
python3 -c 'import tools.build as b; b.generate()' > "$OUT/generate.log"
for test in fvm harmony funcbeat funcbeat_ui func_music funcsong funcengine funcmotion funcsmooth funcbank byte wave waveframes life_byte life_sound life_extreme byte_generation song_generator fuga drumw hive voice_limit euclid_mel byte_myloop byte_controls byte_automation form clay_engines image nav punchseq punchfx modnames uipush chorus_mix dist_noise gain key_trk drift resync relief c1stack gen_radio part_state arp_grid gen_port cpu3 visgen; do
    echo "BYTE: $test"
    # ui_test.c still contains unused helpers for retired FM engines. GCC 14+
    # diagnoses their declarations; they are eliminated from these host checks.
    "${CC:-cc}" -std=gnu99 -O2 -w -Wno-error=implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests \
        -o "$OUT/$test" "tests/${test}_test.c" -lm
    if ! FVM_NO_COST=1 "$OUT/$test" > "$OUT/$test.log" 2>&1; then
        cat "$OUT/$test.log"
        exit 1
    fi
    tail -n 1 "$OUT/$test.log"
    if [ "$test" = form ] && ! FVM_NO_COST=1 "$OUT/$test" checks > "$OUT/form-checks.log" 2>&1; then
        cat "$OUT/form-checks.log"; exit 1
    fi
done
mkdir -p "$OUT/fx_demo"                     # (byte-cpu-light) REVERB TYPE: ROOM and CLOUD at half the rate, as their reference
"${CC:-cc}" -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb" tests/reverb_test.c -lm
"$OUT/reverb" "$OUT/fx_demo" > "$OUT/reverb.log" 2>&1 || { cat "$OUT/reverb.log"; exit 1; }
echo "BYTE: reverb ok"
if command -v valgrind > /dev/null 2>&1; then   # (cpu3) the owner's project: each core's block as the FM-1's time
    python3 tests/cpu3_test.py "$OUT/cpu3"
    python3 tests/cpu_lines_test.py            # (devcpu2) the lines model: the GEN song's worst block, a core each
else
    echo "BYTE: cpu3 timing skipped (no valgrind)"
fi
python3 tests/life_sound_test.py
python3 tests/life_extreme_test.py
python3 tests/funcsong_test.py
python3 tests/font_pack_test.py
python3 tests/text_spacing_test.py build/gen/ui_fonts.h
echo "BYTE: all host checks passed (logs in $OUT)"
