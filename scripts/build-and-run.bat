@echo off
rem Builds the MSVC Release configuration, runs the tests, then starts the game.
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
"%CMAKE%" --build out\build\windows-msvc --config Release >> %LOG% 2>&1
if errorlevel 1 (
    echo BUILD FAILED >> %LOG%
    type %LOG%
    pause
    exit /b 1
)
echo BUILD OK >> %LOG%

rem 3. Tests (a failure is logged but doesn't stop the game from starting).
"%CTEST%" --test-dir out\build\windows-msvc -C Release --output-on-failure >> %LOG% 2>&1
echo CTEST EXIT %errorlevel% >> %LOG%

rem 4. Start the game.
echo === starting Umbra === >> %LOG%
start "" "out\build\windows-msvc\apps\umbra\Release\Umbra.exe"
endlocal
