# The Last Curtain: the campaign's first slice — design (2026-11-23)

**Status:** for the user's approval. "The user" marks the user's decisions; **[call]** marks Claude's
proposals, which the user can still change. Builds on `design-changes-2026-11-22.md` (the new design),
`campaign-content-2026-11-22.md` (recruits, zodiacs, stages), `cast-2-2026-11-22.md` (Bảo, Mai, the
player, Le Thanh, ghosts) and the art made on 2026-11-23 (`assets/art/README.md`).

## 1. Intent

Turn the new design into something playable, end to end, for the first time: a living guest's first
night, through the three tutorials and the first zodiac stage, to the moment Bé Tí remembers who he
was and joins. The slice is where the design's big bets get tested together: a tactical RPG with
biding and falls that stay down, a calm café where the dead get their memories back, Vietnamese words
throughout, and a built-in model that words the dead's memories without inventing them. Measure one
of each before committing to fifteen rooms (the design's top risk: content cost).

**Why** (from the design change doc): the café and the match didn't feed each other, the café was
timed busywork, and the game must carry the war's regret and heartbreak for a Vietnamese audience,
with characters who remember. This slice is the first proof that the new loop (fight, free, café,
remember, join) works.

## 2. Goals

1. **The flow:** T1 *Khách lạ* → T2 *Quán cà phê* → T3 *Hành lang chân dung* → Stage 1 *Phòng vé*
   (the Rat) → the café, where Le Thanh regains her first power (restoring memories) → two café visits
   that restore Bé Tí → Tí joins.
2. **One fight engine, switches set by data** (the user's choice, option A): campaign and faction
   match share the board, battles, abilities, items, monsters and bots; named rules turn on or off per
   mode and per stage.
3. **Biding** in both modes (the user): skip a whole turn for 1 action point (AP).
4. **Falls stay down** until revived; **game over** if the living player falls (retry restarts the
   stage, café, or quit); **win** by defeating the stage's zodiac (the user).
5. **Stage files:** hand-shaped room maps with authored enemies, items, switches and aims.
6. **The campaign save (v2):** progress, party, levels, who was freed and how, memories restored,
   Le Thanh's unlocks; and a save in the middle of a stage [call].
7. **The calm café** (the user): one walkable room, no timers, no failure; Le Thanh hosts and dances;
   a drink in 1–2 steps, then a talk with choices that count.
8. **Vietnamese from the start** (the user): every word on screen in Vietnamese; fonts that carry it;
   the model's Vietnamese tested first, with hand-written lines ready if it fails.
9. **The new art on screen:** the sprites, backdrops and café wall from 2026-11-23, and the forms the
   engine draws (tints, glow, strings, the gold knot, the ally blue with a name plate).

## 3. Non-goals (later slices)

Stages 2–12 and the final stage; the quick match's 12-against-12 pick screen (but biding reaches the
faction match now); boons, power cards, permanent boosts and auto-battle; heroes and boon givers in
the café; the café growing props after each area (a ticket rack may be the first, if cheap [call]);
zodiacs that adapt on retries through the learned policy; free-typed café answers; the storyteller
choosing visitors; cutscene stills (not painted yet: scenes use the backdrops and text cards);
new sounds and music (reuse); Le Thanh's alternate forms in play.

## 4. Anti-goals (a failure even if it works)

- The café becomes busywork again (any timer, patience bar or fail state).
- The model invents a fact about a real or fictional person, or a memory fragment that isn't
  authored. Facts are authored; the model only words them.
- English, or broken Vietnamese, leaks into a Vietnamese line on screen.
- The campaign's switches break the faction match (its soak and tests must still pass).
- A fight ends in a way the player can't read (a fall, a freeing, a game over without telling why).
- Any gore, bodies or skulls; a likeness of a real person.

## 5. Constraints

The repository's `CLAUDE.md`, and in particular:
- Short, simple C++20 in the existing style; step comments; a markdown file beside any non-obvious
  rule. Rules stay headless (no Vulkan, window or model needed to run a fight or a café visit).
- **All numbers and all screen text are data.** C++ string literals stay ASCII (MSVC reads source in
  the local code page); Vietnamese lives only in UTF-8 data files.
- Player input, bots and monsters submit the same actions; only the rules commit state.
- The built-in model proposes; the game validates; every model call has an authored fallback; tests
  use a stand-in model. Saves are versioned and written whole (temp file, then move).
- Windows/GPU checks are the user's; the cloud proves compile, tests and captures only.
- No subagents unless the user asks.

## 6. Approaches (decided)

| | How | Trade-off |
|---|---|---|
| **A. One fight engine, switches set by data** (**chosen, the user**) | The match module learns a few named rules, each on or off per mode and per stage. A new `campaign` module holds the story layer and builds each fight from a stage file. | The match code grows switches; tests must cover both modes. Biding and the Perform/respawn stage come for free in both modes. |
| B. A separate campaign fight engine (a copy of the board and fight code) | Clean campaign code. | Biding built twice; the Perform stage needs the old rules anyway; two sets of fights, fight screens and bots to keep. |
| (dropped) Campaign hooks around the unchanged faction match | Least change to the match code. | Fights the old rules (essence, score, respawn) every turn. |

## 7. The design

### 7.1 The rule switches (`match` module)

A `Rules` value inside `MatchState`, filled from data when a match or stage starts. The faction match
gets today's behaviour plus biding; a stage file sets its own.

| Switch | Faction match | Campaign default | Meaning |
|---|---|---|---|
| `essence` | on | off | Essence drops, carrying, banking; the stage race and score |
| `perform` | on | off | Perform (+ it takes a turn, agreed 2026-11-21, for quick match) |
| `respawn` | on (1 turn, at the hub) | off | Downed characters come back at their hub; hubs and guardians |
| `biding` | **on** | **on** | See 7.2 |
| `fallsStay` | off | on | 0 HP: the unit falls and stays on its square until revived |
| `playerLoss` | off | on | The living player falling ends the stage in a game over |
| `win` | score after 30 rounds | `boss` | `boss`: the stage is won when the unit marked `boss` is defeated; also lost if the whole side is down |
| `roundLimit` | 30 | 0 (none) | A stage may set a limit as its aim (a timed stage) |

One campaign stage turns `perform` and `respawn` back on as its gimmick (the user; which stage is for
the later slice; the content doc lists Perform under Stage 8, the Monkey).

### 7.2 Biding (both modes, the user)

- On its turn a character may **bide** instead of moving and acting: the whole turn passes, it gains
  **1 AP**, and it takes **+1 Defence until its next turn** [call].
- It stores **at most 2 AP** and spends **at most 2 in a turn**, so **at most 3 actions** in one turn;
  no borrowing (the user).
- Spending 1 AP buys one more action (an attack, an ability or an item) after the first, on the same
  turn; moves don't cost AP.
- Monsters can bide too (bosses mostly) [call]; stored AP shows as **pips** under a unit.
- New action: `Bide`. New ability effect: `give-ap` (Tí's *Đưa tin*), so the AP rules sit in the
  shared code from the start.
- The faction match's soak is re-run with biding on; balance targets stay as they are.

### 7.3 Falls, reviving and game over

- With `fallsStay`, a unit at 0 HP **falls**: it stays on its square (it blocks nothing and can't be
  hit [call]), is skipped in the turn order, and shows as fallen.
- **Revive:** a `revive` effect (an item on the map in Stage 1 per the content doc, and Bà Mai's
  board ability in T3 [call]) raises an adjacent fallen ally with part of its HP.
- **The player falls → game over:** the fight stops at once; a screen offers **Thử lại** (retry: the
  stage from its beginning), **Về quán** (back to the café) or **Thoát** (quit) (the user). The whole
  side down is the same.
- **Recruit specters** (marked `recruit`, drawn with the gold knot) are **freed, never killed**: at 0
  HP their strings are cut, a warm gold moment plays, and they leave the board. When the zodiac
  falls, every remaining specter's strings are cut, so **a recruit is always freed** [call] (the story
  needs Tí in every run). The freeing is logged with who cut the string, for the café to remember.

### 7.4 Stage files (`campaign` module, `assets/campaign/stages/*.json`)

One file per tutorial or stage, versioned, validated before use (a bad file names the file, the field
and the fix, and the stage doesn't start):
- `id`, the Vietnamese and English titles (the English only for logs and tools), the recommended
  level, the fight backdrop, the music.
- `map`: the hand-shaped room as rows of text (one character per square: wall, floor, door, window,
  hazard…), read into the existing `Board`, so the board's drawing, paths and light rule work
  unchanged.
- `party`: who fights (`player`, recruit ids, `bao`, `mai` as **temporary** members), start squares.
- `units`: enemies and ghosts with their monster rows, squares, sides, nature (friendly, neutral,
  aggressive), flags (`boss`, `recruit`), and a height scale [call: ordinary ghosts 0.8].
- **Sides and turns** [call]: the party is side 0 and acts by Speed as today; enemies are monsters on
  side 1, hostile to side 0 only; friendly and neutral ghosts are monsters on their own sides with
  their nature's behaviour. All monsters act in the monsters' phase after the party, as today.
- `items`: what lies on which square (the bandage revive in Stage 1).
- `rules`: the switches of 7.1.
- `gimmicks`: a small list of named gimmicks with numbers. Stage 1's is **ticket windows**: windows open
  and close each round and let out rat specters; items the Rat steals sit on a window until he is hit.
  It reuses the existing spawn code.
- `aims`: the hint lines (Le Thanh's voice), the teaching beats (what each tutorial teaches, shown
  when it first matters), and the recap after the fight.
- `scenes`: the scene before and after, by id (`cutscenes` data, text cards over backdrops).

**The cast sheet** (`assets/campaign/cast.csv`) links every campaign character to its stats row, its
sprite folders (`<id>` and `<id>_bound`), its form, its voice sheet and its memory fragments. This is
where the 2026-11-23 sprites are hooked in.

### 7.5 The slice's content

| | Party | Enemies | Teaches | After |
|---|---|---|---|---|
| **T1 Khách lạ** | the player alone, level 1 | rat specters (aggressive ghosts) | moving, attacking | Le Thanh's rescue (scene) [call: game over still applies, but T1 is tuned so falling is unlikely] |
| **T2 Quán cà phê** | — | — | café talk | meet Bảo and Bà Mai |
| **T3 Hành lang chân dung** | player + Bảo + Bà Mai (temporary), level 2 | specters, a mini-boss | biding, falls, reviving, game over | — |
| **1 Phòng vé** | player + Bảo + Bà Mai [call: they stay one more stage, as proposed], level 3 | the Rat (40 HP; *Móc túi* steals an item from an adjacent unit, *Chạy làng* darts 3 tiles after a battle), rat specters from the ticket windows, **Bé Tí as a red specter with the gold knot** | items, stealing, the gold knot, the first revive item | Le Thanh's story, piece 1 (money); she regains her first power: **restoring memories** |

- **Bảo's and Mai's kits** for T3 and Stage 1 are as written in `cast-2` (scalpel, diagnosis, the death
  paper; tend, mend, cold rice) [call: Mai's cold rice also revives in T3, where reviving is taught].
- **Stats** follow the class lines; ordinary ghosts at about 60% [call]; the Rat as written; first
  numbers, tuned by a campaign soak (7.10).
- **Levels:** the cap rises to **15** with new XP steps in data; the slice reaches level 4 at most.
  Characters sitting out still earn part of the XP [call]; retries give XP up to the stage's cap [call].

### 7.6 The campaign save (v2) and flow

- One file, versioned 2, written whole: the player's typed **name** and **male or female**; the
  chapter reached; each character's level, XP, stats, abilities and upgrades (today's career fields);
  the party; who was freed, in which stage, by whom and how (the fight facts); each recruit's
  fragments restored; Le Thanh's unlocks; café visits; talk answers that count.
- The faction match's v1 career is **not migrated** (no players yet) [call]; the faction match keeps
  working without a career, as quick match will.
- **A save in the middle of a stage** stores the `MatchState` beside the campaign file, so quitting and
  coming back resumes the fight [call]; a game-over retry always starts the stage over (the user).
- **The flow:** title → new game (name, male or female) → T1 → T2 → T3 → Stage 1 → café → (a fight) →
  café. Between stages the player picks from a short list: the next stage, a cleared stage to replay
  for XP, or the café.

### 7.7 The café (calm, the user)

- **Removed:** the 150 s shift, patience timers, tips, wrong-drink failure, takings, money.
- **The room:** the walkable 3D room as built, with the painted back wall (done, #312), Le Thanh on a
  low platform in front of its painted stage, dancing [call: a looping bob and turn of her hostess
  sprite until a dance animation exists], and the guests at tables.
- **Guests:** spirits you met in fights, plus Bảo and Bà Mai. Zodiacs never appear (the user).
- **A visit:** each guest asks for a drink made in 1–2 steps at the stations; serving it opens a talk.
  A talk is a few lines and a few choices; serving the right drink counts as a good answer [call].
  No talk can be failed.
- **Restoring Tí** (once Le Thanh has the power): Tí has four authored fragments (his mother's name,
  the Tết promise, the message, the white sand). **Each visit restores two** [call], so two visits
  restore him and he joins, glowing blue, cloth gone. **A new visit opens after any fight** (the next
  stage, or a replay) [call], so the slice can be finished without Stage 2.
- **Memory:** Tí's lines may mention how he was freed (who cut his string, what happened), from the
  saved fight facts.

### 7.8 Words, Vietnamese and the model

- **The model's Vietnamese test comes first** (the user's decision to start in Vietnamese makes it
  gating). A tool command runs the built-in model (Qwen3 4B, on the user's PC) over a fixed test set:
  Tí, Bảo, Bà Mai and Le Thanh, each in a handful of café situations, from their voice sheets and
  fragments. It checks every line automatically (Vietnamese diacritics present, no English words, the
  never-say list, length, the speaker's dialect markers) and writes a report for the user to read.
  Pass: the user is satisfied with the lines and the automatic checks pass for most of them [call:
  90%].
- **If it fails** (the design's plan): every line is hand-written in Vietnamese, and the model only
  chooses among authored lines (or is left out).
- **Authored text:** Claude drafts the Vietnamese (lines, fragments, aims, menus); the user reviews it
  as a native speaker; the line guard gains Vietnamese checks.
- **Screen text:** the game's own words files get a Vietnamese set, chosen by an option `language`,
  default `vi`; the English set stays for development and the later quick match [call].
- **Fonts:** Poppins (body) and Abril Fatface (titles) lack letters such as ư, ả, ắ and ợ; Lora has
  them all. Replace the two with OFL fonts that carry Vietnamese [call: Be Vietnam Pro for body text,
  Playfair Display for titles], pinned and listed in the third-party notices. The engine already packs
  the Vietnamese letter range (U+1EA0–1EF9).

### 7.9 On screen

- **Forms the engine draws** (art list section 12): player solid; allies blue glow + name plate;
  recruit specters red tint, strings, gold knot; café ghosts pale glow; freeing and heroes warm gold
  rim; ghosts green / blue-grey / red by nature; zodiacs strong red glow, thick strings; Le Thanh a
  faint warm glow. Tints, glows and opacities are data (`match-look.json`).
- **Strings:** glowing lines from each sprite's `anchors.json` up out of view, swaying a little. The
  anchors are estimated from outlines; a fix-up tool or hand edits where they look wrong.
- **Fight screens:** the stage's backdrop (`backdrops/foyer_today`, `portrait_corridor`, `box_office`).
- **Game over screen, stage list, title with name and gender**, in the existing data-driven UI
  (`ui/*.ui.json`).

### 7.10 Checks

- Headless tests for every switch in both modes, biding limits, falls and revive, game over,
  boss win, freeing, stage file validation (good and broken files), save v2 round trip, a corrupt or
  old save, and the café visit and restoring flow.
- **The faction match's soak** with biding on; **a campaign soak** that plays each slice stage with
  bots on many seeds and reports wins, falls and turns, to tune the first numbers.
- Captures in the cloud (SwiftShader) of each stage and the café; **the Windows play check is the
  user's**, and the plan 4 Windows check still comes before relying on any of this.

## 8. Order of work [call]

1. **The model's Vietnamese test** tool and test set, so the user can run it on the PC while the rest
   is built; fonts.
2. **Rules:** the switches, biding (both modes), falls, revive, game over, boss win, freeing; the
   faction match's tests and soak still pass.
3. **Stages:** the stage file and cast sheet, the stage builder, T1, T3 and Stage 1 data, the Rat's
   kit and the ticket windows, the campaign soak.
4. **Flow and save:** save v2, the stage list, the game over screen, retries, the mid-stage save,
   name and gender.
5. **The café:** the calm room, visits, talks, restoring Tí, Le Thanh's unlock, fight memory.
6. **On screen:** the forms, strings, gold knot, name plates, backdrops, Vietnamese words throughout.

Each becomes its own plan, built and checked before the next.

## 9. Left to the builder

File and type names, the stage map's character set, the exact switch plumbing, the drink steps,
how many lines a talk has, the soak's seed counts, and the look of the pips and the game-over screen,
within the rules above.

## 10. Still open (outside this slice, or for later)

Stage 2's party (who fights with you and Tí); Bảo and Mai's longer café roles and Mai's son; revive
items' names and drops now money is gone; Le Thanh's twelve story pieces' words (piece 1 is needed
now); the songs; whether the player can change class.
