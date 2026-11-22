# The Last Curtain: design change document (2026-11-21 → 2026-11-23)

**What this is.** Every design change agreed since the game as built on branch `factions` up to
`#UMBRA-297` (`current-design.md`, "the build"). It is the single place to see what changes, what
stays, what is new, and what is still open. Details live in the linked docs.

**How to read it.** "The user" marks the user's decisions. **[call]** marks Claude's proposals the
user hasn't overruled; they can still change. Nothing here is built yet.

**Docs** (all under `claude/last-curtain/` in the Project):

| Doc | Holds |
|---|---|
| `factions-eval-2026-11-21.md` | The evaluation of the build: 20/30, PIVOT |
| `campaign-eval-2026-11-21.md` | The evaluation of the first campaign redesign: 22/30, GO |
| `2026-11-21-campaign-design.md` | The campaign design (revised 2026-11-22) |
| `candidates-2026-11-22.md` | Real-war inspirations, facts, sensitivities |
| `campaign-content-2026-11-22.md` | 10 recruits, 12 zodiac spirits, all stages |
| `cast-2-2026-11-22.md` | Allies, heroes, boon givers, ghosts, player, Le Thanh |
| `art-requests-2026-11-22.md` | The art list for the art agent (~186 images, concept art first; refined 2026-11-23) |
| `art-and-design-review-2026-11-22.md` | The review of the art list and this document, with the user's answers |
| `claude/ai-agency-research-2026-11-22.md` | The AI direction and market research |

---

## 1. Why it changed

The build was a turn-based, single-player *Pokémon Unite*: two factions racing to bank essence on one
stage, with a Dave-the-Diver-style café between matches. The evaluation (2026-11-21, 20/30) found:
- the café and the match didn't feed each other;
- the café was timed busywork;
- the bots reset while the player's career grew;
- no human had played a match.

The user then set the direction in a series of decisions:
1. The café becomes a short, calm break after hard fights; no busywork.
2. Ghosts are restless dead who can be won over in the café.
3. A separate **campaign** (story mode) is added.
4. The game carries **the regret and heartbreak of the Vietnam war**, for a Vietnamese audience.
5. AI agency (characters that remember) is the engine's purpose, and the game should use it.

## 2. The game now, in one paragraph

You are a living guest who stumbles into the Saigon Opera House at night. Le Thanh, a singer burnt
alive there in 1944, rescues you into her café: an illusion where the lingering dead of the war come
to watch her. They have lost their memories to her **rite of grief**. Twelve **zodiac spirits** (the
twelve people who wronged her in life) have gone rogue, and string the dead into red specters. You
fight them room by room in a **tactical RPG**. Between fights, in the café, you serve the dead and
**restore their memories**; restored, they join you. When the twelfth falls, Le Thanh reveals she
trapped them all, and you face her in the last rite. The dead remember you: what you did in fights
and in the café shapes who stands with you at the end.

## 3. Changes by system

### 3.1 Modes

| Build | Now |
|---|---|
| One mode: the faction match | **Campaign** (new, the main game) and, **in a later, separate phase, quick match** (the user): the human side picks from 12 (the player, Le Thanh and the ten allies), the monster side from the 12 zodiacs. It keeps the faction match's rules (+ Perform takes a turn), with no career and no café |

### 3.2 Story and setting

| Build | Now |
|---|---|
| Opera House ghost story, 1944 raid, the singer and the patron | **Real Saigon Opera House (option a, the user).** Le Thanh died in her dressing-room fire on her wedding night, the night of the 1944 raid, her death hidden in the raid's damage. The house's 1955–1975 parliament years stay offstage ("the stage went dark for twenty years"). |
| Ghosts with talks about the house | The **war's dead** linger, unnamed and without memory, under Le Thanh's **rite of grief** (the user). |
| — | **12 zodiac spirits** (Vietnamese zodiac, with the Cat) are the people who harmed her. They are now her **circus band**, ghosts in the **shape of their zodiac animals** (the user, 2026-11-22: intimidating, each carrying one thing from the person it was), on red strings, gone rogue, stringing the dead into **red specters** and luring living guests (the user). |
| — | Le Thanh is the café's **hostess** the whole campaign (the user, 2026-11-23): she rescues you, hosts the dead, regains power with each zodiac, and is revealed as the cause at the end. |
| — | **Her story is revealed bit by bit, one piece after each zodiac falls** (the user). |
| — | **The ending:** win and pacify her, or lose and become her eternal audience (the user). The number of spirits you restored shapes who fights with you and the epilogue [call]. |

### 3.3 Campaign structure

- **Tutorial chapters (3)**: the night tour and rescue, the café, the portrait corridor (the user; names [call]).
- **12 zodiac stages**, one theatre room each, **weakest to strongest** [call]:
  1. Rat: the box office
  2. Rooster: the balcony boxes
  3. Cat: the dressing rooms
  4. Buffalo: the scene workshop
  5. Dog: the stage door
  6. Goat: the costume store
  7. Pig: the wine cellar
  8. Monkey: the fly tower
  9. Horse: the grand staircase
  10. Snake: the orchestra pit
  11. Tiger, her husband: the stalls
  12. Dragon, the patron who set the fire: the stage
- **The final stage**, the Last Curtain: Le Thanh, in two phases.
- **The win condition:** defeat the zodiac (the user). Offerings and Perform are stage gimmicks only
  (the user).
- **Each stage** has authored enemies, map, items and gimmick (the user); new mechanics are taught in
  the stage that brings them (the user).
- **Party size** grows 3 → 4 → 5. **The level cap** rises 5 → 15 [call].
- **Retries** give XP up to the stage's cap; you can **save in the middle of a stage** [call].

### 3.4 Fight rules (campaign)

| Build | Now |
|---|---|
| Essence, Perform, a stage race, a score after 30 rounds | **A conventional tactical RPG:** defeat the zodiac (the user) |
| Respawn after 1 turn at the hub | **Falls stay down** until revived by rare items or abilities (the user). **If the living player falls, it is game over** (the user, 2026-11-23): retry the stage, go back to the café, or quit. The stage is also lost if the whole side is down |
| Move + one action | Unchanged, plus **biding** (the user): skip a whole turn to gain 1 action point (AP), store at most 2, spend at most 2 in a turn, so **at most 3 actions**; no borrowing. Biding gives a small Defence bonus, and enemies can bide too, shown as pips [call] |
| A 3-round fight screen per attack | **Kept** (the user). **Auto-battle later** (the user): 3 rounds played on the board with heal, aggressive, defend or balanced presets |
| Mirrored board, hubs, guardians | Hand-shaped room maps; no hubs or guardians in the campaign |
| — | Boss gimmicks: stealing, height, mirror decoys, falling hazards, fog and locks, class swaps, rule dice, levels and ropes, a timed chandelier, flooding, wards burned by offerings, spreading fire, two-phase bosses [call] |
| — | **New ability effects** (~13): revive, give AP, grant action, last stand, group shield, guard ally, link-join, step through walls, area, silence/poison/root, push/pull, steal, decoy [call] |

**Quick match** (a later phase) **keeps the build's rules**, plus **Perform takes a turn** (banks at the
start of your next turn unless hit; agreed 2026-11-21).

### 3.5 Characters

| Build | Now |
|---|---|
| Five performers (Bảo, Khang, Mai, Lan, Huy) | **The player** + **the ten allies** (the recruits) |
| — | **The player** (the user): a living guest, typed **name**, **male or female**, an **All-Rounder** by default; eyes in shadow, no khăn tang |
| — | **The ten recruits are the official allies** (the user, 2026-11-23). **Bảo and Bà Mai are not allies**; whether they stay as café characters (the doctor who signed her false death paper; the caretaker looking for her son) or are dropped is open |
| — | **Before Tí joins** [call]: you fight T1 alone, and in T3 and Stage 1 Le Thanh lends you two **friendly ghosts** (generic, green). Each recruit who joins takes a lent ghost's place, so the party grows 3 → 4 → 5 with recruits |
| Khang, Lan, Huy (and Bảo and Mai as fighters) | Not in the campaign. The later quick match uses the campaign's characters instead (the user) |
| — | **The 10 recruits** inspired by real people **from the Vietnamese side** (the user), names and stories adjusted, two per class: Bé Tí, Ba Tài (Speedster); Nhân, Khôi (All-Rounder); Diệp, Hiền (Supporter); Quang, Tư Lành (Attacker); Thầy Phong and Thủ trưởng Bảy, an unnamed composite (the user) (Defender) |
| — | **2 heroes, by titles only** (the user): the Young Electrician of Công Lý and the Girl of Đất Đỏ. Honoured café guests, never fought; each gives a **power card** |
| — | **3 boon givers:** the Ferry Mother, the Girl With the Hoe, the Wife of Four Days; each gives a **boon** |
| Le Thanh, an unused cast member | **The hostess and the final boss** (the user): red bridal áo dài; phase 1 the Conductor (her strings), phase 2 the Bride in Embers |
| — | **Ordinary ghosts:** 5 classes × 3 natures, **green friendly / blue-grey neutral / red aggressive** (the user); softer and smaller than zodiacs (the user). One painting per class; the engine tints the nature |

### 3.6 Recruiting, rewards and progression

| Build | Now |
|---|---|
| A saved career, levels and cards carried, bots reset | Levels and stats carried across stages (the user); **stages tuned to the expected level**, so bots don't scale |
| Money from café takings and score; nothing to spend it on | **Money dropped** [call]; Le Thanh's returning power unlocks features |
| — | **Recruiting = restoring memory** [call]: a recruit appears as a red specter marked by a **gold knot** on its strings (the user), is **freed** in the fight (strings cut, never "killed"), then regains memory fragments over café visits until it joins |
| — | **Boons** (from the boon givers) last the **whole campaign**; **power cards** (from heroes) work **once per stage** (the user) |
| — | **Le Thanh unlocks the use** of boons, cards and permanent boosts as she regains power (the user). Schedule [call]: memory restoring after stage 1, boons after 3, cards after 5, boosts after 8, auto-battle after 10 |
| — | Recruits join at the company's average level; characters sitting out a stage still earn part of its XP [call] |

### 3.7 The café

| Build | Now |
|---|---|
| A 150 s timed rush, patience timers, tips, wrong-drink failure, takings | **One calm room**, no timers, no failure, 3–6 minutes (the user) |
| Any ghost, any talk, heard once | Guests are spirits you met in fights, plus heroes and boon givers. **Zodiacs never appear** (the user) |
| — | Each guest: a drink in 1–2 steps, then a talk with a few choices that count; serving the right drink is a good answer [call] |
| — | Le Thanh hosts and dances (the user); her lines read differently after the reveal [call] |
| — | The café grows with each area opened by a zodiac win [call] |

### 3.8 Goals and notices in the fight

| Rejected (the user) | Kept |
|---|---|
| Showing bot intent or plans; Fire Emblem danger tiles; a 2-second bot burst | **Full beat-by-beat telling + Skip + fast-forward** (the user); **lamps** that flicker when a unit can be hit next round; the **ghost audience** reacting; the **hint line voiced by Le Thanh**; a short **recap** after each fight; stage aims where a stage needs them [call] |

### 3.9 AI agency (the engine's purpose)

From `ai-agency-research-2026-11-22.md`. The bar is "the world remembers me", not "they're real
people". AI acts inside rules; facts are authored; local only.
- **Fight memory:** spirits remember who freed them, how, and what happened, and speak of it in the café.
- **Memory fragments**, authored and worded by the built-in model; never invented.
- **Optional free-typed café answers**, sorted by the model into authored answers.
- **Zodiacs that adapt** on retries through the learned turn-based policy, within limits.
- **The storyteller** chooses which spirits appear and visit.
- **Voice sheets** per character (dialect, habits, never-say list) and the line guard.
- **Authored fallbacks** for every model call.
- **Vietnamese:** a testing phase for the model's Vietnamese, with dialects and a Vietnamese line
  guard (the user). If it fails, the lines are hand-written and the model only chooses.
- **Not done:** open chat, cloud AI, marketing it as "an AI game".

### 3.10 Art

- **Style (the user, 2026-11-22):** the existing **dark oil pastel**; figures in **three-quarter view
  turned to the right**; the existing sizes (body 768 px tall, portrait 512 × 627) and cut-out workflow.
- **Visual language:** the dead wear a white **khăn tang** (mourning cloth) over the eyes: the rite
  (the user). **Red strings** mark control; a **gold knot** marks a recruit (the user). **Warm gold
  light** marks the moment of freeing, heroes and the ending. **Allies glow light green** (the user).
  The player is **solid, with eyes in shadow**. Zodiacs are **animal-shaped** ghosts, each with a prop
  from the person it was (a guide, not a rule) (the user).
- **The engine draws the forms** (the user): tint, glow, opacity, strings and the gold knot come from
  the engine; only the eyes (cloth or no cloth) need a second painting.
- **The café painting is the back wall** of the walkable 3D room (the user).
- **Mood:** the dread words for the dead and the zodiacs; **calm, warm, dignified** for heroes, freed
  spirits, the café wall and the good ending (the user).
- **Concept art first** (the user): one of each kind (the player, Le Thanh, a recruit, a zodiac, a
  hero, a ghost, a boss form, a backdrop, the café wall, a still, a card and an icon), plus a mock-up
  style board of the engine forms, before any set is made in bulk.
- **~186 images**, listed with descriptions, sizes and prompts in `art-requests-2026-11-22.md`: 14
  concept pictures first, then a first slice of 20 (11 of them come from the concept art).
- **Rules:** no gore or bodies; invented faces only, never likenesses of real people; no real songs,
  posters or logos.

## 4. What stays from the build

The board and tile engine, tiles and walls, the fight stage and group battles, abilities and level
cards, items, the bots and the soak tool, the café room and walking, the career save (to become v2),
captions and spoken lines, sounds, and the whole faction match (as quick match). The living-town
plugin's memory, built-in model and storyteller are reused for the ghosts.

## 5. Removed from the game

- The café rush and its scoring.
- Money.
- Essence and score in the campaign.
- Respawn in the campaign.
- Bot intent and danger tiles (rejected).
- Lê Văn Tám as an inspiration (disputed history).
- Real names for heroes.
- Real generals.
- The parliament years on screen.

## 6. Risks carried

1. **Content cost** (the top risk): 15 rooms, 15 spirit stories, 10 unique characters with voice
   sheets. Measure one of each in the first slice before committing.
2. **Respect and law:** Vietnamese rules on history, heroes and horrifying images; families of the
   inspirations are alive. A historian or adviser review before release (the heroes' art may be made
   before it, the user).
3. **The model's Vietnamese:** unknown until the testing phase.
4. **No human has played anything since plan 4.** The Windows check of the build still comes first.
5. **Patent:** no procedural enemy hierarchy that rises when it beats you (Nemesis, until 2036).
6. **Steam AI disclosure** for the art and the runtime model.

## 7. Still open

1. Bảo and Bà Mai: café characters, or dropped? (The ten recruits are the allies.)
2. Can the player change class later, or stay an All-Rounder?
3. Le Thanh's 12 story pieces: the words for each.
4. Where Bà Mai's son is found (only if she stays).
5. Revive items now that money is gone (names, drops).
6. Level cap 15: the XP steps.
7. The stage file format, the gimmick switches, save v2.
8. Le Thanh's songs, Hiền's theme and the Girl of Đất Đỏ's song: all written fresh.
9. Review: the recruits' names, looks and fragments; the zodiac order and roles.
10. The two friendly ghosts Le Thanh lends before Tí joins [call].
11. Game over when the player falls: the retry / café / quit screen, and what a retry keeps.

## 8. Next steps [call]

1. The user's Windows play check of plan 4.
2. The art agent's concept art and style board (section 00 of the art list), then the first slice,
   with the engine's form tints, strings and gold knot.
3. The model's Vietnamese testing phase.
4. The plan for the first slice of the campaign: tutorial chapter 1, biding and falls, stage 1 (the Rat),
   Bé Tí restored over two café visits, and Le Thanh's first unlock.
