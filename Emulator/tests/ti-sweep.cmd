@echo off
rem cl6x assembles every .s (asm6x itself writes COFF whatever --abi says),
rem lnk6x links each program - a Shalimar one beside its runtime - against
rem the exception-handling build of TI's runtime, the library after the objects.
set PATH=C:\ti\ccsv7\tools\compiler\ti-cgt-c6000_8.2.2\bin;%PATH%
set TILIB=C:\Users\GRA\Documents\VM6747\tilib
cd /d C:\Users\GRA\Documents\VM6747\tisweep
if not exist %TILIB%\rts6740_elf_eh.lib echo NO_EH_LIBRARY & exit /b 3
for %%d in (c cxx shm shmrt) do (
  for %%f in (%%d\*.s) do (
    cl6x -mv6740 --abi=eabi -c %%f --output_file=%%~dpnf.obj > %%~dpnf.log 2>&1
    if errorlevel 1 echo FAILED %%f
  )
)
for %%d in (c cxx) do (
  for %%f in (%%d\*.obj) do (
    lnk6x -mv6740 --abi=eabi -i %TILIB% ti-link.cmd %%f -l rts6740_elf_eh.lib -o %%~dpnf.out > %%~dpnf.lnk 2>&1
    if errorlevel 1 echo NOLINK %%f
  )
)
for %%f in (shm\*.obj) do (
  lnk6x -mv6740 --abi=eabi -i %TILIB% ti-link.cmd %%f shmrt\*.obj -l rts6740_elf_eh.lib -o %%~dpnf.out > %%~dpnf.lnk 2>&1
  if errorlevel 1 echo NOLINK %%f
)
echo SWEEP_DONE
