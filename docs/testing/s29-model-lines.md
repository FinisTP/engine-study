# The Last Curtain, plan 3: the model's lines — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-162` … `#UMBRA-168`, and `#UMBRA-169` (these records). Built in a cloud copy
  of the checkout; delivered to the PC with `git am`. Design `docs/design/2026-11-15-model-lines.md`;
  plan `docs/design/plans/2026-11-15-model-lines-plan.md`.
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan). This
  build has **no llama.cpp**, so every test uses a stand-in model; the real model, MSVC and a GPU
  have not run this yet.
- State: **implemented**; the real-model check on Windows (§4) is open.
- User decisions (2026-11-15): the model words captions in the player's light, chat questions, goal
  lines, the Turn and the Taken's lines, in English; chat questions are reworded too; the game waits
  up to 3 s for a line; lines are not kept in saves; no model → the written lines.

## What was built

| Part | Where |
|---|---|
| A voice and a Taken voice per guest | `#UMBRA-162`, `guests.csv` `voice`, `takenVoice` |
| Requests (task, three examples, speaker and tone, who it is about, the template) and the guard (plain text, names kept and none added, pronouns, no flipped meaning, no outcome in captions, no hint in chat questions) | `#UMBRA-163`, `modules/nightmind`, `NightMind.md` |
| `Words`: the model on a worker thread, at most 4 lines waiting, late answers dropped, Stop waits only for the running answer | `#UMBRA-164`, `apps/LastCurtain/Words.*` |
| Play asks a seen bot's caption and goal line at its decision (on a copy of the night) and keeps it "thinking" until they are in, up to 3 s; the chat question and the Turn's two sentences wait the same way; seeds from the night | `#UMBRA-165`, `Play.*`, `caption`, `turn` screens |
| `LastCurtain.exe` loads `models/night.gguf` beside its assets folder when the build has llama.cpp; the save menu says when the written lines are used | `#UMBRA-166`, `Main.cpp`, `pause` screen, `.gitignore` |
| `UmbraTool night-mind --model <gguf>`: three bot nights, lines spread over each night, verdicts and times | `#UMBRA-167`, `tool/NightMindCommand.cpp`, `TryLines.cpp` |
| Final review: a Taken line keeps to the form's name | `#UMBRA-168` |

## 1. Automated checks (T132-T136)

| Check | Result |
|---|---|
| T132 voices load; a blank `takenVoice` is refused with its field | pass (RED first) |
| T133 requests name tone and pronouns; the guard's refusals and acceptances | pass (RED first) |
| T134 `Words`: quick answer, empty or refused, no model, late answer dropped, four waiting, Stop | pass (RED first; 30 repeated runs without a failure) |
| T135 a seen caption waits up to 3 s then shows the template; the model's caption with the template result; the same night with and without the model (`Fingerprint`); the chat question; the Turn's two sentences; seeds the same after a save; the save menu's note | pass (RED first; 20 repeated runs) |
| T136 `TryLines` with an echo model: captions, chat questions and the Turn over three nights | pass (RED first) |
| The `UMBRA_TOWN_LLAMA` branches of `Main.cpp` and `NightMindCommand.cpp` | compile (`-fsyntax-only` with the macro on); not linked or run here |
| Full suites (`verify.sh`: GCC, Clang, GCC ASan+UBSan) | GCC 55/55, Clang 55/55, ASan+UBSan 55/55 |

`ui-preview` of the save menu with `writtenLines`: the note shows under the buttons.

## 4. The user's check on Windows

1. Build with the model: `vcpkg` feature `town-model-gpu`, as for the living town.
2. Put the model the town uses (Qwen3 4B Q4_K_M) at
   `plugins/last-curtain/apps/LastCurtain/models/night.gguf` (or `models/night.gguf` next to a
   packaged `LastCurtain.exe`).
3. `UmbraTool night-mind --model plugins\last-curtain\apps\LastCurtain\models\night.gguf`: paste the
   end line (accepted of total, ms a line) and a few lines that read badly.
4. Play a night: bots' captions in your light, a chat question, the Turn screen. Do the guests sound
   like themselves? Is any wait (up to 3 s) noticeable?
5. Without the file: the night plays as before and the save menu says "Written lines".
