@echo off
rem Paints the campaign UI style board's pictures with ComfyUI (it must be running on 127.0.0.1:8188).
rem About 30 minutes; it can be stopped and started again. Pictures go to Temp\art-ui\raw.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py %*
pause
