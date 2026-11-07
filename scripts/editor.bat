@echo off
rem Starts the Umbra Editor on a scene (default: the settlement). Build first with build-and-run.bat.
rem usage: scripts\editor.bat [scenes\dungeon.scene.json]
cd /d "%~dp0.."
set SCENE=%~1
if "%SCENE%"=="" set SCENE=scenes/settlement.scene.json
start "" "out\build\windows-msvc\apps\editor\Release\UmbraEditor.exe" "%SCENE:\=/%"
