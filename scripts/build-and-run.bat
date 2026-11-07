@echo off
rem Builds the MSVC Release configuration, runs the tests, then starts the game.
rem A build whose tests all pass is also copied to out\known-good (game + DLLs + assets), so a
rem broken build or broken content never leaves you without a game that runs.
rem Output goes to out\build-and-run.log as well as this window.
setlocal
cd /d "%~dp0.."
set LOG=out\build-and-run.log
set CMAKE=E:\VisualStudioCommunity\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set CTEST=E:\VisualStudioCommunity\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe

rem 1. Compiler environment (cl.exe, Windows SDK) for this window.
call "E:\VisualStudioCommunity\VC\Auxiliary\Build\vcvars64.bat" > nul
echo === build %date% %time% === > %LOG%

rem 2. Build everything in Release (CMake re-runs configure by itself if a CMakeLists changed).
rem    Any exit code other than 0 is a failure: a crashed or blocked tool returns -1, which
rem    "if errorlevel 1" would miss.
"%CMAKE%" --build out\build\windows-msvc --config Release >> %LOG% 2>&1
if not "%errorlevel%"=="0" (
    echo BUILD FAILED >> %LOG%
    if exist out\known-good\Umbra.exe echo The last known-good game is still there: out\known-good\Umbra.exe >> %LOG%
    type %LOG%
    pause
    exit /b 1
)
echo BUILD OK >> %LOG%

rem 3. Tests (a failure is logged but doesn't stop the game from starting).
"%CTEST%" --test-dir out\build\windows-msvc -C Release --output-on-failure >> %LOG% 2>&1
set TESTS=%errorlevel%
echo CTEST EXIT %TESTS% >> %LOG%

rem 3b. All tests passed: this build and these assets become the known-good pair.
if "%TESTS%"=="0" (
    "%CMAKE%" -E rm -rf out\known-good
    "%CMAKE%" --install out\build\windows-msvc --config Release --component game --prefix out\known-good >> %LOG% 2>&1
    echo KNOWN-GOOD updated: out\known-good\Umbra.exe >> %LOG%
)

rem 4. Start the game.
echo === starting Umbra === >> %LOG%
start "" "out\build\windows-msvc\apps\umbra\Release\Umbra.exe"
endlocal
