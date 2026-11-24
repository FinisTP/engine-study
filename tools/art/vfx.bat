@echo off
rem Paints the particle textures for the VFX library with ComfyUI. Pictures go to Temp\art-vfx.
cd /d "%~dp0..\.."
python tools\art\ui_style_board.py --spec vfx.json --out Temp\art-vfx > Temp\art-vfx.log 2>&1
type Temp\art-vfx.log
pause
