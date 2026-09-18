@echo off
rem  TriLab's Windows leg, on the box: RIDE builds each lab with cc1i/cxx1i and the
rem  project's assembler; MSBuild builds the judge's project with cl; both run.
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
echo WINDOWS-LEG-DONE
