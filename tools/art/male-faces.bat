@echo off
rem The male player's faces from his new open-eyed calm portrait (feedback 2026-11-24 15:10). Pictures go to Temp\art-faces.
cd /d "%~dp0..\.."
set LOG=Temp\art-faces\male-faces.log
echo run %date% %time% > %LOG%
python tools\art\face_edit.py player_male calm talk "{\"aaa\": 20, \"eyebrow\": 2}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm happy "{\"smile\": 1.2, \"eee\": 8, \"aaa\": 12, \"eyebrow\": 4}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm angry "{\"eyebrow\": -10, \"eee\": 10, \"smile\": -0.3, \"rotate_pitch\": -4, \"blink\": -2}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm afraid "{\"eyebrow\": 13, \"blink\": 5, \"aaa\": 45, \"woo\": 6, \"rotate_pitch\": -3}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm scheming "{\"smile\": 0.55, \"blink\": -7, \"eyebrow\": -5, \"pupil_x\": 10, \"rotate_roll\": -5, \"rotate_yaw\": -6}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
python tools\art\face_edit.py player_male calm neardead "{\"blink\": -11, \"eyebrow\": 6, \"aaa\": 18, \"rotate_pitch\": 8, \"rotate_roll\": 7, \"pupil_y\": -6, \"smile\": -0.2}" --tag new --src Temp\art-faces\player_male_calm_new.png >> %LOG% 2>&1
type %LOG%
pause
