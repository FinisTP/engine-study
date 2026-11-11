# The town's built-in model (S23)

A living-town game ships a small language model that runs on the player's PC, offline
(`docs/adr/0011-living-town.md`). This module is how the town uses it, and how it stays safe:
**the model only proposes**. It never changes the town itself.

Code: `Mind.hpp` (what a model is: text in, text out), `TownMind.hpp` (the town's use of it),
`LineGuard.hpp` (what a line must pass), `LlamaMind.hpp` (the real model through llama.cpp).

## What the model is asked

| Job | Asked | Answer may be | Checked by | If refused, late or missing |
|---|---|---|---|---|
| **Word the news** a resident tells the player | when the player comes near them | one or two plain sentences | the guard (below) | the template line (`Town.md` "Talking") |
| **Pick an activity** for a resident about to be free | up to three ticks ahead | the number of one activity that was legal when asked | `Legal` again when it is used | the rule-based `Choose` |

Favours (asked, thanked) are the developer's own lines and are never reworded. A resident with
an urgent need is left to the rules: emergencies are not the model's to steer.

### Grammar

Every request carries a **grammar** (llama.cpp's GBNF). The model can only produce text the
grammar allows: it is applied while the answer is written, token by token, so the answer is
never "almost" right.

- A choice among three: `root ::= "1" | "2" | "3"`. Nothing else can come out.
- A line: a capital letter, then letters, spaces and light punctuation, then `.`, `!` or `?`
  (`root ::= [A-Z] [a-zA-Z ,'-]{8,110} [.!?]`). No digits, quotes, brackets, line breaks or
  markup can come out, so a line cannot carry instructions, tags or code.

### The prompt

A small model follows examples far better than a description, so the wording request is worked
examples and then the real line (shown here with one example; there are three):

```
Reword one line of dialogue for a villager in a game. Keep every name and the meaning, add
nothing. Reply with the new line only: one short spoken sentence.

Speaker: Marta (tone: plain)
Line: They say Jonas has fallen out with Petra.
Reworded: Word is Jonas and Petra are not speaking any more.

Speaker: Old Bren (tone: warm)
Line: They say Tobin now counts Edda as a friend.
Reworded:
```

Two things learned from the first real run (Qwen3 1.7B, user's PC, 2026-11-11: 5 of 15 lines
accepted, most starting with nonsense like "INGLESE,"):

- **Thinking models.** Qwen3 begins its turn with a `<think>` block. The grammar forbids it, so
  the model was forced to start with some other token and derailed. `LlamaMind` now adds an empty
  `<think></think>` block after the chat template when the model's template mentions one.
- **Describing the speaker** ("You are ..., troubled by ...") leaked into the lines. The prompt
  now gives one word of tone and nothing else about them.

The second run (same model, `#UMBRA-94`): 13 of 15 accepted, 1.7 s a line. But the single example
was about a falling-out, and three residents then said "Tobin and Edda are no longer friends" for
a *new* friendship: the guard passed a line that said the opposite. So (`#UMBRA-95`) there are
now three examples of different kinds whose order turns with the speaker and the fact, a
resident may have a `voice` in the town file (`"voice": "gruff, few words"`), and the guard has a
seventh check that compares yes-and-no with the template.

The third run (`#UMBRA-95`): 15 of 15 accepted, 2.4 s a line, nothing turned around. But the
lines are the examples with the names changed ("... have become good friends, from what I hear",
"... knows her trade now, I watched her myself"), also from residents who had only heard: a
1.7B model copies rather than writes. An eighth check now refuses a claim to have seen something
from someone who was not there. Whether a larger model writes its own lines is the open question.

`UmbraTool town-mind ... --show-prompt 1` prints the first prompt exactly as the model gets it.

## The guard (`LineGuard`)

A grammar keeps the *shape* right; the guard checks the *content*. It is a list of small checks
run in order; the first that objects wins, the reason is counted and kept
(`MindStats::lastObjection`), and the player sees the template line instead.

| # | Check | Objects to |
|---|---|---|
| 1 | clean text | anything but printable English characters; `< > { } [ ] * # \` |
| 2 | length | fewer than 8 characters, more than `maxLength` (160) |
| 3 | banned words | any word or phrase in the project's list, whatever the case |
| 4 | only their names | naming a resident who is neither part of the fact nor the speaker (inventing) |
| 5 | keeps their names | not naming someone the fact is about (the line lost its fact) |
| 6 | not shouting | a line that is mostly capital letters |
| 7 | same yes or no | a line that denies ("not", "no longer", "never", ...) where the template does not, or the other way round |
| 8 | no false witness | "I saw", "I watched", "myself" from someone who only heard of it |
| 9 | no new source | "I heard it from the miller", "... told me", "according to ...": a person or source the fact does not have |

The list and the limit are data: `town/lines.rules.json` (`{"maxLength": 160, "banned": [...]}`).
A project adds its own checks in code: `guard.filters.push_back(myCheck)`; a check is a function
that returns an empty text or the reason it objects. This is the place for later layers (a
second model as a judge, a tone check, other languages).

Checks 4 and 5 are how "stay on the fact" is enforced without understanding the sentence: the
people in the line must be exactly the people in the fact. It does not prove the sentence says
the same thing; that needs play-testing with the chosen model (`UmbraTool town-mind` prints
every line beside its template for that).

## Never waiting, never re-rolling

- **Off the frame.** One worker thread answers requests; the game takes finished answers once per
  frame (`Collect`). The queue holds six; a seventh pushes out the oldest.
- **Ahead of time.** A line is asked for when the player comes *near* a resident, not when they
  press the key. If it is not ready when they talk, the template is used at once (`late`).
- **Stale answers are dropped**: a line for news the resident has since told, a pick that is no
  longer legal, anything asked before a game was loaded (`Forget`).
- **What was said stays said.** A shown line is stored in the town's state (`TownState::said`)
  and so in the save. The chronicle shows these stored words; a loaded game never asks the model
  for them again. A model's pick changes the state like any other intent, so it is saved too.
- **Seeds.** Each request carries a seed made from the town's seed, the tick and the resident, so
  one request to one model file gives one answer.
- **Forks do not ask the model.** A copy of the town run ahead (`Town.md` "forks") uses the rules
  only, so looking ahead is fast and repeatable; the real town may then go a slightly different
  way where the model picks differently. S24's storyteller has to allow for that.
- **Shutting down** waits for the one answer being written (a model call cannot be interrupted).

## The model (`LlamaMind`)

llama.cpp runs a model file in GGUF format. The game looks for

```
projects/<project>/models/town.gguf
```

(beside `assets`, not inside: it is large and not packed with them; git ignores it). With the
file, the log says `Living town: model town.gguf loaded`; without it, or in a build without
llama.cpp, the town runs on rules and templates and says so. Nothing else changes.

The build gets llama.cpp from vcpkg (`llama-cpp`, feature `town-model` in `vcpkg.json`, switched
on by the `windows-msvc` preset). If that package gives trouble, remove
`"VCPKG_MANIFEST_FEATURES": "town-model"` from `CMakePresets.json`: everything else still builds.

Which model: any chat model in GGUF form that llama.cpp supports. First candidates (ADR 0011):
Qwen3 0.6B and 1.7B, 4-bit (about 0.5 GB and 1.1 GB, Apache-2.0); the budget allows up to
4-5 GB (user, 2026-11-11), so a 4B model can be tried too. Choose by measuring:

```
UmbraTool town-mind projects\reference\assets\town\settlement.town.json --model <file.gguf>
          [--threads 6] [--gpu 99] [--lines 3] [--days 9] [--seed 1]
```

prints, for every resident, the template and the model's line, whether the guard accepted it and
the milliseconds; then every resident's pick; then the totals. A model is good enough when most
lines are accepted and read well, picks are all valid, and a line takes well under the second or
two a player needs to walk up to someone.

### On the graphics card

Measured on the user's PC (2026-11-11, CPU, 8 threads): Qwen3 1.7B 2.4 s a line but it only
copies the examples; Qwen3 4B writes its own lines ("Edda's getting the hang of things, I
suppose") but takes 6.0 s a line and 2.4 s a pick. So the 4B model needs the graphics card.

The vcpkg feature `town-model-gpu` builds llama.cpp's engine (ggml) with **Vulkan**, the same
graphics interface the game draws with, so it works on NVIDIA, AMD and Intel cards and needs no
extra SDK beyond the Vulkan SDK the project already uses. The first build also compiles shaderc
(a shader compiler), once. The `windows-msvc` preset switches it on
(`"VCPKG_MANIFEST_FEATURES": "town-model;town-model-gpu"`); if it fails to build, remove
`;town-model-gpu` and the model runs on the CPU as before.

By default every layer of the model goes to the card (`LlamaOptions::gpuLayers = -1`). The model
then shares video memory with the game: a 2.5 GB model on a 4 GB card leaves little. If loading
fails or the game stutters, `--gpu 20` (or another number of layers) keeps part of the model on
the CPU; `--gpu 0` is CPU only. `UmbraTool town-mind` prints which it used.

## Tests

`Umbra.Town` (T92) uses a scripted stand-in model: a function that returns what the test says.
No model file or llama.cpp is needed, so the tests run everywhere. The llama.cpp path itself was
exercised in the cloud with a tiny random-weights model (letters only make sense by accident, but
the grammar held for every answer and every pick was a valid number).
