# The Shalimar debug engine: organisation

Phase 2. The design chosen in `DEBUG-ENGINE-ANALYSIS.md` - **option A: the
compiler writes its own table, the program's debug runtime reads itself** -
laid out as formats, protocol lines, classes per repository, milestones and
tests. For approval before any code is written.

## 1. The shape in one paragraph

`shc` writes, into every program it compiles, a text table describing its
units, functions and variables (name, type, kind, 8-byte slot, visible lines),
and passes its address to the runtime once at start-up. Every named function
passes the address of its slot 0 to `shm_enter`, beside the three arguments it
passes today. The **release** runtime ignores both. The **debug** runtime keeps
a frame stack `{function, base, caller's unit and line}`, and when stopped it
answers five new commands - frames, variables, children of an array, a value,
set a value - formatting values as the language prints them. RIDE parses the
answers into `StackFrame`, `Variable` and `Watch`, and the grids fill. The
simulator and emulator change in nothing.

## 2. The debug table, format 1

### 2.1 Where it lives

One table per **compiled program** (a `shc` run writes one assembly file for
the whole program, all units included - `tests/linking.sh`), as a
NUL-terminated byte string emitted with the existing `defineBytes` into the
read-only data the target already uses for string literals:

| target | section | symbol |
| --- | --- | --- |
| x86_64-linux | `.rodata` (as today's literals) | file-local, as other `defineBytes` ids |
| x86_64-windows | `.rdata` / MASM `CONST` | file-local |
| arm64-darwin | `__TEXT,__cstring` | file-local |
| tms6747 | `.const` | file-local |

No symbol is exported: the generated `shm_name_files` (already called first
thing by `main`, `Runtime.cpp`) gains one call,
`shm_describe(table, globals)`, with the table's address and the global
block's address. Text rather than binary because the runtime must parse it on
a C6000 with nothing but its own code, because a person can read it in the
`.s`, and because it needs no relocation but the one pointer it is handed.
**The emitted assembly stays identical between debug and release** - the rule
`DEBUGGING.md` was built on - unless the user decides otherwise (question 1 in
the report).

### 2.2 Grammar

Records are lines, fields are separated by one TAB, the first field is a
one-letter record kind. Unknown kinds are skipped, so later versions add
records without breaking an older runtime.

```
H	shalimar-debug	1                            header: format name, version
U	<unit>	<file name>                           one per unit; unit 0 is the program's own file
F	<id>	<name>	<unit>	<first line>	<last line>	<slots>
                                                  one per function; id is Prototype::id (shm_enter's);
                                                  id -1 is the top-level program
V	<function id>	<kind>	<slot>	<name>	<type>	<from line>	<to line>
                                                  kind: i input, o output (by pointer), r reference
                                                  input (by pointer), l local, g global (function -1,
                                                  slot = global index); lines bound where the name is
                                                  visible, in the function's unit
E                                                 end
```

`<type>` is `Type::spelling()` - `int`, `real`, `char`, `int[]`, `real[][]`,
`char[]` - so the grid's Type column is the language's own word.

Example, for `gcd.shm` with a global `count` and `function g = gcd(a, b)`:

```
H	shalimar-debug	1
U	0	gcd.shm
F	-1	program	0	1	14	0
F	3	gcd	0	3	9	4
V	-1	g	0	count	int	1	14
V	3	i	0	a	int	3	9
V	3	i	1	b	int	3	9
V	3	o	2	g	int	3	9
V	3	l	3	t	int	5	8
E
```

### 2.3 Where each field comes from in the compiler

- `F`: `Function::proto()` (name, id, unit), first and last statement lines
  from the body, `frame().variables()`.
- `V`: `Function::symbols_` (name, type, slot, `isReference`), inputs and
  outputs from `Prototype`; `from`/`to` recorded by the Checker as it pushes
  and pops `scope_` (the `Declare`'s line, and the line of the statement that
  closes the block). Globals from `Program::globals()`.

### 2.4 Reading a value (runtime)

`address = base + 8 * slot` (base from `shm_enter`; for kind `g`, the globals
block). Then by slot kind, exactly as `CodeGen::slotKind` stores it:
`int`/`char` - a 32-bit value at the address; `real` - a 64-bit double; an
array - a pointer to `struct Array {count, element, data}`, nested `KindRef`
arrays for each further rank; kinds `o` and `r` - the slot holds the address
of the real home, read through once. All four targets are little-endian, which
the runtime does not need to know: it reads through typed pointers.

## 3. The protocol, version 2

Today's lines are unchanged. Commands still arrive on the program's **stdin**,
answers go to **stderr**, one line each. Fields TAB-separated wherever a value
may hold spaces.

### 3.1 Announcing it

The debug runtime says `#protocol 2` before `#ready`. A RIDE that does not know
the line ignores it (`Session::listen` skips unknown lines). A RIDE that knows
it, talking to an old runtime that never says it, sends none of the new
commands and keeps today's sentences (`saysWhereOnly`). An old runtime handed a
new command would ignore it (the `default:` of `converse`), so RIDE must not
send one without the announcement - it would wait for an answer that does not
come.

### 3.2 New commands and their answers

Every answer ends with `#end` or is a single `#error <text>`, so RIDE never
waits on a count.

| command | answer lines |
| --- | --- |
| `t` - the stack | `#frame <n>\t<function>\t<unit>\t<line>` for n = 0 (innermost) .. deepest, then `#end` |
| `l <n>` - frame n's variables (`l g` the globals) | `#var <n>\t<name>\t<kind>\t<type>\t<address>\t<value>\t<more>` each, then `#end` |
| `e <n>\t<path>\t<from>\t<count>` - an array's elements | `#elem <path>[<i>]\t<type>\t<address>\t<value>\t<more>` each, then `#end` |
| `v <n>\t<expression>` - a watch | `#value <expression>\t<type>\t<address>\t<value>\t<more>` or `#error ...` |
| `= <n>\t<expression>\t<new value>` - set | `#set <expression>\t<value>` (as read back) or `#error ...` |
| `b <unit> <line>\t<condition>` - conditional breakpoint (D4) | none, as `b` today |

- `<value>` is what `print` would write for a scalar (Console's own format,
  shortest reals); for `char[]`, the text quoted with `\"`, `\\`, `\t`, `\n`
  escaped; for any other array, `[<dim> x <dim> ...]`.
- `<more>` is `1` when the value has children (an array) and `0` otherwise;
  RIDE uses it to draw the expander and asks `e` when it is opened, in pages
  (`<count>` at most 256) so a 10^6-element array is never sent whole.
- `<address>` is the address of the home (`0x` + hex, the target's pointer
  width) - for an array the address of its slot, and an element's the address
  in `data`.
- `<kind>` is the table's `i o r l g`, so the grid can mark parameters.

### 3.3 Expressions (watch, set, condition)

The language has names and indexing and nothing else to reach a value:

```
expression := name { '[' index ']' }
index      := integer-literal | name            (an int variable of the same frame)
condition  := expression op literal             op: == != < <= > >=
```

`a.b` and `*p` do not exist in Shalimar and are not accepted. Indexes are
checked against `count` and answer `#error Index 5 out of range` in the
runtime's own words. Arithmetic in a watch is not offered in format 1.

### 3.4 Setting a value

Scalars only: the new value is parsed as an `int`, `real` or one `char`
literal, converted as an assignment of that kind would convert it (the
runtime's `shm_real_to_int` etc., so the same range errors apply - reported as
`#error`, not raised), and written to the home. An array element is set the
same way through `a[i]`. Setting a whole array or a text is refused by name.

### 3.5 A transcript

```
< #protocol 2
< #ready
> b 0 6
> c
< #stop 0 6 1
> t
< #frame 0	gcd	0	6
< #frame 1	program	0	12
< #end
> l 0
< #var 0	a	i	int	0x7ffee3c0	18	0
< #var 0	b	i	int	0x7ffee3c8	12	0
< #var 0	g	o	int	0x7ffee3e0	0	0
< #var 0	t	l	int	0x7ffee3d8	6	0
< #end
> = 0	t	7
< #set t	7
```

## 4. Classes, per repository

Small files with a header comment saying what each is for, as the tree does.

### 4.1 Compiler-Si (`src/`)

| file | class / change |
| --- | --- |
| `src/DebugTable.h/.cpp` **new** | `class DebugTable` - built from a checked `Program`; `std::string text() const` writes format 1. Owns nothing else. |
| `src/Ast.h` | `Symbol` gains `fromLine_`, `toLine_` (set by the Checker) |
| `src/Check.cpp` | records the visible lines as `scope_` pushes and pops |
| `src/CodeGen.cpp` | `shm_enter` gets a 4th argument, `loadSlotAddress(0)`; `shm_name_files` calls `shm_describe(table, globals)` |
| `src/backend/Emitter.h` + the four targets | one new virtual `loadGlobalsAddress()` (each already has `globalsLabel()` or its equivalent) |
| `runtime/shmrt.h` | `shm_enter(int32_t, int32_t, const char *, void *frame)`, `shm_describe(const char *, void *)` |

### 4.2 Runtime (`runtime/`, compiled into both archives on all four targets)

| file | class | in release |
| --- | --- | --- |
| `Runtime.cpp` | `shm_enter` takes and ignores the frame; `shm_describe` empty | yes |
| `DebugTable.h/.cpp` **new** | `shm::Table` - parses format 1 in place (no allocation: an index of record offsets in a fixed array), looks up a function by id and its variables visible at a line | debug only |
| `Frames.h/.cpp` **new** | `shm::Frames` - stack of `{id, base, callerUnit, callerLine}`, pushed by the debug `shm_enter`, popped by `shm_leave`; capacity 1024, the overall ceiling `Depth` already enforces | debug only |
| `Values.h/.cpp` **new** | `shm::Values` - reads a home by kind and type, formats through `Console`'s number code, walks `Array`, evaluates an expression (3.3), writes a scalar (3.4) | debug only |
| `Debug.cpp` | `converse` dispatches `t l e v =` to the three above; `begin` says `#protocol 2`; `isBreakpoint` gains an optional condition | debug only |

The C6000 debug runtime is the same sources compiled by cpp11 `-O0
-DSHM_DEBUG=1` (`Makefile` 112-128) - nothing target-specific is added.
`Console`'s formatting has to be callable into a buffer rather than straight to
stdout; that is a small split of `Console::emit`, shared by both builds.

### 4.3 RIDE (`src/shalimar/`)

| file | class / change |
| --- | --- |
| `reply.h/.cpp` **new** | `shalimar::Reply` - parses one protocol-2 line (`#frame #var #elem #value #set #error #end`) into `editor::StackFrame`, `editor::Variable`, `editor::Watch`. Pure functions, unit-tested without a program. |
| `session.h/.cpp` | `protocol_` (1 or 2, from `#protocol`); `frames()` sends `t`; new `variables(frame)`, `globals()`, `children(frame, path, from, count)`, `watch(frame, expr)`, `setVariable(frame, expr, value)`, `breakAt(file, line, condition)`; a `collect()` that reads lines up to `#end`/`#error`. The `saysWhereOnly`/`saysHowDeepOnly` sentences stay for protocol 1. |
| `src/debugger.h` | `Variable` gains `address` and `hasChildren` (agree with `feature/debug-grids`, which owns the grids); `StackFrame` unchanged |
| `winforms/bridge.cpp` + the macOS front end | the existing `ride_` calls for locals, watches and frames route to `Session` when Shalimar is live, as `ride_debugger_start` already does |

The frame selected in the Call Stack grid is the `<n>` sent with `l`, `v` and
`=`, so clicking a frame shows that frame's locals with no other state.

### 4.4 sim6747 and vm6747

**No change.** The session runs inside the program; sim6747 already carries
its stdin and stderr. A later, optional companion - sim6747 printing a hung
program's frames by reading `shm::Frames` and the table out of the image - is
noted in the analysis (option C) and is not a milestone.

## 5. Milestones

Each is shippable alone: an older RIDE still works with a newer runtime and
the other way round (section 3.1).

| | milestone | contents | tests | effort |
| --- | --- | --- | --- | --- |
| **D1** | frames and locals, -O0, x86_64-linux and arm64-darwin | table (2), `shm_enter` frame, `shm_describe`, runtime `Table`/`Frames`/`Values` reading scalars, commands `t` and `l`, `#protocol 2`; RIDE `Reply` + `Session::frames/variables` | `tests/debug.sh` grows transcript cases (`tests/debug/*.shl` beside `*.transcript`): a recursive function's stack, inputs/outputs/locals/globals, a name before its declaration line absent; RIDE `test.cpp` `Reply` unit tests and `steppingShalimar` reading locals | 2-3 agent rounds |
| **D2** | all four targets | `loadGlobalsAddress` in Tms6747 and X86_64Windows (MASM and GNU), the C6000 debug runtime rebuilt; reference and output parameters | the same transcripts on the Windows box (both spellings) and through sim6747 on the box; `tests/tms6747.sh` unchanged (release output identical - checked by the existing emit diff) | 1-2 rounds |
| **D3** | arrays, addresses, set value | `#elem`, `<more>`, paging; `=`; addresses in every line | transcripts expanding a `real[][]` and a `char[]`, setting a scalar, an element, an out-of-range refusal; the RIDE window: double-click edits, colour on change, Address column filled (Windows grids; macOS after) | 2 rounds |
| **D4** | watches and conditional breakpoints | `v` with the 3.3 grammar, `b ... <condition>` | transcripts: a watch across steps, `a[i]` following `i`, a condition firing on the 5th pass of a loop; RIDE Watch grid | 1-2 rounds |
| **D5** | optimized builds | only when a Shalimar optimizer keeps a variable out of its slot: table `L` records (location by line range, "register saved in frame word w"), runtime reads the callee-saved word | today: a test that `-O1/-O2` transcripts equal `-O0`'s on all four targets (true now, see analysis 2.1) - added in D2 so the claim is held; the `L` work itself waits | 0 now; ~2 rounds when needed |

**Total to D4: about 6-9 agent rounds**, roughly 8-14 hours of agent time
with three-box verification. The window work (grids) belongs to
`feature/debug-grids`; D1-D4 only have to fill the vocabulary it reads.

## 6. What stays true

- Release has **no debugger code**; the cost it carries is data and one
  address per function entry (question 1 in the report).
- The compiler's assembly is the same for debug and release, so nothing that
  checks the compiler's output learns this exists.
- One engine, one table, one protocol on four targets; the only per-target
  line is `loadGlobalsAddress`.
- Values are shown in the language's own spelling, by the code that prints
  them.
