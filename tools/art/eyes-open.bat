@echo off
rem Repaints the male player's eyes open on his calm portrait (feedback 2026-11-24 15:10). Pictures go to Temp\art-faces\eyes.
cd /d "%~dp0..\.."
if not exist Temp\art-faces\eyes mkdir Temp\art-faces\eyes
python tools\art\inpaint.py Temp\art-faces\eyes_src.png Temp\art-faces\eyes_mask.png "painted portrait of a young Vietnamese man, eyes open, dark brown eyes looking ahead to the right, calm attentive gaze, natural eyelids, soft painterly brushwork, muted colours" --out Temp\art-faces\eyes --name eyes --denoise 0.55 0.7 --seeds 1 2 3 4 > Temp\art-faces\eyes\log.txt 2>&1
type Temp\art-faces\eyes\log.txt
pause
