# The Last Curtain: the game as it stands (design reference, 2026-11-21)

**What this is.** A complete description of The Last Curtain as built on branch `factions` up to
`#UMBRA-297` (2026-11-21), and how it got here. It is written so that a design chat that has never
seen the code can understand the game and propose changes. Every number below is the shipped data
(`plugins/last-curtain/assets/`). Everything is data unless noted, so most changes need no C++.

**Status.** Built and tested headlessly on Linux: 76 match tests, 4 café tests, 12 UI tests, and a
400-match bot soak. The user played the plan-3 build on Windows on 2026-11-20. The plan-4 build
(tile board, group battles, café) has **not yet been played on Windows** (`docs/testing/s42-tiles-groups-cafe.md` §4).

**How to read it.** "The user" means a decision the user made. **[call]** means a choice Claude
made that the user has not ruled on, so it is open to change. Section 20 lists open questions and
known weak spots for the design chat.

---

## Contents

1. The pitch and the pillars
2. How the design got here (history)
3. The game loop: café → match → café
4. The match at a glance
5. The board
6. A turn
7. Battles (group battles)
8. Monsters and spawns
9. The stage and score
10. Hubs and guardians
11. Levels, XP and level cards
12. Abilities (full table)
13. Items, treasure, events, rooms that come back
14. The characters
15. The bots and difficulty
16. Balance targets and the latest readings
17. The match on screen (UX as built)
18. The café (the hub and its minigame)
19. Data files: where every number lives
20. Open questions, weak spots and ideas for the design chat

---

## 1. The pitch and the pillars

**One line.** A turn-based, single-player *Pokémon Unite* inside the Saigon Opera House, with a cosy
ghost café between matches. You play a whole faction of three performers against a bot faction.
You hunt the house's monsters for **essence**, carry it to the **stage** at the centre, and
**Perform** to bank it as score. Kill an enemy carrier to take everything it carries. The most
score after 30 rounds wins.

**The swing (the user, 2026-11-20).** "A swap between cosy simulation and hardcore battle, like
Dave the Diver." By day you run a café for the very ghosts you fight, serving drinks in a timed rush
and hearing their stories. By night you fight for the stage.

**Pillars** (from the factions spec, still true):

1. **One shared throne.** Every fight converges on the single central stage. Score only happens
   there, and only one unit fits on it.
2. **Carrying is risk.** Essence carried is not yet score. A carrier downed by an enemy character
   loses all of it to the killer.
3. **Every turn has a reason.** Monsters to hunt, a carrier to intercept, a stage to take, a hub to
   heal in. The screen always says what to do next.
4. **Fair by construction.** The board is mirrored, so neither side has a better map. Bots play by
   the same rules as the player.
5. **The ghosts are people.** The monsters you fight are the guests you serve. Their talks tell the
   house's history (the 1944 raid, the singer burnt alive, the patron).

**Platform and scope.** PC (Windows) first, single player, offline. 2D sprite figures on a 3D
board, art made with ComfyUI on the user's PC. Built on the Umbra C++20 engine as the plugin
`plugins/last-curtain`. No multiplayer, no online services.

---

## 2. How the design got here (history)

The game has been redesigned four times. Each new design kept the engine work (board, battles,
items, events, saves, bots, soak) and changed the goal. The earlier designs are on file in
`docs/design/` and in the Project docs.

| Dates | Design | What it was | Why it changed |
|---|---|---|---|
| 2026-11-14 → 15 | **The night (co-op/betrayal)** (`game-design.md`, draft 9; `cast.md`) | Seven guests shut in the opera house 19:00-5:00, opening rooms tile by tile (the *Betrayal at House on the Hill* loop). Secret goals; at midnight the curse turns one guest who failed into a traitor with minions. The built-in local model words the lines. Phases S25-S37: board on screen, goals, painted table, room view, cutscenes, sounds. | Slow before midnight (wandering to unlock rooms); weak after it (a lone player could not beat the traitor). |
| 2026-11-16 → 17 | **The battle royale** (`2026-11-16-battle-royale.md`, branch `battle-royale`) | One map, the player against six bots with natures (kind, dirty, wicked). Quests raced for powers, a killer that cannot be fought from midnight, the dead play on as ghosts of the house. | "Outlive everyone" gave no reason to explore or level up. The soak showed aggression wins; exploring did not show in the result (`2026-11-17-hook-research.md`). |
| 2026-11-17 → 18 | **The performances** (`2026-11-17-performances.md`) | Each act, every performer duels a specter on the stage alone; Applause for surviving, damage and the right props. | Dropped by the user on 2026-11-18. The duel came down to matching moves and rolls. |
| 2026-11-19 → now | **The faction matches** (`2026-11-19-factions.md`, branch `factions`) | Turn-based Pokémon Unite: two factions, essence, a central stage, guardians at the hubs, a mirrored lane board. | The current design. |

**The factions evaluation** (`2026-11-19-factions-eval.md`): PIVOT in scope, not in concept,
19/30. The concept has a real gap (no offline single-player turn-based MOBA was found). But it had
about twelve systems at once, so the advice was to build a 3v3 core first and prove the loop with
the soak. The weakest point named was the faction AI: "the game is only as good as the bot side."

**The faction plans** (each in `docs/design/plans/`):

| Plan | Date | Commits | What it added |
|---|---|---|---|
| 1, the core | 2026-11-19 | #251-#262 | Two factions, essence, Perform, 3-round battles, board abilities, the mirrored generator, guardians, respawn, monster behaviours, faction bots and the soak. |
| 2, the screens | 2026-11-19 → 20 | #264-#279 | Spawned monsters and hunters, treasure cards, returning rooms, level cards, event dice, the match on screen with ComfyUI art. |
| 3, polish | 2026-11-20 | #280-#289 | The user's notes 1-16 and the separate fight screen: walking animation, camera focus, zone outlines, frames, a fight stage with backdrops, board abilities give XP. |
| 4, tiles, groups, café | 2026-11-20 → 21 | #291-#297 | The user's 19 notes: walls as tiles, two same-side units per tile, group battles, a reach column, the match told blow by blow, button states. Then the café hub, the career, and two new ghosts. |

**Rulings by the user that shaped the current game** (in order):
- 3v3 by default (1v1 to 5v5 allowed); the player controls a whole faction.
- A central, fixed stage; one unit at a time; Perform to score.
- Interleaved turns by Speed (not whole-side phases).
- The same character may be on both sides, never twice on one side.
- Bot difficulty easy/medium/hard differs only in how well bots choose.
- 30 rounds; "N turns left" on screen; no clock.
- Healing others costs the healer's own HP. Anything unusable says why in a tooltip.
- No traps. Rooms come back.
- Walls are their own tiles (rebuild the board as tiles).
- Each ability has a reach column (walk or straight).
- Only same-side units share a tile.
- Group battles: if two units share a tile and attack, both fight (2v2). Everyone picks in speed order.
- Café: free walk (WASD or click); a saved career; a timed rush (Dave-style); drinks are a few steps at stations; ghosts get art and new monster rows.

---

## 3. The game loop: café → match → café

```
start the game → the café (day N)
   ├─ open the shop (once a day) → a 150 s rush serving ghosts → takings → money
   ├─ talk: served ghosts tell a story once each
   ├─ the notice board: see the company, pick who walks
   └─ the stage door (shop closed) → pick 3 characters → the match (30 rounds)
                                                          → the end screen → "Back to the cafe" (day N+1)
```

- The match's end writes the player's side back into the **career**: levels, stats, abilities and
  upgrades. Money grows by side 0's score × `moneyPerEssence` (1). The day goes up by one.
- The next match starts the player's characters as the career left them (`ApplyCareer`).
  **The bot side starts fresh at level 1.** See section 20.
- Money is earned (café takings, tips, match score) but **nothing spends it yet**. The shop for
  abilities, upgrades and characters is the user's stated next use.
- The career file: `<prefs>/last-curtain/career.json`, version 1, written atomically.

---

## 4. The match at a glance

| | |
|---|---|
| Sides | Two factions, 3 characters each (`options.csv side`, 1-5). The player is side 0 (bottom). |
| Length | 30 rounds (`options.csv rounds`). A round = every character acts once in Speed order, then the monsters' phase. |
| Win | Most score after the last round. Tie: the side whose guardian alone stands, then more kills, else a draw. |
| Score | Only by **Perform** on the stage: all essence carried becomes the side's score. |
| Essence | From monsters (their `essence`), treasure, events, and by killing enemy carriers (all of theirs). |
| Death | Down at 0 HP. Out for 1 turn (`respawn.turns`), then back at the hub with full HP. |
| Bots | The other side, and the player's side never. Easy/medium/hard. |

**Picking.** The player picks its three characters. The bot side is a rigged pick: no role twice,
drawn from Defender, Supporter, All-Rounder and Attacker before the Speedster. A character may be on
both sides; theirs is labelled "Name (theirs)".

**Order.** Every character on the board is in one order by Speed, highest first, ties broken by a
seed-drawn key. The order never changes during a match. Speed therefore decides who acts first each
round, which matters for reaching the stage.

---

## 5. The board

**Size.** 9 × 15 *squares*, expanded into 17 × 29 *tiles* (`stride` 2). Each tile is 1 m on screen.

**The mirror.** Side 1's half is side 0's half flipped: `M(x, y) = (x, H−1−y)`. Every door and room
maps across, so the steps from hub 0 to any square equal the steps from hub 1 to its mirror. The
middle row is its own mirror, so the stage is equally far from both hubs. The generator tries up to
64 seeds until `Fair` passes. Proof in `modules/match/include/Board.md`.

**Square kinds.** Wall, hall, room, centre, hub. Plus the stage (the centre square of the middle
row) and each guardian's door square.

**Layout.** Each hub is in the middle of its end row, with a hall either side. Above it is the
guardian's square on a cross hall. Three lanes (`lanes` 3) run toward the centre. Rooms hang off the
lanes. The centre is the middle band around the stage.

**Placed by distance from the hub** (per half):
- 3 weak dens, 2-4 steps from the hub (`weakMin`/`weakMax`, `weakPerHalf`).
- 2 strong dens, 5-7 steps (`strongMin`/`strongMax`, `strongPerHalf`).
- 2 event rooms, at least 2 apart (`eventsPerHalf`, `eventGap`).
- 2 item (treasure) rooms (`itemsPerHalf`).
- Centre spawn squares: on the middle row, 2 steps either side of the stage (`spawn.centre`).

**Tiles.** Square (x, y) becomes tile (2x, 2y). Between two squares is a tile that is either a
doorway or a wall. Corner tiles are open only inside open blocks. Walls are drawn as papered blocks.
Movement and range count tiles; a square is two tiles' walk from its neighbour.

**Sharing.** Up to 2 units per tile (`squareCapacity`), and only of the same side. Monsters do not
share. The stage holds one.

**Large monsters.** A monster of `size` s covers s × s squares. Covered squares are occupied.
"Next to" means any covered square beside any square of the other. (The 2×2 chorus and 3×3 spider
exist in data but no longer spawn.)

---

## 6. A turn

1. **Move.** Up to `move` tiles (Bao, Khang, Mai: 6; Huy: 8; Lan: 10), through doorways, onto a
   free tile or a friendly one with room. An enemy hub is shut while its guardian stands.
2. **Undo.** The move can be undone until the action.
3. **One action:**
   - **Attack** a unit exactly one step away, not of your faction → a battle (section 7).
   - **A board ability** (Hit an enemy or Heal a friend, within range, costing HP).
   - **Use an item** (heal or XP).
   - **Pick up** an item or open a treasure on your tile.
   - **Look into the room** (answer an event).
   - **Perform** (on the stage, carrying essence).
4. **End turn** (`autoEndTurn` 0: the player ends it).

**Healing at home.** Standing on your own hub when the move is committed (at the action or the
turn's end) restores full HP. Stepping in and undoing heals nothing.

**Level choices first.** A character with a level-up waiting must pick its card before anything
else, even while down.

**The monsters' phase.** After the last character, every monster acts by its behaviour (section 8).
Then a new round starts.

**Why not.** `Check` gives a reason for every refused action, shown as a tooltip: not enough HP,
no target in reach, enemy hub shut, full HP for a heal, and so on.

---

## 7. Battles (group battles)

**Starting one.** Attack a unit one step away. The battle takes in each side's **group**: the
attacker plus whoever of its side shares its tile, and the defender plus whoever shares the
defender's tile. So 1v1, 2v1, 1v2 or 2v2. Places: 0-1 attackers, 2-3 defenders.

**Rounds.** Up to 3 rounds (`battle.rounds`). Each round every standing fighter picks one battle
ability, in order: anyone with First Strike (Lan's `duel` passive) first, then by Speed, the
attackers first on a tie.

**Picks.**
- A **strike** names a standing foe of the other group.
- A **shield** adds its power to the picker's place's shield for the rest of the battle.
- A **heal** gives HP to the picker (Mai's `mend`).
- An ability is usable only while HP is above its cost. Every role has a free (0 HP) strike, so a
  fighter at 1 HP can always act.

**Damage.**
```
damage = power (or the dice roll) + attacker's Attack + upgrades
         − (defender's Defence + the defender's shield this battle)
         at least 1 (battle.minDamage)
```

**The end.** After 3 rounds, or when one whole group is down.

**Falls.**
- A character downed by a character gives the killer **all its essence** and the killer gets 3 XP
  (`killXp`) and a kill.
- A character downed by a monster drops its essence on the floor.
- A monster downed by a character gives its essence and XP and never returns. Its square spawns a
  new one later.

**Monster picks.** Monsters choose by value: a strike is worth its damage (1000 if it downs the
foe); a shield `50 + power` only if the foe's hardest next blow would down it; a heal `50 + HP`
only below half HP. They strike the foe they can down, else the one with least HP.

**Board abilities** are outside battle: one Hit or Heal as the turn's action, with range.

---

## 8. Monsters and spawns

**Behaviours.**
- **Stay**: stands in its den.
- **Flee**: steps away from the nearest character (in code; no current monster uses it).
- **Stage**: walks toward the stage and stops beside it (the Prompter). A stage-blocker.
- **Hunt**: centre only. Finds the nearest character within `sight`, walks toward it, and attacks
  (the Usherette). Its battle pauses the phase; the defender's side picks.
- **Guard**: guardians. Each monsters' phase, deals its first ability's damage to every enemy
  character beside it, with no battle. Can down them. Never starts battles.

Monsters fight back only when attacked. A den's monster belongs to the side whose half it is in, so
only the other side can fight it. **You farm the enemy's dens, not your own.** Centre monsters
belong to no one.

**The roster** (`monsters.csv`):

| id | Name | Tier | Behaviour | HP | Atk | Def | Spd | Essence | XP | Lvl | Abilities |
|---|---|---|---|---|---|---|---|---|---|---|---|
| moths | Moth swarm | weak | stay | 8 | 2 | 1 | 2 | 1 | 2 | 1 | claw |
| rats | Rat pack | weak | stay | 12 | 3 | 1 | 3 | 12 | 3 | 3 | bite |
| ticket-girl | The Box Office Girl | weak | stay | 12 | 3 | 1 | 4 | 12 | 3 | 3 | claw |
| stagehand | Hanged stagehand | strong | stay | 18 | 4 | 3 | 2 | 6 | 5 | 3 | bite |
| prompter | The Prompter | strong | stage | 20 | 4 | 3 | 3 | 22 | 6 | 3 | bite |
| pianist | The Pit Pianist | strong | stay | 22 | 5 | 2 | 3 | 20 | 7 | 4 | bite; brace |
| usherette | The Usherette | strong | hunt (sight 6) | 16 | 5 | 2 | 3 | 7 | 5 | 2 | claw; brace |
| usher | The Usher | guardian | guard | 20 | 6 | 2 | 1 | 20 | 10 | 5 | crush |
| ghost-chorus | Ghost chorus | strong | hunt, 2×2 | 30 | 6 | 3 | 2 | 18 | 8 | 4 | bite; mend |
| the-spider | The Great Spider | great | hunt, 3×3 | 50 | 8 | 4 | 3 | 40 | 14 | 6 | crush; brace; mend |

The chorus and the spider are in the data but no longer spawn (they appear only as café guests).

**Spawns** (`spawns.csv`). Three zones, each square mirrored with its opposite so both sides get
the same monster in the same round. A zone's kind at round r is its latest row with `from ≤ r`.
When a monster is downed, its square respawns after `every` rounds. A spawn is announced 1 round
ahead (`spawn.warn`) on the board.

| Zone | From round | Monster | Every |
|---|---|---|---|
| weak dens | 1 | moths | 6 |
| weak dens | 10 | rats | 6 |
| weak dens | 18 | ticket-girl | 6 |
| strong dens | 1 | stagehand | 10 |
| strong dens | 12 | prompter | 6 |
| strong dens | 20 | pianist | 6 |
| centre | 1 | usherette | once |

So the house escalates: essence per kill rises with the round (moths 1 → rats/ticket-girl 12;
stagehand 6 → prompter 22 → pianist 20).

---

## 9. The stage and score

- The stage is the centre square of the middle row. It never moves (it moved in plan 1; the user
  chose to fix it). It holds one unit. Monsters never stand on it.
- Standing there scores nothing. **Perform**, as the turn's action on the stage with essence
  carried, adds all of it to the side's score.
- So the stage is contested: the first to reach it each round can bank; anyone next to it can
  attack the performer before or after.
- Readings: about 65 essence performed per match (both sides), and the lead last changes around
  round 14.5 on average.

---

## 10. Hubs and guardians

- Each side's hub is at its end of the board. Your own hub restores full HP. Downed characters
  return there after one turn.
- The guardian (the Usher, 20 HP, Atk 6) stands on the door square in front of each hub. It hits
  every adjacent enemy for its `crush` (power 6) each monsters' phase, without a battle.
- Its own side cannot attack it. Killed by the enemy, it gives 20 essence and 10 XP and **never
  returns**: that hub is then open to the enemy.
- A standing guardian is the first tie-break.
- Guardians give about 6-7% of all essence gained (soak). Before plan 1's fix they never fell
  (the bots never attack a guardian they cannot beat in one battle); the user ruled that a
  guardian may fall in one battle, and it was cut to 20 HP.

---

## 11. Levels, XP and level cards

**XP from:** monsters (their `xp`), character kills (3), items and events, and **every board
ability used**, hit or heal (1, `abilityXp`).

**Levels.** XP thresholds `[3, 7, 12, 18]`, so levels 2 to 5 (level 5 is the cap). Each level gives
+2 max HP (`levels.hp`) and deals **3 level cards** (`levels.cards`), drawn by weight without
repeats. The player picks one.

| Card | Kind | Effect | Weight |
|---|---|---|---|
| learn | learn | a new ability of the role (a `jobs.csv` row from level 2 up to the unit's level) | 4 |
| attack | stat | +1 Attack | 3 |
| defence | stat | +1 Defence | 3 |
| hp | stat | +4 max HP (and current) | 3 |
| speed | stat | +1 Speed | 1 |
| move | stat | +2 Move | 1 |
| sharpen | upgrade | +2 power on one known ability | 3 |
| ease | upgrade | −1 HP cost on one known ability | 2 |

Bots favour a new ability, attack and power.

**Jobs** (`jobs.csv`). Level-1 abilities are known from the start; later rows are learnt through
"learn" cards ("a/b" rows offer either).

| Role | Level 1 | Level 2 | Level 3 | Level 4 |
|---|---|---|---|---|
| Attacker | jab, snipe | cleave / gamble | | execute |
| Defender | shove, bash | brace / rally | | wall |
| Supporter | tap, tend | mend / sting | | bloom |
| Speedster | flick, dart | lunge / gamble | duel | blur |
| All-Rounder | slash, throw | guard / lunge | | cleave |

---

## 12. Abilities (full table)

`use`: battle (picked in a battle), board (the turn's action), passive. `reach`: walk (counted
through doorways) or straight (Manhattan distance, walls ignored). Range is in tiles. A board heal
on someone at full HP is refused.

| id | Use | Class | HP cost | Power | Range | Reach | Who |
|---|---|---|---|---|---|---|---|
| jab | battle | strike | 0 | 3 | | | Attacker |
| cleave | battle | strike | 3 | 7 | | | Attacker, All-Rounder |
| gamble | battle | strike | 2 | 2d6 | | | Attacker, Speedster |
| execute | battle | strike | 4 | 9 | | | Attacker |
| snipe | board | hit | 3 | 4 | 6 | straight | Attacker |
| shove | battle | strike | 0 | 2 | | | Defender |
| brace | battle | shield | 1 | 4 | | | Defender |
| rally | battle | shield | 2 | 6 | | | Defender |
| wall | battle | shield | 3 | 8 | | | Defender |
| bash | board | hit | 2 | 3 | 2 | walk | Defender |
| tap | battle | strike | 0 | 2 | | | Supporter |
| mend | battle | heal (self) | 2 | 5 | | | Supporter |
| sting | battle | strike | 2 | 5 | | | Supporter |
| tend | board | heal (ally) | 3 | 6 | 4 | walk | Supporter |
| bloom | board | heal (ally) | 4 | 10 | 6 | walk | Supporter |
| flick | battle | strike | 0 | 3 | | | Speedster |
| lunge | battle | strike | 2 | 6 | | | Speedster, All-Rounder |
| blur | battle | strike | 3 | 7 | | | Speedster |
| duel | passive | first strike | | | | | Speedster |
| dart | board | hit | 2 | 3 | 8 | straight | Speedster |
| slash | battle | strike | 0 | 3 | | | All-Rounder |
| guard | battle | shield | 1 | 3 | | | All-Rounder |
| throw | board | hit | 3 | 4 | 4 | straight | All-Rounder |
| claw | battle | strike | 0 | 2 | | | monsters |
| bite | battle | strike | 0 | 4 | | | monsters |
| crush | battle | strike | 0 | 6 | | | monsters |

Note: HP is the only resource. Every non-free ability costs HP, so a strong turn weakens the user.
"Healing others costs the healer's own HP" was a user ruling.

---

## 13. Items, treasure, events, rooms that come back

**Items** (`items.csv`): **bandage** (heal 6), **programme** (+3 XP). A bag holds 4 (`carry`).

**Treasure.** Each item room holds a treasure. Picking it up deals 2-5 face-down cards drawn by
weight; the opener picks one blind (the player chooses which card; bots pick blind too). Then the
action is spent.

| Card | Gives | Weight |
|---|---|---|
| bandage | the item (or 2 essence if the bag is full) | 3 |
| programme | the item (or 2 essence) | 2 |
| purse | 2 essence | 3 |
| hoard | 4 essence | 1 |
| lesson | 3 XP | 2 |
| rest | heal 6 | 2 |

**Events** (`events.csv`), answered by "Look into the room". Two dice are shown against the chance,
display only (the rules roll the percent).

| Event | Choice A | Choice B |
|---|---|---|
| applause | **Take a bow**: 60% gain 2 essence, else lose 3 HP | Walk on (nothing) |
| mirror | **Touch the glass**: 50% gain 3 XP, else lose 4 HP | Look away: a bandage |

An event that takes the last HP downs the character; its essence drops there.

**Rooms come back.** A claimed treasure or answered event reopens 16 rounds later (`rooms.respawn`)
in a random free room of the same half, as a new event or treasure. The board marks it a round
ahead. No traps (the user).

---

## 14. The characters

Five of the cast of seven are in the match. The cast's full pages (backstories, voices, quirks) are
in `cast.md`; some details changed on 2026-11-19 (Khang is now a boy of 12, Lan a woman of 26) and
their pages are not yet rewritten. Sprites exist for all five (blindfolded figures, ComfyUI).

| id | Name | Role | HP | Atk | Def | Spd | Move | Story hook (cast and café talks) |
|---|---|---|---|---|---|---|---|---|
| bao | Dinh Quoc Bao | Attacker | 20 | 6 | 2 | 4 | 6 | The house's old company doctor; wrote false death papers ("fall from the flies"). |
| khang | Vo Minh Khang | Defender | 30 | 3 | 5 | 2 | 6 | A boy of 12; a debt tied to the patron. |
| mai | Phan Thi Mai | Supporter | 20 | 2 | 3 | 3 | 6 | Caretaker for forty years; leaves rice by the trap door for her son under the stage. |
| lan | Tran Ngoc Lan | Speedster | 18 | 4 | 2 | 6 | 10 | Appraiser of the patron's costumes and masks. |
| huy | Hoang Gia Huy | All-Rounder | 24 | 4 | 3 | 4 | 8 | A student who films haunted buildings; his footage. |

**Not in the match:** Truong Uyen Nhi (10, origami prodigy, looking for her father) and Le Thanh
(25, the house's famous singer in a red bridal dress). Both are the user's own characters.

**Role identities in play:**
- **Attacker (Bao):** highest Attack, a 6-tile straight Snipe. Glass: 20 HP, Def 2.
- **Defender (Khang):** 30 HP, Def 5, shields (brace/rally/wall). Slowest (Spd 2).
- **Supporter (Mai):** the only healer (tend, bloom on the board; mend in battle), paid in her own HP.
- **Speedster (Lan):** acts first, moves 10, Dart reaches 8 straight, Duel strikes first in battle.
  Fragile (18 HP).
- **All-Rounder (Huy):** middle stats, a Throw and both strikes and a shield.

---

## 15. The bots and difficulty

`modules/matchbots` (`ChooseAction`). The bots use the same `Check`/`Apply` as the player.

**Roles per bot character each turn:** Guard (stay near the hub), Defend (meet threats), Carry
(farm and bring essence to the stage).

**Scoring.** For each reachable tile, the bot scores the place plus the best action there. Weights
(in data):

| Weight | Value | Meaning |
|---|---|---|
| performPerEssence | 10 | value of banking each essence |
| stealPerEssence | 4 | value of killing a carrier, per essence carried |
| killCharacter | 12 | value of a character kill |
| killMonsterPerEssence | 3 | value of a monster kill, per essence |
| stagePull | 2.5 | pull toward the stage while carrying |
| huntPull | 1.2 | pull toward monsters |
| homePull | 3 | pull toward the hub when hurt |
| guardPull / defendPull | 2 / 2 | role pulls |
| huntDanger | 30 | penalty for tiles a hunter can reach |
| fallRisk | 20 | penalty for being downed |

**Difficulty** (`bots.noise` `[60, 25, 0]`): easy picks a worse choice 60% of the time, medium 25%,
hard always takes its best. The match default is medium (`options.csv`). In battle, bots pick
targets and abilities by expected value.

The soak (`UmbraTool match-soak`) plays bots against bots on 400 seeds (about 19 minutes).

---

## 16. Balance targets and the latest readings

**Targets** (`rules.json balance`):
- Each side wins 45-55%.
- The first mover's edge is at most 5 points.
- No single score source above 60%.
- Comebacks at least 25% (the side behind when two thirds of the rounds have passed wins anyway).

**Readings** (400 matches, bots vs bots):

| Build | Side 0 wins | First mover | Essence by source: monsters / kills / guardians (%) | Comebacks | Performed | Lead last changes |
|---|---|---|---|---|---|---|
| After #292 (group battles) | 48.4% | 52.4% | 43.8 / 50.1 / 6.1 | 30.1% | | |
| After #297 (new ghosts) | 48.0% | 49.7% | 42.4 / 50.4 / 7.2 | 29.9% | 65.6 a match | round 14.5 |

The bot-vs-bot soak shows the **rules** are fair. It does not show whether a human finds it fun or
whether a human beats medium bots easily. Neither has been measured.

---

## 17. The match on screen (UX as built)

The rules decide everything at once; the display (`MatchPlay`, `MatchTable`) tells it one beat at
a time and never decides a rule.

**The board view.**
- A 3D board of 1 m tiles; walls are papered blocks. Zone edges: hubs in their side's colour, the
  centre red, the stage gold.
- Figures are flat sprites that walk along doorways, stay the same size on screen at any zoom, and
  lean toward the camera. Two on a tile stand apart.
- A name/level/HP label and a bobbing turn arrow sit on the acting figure's head.
- The camera follows tightly, frames whoever the last line is about, and moves to a portrait
  clicked in the order strip.
- Reach tiles blink as a 50% cyan wash (orange when armed); targets blink red.
- Spawns are announced a round ahead; returning rooms are marked a round ahead.

**The HUD.**
- The order strip: 12 portraits from the acting character onward, with a "next turn" marker; each
  has a tip with its numbers, and a click looks at it.
- The actions panel docks beside the acting character. Each button has an enabled state and a
  "why not" tooltip. Buttons have hover, pressed and selected states.
- A hint line always says what to do next.
- Essence is shown big and gold. Score for both sides. "Turn N / M turns left" banners each round.
- The log of every caption.
- Thin ComfyUI frames and cards, 12 icons.

**The flow of a player turn.** Hover a lit tile to see the path and a forecast; click to walk.
Then pick an action; Attack or an ability arms it and shows targets; click a target. The forecast
shows damage ranges, healing and cost.

**The fight stage.** When a battle starts, the screen dips to dark and the battle plays on its own
stage, with a painted backdrop chosen by the defender's square kind (hall, room, centre, hub). Only
the fighters are shown (up to four). On the player's pick, an arrow bobs over the picker and the
battle abilities show their preview; strikes are offered per foe ("slot:target"). Every blow is
told beat by beat (1.8 s), the player's too; HP cards follow the told lines; a fighter downed
stands until its fall is told. Nothing else moves while the stage is up.

**Other screens.** The pick screen (chosen cards marked, bot level button), level cards (they say
what each ability does), treasure (face-down cards), events (card, two choices with their chance,
tumbling dice, the result), the end screen (winner, scores, kills, essence performed, monsters
downed per side, "Back to the cafe").

**Pace** (`apps/LastCurtain/assets/options.csv`): a beat 2.5 s, a blow 1.8 s, plain and fast beat
settings, `Skip` tells everything at once and runs the bots to the player's next turn.

**Sounds and lines.** Each told line brings a motion (walk, attack, hit, fall, level-up, loom) and a
sound. Characters say short lines in bubbles on blows, falls, performances and levels (from
`match-words.json`; the built-in model could word them later).

---

## 18. The café (the hub and its minigame)

**Why.** The user (2026-11-20): a hub to rest, view characters, tend units, and later shop,
multiplayer and exploration areas. It looks like a coffee shop. You can open or close the shop and
sell coffee to the ghosts for money (for abilities, upgrades and characters later). Conversations
between ghosts and characters tell their backstory. "A swap between cosy simulation and hardcore
battle, like Dave the Diver."

**The room** (`room.json`). A 12 × 8 m room seen from the open front. One character walks it (WASD
or click; speed 3.2 m/s). E uses the nearest spot within 1.1 m:

| Spot | What E does |
|---|---|
| The shop sign | Open the shop (once a day) or close it |
| Phin filters, lotus tea, condensed milk, ice box | Add that step to the cup |
| The slop bucket | Empty the cup |
| Four tables | Serve the cup to the guest seated there |
| The notice board | Show the company: each character's portrait, level, abilities; "Walk as" to change the walker |
| The stage door | Go to the next match (only while the shop is closed) |

**A shift** (`cafe.json`). 150 s. The first guest comes at 3 s, then one every 6-12 s while a seat
is free (4 seats). Each guest orders one of its liked drinks and has a patience timer.

- **Make the drink:** visit the stations in the recipe's order, up to 3 steps.
- **Right drink:** the price plus a tip of `ceil(patience left / patience × 3)`, so 1-3.
- **Wrong drink:** the guest leaves unpaid (lost).
- **Patience runs out:** the guest leaves (lost).
- After closing time, the shift ends when the seats are empty. Closing the sign ends it at once.
- The takings screen shows; the money goes into the career once.

**Recipes** (`recipes.csv`):

| Drink | Steps | Price |
|---|---|---|
| Black coffee (den) | phin | 2 |
| Milk coffee (sua-nong) | phin → milk | 3 |
| Iced black coffee (den-da) | phin → ice | 3 |
| Iced milk coffee (sua-da) | phin → milk → ice | 5 |
| Lotus tea (tra-sen) | tea | 2 |
| Iced tea (tra-da) | tea → ice | 3 |
| Milk tea (tra-sua) | tea → milk | 3 |

**Guests** (`guests.csv`): the ten ghosts of the match.

| Ghost | Orders | Patience (s) | How often |
|---|---|---|---|
| Moth swarm | lotus tea, iced tea | 24 | 3 |
| Rat pack | black, iced black | 22 | 3 |
| Hanged stagehand | black, milk coffee | 30 | 2 |
| The Prompter | milk coffee, milk tea | 34 | 2 |
| The Usher | black, iced milk coffee | 40 | 1 |
| The Usherette | iced milk coffee, iced tea | 28 | 2 |
| Ghost chorus | milk tea, iced milk coffee, lotus tea | 26 | 2 |
| The Great Spider | iced milk coffee | 45 | 1 |
| The Pit Pianist | iced black, milk coffee | 32 | 2 |
| The Box Office Girl | iced tea, milk tea, black | 30 | 2 |

**Talks** (`talk.json`). After a served drink, the ghost talks with the walker: its first talk with
that character not heard yet, else its first with anyone. Each is told once (`career.heard`). Three
or so lines, in bubbles. Examples of the content: the moths were "the lamp's" until the lamp went
out in '44; the rats know "the boy under" the old stage, and Mai still leaves rice by the trap door;
the stagehand tells Bao he wrote "fall from the flies" on his death paper. Themes: the 1944 raid,
the singer burnt alive, the patron and his man, Mai's son under the stage, Bao's false papers,
Khang's debt, Huy's footage.

**The career** (`career.json`, version 1): money, day, each character's progress (level, XP, stats,
abilities by id, upgrades), talks heard. A broken or other-version file is refused and the career in
memory is kept.

**Art status.** The room is boxes, lamps and papered walls; the stations are coloured boxes; the
ghosts are flat sprites. **The ten ghost sprites are not generated yet** (ComfyUI runs on the user's
PC); every ghost falls back to the shared `specter` until its art exists. A painted café backdrop is
also still to make.

---

## 19. Data files: where every number lives

All under `plugins/last-curtain/assets/`.

| File | Holds |
|---|---|
| `match/rules.json` | board size, lanes, tile capacity, den/event/item counts and distances; respawn; battle rounds, min damage; levels (XP steps, HP per level, kill XP, ability XP, cards, upgrade sizes); treasure counts; room respawn; spawn warning and centre distance; bag size; bot noise; balance bands |
| `match/options.csv` | rounds, side size, auto end turn, bot level |
| `match/characters.csv` | the five characters' role and stats |
| `match/jobs.csv` | each role's abilities by level |
| `match/abilities.csv` | every ability (use, class, cost, power or dice, range, reach) |
| `match/items.csv` | items |
| `match/events.csv` | events and their two choices |
| `match/treasure.csv` | treasure cards and weights |
| `match/levelcards.csv` | level cards and weights |
| `match/monsters.csv` | monsters (tier, behaviour, stats, essence, XP, size, sight, abilities, sprite) |
| `match/spawns.csv` | what spawns where and when |
| `match-words.json` (app) | every caption, tooltip, banner and spoken line |
| `apps/LastCurtain/assets/options.csv` | pace: beat and blow seconds, camera, order-strip length |
| `cafe/cafe.json` | shift length and arrivals, seats, tip, steps, money per essence |
| `cafe/stations.csv`, `recipes.csv`, `guests.csv`, `talk.json`, `room.json` | the café |

Rule docs beside the code: `modules/match/include/Match.md` (rules), `Board.md` (generator and
fairness proof), `modules/matchbots/include/Bots.md` (bots and soak history),
`modules/cafe/include/Cafe.md`, `apps/LastCurtain/MatchPlay.md`, `MatchTable.md`, `CafePlay.md`,
`CafeRoom.md`.

---

## 20. Open questions, weak spots and ideas for the design chat

These are observations, not decisions. The user decides.

**A. The career snowball (most important).** The player's characters keep their levels, cards and
upgrades between matches, but the bot side starts at level 1 every match. After a few matches the
player's side will outclass medium bots. Options to discuss: scale the bot side to the career's
level; cap carried progress (keep abilities, reset levels); make progress horizontal (new options
rather than bigger numbers); or raise the house's difficulty by day.

**B. Money has no use yet.** The café and matches pay money; nothing spends it. The user's named
uses: new abilities, upgrades, unlocking characters. Also possible: café upgrades (a fifth seat, a
faster phin, a new station and recipes), cosmetics, the hub's planned shop area. A Dave-the-Diver
loop usually needs each side to feed the other: what does the café give the match, and what does
the match give the café (ingredients from monsters? new guests unlocked by defeating a ghost?).

**C. The café is thin as a game.** Seven recipes from four stations, one cup at a time, no
upgrades, no events in the shift, no reason to serve one guest before another beyond patience. Ideas:
guests whose order depends on the last match; regulars who remember you; rush days; a VIP ghost; a
second cup; recipe unlocks.

**D. Talks.** About two talks per ghost; once heard they are gone. The built-in local model
(llama.cpp, already in the engine from the living-town work) could word new lines, but the facts
should stay authored.

**E. Unused cast.** Nhi and Thanh are not in the match. Thanh (the singer burnt alive?) is central to
the house's story in the talks. A sixth role, an unlock, or a story character in the café?

**F. Match pacing and the human.** Balance is measured only bot vs bot. Not yet known: how long a
human match takes, whether a human beats medium bots, whether 30 rounds is too long, how often the
player waits on bots. The beat timings (2.5 s a beat, 1.8 s a blow) slow the bots' turns down for
readability.

**G. Systems that are present but small.** Two events; two items; a fleeing behaviour no monster
uses; the chorus and spider no longer spawn; no status effects; no area attacks; HP is the only
resource. The factions spec's later ideas (fog, a shop in the match, special effects) were left out
of plan 1 on purpose.

**H. Dominant strategies to watch.** You farm the enemy's dens only, which pushes everyone across
the middle. The stage is one tile and the order is fixed by Speed, so Lan (Speed 6, Move 10) acts
first every round and may own the stage (not measured). About half of all essence now comes from killing carriers (50%) against 42% from
monsters: the cap (60%) holds, but stealing is the largest source, so the game leans to fighting
over farming.

**I. The tutorial.** The spec says a 1v1 tutorial match where needed. Not built.

**J. The night designs.** The co-op night and the battle royale remain on other branches (`main`,
`battle-royale`). Some of their ideas could return: the traitor at midnight, secret goals, the
killer who cannot be fought, the dead as ghosts of the house.

**Not yet verified.** Every plan-4 feature (tiles, group battles, the café) needs the user's
Windows play check. Clang and ASan runs were not done for #291-#297.
