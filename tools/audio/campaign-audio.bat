@echo off
rem The campaign's music and its typing sound with the local ComfyUI (ACE-Step 1.5 for music, Stable Audio 3
rem for the effect; plugins/ai-authoring/modules/localai/include/SoundGen.md). Two takes each; the second
rem is saved as <name>_2. Files go to the game's assets\audio\music and audio\sfx. Builds UmbraTool if needed.
cd /d "%~dp0..\.."
set TOOL=out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe
if not exist "%TOOL%" call build.bat only
set A=plugins\last-curtain\apps\LastCurtain\assets
"%TOOL%" sound "a single soft typewriter key press, close, dry, quiet, no music" --assets %A% --kind effect --seconds 1 --name type --count 2 --seed 101
"%TOOL%" sound "slow melancholic solo piano with soft strings, an empty grand opera house before the show, 1960s Saigon, nostalgic, instrumental" --assets %A% --kind music --seconds 75 --name title --count 2 --seed 11 --bpm 70 --key "D minor"
"%TOOL%" sound "warm gentle cafe at night, soft piano and Vietnamese dan bau, calm, tender, a little sad, instrumental" --assets %A% --kind music --seconds 75 --name cafe --count 2 --seed 21 --bpm 76 --key "F major"
"%TOOL%" sound "quiet searching music in an old theatre, sparse piano notes over a low cello drone, mysterious, instrumental" --assets %A% --kind music --seconds 75 --name map --count 2 --seed 31 --bpm 66 --key "A minor"
"%TOOL%" sound "tense steady turn-based battle, low strings ostinato, soft Vietnamese drums, eerie, restrained, instrumental" --assets %A% --kind music --seconds 75 --name fight --count 2 --seed 41 --bpm 96 --key "E minor"
"%TOOL%" sound "short warm relief after a hard fight, strings and piano rising gently, instrumental" --assets %A% --kind music --seconds 30 --name result --count 2 --seed 51 --bpm 80 --key "C major"
"%TOOL%" sound "slow dark sorrow, solo cello and piano, a loss, instrumental" --assets %A% --kind music --seconds 30 --name gameover --count 2 --seed 61 --bpm 60 --key "C minor"
rem Battle themes by kind of fight (the user, 2026-11-24 23:57, note 17), then each one's bass-boosted cut for the battle view (ffmpeg on PATH). The game plays audio\music\<name>.ogg by its name.
"%TOOL%" sound "classic JRPG battle theme, energetic strings and brass, taiko drums, driving bass, uneasy minor key, haunted opera house, instrumental, loopable" --assets %A% --kind music --seconds 90 --name fight_tutorial --count 2 --seed 71 --bpm 128 --key "D minor"
"%TOOL%" sound "fast JRPG skirmish battle music, staccato strings, electric bass, snare and toms, adventurous, instrumental, loopable" --assets %A% --kind music --seconds 90 --name fight_skirmish --count 2 --seed 81 --bpm 140 --key "E minor"
"%TOOL%" sound "dark grand JRPG boss battle, choir pads, pipe organ, heavy drums, Vietnamese gong accents, menacing, instrumental, loopable" --assets %A% --kind music --seconds 100 --name fight_zodiac --count 2 --seed 91 --bpm 150 --key "C minor"
for %%n in (fight_tutorial fight_skirmish fight_zodiac) do ffmpeg -y -loglevel error -i %A%\audio\music\%%n.ogg -af "bass=g=9:f=110,lowshelf=g=4:f=60,volume=-2dB" -c:a libvorbis -q:a 6 %A%\audio\music\%%n_battle.ogg
pause
