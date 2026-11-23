# The Last Curtain: campaign slice 1 — handoff for building (2026-11-23)

Everything a new session needs to build the campaign's first slice, in one place. The design is
**approved by the user (2026-11-23 14:03)**. This document gathers every decision by topic; the
design doc `2026-11-23-campaign-slice-1.md` keeps the same rules with section numbers, the history of
how they were reached (sections 11–14) and the reasons. If the two ever disagree, the design doc wins
and the difference is a bug in this file.

---

## 0. Start here

1. Read the repository's `CLAUDE.md` (code style, constraints, where Claude can verify) and
   `docs/CONTEXT.md` as it asks.
2. Read this file, then the design doc's section 7 for detail on the step you build.
3. Build **one step of section 9 at a time**: write its plan with the superpowers:writing-plans skill,
   save it as `docs/design/plans/2026-11-2x-campaign-plan-<n>-<name>.md` (the folder's convention),
   build it with tests, record it, then the next step. **The first step is plan 1** (section 9.1).
4. The user answers in Vietnamese-native judgement on all Vietnamese text and decides anything marked
   "the user's to decide" below. Proposals still open: none; every earlier proposal was accepted or
   changed by the user on 2026-11-23.

### Where the work stands

| Done | Commits | State |
|---|---|---|
| Art for the whole cast (sprites with faces and bound forms, backdrops, café wall, boon icons) | #303–#311 | in `plugins/last-curtain/apps/LastCurtain/assets/art/`, see its `README.md` |
| The café's painted back wall in the room | #312 | Linux 58/58, capture `captures/2026-11-23-cafe-back-wall.png`; Windows look unchecked |
| This design, approved | #313–#320, #322, #323 | `docs/design/2026-11-23-campaign-slice-1.md` |
| **Engine text field** (typing the player's name, Vietnamese included) | #321 | Linux 58/58; Windows typing check open (`docs/testing/s43-text-field.md`) |

Nothing of the campaign itself is built yet: no stage files, no `campaign` module, no rule switches.
Still-open Windows checks from before: `docs/testing/s42-tiles-groups-cafe.md` §4 (plan 4), which the
design wants done before relying on the slice's results on Windows.

---

## 1. What the slice is

The first playable piece of the new design: a living guest's first night, through three tutorials and
the first zodiac stage, to the moment **Bé Tí** remembers who he was and joins. It proves the new loop
**fight → free → café → remember → join**, and tests the big bets together: a tactical RPG with biding
and falls that stay down, a calm café where the dead get their memories back, **Vietnamese throughout**,
and a built-in model that words memories without inventing them.

**Flow:** title → new game (typed name, male or female) → **T1 Khách lạ** → **T2 Quán cà phê** →
**T3 Hành lang chân dung** → **Stage 1 Phòng vé** (the Rat) → the café, where **Lệ Thanh** regains her
first power (restoring memories) → two café visits restore Tí → Tí joins. Between stages the player
picks the next stage, a cleared stage to replay, or the café.

**Not in this slice:** stages 2–12 and the finale; the quick match's 12-against-12 pick; boons, power
cards, permanent boosts, auto-battle; heroes and boon givers in the café; café props after each area
(a ticket rack only if cheap); zodiacs adapting through the learned policy; free-typed café answers;
the storyteller choosing visitors; painted cutscene stills; new sounds and music (reuse); Lệ Thanh's
other forms in play; **drink serving** (off for now); voicing in the game.

**Failures even if it works:** the café becomes busywork (any timer, patience bar or fail state); the
model invents a fact or a memory fragment; English or broken Vietnamese reaches the screen; the
campaign breaks the faction match (its tests and soak must pass); a fight ends without the player
seeing why; any gore, bodies or skulls, or a real person's likeness.

---

## 2. Fight rules

### 2.1 One engine, switches set by data

Campaign and faction match share one fight engine (the `match` module: board, battles, abilities,
items, monsters, bots). A `Rules` value inside `MatchState`, filled from data at the start, turns
named rules on or off:

| Switch | Faction match | Campaign | Meaning |
|---|---|---|---|
| `essence` | on | off | essence drops, carrying, banking, the stage race and score |
| `perform` | on | off | Perform (takes a turn) |
| `respawn` | on (1 turn, at the hub) | off | downed units return at their hub; hubs and guardians |
| `biding` | **off** (later) | on | 2.3 |
| `fallsStay` | off | on | 2.4 |
| `playerLoss` | off | on | the living player falling ends the stage |
| `win` | score after 30 rounds | `boss` | won when the `boss` unit is defeated **plus any extra condition the stage sets** (Stage 1: Tí freed); lost if the whole side is down |
| `roundLimit` | 30 | 0 (none) | a stage may set a limit as its aim |

One later campaign stage turns `perform` and `respawn` back on as its gimmick.

### 2.2 Turn order

**Every unit, monsters included, acts in one order by Speed, in both modes, no exception.** The
monsters' phase is removed (today's code has it: `monsterTurn` in `Match.hpp`, `Turn.cpp`). A unit
that appears mid-fight (a reinforcement, a rat specter) and a revived unit join the order at their
Speed and act from the **next round**. Ties keep today's seeded key. Re-run the faction match's soak;
its balance targets stay.

### 2.3 Biding and action points (AP), campaign only

- Instead of moving and acting, a unit may **bide**: the turn passes and it gains **1 AP**. No Defence
  bonus: high risk, high reward.
- It stores **at most 2 AP** and spends **at most 2 a turn**: at most 3 actions in a turn. No borrowing.
- **Each extra action comes with its own move.** 1 AP buys one more move and action.
- **Abilities can cost AP** (`abilities.csv` gains an `ap` column, beside the HP cost). The ability's
  AP is paid first, from the same pool and the same limit of 2; only what is left buys extra actions.
  With 2 AP: normal turn + 2 extra; or a 1-AP ability + 1 extra; or a 2-AP ability alone. With 0 AP,
  AP abilities are greyed out with the reason.
- **AP is lost when a unit falls**; every unit starts each stage at 0 AP.
- Monsters may bide; AP shows as **pips** under a unit and on the order strip. **Bosses' biding is the
  difficulty ramp:** early bosses are passive and store no AP (**the Rat never bides**); later bosses
  take 3-action turns.
- New action `Bide`; new ability effect `give-ap` (Tí's *Đưa tin*, used later).

### 2.4 Falls, reviving, removal

- With `fallsStay`, a unit at 0 HP **falls**: stays on its square, **can't be hit**, skipped in the
  order, drawn as fallen, **doesn't block** (others pass and stand there; it doesn't count toward the
  square's limit of 2).
- **Revive** (`revive` effect: a spell or an item) raises a fallen unit on its own side with part of
  its HP. If its square already holds 2 units, it stands on the nearest free square; ties go to the
  square nearest the reviver, then a fixed order. **Enemies can revive** their own, but **nobody on the
  Rat's side revives in Stage 1**. Bà Mai's cold rice revives (T3 and Stage 1).
- **Removed for good** (`remove` effect): a fallen unit leaves the stage, can't be revived in it, and
  returns next stage (allies too). No unit in this slice has it; build and test it anyway.

### 2.5 Game over and the rite

- **The living player falls → game over** at once (the whole side down can't happen otherwise).
  Choices: **Thử lại** (restart the stage), **Về quán** (café), **Thoát** (quit). **Before the café is
  unlocked (T1) only Thử lại.**
- **A lost stage keeps nothing**: back to the save from before the stage, as if never started: XP,
  items and the café's timeline unchanged.
- **Lệ Thanh's rite of resurrection** (*Lễ cầu hồn*): revives every unit on the player's side to full
  HP, **once in the whole game**. Unlocked after T3; offered on the game-over screen while unused; the
  fight goes on where it stopped. **Once used it stays used, win or lose** (saved at once).

### 2.6 Strings and the gold knot (freeing recruits)

Red strings mark a zodiac's control; a **gold knot** marks a person to free (recruits, later boon
givers).
1. A gold-knot specter **falls at 0 HP** like anyone. It is freed when **the player**, standing next
   to it, **cuts** its strings by spending an action. Until then its strings hang slack and its side
   may revive it strung.
2. **Only the living player can cut** (with Lệ Thanh's fan).
3. A specter **never cut, or removed for good, is gone for good**: it doesn't return in later stages;
   the only way to recruit it is to **replay its stage**. In the finale (Lệ Thanh's stage) every recruit
   never freed returns as an enemy and can't be freed there.
4. Freeing isn't needed to win, **except in Stage 1**: there the win needs **Tí freed and the Rat
   defeated**. **If the Rat falls while Tí is still strung, Tí is trapped and the stage is lost.**
5. Ordinary red ghosts are simply defeated in this slice.
6. Zodiacs drawing power from strings: later bosses, not now.
7. On screen: the knot on the strings above the head; slack strings on a fallen strung specter;
   cutting plays a snap and a warm gold light.
The freeing is logged (when, where, what happened) for the café to remember.

### 2.7 Items, storage, HP, XP, levels

- **Each character carries 4 item slots; that is all a fight has** (capacity is part of the game).
  Items picked up are **kept between stages**; a used item is gone. **Each map item can be taken
  once**: a replay offers only the ones not yet taken. Items from a lost attempt are lost with it.
- A character with 4 full slots may **swap** on an item's square; the dropped item stays there.
- **The café shelf** is the party's storage: items not in anyone's slots wait there (Lệ Thanh's gifts
  first).
- Ability slots: **as many as today's career**. New abilities come from levels and the story.
  **Assignment** of abilities and items happens in the café and on the screen before a stage.
- **Everyone's HP is restored between stages**, fallen allies included.
- **XP:** a fixed reward for each **stage won**, set in the stage file, shared by the party, sized for
  about **one level a stage**; no XP per kill. Units sitting out earn part. Replays of a cleared stage
  give XP up to its cap; lost tries give none.
- **Levels:** cap **15**, new XP steps in data; the slice reaches level 4 at most. **Recruits join at a
  fixed level** (data): **Tí at 3**; **Bảo and Bà Mai at 2**.

### 2.8 Enemies

- Built like a player character **of a class at a level, then scaled**: a **zodiac is buffed** above
  that line (attack and HP about **+33%**, other stats as the class); an **ordinary ghost has about
  60%**. Example: a level-3 attacker has 6 attack; the Rat, a level-3 fighter, has 8; a ghost about 4.
  Difficulty settings later change the buff.
- If stats aren't enough, balancing may give a zodiac an **ability**, e.g. the Rat's **Speedkill**
  (double damage against an attacker slower than him).
- **Behaviours** (per unit in the stage file): **aggressive after n rounds** (waits, then comes for
  the party); **passive** (attacks only what it sees: the board's light rule, same room, up to 4
  squares); **reinforcement** (appears at round n on a named square, with the existing "spawns soon"
  marker one round ahead).

---

## 3. Stage files and the cast sheet (new `campaign` module)

`assets/campaign/stages/<id>.json`, one per tutorial or stage, versioned and validated before use (a
bad file names the file, field and fix; the stage doesn't start):

- `id`; Vietnamese and English titles (English for logs and tools only); recommended level; fight
  backdrop; music; the **XP reward**.
- `map`: the room as rows of text, one character per square (wall, floor, door, window, hazard…),
  read into the existing `Board` (drawing, paths and light rule unchanged). **Campaign maps are not
  symmetric**: the party starts clustered, monsters are spread so the player takes them in an order;
  the mirror fairness check is off for them.
- `party`: who fights and start squares. `units`: monster rows, squares, sides, nature (friendly,
  neutral, aggressive), flags (`boss`, `recruit`), height scale (ordinary ghosts 0.8), behaviour.
  Sides: party = 0; enemies = 1, hostile to 0 only; friendly and neutral ghosts on their own sides.
- `items` on squares; `rules` (2.1); `gimmicks` with numbers. Stage 1's **ticket windows** open and
  close each round and let out rat specters (existing spawn code); items the Rat steals sit on a window
  until he is hit, then drop on his square for anyone to pick up.
- `aims`: the stage-start hint (Lệ Thanh's voice), the tutorials' teaching beats, the recap after the
  fight (which names anyone left strung).
- `scenes`: dialog scenes (section 4) at the start, the end and at **any moment** of the fight: a unit
  first attacked, a unit falls, a round begins, the boss below some HP, a freeing.

**The cast sheet** `assets/campaign/cast.csv` links each character to its stats row, sprite folders
(`<id>`, `<id>_bound`), form, voice sheet and memory fragments. This is where the 2026-11-23 sprites
(`assets/art/sprites/`: `player_male`, `player_female`, `lethanh`, `ti`, `ti_bound`, `rat`,
`zodiac_rat`, `cafe_bao`, `cafe_mai`, `ghost_*`, the recruits and their `_bound`…) are hooked in.

### The slice's content

| | Party | Enemies | Teaches | After |
|---|---|---|---|---|
| **T1 Khách lạ** | the player alone, level 1 | rat specters (aggressive) | moving, attacking | Lệ Thanh's rescue (scene); tuned so falling is unlikely |
| **T2 Quán cà phê** | — | — | café talk; **assigning abilities and items** (Bảo and Bà Mai bring unassigned abilities; Lệ Thanh gives basic items) | Bảo and Bà Mai join for good |
| **T3 Hành lang chân dung** | player + Bảo + Bà Mai, level 2 | specters, a mini-boss | biding, falls, reviving, game over | the rite unlocks |
| **Stage 1 Phòng vé** | player + Bảo + Bà Mai, level 3 | **the Rat** (level-3 fighter, buffed, about 40 HP; *Móc túi* steals an item from an adjacent unit; *Chạy làng* darts 3 squares after a battle; never bides; nobody revives on his side), rat specters from the ticket windows, **Bé Tí** strung with the gold knot | items, stealing, **the recruit mechanic** (Tí freed is part of the win), the first revive item (a bandage) | Lệ Thanh's story piece 1 (money); she regains **restoring memories** |

Kits for Bảo and Bà Mai are in `cast-2-2026-11-22.md` (scalpel, diagnosis, the death paper; tend,
mend, cold rice). Play length targets for the soak: T1 about 5 minutes, T3 about 10, Stage 1 15–20.
In-fight story line example: the Rat, first attacked: "Tao cần tiền, Lệ Thanh, mau đưa tiền cho tao!"

---

## 4. Dialog scenes (the user confirmed this as written)

A generic RPG conversation used in the café, before and after a fight, and inside one.

```
 (the café room, the board, or a backdrop, dimmed behind)
  [Tí (lit)] [Mai (dim)]                         [Rat (dim)] [    ]
 ┌─[ Bé Tí ]───────────────────────────────────────────────────┐
 │ Chị ơi, con nhớ ra rồi! Mạ con hứa Tết ni…                 ▼│
 └──────────────────────────────────────────────────────────────┘
                                               [Nhật ký] [Bỏ qua]
```

- **0–2 portraits on the left, 0–2 on the right**; a line brings one in, takes one out or swaps;
  portraits face the middle (right side mirrored).
- **Name tag** on the box, on the speaker's side; the speaker lit, others dimmed; a line with no
  speaker is narration (Lệ Thanh's dancing in the café is told this way).
- **Portraits** from each sprite's body and a face (calm, talk, happy, angry, afraid, scheming,
  near-dead), set per line, drawn in the character's form (strung specter red with strings, ally
  blue, ghost pale). The player's portrait and name come from the save.
- Text appears letter by letter; a click or Space completes the line, the next goes on. **Bỏ qua**
  skips to the end but **stops at a choice**; **Nhật ký** shows the lines so far. Auto-advance later.
- **Choices:** 2–4 at the end of a line; a choice sets a saved fact and may jump within the scene.
- **Inside a fight** a scene waits for the current action to finish, pauses the fight, and the fight
  resumes where it was. Each moment's scene plays once per attempt.
- Lines are authored; in café talks a line may be worded by the model from authored facts, with the
  authored line as fallback. A line may name a sound file (plays if present).
- **Data:** `assets/campaign/scenes/<id>.json`, versioned, validated (unknown character, a third
  portrait on a side, a jump to a missing line → file, line, fix). A headless scene player (line, who
  is where, choices, facts); drawing by `ui/dialog.ui.json` plus portraits. The night game's
  camera-shot cutscenes stay as they are.

---

## 5. The café (calm)

- **Gone:** the 150 s shift, patience timers, tips, wrong-drink failure, takings, money. **Drink
  serving is off for now**; the stations stay in the room, unused.
- The walkable 3D room as built, with the painted back wall. **Lệ Thanh** walks around, appears at
  random places and starts conversations like any guest; she doesn't dance in play (her dancing lives
  in narration; the `lethanh/dance` painting isn't used).
- **Guests in the slice: only Bảo and Bà Mai**, plus Tí once freed. Zodiacs never appear.
- **A visit** = talking: a dialog scene of a few lines and choices. No talk can be failed.
- **Assignment** of abilities and items (taught in T2). More hub features later.
- **Restoring Tí:** four authored fragments (his mother's name, the Tết promise, the message, the white
  sand); **each visit restores two**; then he joins, glowing blue, cloth gone. **A new visit opens after
  a won fight** (next stage or a won replay); a lost fight doesn't move the café's timeline. In the
  slice the second visit therefore needs a won replay; the user will handle that later without
  complicating the tutorials. Tí's lines may mention how he was freed (fight facts).

---

## 6. Save and flow

- **Campaign save v2**, one file, versioned, written whole (temp file then move): name and gender;
  chapter; each character's level, XP, stats, abilities, upgrades, 4 item slots and assigned
  abilities; the party; the café shelf; map items taken; whether the rite was used; who was freed,
  where, how; each recruit's fragments restored; Lệ Thanh's unlocks; café visits; answers that count.
- The faction match's v1 career is **not migrated**; the faction match works without a career.
- **No save in the middle of a stage**: written only between stages and in the café; quitting a stage
  loses it. A **developer-only mid-stage save** (tool command or debug option, off in a shipped game)
  stores the `MatchState` for debugging and balancing.

---

## 7. Words, Vietnamese, model, voice

- **Everything on screen in Vietnamese.** The game's words files get a Vietnamese set chosen by an
  option `language` (default `vi`); the English set stays for development and the later quick match.
  Tests check ids and structure, not wording. C++ string literals stay ASCII; Vietnamese lives only in
  UTF-8 data.
- **Authored text:** Claude drafts the Vietnamese (lines, fragments, aims, menus); **the user reviews
  it as a native speaker**. Lines use `{player}` and **gendered address tokens such as `{anh/chị}`**
  filled from the save. The line guard gains Vietnamese checks.
- **The name** is typed in the engine's new text field (#321, section 8).
- **Fonts:** Poppins and Abril Fatface lack ư, ả, ắ, ợ, ậ…; replace them with **Be Vietnam Pro** (body)
  and **Playfair Display** (titles), OFL, pinned, listed in the third-party notices. Lora is complete.
  The engine already packs U+1EA0–1EF9.
- **The model's Vietnamese test comes first.** A tool command runs a model on the user's PC over a fixed
  test set (Tí, Bảo, Bà Mai and Lệ Thanh in a handful of café situations, from voice sheets and
  fragments), checks each line automatically (diacritics present, no English words, the never-say
  list, length, dialect markers) and writes a report. **Pass:** the user is satisfied and at least 90%
  of lines pass the checks.
  - **Arcee-VyLinh 3B first** (Q4_K_M, 1.9 GB, `QuantFactory/Arcee-VyLinh-GGUF`, Qwen2 architecture, so
    the pinned llama.cpp loads it). Licence: card says Apache 2.0, but it merges Qwen2.5-3B-Instruct
    (qwen-research, no commercial use without Alibaba's permission): fine to test, settle before
    shipping.
  - Then **side by side** (one report per model): Qwen3 4B (today's default, 0.8 s a line on the user's
    RTX 3050 4 GB) and candidates the user downloads. Sailor-4B-Chat has the same Qwen licence problem;
    Gemma-SEA-LION v4 4B and SEA-LION v4.5 E2B are under Gemma terms (read before shipping). A
    candidate must load in the pinned llama.cpp (the tool says if not), fit about 3 GB of video memory
    beside the game, answer in under about 1.5 s a line, and carry a licence that lets it ship. A newer
    llama.cpp is its own reviewed change. **The user picks the winner.**
  - Model files go in `projects/<project>/models/` and are **never committed**. The cloud can't
    download models; tests use the scripted stand-in model.
  - **If it fails:** every line is hand-written and the model only chooses among authored lines (or is
    left out).
- **Voice test** (outside the game): **VieNeu-TTS v3 Turbo** (Apache 2.0, local), which the user runs
  at `http://127.0.0.1:7860` (Gradio; the built-in browser pane can reach it; its API needs one queue
  session for loading the model and generating: `load_model`, then `wrapper` with the text and a
  preset voice name). ComfyUI's installed voice nodes are paid cloud services: not used. Voices: a
  Central voice for Tí and Bà Mai (Quang Sơn / Ngọc Trân), a Southern one for Bảo (Thái Sơn), Lệ
  Thanh's picked by the user (samples: Ngọc Linh, Thục Đoan). **Tí's child voice:** pitch a preset up
  +3 to +5 semitones with formant correction; the user judges. A line took 10–15 s to make. No cloning
  of real people. If voice is kept, only authored lines are voiced, ahead of time, as Ogg files; model
  lines stay text.

---

## 8. On screen

- **Forms** (tints, glows, opacities in `match-look.json`): player solid; allies blue glow and name
  plate; recruit specters red tint, strings, gold knot; café ghosts pale glow; freeing and heroes a
  warm gold rim; ghosts green / blue-grey / red by nature; zodiacs strong red glow, thick strings; Lệ
  Thanh a faint warm glow.
- **Strings:** glowing lines from each sprite's `anchors.json` up out of view, swaying; the anchors
  are estimates (a fix-up tool or hand edits where wrong).
- Fight backdrops: `backdrops/foyer_today`, `portrait_corridor`, `box_office`.
- **The order strip** shows monsters too, in the Speed order, with AP pips.
- New screens in the data-driven UI (`ui/*.ui.json`): game over, stage list, title with name and
  gender, assignment, dialog.
- **The text field (built, #321):** a `field` widget (`engine/ui/include/UI.md`, "Text fields"):
  `{ "type": "field", "id": "name", "placeholder": "...", "max": "24", "action": "named" }`. The game
  calls `window.SetTextInput(ui.WantsText())` each frame (already wired in the Last Curtain loops and
  the engine app); Enter sends the action with the text; `FieldText` / `SetFieldText` read and set it.
  Limits: caret only at the end; decomposed tone marks aren't joined (Unikey's default is fine).

---

## 9. Order of work (one plan each, built and checked before the next)

1. **Vietnamese model test** tool and test set, **Arcee-VyLinh first**, then the side-by-side
   comparison; the **voice test script** (VieNeu, with Tí's pitch shift); **the fonts**.
2. **Rules:** the switches, the one Speed order, biding (campaign), falls, revive, removal, game over,
   boss win and extra win conditions, freeing, the rite; the faction match's tests and soak still pass.
3. **Dialog scenes:** scene file, headless player, dialog screen and portraits.
4. **Stages:** stage file and cast sheet, the stage builder, monster behaviours and enemy scaling,
   T1, T3 and Stage 1 data and their scenes, the Rat's kit and the ticket windows, the campaign soak
   (wins, falls, turns, play-time estimate; a round cap in the soak only).
5. **Flow and save:** save v2, the stage list, the game over screen, retries, XP, the developer
   mid-stage save, name and gender (with the text field).
6. **The café:** the calm room, visits and talks, the shelf and assignment, restoring Tí, Lệ Thanh's
   unlock, fight memory.
7. **On screen:** forms, strings, gold knot, name plates, backdrops, pips, Vietnamese throughout.

**Checks across the plans:** headless tests for every rule above (each switch in both modes, the
Speed order with joining and revived units, biding limits, AP lost on a fall and reset per stage,
AP-cost abilities, the rite once per game and kept after a loss, 4 slots, shelf and swap, items taken
once, T1's retry-only game over, falls that don't block, revive placement, Stage 1 lost if the Rat
falls with Tí strung, the three behaviours, scenes firing once and the fight resuming, stage and scene
file validation, save v2 round trip and corrupt/old saves, no mid-stage save outside the developer
option, a lost stage leaving the café unchanged, assignment, `{anh/chị}` tokens); the faction match's
soak; captures in the cloud (SwiftShader); **the Windows play checks are the user's**.

**Left to the builder:** file and type names, the map's character set, the switch plumbing, how many
lines a talk has, seed counts, the scene file's field names, text speed, portrait crop, and the look
of the pips, dialog box and game-over screen.

**Still open for later (not this slice):** Stage 2's party; Bảo's and Mai's longer café roles and
Mai's son; revive items' names and drops; Lệ Thanh's story pieces 2–12 (piece 1 is needed now); Tí's
second visit needing a replay; the songs; voiced lines; whether the player can change class.

---

## 10. Working on this repository

**Code style (project rule):** short, simple C++20; few abstractions; `// 1.` step comments only
where needed; a markdown file beside the code for anything deeper (math, rules); `umbra::<module>`
namespaces, `PascalCase`, headers included by bare name, one `Umbra::<Module>` library per module,
Catch2 tests (Last Curtain's in `plugins/last-curtain/tests/`). All numbers and screen text are data.
Rules stay headless. Player input, bots and monsters submit the same actions; only the rules commit.

**The plugin:** `plugins/last-curtain` (modules `match`, `matchbots`, `cafe`, `board`, `night`,
`nightbots`, `nightmind`; each has a `.md` beside its headers: start with `match/include/Match.md`,
`matchbots/include/Bots.md`, `cafe/include/Cafe.md`). Data: `assets/match/*.csv` and `rules.json`,
`assets/cafe/`. The app: `apps/LastCurtain` (`Main.cpp`, `MatchApp`, `MatchPlay`, `CafeApp`,
`CafePlay`, `CafeRoom`, `Screens`, `README.md`), with its own `assets/` (art, ui, words files,
`match-look.json`). The built-in model: `plugins/living-town/modules/mind/include/Mind.md`
(`UmbraTool town-mind` is the measuring tool to learn from).

**Commits:** `issue #UMBRA-<n> <summary>` (next free number: **#324**), ending with the
`Co-Authored-By` and `Claude-Session` lines the session gives. Stage files by name; never
`git commit -a`. **Never commit the user's uncommitted
`projects/reference/assets/scenes/settlement.scene.json`** or any `models/*.gguf`.

**Verifying in the cloud:** dependencies are built from source (`docs/testing/linux-headless-build.md`);
a full build and the 58 CTests take about 25 minutes (`cmake --build` then `ctest`). Single targets
build in minutes (e.g. `UmbraUiTests`). Pictures: SwiftShader with `xvfb-run`, run from the checkout,
e.g. `LastCurtain --cafe --seed 5 --frames 60 --capture out.png` (`--watch` forces the match).
`UmbraTool ui-preview <screen> --assets <folder> --out x.png` draws a screen headless. Never report a
Windows, GPU or GUI check as passed from Linux.

**Getting commits onto the user's PC** (`E:\Projects\EngineStudy`, through the desktop app's folder
bridge; its shell is a Linux VM with git, no build tools): for each commit, `git archive` its added or
changed files into `c<n>.tgz` with `c<n>.files` (the list) and `c<n>.msg` (the message); copy the three
into `E:\Projects\EngineStudy\Temp\lcship\` with **new file names each time** (overwriting a file of
the same name there didn't take once); run `bash Temp/lcship/apply.sh <n>` there (unpacks, adds
exactly those files, commits as the user); then compare `git rev-parse HEAD^{tree}` on both sides.
Use `git --no-optional-locks` for read-only git on the PC. Delete permission was granted for that
folder only for lock and temp files our own git leaves. Large `git am` mailboxes time out: use the
tarballs.

**The user:** native Vietnamese speaker, decides product questions, installs ComfyUI nodes and
downloads models themselves. Paid ComfyUI API nodes are not used without their OK. No blood, gore,
skulls or bodies; invented faces only; no real songs or logos. No subagents unless the user asks.
Credentials never go in chat.

## 11. Documents

- `docs/design/2026-11-23-campaign-slice-1.md`: the approved design with section numbers and the
  review history (also in the Project as `claude/last-curtain/campaign-slice-1-design-2026-11-23.md`).
- `docs/design/design-changes-2026-11-22.md`, `campaign-content-2026-11-22.md` (recruits, zodiacs,
  stages), `cast-2-2026-11-22.md` (Bảo, Mai, the player, Lệ Thanh, ghosts), `art-requests-2026-11-22.md`
  (the forms list, section 12).
- `plugins/last-curtain/apps/LastCurtain/assets/art/README.md`: what each picture is and its weak spots.
- `docs/testing/s42-tiles-groups-cafe.md` §4: the user's open Windows checks from plan 4.
- `docs/testing/s43-text-field.md`: the text field's record and its Windows check.
