# Known gaps

## main's status is lost in cl6x -O2 code, 2026-09-28

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

## Wrong output from cl6x code at its default optimization, 2026-09-28

Same setup (cl6x 8.2.2, `-n --symdebug:none`, no `-O`), C++Optimize's
`tools/c6747/bench`: `isort.c` prints one garbage byte instead of `isort 344346`, and
`matmul.c` and `sieve.c` print nothing; all three exit 0. `fib.c`, `hash.c` and
`virt.cpp` are right. TI's C6747 cycle-accurate simulator runs all six, built by cl6x
7.4.4 at -O2, with the right output. Not yet narrowed down; recorded, not mended.
