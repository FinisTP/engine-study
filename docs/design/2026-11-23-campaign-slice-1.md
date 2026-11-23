# The Last Curtain: the campaign's first slice — design (2026-11-23)

**Status:** for the user's approval. Revised 2026-11-23 with the user's answers to two reviews and the
evaluation (section 11), and the user's confirmation of the dialog scenes (7.11). "The user" marks the
user's decisions; **[call]** marks Claude's proposals, which the user can still change. Builds on
`design-changes-2026-11-22.md` (the new design), `campaign-content-2026-11-22.md` (recruits, zodiacs,
stages), `cast-2-2026-11-22.md` (Bảo, Mai, the player, Le Thanh, ghosts) and the art made on
2026-11-23 (`assets/art/README.md`).

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
3. **One turn order by Speed for every unit, monsters included, no exception** (the user).
4. **Biding** in the campaign (the user): skip a whole turn for 1 action point (AP). Biding in the
   faction match is left for later (the user, 2026-11-23): the switch exists, off there for now.
5. **Falls stay down** until revived; **game over** if the living player falls (retry restarts the
   stage, café, or quit); **win** by defeating the stage's zodiac (the user).
6. **Stage files:** hand-shaped room maps with authored enemies, items, switches and aims.
7. **The campaign save (v2):** progress, party, levels, who was freed and how, memories restored,
   Le Thanh's unlocks. **No save in the middle of a stage**: a stage is only ever restarted from its
   beginning (the user). A developer-only mid-stage save exists for debugging and balancing.
8. **The calm café** (the user): one walkable room, no timers, no failure; Le Thanh hosts and walks
   among the guests; a drink in 1–2 steps, then a talk with choices that count.
9. **Vietnamese from the start** (the user): every word on screen in Vietnamese; fonts that carry it;
   the model's Vietnamese tested first, with hand-written lines ready if it fails.
10. **Dialog scenes** (the user, 2026-11-23): one conversation system for the café, the start and end
    of a battle and moments inside it: up to two characters on each side, a name tag, a dialog box.
11. **The new art on screen:** the sprites, backdrops and café wall from 2026-11-23, and the forms the
    engine draws (tints, glow, strings, the gold knot, the ally blue with a name plate).

## 3. Non-goals (later slices)

Stages 2–12 and the final stage; the quick match's 12-against-12 pick screen (but the one Speed order
reaches the faction match now; biding there is for later, the user); boons, power cards, permanent
boosts and auto-battle; heroes and boon givers in the café; the café growing props after each area (a
ticket rack may be the first, if cheap [call]); zodiacs that adapt on retries through the learned
policy; free-typed café answers; the storyteller choosing visitors; cutscene stills (not painted yet:
scenes are dialog scenes over the room, the board or a backdrop); new sounds and music (reuse); Le
Thanh's alternate forms in play.

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
| **A. One fight engine, switches set by data** (**chosen, the user**) | The match module learns a few named rules, each on or off per mode and per stage. A new `campaign` module holds the story layer and builds each fight from a stage file. | The match code grows switches; tests must cover both modes. The Speed order, biding and the Perform/respawn stage live in one place for both modes. |
| B. A separate campaign fight engine (a copy of the board and fight code) | Clean campaign code. | Biding built twice; the Perform stage needs the old rules anyway; two sets of fights, fight screens and bots to keep. |
| (dropped) Campaign hooks around the unchanged faction match | Least change to the match code. | Fights the old rules (essence, score, respawn) every turn. |

## 7. The design

### 7.1 The rule switches (`match` module)

A `Rules` value inside `MatchState`, filled from data when a match or stage starts. The faction match
gets today's behaviour plus the shared Speed order; a stage file sets its own.

| Switch | Faction match | Campaign default | Meaning |
|---|---|---|---|
| `essence` | on | off | Essence drops, carrying, banking; the stage race and score |
| `perform` | on | off | Perform (+ it takes a turn, agreed 2026-11-21, for quick match) |
| `respawn` | on (1 turn, at the hub) | off | Downed characters come back at their hub; hubs and guardians |
| `biding` | off (for later, the user) | **on** | See 7.2 |
| `fallsStay` | off | on | 0 HP: the unit falls and stays on its square until revived |
| `playerLoss` | off | on | The living player falling ends the stage in a game over |
| `win` | score after 30 rounds | `boss` | `boss`: the stage is won when the unit marked `boss` is defeated; also lost if the whole side is down |
| `roundLimit` | 30 | 0 (none) | A stage may set a limit as its aim (a timed stage) |

One campaign stage turns `perform` and `respawn` back on as its gimmick (the user; which stage is for
the later slice; the content doc lists Perform under Stage 8, the Monkey).

**Turn order (the user, 2026-11-23): every unit acts in one order by Speed, monsters included, in
both modes, with no exception.** The separate monsters' phase is removed. A unit that appears during
a fight (a rat specter from a ticket window) joins the order at its Speed, taking its first turn in
the next round (the user). **A revived unit follows the same rule**: it acts from the next round (the
user). Ties keep today's seeded key. The faction match's soak is re-run with the Speed order; its
balance targets stay as they are.

### 7.2 Biding (campaign, the user)

- On its turn a unit may **bide** instead of moving and acting: the whole turn passes and it gains
  **1 AP**. **No Defence bonus** (the user): biding is a high-risk, high-reward choice.
- It stores **at most 2 AP** and spends **at most 2 in a turn**, so **at most 3 actions** in one turn;
  no borrowing (the user).
- **Each extra action comes with its own move** (the user): spending 1 AP buys one more *move and
  action* after the first, on the same turn.
- **Some abilities cost AP** (the user): an ability's cost has an HP part and an **AP part**
  (`abilities.csv` gains an `ap` column). **The ability's AP is paid first, from the same pool and
  within the same limit of 2 a turn; only the AP left over can buy extra actions** (the user). With 2
  AP stored, a turn can be:
  - the normal move and action, plus 2 extra moves and actions (2 AP);
  - a 1-AP ability as the normal action, plus 1 extra move and action (2 AP);
  - a 2-AP ability as the normal action, and nothing more.
  With 0 AP, the unit takes a normal turn and AP abilities are greyed out with the reason.
- **AP is lost when a unit falls**, and every unit starts each stage at 0 AP (the user).
- Monsters can bide too; stored AP shows as **pips** under a unit. **Bosses' biding is the difficulty
  ramp** (the user): early bosses play more passively and don't store AP (the Rat never bides); later
  bosses bide for turns of up to 3 actions, pushing the player to build a tank or a ranged unit that
  wears the boss down first.
- **The faction match** keeps biding off for now (the user): how it sits with Perform and respawn is
  decided when the faction match comes back into focus.
- New action: `Bide`. New ability effect: `give-ap` (Tí's *Đưa tin*), so the AP rules sit in the
  shared code from the start.

### 7.3 Falls, reviving and game over

- With `fallsStay`, a unit at 0 HP **falls**: it stays on its square, **can't be hit** (the user), is
  skipped in the turn order, and shows as fallen.
- **Fallen units don't block** (the user): others walk through and stand on their square, and a fallen
  unit doesn't count toward the tile's limit of 2.
- **Revive:** a `revive` effect (a spell or an item; in Stage 1 a revive item on the map, and Bà Mai's
  cold rice [call]) raises a fallen unit on its side with part of its HP. **If its square already holds
  2 units, it stands up on the nearest square it can stand on** (the user); ties go to the square
  nearest the reviver, then a fixed order, so tests are repeatable [call]. **Enemies revive too** (the
  user): a monster with a revive ability can raise its own fallen.
- **Removed for good** (the user): certain abilities (a `remove` effect) take a fallen unit off the
  stage; it can't be revived in this stage, but **returns in the next stage**. Players' allies too.
- **The player falls → game over:** the fight stops at once; a screen offers **Thử lại** (retry: the
  stage from its beginning), **Về quán** (back to the café) or **Thoát** (quit) (the user). **Before the
  café is unlocked (T1), the only choice is Thử lại** (the user). The whole side down is the same
  (it can't happen without the player falling).
- **Le Thanh's rite of resurrection** (the user, 2026-11-23): an early power that **revives every unit
  on the player's side to full HP, once in the whole game**, so a late game over doesn't have to cost
  the whole stage. [call: it is unlocked after T3, where falls are taught; while unused it appears as a
  choice on the game-over screen (*Lễ cầu hồn*), and the fight goes on where it stopped; its use is
  written to the save at once, so losing the stage afterwards doesn't give it back.]
- **Strings and the gold knot** (the user, 2026-11-23). Red strings mark a zodiac's control; the gold
  knot marks a person to free (recruits and boon givers).
  1. **Freeing:** a gold-knot specter **falls at 0 HP** like anyone; it is freed when its strings are
     **cut** by the player standing next to it and spending an action. Until then its strings hang
     slack, and **its side can revive it** strung (a race against the zodiac's revives).
  2. **Only the living player can cut** (with Le Thanh's fan), so freeing always puts the player, whose
     fall is game over, at risk.
  3. **Never cut, or removed for good** by a removal ability: it leaves this stage and **comes back
     strung in a later stage** (the next stage fought, a replay included [call]); one never freed by
     the end **fights for Le Thanh in the finale**.
  4. **Freeing isn't needed to win:** the stage is won when the zodiac falls, but a specter still strung
     then stays Le Thanh's (rule 3). **No exception, Stage 1 included** (the user, 2026-11-23): Tí acts
     like every other character. Le Thanh says at the start of the stage that he must be cut free; a
     player who misses it replays the stage to free him (rule 3).
  5. **Ordinary red ghosts:** strings don't matter in this slice; they are simply defeated. Later,
     cutting a fallen one may send it to the café and shrink the finale's audience.
  6. **Zodiacs drawing power from their strings:** not in this slice; kept for later bosses (the user).
  7. **On screen:** the knot sits on the strings above the head; a fallen strung specter keeps slack
     strings until cut; cutting plays a snap and the warm gold light.
  The freeing is logged with when, where and what happened, for the café to remember (only the player
  cuts, so "who" is always the player).
- **Le Thanh's hint** (the user, 2026-11-23, simplified): she gives hints **only at the start of a
  stage**, in the opening scene and the hint line (in Stage 1: Tí's gold knot must be cut before the
  Rat falls). There are no warnings during the fight (an earlier list of in-fight warnings was dropped
  to keep the plan simple). The hint is data (`aims` in the stage file). The recap after the fight
  names anyone left strung.

### 7.4 Stage files (`campaign` module, `assets/campaign/stages/*.json`)

One file per tutorial or stage, versioned, validated before use (a bad file names the file, the field
and the fix, and the stage doesn't start):
- `id`, the Vietnamese and English titles (the English only for logs and tools), the recommended
  level, the fight backdrop, the music.
- `map`: the hand-shaped room as rows of text (one character per square: wall, floor, door, window,
  hazard…), read into the existing `Board`, so the board's drawing, paths and light rule work
  unchanged. The mirror fairness check is off for campaign maps [call].
- `party`: who fights (`player`, ally ids such as `bao`, `mai`, recruit ids), start squares.
- `units`: enemies and ghosts with their monster rows, squares, sides, nature (friendly, neutral,
  aggressive), flags (`boss`, `recruit`), and a height scale [call: ordinary ghosts 0.8].
- **Sides and turns:** the party is side 0; enemies are on side 1, hostile to side 0 only; friendly and
  neutral ghosts are on their own sides with their nature's behaviour. **Everyone acts in the one Speed
  order** (the user, 7.1).
- `items`: what lies on which square (the bandage revive in Stage 1). **Items picked up are kept
  between stages** in the party's bag (the user, 2026-11-23), so exploring a room pays off; a used item
  is gone. **Each map item can be taken once** (the user): a replay offers only the items not yet
  taken; one already taken doesn't come back. Items picked up in a lost attempt are lost with it (7.6).
- `rules`: the switches of 7.1.
- `gimmicks`: a small list of named gimmicks with numbers. Stage 1's is **ticket windows**: windows open
  and close each round and let out rat specters; items the Rat steals sit on a window until he is hit,
  then drop on his square for anyone to pick up [call]. It reuses the existing spawn code.
- `aims`: the stage-start hints (Le Thanh's voice), the teaching beats (what each tutorial
  teaches, shown when it first matters), and the recap after the fight.
- `scenes`: the dialog scenes (7.11) at the start and end, and the ones a moment in the fight sets
  off (a unit falls, a round begins, the boss drops below some HP, a freeing).

**The cast sheet** (`assets/campaign/cast.csv`) links every campaign character to its stats row, its
sprite folders (`<id>` and `<id>_bound`), its form, its voice sheet and its memory fragments. This is
where the 2026-11-23 sprites are hooked in.

### 7.5 The slice's content

| | Party | Enemies | Teaches | After |
|---|---|---|---|---|
| **T1 Khách lạ** | the player alone, level 1 | rat specters (aggressive ghosts) | moving, attacking | Le Thanh's rescue (scene) [call: game over still applies, but T1 is tuned so falling is unlikely] |
| **T2 Quán cà phê** | — | — | café talk | meet Bảo and Bà Mai |
| **T3 Hành lang chân dung** | player + Bảo + Bà Mai (they join for good at T2, the user), level 2 | specters, a mini-boss | biding, falls, reviving, game over | — |
| **1 Phòng vé** | player + **Bảo + Bà Mai**, permanent allies from the second tutorial on (the user), level 3 | the Rat (40 HP; *Móc túi* steals an item from an adjacent unit, *Chạy làng* darts 3 tiles after a battle), rat specters from the ticket windows, **Bé Tí as a red specter with the gold knot** | items, stealing, the gold knot, the first revive item | Le Thanh's story, piece 1 (money); she regains her first power: **restoring memories** |

- **Bảo and Bà Mai are permanent allies** from T2 on (the user, 2026-11-23, replacing "café characters
  who join only in T3"), so the allies are the ten recruits plus these two. **Bé Tí is freed in Stage
  1** (the user) and joins once his memories are restored.
- **Bảo's and Mai's kits** for T3 and Stage 1 are as written in `cast-2` (scalpel, diagnosis, the death
  paper; tend, mend, cold rice) [call: Mai's cold rice also revives, in T3 where reviving is taught
  and in Stage 1, so a stolen bandage never leaves the party without a revive].
- **Stats** follow the class lines; ordinary ghosts at about 60% [call]; the Rat as written; first
  numbers, tuned by a campaign soak (7.10).
- **Levels:** the cap rises to **15** with new XP steps in data; the slice reaches level 4 at most.
  Characters sitting out still earn part of the XP [call] (no one sits out in this slice); **replays of a
  cleared stage** give XP up to its cap (the user).

### 7.6 The campaign save (v2) and flow

- One file, versioned 2, written whole: the player's typed **name** and **male or female**; the
  chapter reached; each character's level, XP, stats, abilities and upgrades (today's career fields);
  the party and its bag of items; which map items have been taken; whether the rite has been used; who
  was freed, in which stage, by whom and how (the fight facts); each recruit's fragments restored; Le
  Thanh's unlocks; café visits; talk answers that count.
- The faction match's v1 career is **not migrated** (no players yet) [call]; the faction match keeps
  working without a career, as quick match will.
- **No save in the middle of a stage** (the user): quitting a stage loses it, and every retry starts the
  stage over. The campaign file is written only between stages and in the café. A **developer
  mid-stage save** (a tool command or debug option, off in a shipped game) stores the `MatchState` for
  debugging and balancing only (the user).
- **What a lost stage keeps** [call]: nothing. A game over or a quit returns to the save from before
  the stage, so XP and items from the failed attempt are gone (the user's "only full reset").
- **The flow:** title → new game (name, male or female) → T1 → T2 → T3 → Stage 1 → café → (a fight) →
  café. Between stages the player picks from a short list: the next stage, a cleared stage to replay
  for XP, or the café.

### 7.7 The café (calm, the user)

- **Removed:** the 150 s shift, patience timers, tips, wrong-drink failure, takings, money.
- **The room:** the walkable 3D room as built, with the painted back wall (done, #312) and the guests
  at tables. **Le Thanh doesn't dance in play** (the user): she walks around, appears at random places
  in the room, and starts conversations like any other guest. **Her dancing lives in the words only**
  (the user): a line of description when you arrive or when she moves ("Lệ Thanh xoay một vòng giữa
  quán, tà áo đỏ lướt qua những chiếc ghế mây…"). The provisional `lethanh/dance` painting isn't used
  in play [call].
- **Guests:** spirits you met in fights, plus Bảo and Bà Mai. Zodiacs never appear (the user).
- **A visit:** each guest asks for a drink made in 1–2 steps at the stations; serving it opens a talk.
  A talk is a dialog scene (7.11) of a few lines and a few choices; serving the right drink counts as
  a good answer [call]. No talk can be failed.
- **Restoring Tí** (once Le Thanh has the power): Tí has four authored fragments (his mother's name,
  the Tết promise, the message, the white sand). **Each visit restores two** [call], so two visits
  restore him and he joins, glowing blue, cloth gone. **A new visit opens after any fight** (the next
  stage, or a replay) [call]. In the slice this means a replay before Tí's second visit; how Tí's case
  is handled is left for later (the user, section 10).
- **Memory:** Tí's lines may mention how he was freed (who cut his string, what happened), from the
  saved fight facts.

### 7.8 Words, Vietnamese and the model

- **The model's Vietnamese test comes first** (the user's decision to start in Vietnamese makes it
  gating). A tool command runs a model on the user's PC over a fixed test set: Tí, Bảo, Bà Mai and Le
  Thanh, each in a handful of café situations, from their voice sheets and fragments. It checks every
  line automatically (Vietnamese diacritics present, no English words, the never-say list, length, the
  speaker's dialect markers) and writes a report for the user to read. Pass: the user is satisfied with
  the lines and the automatic checks pass for most of them [call: 90%].
- **Arcee-VyLinh 3B is tested first** (the user, 2026-11-23; Q4_K_M, 1.9 GB, from
  `QuantFactory/Arcee-VyLinh-GGUF`). Its card says Apache 2.0, but it is merged with
  Qwen2.5-3B-Instruct, whose licence (qwen-research) forbids commercial use without Alibaba's
  permission: fine to test, settle before shipping.
- **Model comparison (same tool, after VyLinh):** several model files, one report each, side by side:
  Qwen3 4B (the current default) and other candidates the user downloads. **Sailor-4B-Chat is not a
  safe fallback after all**: its card says Apache 2.0 but asks users to comply with the Qwen licence,
  and its base, Qwen1.5-4B, carries the `tongyi-qianwen-research` licence, so it has the same problem
  as VyLinh. Gemma-SEA-LION v4 4B and SEA-LION v4.5 E2B are under Google's Gemma terms, to be read
  before shipping. A candidate must load in the pinned llama.cpp (the tool says so if it doesn't), fit
  the card with the game (about 3 GB, measured by the tool), answer in under about 1.5 s a line, and
  carry a licence that lets it ship inside the game; a newer llama.cpp is a separate, reviewed change.
  The user picks the winner from the reports.
- **If it fails** (the design's plan): every line is hand-written in Vietnamese, and the model only
  chooses among authored lines (or is left out).
- **Voice test (alongside, outside the game):** ComfyUI's installed voice nodes are all paid cloud
  services, so they are not used. The local test uses VieNeu-TTS v3 Turbo (Apache 2.0, runs on the
  CPU, preset Northern, Central and Southern voices) on the same test lines, one script writing one
  sound file per line, for the user to listen to [call: a Central voice for Tí and Bà Mai, a Southern
  one for Bảo, the user picks Le Thanh's]. No voice cloning of real people. **First samples**
  (2026-11-23, the user's VieNeu on port 7860): six lines (Tí as Quang Sơn and as Ngọc Trân, Bà Mai as
  Ngọc Trân, Bảo as Thái Sơn, Le Thanh as Ngọc Linh and as Thục Đoan) took 10–15 s each for 5–7 s of
  sound, fine for lines made ahead of time. **Tí's child voice** (the user): VieNeu has no child voice,
  so the script raises the pitch of a preset voice (a pitch shift with formant correction, so it
  sounds younger rather than sped up [call: try +3 to +5 semitones; the user judges]). If voice is
  kept, only **authored** lines are voiced, ahead of time, as Ogg files in the assets; model lines stay
  text, so the game ships no second model [call]. Voicing itself is a later slice.
- **Authored text:** Claude drafts the Vietnamese (lines, fragments, aims, menus); the user reviews it
  as a native speaker; the line guard gains Vietnamese checks.
- **Screen text:** the game's own words files get a Vietnamese set, chosen by an option `language`,
  default `vi`; the English set stays for development and the later quick match [call]. Tests check
  ids and structure, not wording, so they pass in either language [call].
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
- **The order strip** now shows monsters too, in the one Speed order, with AP pips on each portrait
  [call].
- **Game over screen, stage list, title with name and gender**, in the existing data-driven UI
  (`ui/*.ui.json`).

### 7.10 Checks

- Headless tests for every switch in both modes, the one Speed order (monsters included, units joining
  or revived mid-fight act next round), biding limits, AP lost on a fall and reset per stage, AP-cost
  abilities, the rite of resurrection (once per game, kept used after a loss), items kept in the bag
  and each map item taken once, T1's retry-only game over, falls that don't block, revive onto the
  nearest free square, game over, boss win, freeing, the stage-start hint, stage file validation (good
  and broken files), dialog scenes (slots, enter and leave, skip stopping at a choice, choices setting
  facts, a moment's scene firing once and the fight resuming, broken scene files), save v2 round trip,
  a corrupt or old save, no mid-stage save outside the developer option, and the café visit and
  restoring flow.
- **The faction match's soak** with the one Speed order (biding off); **a campaign soak** that plays each
  slice stage with bots on many seeds and reports wins, falls, turns, and **an estimate of play time**
  (turns × beat lengths), to tune the first numbers. A safety cap on rounds in the soak only, so two
  biding bots can't stall it [call].
- Captures in the cloud (SwiftShader) of each stage and the café; **the Windows play check is the
  user's**, and the plan 4 Windows check still comes before relying on any of this.

### 7.11 Dialog scenes (the user, 2026-11-23; confirmed as written)

A generic RPG conversation: portraits on the left and right, a name tag, a dialog box. The same player
runs in the café, before and after a fight, and inside one. The user confirmed this section as
written (2026-11-23), so its points are the user's decisions.

```
 ┌───────────────────────────────────────────────────────────────┐
 │   (the café room, the board, or a backdrop, dimmed behind)     │
 │  ┌──────┐┌──────┐                          ┌──────┐┌──────┐    │
 │  │ Tí   ││ Mai  │                          │ Rat  ││      │    │
 │  │(lit) ││(dim) │                          │(dim) ││      │    │
 │  └──────┘└──────┘                          └──────┘└──────┘    │
 │ ┌─[ Bé Tí ]─────────────────────────────────────────────────┐ │
 │ │ Chị ơi, con nhớ ra rồi! Mạ con hứa Tết ni…                  ▼│ │
 │ └────────────────────────────────────────────────────────────┘ │
 │                                          [Nhật ký] [Bỏ qua]   │
 └───────────────────────────────────────────────────────────────┘
```

- **Places:** the café, the start of a battle, inside a battle, the end of a battle. Behind the
  portraits: whatever was on screen, dimmed (the café room or the board), or a backdrop picture the
  scene names.
- **Slots:** **0–2 characters on the left, 0–2 on the right.** A line can bring a character in, take
  one out or swap one. Portraits face the middle (the right side mirrored).
- **Who speaks:** the **name tag** on the dialog box, on the speaker's side; the speaker's portrait is
  lit, the others dimmed. A line without a speaker is narration (no tag). Le Thanh's dancing in the
  café is told in narration lines (7.7).
- **Portraits:** made from each character's sprite (the body and a face: calm, talk, happy, angry,
  afraid, scheming, near-dead), so a line can set the face; drawn in the character's form (a strung
  specter red with strings, an ally's blue, a ghost's pale glow). The player's portrait and name come
  from the save (male or female, the typed name, `{player}` in lines).
- **The box:** the text appears letter by letter; a click (or Space) shows the whole line, the next
  click goes on. **Bỏ qua** (skip) jumps to the scene's end but **stops at a choice**; **Nhật ký** (log)
  shows the lines so far. Auto-advance is later.
- **Choices:** a line may end in 2–4 choices; a choice sets a fact (saved, as café answers are) and may
  jump to another part of the scene. Café talks use this; most battle scenes don't.
- **Inside a fight:** a scene set off by a moment waits until the current action has finished, then
  pauses the fight; the fight goes on where it was. Each moment's scene plays once per attempt.
- **Words:** scene lines are **authored** (Vietnamese, the user reviewing). In café talks a line may be
  worded by the model from authored facts, with the authored line as its fallback; the box doesn't care
  which.
- **Voice:** a line may name a sound file; it plays if present (voicing is a later slice).
- **Data:** one file per scene, `assets/campaign/scenes/<id>.json`, versioned and validated (an unknown
  character, a third portrait on a side or a jump to a missing line names the file, the line and the
  fix). The scene player is headless (which line, who is where, the choices, the facts set); drawing is
  a data-driven UI screen (`ui/dialog.ui.json`) plus the portraits. The night game's camera-shot
  cutscenes stay as they are.

## 8. Order of work [call]

1. **The model's Vietnamese test** tool and test set, run first with **Arcee-VyLinh** (the user), then
   the side-by-side comparison; the voice test script with the pitch shift for Tí; fonts.
2. **Rules:** the switches, the one Speed order, biding (campaign), falls, revive, game over, boss
   win, freeing, the rite of resurrection; the faction match's tests and soak still pass.
3. **Dialog scenes:** the scene file, the headless player, the dialog screen and portraits, so the
   stages and the café can use them.
4. **Stages:** the stage file and cast sheet, the stage builder, T1, T3 and Stage 1 data and their
   scenes, the Rat's kit and the ticket windows, the campaign soak.
5. **Flow and save:** save v2, the stage list, the game over screen, retries, the developer mid-stage
   save, name and gender.
6. **The café:** the calm room, visits, talks, restoring Tí, Le Thanh's unlock, fight memory.
7. **On screen:** the forms, strings, gold knot, name plates, backdrops, pips, Vietnamese words
   throughout.

Each becomes its own plan, built and checked before the next.

## 9. Left to the builder

File and type names, the stage map's character set, the exact switch plumbing, the drink steps,
how many lines a talk has, the soak's seed counts, the scene file's field names, the text speed and
portrait crop, and the look of the pips, the dialog box and the game-over screen, within the rules
above.

## 10. Still open (outside this slice, or for later)

Stage 2's party (who of the player, Bảo, Bà Mai and Tí fights); Bảo and Mai's longer café roles and
Mai's son; revive items' names and drops now money is gone; Le Thanh's twelve story pieces' words
(piece 1 is needed now); **Tí's second café visit in the slice, which today needs a replay** (the
user: handle later, without making the tutorial rules more complex); the songs; voiced lines (after
the voice test); whether the player can change class.

## 11. Review log

**2026-11-23, the user's answers to the first review:**
1. Every unit, monsters included, uses Speed in one turn order, no exception (7.1).
2. Each extra action includes a move; an ability's AP is paid first from the same pool (7.2).
3. Arcee-VyLinh is tested first (7.8).
4. Le Thanh warns before important actions such as the final blow (7.3). *(Replaced by the
   evaluation's answer 5.)*
5. Fallen units don't block; a revived unit stands on the nearest free square if its own is full (7.3).
6. No mid-stage save; only a full restart; a developer-only save for debugging and balancing (7.6).
7. Le Thanh dances in the words only (7.7). Tí's voice comes from raising a preset's pitch (7.8). The
   duplicate copy of this file was removed.

**2026-11-23, the user's answers to the second review:**
1. Stage 1: the Rat can't fall while Tí is strung; he flees at 1 HP (7.3). *(Replaced by the
   evaluation's answer 5.)*
2. The warning about where enemies can reach is dropped from the slice (7.3).
3. Items are kept between stages, to reward exploring (7.4, 7.6).
4. Revived units act from the next round; AP is lost on a fall and reset each stage (7.1, 7.2).
5. Before the café is unlocked, a game over offers only Thử lại (7.3).
6. Replays of a cleared stage, not lost retries, give XP up to the cap (7.5).
7. Biding in the faction match is left for later (7.1, 7.2).

**2026-11-23, the user's answers to the evaluation (`slice-1-eval-2026-11-23.md`):**
1. Tí's second visit needing a replay is left for later; the tutorial rules stay simple (7.7, 10).
2. Each map item can be taken once; replays offer only the items not yet taken (7.4).
3. Le Thanh's rite of resurrection: revives the whole side to full HP, once in the game (7.3).
4. Bosses' biding is the difficulty ramp; early bosses are passive and don't store AP (7.2).
5. Tí acts like every other character. Le Thanh hints only at the start of a stage; a player who
   misses it replays the stage. This replaces the second review's "the Rat waits for Tí" and the
   in-fight warnings (7.3).

**2026-11-23, the user's confirmation:** section 7.11 (dialog scenes) matches the user's intention
exactly; its proposals became the user's decisions.

## 12. Holes found on 2026-11-23 (for the user)

1. **Forms of address:** Vietnamese lines change with the player's gender ("Chị ơi" / "Anh ơi"), so
   lines need gendered words, not only `{player}` [call: tokens such as `{anh/chị}` filled from the
   save].
2. **Typing the name:** the engine's UI has no text-entry box; the name screen needs one that takes
   Vietnamese typing (Telex/Unikey through SDL's text input) [call: a small text-field widget, with a
   default name if left empty].
3. **Items in a fight:** today each unit carries 4 item slots (`BagFull`), but the design keeps items
   in the party's bag between stages. Who carries what into a fight, and how big is the bag?
4. **Stage 1's revive race:** rule 1 says Tí's side can revive him, but nothing on the Rat's side has a
   revive, so once Tí falls, cutting him free is never contested. Intended, or does someone get one?
5. **A café visit after a lost fight:** if a lost fight opens a visit ("any fight"), Tí's second visit
   no longer needs a won replay. Does a loss count?
6. **The café's guests and T2:** besides Bảo and Bà Mai, who sits in the café in the slice, and what
   does T2 teach before Le Thanh can restore memories?
7. **Teaching beats vs start-only hints:** the tutorials' teaching beats appear "when they first
   matter" (during a fight), but Le Thanh hints only at the start of a stage. Are the tutorials the
   exception (beats shown as dialog scenes mid-fight)?
8. **Tí's level when he joins** (and later recruits') [call: the party's lowest level].
9. **Play length:** the soak estimates play time, but there is no target [call: T1 about 5 minutes,
   T3 about 10, Stage 1 about 15–20].
10. **Le Thanh's name on screen:** the doc writes "Le Thanh" and "Lệ Thanh" [call: "Lệ Thanh" in all
    Vietnamese text].
11. **The `remove` effect** is built and tested, but no unit in the slice has it, so players won't see
    it until a later stage (a note, not a question).
