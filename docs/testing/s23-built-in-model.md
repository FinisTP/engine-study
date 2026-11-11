# S23 — Built-in model: verification record

## Record

- Date: 2026-11-11
- Commit: `issue #UMBRA-93`, ADR 0011
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan);
  llama.cpp b9030 built from source there. The cloud cannot download model files, so **no real
  model has answered yet**: the llama.cpp path ran with a tiny random-weights model only.
- State: **implemented, not measured**. The model choice and the numbers ADR 0011 asks for
  (accepted lines, valid picks, milliseconds, memory) are the user's run in §3.
- User decisions (2026-11-11): up to 4-5 GB for the model is acceptable; the user downloads the
  file; English only for now; lines must stay on the fact, with filter layers that can grow.

## What was built

| Part | Where |
|---|---|
| `town::Mind`: a model is text in, text out | `plugins/living-town/modules/mind/include/Mind.hpp` |
| `TownMind`: words residents' news ahead of the player, picks among legal activities, one worker thread, late and stale answers dropped | `TownMind.hpp`, `src/TownMind.cpp`; explained in `Mind.md` |
| Grammars: a pick can only be one of the listed numbers; a line only plain sentences | `WordingRequest`, `ChoiceRequest` |
| `LineGuard`: five checks in a list a project can extend; limits and banned words in `town/lines.rules.json` | `LineGuard.hpp`, `src/LineGuard.cpp` |
| What was said is kept (`TownState::said`), saved, and shown in the chronicle with the speaker | `plugins/living-town/modules/town` |
| `LlamaMind`: llama.cpp behind `Mind` (chat template, grammar sampler, seeded draw) | `LlamaMind.hpp`, `src/LlamaMind.cpp`; built when llama.cpp is found |
| vcpkg feature `town-model` (`llama-cpp`), on in the `windows-msvc` preset | `vcpkg.json`, `CMakePresets.json` |
| Game: loads `projects/<project>/models/town.gguf` if present; approaching a resident prepares their line; picks feed the tick | `apps/umbra/TownFlow` |
| `UmbraTool town-mind`: every resident's line beside its template, the guard's verdict, picks, milliseconds | `plugins/living-town/tool/MindCommand.cpp` |

## 1. Automated checks (T92, in `Umbra.Town`, with a scripted stand-in model)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **53/53 / 53/53 / 53/53** |
| Guard: good lines pass; too short, too long, markup, a line break, a banned word (any case), naming an outsider, dropping someone the fact is about are each refused with their reason; "pipes" does not name Pip; a project's own check runs; the rules file loads | pass |
| A checked line is shown in the model's words and kept in the chronicle; asked once per piece of news | pass |
| A refused line, an empty answer, markup, an off-fact line and an unprepared talk all give the template, and the town's state equals the one with no model | pass |
| Saved and loaded: the same words in the chronicle, no new request | pass |
| The choice request lists exactly the legal activities and its grammar exactly their numbers | pass |
| A model that always answers "2" steers a week (more than 50 accepted picks) without breaking money or needs; answers "99", "", "two", "0", "-1" are never used and leave the town identical to the rule-based one | pass |
| A model that takes 150 ms: talking at once gives the template in under 100 ms; later its words; after `Forget` its answer is dropped; destroying it mid-answer is clean | pass |

## 2. The llama.cpp path (cloud, random-weights model)

`UmbraTool town-mind ... --model rand.gguf` (a 13 MB model with random weights, made with
llama.cpp's own converter): loaded, every line obeyed the grammar (letters and light punctuation,
ending in `.`, `!` or `?`), every pick was a valid number, about 170 ms a line and 8 ms a pick on
the cloud CPU. The lines are gibberish, as random weights must give; the guard refused four of
five (they did not name the fact's people). This proves the plumbing, not the quality.

## 2b. First real run (user's PC, 2026-11-11)

Qwen3 1.7B Q4_K_M, 8 CPU threads, MSVC Release; vcpkg's llama-cpp built and loaded (1.2 s).

| Measure | Result |
|---|---|
| Picks | 7 of 7 valid, about 770 ms each |
| Lines | **5 of 15 accepted**, 2.9 s each (slowest 5.1 s) |

Most lines began with nonsense ("INGLESE,", "AdapterFactory:", "UPPORTED BY ...") and several
repeated the description of the speaker. Causes and fixes (`#UMBRA-94`, `Mind.md` "The prompt"):
the model's `<think>` opening was forbidden by the grammar, so its first token was forced to be
something else (an empty think block is now added); the prompt described the speaker at length
(now one worked example and one word of tone); lines in capitals passed the guard (a sixth check,
"not shouting"). The safety side held: nothing refused reached the player, and every pick was
legal. **To be measured again by the user.**

## 3. Windows checklist (user)

1. Build with the `windows-msvc` preset. The first configure now also builds llama.cpp through
   vcpkg (several minutes, once). The configure log must say
   `Living town: llama.cpp found, the built-in model is available`. If vcpkg fails on
   `llama-cpp`, send the error; to get going without it, remove the line
   `"VCPKG_MANIFEST_FEATURES": "town-model",` from `CMakePresets.json`.
2. Download a model: from `https://huggingface.co/ggml-org/Qwen3-1.7B-GGUF` the file ending in
   `Q4_K_M.gguf` (about 1.3 GB). Save it as `E:\Projects\EngineStudy\projects\reference\models\town.gguf`
   (create the `models` folder).
3. Measure:
   `out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe town-mind projects\reference\assets\town\settlement.town.json --model projects\reference\models\town.gguf`
   and send the output. It shows each template beside the model's line, accepted or refused, and
   the time per line and per pick.
4. Start the game: the log says `Living town: model town.gguf loaded`. Walk up to residents, wait
   a second, press **E**: news now comes in the model's words; **L** shows them in the chronicle.
5. Optional, for comparison: the 0.6B model (`ggml-org/Qwen3-0.6B-GGUF`) and a 4B one, each as
   `--model <file>` in step 3.

Open until then: model choice, context size, CPU or GPU, the measured numbers, and whether the
packaged game (`scripts\package.bat`) carries llama.cpp's DLLs and the model file.
