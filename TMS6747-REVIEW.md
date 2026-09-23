# TMS6747 backend review — discrepancy checklist (Fable 5.1, 2026-09-14)

Reviewed: c90 and cpp11 `src/backend/Tms6747.{h,cpp}`, the Walkers, cpp11's
parser, the VM6747 emulator runtime, shalimar's target registry. ~20 probe
programs compiled with the docked binaries and run on vm6747. Paths are
under `~/Documents/Claude/VM6747/`.

**Silent wrong answers first: A1, A2, A3.** Everything else is refused by
name, unverified against TI tools, or missing.

## A. C++ coverage on tms6747

| id | title | who | severity | evidence | fix |
|---|---|---|---|---|---|
| A1 | 64-bit shift by a runtime count >= 32 yields 0 (or the sign) | c90 **and** cpp11 | **wrong-code** | `Compiler-Cppi/src/backend/Tms6747.cpp:582-598`: `SUB A0, A6, A0` (32-count) is emitted after `[!A1] B big` + `NOP 5`, so on the taken path A0 is still 32 and `NEG` gives -32 -> shift by 32. Probe: `1LL<<33, u>>40, s>>35, (1LL<<40)>>8` printed `0 0 0 0`; host `8589934592 8388608 -33554432 4294967296`. Identical in c90. No suite has a runtime count >= 32. | Compute count-32 on the big path, or hoist the SUB above the branch. Add cases. |
| A2 | Pointer-to-member sized as on a 64-bit target | cpp11 | **wrong-code** (layout) | `src/Type.cpp:22,38` data-member pointer = `microsoftNames() ? 4 : 8`; `src/Type.cpp:191-195` pmf struct `{$fn @0, $adj: long long @8}` size 16 align 8. Probe: `pm 8 pmf 16` (32-bit Itanium: 4/8). Values work; any class holding one, `sizeof`, and interop are off. | Size both from `pointerBytes()`/`ptrdiffType()`, as vtables already are (`Parser.h:576-578`). |
| A3 | `size_t` is `unsigned long` in the headers but `unsigned int` to the target; `operator new` mangled `_Znwm` | cpp11 | divergence, masked by the emulator | `lib/stddef.h:34` has no `__TMS320C6X__` case; `Tms6747.h:19` `sizeType()=UInt`; probe: "'size_t' is typedefed twice ... 'unsigned long' ... 'unsigned int'". `ParserExprNew.cpp:1077` hard-codes `_Znwm`/`_Znam` (32-bit Itanium is `_Znwj`); the emulator accepts both (`Emulator/src/Runtime.cpp:389`). | A `__TMS320C6X__` branch in stddef.h; pick `j`/`m` from `sizeType()`. |
| A4 | Pointer to a **virtual** member function | cpp11, all targets | refused-but-should-work | "'S::vget' is virtual, and a pointer to a virtual member function is not supported yet" | Itanium encoding `$fn = 1 + vtable offset`, dispatch through the vptr at the call. |
| A5 | `typeid` | cpp11, all targets | missing-feature | "'typeid' is not supported yet"; `src/parser/Parser.cpp:23`. Leaves `__cxa_bad_typeid` and `type_info::{==,!=,name,before}` in `Runtime.cpp:957-962` dead. | Emit `&_ZTIx` / load vptr[-4]; a `<typeinfo>` header (absent from `lib/`). |
| A6 | `throw` of a pointer type | cpp11, all targets | refused-but-should-work | "'throw' cannot name the type of this: only a fundamental type has a type_info..." (`src/Mangle.cpp:1243`); the runtime already carries `_ZTIPKc` (`Runtime.cpp:186-200`). | Emit `__pointer_type_info` objects as class typeinfo is emitted. |
| A7 | `new T[n]` with a constructor; arrays of a class with a destructor | cpp11, all targets | missing-feature | "'new T[n]' of a class with a constructor would have to run it once per element - not supported yet"; "an array of 'struct Sc' is not supported yet because it has a destructor". No array cookie, no `__cxa_vec_*`. | Element loops + cookie. |
| A8 | User / placement `operator new` | cpp11, all targets | refused | "'operator new' is not supported yet"; `tests/cases/operator-new-refused.cpp` | - |
| A9 | `enum E : T`, `alignas` | cpp11, all targets | refused | `ParserType.cpp:1175`; `alignas` lexed (`Lexer.cpp:72`), not parsed. | - |
| A10 | Backend refusals by name | both | refused | `Tms6747.cpp:836` switch on a 64-bit value; `:352` bit-field in a 64-bit unit; `:882` non-word relocated piece; `:236` "this address". | Switch: compare pairs. Bit-field: LDDW + two-word EXT. |
| A11 | `noexcept` | cpp11 | works | Direct throw -> abort (`ParserStmt.cpp:1604-1612`; rc 134). Propagation through a noexcept frame terminates, handler not entered. | - |
| A12 | Everything else probed **works** | cpp11 | - | MI thunks, covariant returns, cross `dynamic_cast`, pure virtual, 4-byte vtables with offset-to-top and `_ZTI`; virtual bases (`sizeof(D)=28`, correct ILP32 Itanium); catch by ref/value/`...`, rethrow, nested throw from a handler, cleanups, unwinding through a variadic function and a 1200-byte frame; hidden-pointer class return with a balanced ctor/dtor ledger; static guards; `.init_array` + `__cxa_atexit` order; lambdas, range-for, `auto`, `constexpr`, bit-fields, goto across a cleanup, EBO, unions, `bool`=1, `nullptr_t`=4, `long double`=8. | - |
| A13 | Not probed | - | unverified | catch-by-value of a class with a non-trivial copy ctor (`tests/open/catch-copy-throws.cpp`, open on all targets); `char16_t`/`char32_t`; rvalue references to prvalues. | - |

## B. c90 vs cpp11 `Tms6747.cpp`

| id | delta | class | evidence |
|---|---|---|---|
| B1 | Not byte-identical any more: cpp11 mangles even a `.c` input | drift, doc stale | Diff is labels only (`mk` -> `_Z2mkii`, `counter` -> `_ZL7counter`); bodies identical; `TMS6747.md:35-36` still claims byte-identity. Fix: C linkage for `.c` in cpp11's driver, or amend the claim. |
| B2 | C++-only additions | intended | `Abi.h`, `Kind::Bool/NullPtr`, `codegen(..., gnuAsm)`, `emitsLineTable`, `landingPad`/`emitExceptionTable`, `.weak`/`isInline`/`prefixWord`/`.space`, ctor alias labels, `.init_array`, `clearCallSites`, `symbol()` for `name()`, `microsoftNames()`. Walker +220 lines. |
| B3 | Comment drift | drift | c90 keeps the long explanatory blocks; cpp11's were trimmed to its comment-line policy on 2026-09-14. Harmless; the files no longer diff cleanly. |
| B4 | A1 is in both | - | no hunk at `:575-601`. |

## C. Conventions not confirmed against TI tools

| id | claim | status |
|---|---|---|
| C1 | All structs returned through A3 (`structReturnLimit 0`, `Tms6747.cpp:43-45`) | unverified; interop-breaking if the EABI returns <= 8-byte structs in A4/A5 |
| C2 | Stack arguments from SP+4 (`:704`, `:747-755`) | unverified |
| C3 | Variadic: last named argument on the stack (`:693-697`, `:1000-1007`) | unverified, matches TI's guide as remembered |
| C4 | `.weak` is a weak definition (`:862`, `:1034`) | unverified; the emulator implements it (`Asm.cpp:319-320, 496-500`) |
| C5 | 6-bit shift counts (`:582`; emulator `Cpu.cpp:168-170` masks & 63) | consistent; the real bug is A1 |
| C6 | FP NOP counts (`:504-506`, `:787-807`) | unverified and **circular**: `Emulator/src/Isa.cpp:24-31` carries the same numbers, so the emulator is not an independent oracle for them |
| C7 | `__c6xabi_*` helper names and registers | circular: `Runtime.cpp:397-401, 965-984` implement exactly these |
| C8 | Symbol alphabet with dots (`L.return.f`, `L.str.0`, `next.n`, `_GLOBAL__sub_I_cls.cpp`) | unverified; the emulator accepts any token before `:` (`Asm.cpp:102-104`) |
| C9 | `.bss sym,size,align`, `.sect ".const"`, `.align n`, `.space`, `.word sym+n` (`:866-903`) | unverified TI forms |
| C10 | `char` signed (`Tms6747.h:18`); `wchar_t` = int (`:20`) - TI C6000 defaults to a 16-bit `wchar_t` as recalled; `long double` = 8 matches TI | unverified |
| C11 | The unwinder does not restore A10-B12 (`TMS6747.md:191`) | this line's own convention; fine while no value lives in them across a call (`:735`) |

## D. The emulator as an oracle

| id | finding | evidence |
|---|---|---|
| D1 | Native runtime, so a missing RTS symbol is invisible: all of libc, every `__c6xabi_*`, every `__cxa_*`, `__dynamic_cast`, the `__cxxabiv1` vtables and fundamental typeinfos | `Runtime.cpp:369-401, 170-200` |
| D2 | Present but unreachable from cpp11: `__cxa_bad_typeid`, `type_info::{==,!=,name,before}` (A5); `__gxx_personality_v0` faults | `:957-963` |
| D3 | Missing: nothrow new, aligned new, `__cxa_throw_bad_array_new_length`, `__cxa_vec_*`, `std::uncaught_exception(s)`, `.fini_array` (assembler knows `.init_array` only, `Asm.cpp:409`); `__cxa_guard_*` is a one-byte flag, not thread-safe - acceptable single-core. Nothing emits these today; latent. | - |
| D4 | Emulator-only behaviours a real RTS lacks: division by zero faults (`:965-972`); `abort` exits 134 silently (`:635`) | - |

## E. shalimar - the missing fourth target

`Compiler-Si/src/Target.cpp:36-53` registers three targets; `Driver.cpp:56`
lists them; no `tms6747`. The port is smaller than the C compilers' because
`CodeGen.cpp` drives a 30-method `Emitter` interface (`src/backend/Emitter.h`:
constants, slot load/store, `setArg`/spill, `call`, globals, byte blobs,
labels, `jumpIfZero`, `widenAccumulator`) over three slot kinds (Int/Real/
Wide), and the Arm64 emitter is 244 lines. A `Tms6747Emitter` maps the
accumulator to A4 (Real to A4/A5:A4), slots to `A15-off`, arguments to
A4,B4,A6,... overflowing to B15+4, `call` to `MVKL/MVKH B3; B; NOP 5`, blobs
to `.sect ".const"` `.byte` lists, plus a `Target` subclass that stops at
`.s`. Two harder parts: `Driver.cpp:477-500` assembles and links on the host,
so a tms6747 target must stop at `-S` and hand the file to `vm6747`; and the
Shalimar runtime (`runtime/shmrt.h`, ~90 `shm_*` entry points) is a C library
the emulator does not implement natively - either compile it with `c90 -arch
tms6747` and give both `.s` files to `vm6747`, or add the `shm_*` names to
`Runtime::names()`.


---

# Second review — the target against TI CGT 8.2.2 (Fable 5.1, 2026-09-14, evening)

Paths (abbreviated below): **Ci** = `/Users/g.r.akhtar/Documents/Claude/VM6747/Compiler-Ci/src/backend/Tms6747.cpp` (+`.h`), **Cppi** = `/Users/g.r.akhtar/Documents/Claude/VM6747/Compiler-Cppi/src/backend/Tms6747.cpp` (+`.h`), **Si** = `/Users/g.r.akhtar/Documents/Claude/VM6747/Compiler-Si/src/backend/Tms6747.cpp`, **Asm/Isa/Cpu/Runtime/main** = `/Users/g.r.akhtar/Documents/Claude/VM6747/Emulator/src/{Asm,Isa,Cpu,Runtime,main}.cpp`, **TI** = `.../scratchpad/ti/conv-ti.asm`. Probes and their outputs are in `/private/tmp/claude-501/-Users-g-r-akhtar-Documents-Claude-Shalimar/6d3ec67a-d371-49c6-a59c-e5cf26c14ba7/scratchpad/fable-review/` (p1_types.c, p2_structs.c, p3_imm.c, p4_cpp.cpp, p5_regs.c, s1.shm, lax.s, lax2.s). All three backends and the emulator were read end to end; nothing was written outside that directory.

## Ranked checklist

Severity: **S** = silent wrong result on silicon or at a boundary with TI-compiled code; **R** = assembler/linker rejection; **C** = cosmetic/conformance.

1. **[S, verified by TI] Struct/union return of 8 bytes or less.** TI returns it in A5:A4 (TI:313-334 `mk2`: `ADD .L1 1,A4,A5; RETNOP A0,5`); we always go through the hidden pointer in A3: `structReturnLimit 0` at Ci:43-45 / Cppi:44-46, the callee copy at Ci:694-701 / Cppi:715-722, the caller at Ci:802,806 / Cppi:813,817. Probe p2 (`3 4 | abc | 2 -2 | 9 | 34 9`) is right only because both sides are ours; p2_structs.s:517 shows `SUB A15, 8, A3` before `B mk2`. Fix: `structReturnLimit 8` in both Abis (the parser's `returnsIndirectly`, Compiler-Ci `src/Parser.h:98-110`, already keys on it); callee loads the value into A5:A4 (LDDW when 8-aligned, else LDW/LDH/LDB pieces), caller stores A5:A4 into `resultSlot`. Also the shalimar runtime (cpp11-compiled) and any Shalimar `foreign` declaration returning a small struct.

2. **[S, verified by TI] A3 may be null for a >8-byte struct return.** TI's callee tests it (TI:227,252 `MV .S1 A3,A0 ... [!A0] BNOP $C$L1,4`); ours stores unconditionally (Ci:697-700, Cppi:718-721). A TI caller that discards the result would fault our callee. Fix: guard the copy in `Return` with `[!A1] B skip` on the loaded pointer.

3. **[S, unverified] Struct arguments of 8 bytes or less.** We pass every struct as the address of a copy (Ci:748-753, 827; Cppi:760-765, 835; callee copies from A4 at Ci:1028-1033); p2_structs.s:812-815 passes `useTwo` the address. TI's EABI may pass small structs by value in a register (pair). Not in conv.c; `useTwo(Two)`/`callTwo()` in conv2.cpp will settle it. If TI passes them by value, every such call across the boundary is silently wrong.

4. **[S, verified by TI] `wchar_t` is 16-bit on TI** (TI:881-884 `wchar_t ... DW_ATE_signed_char, byte_size 0x02`; `.battr Tag_ABI_wchar_t(1)` at TI:772). Ours is `Kind::Int`: Ci `Tms6747.h:53`, Cppi `Tms6747.h:20`; probe p1 prints `wchar=4 L"ab"=12`. Fix: `wcharType() = Kind::Short`; the wide-string `.align width` and byte layout at Ci:977 / Cppi:991 follow automatically. Mangling (`w`) is unaffected.

5. **[S, unverified] Plain `char` signedness.** Ours signed (Ci `.h:51`, Cppi `.h:18`; p1: `(char)200 < 0: 1, value -56`). TI's default for C6000 is not settled by conv.c; `isneg()` in conv2.cpp will. If TI is unsigned, `char` compares and `%d` of chars differ everywhere.

6. **[S, unverified] Enum size.** Ours always 4 (p1: `E1=4 E2=4 E3=4`); TI emits `Tag_ABI_enum_size(3)` (TI:771), which in the ARM-derived attribute scheme means "container may be smaller". Add `sizeof(enum {A=1})` to conv2.cpp.

7. **[S, unverified, interop only] Array object alignment.** TI compiles with `--array_alignment=8` and records `Tag_ABI_array_object_alignment(0)` / `align_expected(0)` (TI:7, 773-774): TI code may assume a pointer to an array is 8-aligned. Ours aligns arrays to the element (Ci:931-939 `.bss a, 10, 1`; Cppi:936-949; locals via `objectAlign` in Compiler-Ci `src/Parser.cpp:524-525`). Matters only when TI-optimized code receives our arrays.

8. **[S, unverified] Bit-field layout.** Ours is byte-identical to clang's (p1: `sizeof BF=8 fd 55 75 4d 3c 2b 1a 09`, layout in Compiler-Ci `src/Parser.cpp:106-159`: LSB-first, declared-type container, no straddling, `long long` containers allowed). TI records `Tag_Bitfield_layout(2)` (TI:770); believed the same, not proven. Add the BF struct to conv2 and read TI's initializer bytes.

9. **[S, unverified] 64-bit stack arguments 8-aligned.** Caller: Ci:768-776 (`end = align8(end)` for a wide arg after the reserved word, so the 11th double sits at SP+8), callee mirror at Ci:815-823. conv.c has no such call; p5's `dmany(11 doubles)` printed `22` under our own convention. Add to conv2.

10. **[S at a TI boundary, verified by reading] shalimar clobbers callee-saved A10/B10/A12/B12.** Si:43-46 assigns arguments 7-10 to them and Si:109-124 loads them for a call; the shalimar frame saves only B3 (Si:72-87). Probe s1.shm: `shm_user_main` loads B12/A12/B10/A10 (s1.s:463-478) and its prologue is `STW B3, *B15` alone (s1.s:345). c90/cpp11 save them (Ci:1105-1116). Harmless inside this line (nothing keeps a value there across a call), wrong for a TI-compiled caller (e.g. RTS `qsort` calling a Shalimar comparator). Fix: save the four in the shalimar frame when a function makes a call with more than six arguments.

11. **[S, private convention, verified by reading] shalimar's >10-argument overflow block travels in B1** (Si:129-139), not at SP+4 as TI does (TI:707-718). Breaks only a `foreign` declaration with more than ten arguments.

12. **[R, verified by TI] `AND A6, 63, A6`** — Ci:637, Cppi:658; 32 occurrences in the sweep. Neither the range (scst5 is -16..15) nor the position (the constant is src1 for AND/OR/XOR) is legal. Fix: `EXTU A6, 26, 26, A6` (one instruction, ucst5 fields; masks to 6 bits).

13. **[R, unverified] `AND A6, -8, A6`** — Ci:899, Cppi:905 (va_arg of an 8-byte value). -8 fits scst5, but it is in src2; whether asm6x reorders commutative operands is unverified. Fix: `CLR A6, 0, 2, A6` (idiomatic; clears bits 0-2).

14. **[R, structural, verified by probe] The emulator range-checks no immediate and no operand position.** Isa.cpp:45-99 (`isaCheck`) checks shapes only: default case at 87-95, NOP at 53, MVK at 56; Cpu.cpp:142-144 silently truncates MVK. Hand-written `lax.s` (ADD ucst5 40, `AND 63`, `OR 200`, `SHL 40`, `CMPEQ A4, 100`, `MVK 100000`, `NOP 12`) ran to rc=6. So items 12-13 can recur unnoticed in any of the three emulator sandboxes. Fix (the cheapest way to make the Mac/Linux/Windows sandboxes catch what asm6x catches): scst5 for constant-first .L/.S forms, ucst5 for `ADD/SUB reg, ucst5`, shift counts and EXT/EXTU/SET/CLR fields, scst16 for MVK, 1..9 for NOP, and reject a constant in src2 of AND/OR/XOR/CMPxx.

15. **[R, verified by TI; in progress] Dots in symbols.** `tiSpelling` (Ci:86-104, Cppi:87-105, applied at Ci:1146 / Cppi:1178) is a whole-text post-pass; the rebuilt c90 emits `L$return$mk2` (conv-c90-new.s:61-66). shalimar emits no dots itself (`Lshm`, `Lshmb`, `Lshmr`: Si:59-60,142) but its runtime is cpp11's, so `tisweep/shmrt` must be regenerated with the rebuilt cpp11. The emulator accepts any token before `:` (Asm.cpp:102-116; lax2.s assembled `L.done.x`), so it cannot guard this either.

16. **[R, unverified] Symbols spelled like registers or mnemonics.** cl6x writes `||fp||` for a function named `fp` (TI:499,538) because of `.asg A15, FP`. We emit no `.asg`, so only raw `A0-A15`/`B0-B15` names collide; the sweep also has column-0 labels `add:`, `b:`, `set:`, `sub:` (likely fine with the colon). Cheap fix: `||name||`-quote any symbol matching a register or mnemonic.

17. **[R, unverified] `.weak` as a weak definition** (Cppi:940, 1112, 1118; 4,337 uses in the C++ sweep). The emulator keeps the first of several weak definitions (Asm.cpp:325-326, 497-513). TI's `.weak` semantics and lnk6x's treatment of duplicate weak definitions are unverified; conv2.asm (`twice`, `S::v`) will show cl6x's own spelling — if it is a section per symbol (`.sect ".text:_Z5twicei"` + `.clink`), that is the fix, and vtables/typeinfo need the same in `.const`.

18. **[R, unverified] `.bss sym, size, 1`** for char arrays (Ci:935, Cppi:944): TI requires a power-of-two alignment; 1 should qualify. `.align n` is only emitted for n > 1 (Ci:939).

19. **[R at link, verified by reading] What the emulator answers to that a real RTS must provide.** Runtime.cpp:409-447 lists 254 natives, plus the prelude data at 171-214 (`stdin/stdout/stderr` objects, `__dso_handle`, the six `_ZTVN10__cxxabiv1..._type_infoE` vtables, `_ZTVSt9type_info`, `_ZTI*`/`_ZTS*` for every fundamental type and pointers to them). Certain misses against rts6740_elf.lib: `__errno_location` (Compiler-Ci `lib/errno.h:36-38` falls into glibc's spelling for the C6000), `__assert_fail` (`lib/assert.h:12-13`, same fallback), and probably `strdup`, `signal`/`raise`. Unverified but must be checked: `__cxa_*`, `_Unwind_Resume`, `__cxa_atexit` (ARM-derived ABIs use `__aeabi_atexit`), `__cxa_guard_*`, `__dynamic_cast`, `_Znwj/_Znaj/_ZdlPv/_ZdaPv`, `__dso_handle`, the `__cxxabiv1` vtables, and the sixteen `__c6xabi_*` names at Ci:515-516, 563, 600-601, 714-718, 873 (`__c6xabi_divu/remu` are confirmed by TI:637,649; `__c6xabi_llshl` exists, TI:96). The lnk6x unresolved-symbol list is the definition of "what must exist".

20. **[S on silicon, emulator-only behaviour] Semantics the emulator invents:** division by zero faults (Runtime.cpp:1009-1016; TI helpers return an unspecified value), `abort` → 134 silently (678), argc/argv from the command line (main.cpp:107-128; TI's `_c_int00` with no `.args` gives argc 0), `time()` epoch (TI counts from 1900), `qsort` comparison order (848-861), a 16 MB heap.

21. **[C++ EH, verified by reading] `.vm6747.eh` is private** (Cppi:356-371 rows `begin,end,pad,frame,cleanup,n` + `typeinfo,index`; runtime Runtime.cpp:305-406 walks fp→fp+4 and "lands" by setting A15/B15/A4/B4/B3, landing pad at Cppi:346-351). A TI-hosted equivalent needs `.c6xabi.exidx`/`.c6xabi.extab` per function with TI's personality routines (ARM-EHABI style), which our uniform frame (B3 at fp+4, caller's A15 at fp, SP = fp + link, Ci:1040-1046) can describe compactly; the pad convention (A4 = exception, B4 = selector) already matches `__cxa_begin_catch`. The unwinder's non-restoration of A10-B12 (TMS6747.md:193-194) becomes wrong under a real unwinder. Sizeable but bounded; until then a throw through our frames on TI terminates.

22. **[C++ misc, verified by probe] Compatible as is:** guard variable 8 bytes (p4_cpp.s:54 `.bss once$s$guard, 8, 8`; ≥ Itanium's 8 / ARM's 4); array cookie = max(size_t, align) with the count at arr-4 (Compiler-Cppi `src/parser/ParserClass.cpp:2189-2194`; p4 printed `cookie=3`); `_Znaj`/`_ZdaPv` (p4_cpp.s:689,1042); vtable `[0, _ZTI, fns]` with 4-byte entries (30-35); typeinfo `[_ZTVN10__cxxabiv117__class_type_infoE+8, _ZTS]` (25-29); `.init_array` + `_GLOBAL__sub_I_<file>$cpp` (Cppi:1173-1177, p4_cpp.s:1370); `once()` guard through `__cxa_guard_acquire/release` (272, 295). Output: `G() / v=8 once=7 twice=8 tmax=9 cookie=3 wchar=4 counter=3 / ~Dt 5 x3 / caught 42 / ~G()`.

23. **[C, verified] Every constant is MVKL+MVKH** (Ci:70-74, Si:27-30): 132,881 pairs in the sweep, half of all instructions. `MVK` covers -32768..32767. Legal, only slow.

24. **[C, verified] Delay slots and forms that are right:** MPY32 NOP 3, LD NOP 4, B NOP 5, ADDSP/SUBSP/MPYSP 3, ADDDP/SUBDP 6, MPYDP 9 (TI used NOP 8 + one independent packet, TI:544-553), CMPxxSP/DP 1, INTSP 3, INTDP 4, SPTRUNC/DPTRUNC 3, SPDP/DPSP 1 (Ci:501, 556-558, 863-864, 876, 856-857; Isa.cpp:16-31); a DP source is never overwritten in the cycle after (only NOPs follow); `CMPEQ 0, A4, A4` / `XOR 1, A4, A4` (scst5 first); `EXT/EXTU/CLR` fields all ≤ 31; `ZERO A5:A4`, `MPY32U A4, A6, A1:A0` (even:odd); cross-path use is at most one per instruction; B15 stays 8-aligned (link 8/24, `align8` frames and areas); shift counts ≥ 32 give 0/sign (Cpu.cpp:168-170) — p1 `shr ... 1 80000000 4000000000000000`, `shl 10000000000 80000000` confirm A1 stays fixed. The emulator, unlike silicon, faults unaligned LDW (Cpu.cpp:16-21; lax2.s) — stricter, good.

25. **[C, verified by TI] Agrees with TI:** stack args at SP+4/SP+8 (TI:471-472, 707-718 vs Ci:768-776, 1025); variadic last-named-on-stack (TI:164, 725-739 vs Ci:757-761, 1064-1071); `long` 32-bit (TI:7), `long double` 8 (TI:942-943), `bool` 1 (TI:866-869), `size_t` = `unsigned int` (Compiler-Ci `lib/stddef.h:26`), `va_list` = `char *` (`lib/stdarg.h:34-38`); helpers take A4/B4 and return A4 (TI:627-653); callee-saved set (TI's `many` saves only FP, keeps B3 in A0; ours saves A15, B3 when calling, A10/B10/A12/B12 when loaded: p5_regs.s callit:2-13).

## 4. What the fourth sandbox can prove

- **asm6x alone:** symbol alphabet, directive spelling and arity, mnemonic existence, operand shape and immediate range, unit assignability (incl. one cross path per unit), register-pair parity, predicate registers, `.align` power of two, `NOP 1-9`. It does *not* check delay-slot sufficiency (a missing NOP assembles), branch reach, ABI, or that a name exists. Best fixed in the backend: 12, 13, 15, 16. Nothing in the sweep's classes should be accommodated by the assembler; "Operand #2 missing"/"Commas must separate operands" are most likely the dot fallout (`B L.wide.wide0` parsed as `L` + `.wide`) and should be re-judged after the respelled sweep.
- **lnk6x + rts6740_elf.lib + a .cmd** (needs MEMORY/SECTIONS, `--rom_model`, `--stack_size`, `--heap_size`): item 19's list, `.weak` semantics (17), orphan placement of `.vm6747.eh`, `.init_array` model, branch reach/trampolines. `dis6x` on the `.out` then shows the chosen encodings (e.g. that `ADD A4, 31, A4` went to .D).
- **Execution:** no simulator in CCS7, so delay slots, hazards and the boundary ABI remain the emulator's — hence item 14, and a fifth cross-check worth doing: make vm6747 read cl6x's own `.asm` (it already takes `||` and unit specifiers, Asm.cpp:125-145, and `CALLP`, Cpu.cpp:229-233; missing are `RETNOP`, `BNOP`, and ignoring `.dwtag/.dwattr/.dwpsn/.dwcfi/.dwendtag/.dwendentry/.dwfde/.battr`, Asm.cpp:367-371) so TI-compiled `mk2` can be called from our `main` on the emulator.

## 5. Sandbox symmetry

Windows-built compilers write CRLF (`std::ofstream` text mode, Compiler-Ci `src/Driver.cpp:612`, Compiler-Cppi `src/Driver.cpp:780`); asm6x and the emulator (Asm.cpp:60 trims `\r`) cope, but text fingerprints taken on the box would differ from the Mac's. AppleDouble: the relays exclude `._*` and use `--no-mac-metadata`/`COPYFILE_DISABLE` (Emulator `tests/windows.sh:36-40`, RStudio `tools/to-windows.sh:56-70`), but vm6747's directory mode filters only names with spaces (main.cpp:59-61), so a `._X.s` arriving by any other route would be assembled and fail. Case: no same-extension collisions in the three corpora or the runtime (checked); only Linux is case-sensitive. `$` in symbols is safe for asm6x, the emulator, and the suites (no script greps symbol names; no Makefile carries one).


---

# Third review - binary compatibility of cpp11's C6000 objects with cl6x's (Fable 5.1, 2026-09-15)

**Question.** Can an object cl6x produces and one cpp11 produces be linked together and run on a C6747: call, be called, throw through, catch from, inherit from, share globals and layouts. Instruction text is not compared; the ABI is.

**Method.** Nine probe translation units, each compiled by `cl6x -mv6740 --abi=eabi -O0 -k --exceptions --rtti` (TI CGT 8.2.2 on the Windows box) and by `cpp11 -S -arch tms6747`, our `.s` assembled by `cl6x -c`, mixed links by `lnk6x` against `rts6740_elf_eh.lib`, objects read with `nm6x`/`ofd6x`; TI's runtime sources (`lib/src/tdeh_*.cpp`, `tdeh_uwentry_c6000.asm`, `autoinit.c`, `boot.c`, `guard.cpp`, `rtti.cpp`, `vec_newdel.cpp`, `stdarg.h`) read where a fact is not in the text. Everything is under `scratchpad/ti/review3/` (abbreviated **R3** below): probes `cc.cpp` (calling convention), `layout.cpp`, `mangle.cpp`, `sect.cpp` (+`sect-ext.cpp`, `tm2.cpp`), `eh2.cpp`, `init.cpp`, `newdel.cpp`, `dp-ti.c`/`dp-ours.cpp`/`dp-link.cmd` (data-page test), `probe2.cpp`/`probe2b.c`/`probe3.cpp` (hidden-pointer order, extern addressing), `extra.cpp` (cl6x only), `vbase.cpp` (emulator); our output `<probe>.s`; TI's output and every log, map and object dump in `R3/out/` (`<probe>-ti.asm`, `*.lnk`, `*.map`, `*.nm`, `*.ofd`); TI's sources and the two library symbol lists in `R3/rts/` (`eh-syms.txt` is `nm6x -g rts6740_elf_eh.lib`). The box build scripts are `R3/build.cmd`, `build2.cmd`; the box copy is `C:\Users\GRA\Documents\VM6747\review`. Nothing under `~/Documents/Claude` was changed but this file.

Two facts about the vendor side that frame everything: **cl6x 8.2.2 defaults to no exceptions and no RTTI** (`--exceptions --rtti` must be given, and the shipped `rts6740_elf.lib` is the no-EH build; the EH build is the one mklib made), and **it is a C++03 compiler** (`noexcept`, `override`, `enum class`, `char16_t`, `decltype(nullptr)` are syntax errors - `R3/out/eh2-ti.log`, `extra-ti.log` before the probes were trimmed). A shared header must be C++03 whatever the ABI says.

## Ranked checklist

Severity: **S** = silent wrong behaviour on silicon in a mixed link; **R** = the link is refused; **C** = conformance, no consequence found. Effort: S/M/L.

### T1. [S] Cleanup landing pads resume with `_Unwind_Resume(A4)`; TI's contract is `__cxa_end_cleanup()` and A4 is not set for a cleanup
- Area: exceptions. Probe: `R3/eh2.cpp` (`cleanup_only`, `conditional_cleanup`, `nested`, `ret_in_catch`, every function with a destructor-bearing local), and `R3/../eh.cpp` from the second review.
- cl6x: every cleanup pad ends `CALLP .S2 __cxa_end_cleanup,B3` (`R3/out/eh2-ti.asm:1668, 2011, 2572, 2595, 2616`, `eh-ti.asm:490, 662`); `__cxa_end_cleanup` takes no argument. TI's personality, `tdeh_pr_common.cpp` `process_cleanup` (scratchpad/ti/tdeh, line ~265): for a cleanup it calls `__cxa_begin_cleanup(uexcep)` (records the exception in `__cxa_eh_globals::cleanup_exception`) and `__TI_targ_regbuf_set_pc(context, pad)` - *nothing else*; only `process_catch` (line ~394) calls `__TI_targ_setup_call_parm0(context, uexcep)`. `__TI_Install_CoreRegs` (`tdeh_uwentry_c6000.asm:270ff`) then loads A4 unconditionally from the register buffer (`LDW *A4[_Unwind_Reg_Id._UR_A4], A4`), and `_Unwind_RaiseException` (`:58-175`) never stores A4 into that buffer (it stores B3, DP, B10-B13, A10-A15, SP). So at one of our cleanup pads under TI's runtime A4 is an uninitialised stack word. `__cxa_end_cleanup` (`:385-460`) fetches the exception from `cleanup_exception` via `__TI_cxa_end_cleanup()` and only then calls `_Unwind_Resume`.
- ours: `R3/eh2.s:904, 1042, 1132, 1194` (`B _Unwind_Resume` after loading the pad's stored A4); the backend's `landingPad` stores A4 for every pad (`Compiler-Cppi/src/backend/Tms6747.cpp` `landingPad`), the parser emits the `_Unwind_Resume` call (`src/parser/ParserStmt.cpp` and `ParserClass.cpp`, `runtimeCall("_Unwind_Resume", ...)`). The emulator's `_Unwind_Resume` takes the exception as its argument (`Emulator/src/Runtime.cpp:1043-1049`), which is why 281/281 pass there.
- Consequence: the first exception that unwinds through any cpp11 frame with a destructor on silicon calls `_Unwind_Resume` with garbage - a crash or a wrong resumption. This is the one item that breaks every C++ program with exceptions, not a corner.
- Fix: on this target end every cleanup pad with `__cxa_end_cleanup()` (no argument, never returns) instead of `_Unwind_Resume(ptr)`; the pad need not store A4 for a cleanup at all. Teach the emulator `__cxa_end_cleanup` (resume the exception the last cleanup landing recorded). A catch handler that falls off its end stays as it is (`__cxa_end_catch`). Effort: S (parser hook + one native).

### T2. [S] A class that is non-trivial for calls is returned through a hidden pointer in **A4, as the first parameter, before `this`** - not through A3
- Area: calling convention. Probes: `R3/cc.cpp` (`rctor`, `rdtor`, `rpoly`, `useall`), `R3/probe2.cpp` (`rhc`, `ahc`), `R3/probe3.cpp` (`M::mc`, `M::vmc`, `M::mc2`, `free2`, `useM`).
- cl6x: `_Z5rctorv: MVK 3,A3; STW A3,*A4(0)`; `_Z5rdtorv: STW A3,*A4(0)`; `_Z5rpolyv: STW B4,*A4(0) ... STW B4,*A4(4)` (a class with a vptr counts); `_Z3rhcv: STW B4,*A4(0)` (a class holding such a member counts) - `R3/out/cc-ti.asm:3717-3860`, `probe2-ti.asm`. A member function: `_ZNK1M2mcEi: LDW *B4(4),B4; ADD A6,B4,A3; STW A3,*A4(0)` - A4 the result, B4 `this`, A6 the first explicit argument; `_ZNK1M3mc2Ei4Ctori`: A4 result, B4 `this`, A6 `x`, B6 `&y` (a non-trivial argument goes by the address of the caller's copy), A8 `z`; `_Z5free2i4Ctori`: A4 result, B4, A6 `&y`, B6 (`R3/out/probe3-ti.asm`). The trivial cases keep A3: `_ZNK1M3m12Ei` has `this` in A4, `x` in B4, the result pointer in A3 with the `[!A0]` null test; `_Z3rcdv` likewise. Who qualifies is the Itanium rule "non-trivial for the purposes of calls": a non-trivial copy/move constructor or destructor - user-provided, or implicit but non-trivial because the class is polymorphic or has a virtual base or such a member/base. `DefOnly` (user default constructor only) is trivial and comes back in A4 by value (`_Z4rdefv: LDW *SP(4),A4`).
- ours: `R3/cc.s` `_Z5rctorv`/`_Z5rpolyv` store A3 into the sret slot (`SUB A15,A0,A0; STW A3,*A0`); `_Z6useallv` passes it: `SUB A15, A0, A3; B _Z5rctorv` (cc.s:5857-5860), `... A3; B _Z5rpolyv` (5973-5976); `R3/probe2.s` `_ZNK1M2mcEi` stores A3 (sret), A4 (`this`), B4 (`x`) in that order; `useM` sets A3 and B4 before `B _ZNK1M2mcEi`. The backend keys on `inPair`/`sret` with A3 for everything indirect (`Tms6747.cpp` `visit(const Call&)`, `visit(const Return&)`, `emitFunction`'s `sretSlot_`).
- Consequence: both directions are wrong for every class with a user-provided copy constructor or destructor, every polymorphic class, and every class containing one: our callee writes the result through whatever was in A3, TI's callee writes through what we put in A4 (the first argument or `this`), and every explicit argument sits one register off.
- Fix: in the backend, when the returned class is non-trivial for calls, take the hidden pointer as an extra first parameter (A4, shifting `this` and the rest), never null-test it, and on the caller side pass it in A4 the same way; keep A3 with the null guard for trivially copyable classes over 8 bytes. `inPair` already asks `nonTrivialCopy()`/`hasDestructor()`; the polymorphic/virtual-base case must count as non-trivial too. Effort: M.

### T3. [S] Sub-word stack arguments are packed at their natural alignment, not a word each
- Area: calling convention. Probe: `R3/cc.cpp` `st1` (20 parameters) / `callst1`.
- cl6x caller `_Z7callst1v`: `STW A3,*SP(4)` (k), `STDW A5:A4,*SP(8)` (l, long long), `STW A3,*SP(16)` (m), `STDW B5:B4,*SP(24)` (n, double), `STB A3,*SP(32)` (o, char), `STH B8,*SP(34)` (p, short), `STW B9,*SP(36)` (q, S3 as a word), `STDW B7:B6,*SP(40)` (r, S8), `STW B16,*SP(48)` (&s, S12 by reference), `STW B4,*SP(52)` (t, float); callee `_Z3st1...`: `LDB *+A3[FP]` with A3=32, `LDH *FP(34)`, `LDB` at 36, `LDNDW *+FP[5]` (40), `LDW *FP(48)`, `LDW *FP(52)` (`R3/out/cc-ti.asm:2208-2427`).
- ours: `R3/cc.s` `_Z3st1...` reads at `A15+4, 8, 16, 24, 32, 36, 40, 48, 56, 60`; `_Z7callst1v` writes at `B15+4, 8, 16, 24, 32, 36, 40, 48, 56, 60` (`stackParamOffset`, `visit(const Call&)`: `end += wide ? 8 : 4`).
- Consequence: from the first `short`/`char`/`bool` that follows another sub-word argument on the stack, every later argument is read from the wrong place. Needs more than ten parameters (variadic extras are promoted, so `printf` is unaffected).
- Fix: `at = align(end, alignof(t)); end += sizeof(t)` for scalars (1/2/4/8); a struct of 1-4 bytes takes a 4-aligned word, 5-8 an 8-aligned doubleword, larger ones a pointer word - as now. Mirror in `stackParamOffset`. Effort: S.

### T4. [S] A call with a 64-bit 7th-10th argument clobbers A11/B11/A13/B13 without saving them
- Area: calling convention, callee-saved registers. Probe: `R3/cc.cpp` `callpr3` (doubles as the 9th and 10th arguments).
- cl6x `_Z7callpr3v`: `STW B10,*SP++(-8); STDW B13:B12,*SP++(-8); STDW A13:A12,*SP++(-8)` before `MVKH 0x40240000,B13` / `MVKH 0x40220000,A13`, and restores them after (`R3/out/cc-ti.asm:2722-2760`); `keep` and `sc2` show TI treating A10-A15 and B10-B15 as callee-saved (each saved only when the function writes it).
- ours: `R3/cc.s` `_Z7callpr3v` prologue saves `B12, B10, B3, A12, A10` only, then `MV A4, B12; MV A5, B13` and `LDDW *B15, A13:A12` (`moveValue`/`popValue` write the pair; `usesSavedArgRegs_` covers A10/B10/A12/B12 only; `savedRegs()`/`unwindWord`).
- Consequence: a TI (or our) caller keeping a value in A11/A13/B11/B13 across a call into our code that makes such a call reads garbage afterwards. Narrow trigger, silent.
- Fix: when a wide argument lands in A10/B10/A12/B12, save its partner too (bits: A11=1, A13=3, B11=7, B13=9 in the pr3 mask, TI's pop order A15,B15..B10,B3,A14..A10); grow the fixed save area from 24 to 40 bytes (or 48 for 8-alignment) so `localAddr` still needs no knowledge of the body. Effort: S/M.

### T5. [S] Scalar globals are not in TI's near sections; TI code reaches every scalar through DP (B14)
- Area: sections / memory model. Probes: `R3/sect.cpp`, `R3/dp-ti.c` + `dp-ours.cpp` + `dp-link.cmd`, `R3/probe2.cpp` (`readx`, `writex`).
- cl6x: `int gi = 7` -> `.sect ".neardata", RW`; `int gz` -> `.nearcommon gz,4,4`; `static int sz` -> `.bss sz,4,4`; `extern const int egc = 11` -> `.sect ".rodata"`; `double gd`, `long long gll`, `short gs`, `char gch`, `bool gb`, pointers -> `.neardata`; `Tm<int>::count` -> `.sect ".neardata:_ZN2TmIiE5countE"` (`R3/out/sect-ti.asm:59-470, 694`). Arrays, structs, classes -> `.fardata:name`/`.usect ".far:name"`/`.const:name`, 8-aligned. Every scalar, const or not, declared or defined, is addressed DP-relative: `LDW *+DP(gi)`, `LDW *+DP(ec)`, `LDW *+DP(ecd)`, `STW A4,*+DP(ei)`, `STB A4,*+DP(ech)` (`sect-ti.asm:849-928`, `probe2-ti.asm` `_Z5readxv`, `_Z6writexi`); aggregates absolute: `MVKL garr+1`, `MVKL es`.
- ours: initialised scalars in `.data` (`R3/sect.s:70-104`, `emitGlobal`), constants in `.const`, zero-filled in `.bss` (correct: TI's `.bss` is near).
- Proof: `dp-ti.c` (TI) reading `gi/gz/gd` defined by `dp-ours.cpp` (ours), linked with `dp-link.cmd` (near sections at 0xC0000000, everything else at 0xD0000000): `warning: Section ".data" requires a STATIC_BASE relative relocation, but is located at 0xd0108388, which is probably out of range of the STATIC_BASE. STATIC_BASE is located at 0xc0000000.` (`R3/out/dp-split.lnk`) - a warning, the link succeeds (`LINKED_SPLIT`), the 15-bit field is truncated. With the flat one-RAM command file it happens to be in range (`dp-flat.lnk` clean), which is why the fourth sandbox never saw it. TI's default linker command files group `.bss/.neardata/.rodata` under `__TI_STATIC_BASE`; `.data` is not in that group.
- Consequence: on a real link map every TI access to one of our scalar globals (and to an `extern const` scalar, and to a template static member) reads or writes the wrong address, silently. Our own code is unaffected (absolute addressing, B14 never touched).
- Fix: in `emitGlobal`, for a non-aggregate object: `.sect ".neardata", RW` when initialised, `.bss` (as now) when zero, `.sect ".rodata"` when const; aggregates may stay in `.data`/`.const` (TI reaches them absolutely) or, to match cl6x, go to `.fardata:sym`/`.usect ".far:sym"`/`.const:sym`. Effort: S. (Whether `.rodata` and `.neardata` are per-symbol sections is irrelevant to reach; keep `.bss sym,size,align`.)

### T6. [R] Template static data members are strong `.global` definitions
- Area: symbols. Probe: `R3/sect.cpp` + `R3/tm2.cpp` (both instantiate `Tm<int>::count`/`value`).
- cl6x: `.global _ZN2TmIiE5countE` inside `.sect ".neardata:_ZN2TmIiE5countE", RW` with `.group "_ZN2TmIiE5countE", 1` / `.gmember` (COMDAT), `nm6x` binding `?` (`R3/out/sect-ti.asm:693-698, 1896ff`, `sect-ti.nm`).
- ours: `.global _ZN2TmIiE5countE` in `.data` (`R3/sect.s:159`), `nm6x` `D` (`R3/out/mangle-ours.nm`: `D _ZN3ArrIiLi3EE5countE`).
- Proof: `lnk6x sect-ours.obj tm2-ours.obj`: `error: symbol "Tm<int>::value" redefined: first defined in "sect-ours.obj"; redefined in "tm2-ours.obj"` and the same for `count` (`R3/out/tm-oo.lnk`); TI+ours links because the group yields (`tm-to.lnk`, `tm-ot.lnk` clean).
- Fix: mark implicitly instantiated data (template static members) `isInline` so `emitGlobal` writes `.weak` as it does for vtables and typeinfo. Effort: S.

### T7. [R] `extern const int egc = 11;` is not emitted at all
- Area: symbols / front end. Probe: `R3/sect.cpp` (`egc`), earlier `dp-ours.cpp` (`gc`, removed to get the link through).
- cl6x: `.global egc / .sect ".rodata" / .align 4 / egc: .bits 11,32` (`R3/out/sect-ti.asm:112-117`).
- ours: no definition; `R3/sect.s:1441` is `.ref egc` (the post-pass declaring what is used and not defined). `lnk6x`: `undefined symbol egc, first referenced in sect-ours.obj` (`R3/out/sect-ours.lnk`, `dp-flat.lnk` before the probe was changed).
- Fix: `extern` on a definition of a const object keeps external linkage; emit it (into `.rodata` per T5). Effort: S.

### T8. [S] `wchar_t` is mangled as `t` (unsigned short), not `w`
- Area: name mangling. Probe: `R3/mangle.cpp` (`f_wchar`, `tmax<wchar_t>`).
- cl6x: `_Z7f_wcharw`, `_Z4tmaxIwET_S0_S0_` (`R3/out/mangle-ti.nm`).
- ours: `_Z7f_wchart`, `_Z4tmaxItET_S0_S0_` (`R3/out/mangle-ours.nm`; `Tms6747.h` `wcharType() = Kind::UShort` reaches the mangler).
- Consequence: any function or template taking `wchar_t` fails to link across the boundary (or, worse, resolves to a `wchar_t`-vs-`unsigned short` overload).
- Fix: keep `wchar_t` a distinct kind through the mangler (`w`), sizing it from the target; this is `tests/open/wchar-distinct` seen from the link. Effort: S.

### T9. [S] Virtual inheritance is layout-only: no secondary vtables, VTT, virtual thunks, construction vtables, deleting destructor; base offsets dropped from static initialisers - CLOSED 2026-09-15 (the initialisers that afternoon, the rest that night; every table below now matches cl6x's, see TMS6747.md)
- Area: class layout / vtables. Probes: `R3/layout.cpp` (`V5`, `V6`, `V7`, `v43`, `v51`, `v71`, `v73`, `v76`), `R3/mangle.cpp` (`VB`, `VD`), `R3/vbase.cpp` on the emulator.
- cl6x: `_ZTV2V5` = `8, 0, _ZTI2V5 | 0, -8, _ZTI2V5, _ZN2V11fEv` plus `_ZTT2V5` = `_ZTV2V5+12, _ZTV2V5+24`; `_ZTV2VD` = `8, 0, _ZTI2VD, _ZN2VDD1Ev, _ZN2VDD0Ev | -8, -8, _ZTI2VD, _ZTv0_n12_N2VDD1Ev, _ZTv0_n12_N2VDD0Ev`, `_ZTT2VD`, plus `_ZN2VDD0Ev` and the two `_ZTv0_n12_` thunks as symbols (`R3/out/layout-ti.asm`, `mangle-ti.asm:118-145`, `mangle-ti.nm`); `V7`'s group carries `_ZTV2V5__2V7`, `_ZTV2V1__2V5__2V7`, ... construction vtables. Pointer constants: `v43: .bits v4 + 8`, `v51: .bits v5 + 8`, `v71: v7 + 20`, `v73: v7 + 28`, `v76: v7 + 8`.
- ours: `_ZTV2V5` = `8, 0, _ZTI2V5` and nothing else; `_ZTV2VD` = `8, 0, _ZTI2VD` - no destructor slots, no `_ZN2VDD0Ev`, no thunks, no VTT (`R3/layout.s`, `R3/mangle.s`, `mangle-ours.nm`); `_ZN2VDC1Ev` calls `_ZN2VBC2Ev`, which points the `VB` subobject at `_ZTV2VB+8`. `v43: .word v4`, `v51: .word v5`, `v71/v73/v76: .word v7` - the derived-to-base adjustment is missing from every static initialiser, virtual or not.
- Proof on the emulator: `R3/vbase.cpp` prints `~VB` alone and faults `free of a pointer malloc did not return` at `delete h` (host: `2332`, `~VD ~VB ~VD ~VB`): a call through the virtual base reaches the base's function, and `delete` through it frees the wrong address.
- Consequence: any class with a virtual base is wrong on its own and cannot cross the boundary in either direction (TI calls `_ZN2VDC2Ev(this, vtt)` for a base subobject - a second parameter ours does not take); any MI base pointer initialised statically points at the wrong subobject.
- Fix: the Itanium virtual-base machinery (vbase-offset slots, secondary vtables with `_ZTv` thunks, VTT, C2/D2 with the VTT parameter, construction vtables, `D0` for every class whose destructor is virtual) and the base adjustment in constant initialisers. Effort: L. (The first review's "virtual bases work" was `sizeof` only.) A related refusal: `&MD::vf` for a virtual inherited through MI is a parse error ("'MD' was not declared").

### T10. [S] `throw()`/`noexcept` functions carry no exception-specification descriptor
- Area: exceptions. Probe: `R3/eh2.cpp` `noexc` (`throw()`), `noexc_call`.
- cl6x `__c6xabi_extab$_Z5noexci`: a cleanup row, the catch-and-terminate scope over the pad, then `.half len + 0; .half off + 2 + 1; .ulong 0 (rtti count); .ulong 0` and `.symdepend "__cxa_call_unexpected"` - a function-exception-specification descriptor (key offset-bit 1, count 0) over the whole body, which makes any escaping exception call `__cxa_call_unexpected` (`R3/out/eh2-ti.asm`, `tdeh_pr_common.cpp` `process_fespec`).
- ours: `_Z5noexci` has the cleanup row only (`R3/eh2.s`); the direct `throw` in a `noexcept` body aborts (first review A11), a callee's exception passes through.
- Consequence: an exception from a TI-compiled callee propagates through our `noexcept` frame to an outer handler instead of terminating; TI's semantics (and the standard's) differ.
- Fix: emit the fespec descriptor (offset low bit set, count 0) covering the body of a `noexcept`/`throw()` function; teach the emulator's runtime the descriptor kind (terminate). Effort: S/M.

### T11. [R] `__cxa_get_exception_ptr` does not exist in TI's runtime
- Area: exceptions. `src/parser/ParserStmt.cpp:1340` calls it for a catch-by-value of a class with a copy constructor; `R3/rts/eh-syms.txt` has no such symbol; `Emulator/tests/ti-nolink.txt` names the two cases.
- cl6x: copies from what `__cxa_begin_catch` returns (`R3/out/eh2-ti.asm:2424ff`: `CALLP __cxa_begin_catch` then the object is used through A4) - `__cxa_begin_catch` returns the adjusted object pointer (`tdeh_cpp_abi.cpp:452-500`, `barrier_data.data[0]`).
- Fix: on this target construct the copy from `began`, as for the trivial case. Effort: S.

### T12. [C] Duplicate catch rows on nested tries
- `__c6xabi_extab$_Z6nestedi` lists `Derived, Base, Derived, Base, Base*, Other, ...` for the inner region (`R3/eh2.s:1339-1366`); cl6x lists each once. TI's personality scans in order and stops at the first match, so this is only size. Effort: S (Walker: the inner region's own types are appended twice when the enclosing region's chain is added).

### T13. [C] Inline functions, vtables and typeinfo are `.weak` in `.text`/`.const`, not per-symbol sections in a group
- Proven harmless at the link (`scratchpad/ti/weak/`: two `.weak w` definitions link, `nm6x` shows one `W w`; a `.weak` beside a `.global` takes the `.global`; `R3/out/mangle-ours.nm` `W` against TI's `?` links in every mixed probe). Every TU keeps its own body of every inline function and vtable; TI folds them through `.sect ".text:sym"` + `.clink` + `.group`/`.gmember`. Size only. Effort: M if wanted.

### T14. [C, c90 only] Uninitialised globals are common symbols in TI's output
- `.nearcommon gz,4,4`, `.farcommon zarr,16,8` even for C++ (`R3/out/sect-ti.asm:75, 291`); ours `.bss gz,4,4` is a definition. No C++ consequence; in C two TUs with `int x;` link under cl6x and would clash under c90's `.bss`. Note for the C sibling.

### Cxx1i front-end limits met while probing (not ABI, recorded for the author)
- a member of a struct returned by a *virtual* call: "this address is not supported yet" (`c.vbig().c`); `p != 0` on a member pointer, and a member-function-pointer call through a typedef of a class in a nested namespace: "target: no size for this type yet (tms6747)"; `int (M::*pmv)() = &M::vf;` at file scope: "needs a braced initialiser"; `extern const` definitions dropped (T7); `va_arg` of an aggregate refused (the backend has the code); `offsetof` and dynamic initialisers of scalars refused as non-constant; `volatile int *` and pointers to const member functions refused; `enum class`, `char16_t`, function-try-blocks absent. Each is in the probe sources' history (`R3/*.cpp` were trimmed to what both compilers take).

## Confirmed correct (measured, not inferred)

- **Argument registers** A4, B4, A6, B6, A8, B8, A10, B10, A12, B12 in order; a 64-bit value takes the pair of its register (`_Z3sc2ixidifxiidi`: B5:B4, B7:B6, A11:A10, B13:B12; `_Z3pr1ix`: B5:B4; `_Z3pr3iiiiiiiidd`: A13:A12, B13:B12); `float` in a single register (B8).
- **Stack arguments** from SP+4, the word at SP the callee's (`STW FP,*SP++(-16)`), 8-byte values 8-aligned with a hole (`odd`: k@4, l@8; `odd2`: char@4, long long@8, short@16); a struct of 1-4 bytes in a word (S3 at 36), 5-8 in an 8-aligned doubleword (S8 at 40), larger by the address of a copy (S12 at 48). SP stays 8-aligned. Except T3.
- **Struct arguments and returns** of 8 bytes or less, trivially copyable: by value in A4 / A5:A4, the register holding the memory image (3-, 5-, 7-, 9-byte structs `r3/r5/r7/r9`, `a3..a9`, `useOdd`, `S6`, `SI8`, `SFF`, `U8`, `SLL`, `SD`); larger trivially copyable ones through A3 with `[!A0]` null test in the callee and `ZERO A3` from a caller that discards the result (`_Z7discardv`); `this` in A4; an empty class takes a register (`aempty`: x in B4; ours `MV B4, A4` too). A class with only a user-provided default constructor is trivial for calls (`rdef`, `adef`). Except T2.
- **Variadic**: the last named parameter and everything after it on the stack (`va1`: n at SP+4; `va2`: a in A4, b in B5:B4, c at SP+4), `va_list` is `char *` in TI's `stdarg.h` and ours (mangled `Pc` both: `_Z5vlistPKcPc`), `va_start` = past the last named, `va_arg` aligns to 4 or 8 and reads aggregates over 8 bytes through a pointer word (`R3/out/extra-ti.asm` `_Z3va1iz`), small ones by value (S3 in 4 bytes, S8 in 8).
- **Callee-saved** A10-A15, B10-B15 (`keep`, `sc2`); B3 the return address; the prologue/epilogue shape with A15 at the caller's word; pr3 unwind word: return register nibble 7 = B3, `0x7f` SP:=A15, pop mask bits A15=12 ... B3=5 ... A10=0 read by `process_unwind_pr3_pr4`/`process_unwind_pop_bitmask` a word each downward from the restored SP - matches `savedRegs()`/`unwindWord()`; TI itself uses pr3 for FP frames and pr4 elsewhere, both accepted.
- **Member pointers**: data = byte offset, null = -1 (`pm: 2`, `pmd: 8`, `pmnull: -1` both); function = `{ptr, adj}` 8 bytes, virtual = vtable offset + 1 (`{1, 0}` for `&M::vf` both), dispatched by `AND 1` / `ptr-1` through the vptr (`_Z2mpM2S8iM4PolyFivERS1_RS_`), passed in a register pair (B5:B4).
- **Data layout**: every size and alignment in `layout.cpp` identical (`bfsz`, `sz`, `al`, `esz`, `fund`, `csz2`, `hasarr`, `vtsize`): `bool` 1, `wchar_t` 2 unsigned, `char` signed, `long` 4, `long long` 8 align 8, `double`/`long double` 8 align 8, pointers 4, enums 4, `sizeof(L"ab")` 6, member pointers 4/8; ten bit-field structs identical bit for bit (`R3/out/extra-ti.asm` `.bits` against our `CLR` ranges, including a 40-bit field at bit 18 of a 64-bit unit and `long long a:33; int b:3` at bit 33); `char c; double d` = 16; unions; arrays inside structs unaffected by `--array_alignment`. TI aligns its own global arrays to 8 and does not assume it for `extern` arrays at -O2 (`R3/out/probe2b-ti.asm`: `LDW *B4(4)` pairs, no `LDDW`, on `extern int earr[4]`), so ours aligning to the element is compatible as far as measured.
- **Vtables and RTTI** for single and non-virtual multiple inheritance: `[offset-to-top, RTTI, fns]` with 4-byte slots, secondary vtable with `-8` and `_ZThn8_` thunks (`_ZTV2V4`, `_ZThn8_N2MI2fbEv` both); `_ZTI` = `_ZTV<abi>+8, _ZTS[, base | flags, count, {base, offset<<8|flags}]` with identical `2050`/`-3069`/`-4093` words; `__class_type_info`/`__si_class_type_info`/`__vmi_class_type_info`/`__pointer_type_info`/`__enum_type_info`/`__fundamental_type_info` vtables and `_ZTIi`, `_ZTIPKc`... are in `rts6740_elf_eh.lib` (`R3/rts/eh-syms.txt`); `typeid` reads vptr[-1], compares by `_ZNKSt9type_infoeqERKS_`, `__cxa_bad_typeid` on null (`tname`, `tsame`); TI's `__cxa_type_match` compares typeinfo *or name-string addresses*, satisfied once the linker resolves `_ZTI`/`_ZTS` to one definition (weak against TI's group: yes).
- **dynamic_cast**: `__dynamic_cast(obj, src, dst, hint)` with A4/B4/A6/B6 = -1 (ours also fills B7 with a 64-bit hint's high word - ignored); cast to `void*` via offset-to-top at vptr[-2] (`cast_void`); cross cast the same call.
- **new/delete**: `_Znwj`, `_Znaj`, `_ZdlPv`, `_ZdaPv` (`j` since A3); array cookie = max(4, alignof(T)) with the count in its last word (TI: `__cxa_vec_new2(n, size, padding=4|8, ctor, dtor, _Znaj, _ZdaPv)` - `_Z2mki` padding 4, `_Z3mk8i` padding 8), so an array allocated on one side is deleted correctly on the other; `__cxa_pure_virtual` in the abstract slot, defined by the rts; `__cxa_bad_cast`, `__cxa_bad_typeid` defined.
- **Static locals and static objects**: `__cxa_guard_acquire/release` with the first-byte protocol (TI's guard is 4 bytes, `LDB` test before the call; ours 8 - a superset); `__cxa_atexit(dtor, obj, &__dso_handle)`, `__dso_handle` a common symbol in the rts; `.init_array` entries (`.field f, 32` vs our `.word f` - the same bytes) collected between `__TI_INITARRAY_Base/Limit` and run by `_c_int00 -> AUTO_INIT -> run_pinit` before `_args_main -> main` (`autoinit.c`, `boot.c`, `R3/out/init-ours.map`); `exit` runs `__TI_dtors_ptr`, the `__cxa_atexit` list. Under `--rom_model` our `.data` gets its own cinit record (`(.cinit..data.load) rle`, `R3/out/dp-flat.map`) and `.bss` its zero record: our globals are initialised on silicon.
- **Constructor/destructor entry points**: TI calls `C1`/`D1` for a complete object and `C2`/`D2` for a base subobject (`_Z6useExti`, `_Z7useExtDi`); out-of-line ones are defined under all three names; ours calls and defines the same (`R3/probe2.s:1756-2578`).
- **Mangling** of everything else in `mangle.cpp`: `size_t` `j` (`_ZN2OpnwEj`), `ptrdiff_t` `i`, `long` `l`/`m`, `long long` `x`/`y`, `bool` `b`, `long double` `e`, nested namespaces, class and function templates with type and int arguments, operators, const member functions, references, pointers to functions/arrays, member pointers (`M2S8i`, `M4PolyFivE`), ellipsis `z`, enums, thunks, `_ZZ...E` static locals - identical symbol sets in `R3/out/mangle-{ti,ours}.nm` apart from T8, the missing virtual-base symbols (T9), and names that are internal on one side (anonymous namespace, string literals).
- **Exception tables**: `.c6xabi.exidx`/`.c6xabi.extab:f` forms, `__c6xabi_extab$f` naming, `$EXIDX_FUNC`/`$EXIDX_EXTAB`/`$EXTAB_SCOPE`/`$EXTAB_LP`/`$EXTAB_RTTI`, catch descriptors (`len+1`, `off+2`, pad, rtti or `0xffffffff`), cleanup descriptors, the `0, 0xfffffffe` terminate scope over a cleanup pad (TI has it over every cleanup pad too), `__c6xabi_unwind_cpp_pr3` via `.symdepend`; a catch handler lands with the `_Unwind_Exception` pointer in A4 and calls `__cxa_begin_catch` on it; `__cxa_throw(obj, tinfo, dtor)`, `__cxa_allocate_exception`, `__cxa_rethrow`, `__cxa_end_catch` all present and called the same way; the linked exidx table is sorted by function address across TI's and our objects (43 entries decoded from `R3/out/dp-flat.out`, monotonic) because lnk6x orders exidx input sections by their text and our entries are one section in `.text` order - keep every function in the one `.text`.
- **Assembler-level**: `.bss sym,size,align`, `.sect`, `.align`, `.word`/`.short`/`.byte`, a double as two words low first (TI: `.word 000000000h,03ff80000h`), `.space`, `.word sym+n`, `.weak`, `.ref`, `$`-spelled labels, `.symdepend` - all taken by `cl6x -c` for all nine probes (`R3/out/*-ours.log` empty); `__c6xabi_divi/divu/remi/remu/divlli/divull/remlli/remull/divd/divf/fixdu/fixfu/fix{f,d}{lli,ull}/flt{llif,llid,ullf,ulld}` and `__c6xabi_errno_addr`, `__c6xabi_abort_msg` all exported by the rts.

## Not measurable without silicon, and how the emulator could measure it

TI's tools assemble, link and dump; they do not run. What stays unproven is behaviour: T1 (A4 at a cleanup pad), T2 (which register the caller wrote), T3/T4 (which stack word or register the other side reads), T5 (a truncated DP relocation), T10 (an escaping exception). The cheapest oracle short of a board is the one the second review proposed: make `vm6747` read cl6x's `.asm` - it already takes `||`, unit specifiers and `CALLP`; it needs `RETNOP`, `BNOP`, `ADDKPC`, `LDNDW/STNDW`, `*+FP[reg]`/`*SP(n)` forms, `.asg`, `.bits`, `.field`, `.usect`, `.nearcommon/.farcommon`, `.elfsym`, `.group/.gmember/.endgroup`, `.battr`, and to ignore the `.dw*` directives - and a real DP: `B14 = __TI_STATIC_BASE` with `*+DP(sym)` resolved by the assembler as `sym - __TI_STATIC_BASE` against a fixed near-section base. Then each probe becomes a run: link `cc-ti.asm`'s callers against our callees and the reverse (`useall` split across the two), `eh2` with TI's `thrower` and our `nested` and vice versa, `dp-ti.asm` against `dp-ours.s` with the near sections placed far from the data. The natives that stand in for TI's runtime should then be replaced by TI's own `.asm` of `tdeh_*.cpp`, `guard.cpp`, `rtti.cpp`, `vec_newdel.cpp` compiled by cl6x, so that the personality routine's register contract (T1) is TI's code and not our reading of it. The exception index would then also be exercised with TI's own `bsearch` over a table the emulator's assembler must sort as lnk6x does.

## Ranked checklist, one line each

- T1 [S] cleanup pads call `_Unwind_Resume(A4)`; TI lands a cleanup with A4 unset and expects `__cxa_end_cleanup()`.
- T2 [S] non-trivial-for-calls class results: hidden pointer in A4 as the first parameter (before `this`), not A3.
- T3 [S] sub-word stack arguments packed at natural alignment (char@32, short@34), ours a word each.
- T4 [S] a 64-bit 7th-10th argument writes A11/B11/A13/B13 without saving them.
- T5 [S] scalar globals in `.data`/`.const`, out of DP reach for TI code (`.neardata`/`.bss`/`.rodata` expected); lnk6x only warns.
- T6 [R] template static data members are strong `.global`s: "symbol redefined" between two of our TUs.
- T7 [R] `extern const int x = v;` is dropped (`.ref` instead of a definition).
- T8 [S] `wchar_t` mangled `t` not `w`.
- T9 [S] virtual inheritance is layout-only (no secondary vtables, VTT, `_ZTv` thunks, `D0`, C2-with-VTT); static base-pointer initialisers unadjusted; wrong on the emulator too.
- T10 [S] no exception-specification descriptor for `noexcept`/`throw()` functions.
- T11 [R] `__cxa_get_exception_ptr` is not in TI's runtime; copy from `__cxa_begin_catch`'s result.
- T12 [C] duplicate catch rows on nested tries.
- T13 [C] `.weak` bodies are not folded (size only).
- T14 [C] c90: TI's uninitialised globals are common symbols.
