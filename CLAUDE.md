# VM6747

The C6747 emulator, `vm6747.exe`, and the notes beside it. Until 2026-10-12
this repository also held the three compilers RIDE builds - `Compiler-Ci`,
`Compiler-Si` and, as a submodule, `Compiler-Cppi` - with TriLab and the
2026-09-17 review's probes, and the emulator sat in `Emulator/`. The same day
the compilers left for their own repositories beside this one, TriLab and the
probes were removed, and `Emulator/` was moved up to be the repository itself:
`src/`, `tests/`, `msvc/`, `tools/`, the Makefile and the two IDE projects are
at the top now, objects in `build/` (ignored).

## Where the compilers live (since 2026-10-12)

The workspace has three groups since 2026-10-13: this repository is `TI/VM6747`, beside
`TI/SIM6747`, `TI/ASM6x`, `TI/LNK6x` and `TI/RTS6x`; the compilers are under `Compilers/`.

| compiler | directory, from this repository | repository | executable |
|---|---|---|---|
| c90      | `../../Compilers/Compiler-Ci` | `git@github.com:Ghulamrs/Compiler-Ci.git`           | `c90.exe` |
| shalimar | `../../Compilers/Compiler-Si` | `git@github.com:Ghulamrs/Compiler-Si.git`           | `shalimar.exe`, `lib/shmrt-*.a` |
| cpp11    | `../../Compilers/C++Optimize` | `git@github.com:Ghulamrs/Compiler-Cpp-Optimize.git` | `cpp11.exe` |

Their histories up to the removal are in this repository's log; from here on
they are committed and pushed in their own repositories, and RIDE reads them
there. `tests/ti.sh` and `tests/windows.sh` read their `tests/out-tms6747`
from those directories.

## The emulator
`make` here builds `vm6747.exe` (`msvc\build.cmd` on Windows), the emulator
that runs what the compilers emit for tms6747; see README.md and TMS6747.md.
Same build conventions as the compilers.

## TMS6747 — the point of this line
VM6747 exists to add a third codegen target, the TI **TMS320C6747** (C6000
VLIW DSP), to the i-compilers. See **TMS6747.md** for the strategy, milestones,
and status. Milestone 1 (target stood up, integer-constant returns) is done in
`c90` (`8e0dc35`) and `cpp11` (`090732b`); `shalimar` is later.
