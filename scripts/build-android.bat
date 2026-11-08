@echo off
rem Builds the game for Android and starts it on the emulator (roadmap P11) - see
rem plugins\android-export\README.md. Options: -Abis arm64-v8a (phones), -Project <name>, -NoRun
powershell -ExecutionPolicy Bypass -File "%~dp0..\plugins\android-export\tools\build-android.ps1" %*
if not "%errorlevel%"=="0" pause
