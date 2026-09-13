#!/bin/sh
# The Windows leg: relay the emulator's sources to the Windows box, build
# them there with cl (msvc/build.cmd), run every .s the two compilers' suites
# left in their tests/out-tms6747 directories on that binary, and require
# the same output and exit status as this machine's binary gives for the
# same assembly. The .s files are the Mac's, so what this checks is the
# emulator's portability, not the compilers'; the compilers' own Windows
# legs are their business.
#
# The box is `ssh windows` (see the windows-test-machine note); its shell
# is cmd, so build.cmd is run by its full path with no "cmd /c" in front,
# and the loop over the cases runs in Git bash there. MSVC writes CRLF on
# stdout, so the Windows output is normalised before the comparison.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
WORK=${TMPDIR:-/tmp}/vm6747-windows.$$
REMOTE='C:/Users/GRA/Documents/VM6747'
mkdir -p "$WORK/xwin/c" "$WORK/xwin/cxx"
# Only real case names: macOS leaves "name 2.s" duplicates beside the files
# it rewrites, and those are noise, not cases.
for f in "$HERE"/../Compiler-Ci/tests/out-tms6747/*.s; do case "$(basename "$f")" in *" "*) ;; *) cp "$f" "$WORK/xwin/c/";; esac; done
for f in "$HERE"/../Compiler-Cppi/tests/out-tms6747/*.s; do case "$(basename "$f")" in *" "*) ;; *) cp "$f" "$WORK/xwin/cxx/";; esac; done
cat > "$WORK/xwin/run.sh" <<'REMOTE_EOF'
cd "$(dirname "$0")"
for d in c cxx; do for s in $d/*.s; do b=${s%.s}; ../Emulator/vm6747.exe "$s" > "$b.out" 2>&1 < /dev/null; echo $? > "$b.rc"; done; done
REMOTE_EOF
( cd "$HERE/.." && tar czf "$WORK/emulator.tgz" --exclude='*.exe' --exclude='.git' --exclude='._*' Emulator )
( cd "$WORK" && tar czf xwin.tgz xwin )
ssh windows "if not exist C:\\Users\\GRA\\Documents\\VM6747 mkdir C:\\Users\\GRA\\Documents\\VM6747" || exit 1
scp -q "$WORK/emulator.tgz" "$WORK/xwin.tgz" "windows:$REMOTE/" || exit 1
ssh windows "cd C:\\Users\\GRA\\Documents\\VM6747 && tar xzf emulator.tgz && tar xzf xwin.tgz && del /q Emulator\\._* Emulator\\src\\._* 2>nul & C:\\Users\\GRA\\Documents\\VM6747\\Emulator\\msvc\\build.cmd" || { echo "windows.sh: the build on the box failed"; exit 1; }
ssh windows "\"C:\\Program Files\\Git\\bin\\bash.exe\" -c \"cd /c/Users/GRA/Documents/VM6747/xwin && sh run.sh\"" || exit 1
ssh windows "cd C:\\Users\\GRA\\Documents\\VM6747 && tar czf xwin-out.tgz xwin" && scp -q "windows:$REMOTE/xwin-out.tgz" "$WORK/" || exit 1
mkdir -p "$WORK/win" && tar xzf "$WORK/xwin-out.tgz" -C "$WORK/win"
same=0; differ=0
for d in c cxx; do
    for s in "$WORK"/xwin/$d/*.s; do
        [ -f "$s" ] || continue
        b=$(basename "$s" .s)
        "$HERE/vm6747.exe" "$s" > "$WORK/xwin/$d/$b.mac" 2>&1 < /dev/null; mrc=$?
        wrc=$(cat "$WORK/win/xwin/$d/$b.rc")
        tr -d '\r' < "$WORK/win/xwin/$d/$b.out" > "$WORK/win/xwin/$d/$b.lf"
        if [ "$mrc" = "$wrc" ] && cmp -s "$WORK/xwin/$d/$b.mac" "$WORK/win/xwin/$d/$b.lf"; then same=$((same + 1))
        else differ=$((differ + 1)); echo "DIFF $d/$b: rc mac=$mrc windows=$wrc"; fi
    done
done
echo "windows.sh: $same identical, $differ differ (the Windows-built emulator against this one, same assembly)"
rm -rf "$WORK"
[ "$differ" -eq 0 ]
