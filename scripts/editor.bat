@echo off
rem Starts the Umbra Editor on a scene (default: the settlement). Build first with build-and-run.bat.
rem usage: scripts\editor.bat [scenes\dungeon.scene.json]
rem The editor runs in this window, so its log stays visible if it stops with an error.
cd /d "%~dp0.."
set SCENE=%~1
if "%SCENE%"=="" set SCENE=scenes/settlement.scene.json
set EDITOR=out\build\windows-msvc\apps\editor\Release\UmbraEditor.exe
if not exist "%EDITOR%" (
    echo %EDITOR% not found. Build first: scripts\build-and-run.bat
    pause
    exit /b 1
)
"%EDITOR%" "%SCENE:\=/%"
if not "%errorlevel%"=="0" (
    echo.
    echo The editor stopped with exit code %errorlevel%. The log above says why.
    pause
)
