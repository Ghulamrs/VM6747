# VM6747

A parallel **i-line** of the three compilers: each cloned from its original in
`~/Documents/Claude` and built beside it under a distinct executable name, on
branch `vm6747`.

## Where it lives (since 2026-09-16)
This directory is one git repository, `git@github.com:Ghulamrs/VM6747.git`,
holding `Compiler-Ci`, `Compiler-Si`, `Emulator` and the notes above them
directly, each brought in with its full history. They no longer have a
`.git` of their own: commit them here, at the top.

`Compiler-Cppi` is the exception. It has its own repository,
`git@github.com:Ghulamrs/Compiler-Cppi.git`, and is only ever committed and
pushed there; this repository carries it as a **submodule**, so after a push
in `Compiler-Cppi` the pointer here is moved with
`git add Compiler-Cppi && git commit`. A fresh clone wants
`git clone --recurse-submodules`.

## Rules this was set up under
- All work inside `~/Documents/Claude`.
- **Originals never modified.** Verified after cloning and building: each
  original's `git status` is clean and its HEAD is unchanged.
- **One commit per change.** Until 2026-09-16 each clone was its own
  repository on branch `vm6747` with the original's GitHub remote as `origin`
  and nothing pushed; see *Where it lives* for what replaced that.
- Cloned with `--no-hardlinks` — no shared object store, no
  `.git/objects/info/alternates`; each clone is independent of its original.

## The three clones

| clone | from original | taken at commit | origin remote | executable: was → now |
|---|---|---|---|---|
| `Compiler-Ci`   | `~/Documents/Claude/Compiler-C` | `7be288e` | `https://github.com/Ghulamrs/Compiler-C.git` | `cc1` → `c90` |
| `Compiler-Si`   | `~/Documents/Claude/Compiler-S` | `f8e88c8` | `https://github.com/Ghulamrs/Compiler-S.git` | `shc` → `shalimar` |
| `Compiler-Cppi` | `~/Documents/Claude/C++`        | `6d7386a` | `git@github.com:Ghulamrs/Compiler-Cpp.git`   | `cxx1` → `cpp11` |

## Commits made on `vm6747` (one per change)
**Compiler-Ci**
- `e3addc0` — rename `cc1` → `c90` (Makefile `TARGET`)
- `92e3b03` — rename `cc1` → `c90` in `cc1.xcodeproj`

**Compiler-Si**
- `97ab0ff` — rename `shc` → `shalimar` (Makefile `SHC`)
- `edcf578` — rename `shc` → `shalimar` in `shc.xcodeproj` and `shc.vcxproj`

**Compiler-Cppi**
- `30edb76` — rename `cxx1` → `cpp11` (Makefile `TARGET`)
- `bdc4d0b` — point `ide/cxx1.xcodeproj` off the original tree into the clone
  (`$(SRCROOT)/..`); it was the only file referencing an original path
- `d5a76c2` — rename `cxx1` → `cpp11` in `cxx1.xcodeproj`, `ide/cxx1.xcodeproj`,
  `cxx1.vcxproj`, `ide/cxx1.vcxproj`

## Build
`make` in each clone produces **only** the i-named executable — verified,
runs:
- `Compiler-Ci`   → `c90.exe`   (no `cc1.exe`)
- `Compiler-Si`   → `shalimar.exe`   (no `shc.exe`) plus `lib/shmrt-*.a` (runtime; names unchanged)
- `Compiler-Cppi` → `cpp11.exe`  (no `cxx1.exe`)

Every IDE project in each clone (Xcode and MSVC) also names the i-executable
now, so no build system emits the plain name. The runtime archive names
(`shmrt-*.a`) are unchanged — they are the runtime, not the compiler.

## Notes
- **`shc`, not `shm`.** The Shalimar compiler's binary is `shc`; the rename
  request said `shm → shmi`. It was renamed `shc → shalimar` to match the family
  pattern (`c90`, `cpp11`), with your approval.
- **Third original.** The paths given (`Documents/Compiler++2/Compiler++`,
  `Claude/Compiler++2`, `Claude/C++2/C++`) did not exist; `~/Documents/Claude/C++`
  (builds `cxx1`, remote `Compiler-Cpp`) was used, confirmed.
- Only `ide/cxx1.xcodeproj` referenced an original path; that was fixed. No
  clone file references an original path.

## The fourth repository: Emulator
`VM6747/Emulator` (part of this repository since 2026-09-16) builds `vm6747.exe`,
the emulator that runs what the i-compilers emit for tms6747; see its
README.md and TMS6747.md. Same build conventions as the compilers.

## TMS6747 — the point of this line
VM6747 exists to add a third codegen target, the TI **TMS320C6747** (C6000
VLIW DSP), to the i-compilers. See **TMS6747.md** for the strategy, milestones,
and status. Milestone 1 (target stood up, integer-constant returns) is done in
`c90` (`8e0dc35`) and `cpp11` (`090732b`); `shalimar` is later.
