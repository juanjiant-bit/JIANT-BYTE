#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# BYTE's host checks, independent of the JieLi SDK and retired-engine tests.
set -eu
cd "$(dirname "$0")/.."
OUT=build/byte-check
mkdir -p "$OUT"
python3 -c 'import tools.build as b; b.generate()' > "$OUT/generate.log"
for test in fvm funcbeat funcbeat_ui func_music funcsong byte wave life_byte life_sound life_extreme byte_generation song_generator fuga dxform voice_limit byte_myloop byte_controls byte_automation form form_life; do
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
done
"$OUT/form" checks
"$OUT/form" probes
python3 tests/form_audio_test.py
python3 tests/life_sound_test.py
python3 tests/life_extreme_test.py
python3 tests/funcsong_test.py
python3 tests/font_pack_test.py
python3 tests/text_spacing_test.py build/gen/ui_fonts.h
echo "BYTE: all host checks passed (logs in $OUT)"
