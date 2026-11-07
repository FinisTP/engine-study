@echo off
rem Packages the game: Release build -> out\package\Umbra (Umbra.exe, DLLs, MSVC runtime, assets,
rem shaders) -> out\package\Umbra.zip. The folder runs on another Windows PC without the source.
rem Output also goes to out\package.log.
setlocal
cd /d "%~dp0.."
set LOG=out\package.log
set CMAKE=E:\VisualStudioCommunity\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe

rem 1. Compiler environment, then a Release build of everything.
call "E:\VisualStudioCommunity\VC\Auxiliary\Build\vcvars64.bat" > nul
echo === package %date% %time% === > %LOG%
"%CMAKE%" --build out\build\windows-msvc --config Release >> %LOG% 2>&1
if errorlevel 1 (
    echo BUILD FAILED, nothing packaged >> %LOG%
    type %LOG%
    pause
    exit /b 1
)

rem 2. Check the content the package will ship (same checks as the game and the editor).
out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe validate assets\scenes\settlement.scene.json assets\scenes\dungeon.scene.json assets\scenes\arena.scene.json assets\rpg\reference.rpg.json --assets assets >> %LOG% 2>&1
if errorlevel 1 (
    echo CONTENT HAS ERRORS, nothing packaged >> %LOG%
    type %LOG%
    pause
    exit /b 1
)

rem 3. A clean package folder from the install rules (component "game"), then the zip.
"%CMAKE%" -E rm -rf out\package\Umbra out\package\Umbra.zip
"%CMAKE%" --install out\build\windows-msvc --config Release --component game --prefix out\package\Umbra >> %LOG% 2>&1
pushd out\package
"%CMAKE%" -E tar cf Umbra.zip --format=zip Umbra >> ..\package.log 2>&1
popd
echo PACKAGE OK: out\package\Umbra\Umbra.exe and out\package\Umbra.zip >> %LOG%
type %LOG%
endlocal
