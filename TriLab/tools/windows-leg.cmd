@echo off
rem  TriLab's Windows leg, on the box: RIDE builds each lab with c90/cpp11/shalimar and
rem  the project's assembler; the judge builds the same sources with Microsoft's
rem  tools - MSBuild and cl for C and C++, ml64 and link over shalimar's assembly for
rem  Shalimar, which no Visual Studio project can hold; both run.
rem  Usage: windows-leg.cmd <TriLab dir> <RIDEConsole.exe> <assembler.exe>
rem  Writes <TriLab dir>\out\<lab>-ride.out, <lab>-vs.out, and the build logs.
setlocal enabledelayedexpansion
if "%~1"==":shard" goto :shard
if "%~3"=="" (echo windows-leg.cmd: needs the TriLab dir, RIDEConsole.exe and the assembler & exit /b 2)
set LAB=%~1
set RIDE=%~2
set ASM=%~3
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo windows-leg: no vcvars64 & exit /b 1)
if not exist %LAB%\out mkdir %LAB%\out
rem  Six jobs at once with par.cmd: the C, C++ and Shalimar labs, each built by RIDE and by its
rem  judge - the candidate and the reference side by side, where all six once ran in turn.
set BIN=%~dp2
call "%~dp0par.cmd" 6 "%~f0"
echo WINDOWS-LEG-DONE
exit /b 0

rem  Shards 1-2 are the C lab, 3-4 the C++ one, 5-6 Shalimar's; the odd shard is RIDE, the even the judge.
:shard
set /a S=%~2 %% 2
if %~2 GEQ 5 goto :shmshard
if %~2 LEQ 2 (set L=c:CC1Lab:cc1lab) else (set L=cpp:CXX1Lab:cxx1lab)
for /f "tokens=1,2,3 delims=:" %%a in ("%L%") do if !S!==1 (call :ride %%a %%b %%c) else (call :vs %%a %%b %%c)
exit /b 0

:shmshard
if !S!==1 (call :shmride) else (call :shmjudge)
exit /b 0

:ride
rem  the candidate: RIDE, batch, the assembler named for this run
pushd %LAB%\%1
"%RIDE%" %2.pro --arch x86_64-windows --assembler "%ASM%" --build > %LAB%\out\%1-ride.build 2>&1
if errorlevel 1 (echo RIDE-FAILED %1) else (
  %3.exe > %LAB%\out\%1-ride.out 2>&1 < nul
  echo RIDE %1 rc=!errorlevel!
)
popd
exit /b 0

:vs
rem  the judge: Visual Studio's own project, cl.exe
msbuild %LAB%\%1\vs\%2.sln /nologo /v:q /p:Configuration=Release /p:Platform=x64 > %LAB%\out\%1-vs.build 2>&1
if errorlevel 1 (echo VS-FAILED %1) else (
  pushd %LAB%\%1
  %LAB%\%1\vs\x64\Release\%2.exe > %LAB%\out\%1-vs.out 2>&1 < nul
  echo VS %1 rc=!errorlevel!
  popd
)
exit /b 0

:shmride
rem  the Shalimar lab: RIDE with shalimar and the assembler, against shalimar's own
rem  assembly through ml64 and link - the two commands shalimar runs when no
rem  assembler is named, from Compiler-Si\src\Driver.cpp - with RIDE's runtime
pushd %LAB%\shm
"%RIDE%" ShmLab.pro --arch x86_64-windows --assembler "%ASM%" --build > %LAB%\out\shm-ride.build 2>&1
if errorlevel 1 (echo RIDE-FAILED shm) else (
  main.exe > %LAB%\out\shm-ride.out 2>&1 < nul
  echo RIDE shm rc=!errorlevel!
)
popd
exit /b 0

:shmjudge
pushd %LAB%\shm
set SRCS=
for %%s in (main prime sqroot invert gaussseidel rotations strsplit) do set SRCS=!SRCS! %%s.shl
"%BIN%shalimar.exe" !SRCS! --target=x86_64-windows -S -o %LAB%\out\shm-vs.asm > %LAB%\out\shm-vs.build 2>&1
if errorlevel 1 (echo VS-FAILED shm) else (
  ml64 /nologo /c /Fo%LAB%\out\shm-vs.obj %LAB%\out\shm-vs.asm >> %LAB%\out\shm-vs.build 2>&1
  link /nologo /subsystem:console /out:%LAB%\out\shm-vs.exe %LAB%\out\shm-vs.obj "%BIN%lib\shmrt-x86_64-windows.lib" >> %LAB%\out\shm-vs.build 2>&1
  if errorlevel 1 (echo VS-FAILED shm) else (
    %LAB%\out\shm-vs.exe > %LAB%\out\shm-vs.out 2>&1 < nul
    echo VS shm rc=!errorlevel!
  )
)
popd
exit /b 0
