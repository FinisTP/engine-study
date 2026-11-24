@echo off
rem Five battle-theme candidates to compare (the user, 2026-11-25 02:51): melody first, light bass, no long drum intro.
rem Each comes as <name>_battle.ogg (the take, mixed for the battle view) and <name>_board.ogg (the same track
rem softened for the board: high end rolled off and quieter, so the swap keeps the same moment, as in Fire Emblem).
rem Needs the local ComfyUI and ffmpeg on PATH. Results in Temp\music-candidates\audio\music; log in Temp\battle-music.log.
cd /d "%~dp0..\.."
set TOOL=out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe
if not exist "%TOOL%" call build.bat only
set C=Temp\music-candidates
set M=%C%\audio\music
if not exist %M% mkdir %M%
set STYLE=melody-led, the lead melody starts in the first bar and stays in front, balanced mix, light bass, no long drum intro, instrumental, loopable
echo started > Temp\battle-music.log
"%TOOL%" sound "tactical RPG battle theme, heroic violin melody with brass answers, marching snare, %STYLE%" --assets %C% --kind music --seconds 80 --name cand_a --count 1 --seed 101 --bpm 126 --key "D minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "classic JRPG battle theme, fast string ostinato under a soaring flute and oboe melody, timpani accents, %STYLE%" --assets %C% --kind music --seconds 80 --name cand_b --count 1 --seed 102 --bpm 138 --key "A minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "tense chamber battle music in a haunted opera house, piano and solo cello melody, pizzicato strings, %STYLE%" --assets %C% --kind music --seconds 80 --name cand_c --count 1 --seed 103 --bpm 132 --key "E minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "battle theme with a Vietnamese bamboo flute and zither melody over strings, light hand drums, %STYLE%" --assets %C% --kind music --seconds 80 --name cand_d --count 1 --seed 104 --bpm 124 --key "G minor" >> Temp\battle-music.log 2>&1
"%TOOL%" sound "dark waltz battle theme in 3/4, violin and accordion melody, music-box countermelody, %STYLE%" --assets %C% --kind music --seconds 80 --name cand_e --count 1 --seed 105 --bpm 150 --key "C minor" >> Temp\battle-music.log 2>&1
rem The full take, its bass turned down a little and the melody band lifted, becomes the battle cut; the board cut is softer still.
for %%n in (cand_a cand_b cand_c cand_d cand_e) do (
  ffmpeg -y -loglevel error -i %M%\%%n.ogg -af "bass=g=-4:f=100,equalizer=f=2500:t=q:w=1:g=2,loudnorm=I=-16" -c:a libvorbis -q:a 6 %M%\%%n_battle.ogg >> Temp\battle-music.log 2>&1
  ffmpeg -y -loglevel error -i %M%\%%n_battle.ogg -af "lowpass=f=1800,volume=-4dB" -c:a libvorbis -q:a 6 %M%\%%n_board.ogg >> Temp\battle-music.log 2>&1
)
echo done >> Temp\battle-music.log
