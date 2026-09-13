#!/bin/sh
# The tms6747 backend, run the same way the arm64 one is: compile each case
# twice, once with cc1i for the C6000 and once with the host's compiler, run
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
CC1="${CC1:-$ROOT/cc1i.exe}"
VM="${VM:-$ROOT/../Emulator/vm6747.exe}"
SRC="$ROOT/tests/cases"
OUT="$ROOT/tests/out-tms6747"

if [ ! -x "$VM" ]; then
    echo "tms6747.sh: no emulator at $VM - build VM6747/Emulator first"
    exit 1
fi
if command -v clang >/dev/null 2>&1; then HOST=clang; else HOST=gcc; fi

rm -rf "$OUT" && mkdir -p "$OUT"
pass=0
fail=0
skip=0
only="${1:-}"
# The cases whose reference needs a 64-bit long or pointer - see the file.
LP64="$ROOT/tests/tms6747-lp64.txt"

for src in "$SRC"/*.c; do
    name=$(basename "$src" .c)
    [ -n "$only" ] && [ "$name" != "$only" ] && continue
    if [ -z "$only" ] && grep -q "^$name[[:space:]]" "$LP64"; then
        skip=$((skip + 1))
        continue
    fi
    expect=$(sed -n 's|^// expect: *||p' "$src" | head -1)

    if ! "$CC1" -S -arch tms6747 "$src" -o "$OUT/$name.s" 2> "$OUT/$name.cc1.err"; then
        echo "FAIL $name - cc1i refused it:"
        sed 's/^/       /' "$OUT/$name.cc1.err" | head -3
        fail=$((fail + 1))
        continue
    fi
    $HOST -w "$src" -o "$OUT/$name.ref" -lm 2> /dev/null

    ours_out=$("$VM" "$OUT/$name.s" 2>&1 < /dev/null); ours_rc=$?
    ref_out=$("$OUT/$name.ref" 2>&1 < /dev/null);  ref_rc=$?
    # glibc's printf spells a null %p "(nil)" and a NaN with its sign bit set
    # "-nan"; clang's libc and the emulator, which prints canonically, say
    # "0x0" and "nan". The reference is brought to the emulator's spelling
    # rather than the other way round, because the emulator's is the one that
    # is the same on every machine - and it is the reference's libc talking,
    # not the program under test. Found on the Linux box, 2 of 425.
    ref_out=$(printf '%s' "$ref_out" | sed 's/(nil)/0x0/g; s/-nan/nan/g')

    if [ "$ours_out" != "$ref_out" ] || [ "$ours_rc" != "$ref_rc" ]; then
        echo "FAIL $name - disagrees with $HOST"
        echo "       ours: rc=$ours_rc out=[$(printf '%s' "$ours_out" | head -c 300)]"
        echo "       ref : rc=$ref_rc out=[$(printf '%s' "$ref_out" | head -c 300)]"
        fail=$((fail + 1))
        continue
    fi
    if [ -n "$expect" ] && [ "$ours_rc" != "$expect" ]; then
        echo "FAIL $name - both agree on $ours_rc, but the case expects $expect"
        fail=$((fail + 1))
        continue
    fi
    pass=$((pass + 1))
done

echo
echo "tms6747  PASS: $pass   FAIL: $fail   SKIP: $skip (need a 64-bit long or pointer - tests/tms6747-lp64.txt)"
[ "$fail" -eq 0 ]
