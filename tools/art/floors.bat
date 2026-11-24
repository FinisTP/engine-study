@echo off
rem Paints the board floors and two item icons with ComfyUI (feedback 2026-11-24 16:57). Pictures go to Temp\art-floors.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py --spec floors.json --out Temp\art-floors > Temp\art-floors.log 2>&1
type Temp\art-floors.log
pause
