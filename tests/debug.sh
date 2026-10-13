#!/bin/sh
# tests/debug.sh - the --debug session (src/Debug.h) over tests/debug/step.s, cpp11's -g assembly of
# step.cpp: every command RIDE's Debug session sends gdb, and the transcript gdb's shape of the answers.
# step.s is kept rather than rebuilt, so the test needs no compiler; regenerate it with
#   cpp11 -S -arch tms6747 -g tests/debug/step.cpp -o tests/debug/step.s   (then s/step.cpp/step.cpp/)
set -u
D=$(cd "$(dirname "$0")" && pwd)
VM=${VM:-$D/../vm6747.exe}
OUT=$D/out-debug; mkdir -p "$OUT"
cd "$D/debug" || exit 2
"$VM" --debug step.s < step.commands > "$OUT/step.got" 2>&1; st=$?
echo "exit $st" >> "$OUT/step.got"
if diff "$D/debug/step.expected" "$OUT/step.got" > "$OUT/step.diff"; then echo "debug: 1/1"; exit 0; fi
echo "debug: FAILED - $OUT/step.diff"; head -20 "$OUT/step.diff"; exit 1
