#!/bin/sh
# The tms6747 backend, run the same way the arm64 one is: compile each case
# twice, once with c90 for the C6000 and once with the host's compiler, run
# both - the C6000 one on vm6747, the VM6747 emulator - and require that they
# agree on the printed output and the exit status.
#
# There is no TI toolchain on any of these machines, so the emulator is the
# only thing that runs this backend's output; it assembles the text itself
# and models the pipeline's delay slots, which is how a missing NOP shows up
# as a wrong answer here rather than as a suspicion in a code review. The
# corpus is tests/cases, whole: everything the backend refuses is a FAIL.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="${CC1:-$ROOT/c90.exe}"
VM="${VM:-$ROOT/../Emulator/vm6747.exe}"
SRC="$ROOT/tests/cases"
OUT="$ROOT/tests/out-tms6747"

if [ ! -x "$VM" ]; then
    echo "tms6747.sh: no emulator at $VM - build VM6747/Emulator first"
    exit 1
fi
if command -v clang >/dev/null 2>&1; then HOST=clang; else HOST=gcc; fi

# One case by name, or all of them; a worker is handed the name it was asked for after --one.
if [ "${1:-}" = --one ]; then only=$2; else only="${1:-}"; fi
# The cases whose reference needs a 64-bit long or pointer - see the file.
LP64="$ROOT/tests/tms6747-lp64.txt"

# One case, its report and verdict written beside its output. c90 and the emulator on one side,
# the host compiler and its native run on the other - the program under test and the reference
# at once - and the cases themselves JOBS at a time.
one() {
    name=$1; src="$SRC/$name.c"
    if [ -z "$only" ] && grep -q "^$name[[:space:]]" "$LP64"; then echo skip > "$OUT/$name.verdict"; return; fi
    expect=$(sed -n 's|^// expect: *||p' "$src" | head -1)
    ( if "$CC1" -S -arch tms6747 "$src" -o "$OUT/$name.s" 2> "$OUT/$name.cc1.err"; then
          "$VM" "$OUT/$name.s" > "$OUT/$name.ours" 2>&1 < /dev/null; echo $? > "$OUT/$name.ours.rc"
      else echo refused > "$OUT/$name.ours.rc"; fi ) &
    ( $HOST -w "$src" -o "$OUT/$name.ref" -lm 2> /dev/null
      "$OUT/$name.ref" > "$OUT/$name.theirs" 2>&1 < /dev/null; echo $? > "$OUT/$name.theirs.rc" ) &
    wait
    if [ "$(cat "$OUT/$name.ours.rc")" = refused ]; then
        echo "FAIL $name - c90 refused it:"
        sed 's/^/       /' "$OUT/$name.cc1.err" | head -3
        echo fail > "$OUT/$name.verdict"; return
    fi
    ours_out=$(cat "$OUT/$name.ours"); ours_rc=$(cat "$OUT/$name.ours.rc")
    ref_out=$(cat "$OUT/$name.theirs"); ref_rc=$(cat "$OUT/$name.theirs.rc")
    ref_out=$(printf '%s' "$ref_out" | sed 's/(nil)/0x0/g; s/-nan/nan/g')

    if [ "$ours_out" != "$ref_out" ] || [ "$ours_rc" != "$ref_rc" ]; then
        echo "FAIL $name - disagrees with $HOST"
        echo "       ours: rc=$ours_rc out=[$(printf '%s' "$ours_out" | head -c 300)]"
        echo "       ref : rc=$ref_rc out=[$(printf '%s' "$ref_out" | head -c 300)]"
        echo fail > "$OUT/$name.verdict"; return
    fi
    if [ -n "$expect" ] && [ "$ours_rc" != "$expect" ]; then
        echo "FAIL $name - both agree on $ours_rc, but the case expects $expect"
        echo fail > "$OUT/$name.verdict"; return
    fi
    echo pass > "$OUT/$name.verdict"
}
if [ "${1:-}" = --one ]; then one "$3" > "$OUT/$3.report" 2>&1; exit 0; fi

rm -rf "$OUT" && mkdir -p "$OUT"
cases() { for src in "$SRC"/*.c; do n=$(basename "$src" .c); [ -n "$only" ] && [ "$n" != "$only" ] && continue; echo "$n"; done; }
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
cases | xargs -P "$JOBS" -I{} sh "$0" --one "$only" {}
for n in $(cases); do cat "$OUT/$n.report"; done
count() { cat "$OUT"/*.verdict 2>/dev/null | grep -cx "$1" || true; }
pass=$(count pass); fail=$(count fail); skip=$(count skip)

echo
echo "tms6747  PASS: $pass   FAIL: $fail   SKIP: $skip (need a 64-bit long or pointer - tests/tms6747-lp64.txt)"
[ "$fail" -eq 0 ]
