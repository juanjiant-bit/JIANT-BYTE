#!/bin/sh
# Requires generated headers (run tests/run_byte_tests.sh first).
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/life-v08-after
"${CC:-cc}" -std=gnu99 -O2 -ffp-contract=off -w -Wno-error=implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests tests/life_song_test.c -lm -o build/life-song-after
for genre in 'MINIMAL TECHNO' FUGA AMBIENT; do
    for amount in 0 25 50 75 100; do
        build/life-song-after "$genre" "$amount" "build/life-v08-after/$genre-$amount.i32" > "build/life-v08-after/$genre-$amount.log"
    done
done
python3 tests/life_song_sound.py
