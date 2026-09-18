# TriLab

Two programs, one in C and one in C++, each built by **RIDE 3.5 with the project's own
compilers** and by **the environment's native toolchain**, and the two outputs compared.
The native toolchain is the judge; RIDE is what is judged. A leg passes when, for each
program, the two print the same lines - or differ only in lines the ledger records as
legitimately different, with both readings beside each other.

| Environment | The candidate | The judge |
|---|---|---|
| macOS | RIDE on the Mac, `arm64-darwin`, cc1i / cxx1i | Xcode: the same sources in a native project, Apple clang |
| Windows, first | RIDE on the box, `x86_64-windows`, cc1i / cxx1i with the project's assembler | Visual Studio 2022: a native `.vcxproj`, `cl.exe` |
| Windows, next | RIDE on the box, `tms6747`, cc1i / cxx1i, run on vm6747 | CCS 7.4: `cl6x` / `lnk6x`, run on vm6747 too (CCS has no simulator) |

## The two programs

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

## Running a leg

    sh trilab.sh mac

builds each lab through `RStudio --build` (the same code path as F4), runs it, builds the
judge's project with `xcodebuild`, runs that, and diffs. Products go under `$TMPDIR`, not
the tree; RIDE's own `cc1lab` / `cxx1lab` land beside the sources and are ignored by git.

## The ledger

`expected/<leg>-<lab>.allowed` is the exact `diff` a leg is allowed to show for a lab, so an
allowed difference is a measured one: `jmp_buf` sizes, pointer widths, `sizeof(long)`. A
leg with no such file must print identical output.

## Measured

- **macOS, 2026-09-18**: C 111 lines and C++ 6 lines, RIDE (cc1i, cxx1i) and Xcode (Apple
  clang 17) identical; no ledger entry needed.
