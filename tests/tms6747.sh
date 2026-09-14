#!/usr/bin/env bash
#
# The fourth target's suite: every case compiled for the TMS320C6747, run on
# the VM6747 emulator beside the runtime cxx1i compiled for it, and compared
# with the output recorded from the app's interpreter - the same claim
# run.sh makes for the host, made for a target no machine here can execute.
#
#   SHC=... VM=... RUNTIME=... ./tests/tms6747.sh [filter]
#
# RUNTIME is the directory of C6000 text `make tms6747` writes, one file per
# runtime source, which the emulator assembles whole. The compiler's own
# diagnostics come first in the recorded output, as in run.sh.

set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

FILTER="${1:-}"
OUT=tests/out-tms6747
mkdir -p "$OUT"

SHC="${SHC:-./shci.exe}"
VM="${VM:-../Emulator/vm6747.exe}"
RUNTIME="${RUNTIME:-lib/shmrt-tms6747}"
[ -x "$SHC" ] || { echo "no $SHC - run make first, or set SHC=" >&2; exit 2; }
[ -x "$VM" ] || { echo "no emulator at $VM - build VM6747/Emulator, or set VM=" >&2; exit 2; }
[ -d "$RUNTIME" ] || { echo "no runtime at $RUNTIME - run 'make tms6747', or set RUNTIME=" >&2; exit 2; }

pass=0
fail=0

for case in tests/cases/*.shm tests/load/*.shm; do
    name=$(basename "$case" .shm)
    [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]] && continue
    expected="${case%.shm}.expected"
    [ -f "$expected" ] || { echo "SKIP $name (nothing recorded)"; continue; }

    "$SHC" --target=tms6747 -S "$case" -o "$OUT/$name.s" > "$OUT/$name.compile" 2>/dev/null
    compiled=$?
    cp "$OUT/$name.compile" "$OUT/$name.actual"
    if [ $compiled -ne 0 ]; then
        if diff -u "$expected" "$OUT/$name.actual" > "$OUT/$name.diff" 2>&1; then
            pass=$((pass+1)); continue
        fi
        echo "FAIL $name (did not compile)"
        sed -n '1,12p' "$OUT/$name.diff"
        fail=$((fail+1)); continue
    fi

    "$VM" "$OUT/$name.s" "$RUNTIME" >> "$OUT/$name.actual" 2>&1 < /dev/null
    if diff -u "$expected" "$OUT/$name.actual" > "$OUT/$name.diff" 2>&1; then
        pass=$((pass+1))
    else
        echo "FAIL $name"
        sed -n '1,12p' "$OUT/$name.diff"
        fail=$((fail+1))
    fi
done

echo
echo "tms6747: $pass passed, $fail failed"
[ $fail -eq 0 ]
