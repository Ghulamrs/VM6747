# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## The language this is written in

`src/` is **ISO C++14**, and anything added to it must be too. All three
toolchains pin it: `-std=c++14 -Wall -Wextra -Werror -pedantic` in the
`Makefile`, `<LanguageStandard>stdcpp14</LanguageStandard>` in
`msvc/cc1.vcxproj`.

**A Mac cannot enforce this, and that is the trap.** Apple's libc++ hands you
`std::string_view` under `-std=c++14`, so a C++17-ism compiles clean there,
passes the suites, and is refused only when it reaches real g++. The same file
that builds with `clang++ -std=c++14` on a Mac fails under `g++ -std=c++14`
with `'string_view' is not a member of 'std'`. **A change that compiles on a
Mac has been checked against Apple's library, not against the standard.**

The one feature that ever wanted C++17 was `string_view`, for the borrowed
text an operand carries. It is written by hand instead — the four-operation
`Str` in `src/backend/Spelling.h`. Its comment says it is not a string class
and nothing should grow it into one; that is a decision, not an omission.

## Build and test

```
./build              build (wraps make in a memory cgroup where one is available)
./build test         build and run the full Linux suite
./build clean
```

Use `./build` rather than bare `make`: a class-heavy translation unit here was
measured at 142 MB, and the build is serial by design for that reason.

`make test` runs six suites and **not** the whole set — `run.sh`, `windows.sh`,
`driver-modes.sh`, `debug.sh` (twice: host target and `x86_64-windows`) and
`fingerprint.sh`. Do not read "all suites green" into it.

Individual suites, and what each needs:

| Suite | Needs | What it asks |
| --- | --- | --- |
| `tests/run.sh` | Linux + gcc | the whole corpus, differential against gcc |
| `tests/windows.sh` | Linux | Microsoft-ABI code, run natively (see its header) |
| `tests/debug.sh [arch]` | gdb or lldb | what `-g` produced, asked of a debugger |
| `tests/arm64.sh` | an arm64 Mac | the arm64 backend against clang |
| `tests/cross-abi.sh` | any host with a C compiler | cc1's objects linked against the host compiler's |
| `tests/fingerprint.sh` | nothing | every byte of every target's assembly against recorded digests |
| `tests/masm-native.sh`, `tests/windows-native.sh` | a Windows host over ssh | ml64/link and clang/PE, natively |
| `tests/c90-probe.sh`, `tests/not-c90-probe.sh` | a C compiler | what the language says vs what this accepts |

One case, or a serial run for debugging:

```
./tests/run.sh gcd          cases whose name contains "gcd"
./tests/run.sh '' 1         serially
./tests/debug.sh x86_64-windows    the debug corpus against the Microsoft ABI
```

## Architecture

A single pass per stage, no IR:

`Driver` → `Preprocessor` (text in, text out, with `#line` records) → `Lexer`
→ `Parser` (builds `Ast.h` and does all type work in `Type.cpp`) → a backend.

**`Source` owns the preprocessed text** and turns a byte offset into a file,
line and column. Diagnostics and the line table both go through it, so the two
can never disagree about where something was written.

**The backends share one statement walk and nothing else.** `backend/Walker`
holds fourteen visitors, the jump stack and the label discipline in one text;
each target supplies five one-line primitives (compare-and-branch, jump,
label, case-compare). Expressions stay per-target — `Call` most of all, since
that is where the ABIs genuinely part. A difference between targets belongs in
a primitive, where it is visibly a decision.

**Three things vary per target and they are separate axes.** `Target` answers
what the types measure; `Abi` answers how arguments travel; `Spelling` answers
how an instruction is written down. One instruction stream serves both x86-64
targets — `GnuSpelling`, `CoffSpelling` and `MasmSpelling` are the whole of what
differs between them, which is why `-masm=gnu`, `-masm=gnu-elf` and MASM produce
the same program.

**`backend/Dwarf.cpp`** writes the debug information for the ELF and Mach-O
targets, **`backend/CodeView.cpp`** for `x86_64-windows` under `-masm=gnu`, where
cdb reads it from the PDB link.exe makes. `-g` is refused in the MASM spelling —
ml64 builds no line table. See the M10 W3 section below.

## The bug class this project produces

**Cross-target divergence** — two lowerings answering the same C differently.
When one backend is wrong, ask what the other one does before assuming both
are: the answer is usually there, and a divergence is far likelier than both
being wrong.

**Being hosted on a platform is a separate axis from targeting it.** Bugs have
hidden in `directoryOf` treating `/` as the only separator, and in symbol
mangling escaping onto imported names — neither reachable from the machines
this is usually developed on.

## Verification

**Prove the artefact, not the exit status.** A green suite proves nothing
until you know what it ran against. `make` with nothing to rebuild, a relay
that silently failed, or a binary older than its sources will all report
success.

- Check the binary is newer than its sources: `[ cc1 -nt src/Parser.cpp ]`
- Grep the emitted assembly for a token the change introduces — the cheapest
  confirmation there is
- `tests/fingerprint.sh` is 1,672 digests, which is every case for every
  target variant; if a change is meant to alter nothing, this is what says so

When a suite fails on one host and passes on another, **suspect the host's
tools before the compiler**. `timeout` is GNU coreutils and absent on macOS,
which once presented as eight ABI failures in `cross-abi.sh`.

Development happens on more than one machine (see the README). When relaying
files rather than pulling them, hash them on the far side against
`git show HEAD:<path>` — a relayed tree is not a checked-out one, and `git
pull` aborts over untracked files it would overwrite *after* printing
"Updating", so the failure reads like success.

## Comments cut to the cap, 2026-09-26: the long forms

**No comment group in `src/` runs longer than three lines, and one standing in
front of a single line of code runs one line** - the house rule, with
`C++Optimize/tools/comment-lines` as its oracle (`--count` for a gate). The
28 groups over it were cut the way the compiler repository's were: whole
sentences kept, the finding first, and what the code beside them no longer
needs to say written here. `git log -p` holds every long form; these are the
ones worth finding without it.

**1.1 is the first version this compiler has had a number for** (`Driver.cpp`,
`cc1Version`). It had none until 2026-08-26 - built, relayed between three
machines and run without one, which works until somebody holds two copies and
has to say which is which. It is numbered with the group rather than on its
own: cc1 is used beside a particular RStudio and shc, and 1.1 is the release
where it stopped being the only compiler in the workbench - a target could hold
C and C++ together, and C became the one language with a choice of compiler in
it. The 1.2 work is Shalimar's; cc1's part in it was to build the libraries a
Shalimar program calls, which asked nothing new of the compiler itself.

**--version leaves with 0** because a question answered is not a failure; it
and a bad argument both used to leave with 1, which is why a script asking three
compilers their versions stopped at the first one.

**The Windows toolchain, from inside an editor** (`Driver.cpp`, `askVswhere`,
`developerShell`, `forCmd`). Three findings, each a build that failed only from
RStudio and never from a command prompt:

- vswhere's answer is fetched through a temporary file rather than a pipe.
  `_popen` would be the obvious way and does not work here: cc1 is itself run
  through a pipe by the editor, and a nested `_popen` fails when the parent's
  stdio are not consoles - so it found Visual Studio from a command prompt and
  never from inside RStudio, which is the one place it was needed.
- ml64 and link live in Visual Studio and are on PATH only inside a Developer
  Command Prompt. An editor launched from Explorer is not one, so every build it
  asked cc1 for failed at the assembler with a message telling a person to open
  a different shell - which the editor cannot do for them. So when the tools are
  not already reachable, the command runs inside a shell that has sourced
  `vcvars64.bat`. That sets LIB as well as PATH, which matters: finding ml64
  alone still leaves the linker unable to see `libcmt.lib`.
- The tool runs through a batch file rather than by prefixing the command.
  cmd's rule for stripping the outer quotes of a `/c` string is not something to
  build on when the string already holds several quoted paths and an `&&` -
  every spelling tried produced "The filename, directory name, or volume label
  syntax is incorrect" from somewhere inside it. A file has no quoting question:
  the call is on its own line and the command is on the next, exactly as
  written. And `cmd /c` strips the first and last quote of a command that has
  both, so `"ml64.exe" ... "x.s"` arrives as `ml64.exe" ... "x.s`; one more pair
  around the whole thing is what cmd eats instead. That was visible only where
  the tools were already on PATH and no vcvars shell was added, which is how it
  hid: standalone the wrapper replaced the command and covered it, and the
  editor - which imports the MSVC environment into itself before running
  anything - was the one caller that took this path.

**A typedef name reaching the specifier loop ends the specifiers**
(`Parser.cpp`, the `while (atTypeName())` loop). `atTypeName()` is also true
for an identifier naming a typedef, and nothing in the loop consumes one, so
without the `break` it spun forever on `typedef long T;` where `T` was already a
typedef. Stopping there is what lets the "typedefed twice" error be reached at
all: it never was - 425 cases and not one of them redeclares a typedef, so the
compiler hung instead of saying no, which is the worse of the two by a distance.

## M10 W3, 2026-10-08: c90 under cdb

**`-masm=gnu` on `x86_64-windows` is now the GNU spelling of a COFF object**, and
the driver assembles it with clang (`-target x86_64-pc-windows-msvc`; Visual
Studio's own, or `C90_CLANG`) rather than ml64, which reads only MASM and had
been handed it. `CoffSpelling` (`backend/Spelling.h`) is the difference from the
ELF spelling: `.section .rdata`, `.def` per function, `.balign`, and `.seh_*`
round the one prologue every function has, so cdb can walk the stack - RBP is
the frame register, which is why the pushes an expression makes need no unwind
code. `link.exe` gets `/debug` under `-g`.

**The old spelling is `-masm=gnu-elf`, byte for byte.** `tests/windows.sh` and
`tests/debug.sh x86_64-windows` assemble the Microsoft convention with gcc for
Linux and debug it with gdb, which wants ELF and DWARF; they now ask for that by
name. `tests/fingerprint.txt` keeps the old digests under `-masm=gnu-elf` and has
new ones for `-masm=gnu`. Measured: every case in `tests/` under `-masm=gnu-elf`
is identical to what `-masm=gnu` wrote before, with and without `-g` and `-O2`.

**`-g` there writes CodeView** (`backend/CodeView.cpp`), cpp11's design (its
`CodeView.cpp`, M10 W1 and W2) in c90's own code: symbols in `.debug$S`, types in
`.debug$T`; the header lists the records. Two things were found by comparing with
cl /TC /Zi in cdb (`m10.py oracle` and `m10.py step`, `--compiler c90`), and the
first made a difference to every function:

- **cl gives a closing brace a line of its own.** A function's epilogue is the
  brace's line, and so is the return address of a call that `return f();` makes
  as the function's last statement; a loop jumps back from its body's brace.
  Without those, a step out of a function skipped the brace and a frame above a
  `return f();` named the wrong line. `Walker::marksClosingBrace` is true only
  for the COFF code generator: the DWARF line tables are as they were, and a
  `for` gets its one extra instruction (a jump to the step) only under `-g`.
- **An enum is int to the type system, and stays so** - giving it a type of its
  own would move every comparison of types by identity. The parser keeps each
  enum's tag and constants beside it (`Program::enums`) and marks the
  declarations whose type is the enum itself, which is what `LF_ENUM` needs for
  `colour col = BLUE (0n6)`; a pointer to an enum still reads as `int *`.

The end label CodeView measures a function by is `CoffSpelling`'s own: at `-O1`
and up the walker's `.Lfunc.end.` label waits in the optimizer and, after the
last function, is never written at all. The DWARF path still refers to it: `-g
-O2` for x86_64-linux names `.Lfunc.end.main` and defines it nowhere, a fault left
as it was here, since the ELF and Mach-O output was to stay byte for byte.

`tests/m10/w3.c` is the gate case: at line 27 (innermost) and 33 (a nested
block) `k` and `dv /t /V` name the same frames, lines, locals, types and values as
cl's, and `m10.py step` walks the same lines over and into the calls. The same
holds at the stop line of every case in `tests/debug/`.


## M10: no second run of a line in the CodeView line table, 2026-10-08

**A `for` whose body is on lines of its own had its line twice**: the step and the test are written
after the body, and their entry named the `for` line again, so cdb answered `bp` on it with
"Ambiguous symbol error" (cl writes the line once, its step coming before the body). A line that
already had a run in the function gets no second: that code counts as the line before it, and the
breakpoint binds. Measured with m10.py on a three-line loop: cl /Zi and ours both stop on it; c90
steps 5, 6, 7, 5, 6, 7 as cl does, cpp11 5, 6, 5, 6 (it keeps no line for the closing brace).
tests/m10 compares are unchanged; cases and emit golden unchanged.


## M10 W5: a function's entry is named by its `{` line, as cl names it, 2026-10-08

**cdb's step into a call stops on the callee's first instruction**, and reports the line that address
belongs to. cl gives that address the line of the `{`; ours gave it the declarator's line, so with the
brace on a line of its own the stack said `twice stepped.c:1` where cl's said `:2`. On x86_64-windows
the entry line is now the first `{` outside parentheses after the function's position
(`Walker::openingBrace`); other targets keep theirs, so their DWARF is unchanged. Measured with
m10.py step on stepped.c (RIDE's session program): cl 11, 2, 3, 4 and ours 11, 2, 3, 4. RIDE's F6
now steps once more from such a stop, to the first line, for cl's programs and ours alike.
