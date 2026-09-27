#!/bin/sh
# The arm64-darwin backend, run the same way the x86-64 one is: compile each
# case twice, once with cc1 and once with the reference compiler, run both, and
# require that they agree on the printed output and the exit status.
#
# This runs on the Mac rather than the box, because the Mac is arm64 and can
# execute what this backend emits. clang is the reference here, where gcc is the
# reference on Linux - a different compiler, but the same argument: it is the
# implementation sitting on the same disk, and where the two disagree the case
# is wrong until the standard says otherwise.
#
# It is a subset. The backend still refuses structs, member access, calls
# through a function pointer and va_start by name, so tests/cases is not the
# corpus here - tests/arm64/ is, and it grows as the backend does. Floating
# point, postfix and switch have since landed and have cases below.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
# Overridable, because a workspace build puts every binary in one directory
# rather than in each repository's own root. Without this these suites test
# whichever cc1.exe happens to be sitting here, which is not necessarily the
# one that was just built - and after a clean, is not there at all.
CC1="${CC1:-$ROOT/cc1.exe}"
SRC="$ROOT/tests/arm64"
OUT="$ROOT/tests/out-arm64"

if [ "$(uname -m)-$(uname -s)" != "arm64-Darwin" ]; then
    echo "arm64.sh: this must run on an arm64 Mac; it is $(uname -m)-$(uname -s)"
    exit 1
fi

# One case, its report and verdict beside its output: c90 and the assembler on one side, clang's
# own build of the source on the other - the program under test and the reference at once - and
# the cases JOBS at a time.
one() {
    name=$1; src="$SRC/$name.c"
    expect=$(sed -n 's|^// expect: *||p' "$src" | head -1)
    ( clang -w "$src" -o "$OUT/$name.ref" -lm 2> /dev/null ) &
    if ! "$CC1" -S -arch arm64-darwin "$src" -o "$OUT/$name.s" 2> "$OUT/$name.cc1.err"; then
        wait; echo "FAIL $name - cc1 refused it:"
        sed 's/^/       /' "$OUT/$name.cc1.err"
        echo fail > "$OUT/$name.verdict"; return
    fi
    if ! clang "$OUT/$name.s" -o "$OUT/$name.ours" -lm 2> "$OUT/$name.as.err"; then
        wait; echo "FAIL $name - the assembler refused what cc1 emitted:"
        sed 's/^/       /' "$OUT/$name.as.err" | head -5
        echo fail > "$OUT/$name.verdict"; return
    fi
    wait
    ( "$OUT/$name.ours" > "$OUT/$name.ours.out" 2>&1 < /dev/null; echo $? > "$OUT/$name.ours.rc" ) &
    ( "$OUT/$name.ref" > "$OUT/$name.ref.out" 2>&1 < /dev/null; echo $? > "$OUT/$name.ref.rc" ) &
    wait
    ours_out=$(cat "$OUT/$name.ours.out"); ours_rc=$(cat "$OUT/$name.ours.rc")
    ref_out=$(cat "$OUT/$name.ref.out"); ref_rc=$(cat "$OUT/$name.ref.rc")

    if [ "$ours_out" != "$ref_out" ] || [ "$ours_rc" != "$ref_rc" ]; then
        echo "FAIL $name - disagrees with clang"
        echo "       ours: rc=$ours_rc out=[$ours_out]"
        echo "       ref : rc=$ref_rc out=[$ref_out]"
        echo fail > "$OUT/$name.verdict"; return
    fi
    if [ -n "$expect" ] && [ "$ours_rc" != "$expect" ]; then
        echo "FAIL $name - both agree on $ours_rc, but the case expects $expect"
        echo fail > "$OUT/$name.verdict"; return
    fi
    echo pass > "$OUT/$name.verdict"
}
if [ "${1:-}" = --one ]; then one "$2" > "$OUT/$2.report" 2>&1; exit 0; fi

rm -rf "$OUT" && mkdir -p "$OUT"
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
for src in "$SRC"/*.c; do basename "$src" .c; done | xargs -P "$JOBS" -n 1 sh "$ROOT/tests/arm64.sh" --one
for src in "$SRC"/*.c; do cat "$OUT/$(basename "$src" .c).report"; done
pass=$(cat "$OUT"/*.verdict 2>/dev/null | grep -cx pass || true)
fail=$(cat "$OUT"/*.verdict 2>/dev/null | grep -cx fail || true)

echo
echo "arm64-darwin  PASS: $pass   FAIL: $fail"
[ "$fail" -eq 0 ]
