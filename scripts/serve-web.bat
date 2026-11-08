@echo off
rem Opens the browser build (out\web) on http://localhost:8080/ - see apps\umbra\web\README.md
powershell -ExecutionPolicy Bypass -File "%~dp0serve-web.ps1" %*
