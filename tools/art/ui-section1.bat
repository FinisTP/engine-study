@echo off
rem Paints the campaign UI art for slice 1 with ComfyUI (ports 8000 and 8188 are tried).
rem About an hour; it can be stopped and started again. Pictures go to Temp\art-ui\raw.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py --spec ui_section1.json %*
pause
