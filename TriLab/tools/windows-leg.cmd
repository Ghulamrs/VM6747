@echo off
rem  TriLab's Windows leg, on the box: RIDE builds each lab with cc1i/cxx1i/shci and
rem  the project's assembler; the judge builds the same sources with Microsoft's
rem  tools - MSBuild and cl for C and C++, ml64 and link over shci's assembly for
rem  Shalimar, which no Visual Studio project can hold; both run.
rem  Usage: windows-leg.cmd <TriLab dir> <RStudioConsole.exe> <assembler.exe>
rem  Writes <TriLab dir>\out\<lab>-ride.out, <lab>-vs.out, and the build logs.
setlocal enabledelayedexpansion
if "%~3"=="" (echo windows-leg.cmd: needs the TriLab dir, RStudioConsole.exe and the assembler & exit /b 2)
set LAB=%~1
set RIDE=%~2
set ASM=%~3
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo windows-leg: no vcvars64 & exit /b 1)
if not exist %LAB%\out mkdir %LAB%\out
for %%L in (c:CC1Lab:cc1lab cpp:CXX1Lab:cxx1lab) do (
  for /f "tokens=1,2,3 delims=:" %%a in ("%%L") do (
    rem  the candidate: RIDE, batch, the assembler named for this run
    pushd %LAB%\%%a
    "%RIDE%" %%b.pro --arch x86_64-windows --assembler "%ASM%" --build > %LAB%\out\%%a-ride.build 2>&1
    if errorlevel 1 (echo RIDE-FAILED %%a) else (
      %%c.exe > %LAB%\out\%%a-ride.out 2>&1
      echo RIDE %%a rc=!errorlevel!
    )
    popd
    rem  the judge: Visual Studio's own project, cl.exe
    msbuild %LAB%\%%a\vs\%%b.sln /nologo /v:q /p:Configuration=Release /p:Platform=x64 > %LAB%\out\%%a-vs.build 2>&1
    if errorlevel 1 (echo VS-FAILED %%a) else (
      pushd %LAB%\%%a
      %LAB%\%%a\vs\x64\Release\%%b.exe > %LAB%\out\%%a-vs.out 2>&1
      echo VS %%a rc=!errorlevel!
      popd
    )
  )
)
rem  the Shalimar lab: RIDE with shci and the assembler, against shci's own
rem  assembly through ml64 and link - the two commands shci runs when no
rem  assembler is named, from Compiler-Si\src\Driver.cpp - with RIDE's runtime
set BIN=%~dp2
pushd %LAB%\shm
"%RIDE%" ShmLab.pro --arch x86_64-windows --assembler "%ASM%" --build > %LAB%\out\shm-ride.build 2>&1
if errorlevel 1 (echo RIDE-FAILED shm) else (
  main.exe > %LAB%\out\shm-ride.out 2>&1
  echo RIDE shm rc=!errorlevel!
)
set SRCS=
for %%s in (main prime sqroot invert gaussseidel rotations strsplit) do set SRCS=!SRCS! %%s.shl
"%BIN%shci.exe" !SRCS! --target=x86_64-windows -S -o %LAB%\out\shm-vs.asm > %LAB%\out\shm-vs.build 2>&1
if errorlevel 1 (echo VS-FAILED shm) else (
  ml64 /nologo /c /Fo%LAB%\out\shm-vs.obj %LAB%\out\shm-vs.asm >> %LAB%\out\shm-vs.build 2>&1
  link /nologo /subsystem:console /out:%LAB%\out\shm-vs.exe %LAB%\out\shm-vs.obj "%BIN%lib\shmrt-x86_64-windows.lib" >> %LAB%\out\shm-vs.build 2>&1
  if errorlevel 1 (echo VS-FAILED shm) else (
    %LAB%\out\shm-vs.exe > %LAB%\out\shm-vs.out 2>&1
    echo VS shm rc=!errorlevel!
  )
)
popd
echo WINDOWS-LEG-DONE
