#!/bin/sh
# The tms6747 target through asm6x, the project's own C6000 assembler: every
# case tests/tms6747.sh compiles is taken to a TI object by the driver itself
# (`cc1i -arch tms6747 -c`, which runs asm6x on what it wrote), and that
# object must be the one asm6x writes for the same assembly by hand - every
# table of it, the file symbol aside, which names the temporary. What this
# checks is the driver's path to the assembler, on any host; that asm6x's
# objects are TI's is ASM6x's own suite, and that TI links them is
# tests/windows/ti-link.cmd on the box, where lnk6x is (tests/ti-link.sh runs it).
#
#   ASM6X=<asm6x> sh tests/asm6x.sh        the assembler: $ASM6X, else ASM6x's
#                                          build beside this tree, else one on PATH
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="${CC1:-$ROOT/cc1i.exe}"
ASM6X="${ASM6X:-}"
if [ -z "$ASM6X" ]; then
    for c in "$ROOT/../../ASM6x/build/asm6x.exe" "$HOME/asm6x-build/asm6x.exe" "$(command -v asm6x 2>/dev/null)"; do
        [ -n "$c" ] && [ -x "$c" ] && { ASM6X=$c; break; }
    done
fi
[ -n "$ASM6X" ] || { echo "asm6x.sh: no asm6x - name it with ASM6X=<path>"; exit 2; }
C6XDIFF="${C6XDIFF:-$ROOT/../../ASM6x/tests/c6xdiff.py}"
[ -f "$C6XDIFF" ] || C6XDIFF="$HOME/asm6x-build/tests/c6xdiff.py"
[ -f "$C6XDIFF" ] || { echo "asm6x.sh: no c6xdiff.py - name it with C6XDIFF=<path>"; exit 2; }
OUT="$ROOT/tests/out-asm6x"
rm -rf "$OUT" && mkdir -p "$OUT"

LP64="$ROOT/tests/tms6747-lp64.txt"
pass=0; fail=0; skip=0
for src in "$ROOT"/tests/cases/*.c; do
    name=$(basename "$src" .c)
    case "$name" in *" "*) continue;; esac
    # the cases written for a 64-bit long, which tests/tms6747.sh leaves out too
    if grep -q "^$name[[:space:]]" "$LP64"; then skip=$((skip + 1)); continue; fi
    if ! ( ulimit -t 20; CC1_AS="$ASM6X" "$CC1" -arch tms6747 -c "$src" -o "$OUT/$name.obj" < /dev/null ) 2> "$OUT/$name.err"; then
        echo "FAIL $name - the driver did not make an object:"
        sed 's/^/       /' "$OUT/$name.err" | head -3
        fail=$((fail + 1)); continue
    fi
    "$CC1" -S -arch tms6747 "$src" -o "$OUT/$name.s" 2> /dev/null
    if ! "$ASM6X" "$OUT/$name.s" -o "$OUT/$name.hand.obj" > "$OUT/$name.hand.err" 2>&1; then
        echo "FAIL $name - asm6x refused the assembly by hand:"
        sed 's/^/       /' "$OUT/$name.hand.err" | head -3
        fail=$((fail + 1)); continue
    fi
    if python3 "$C6XDIFF" "$OUT/$name.obj" "$OUT/$name.hand.obj" > "$OUT/$name.diff"; then
        pass=$((pass + 1))
    else
        echo "FAIL $name - the driver's object is not asm6x's:"
        sed 's/^/       /' "$OUT/$name.diff" | head -6
        fail=$((fail + 1))
    fi
done
echo "asm6x.sh: $pass objects through the driver as by hand, $fail failed, $skip written for a 64-bit long"
[ "$fail" -eq 0 ]
