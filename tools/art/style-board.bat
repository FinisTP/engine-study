@echo off
rem Paints the campaign UI style board's pictures with ComfyUI (ComfyUI must be running; ports 8000 and 8188 are tried).
rem About 30 minutes; it can be stopped and started again. Pictures go to Temp\art-ui\raw.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py %*
pause
