@echo off
rem The battle themes only (note 17): each kind of fight, then its bass-heavy cut for the battle view.
rem Needs the local ComfyUI (ACE-Step 1.5) and ffmpeg on PATH; the log goes to Temp\battle-music.log.
cd /d "%~dp0..\.."
set TOOL=out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe
if not exist "%TOOL%" call build.bat only
set A=plugins\last-curtain\apps\LastCurtain\assets
echo started > Temp\battle-music.log
"%TOOL%" sound "classic JRPG battle theme, energetic strings and brass, taiko drums, driving bass, uneasy minor key, haunted opera house, instrumental, loopable" --assets %A% --kind music --seconds 90 --name fight_tutorial --count 2 --seed 71 --bpm 128 --key "D minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "fast JRPG skirmish battle music, staccato strings, electric bass, snare and toms, adventurous, instrumental, loopable" --assets %A% --kind music --seconds 90 --name fight_skirmish --count 2 --seed 81 --bpm 140 --key "E minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "dark grand JRPG boss battle, choir pads, pipe organ, heavy drums, Vietnamese gong accents, menacing, instrumental, loopable" --assets %A% --kind music --seconds 100 --name fight_zodiac --count 2 --seed 91 --bpm 150 --key "C minor" >> Temp\battle-music.log 2>&1
for %%n in (fight_tutorial fight_skirmish fight_zodiac) do ffmpeg -y -loglevel error -i %A%\audio\music\%%n.ogg -af "bass=g=9:f=110,lowshelf=g=4:f=60,volume=-2dB" -c:a libvorbis -q:a 6 %A%\audio\music\%%n_battle.ogg >> Temp\battle-music.log 2>&1
echo done >> Temp\battle-music.log
