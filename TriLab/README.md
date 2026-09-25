# TriLab

Two programs, one in C and one in C++, each built by **RIDE 3.5 with the project's own
compilers** and by **the environment's native toolchain**, and the two outputs compared.
The native toolchain is the judge; RIDE is what is judged. A leg passes when, for each
program, the two print the same lines - or differ only in lines the ledger records as
legitimately different, with both readings beside each other.

| Environment | The candidate | The judge |
|---|---|---|
| macOS | RIDE on the Mac, `arm64-darwin`, c90 / cpp11 | Xcode: the same sources in a native project, Apple clang |
| Windows, first | RIDE on the box, `x86_64-windows`, c90 / cpp11 / shalimar with the project's assembler | Visual Studio 2022: a native `.vcxproj`, `cl.exe`; for Shalimar, `ml64` and `link` over shalimar's assembly |
| Windows, next | RIDE on the box, `tms6747`, c90 / cpp11, run on vm6747 | CCS 7.4: `cl6x` / `lnk6x`; its C runs on vm6747 too (CCS has no simulator), its C++ links but cannot run there |

## The three programs

`c/` is the C lab, **CC1Lab**: `main.c` and ten examples, one per area of the language, plus
`heavy.c` - a copy of `Compiler-Ci/examples`, frozen here so that the lab's program is its own.
`cpp/` is the C++ lab, **CXX1Lab**: `demo.cpp`, `shapes.cpp`, `report.cpp` - a class hierarchy
with virtual functions and `<iostream>`, `<map>`, `<string>`, `<vector>`, a copy of
`Compiler-Cppi/examples`. Each directory holds one source tree and every project that builds
it; nothing is written twice for an environment, and where an environment must differ the
difference is inside the shipped header or the compiler, never in the lab's source.

`CC1Lab.pro` and `CXX1Lab.pro` are RIDE's project files - open them in RIDE, pick the target,
Run. `xcode/` holds the judge's Xcode project, written from the `.pro` by
`tools/make-xcode.py` and kept, so it is what a user of Xcode would see.

**ShmLab** (`shm/`, Windows leg only): `main.shl` and six of Compiler-Si's examples -
prime, sqroot, invert, gaussseidel, rotations, strsplit - each a program of its own until
its `main` was renamed, which is how a Shalimar program becomes a library. Real and
integer arithmetic, one- and two-dimensional arrays, text, borrowed library functions,
and a seven-file program the project says how to assemble. There is no Visual Studio
project for Shalimar, so its judge is Microsoft's assembler and linker over shalimar's own
assembly (`ml64`, `link`, the two commands shalimar runs when no assembler is named) against
RIDE's runtime archive: what the lab judges is the project's assembler, as the C and
C++ labs judge the compilers.

## Running a leg

    sh trilab.sh mac
    sh trilab.sh windows
    sh trilab.sh ccs

The macOS leg builds each lab through `RIDE --build` (the same code path as F4), runs it, builds the
judge's project with `xcodebuild`, runs that, and diffs. Products go under `$TMPDIR`, not
the tree; RIDE's own `cc1lab` / `cxx1lab` land beside the sources and are ignored by git.

The Windows leg ships the lab to the box as a tar, and `tools/windows-leg.cmd` there builds
each lab through `RIDEConsole --build --arch x86_64-windows --assembler <asm-win.exe>`
(c90 and cpp11 writing MASM, the project's assembler, `link`), builds the judge's `.sln`
with `msbuild`, runs all four, and the outputs come back to be compared here. The box needs
RIDE built by RIDE 4.5's `tools/to-windows.sh` and the assembler by MASM's `tests/windows.sh`.

The CCS leg is the box again: `tools/ccs-leg.cmd` has RIDE build each lab for `tms6747`
(a `.vm` directory of one `.s` per source) and runs it on RIDE's `vm6747.exe`; then cl6x
compiles the same sources to assembly, which vm6747 runs as well - CCS 7.4 ships no
simulator, so the emulator runs both sides. Each side is also assembled by cl6x and
linked by lnk6x against `rts6740_elf_eh.lib` into a real `.out` (`TI-LINKED-RIDE`,
`TI-LINKED-CCS` per lab): TI accepts what RIDE wrote, and the sources build as a real TI
program. cl6x's C++ program links but cannot run on vm6747 - STLport's streams call into
TI's compiled runtime, machine code the emulator does not read - so for C++ the judge is
that double acceptance, and RIDE's emulator output is held to Xcode's native run of the
same sources. Needs CCS 7.4's compiler at `C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2`
and the exception-handling runtime built once as `Emulator/tests/ti.sh` describes.

## The ledger

`expected/<leg>-<lab>.allowed` is the exact `diff` a leg is allowed to show for a lab, so an
allowed difference is a measured one: `jmp_buf` sizes, pointer widths, `sizeof(long)`. A
leg with no such file must print identical output.

## Measured

- **macOS, 2026-09-18**: C 111 lines and C++ 6 lines, RIDE (c90, cpp11) and Xcode (Apple
  clang 17) identical; no ledger entry needed.
- **Windows, 2026-09-18**: C 111 lines and C++ 6 lines, RIDE (c90, cpp11, the assembler)
  and Visual Studio 2022 (cl 19.44) identical; no ledger entry needed. The first run showed
  two things. c90 packed bit-fields end to end on every target, where the Microsoft ABI
  allocates them in units of the declared type: `{char; unsigned:6; unsigned:6; int}` was
  8 bytes to RIDE and 12 to cl, one line of 104 (fixed in c90, `Target::microsoftLayout`).
  And the file-I/O example wrote to `/tmp/`, which Windows has not, so both sides skipped
  its seven lines and exited 1 in agreement - the comparison was blind to the one example
  that exercises the C runtime. It writes beside the program now.
- **Windows, Shalimar, 2026-09-18 night**: 61 lines, RIDE (shalimar through the assembler)
  and ml64 + link identical, and identical to the Mac's run. Putting shalimar's corpus through
  the assembler first, before the lab, found three assembler faults (MASM `36a4c4a`,
  `b030f89`), one of them real jump sizing; the lab itself passed on its first run.
- **CCS 7.4, 2026-09-19**: RIDE links the `.out` itself now - `asm6x.exe` beside it (the
  ASM6x project, the compilers' C6000 assembler, held object for object to TI's asm6x)
  assembles the `.vm`, and told TI's compiler directory (`--ti`, `--tilib`) lnk6x links it:
  `RIDE-OUT c`, `RIDE-OUT cpp`. The cl6x-over-RIDE's-assembly check stays beside it as the
  independent reading of the same assembly.
- **CCS 7.4, 2026-09-18**: C 111 lines, RIDE (c90 on vm6747) and cl6x (its assembly on
  vm6747) identical, both sides linked by lnk6x; C++ 6 lines, RIDE on vm6747 identical to
  Xcode's native run, both sides linked by lnk6x, cl6x's own C++ not runnable (above). The
  first run of cl6x's code found two things in the emulator, not the compilers: `CALLP`
  written second in a parallel pair returned to its own slot rather than the packet's end,
  and the double-precision pipeline was not split-phase - cl6x's `INTDP; NOP 3; MPYDP`
  read a high word a cycle early and `2.25 * 7` came out `3.375`. c90 pads past both.
  The emulator also learned cl6x's dialect (all 64 registers, BNOP/RETNOP/ADDKPC, .asg,
  .bits, .group) to run it at all; the ILP32 lines (`long` 4, pointers 4) differ from the
  host legs by design and match on both sides.
