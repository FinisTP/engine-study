@echo off
rem Overnight pictures (2026-11-25): waits for the battle music to finish (Temp\battle-music.log says done),
rem then paints the shared item pictures and the plinth wood into Temp\art-shared.
cd /d "%~dp0..\.."
:wait
findstr /x /c:"done" Temp\battle-music.log >nul 2>&1 || (timeout /t 30 /nobreak >nul & goto wait)
python tools\art\ui_style_board.py --spec shared-art.json --out Temp\art-shared > Temp\art-shared.log 2>&1
echo done >> Temp\art-shared.log
