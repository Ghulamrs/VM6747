@echo off
REM Prove the binary this project builds is the compiler: take one case, compile
REM it with c90.exe, and run what comes out. ml64 and link come from vcvars64,
REM which is also why this is a file rather than a chain typed at ssh.
setlocal
set HERE=%~dp0
set CC1=%HERE%build\Release\c90.exe
set CASE=%HERE%..\tests\cases\out_hello.c
if not exist "%CC1%" ( echo check-vs.cmd: build it first & exit /b 1 )

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
"%CC1%" "%CASE%" -o "%HERE%build\Release\case.exe"
if errorlevel 1 ( echo check-vs.cmd: c90 refused it & exit /b 1 )
"%HERE%build\Release\case.exe"
if errorlevel 1 ( echo check-vs.cmd: the program failed & exit /b 1 )
echo check-vs.cmd: the case compiled and ran
endlocal
