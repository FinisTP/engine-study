@echo off
rem Umbra's one way to build and start things (README "Build and run"). Double-click it, or:
rem   build                     build Release, start The Last Curtain (the cafe, then a match)
rem   build editor              build, start the Umbra Editor (Play / F5 starts the game; pick how in the menu bar)
rem   build match               build, start a faction match at once
rem   build scene prologue      build, play one campaign scene
rem   build stage stage1        build, play one campaign stage (add a number for a skirmish refill: build stage sk_props 1)
rem   build game --any --flags  build, start The Last Curtain with these flags (its README)
rem   build test                build, run the tests, but not the long balance soaks
rem   build soak                build, run the balance soaks only (about 25 minutes)
rem   build only                build everything, start nothing
rem Set UMBRA_JOBS=4 first if Windows blocks compilers ("Access is denied", LNK1104).
setlocal EnableDelayedExpansion
cd /d "%~dp0"
set BUILD=out\build\windows-msvc
set WHAT=%~1
if "%WHAT%"=="" set WHAT=game
set "KNOWN= game editor match scene stage test soak only "
if /i "!KNOWN: %WHAT% =!"=="!KNOWN!" (
    echo Unknown word "%WHAT%". Use one of:%KNOWN%
    echo See the top of build.bat.
    goto failed
)
if /i "%WHAT%"=="scene" if "%~2"=="" (
    echo Which scene? For example: build scene prologue
    goto failed
)
if /i "%WHAT%"=="stage" if "%~2"=="" (
    echo Which stage? For example: build stage stage1  ^(t1, t3, stage1, sk_props, sk_stairs, sk_boxoffice^)
    goto failed
)

rem 1. Visual Studio's compiler, CMake and Ninja (vswhere finds the install).
set "VS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if "!VS!"=="" set "VS=E:\VisualStudioCommunity"
if not exist "!VS!\VC\Auxiliary\Build\vcvars64.bat" (
    echo Visual Studio with C++ was not found. Install "Desktop development with C++".
    goto failed
)
call "!VS!\VC\Auxiliary\Build\vcvars64.bat" > nul
set "PATH=!VS!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!VS!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"

rem 2. Configure the first time (vcpkg builds the libraries once; that takes long).
if not exist "%BUILD%\CMakeCache.txt" (
    if "%VCPKG_ROOT%"=="" (
        echo VCPKG_ROOT is not set: point it at your vcpkg folder first.
        goto failed
    )
    cmake --preset windows-msvc
    if not "!errorlevel!"=="0" goto failed
)

rem 3. Build what is needed: the game, the editor (it builds the game too), or everything.
set TARGET=--target LastCurtain
if /i "%WHAT%"=="editor" set TARGET=--target UmbraEditor
if /i "%WHAT%"=="test" set TARGET=
if /i "%WHAT%"=="soak" set TARGET=
if /i "%WHAT%"=="only" set TARGET=
set JOBS=
if not "%UMBRA_JOBS%"=="" set JOBS=--parallel %UMBRA_JOBS%
echo Building Release...
cmake --build %BUILD% --config Release %TARGET% %JOBS%
if not "!errorlevel!"=="0" (
    echo.
    echo BUILD FAILED. The first error is above.
    goto failed
)

rem 4. Start it, or run the tests.
set GAME=%BUILD%\plugins\last-curtain\apps\LastCurtain\Release\LastCurtain.exe
set EDITOR=%BUILD%\apps\editor\Release\UmbraEditor.exe
set REST=
for /f "tokens=1,* delims= " %%a in ("%*") do set "REST=%%b"
if /i "%WHAT%"=="game" start "" "%GAME%" !REST!
if /i "%WHAT%"=="match" start "" "%GAME%" --match
if /i "%WHAT%"=="scene" start "" "%GAME%" --scene %~2 --backdrop foyer_today
if /i "%WHAT%"=="stage" if "%~3"=="" start "" "%GAME%" --stage %~2
if /i "%WHAT%"=="stage" if not "%~3"=="" start "" "%GAME%" --stage %~2 --refill %~3
if /i "%WHAT%"=="editor" start "" "%EDITOR%" scenes/settlement.scene.json
if /i "%WHAT%"=="test" (
    ctest --test-dir %BUILD% -C Release --output-on-failure -E Balance
    pause
)
if /i "%WHAT%"=="soak" (
    ctest --test-dir %BUILD% -C Release --output-on-failure -R Balance
    pause
)
endlocal
exit /b 0

:failed
pause
endlocal
exit /b 1
