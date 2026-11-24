@echo off
rem Repaints the male player's talking face with his eyes open (feedback 2026-11-24, item 2).
rem Needs ComfyUI running with ComfyUI-AdvancedLivePortrait. Pictures go to Temp\art-faces.
cd /d "%~dp0..\.."
if not exist Temp\art-faces mkdir Temp\art-faces
set LOG=Temp\art-faces\fix-faces.log
echo run %date% %time% > %LOG%
python tools\art\face_edit.py player_male calm talk "{\"aaa\": 22, \"blink\": 7, \"eyebrow\": 3}" --tag a >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm talk "{\"aaa\": 30, \"blink\": 10, \"eyebrow\": 5, \"pupil_y\": 4}" --tag b >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm calm "{\"blink\": 8, \"eyebrow\": 2}" --tag a >> %LOG% 2>&1
type %LOG%
pause
