@echo off
rem  The tms6747 target taken all the way to a TI program by the driver, on the
rem  box: every case of tests/cases is compiled with `c90 -arch tms6747`, which
rem  assembles what it wrote with the asm6x.exe beside it and links the object
rem  with TI's lnk6x against rts6740_elf_eh.lib into <case>.out. The linker is
rem  the judge: it takes every object asm6x wrote, or names the one it does not.
rem  c90.exe and asm6x.exe are RIDE's, in its bin, built from the trees
rem  RStudio's tools/to-windows.sh relayed; the TI tools are CCS 7.4's.
rem    ti-link.cmd <Compiler-Ci tree on the box> <RIDE bin>
setlocal enabledelayedexpansion
if "%~1"==":shard" goto :shard
if "%~2"=="" (echo ti-link.cmd: needs the tree root and RIDE's bin & exit /b 2)
set ROOT=%~1
set BIN=%~2
set C90_TI=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2
set C90_TILIB=C:\Users\GRA\Documents\VM6747\tilib
if not exist %BIN%\c90.exe (echo ti-link.cmd: no c90.exe in %BIN% & exit /b 1)
if not exist %BIN%\asm6x.exe (echo ti-link.cmd: no asm6x.exe beside it & exit /b 1)
if not exist %C90_TI%\bin\lnk6x.exe (echo ti-link.cmd: no lnk6x under %C90_TI% & exit /b 1)
if not exist %C90_TILIB%\rts6740_elf_eh.lib (echo ti-link.cmd: no rts6740_elf_eh.lib - see Emulator/tests/ti.sh & exit /b 1)
set OUT=%ROOT%\tests\out-ti-link
if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%
rem  Six shards at once with par.cmd; each prints a marker line per case, and the counts are
rem  read off the markers once all six are done.
rem  Visual Studio's environment once, here: a compiler that finds it set runs its tools
rem  directly, where without it every asm6x and lnk6x call went through vcvars64.bat again.
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
call "%~dp0par.cmd" 6 "%~f0" > %OUT%\shards.log
findstr /v /b /c:"=" %OUT%\shards.log
for /f %%n in ('findstr /b /c:"=LINKED " %OUT%\shards.log ^| find /c /v ""') do set linked=%%n
for /f %%n in ('findstr /b /c:"=FAILED " %OUT%\shards.log ^| find /c /v ""') do set failed=%%n
for /f %%n in ('findstr /b /c:"=SKIPPED " %OUT%\shards.log ^| find /c /v ""') do set skipped=%%n
echo ti-link.cmd: %linked% programs linked by lnk6x, %failed% failed, %skipped% written for a 64-bit long
if not %failed%==0 exit /b 1
endlocal
exit /b 0

:shard
set /a I=0
for %%f in (%ROOT%\tests\cases\*.c) do (
    set /a I+=1, M=I %% %~3 + 1
    if !M!==%~2 call :one %%~nf %%f
)
exit /b 0

:one
findstr /b /c:"%1 " %ROOT%\tests\tms6747-lp64.txt >nul 2>&1 && (echo =SKIPPED %1& exit /b 0)
%BIN%\c90.exe -arch tms6747 -nologo %2 -o %OUT%\%1.out > %OUT%\%1.log 2>&1
if errorlevel 1 (echo =FAILED %1& echo TI-FAILED %1& type %OUT%\%1.log& exit /b 0)
if exist %OUT%\%1.out (echo =LINKED %1) else (echo =FAILED %1& echo TI-NO-OUT %1)
exit /b 0
