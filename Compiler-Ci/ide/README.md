# IDE projects for cc1

The same layout as Compiler-Cppi's `ide/`, file for file: two projects that
build the whole compiler, every path in them `..` and one more step. Nothing in
the build depends on them - `make` and the suites are unchanged.

    cc1.xcworkspace / cc1.xcodeproj    Xcode, on macOS
    cc1.sln / cc1.vcxproj              Visual Studio 2022, on Windows
    cc1.vcxproj.filters                the folder tree VS shows
    generate.py                        writes all five from the source tree
    build-vs.cmd, check-vs.cmd         MSBuild and a smoke test, from a shell

**Written, not kept by hand.** Until 2026-09-30 the Visual Studio project was
`msvc/cc1.vcxproj`, kept by hand, and the Xcode one was RIDE's, written into
the root of this tree by RIDE's `tools/make-projects.py`. `generate.py` writes
both now from `src/` and `src/backend/`, which is what the Makefile's SRCS
globs, and carries the old project's flags: `/std:c++14`, `/W4 /WX` in both
configurations, warnings 4996, 4267, 4244, 4456 and 4146 off, the static
runtime, and `msvc\compat` on the include path for `<unistd.h>`.
`CC1_INCLUDE_DIR` is compiled in from `..\lib`, as the Makefile's INCDIR is.

**RIDE builds these very projects.** `RIDE.sln` and `RIDE.xcworkspace` open
them: the program is `c90.exe` - the name `make` gives it - the Xcode target and
product carry the ids RIDE derives from that name, and in any solution but its
own Visual Studio writes `c90.exe` beside that solution's other programs.

## Building

    ./generate.py                                      after adding or removing a source
    ./generate.py --check                              says whether they are current
    xcodebuild -project cc1.xcodeproj -scheme cc1 -configuration Release build
                                                       -> $TMPDIR/ride-xcode/Release/c90.exe
    ide\build-vs.cmd [Debug]                           -> ide\build\Release\c90.exe
    ide\check-vs.cmd                                   compiles tests\cases\out_hello.c and runs it

`tools/cc1-as-cl.bat` finds the compiler at `ide\build\Release\c90.exe`, and
`examples/cc1-in-visual-studio/` is how to make Visual Studio compile C with it.
