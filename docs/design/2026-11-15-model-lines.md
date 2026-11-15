# The Last Curtain: the model's lines (plan 3)

> **Status:** Approved by the user 2026-11-15 (open questions answered); not started
> **Author:** the user + Claude · **Last updated:** 2026-11-15
> **Quick reference** — Layer: presentation over the night rules · Key deps: the night (`Night.md`), the narrator (`Narrate.hpp`, `captions.json`), the built-in model (`plugins/living-town/modules/mind`, S23)

Marks: **[call]** = Claude's fill-in for the user to change. "User" = decided by the user.

## Summary

The built-in model rewords what the player sees and hears so the night sounds like seven people in
a haunted opera house instead of a log. It only words lines; it never decides anything. Every line
has a written template that is always ready, and the template is what shows when the model is
missing, late or refused.

## Decided by the user (2026-11-15)

- The model words what the player sees and hears: captions of actions in the player's light, the
  player's chats, goals done or failed, the Turn, and the Taken's lines.
- English.
- No model file or no capable graphics card: the night plays on with the written lines.
- The chat questions are reworded too (guarded; the answers stay as written).
- The game waits up to **3 seconds** for a line before it shows the written one.
- The model's lines are not kept in saves.

## Why

Plan 2c made the night readable, but every guest speaks in the same flat captions ("Le Thanh talks
with Vo Minh Khang."). The cast has voices on paper (`cast.md`); the player never hears them. The
lines are where the guests' personalities reach the player, which is what makes "who will turn?"
a question about people.

## Goals, non-goals, anti-goals

- **Goals:** each guest sounds like themselves; the Taken sounds like a distorted echo of them;
  the player never waits on the model; a night with the model plays exactly the same moves as
  without it.
- **Non-goals:** the model choosing bot actions; typing free text to guests; voice audio;
  languages other than English; rewording numbers, dice or rules text.
- **Anti-goals (a failure even if it works):** a line that tells the player something false (a
  wrong name, a wrong pronoun, a hit that missed); a line that hides which chat answer is the safe
  one; any line that could carry instructions or markup; a pause the player can feel.

## What the model words

| Line | When | Template (today) | Model's job | Never reworded |
|---|---|---|---|---|
| **Caption** of an action in the player's light | the beat is made (`Narrate`) | `captions.json` caption | say it as a short stage-direction-like line, in the night's mood | the `result` line (dice, damage, items: numbers and facts stay templates) |
| **Chat question** a guest asks the player | the chat opens | `questions.csv` `text` | say it in the asker's voice | the two answers and their stat and target |
| **Goal done / failed** | the log says so (in light, or the player's own) | `goalDone` / `goalFailed` | one line spoken by that guest | who did or failed what |
| **The Turn** | midnight, once the Taken is drawn | the Turn screen's text | two sentences: the guest's last human words, then the Taken's first | who turned, the form's name |
| **The Taken's lines** | it acts in the player's light | the action's caption | a taunt in the form's voice | the result line |

Five kinds, one request shape: *line kind, speaker, tone, who it is about (names and pronouns), the
template line*. The answer must keep every name and the meaning (`Mind.md`, "Word the news").

### Voices [call]

`guests.csv` gains two columns, a few words each, used as the speaker's tone:

| Guest | `voice` | `takenVoice` |
|---|---|---|
| Nhi | polite schoolgirl, short sentences | sing-song, childlike, cruel |
| Thanh | theatrical, warm, proud | operatic, burning, mocking |
| Bao | careful doctor, formal | drowned whisper, clinical |
| Khang | blunt, few words | flat debt-collector's threats |
| Mai | dry, practical, knows the house | slow, rooted, the house speaking |
| Huy | curious student, quick | echoing, everything twice |
| Lan | cool, precise, appraising | mirrored, repeats you back |

## How a line is made

1. The rules run and the template line is made exactly as today.
2. A request goes to the model on a worker thread when the beat is made. The game **waits for it,
   up to 3 s** (user), before the line goes on screen: a bot's thinking pause becomes
   `max(0.4 s, decision time, line ready)`, capped at 3 s; the player's own caption, chat question
   or goal line waits the same way. The user's PC measured 0.8 s a line for Qwen3 4B on the
   graphics card (2026-11-12), so most lines arrive well inside the cap.
3. The answer passes the guard or is dropped. Past 3 s, empty or refused: the template shows. A line
   is never swapped after it is on screen (no flicker) [call].
4. The model's output goes into the beat's caption only; nothing in `NightState` changes.

The Turn and goal lines can be asked a little ahead: the Taken is known the moment it is drawn,
and the Turn screen's fade-in (about 1 s) covers the wait.

### Grammar and guard (reused from S23)

- Grammar: one line, a capital letter, letters, spaces and light punctuation, ending `.`, `!` or
  `?`, 8 to 120 characters. No digits, quotes, brackets, markup or line breaks can come out.
- Guard (the town's `LineGuard` checks, plus two for the night):
  every name in the template is still there; no other guest's name is added; pronouns match
  `guests.csv`; no claim of the opposite (yes/no check); length; **a chat question must not name
  either answer's stat or say which is easier**; **a caption must not state an outcome** (it is
  shown before the result).

### Same seed, same words

Each request's seed is `night seed + beat number`, so a replayed night with the same model file
words the same lines. Lines are **not** stored in the save (user): a save is taken at a round's
start, when nothing worded is on screen. (The living town stores its worded lines because they
become rumours the town repeats; here a line is shown once.)

### Where the model lives

- The model module is the living town's (`Umbra::Mind`, `LlamaMind`, llama.cpp feature
  `town-model-gpu`). The last-curtain plugin lists `living-town` in its `plugin.json` `requires`
  (plugins already do this, e.g. `rpg-session`) instead of a second copy [call]. Only
  `LastCurtain.exe` links the model; the night's rules and the soak stay model-free.
- File: `models/night.gguf` next to `LastCurtain.exe` (or under the source tree's
  `plugins/last-curtain/apps/LastCurtain/models/`), never committed. Default: the same Qwen3 4B
  Q4_K_M the user measured.
- No file, a load failure or no Vulkan device for the model: one log line, a note in the pause
  menu ("Written lines: the model is not installed"), and the night plays on.

## Approaches

- **A. Reword templates (chosen [call]).** The model gets the template line and rewords it. Facts
  come from the rules; the guard can compare. Proven on the living town (15 of 15 accepted).
- B. Write lines from facts. Freer prose, but the guard has nothing to compare against; small
  models invent.
- C. Pre-write many variants per template offline (the editor's authoring tool) and pick at
  random. No runtime model at all; less alive, no voices for every situation. A fallback if the
  model proves too slow on lower-end PCs.

## Testing

- A scripted stand-in model (as `tests/localai` and the town's tests): good lines, empty answers,
  late answers, refused lines.
- The night with the stand-in plays the same actions and ends in the same state fingerprint as
  without it (the model cannot change the night).
- Guard: refuses a dropped name, an added name, a wrong pronoun, a chat line that hints at the
  easy answer, a caption that tells the outcome.
- Fallback: no model file → templates, one log line, no crash.
- The user's real-model check on Windows: a list of 30 lines from three nights (`UmbraTool
  night-mind --show`), accepted count, seconds per line, and a read for voice.

## Answers (user, 2026-11-15)

1. The chat question is reworded, guarded; the answers stay as written.
2. The longest wait is 3 s; the game waits for the line up to then.
3. Lines are not stored in saves.

## What is left to the builder

Prompt wording and examples per line kind (three examples each, as the town learned), queue size
(one job per kind, newest wins), the pause-menu note's wording.
