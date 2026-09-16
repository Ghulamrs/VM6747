# VM6747

A parallel **i-line** of the three compilers: each cloned from its original in
`~/Documents/Claude` and built beside it under a distinct executable name, on
branch `vm6747`.

## Rules this was set up under
- All work inside `~/Documents/Claude`.
- **Originals never modified.** Verified after cloning and building: each
  original's `git status` is clean and its HEAD is unchanged.
- **One commit per change**, on branch `vm6747`. **Nothing pushed.** Each
  clone's `origin` is the original's GitHub remote, and
  `remote.origin.push = refs/heads/vm6747:refs/heads/vm6747`.
- Cloned with `--no-hardlinks` — no shared object store, no
  `.git/objects/info/alternates`; each clone is independent of its original.

## The three clones

| clone | from original | taken at commit | origin remote | executable: was → now |
|---|---|---|---|---|
| `Compiler-Ci`   | `~/Documents/Claude/Compiler-C` | `7be288e` | `https://github.com/Ghulamrs/Compiler-C.git` | `cc1` → `cc1i` |
| `Compiler-Si`   | `~/Documents/Claude/Compiler-S` | `f8e88c8` | `https://github.com/Ghulamrs/Compiler-S.git` | `shc` → `shci` |
| `Compiler-Cppi` | `~/Documents/Claude/C++`        | `6d7386a` | `git@github.com:Ghulamrs/Compiler-Cpp.git`   | `cxx1` → `cxx1i` |

## Commits made on `vm6747` (one per change)
**Compiler-Ci**
- `e3addc0` — rename `cc1` → `cc1i` (Makefile `TARGET`)
- `92e3b03` — rename `cc1` → `cc1i` in `cc1.xcodeproj`

**Compiler-Si**
- `97ab0ff` — rename `shc` → `shci` (Makefile `SHC`)
- `edcf578` — rename `shc` → `shci` in `shc.xcodeproj` and `shc.vcxproj`

**Compiler-Cppi**
- `30edb76` — rename `cxx1` → `cxx1i` (Makefile `TARGET`)
- `bdc4d0b` — point `ide/cxx1.xcodeproj` off the original tree into the clone
  (`$(SRCROOT)/..`); it was the only file referencing an original path
- `d5a76c2` — rename `cxx1` → `cxx1i` in `cxx1.xcodeproj`, `ide/cxx1.xcodeproj`,
  `cxx1.vcxproj`, `ide/cxx1.vcxproj`

## Build
`make` in each clone produces **only** the i-named executable — verified,
runs:
- `Compiler-Ci`   → `cc1i.exe`   (no `cc1.exe`)
- `Compiler-Si`   → `shci.exe`   (no `shc.exe`) plus `lib/shmrt-*.a` (runtime; names unchanged)
- `Compiler-Cppi` → `cxx1i.exe`  (no `cxx1.exe`)

Every IDE project in each clone (Xcode and MSVC) also names the i-executable
now, so no build system emits the plain name. The runtime archive names
(`shmrt-*.a`) are unchanged — they are the runtime, not the compiler.

## Notes
- **`shc`, not `shm`.** The Shalimar compiler's binary is `shc`; the rename
  request said `shm → shmi`. It was renamed `shc → shci` to match the family
  pattern (`cc1i`, `cxx1i`), with your approval.
- **Third original.** The paths given (`Documents/Compiler++2/Compiler++`,
  `Claude/Compiler++2`, `Claude/C++2/C++`) did not exist; `~/Documents/Claude/C++`
  (builds `cxx1`, remote `Compiler-Cpp`) was used, confirmed.
- Only `ide/cxx1.xcodeproj` referenced an original path; that was fixed. No
  clone file references an original path.

## The fourth repository: Emulator
`VM6747/Emulator` (its own git repository, no remote) builds `vm6747.exe`,
the emulator that runs what the i-compilers emit for tms6747; see its
README.md and TMS6747.md. Same build conventions as the compilers.

## TMS6747 — the point of this line
VM6747 exists to add a third codegen target, the TI **TMS320C6747** (C6000
VLIW DSP), to the i-compilers. See **TMS6747.md** for the strategy, milestones,
and status. Milestone 1 (target stood up, integer-constant returns) is done in
`cc1i` (`8e0dc35`) and `cxx1i` (`090732b`); `shci` is later.
