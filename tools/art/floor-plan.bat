@echo off
rem Paints the theatre map's floor plan with ComfyUI (ports 8000 and 8188 are tried). Pictures go to Temp\art-ui\raw.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py --spec ui_floorplan.json %*
pause
