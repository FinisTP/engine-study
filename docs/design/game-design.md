# The Last Curtain (working title): game design, draft 8 (approved 2026-11-14)

Date: 2026-11-14. Status: **approved by the user 2026-11-14** as the design to build from. Nothing in it is built yet. Plan: `plans/2026-11-14-night-simulation.md`.
Companion: `cast.md` (the seven characters).

How to read it: plain statements are the user's decisions. **[call]** marks a gap filled by
Claude that the user has not confirmed. Section 20 lists the questions still open.

---

## 1. The game in one paragraph

Seven people are shut inside Saigon's old opera house for one night, 19:00 to 5:00. Turn by turn they open
its hidden rooms, find items, meet what lives there, fight, and grow stronger. The house holds omens: cursed
keepsakes that make whoever takes one stronger, at a risk that grows with every omen and every
hour. When the curse takes one of them, maybe the player, the theatre shifts time to midnight and
that person returns as something else, with its minions; the rest have until 5:00 to destroy it
or stay in the theatre forever. Every night has a different building, a different victim and a different secret.

It is a single-player digital game built on the core loop of *Betrayal at House on the Hill*:
explore tile by tile, then a betrayal. It uses none of that game's text, names, scenarios or art.

## 2. What it must be, and what it must not

**Pillars**

1. **Who will turn?** Before midnight the tension is watching six others (and yourself).
2. **The building is the board.** Opening rooms is the main verb; every room opened changes what everyone can do.
3. **Prepare, then survive.** The evening is spent getting ready for a fight whose shape you do not yet know.
4. **Every night differs:** the layout, the one who turns, their secret, the items and events.

**Not goals**

- No daily-life simulation (needs, schedules, wages): removed with the town.
- No multiplayer.
- No campaign or progress carried between nights in the first version.

**Would make it a failure even if it works**

- A night where the player cannot tell why they lost.
- Bots whose turns are slow or unreadable (six of every seven turns are theirs).
- A cost that grows with every mechanic: anything that does not serve a pillar is cut (section 18).

## 3. Setting

The user asked for a setting other than a mansion that still fits opening tiles, and then for
**a well-known place in Saigon**. A theatre fits the game best (reasons below), so the
recommended place is the city's opera house.

**Recommended: the Saigon Opera House (Nhà hát Thành phố), after hours.** Opened in 1900 on Lam
Sơn Square, restored in 1998, a national monument since 2012, about 500 seats, and still a
working theatre. In the game a wealthy patron hires the house for a private midnight gala and
invites seven people. The doors lock at 19:00.

Why a theatre fits:

- **Rooms are distinct and nameable:** foyer, auditorium, stage, orchestra pit, dressing rooms,
  costume store, prop room, fly loft, lighting box, under-stage. A grid of them reads at a glance.
- **Its machinery is the mechanics:** trapdoors and catwalks are the transport rooms; stage
  rigging and old boards are the traps; the under-stage is where the Taken appears.
- **Its theme is the twist:** a theatre is where people put on another face. The one who turns
  "takes the role"; the hunt is the last act; 5:00 is the final curtain.
- **The cast already fits** (section 7), and the user's own sheet has Thanh singing "at the
  central opera house".

**Other well-known Saigon places considered**

| Place | For | Against |
|---|---|---|
| Chung cư 727 Trần Hưng Đạo: 13 floors, 530 rooms, built 1960, the city's best-known "haunted" block, now demolished | The strongest ghost reputation in the city; a night before demolition is a natural deadline | Rooms look alike, so the board is hard to read and costly to make varied; the cast's reasons to be there must be rewritten |
| Thuận Kiều Plaza: three towers built in the 1990s, long empty and the subject of urban legends | Famous silhouette and legend | It trades today as The Garden Mall under a private owner; attaching a horror story to a running business is the riskiest choice |

**The house keeps its real name and its real history** (user decision); only the characters and
the night's scenarios are invented. The real history the game may draw on:

| Year | What happened |
|---|---|
| 1898-1900 | Built by the architects Ernest Guichard, Félix Olivier and Eugène Ferret; inaugurated 1 January 1900 |
| 1944 | Damaged in an air raid |
| 1955 or 1956 | Becomes the seat of the Lower House |
| 1975-1976 | Returns to use as a theatre |
| 1998 | Restored for the city's 300th anniversary; façade sculpture put back |
| 2007 | Statues, roof and seats renewed |
| 2012 | National monument |

Sources are listed at the end. Dates the sources disagree on are given as they give them.

- **What is invented:** the seven guests, the patron who hires the house for one night, the
  curse, and each guest's secret. The game says so at its start, and no invented event is
  presented as the house's real history.
- **How the secrets meet the history:** a secret may stand *beside* a real date without
  changing it. Thanh's earlier life can end in the house on the night of the 1944 raid: the raid
  is real, the singer and her killer are not.
- Before selling a game that uses a public monument's name and likeness, check whether
  permission is needed (Claude is not a lawyer).

**What the real place changes in the story:** the house was never closed or for sale, so "the
owner" of the first sheets becomes the patron: a collector who paid for the house for one night.

**Words we use** (ours, not the board game's)

| Meaning | Our word |
|---|---|
| The seven characters | the guests |
| The one who turns | the Taken |
| Those who hunt it | the company |
| The betrayal at midnight | the Turn |
| A cursed keepsake that may mark its taker | an omen |
| The one who turns (at a failed curse roll or by chance at midnight) | the Taken |
| The room where the Taken appears | the under-stage |

## 4. The night

- **Clock:** 19:00 to 5:00, 20 rounds of 30 minutes. In a round every guest takes one turn: about 140 turns a night (confirmed).
- **Order in a round:** every guest rolls a d20 at 19:00; highest goes first all night, ties to the higher Speed. After the Turn the Taken always acts last.
- **Evening** (rounds 1-10, 19:00 to 23:30): explore, fight, level up, recruit, watch.
- **The Turn** (midnight): the Taken takes shape (section 13).
- **The hunt** (rounds 11-20, 0:00 to 4:30): the company against the Taken. At 5:00 the night ends.

```
19:00                         0:00                          5:00
  |------- evening, 10 rounds ---|------- the hunt, 10 rounds ---|
  omens are found and taken; a failed curse roll jumps to midnight   the Taken appears
```

## 5. A turn

1. **Move** up to Speed rooms through opened rooms and through doorways not yet opened.
2. **Opening a room** reveals it. An empty room does not stop you: keep moving. A room with
   something in it (item, event, trap, omen) stops your movement and you resolve it.
3. **Free at any point, and you may still move afterwards:** use an item; leave an item on the
   floor; pick one up.
4. **One of these ends the turn:** resolving a room; trying to persuade a guest; attacking a
   guest or a monster; being caught by a monster on your path.

**What you see: light.** Every guest lights the rooms they could reach within 4 steps through
open doors; a lantern makes it 5. Everything else is dark: you do not see who is there or what
they do. A room opened by anyone appears on your map at once (its shape and kind, not who is in
it). The player sees with the light of their own guest and their allies. So guests near each
other are aware of each other's actions, and a bot's turn is shown with its decision said aloud
when it happens in your light; outside it you see only a new room appear. The Taken can use the dark
to slip away.

## 6. The board

- One floor, a 5x5 grid. About 20 rooms are placed each night from a set of 25.
- All guests start in the **foyer** (bottom centre). The **under-stage** is always placed, far from the foyer, and is a plain room until midnight.
- Unopened rooms are hidden, but a doorway shows there is a room beyond it.
- The generator never seals the board: there is always an unopened doorway until all rooms are placed.

| Room kind | About | What happens |
|---|---|---|
| Plain | 7 | Nothing. Does not stop movement. |
| Item | 4 | The first guest in takes one item. |
| Event | 4 | The first guest in resolves one event (a check, section 8). |
| Trap | 3 | The first guest in rolls to save themselves or takes damage. |
| Transport | 2 (one pair) | Trapdoor and catwalk: enter one, leave from the other. Costs 1 movement. |
| Bonus | 3 | End your turn here: +1 to one stat. Once per guest per night. |
| Omen | 5 | The first guest in finds an omen and decides whether to take it (section 12). |
| Foyer, under-stage | 2 | Start; where the Taken appears. |

Bonus rooms: rehearsal room (+1 Might), chapel of the stage ancestors (+1 Sanity), rigging gallery (+1 Speed).

## 7. The guests

Seven, in `cast.md`. The player picks one; six are bots. Each has HP, Might, Sanity,
Speed, a physical or magical weapon, a play style, one bond, and a Taken form with one power.
Talent, quirk and the rest of the sheet are backstory for the model and the look: they add no
rules.

| Stat | On the board | In battle |
|---|---|---|
| HP | 0 = down (section 11) | 0 = defeated |
| Might | Physical checks | Physical damage and physical defense |
| Sanity | Mental checks, persuading | Magical damage and magical defense |
| Speed | Rooms moved per turn; breaks ties in turn order | (whoever starts the fight attacks first) |

**Levels.** Beating monsters gives XP. Each level: +4 max HP and +1 to Might or Sanity (the
player chooses; bots follow their weapon type). A level does not heal. Level cap 5. Levels come
at 1, 3, 6 and 10 kills (`rules.json` `levels.xp`); in practice a guest gains a level or two a
night and level 5 is out of reach. That is intended (user, 2026-11-14): levels build the
character, they are not for min-maxing.

The cast document is written for the opera house: the patron's agent and his daughter, a singer
of the house, its caretaker, its old company doctor, the appraiser of the patron's costumes and
masks, the collector of the patron's debt, and a student who films old buildings at night.

## 8. Checks (dice)

- Roll a d20, add the stat the room or event names, meet the target: **easy 10, normal 13, hard 16.**
- Stats run 1-8, so a normal check succeeds 45% to 80% of the time.
- Used for traps, events and persuading. **Battles use no dice.**
- The roll is shown on screen: die, stat, target, result.
- No special rule for a 1 or a 20.

## 9. Items

A guest carries **four** items. The UI shows each with its picture, kind and a one-line effect,
on the board and in battle.

| Kind | Used | The first set of 12 |
|---|---|---|
| Healing | Board or battle, used up | Bandage (+6 HP), tonic (+12 HP), stage brandy (+4 HP to all allies in the room) |
| Fight boost | In battle, used up; gives a buff (below) | Whetstone (Strengthened), incense (Focused), padded coat (Guarded) |
| Worn | Always on while carried | Prop sword (+1 Might), opera fan (+1 Sanity), dancer's shoes (+1 Speed), brass amulet (+4 max HP), stage armour (-1 physical damage taken), lantern (light reaches 5 steps) |

- **Using, leaving and picking up items are free** and do not end movement.
- A room can hold any number of items on its floor. To take a fifth, leave one.
- **Trading is leaving:** put an item down and another guest, bot or ally, may pick it up.
- A guest's items drop in the room when they die.
- **Relics** (crucifix, rosary) are a rare thing an item room can give: they wake a downed guest (section 11) and do nothing else.

**Buffs and debuffs.** A small fixed set, each shown as an icon with the rounds it has left, so
effects like "halve the next hit" are visible:

| Name | Effect | Lasts |
|---|---|---|
| Guarded | The next hit taken is halved | until hit, or the battle ends |
| Strengthened | +2 Might | this battle |
| Focused | +2 Sanity | this battle |
| Weakened | -2 Might | 2 rounds |
| Shaken | -2 Sanity | 2 rounds |

Buffs come from items; debuffs from some monster and Taken attacks **[call]**. Nothing else
(poison, stun, sleep and the like) is in the first version.

## 10. Monsters and battles

**Monsters (three kinds) [call]**

| Monster | Hits with | Tough against | Role |
|---|---|---|---|
| Moth swarm | Might (weak) | nothing | Easy XP early |
| Stagehand (a shape in work clothes) | Might | physical | Punishes low-Might guests |
| Understudy (a figure wearing a guest's face) | Sanity | magical | Punishes low-Sanity guests |

- From 20:00 **[call]**, each round there is a chance one monster appears in a random opened room; it leaves after 3 rounds. At most 2 on the board.
- A guest may walk in to fight it. A guest whose path crosses it is forced to fight.
- Monsters grow with the hour so they stay worth fighting **[call]**.
- **After the Turn, monsters attack only the company and spare the Taken.** Traps also spare the Taken.

**Battles** are the engine's existing turn-based battles: no dice; Might and Sanity set damage
and defense. A guest's options are: attack with their weapon, use an item, guard **[call: guard]**.

- **Whoever starts the fight attacks first.**

- Who fights: the guest who started it, plus the player's allies in the same room. Bots fight alone.
- **Length: at most 3 rounds**, for every battle. Then the board resumes and everyone keeps their HP.
- Fleeing: after the first round, a side may leave early **[call]**.

## 11. Down before midnight, dead after

Revised by the user on 2026-11-14 (evening, after plan 1's first balance pass): there is no
hidden marked guest and no fighting between guests before midnight.

Before midnight nobody dies:

- **Down:** a guest whose HP reaches 0 (trap, event, monster) lies unconscious where they fell
  and takes no turns.
- **At the Turn** every downed guest stands up with 1 HP in the foyer; the one who turns rises
  as the Taken **at full HP**, wherever they were.
- **Reviving:** another guest in the same room can wake a downed guest with a relic: the
  **crucifix** (thập tự giá) or the **rosary** (chuỗi tràng hạt). Relics are rare: at most one
  of each in a night. Used up.
- **After midnight**, 0 HP is death.
- **Guests do not fight each other before midnight**; after it, only the Taken fights the
  company. Every battle lasts at most 3 rounds, then the board resumes and both sides keep their
  HP. A monster not beaten in 3 rounds stays with the HP it has left; XP is given only for the kill.

## 12. Omens and the curse

- **Omen rooms** (five) each hold one omen, drawn at random: a cursed keepsake of the house.
- The first guest in may **take it or leave it**. An omen is not an item and takes no slot. It
  grants great power for the night:

| Omen | Grants |
|---|---|
| The cracked mask | +4 Might |
| The scorched playbill | +4 Sanity |
| The single dancing shoe | +2 Speed |
| The prompter's bell | +12 max HP, healed |
| The stopped metronome | +2 Might and +2 Sanity |

- **Taking an omen means a curse roll** (user, 2026-11-14): a plain d20, the same odds for every
  guest, against a target that rises with every omen taken and with every hour of the night
  (`rules.json` `omens.targets` and `omens.perHour`). Early in the evening only a very unlucky
  roll fails.
- **A failed roll and the theatre shifts time** (user, 2026-11-14): that guest turns at once.
  Everyone skips the rest of the evening and wakes at midnight as the burning past takes the
  house; the hunt begins. Everyone sees who failed.
- **If nobody has failed by midnight,** chance picks one of the omen holders, all alike, the
  player included; if no omen was taken at all, one of all seven.
- **Only the Taken's secret is true tonight;** the model tells it at the Turn.
- **The Taken form has its power unconditionally**, whoever turned.

**The seven powers must be balanced against each other** (user requirement): the company wins
40% to 60% of hunts against every form (soak test, `night-soak`). Refusing every omen must not be
the winning plan.

## 13. The Turn (midnight)

1. The one who turns vanishes from where they are and the **Taken** appears in the under-stage, in its own form (own model, own voice: a distorted echo of the person).
2. Every other living or downed guest is placed in the foyer; the downed stand up with 1 HP.
3. The Taken's power and its secret are revealed to all.
4. **The Taken's strength is fixed** for each form: it does not scale with the company. A well-prepared company has it easier; that is the reward for the evening.
5. **Its minions appear** (user, 2026-11-14): a fixed set of the form's own minions
   (`minions.csv`) waits in the rooms farthest from the foyer. Ordinary monsters vanish and do
   not come back.

## 14. The hunt (0:00 to 5:00)

- **The company moves first each round, the Taken last.**
- **On the board** the Taken moves and may use its power once per turn (its seven powers are in
  the cast document: a line, an area, a flood, a pull, a sealed door, a swap, a reflection).
  Powers act on the board at once, like using an item.
- **Minions** fight on the Taken's side. **Destroying a minion takes its share of the Taken's
  HP at once** (`hpShare`), so the company has a reason to hunt the minions first.
- **Battle on contact:** when a member of the company enters the Taken's room, or it enters
  theirs, a battle starts with everyone of the company in that room (and any minion there). It
  lasts 3 rounds, then the board resumes. **The Taken's HP carries over** from battle to battle.
- **The company wins** when the Taken's HP reaches 0 with at least one of them alive: the doors open and the survivors leave.
- **The Taken wins** at 5:00, or when no member of the company is left: they are trapped in the theatre for good.
- **A player who is the Taken** wins and loses by the same two rules, from the other side.
- **If the player's guest dies** (after midnight), the player watches the bots play the night out and gets the company's ending.

## 15. Allies

- Only the player recruits. A group is at most 3: the player and two allies.
- **Winning a guest over by talking** (user, 2026-11-14): in the same room, the player chats with
  a guest, which uses the turn; a bot may also open a chat with the player at the start of its own
  turn. The guest asks one of their five questions (`questions.csv`), with two answers: one needs
  a physical roll (d20 + Might), the other a magical roll (d20 + Sanity), each with its own target;
  the guest's bond toward the player makes both easier or harder by 3. A passed roll is one
  approval, a failed one costs nothing; **three approvals**, on three separate turns, and the
  guest joins the player's group.
- **An ally is controlled by the player** on its own turn, and joins the player's battles when in the same room.
- A recruited guest who turns leaves the group: the player loses that ally.
- Bots never form groups.

## 16. The bots

Bots follow rules, not the language model. Each turn a bot scores its legal moves by its **play
style** (explorer, fighter, loner, protector, schemer), its HP, the hour, its bond with nearby
guests, and the minions and the Taken after midnight, and takes the best. Before the game applies it, the decision is said
aloud in that guest's voice.

Requirements:

- A bot's turn takes a few seconds: camera to the bot, the line, the move, the result.
- The Taken as a bot plays to win: it uses its power on the most hurt guest in reach and avoids the whole company at once **[call]**.

## 17. The built-in model

Required (user decision). It never decides anything; the rules do.

| It writes | From |
|---|---|
| The line a bot says for its decision | The decision, the guest's voice and personality |
| What a guest says when asked to join | The roll's result and the bond |
| What a guest says on taking an omen | The omen, the guest's personality |
| The Taken's lines | The form's distorted personality and the secret |
| Tonight's telling of the secret at the Turn | The cast sheet's plot |
| How the one who turned comes back as the Taken | The omen, the room, the secret |

Every line passes the existing guard (names, length, no invented facts) and falls back to a
written template if refused or late. Qwen3 4B on the graphics card gave about 0.8 s a line,
which fits one line per bot turn.

## 18. Crossed out

The random betrayal roll; strange conduct, clues and named rooms; talents as rules; tools; status
effects beyond the five buffs and debuffs; Taken weaknesses; a Taken that scales; dice in
battles; bot groups; voting on accusations; a second floor; barrier rooms and the moving
elevator; unlimited items, stealing and trading; companions; a Knowledge stat; the town's needs,
schedules and wages; progress between nights.

## 18a. Data, not code

All gameplay data is metadata (user requirement, 2026-11-14): every stat, cost, chance, target,
count and list lives in CSV or JSON files under `plugins/last-curtain/assets/night/` (guests,
bonds, powers, rooms, events and traps, items, buffs, monsters, omens, bot styles, and one
`rules.json` for the clock, checks, light, battle formula, levels and the balance band).
Balancing means editing those files and running the soak test again; code changes only when a
new kind of mechanic is added.

## 19. How it sits on the engine

**Reused:** turn-based battles, stats, XP, items and equipment, skills; the learned and
conventional battle AI; game UI screens; the built-in model with its guard and worker thread;
bonds and regard from the town; the seeded, copyable state with saves; the soak test.

**New:** the board (grid, room deck, generator, movement, line of sight); the night's rules
(clock, checks, down and dead, the Turn, the hunt, powers); the bots' brain and suspicion;
playing any guest and controlling allies; the board's camera and UI; a room art kit; seven
guests and seven Taken forms as models.

**Ways to build it**

| Way | What it is | Trade-off |
|---|---|---|
| **A. Rules first, headless (recommended)** | A new plugin holding the whole night as whole-number rules with no graphics, like the town was built. Bots play each other in tests; the soak test runs 1,000 nights for balance before any art exists. Then the 3D board and UI on top. | Nothing to look at for the first phase. Balance and bot quality are proven early, where the risk is. |
| B. Playable first | Build the 3D board, camera and one guest walking it, then add rules as they are needed. | Something on screen quickly. Rules get tangled with the display, bots come late, balance is found by hand. |
| C. Extend the town plugin | Add the board and the night to `living-town`. | Less new structure, but carries the town's unused half and makes both harder to read. |

A is recommended: the town's unused parts stay where they are, and what is shared (bonds, the
model, the storyteller's forks) is used from the new plugin.

**Phases under A [call]:** (1) board and movement, headless; (2) checks, items, monsters, down
and dead; (3) omens and the mark, the Turn, the hunt and the seven powers; (4) bots
and the soak test; (5) the 3D board, camera and UI with placeholder art; (6) the model's lines;
(7) art: room kit, guests, Taken forms.

## 20. Open questions

None from the user's side.

Still marked **[call]** in the text, for the test runs or a later review to settle: the XP
steps, guard as a battle option, where debuffs come from, how often relics appear, what each
omen grants and the curse targets, the win-rate band for balancing the seven powers, and the
build phases.

## 21. Decision log

| Date | Decision | By |
|---|---|---|
| 2026-11-14 | One night of tile exploration and betrayal replaces the living town as the example game | user |
| 2026-11-14 | The player's own guest can be the one who turns; the player is told | user |
| 2026-11-14 | Taken powers are special powers used on the board like an item | user |
| 2026-11-14 | The built-in model is required | user |
| 2026-11-14 | Only the chosen one's secret is true that night; a "strange conduct" field per guest | user |
| 2026-11-14 | Seven guests; Vietnam, present day; the chosen one is picked by chance | user |
| 2026-11-14 | Eliminating the chosen one before midnight saves the run | user |
| 2026-11-14 | The Taken has its own model and style, no longer the person | user |
| 2026-11-14 | Bots' turns are shown and their decisions said aloud | user |
| 2026-11-14 | Items: healing, fight boosts, worn equipment; four slots; no tools or status effects | user |
| 2026-11-14 | Rooms: item, event, named, transport, trap; omens cut | user |
| 2026-11-14 | Dice stay on the board (d20 + stat); battles use no dice | user |
| 2026-11-14 | No Taken weakness; anyone may attack anyone at any time | user |
| 2026-11-14 | The hunt: battle on contact, the Taken's HP carries over | user |
| 2026-11-14 | Only the player recruits; a dead player watches the night play out | user |
| 2026-11-14 | First version: one floor, about 20 rooms, 3 monsters | user |
| 2026-11-14 | Only a room with something in it stops movement | user |
| 2026-11-14 | 0 HP before midnight is "down": back at midnight as the Taken or with 1 HP | user |
| 2026-11-14 | After the Turn, monsters attack only the company | user |
| 2026-11-14 | Bonus rooms that reward a visit | user |
| 2026-11-14 | Our own terms; a setting other than a mansion | user |
| 2026-11-14 | 0 HP before midnight is always "down", never dead; a downed chosen one wins the night; others return at 1 HP | user |
| 2026-11-14 | Bringing down a guest must take time and weaken the attacker | user |
| 2026-11-14 | Rare relics (crucifix, rosary) wake a downed guest | user |
| 2026-11-14 | A chosen player must perform the strange acts or the Taken form loses its power | user |
| 2026-11-14 | The Taken's power is revealed at midnight | user |
| 2026-11-14 | An ally who is the chosen one leaves the player's group at midnight | user |
| 2026-11-14 | A night is about 140 turns | user |
| 2026-11-14 | The place should be a well-known location in Saigon | user |
| 2026-11-14 | Every battle lasts at most 3 rounds | user |
| 2026-11-14 | Bots react badly to a guest attacking another who has not turned | user |
| 2026-11-14 | Only a guest's attack on the chosen one wins the night; felled by the house, they return at midnight as the Taken at full HP, with a tale the model writes | user |
| 2026-11-14 | Any skipped act removes the Taken's power entirely | user |
| 2026-11-14 | The Saigon Opera House under its real name and real history; only characters and scenarios are invented | user |
| 2026-11-14 | Attacking a guest gives no XP; no other penalty for downing an innocent | user |
| 2026-11-14 | An attacked guest fights back or runs by temper; a bot in the room may join the defender | user |
| 2026-11-14 | Chosen bots never skip an act | user |
| 2026-11-14 | A secret may stand beside a real date (Thanh and the 1944 raid) | user |
| 2026-11-14 | Turn order by a d20 rolled at the start, ties to Speed; whoever starts a fight attacks first | user |
| 2026-11-14 | Level cap 5, about 15 turns of monster fights; a level does not heal | user |
| 2026-11-14 | A small set of visible buffs and debuffs; "act first" item removed | user |
| 2026-11-14 | Items: using, leaving and picking up are free; rooms hold any number; trading is leaving | user |
| 2026-11-14 | The under-stage is a plain room until midnight; bonus and plain rooms as drafted | user |
| 2026-11-14 | Light (first version, superseded below): each guest sees 2 rooms, more with a lantern; the rest is dark; opened rooms show on the map | user |
| 2026-11-14 | The Taken's strength is fixed | user |
| 2026-11-14 | Build the whole night as a simulation before graphics | user |
| 2026-11-14 | Talents are backstory only, no rules | user |
| 2026-11-14 | Strange conduct withdrawn in favour of an omen-like mechanic (supersedes the strange-conduct rows above) | user |
| 2026-11-14 | Light reaches 4 steps | user |
| 2026-11-14 | Omens are cursed keepsakes granting great power (+4) at the price of a scaled d20 roll | user |
| 2026-11-14 | Only guests whose light reaches the omen room see the taking and the roll; no one else knows of omens until the Turn | user |
| 2026-11-14 | The Taken has its power unconditionally; the seven powers must be balanced against each other, by simulation | user |
| 2026-11-14 | Design and cast approved; all gameplay data as JSON or CSV so balance needs no code change | user |
| 2026-11-14 | The first to fail is secretly marked; a seen failure sends them into hiding; if none fails, a random omen holder, the player included | user |
| 2026-11-14 | No hidden marked guest; a failed curse roll turns that guest at once and time jumps to midnight; the roll is a plain d20 (same odds for all) against a target that rises with omens taken and the hour | user |
| 2026-11-14 | With no failed roll by midnight, chance picks an omen holder (all alike), else anyone | user |
| 2026-11-14 | No fights between guests before midnight; the early win, bystanders joining and temper are cut | user |
| 2026-11-14 | Guests are won over by talking: five questions each, a physical or a magical answer, three approvals | user |
| 2026-11-14 | An event stays one roll on one stat; its outcomes may push the guest back a room | user |
| 2026-11-14 | Level 5 out of reach in a night is intended: levels build the character, not min-maxing | user |
| 2026-11-14 | After midnight the Taken's own minions (one kind per form, a fixed set at the Turn) replace ordinary monsters; destroying one takes a share of the Taken's HP | user |

## Sources (the house's history)

- https://www.vietnamtourism.com/vi/nha-hat-lon-sai-gon-lich-su-cac-buoi-dien-ve-va-cach-tham
- https://vinwonders.com/vi/wonderpedia/news/nha-hat-thanh-pho-tphcm/
- https://nhandan.vn/nha-hat-thanh-pho-post604942.html
- https://en.wikipedia.org/wiki/Ho_Chi_Minh_City_Opera_House
