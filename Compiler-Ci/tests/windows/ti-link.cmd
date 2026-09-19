@echo off
rem  The tms6747 target taken all the way to a TI program by the driver, on the
rem  box: every case of tests/cases is compiled with `cc1i -arch tms6747`, which
rem  assembles what it wrote with the asm6x.exe beside it and links the object
rem  with TI's lnk6x against rts6740_elf_eh.lib into <case>.out. The linker is
rem  the judge: it takes every object asm6x wrote, or names the one it does not.
rem  cc1i.exe and asm6x.exe are RIDE's, in its bin, built from the trees
rem  RStudio's tools/to-windows.sh relayed; the TI tools are CCS 7.4's.
rem    ti-link.cmd <Compiler-Ci tree on the box> <RIDE bin>
setlocal enabledelayedexpansion
if "%~2"=="" (echo ti-link.cmd: needs the tree root and RIDE's bin & exit /b 2)
set ROOT=%~1
set BIN=%~2
set CC1_TI=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2
set CC1_TILIB=C:\Users\GRA\Documents\VM6747\tilib
if not exist %BIN%\cc1i.exe (echo ti-link.cmd: no cc1i.exe in %BIN% & exit /b 1)
if not exist %BIN%\asm6x.exe (echo ti-link.cmd: no asm6x.exe beside it & exit /b 1)
if not exist %CC1_TI%\bin\lnk6x.exe (echo ti-link.cmd: no lnk6x under %CC1_TI% & exit /b 1)
if not exist %CC1_TILIB%\rts6740_elf_eh.lib (echo ti-link.cmd: no rts6740_elf_eh.lib - see Emulator/tests/ti.sh & exit /b 1)
set OUT=%ROOT%\tests\out-ti-link
if exist %OUT% rmdir /s /q %OUT%
mkdir %OUT%
set linked=0
set failed=0
set skipped=0
for %%f in (%ROOT%\tests\cases\*.c) do (
    set NAME=%%~nf
    findstr /b /c:"!NAME! " %ROOT%\tests\tms6747-lp64.txt >nul 2>&1
    if not errorlevel 1 (set /a skipped+=1) else (
        %BIN%\cc1i.exe -arch tms6747 -nologo %%f -o %OUT%\!NAME!.out > %OUT%\!NAME!.log 2>&1
        if errorlevel 1 (set /a failed+=1 & echo TI-FAILED !NAME! & type %OUT%\!NAME!.log) else (
            if exist %OUT%\!NAME!.out (set /a linked+=1) else (set /a failed+=1 & echo TI-NO-OUT !NAME!)
        )
    )
)
echo ti-link.cmd: %linked% programs linked by lnk6x, %failed% failed, %skipped% written for a 64-bit long
if not %failed%==0 exit /b 1
endlocal
