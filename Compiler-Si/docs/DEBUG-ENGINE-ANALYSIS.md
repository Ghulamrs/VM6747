# The Shalimar debug engine: analysis

Phase 1 of the redesign asked for on 2026-10-07: a Shalimar debug session that
shows **what the program holds**, not only where it is, on all four targets -
tms6747 (through sim6747), x86_64-windows, x86_64-linux and arm64-darwin. This
document is what exists, what is missing, and the options. The chosen design
is in `DEBUG-ENGINE-ORGANISATION.md`. Nothing here is implemented yet.

Everything below was read from the sources at VM6747 `1eb4825` (Compiler-Si)
and RIDE-4.7 `main`; file and line references are to those.

## 1. What exists, and why it is that way

### 1.1 The program stops itself

`CodeGen` emits `shm_line(unit, line)` before every statement in every build
(`Emitter::setLine`, `src/backend/Emitter.h`). It exists so a runtime error can
name its file and line; `docs/DEBUGGING.md` turned the same call into a
debugger. The compiler's output is **byte-identical** between debug and
release; what differs is the runtime archive linked (`shc --debug` picks
`shmrt-<target>-debug`, `src/Driver.cpp:295`):

| | release runtime | debug runtime |
| --- | --- | --- |
| `shm_line` | records the position | records it, then `Debug::at` |
| `Debug` class | not compiled (`#ifdef SHM_DEBUG`) | armed by `SHM_DEBUG=1` |

`runtime/Debug.cpp` (181 lines) is the whole engine: a breakpoint table of 100
`(unit, line)` pairs, four modes (Running, Stepping, Over, Out) compared
against `callDepth()`, and a loop reading one-letter commands from **standard
input** and answering on **standard error**: `#file`, `#ready`,
`#stop <unit> <line> <depth>`, `#at`, `#exit`. Commands `b d c s n o w q`.

### 1.2 RIDE's side

`RIDE-4.7/src/shalimar/` (708 lines with its README): `Channel` keeps the
child's stdout and stderr apart (a `#stop` must not land inside a half-printed
program line); `Session` turns the protocol into `editor::Stop` and a
**one-entry** call stack whose function name is the text "N calls deep"
(`Session::frames`). `saysWhereOnly`, `saysHowDeepOnly` are the sentences the
tab shows instead of locals and a stack. `Session::listen` ignores any stderr
line it does not recognise - which is what makes the protocol extensible
without breaking an old RIDE (section 3 of the organisation document).

On the C6000, `sessionCommand` (`src/toolchain.cpp` ~309) runs
`sim6747 --run <program>.out`: the TI image, linked by lnk6x against
`lib/shmrt-tms6747-debug` (the runtime compiled by cpp11 at `-O0` with
`-DSHM_DEBUG=1`, Compiler-Si `Makefile:112-128`). sim6747 passes the target's
stdin/stderr through, so **the same in-program engine already runs on all four
targets**; the protocol never knew which machine it was on. On the hosts RIDE
runs the native executable.

### 1.3 RIDE's debugger vocabulary

`src/debugger.h`: `Stop`, `StackFrame {function, file, line}`,
`Variable {name, type, value}`, `Watch {expression, value, ok}`, and the C/C++
engines (gdb, lldb, cdb) behind `Debugger::locals()`, `setVariable`,
`watches()`. The approved Debug tab (`docs/debug-tab-mockup.html`) wants
Locals and Watch grids `Name | Value | Type | Address`, a Call Stack
`Function | File | Line` whose rows can be clicked, a value edited by
double-click, changes in colour. **`Variable` has no address field yet**; the
`feature/debug-grids` branch carries no change to `debugger.h` at the time of
writing, so the field is to be agreed with that work (organisation, 4.3).

## 2. What is missing, and where it would come from

| needed at a stop | who knows it today | where |
| --- | --- | --- |
| each frame's function, file, line | runtime knows only the current position and a depth count | `Position` (one pair), `Depth` (per-function counters, `Runtime.cpp`) |
| which variables a function has, their types | **compiler only** | `Function::symbols_`, `Symbol {name, type, slot, storage, reference}` (`Ast.h:78`) |
| where each lives | compiler: an 8-byte **slot number**; target: slot -> address | `Frame` (`Ast.h:549`), `slotAddress` / `slotOperand` per backend |
| in which lines a name is visible | compiler, transiently | `Checker::scope_` push/pop (`Check.cpp:165-189`); `Declare` carries its line |
| the frame's address at run time | **nobody records it** | the prologue sets it; nothing passes it out |
| globals' address | compiler (one block, `globalsLabel()`) | `defineGlobals`, `loadGlobal(index)` |
| how to read an array | runtime | `struct Array {count, element, data}`, `KindInt/Real/Char/Ref`; rank is nested `Ref` arrays |
| how to print a value as Shalimar prints it | runtime | `Console` (`Shortest.cpp` for reals) |
| memory and registers from outside | sim6747 (C6000 only); the OS debug API on hosts | `SIM6747/src/C6xCpu`, `C6xMem` |

Four facts about the language and the code generator make the problem much
smaller than for C:

1. **Every variable has a home slot, always.** The machine model is one
   accumulator per kind and "anything that has to outlive ... waits in a
   numbered eight-byte slot" (README). The optimizer (`Optimize.cpp`, x86 only)
   folds register-to-register moves inside a run and **never removes a store
   to a slot**; Tms6747 and Arm64Darwin have no optimizer (`setOptimize` is a
   no-op there). So at every statement boundary every local is in its slot, at
   **-O0, -O1 and -O2 as the compiler stands today**.
2. **Three scalar kinds and arrays, nothing else.** `Type::Kind {Int, Real,
   Char, Array}`. No records, no pointers, no structs: `a.b` and `*p` do not
   exist in Shalimar. A reference parameter is a slot holding an address
   (`Symbol::isReference`), and an array value is a pointer to a runtime
   `Array`.
3. **Slots are frame-relative on every target**, and the emitter already has
   a target-independent primitive for a slot's address: `loadSlotAddress(slot)`.
   `address(slot k) = address(slot 0) + 8k` on all four (x86: `shadow + 8k`
   from the frame register, `X86_64.cpp:50`; C6000: `A15 + 8k - base`,
   `Tms6747.cpp:46`; arm64: `sp` plus a fixed offset plus 8k, `Arm64Darwin.cpp:27`).
4. **Every named function already reports its entry and exit** -
   `shm_enter(id, limit, name)` / `shm_leave(id)` (`CodeGen.cpp:116-135`) for
   the recursion ceiling. The top-level program is not counted and has no
   locals: its names are globals.

So the information is all in the compiler and the missing run-time fact is a
single pointer per frame.

## 3. The options

### (A) The compiler emits its own table; the program's session reads itself

The compiler writes a small, versioned description of the program (units,
functions, variables: name, type, slot, kind, visible lines) as read-only data,
and hands its address to the runtime at start-up. `shm_enter` also receives the
address of the frame's slot 0. The debug runtime keeps a stack of
`{function, frame base, caller's unit and line}` and, when stopped, answers new
commands - frames, locals, a value, set a value - by reading its own memory
and printing in the language's own format, over the channel that exists.

- **All four targets: yes, by construction.** The session already runs on all
  four with no target-specific code; reading a slot is a C++ pointer read in
  the runtime, compiled by clang, g++, cl and cpp11 alike. The only
  per-target work is one `lea` the emitters already know how to write.
- **Clean room: trivially.** Own format, own code; no specification to follow
  and nothing to copy.
- **Release cost:** no debugger code (the release runtime does not have it).
  What release does carry: the table as read-only data (tens of bytes per
  variable), one address computed per function entry (one `lea`/`ADD` and one
  argument register, beside a call that already exists), and one call at
  start-up. This keeps the **"compiler output identical between debug and
  release"** rule of `DEBUGGING.md`; the alternative (table only under
  `--debug`) is cheaper and breaks that rule - a question for the user (see the
  report).
- **Values are formatted by the runtime that printed them**, so `0.1` shows as
  Shalimar prints `0.1`, on every target, with no second formatter.
- **Limits:** the program must be a debug build, and it must be able to talk
  (a crashed or hung program cannot answer). A variable put in a register by a
  future optimizer needs a location list (section 5).
- **Effort:** small. Compiler: a table writer and two call changes. Runtime:
  frame stack, value reader/writer, a tiny expression reader. RIDE: a reply
  parser. No simulator change.

### (B) DWARF / CodeView and a real debugger

The compiler emits standard debug information; RIDE drives gdb/lldb/cdb.

- **tms6747: no.** There is no debugger for the C6000 in this toolchain; CCS's
  is TI's and closed, sim6747 has no debugger interface (`SIM6747/src` has a
  CPU and memory, no protocol), ASM6x/LNK6x carry no `.debug_*` today (LNK6x
  even drops the runtime's DWARF, CLAUDE.md "LNK6x links the kernels"). One
  would have to be written - which is option C.
- **x86_64-windows: not with MASM.** The default Shalimar Windows output is
  MASM for ml64, which carries no line table and cannot spell CodeView's
  relocations (`DEBUGGING.md`). A GNU-syntax path assembled by clang could
  carry `.cv_*` and work with cdb - a new Windows emitter mode first.
- **Linux/macOS: works** - DWARF `DW_OP_fbreg` for slots, `DW_TAG_array_type`
  cannot describe Shalimar's runtime `Array` without a pretty-printer per
  debugger (gdb Python, lldb formatters, natvis) - three formatters for one
  language.
- **Clean room:** DWARF and CodeView are public specifications; allowed, but a
  large one to implement correctly from the text, twice.
- **Release cost:** zero (debug info is not loaded) - its one advantage.
- **Verdict:** two of the four targets, three debuggers to drive and three
  value formatters; it replaces a working engine with a heavier one.

### (C) sim6747 is the debugger

sim6747 reads the program's memory and registers from outside, using the
compiler's table (A's table) to name them.

- **tms6747 only.** Hosts would still need A or B. Two engines for one
  language - the arrangement the tree's own rule 2 warns about ("divergence
  between targets is the house bug class").
- **Strength:** works on a program that has stopped answering, needs no
  runtime support, and could see an optimized frame through registers.
- **Cost:** sim6747 grows a breakpoint mechanism (an instruction-address
  breakpoint needs a line table - which A does not need, because `shm_line`
  is the line table), and its stdin is the program's, so the control channel
  would need a second pipe or a socket.
- **Verdict:** not the engine; a possible later **companion** reading the
  same table for post-mortem and hung-program inspection.

### Hybrids

- **A now, C later (recommended):** one table format, read by the program
  today and, if ever wanted, by sim6747 for a program that cannot talk. The
  table therefore records slots relative to a frame base that both can find:
  the runtime gets it from `shm_enter`; sim6747 could read the runtime's frame
  stack, which is a plain array in data.
- **A for values, B for lines:** no gain; `shm_line` is already better than a
  line table (statement-exact, all targets).

## 4. Judged together

| | (A) own table, in-program | (B) DWARF + debugger | (C) sim6747 outside |
| --- | --- | --- | --- |
| tms6747 | yes | no debugger exists | yes |
| x86_64-windows | yes | not with MASM | no |
| x86_64-linux / arm64-darwin | yes | yes, plus formatters | no |
| clean room | trivial | public specs, large | own format |
| release cost | table data + one `lea` per entry | none | none |
| values in Shalimar's own format | yes | per-debugger formatter | sim needs a formatter |
| works at -O0/-O1/-O2 today | yes (homes are slots) | yes | yes |
| hung or crashed program | no | yes | yes |
| effort (agent rounds) | ~8-11 over five milestones | ~25+, still no C6000 | ~12, C6000 only |

**Verdict: (A)**, with the table designed so that (C) can read it later.

## 5. Optimized builds

Today the question does not arise: no Shalimar optimizer removes a slot store,
so `-O1/-O2` debugging works with the -O0 table. When a register allocator
arrives (on any target), each variable needs a **location list**: for a range
of statements, "in register R" instead of "in slot k". In design A the runtime
can only read a register if it is spilled somewhere it can find, so the rule
would be: at each `shm_line` call the variables live in callee-saved
registers are those the function saved in its prologue; the table would
record "register R, saved in frame word w" per range, and the runtime reads
the saved word of the *callee* frame (the runtime's own `shm_line` frame is
the callee). That is a table-format v2 addition (`L` records), not a redesign;
it is milestone D5 and is only worth doing when the optimizer exists.

## 6. Could c90 and cpp11 share it (M10, debugging C/C++ on x86_64-windows)?

**The protocol and RIDE's grids: yes. The mechanism: no, and it should not.**
Design A works because Shalimar's compiler calls the runtime before every
statement and has no pointers, unions or aliasing; a C program has neither the
per-statement hook nor a runtime that knows its types, and instrumenting
cpp11's -O0 output with `shm_line`-like calls would make the debug build a
different program from the release one in exactly the way DEBUGGING.md
refuses. For M10 the right road is (B) for C and C++: cpp11's Windows output
is already GNU syntax assembled by clang (CLAUDE.md, "The GNU spelling is
x86_64-windows's default now"), which can carry CodeView `.cv_*` directives,
so cdb - already one of RIDE's three engines - can debug it. What M10 can
reuse from this work is **everything RIDE-side**: the `Variable` with an
address, the grids, the Watch and Call Stack handling, and the Session/Debugger
split that `Editor::debugging()` already routes. c90 (cc1 / Compiler-C) on
Windows is the MASM case and is blocked on CodeView exactly as Shalimar would
have been under (B).
