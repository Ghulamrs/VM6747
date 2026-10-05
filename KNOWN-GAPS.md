# Known gaps

## Mended 2026-10-05: cl6x 8.2.2's code at -O2 and at its default level runs

The two entries below are history now. With the software pipelined loop buffer modelled
(`SPLOOP`/`SPLOOPD`/`SPLOOPW`, `SPKERNEL(R)`, `SPMASK(R)`, reload - SPRUFE8 chapter 7) and three
emulator faults found and fixed - `.usect` reserved its alignment as its size and left its label in
the section before it; `MPY32`'s 64-bit form wrote only the low word; a branch issued in another's
delay slots faulted instead of queueing - all ten of C++Optimize's `tools/c6747` programs built by
CCS 7.4's cl6x 8.2.2 print their `.expected` on vm6747, at `-O2` and without `-O`, exit status
included (`structs`' 7). Also new: `CALLRET`, `DINT`/`RINT`, `MVD`, `MPYLI`, `MVC` to and from
ILC/RILC.

Swept 2026-10-05 over Compiler-Ci's 431 cases (the 26 that assume a 64-bit long left out), each built
by cl6x 8.2.2 and held to the host's build of the same C: at `-O2` 374 of 400 now agree where 316 did,
at its default level 380 where 377 did, and none that agreed before disagrees. 32 of the agreeing
`-O2` cases run loops from the buffer - `SPLOOP`, `SPLOOPD` and conditional `SPLOOPW`; reload
(`[cond] SPLOOPD` with `SPMASKR`) is exercised by `matmul` alone.

## What cl6x's code still meets here, 2026-10-05

From the same sweep; none is in the loop buffer, and each stops at assembly or at entry:
- `.nearcommon` / `.farcommon` (cl6x's common symbols): 12 cases;
- TI's hex spelling `03fd55555h` as an operand: 3 cases;
- `PACK2`, `SUBAH` not in the instruction table; `MVK` refusing a constant cl6x writes in range for
  it (`struct_small_return`, `ll_switch_bitfield`);
- `__c6xabi_llshl` and `_ctypes_` not provided by the runtime;
- three cases (`fn_call`, `fn_void_params`, `pd_parenthesised_name`) branch to 0 at entry;
- `ce_unsigned_long_div` and `fp_float_arithmetic` print a different value - not yet narrowed.
The reload model refuses by name two shapes no sample has: a reload with RILC 0, and a reloaded
loop that ends before its reload completes.

## (mended) main's status is lost in cl6x -O2 code, 2026-09-28

Measured from C++Optimize (`tools/c6747-three`), with CCS 7.4's cl6x 8.2.2 compiling to
assembly (`-n -O2 --symdebug:none`) and `vm6747.exe` from RIDE-4.5's bin running it on the
Windows box. Recorded, not mended.

cl6x -O2 ends `main` with `RET B3` and sets the return value in the branch's delay slots -
`structs.c` (`return 7;`) ends

        LDDW    .D2T2   *++SP(8),B11:B10
    ||  RET     .S2     B3
        LDW     .D2T2   *++SP(8),B13
        MVK     .L1     7,A4
        NOP             3

so A4 is 7 when the branch lands, five packets later. vm6747 prints the program's output
correctly and exits **1** - for `return 7` and for `arith.c`'s `return 0` alike. TI's C6747
cycle-accurate simulator runs the same program to C$$EXIT with the right output. At cl6x's
default optimization both programs' exit status is right.

Reproduce: `cl6x -mv6740 --abi=eabi -n -O2 --symdebug:none structs.c`, then
`vm6747 structs.asm; echo %errorlevel%` - C++Optimize's `tools/c6747/programs/structs.c`.

## (mended) Wrong output from cl6x code at its default optimization, 2026-09-28

Same setup (cl6x 8.2.2, `-n --symdebug:none`, no `-O`), C++Optimize's
`tools/c6747/bench`: `isort.c` prints one garbage byte instead of `isort 344346`, and
`matmul.c` and `sieve.c` print nothing; all three exit 0. `fib.c`, `hash.c` and
`virt.cpp` are right. TI's C6747 cycle-accurate simulator runs all six, built by cl6x
7.4.4 at -O2, with the right output. Not yet narrowed down; recorded, not mended.

**Re-measured 2026-10-01** with CCS 7.4's cl6x 8.2.2 at its default optimization, after
the emulator learned `CALLP`'s five protected delay slots (a load issued just before the
call had not been landing before the callee ran - which is how RIDE 4.51's Sample, cl6x's
`LDW *SP(12),A4; CALLP`, read a `this` of 3): `isort`, `matmul` and `sieve` print what they
printed before, so this is not their cause. `fib`, `hash`, `hello`, `arith`, `floats` and
`structs` are right as before. At `-O2` those six stop at the assembler on `SPLOOPD` and
`SPLOOPW`, which the table in `src/Isa.cpp` does not list.
