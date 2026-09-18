#!/bin/sh
# TriLab: two programs, each built by RIDE with the project's own compilers and by
# the environment's native toolchain - the judge - and their outputs compared.
#
#   sh trilab.sh mac            RIDE (cc1i, cxx1i, arm64-darwin) against Xcode, on this Mac
#   sh trilab.sh windows        RIDE on the box (x86_64-windows, the project's assembler)
#                               against Visual Studio 2022
#   sh trilab.sh ccs            RIDE on the box (tms6747, run on vm6747) against CCS 7.4's
#                               cl6x, whose assembly vm6747 runs too (CCS has no simulator)
#
# A leg passes when, for each program, RIDE's output and the judge's are the same
# line for line, except lines the ledger (expected/<leg>-<lab>.allowed) records as
# legitimately different, with both readings. Anything else is a defect, named.
#
# Needs: RStudio built (../../RStudio/RStudio.exe, or RIDE=path), cc1i.exe and
# cxx1i.exe built in ../Compiler-Ci and ../Compiler-Cppi, and xcodebuild.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
RIDE=${RIDE:-$HERE/../../RStudio/RStudio.exe}
CC1I=${CC1I:-$HERE/../Compiler-Ci/cc1i.exe}
CXX1I=${CXX1I:-$HERE/../Compiler-Cppi/cxx1i.exe}
OUT=${OUT:-${TMPDIR:-/tmp}/trilab.$$}
mkdir -p "$OUT"
leg=${1:-mac}
status=0

# the two labs: directory, project, RIDE compiler flag, Xcode target
labs="c:CC1Lab:--cc1=$CC1I cpp:CXX1Lab:--cxx1=$CXX1I"

compare() {   # compare <lab> <candidate output> <judge output> <judge name>
    lab=$1; mine=$2; theirs=$3; judge=$4
    allowed="$HERE/expected/$leg-$lab.allowed"
    if diff "$mine" "$theirs" > "$OUT/$lab.diff"; then
        echo "  $lab: RIDE and $judge agree, $(wc -l < "$theirs" | tr -d ' ') lines"
        return
    fi
    if [ -f "$allowed" ] && diff "$OUT/$lab.diff" "$allowed" > /dev/null; then
        echo "  $lab: RIDE and $judge differ only as expected/$leg-$lab.allowed records"
        return
    fi
    echo "  $lab: RIDE and $judge DIFFER, and the ledger does not explain it:"
    sed 's/^/      /' "$OUT/$lab.diff" | head -20
    status=1
}

case $leg in
mac)
    echo "TriLab, macOS: RIDE against Xcode"
    for entry in $labs; do
        d=${entry%%:*}; rest=${entry#*:}; n=${rest%%:*}; flag=${rest#*:}
        # the candidate: RIDE builds and runs the lab's own project file
        ( cd "$HERE/$d" && "$RIDE" "$n.pro" "${flag%%=*}" "${flag#*=}" --arch arm64-darwin --build > "$OUT/$d-ride.build" 2>&1 ) \
            || { echo "  $d: RIDE did not build it:"; tail -5 "$OUT/$d-ride.build"; status=1; continue; }
        prog=$(sed -n 's/^\[built \(.*\)\]$/\1/p' "$OUT/$d-ride.build" | tail -1)
        ( cd "$HERE/$d" && "$prog" > "$OUT/$d-ride.out" 2>&1 ); echo "  $d: RIDE's program returned $?"
        # the judge: Xcode's own project, Apple clang, built out of the tree
        python3 "$HERE/tools/make-xcode.py" "$d" > /dev/null
        xcodebuild -project "$HERE/$d/xcode/$n.xcodeproj" -target "$n" -configuration Release ARCHS=arm64 \
            SYMROOT="$OUT/xcode-$n" OBJROOT="$OUT/xcode-$n/obj" build > "$OUT/$d-xcode.build" 2>&1 \
            || { echo "  $d: Xcode did not build it:"; grep -E "error:" "$OUT/$d-xcode.build" | head -5; status=1; continue; }
        ( cd "$HERE/$d" && "$OUT/xcode-$n/Release/$n" > "$OUT/$d-xcode.out" 2>&1 ); echo "  $d: Xcode's program returned $?"
        compare "$d" "$OUT/$d-ride.out" "$OUT/$d-xcode.out" Xcode
    done
    ;;
windows)
    # The box: `ssh windows`, the RIDE solution built there by RStudio's
    # tools/to-windows.sh (RStudioConsole.exe and the compilers in bin\), and
    # the assembler built by MASM's tests/windows.sh. The lab is shipped as a
    # tar, the .cmd runs both sides, and the outputs come back to be compared.
    BOX=${BOX:-windows}
    BOXLAB=${BOXLAB:-C:/Users/GRA/source/VM6747/TriLab}
    BOXRIDE=${BOXRIDE:-C:/Users/GRA/source/RStudio/bin/RStudioConsole.exe}
    BOXASM=${BOXASM:-C:/masm-tests/build/asm-win.exe}
    W=$(echo "$BOXLAB" | sed 's|/|\\|g')
    echo "TriLab, Windows: RIDE (cc1i, cxx1i, the project's assembler) against Visual Studio 2022"
    python3 "$HERE/tools/make-vs.py" > /dev/null
    find "$HERE" -name "* [0-9].*" -delete
    COPYFILE_DISABLE=1 tar -C "$HERE" --no-xattrs --exclude out --exclude xcode --exclude 'c/cc1lab*' --exclude 'cpp/cxx1lab*' \
        -czf "$OUT/trilab.tgz" c cpp tools expected 2>/dev/null || { echo "  cannot pack the lab"; exit 2; }
    ssh -n -o BatchMode=yes "$BOX" "if not exist $W mkdir $W" > /dev/null || exit 2
    scp -q "$OUT/trilab.tgz" "$BOX:$BOXLAB/trilab.tgz" || exit 2
    ssh -n -o BatchMode=yes "$BOX" "cd /d $W & tar xzf trilab.tgz & $W\\tools\\windows-leg.cmd $W $(echo "$BOXRIDE" | sed 's|/|\\|g') $(echo "$BOXASM" | sed 's|/|\\|g')" \
        | grep -vE "^\s*$" | sed 's/^/  /'
    mkdir -p "$OUT/win" && scp -q "$BOX:$BOXLAB/out/*" "$OUT/win/" || { echo "  no outputs came back"; exit 1; }
    for d in c cpp; do
        for side in ride vs; do
            [ -f "$OUT/win/$d-$side.out" ] || { echo "  $d: no $side output - see $OUT/win/$d-$side.build"; status=1; continue 2; }
            tr -d '\r' < "$OUT/win/$d-$side.out" > "$OUT/$d-$side.out"
        done
        compare "$d" "$OUT/$d-ride.out" "$OUT/$d-vs.out" "Visual Studio"
    done
    ;;
ccs)
    # The box again, with CCS 7.4's compiler (cl6x, lnk6x under C:\ti\ccsv7) and
    # RIDE's vm6747.exe. CCS 7.4 has no simulator, so the emulator runs both
    # sides: RIDE's tms6747 program and the assembly cl6x writes for the same
    # sources. Each side also goes through TI's assembler and linker to a real
    # .out (the leg says so per lab), which is the acceptance half of the judge.
    BOX=${BOX:-windows}
    BOXLAB=${BOXLAB:-C:/Users/GRA/source/VM6747/TriLab}
    BOXRIDE=${BOXRIDE:-C:/Users/GRA/source/RStudio/bin/RStudioConsole.exe}
    BOXVM=${BOXVM:-C:/Users/GRA/source/RStudio/bin/vm6747.exe}
    W=$(echo "$BOXLAB" | sed 's|/|\\|g')
    echo "TriLab, CCS 7.4: RIDE (cc1i, cxx1i, tms6747, run on vm6747) against cl6x, run on vm6747 too"
    find "$HERE" -name "* [0-9].*" -delete
    COPYFILE_DISABLE=1 tar -C "$HERE" --no-xattrs --exclude out --exclude xcode --exclude vs --exclude 'c/cc1lab*' --exclude 'cpp/cxx1lab*' \
        -czf "$OUT/trilab.tgz" c cpp tools expected 2>/dev/null || { echo "  cannot pack the lab"; exit 2; }
    ssh -n -o BatchMode=yes "$BOX" "if not exist $W mkdir $W" > /dev/null || exit 2
    scp -q "$OUT/trilab.tgz" "$BOX:$BOXLAB/trilab.tgz" || exit 2
    ssh -n -o BatchMode=yes "$BOX" "cd /d $W & tar xzf trilab.tgz & $W\\tools\\ccs-leg.cmd $W $(echo "$BOXRIDE" | sed 's|/|\\|g') $(echo "$BOXVM" | sed 's|/|\\|g')" \
        | grep -vE "^\s*$" | sed 's/^/  /'
    mkdir -p "$OUT/win" && scp -q "$BOX:$BOXLAB/out/*.out" "$OUT/win/" || { echo "  no outputs came back"; exit 1; }
    for d in c cpp; do
        for side in ride-ccs ccs; do
            [ -f "$OUT/win/$d-$side.out" ] || { echo "  $d: no $side output - see the box's out\\$d-$side.build"; status=1; continue 2; }
            tr -d '\r' < "$OUT/win/$d-$side.out" > "$OUT/$d-$side.out"
        done
        if [ "$d" = cpp ]; then
            # cl6x's C++ program links, but its iostreams are STLport's, whose
            # locale and num_put live in TI's compiled runtime - machine code
            # the emulator does not read. So the judge's own run is not
            # available for C++; the judge is TI accepting and linking both
            # sides (the box says TI-LINKED for each), and the output is held to
            # Xcode's native run of the same sources, built here.
            xcodebuild -project "$HERE/cpp/xcode/CXX1Lab.xcodeproj" -target CXX1Lab -configuration Release ARCHS=arm64 \
                SYMROOT="$OUT/xcode-CXX1Lab" OBJROOT="$OUT/xcode-CXX1Lab/obj" build > "$OUT/cpp-xcode.build" 2>&1 \
                || { echo "  cpp: Xcode did not build the native reference"; status=1; continue; }
            ( cd "$HERE/cpp" && "$OUT/xcode-CXX1Lab/Release/CXX1Lab" > "$OUT/cpp-xcode.out" 2>&1 )
            echo "  cpp: cl6x's own program cannot run on vm6747 (STLport's runtime); the judge is TI linking both sides"
            compare cpp "$OUT/cpp-ride-ccs.out" "$OUT/cpp-xcode.out" "Xcode's native run"
        else
            compare "$d" "$OUT/$d-ride-ccs.out" "$OUT/$d-ccs.out" "cl6x on vm6747"
        fi
    done
    ;;
*)
    echo "usage: trilab.sh mac|windows|ccs"; status=2 ;;
esac
[ $status = 0 ] && echo "TriLab $leg: PASS" || echo "TriLab $leg: FAIL"
exit $status
