# vm6747 — the VM6747 emulator

Runs the C6000 assembly that `cc1i` and `cxx1i` emit for `-arch tms6747`,
so that the fourth target can be verified the way the other three are: the
same program compiled twice, run twice, and required to agree.

    vm6747 [-t] [-m megabytes] file.s [more.s ...] [-- args]

The files are assembled together — `.text`, `.data`, `.sect ".const"`,
`.bss sym, size, align`, `.global`, `.weak`, `.align`, `.byte`/`.short`/
`.word`, `.space`, `.string`, labels, predicates `[A1]`/`[!A1]`, parallel
bars `||`, unit specifiers (accepted, ignored), register pairs `A5:A4`, and
the addressing forms `*R`, `*+R(n)`, `*+R[n]`, `*R++(n)` and their kin —
`main` is called with `argc` and `argv`, and the exit status is `main`'s
return or `exit`'s argument. `-t` traces every instruction.

## What it models

**The C674x as the compilers see it.** The two register files, a program
counter, and the pipeline's one visible property: a result lands some cycles
after its instruction issues — 4 delay slots for a load, 3 for a multiply,
3/6/9 for single/double-precision arithmetic, 1 for a floating compare — and
a branch takes effect five execute packets after it. A register read before
its write lands returns the old value. That is the whole point: the NOPs in
the emitted code are the compiler's claim about these latencies, and a
missing one shows up here as a wrong answer, not as a suspicion. The first
run found one (`MPY32` with no delay slots).

The double-precision pipeline is split-phase, as SPRUFE8 draws it: a DP
instruction reads its sources' low words at issue and the high words a cycle
later, and writes its result low word first, a cycle before the high one -
the delay-slot count names the high word. cl6x schedules to exactly that
(`INTDP; NOP 3; MPYDP`); cc1i pads past it, which is why the gap showed only
when TriLab ran cl6x's code here.

Memory effects are immediate at issue, which keeps a store and a later load
in order. Loads and stores fault on misalignment, as the hardware would.
Instructions are executed from their parsed operands; nothing here encodes
or decodes C6000 machine words, and the memory image holds data only. A
code address is still a real number four bytes apart from the next, so
labels, function pointers and the PC behave.

**cl6x's assembly as well as the compilers'.** Since TriLab's CCS leg
(2026-09-18) the emulator runs what TI's compiler writes for the same C: all
sixty-four registers, `.asg`'s FP/DP/SP, `BNOP`/`RETNOP`/`CALL`+`ADDKPC` with
their folded NOPs (unconditional, as the manual says), `ADDAD`, `ANDN`, the
ucst15 address-adds from DP or SP, `.bits`/`.field`, `.group` as a COMDAT
(one definition kept, like `.weak`), `.string` with byte values, and a
directory of `.asm` files. What it does not run is cl6x's C++: STLport's
streams call into TI's compiled runtime, which is machine code.

**The C library, natively.** A call to a library name lands on a stub below
the text base and is answered on the host, reading its arguments by the
convention the compilers emit — A4, B4, A6, B6, A8, B8, A10, B10, A12, B12,
then the stack from B15+4; for a variadic function the last named argument
and everything after it on the stack, 8-byte values at 8-byte boundaries —
and returning in A4 or A5:A4. `printf` and its family, `puts`, `putchar`,
files (`fopen` … `fclose`, kept in memory until closed), `malloc`/`free`,
the string and memory functions, `strtod` and friends, `qsort`/`bsearch`
(calling back into the program), ctype, the C90 math functions,
`setjmp`/`longjmp` (the callee-saved registers, B15 and B3), `signal`/`raise`,
`time`/`mktime`/`strftime`/`localtime`, `setlocale`/`localeconv`, `assert`,
`errno`, and the EABI helpers `__c6xabi_div*`/`rem*`/`fix*`/`flt*`. The three
standard streams are data the emulator contributes before assembly.

## What it does not model

Machine encoding and fetch packets; functional-unit assignment and the
resource rules of a parallel packet; the memory system (caches, EDMA);
interrupts; the 40-bit forms beyond ADD/SUB into a pair; and any instruction
the compilers do not emit and the table in `src/Isa.cpp` does not list —
such a one is an assembly error, by line.

## Verification

`Compiler-Ci/tests/tms6747.sh` runs the whole C corpus through it: 399 of
425 cases agree with clang's native run; the other 26 need a 64-bit long or
an 8-byte pointer on the reference side and are skipped by name
(`tests/tms6747-lp64.txt`). C++ through `cxx1i` runs the same way; the C++
ABI runtime (`__dynamic_cast`, the `__cxxabiv1` typeinfo vtables, `operator
new`, the `__cxa_*` family) is the next piece.

Built like the compilers: C++14, `-Wall -Wextra -Werror -pedantic`, clang++
on the Mac and g++ on the box, objects outside the checkout.
