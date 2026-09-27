@echo off
rem  TriLab's CCS 7.4 leg, on the box: RIDE builds each lab for tms6747 with
rem  c90/cpp11 and vm6747 runs it; TI's cl6x compiles the same sources to
rem  assembly and vm6747 runs that - CCS 7.4 has no simulator, so the emulator
rem  runs both sides. Beside the run, each side is also taken through TI's own
rem  assembler and linker (asm via cl6x -c, lnk6x against rts6740_elf_eh.lib) to
rem  a real .out: the judge accepts RIDE's output, and the sources build as a
rem  real TI program.
rem  Since 2026-09-19 RIDE links the .out itself: asm6x.exe beside it assembles the
rem  .vm's assembly and, told TI's compiler directory (--ti, --tilib), lnk6x links
rem  it - RIDE-OUT below says the .out is there. The cl6x-over-RIDE's-assembly check
rem  stays as the independent reading of the same assembly.
rem  Usage: ccs-leg.cmd <TriLab dir> <RIDEConsole.exe> <vm6747.exe>
rem  Writes <TriLab dir>\out\<lab>-ride-ccs.out, <lab>-ccs.out, and the logs.
setlocal enabledelayedexpansion
if "%~1"==":shard" goto :shard
if "%~3"=="" (echo ccs-leg.cmd: needs the TriLab dir, RIDEConsole.exe and vm6747.exe & exit /b 2)
set LAB=%~1
set RIDE=%~2
set VM=%~3
set CGT=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2
set TILIB=C:\Users\GRA\Documents\VM6747\tilib
set PATH=%CGT%\bin;%PATH%
if not exist %CGT%\bin\cl6x.exe (echo ccs-leg: no cl6x at %CGT% & exit /b 1)
if not exist %TILIB%\rts6740_elf_eh.lib (echo ccs-leg: no rts6740_elf_eh.lib - see Emulator/tests/ti.sh & exit /b 1)
if not exist %LAB%\out mkdir %LAB%\out
rem  Four jobs at once with par.cmd: each lab built by RIDE and each compiled by cl6x - the
rem  candidate and the judge side by side, where the two labs and the two sides once ran in turn.
call "%~dp0par.cmd" 4 "%~f0"
echo CCS-LEG-DONE
exit /b 0

rem  Shards 1 and 2 are the C lab, 3 and 4 the C++ one; the odd shard is RIDE's side, the even cl6x's.
:shard
if %~2 LEQ 2 (set L=c:CC1Lab:cc1lab:c) else (set L=cpp:CXX1Lab:cxx1lab:cpp)
set /a S=%~2 %% 2
for /f "tokens=1,2,3,4 delims=:" %%a in ("%L%") do if !S!==1 (call :ride %%a %%b %%c %%d) else (call :ccs %%a %%b %%c %%d)
exit /b 0

:ride
rem  the candidate: RIDE, batch, the emulated target; the program is a
rem  directory of one .s per source, which vm6747 assembles and runs
pushd %LAB%\%1
del /q %3.out 2>nul
"%RIDE%" %2.pro --arch tms6747 --ti %CGT% --tilib %TILIB% --build > %LAB%\out\%1-ride-ccs.build 2>&1
if errorlevel 1 (echo RIDE-FAILED %1) else (
  "%VM%" %3.vm > %LAB%\out\%1-ride-ccs.out 2>&1 < nul
  echo RIDE %1 rc=!errorlevel!
  if exist %3.out (echo RIDE-OUT %1) else echo RIDE-NO-OUT %1
)
popd
rem  TI accepts what RIDE wrote: every .s assembled by cl6x, linked by lnk6x
set TIR=%LAB%\out\ti-%1-ride
if exist !TIR! rmdir /s /q !TIR!
mkdir !TIR!
set OBJS=
set ASMFAIL=0
for %%s in (%LAB%\%1\%3.vm\*.s) do (
  cl6x -mv6740 --abi=eabi -c %%s --output_file=!TIR!\%%~ns.obj > !TIR!\%%~ns.log 2>&1
  if errorlevel 1 (set ASMFAIL=1 & echo TI-REFUSED-RIDE %1 %%~ns.s) else set OBJS=!OBJS! !TIR!\%%~ns.obj
)
if !ASMFAIL!==0 (
  lnk6x -mv6740 --abi=eabi -i %TILIB% %LAB%\tools\ti-link.cmd !OBJS! -l rts6740_elf_eh.lib -o !TIR!\%3.out > !TIR!\link.log 2>&1
  if errorlevel 1 (echo TI-NOLINK-RIDE %1) else echo TI-LINKED-RIDE %1
)
exit /b 0

:ccs
rem  the judge: cl6x compiles the same sources - to assembly for vm6747,
rem  and to objects lnk6x links into the real program
set TIJ=%LAB%\out\ti-%1-ccs
if exist !TIJ! rmdir /s /q !TIJ!
mkdir !TIJ!
set OBJS=
set CCFAIL=0
pushd !TIJ!
for %%s in (%LAB%\%1\*.%4) do (
  cl6x -mv6740 --abi=eabi -n --symdebug:none -I %CGT%\include -I %LAB%\%1 %%s > %%~ns.cc.log 2>&1
  if errorlevel 1 (set CCFAIL=1 & echo CL6X-FAILED %1 %%~nxs)
  cl6x -mv6740 --abi=eabi -c --symdebug:none -I %CGT%\include -I %LAB%\%1 %%s --output_file=%%~ns.obj > %%~ns.obj.log 2>&1
  if errorlevel 1 (set CCFAIL=1) else set OBJS=!OBJS! !TIJ!\%%~ns.obj
)
popd
if !CCFAIL!==0 (
  lnk6x -mv6740 --abi=eabi -i %TILIB% %LAB%\tools\ti-link.cmd !OBJS! -l rts6740_elf_eh.lib -o !TIJ!\%3.out > !TIJ!\link.log 2>&1
  if errorlevel 1 (echo TI-NOLINK-CCS %1) else echo TI-LINKED-CCS %1
  pushd %LAB%\%1
  "%VM%" !TIJ! > %LAB%\out\%1-ccs.out 2>&1 < nul
  echo CCS %1 rc=!errorlevel!
  popd
)
exit /b 0
