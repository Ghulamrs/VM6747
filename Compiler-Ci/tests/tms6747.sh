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
#
# A second leg runs each case as a TI program - asm6x, then lnk6x against TI's runtime - on
# vm6747sim, which runs the machine code, and holds it to the same reference. It sees what the
# emulator cannot: the assembler's encoding and TI's own runtime. SIM=0 leaves it out, said aloud;
# tests/tms6747-sim.txt names the cases it cannot run, why beside.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC1="${CC1:-$ROOT/c90.exe}"
VM="${VM:-$ROOT/../Emulator/vm6747.exe}"
SRC="$ROOT/tests/cases"
# The second leg's tools: each named, else where the sibling checkouts keep it, else on PATH.
SIM="${SIM:-1}"
find_tool() {
    for t in "$@"; do [ -n "$t" ] && [ -x "$t" ] && { echo "$t"; return; }; done
}
ASM6X=$(find_tool "${ASM6X:-}" "$ROOT/../../ASM6x/build/asm6x.exe" "$(command -v asm6x 2>/dev/null)")
LNK6X=$(find_tool "${LNK6X:-}" "$ROOT/../../LNK6x/build/lnk6x.exe" "$(command -v lnk6x 2>/dev/null)")
VMSIM=$(find_tool "${VMSIM:-}" "$ROOT/../../VM6747-sim/vm6747.exe" "$(command -v vm6747sim 2>/dev/null)")
# TI's runtime is TI's and not in any repository: the exception-handling build of rts6740.
TIRTS="${TIRTS:-${C6747_EHLIB:-$HOME/c6747-lib}}"
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
    if [ "$SIM" = 1 ] && ! grep -q "^$name[[:space:]]" "$ROOT/tests/tms6747-sim.txt"; then
        if ! { "$ASM6X" "$OUT/$name.s" -o "$OUT/$name.obj" &&
               "$LNK6X" -mv6740 --abi=eabi -i "$TIRTS" "$OUT/link.cmd" "$OUT/$name.obj" \
                   -l rts6740_elf_eh.lib -o "$OUT/$name.ti.out"; } > "$OUT/$name.ti.log" 2>&1 < /dev/null; then
            echo "FAIL $name (vm6747sim) - asm6x or lnk6x refused it:"
            sed 's/^/       /' "$OUT/$name.ti.log" | head -3
            echo fail > "$OUT/$name.verdict"; return
        fi
        { ( ulimit -t 20; "$VMSIM" --run --main-status "$OUT/$name.ti.out" ) > "$OUT/$name.sim" 2>&1 < /dev/null; echo $? > "$OUT/$name.sim.rc"; } 2>/dev/null
        sim_out=$(cat "$OUT/$name.sim"); sim_rc=$(cat "$OUT/$name.sim.rc")
        if [ "$sim_out" != "$ref_out" ] || [ "$sim_rc" != "$ref_rc" ]; then
            echo "FAIL $name (vm6747sim) - disagrees with $HOST"
            echo "       sim: rc=$sim_rc out=[$(printf '%s' "$sim_out" | head -c 300)]"
            echo "       ref: rc=$ref_rc out=[$(printf '%s' "$ref_out" | head -c 300)]"
            echo fail > "$OUT/$name.verdict"; return
        fi
    fi
    echo pass > "$OUT/$name.verdict"
}
if [ "${1:-}" = --one ]; then one "$3" > "$OUT/$3.report" 2>&1; exit 0; fi

rm -rf "$OUT" && mkdir -p "$OUT"
if [ "$SIM" = 1 ]; then
    for need in "asm6x:$ASM6X" "lnk6x:$LNK6X" "vm6747sim:$VMSIM"; do
        [ -n "${need#*:}" ] || { echo "tms6747.sh: no ${need%%:*} - build it, name it, or SIM=0 to leave the vm6747sim leg out"; exit 1; }
    done
    [ -f "$TIRTS/rts6740_elf_eh.lib" ] || { echo "tms6747.sh: no rts6740_elf_eh.lib in $TIRTS - set TIRTS, or SIM=0"; exit 1; }
    # RIDE's flat map, with room for a case's stack.
    cat > "$OUT/link.cmd" <<'MAP'
--rom_model
--stack_size=0x100000
--heap_size=0x100000
MEMORY { RAM : origin = 0xC0000000, length = 0x04000000 }
SECTIONS
{
    .text > RAM  .const > RAM  .data > RAM  .bss > RAM  .far > RAM  .fardata > RAM
    .neardata > RAM  .rodata > RAM  .cinit > RAM  .init_array > RAM  .switch > RAM
    .cio > RAM  .stack > RAM  .sysmem > RAM  .vm6747.eh > RAM
}
MAP
else
    echo "tms6747.sh: SIM=0 - the vm6747sim leg is left out; only the assembly is run, on vm6747"
fi
cases() { for src in "$SRC"/*.c; do n=$(basename "$src" .c); [ -n "$only" ] && [ "$n" != "$only" ] && continue; echo "$n"; done; }
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
cases | xargs -P "$JOBS" -I{} sh "$0" --one "$only" {}
for n in $(cases); do cat "$OUT/$n.report"; done
count() { cat "$OUT"/*.verdict 2>/dev/null | grep -cx "$1" || true; }
pass=$(count pass); fail=$(count fail); skip=$(count skip)

echo
echo "tms6747  PASS: $pass   FAIL: $fail   SKIP: $skip (need a 64-bit long or pointer - tests/tms6747-lp64.txt)"
[ "$SIM" = 1 ] && echo "tms6747  every PASS also on vm6747sim (asm6x, lnk6x, TI's rts6740) but those in tests/tms6747-sim.txt"
[ "$fail" -eq 0 ]
