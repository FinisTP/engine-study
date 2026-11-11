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
  (`root ::= [A-Z] [a-zA-Z ,;:'-]{8,150} [.!?]`). No digits, quotes, brackets, line breaks or
  markup can come out, so a line cannot carry instructions, tags or code.

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

`--gpu` only helps if llama.cpp was built for a GPU; the vcpkg package is CPU-only by default,
which a small model does not need more than.

## Tests

`Umbra.Town` (T92) uses a scripted stand-in model: a function that returns what the test says.
No model file or llama.cpp is needed, so the tests run everywhere. The llama.cpp path itself was
exercised in the cloud with a tiny random-weights model (letters only make sense by accident, but
the grammar held for every answer and every pick was a valid number).
