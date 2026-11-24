@echo off
rem Paints the gear and mung-bean icons with ComfyUI (2026-11-25). Pictures go to Temp\art-gear.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py --spec gear.json --out Temp\art-gear > Temp\art-gear.log 2>&1
type Temp\art-gear.log
