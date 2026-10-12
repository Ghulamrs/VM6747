# VM6747

The C6747 emulator and the notes above it. Until 2026-10-12 this repository
also held the three compilers RIDE builds - `Compiler-Ci`, `Compiler-Si` and,
as a submodule, `Compiler-Cppi` - and TriLab. They are gone from here: each
compiler is its own repository, checked out beside this one, and TriLab is
obsolete.

## Where the compilers live (since 2026-10-12)

| compiler | directory beside VM6747 | repository | executable |
|---|---|---|---|
| c90      | `../Compiler-Ci` | `git@github.com:Ghulamrs/Compiler-Ci.git`           | `c90.exe` |
| shalimar | `../Compiler-Si` | `git@github.com:Ghulamrs/Compiler-Si.git`           | `shalimar.exe`, `lib/shmrt-*.a` |
| cpp11    | `../C++Optimize` | `git@github.com:Ghulamrs/Compiler-Cpp-Optimize.git` | `cpp11.exe` |

Their histories up to the removal are in this repository's log; from here on
they are committed and pushed in their own repositories, and RIDE reads them
there. `Emulator/tests/ti.sh` and `windows.sh` read their `tests/out-tms6747`
from those directories.

## The fourth repository: Emulator
`VM6747/Emulator` (part of this repository since 2026-09-16) builds `vm6747.exe`,
the emulator that runs what the i-compilers emit for tms6747; see its
README.md and TMS6747.md. Same build conventions as the compilers.

## TMS6747 — the point of this line
VM6747 exists to add a third codegen target, the TI **TMS320C6747** (C6000
VLIW DSP), to the i-compilers. See **TMS6747.md** for the strategy, milestones,
and status. Milestone 1 (target stood up, integer-constant returns) is done in
`c90` (`8e0dc35`) and `cpp11` (`090732b`); `shalimar` is later.
