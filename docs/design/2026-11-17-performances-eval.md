# The Last Curtain: the performances — evaluation

2026-11-17. An evaluation of `2026-11-17-performances.md`, using the game-trend evaluation method
(core loop, live comparables, gates, a 7-criteria score) adapted to a PC single-player game with no
store or ad plan. It also answers the user's questions: what is missing, what is irrelevant, what to
focus on, and what to verify.

## Verdict: GO, with two conditions

The goal finally gives every system a job (explore → props → performance → Applause). Two things
decide whether it is fun, and neither is designed yet: **the duel has no real decision**, and
**seven guests × four acts is up to 28 duels a night to sit through**. Fix both before building
anything else (see Focus).

## Core loop

Read the programme → roam the house for the next act's props, racing rivals and dodging the
killer → face the act's specter alone in a short duel → Applause → repeat four times → the most
Applause at dawn wins. You fail by arriving without props, being booed off, or dying (or being
robbed of half your Applause) just before an act.

## Scorecard

Monetization is left out: the game is not sold on a store plan yet. Six criteria, out of 30
(the method's GO line, 24/35, is about 21/30).

| Criterion | Score | Why |
|---|---|---|
| Simplicity | 3 | Three new ideas (Applause, script, duel) are easy, but they sit on ten kept ones (killer, monsters, ghosts, Dressing room, persuasion, natures, events, traps, items, levels). |
| Content cost | 4 | Board, items, fights, blows, spotlight and soak are reused. New: duel view, review screen, 4 specters, ~8 scripts, 1 backdrop. |
| Shareability | 3 | A horror opera stage and an AI critic's review are good clips; single-player limits spread. |
| Competition gap | 4 | No solo board game has rivals racing for a boss's props. Armello and Mario Party are both criticised for slow AI turns, which the design can avoid. |
| Timing | 3 | Digital board games and short roguelite sessions are steady, not rising. Not trend-driven. |
| Creator fit | 5 | Built on a working engine, 212 tests, data files and a soak tool. Every new rule is data. |
| **Total** | **22/30** | No score of 1. Gates pass: no third-party IP (the Saigon Opera House is a real building; no *Betrayal* material), no backend, no multiplayer. |

**Distance to a playable MVP:** about 10–14 working sessions (Applause and acts 2–3, props 2, duel
view 3–4, review 1, bot rules 2, save v7 and soak 1–2).
**Reusable parts:** the whole board game, fights and blows, `Play::Spotlight`, menu groups, the
obituary, captions, the model's lines with fallbacks, `UmbraTool` soak.

## Comparables

| Game | Maker, launch | Standing | Same idea | Top complaints |
|---|---|---|---|---|
| Super Mario Party Jamboree | Nintendo, Oct 2024 | Metacritic 84 | Score race on a board, a round-end set piece | Slow pacing; long minigames swing the game; CPU and player turns drag |
| Armello | League of Geeks, 2015 | Metascore 75; Steam reviews mixed recently | Most Prestige wins; quests on a board vs AI | Long AI turns; too much dice luck; predictable AI; a poor tutorial |
| Inscryption | Daniel Mullins, Oct 2021 | 1M sold by Jan 2022, many GOTY awards | A captor stages games you must win to get out | The later acts left the cabin game; players wanted more of the cabin |
| Monster Hunter (series) | Capcom | Long-running | Prepare (items) → one set-piece fight → repeat | (from the hook research) |
| King of Tokyo | Iello, 2011 | Board-game staple | Points for holding the stage; dice | (from the hook research) |

## Gaps to exploit

1. **Downtime.** Every comparable is criticised for watching AI turns. Make bot duels a single
   result line by default, shown together on the review screen.
2. **Luck over skill** (Armello). Make the duel a choice made with information (the specter shows
   its next blow), not a dice-off.
3. **A captor who stages the games** (Inscryption's best-loved part). The specters and the critic
   are this game's Leshy: give them a voice in every act, not only after it.

## Sharpen: the one change that raises the score most

**Give the duel one real decision: the specter shows its next move.** Each exchange the specter
shows what it will do (a heavy blow, a light one, or a taunt). Hold is right against a heavy blow,
Strike against a light one, and each script prop has its own moment (the mask on the taunt, the
blade on the light blow) for a bonus. Three exchanges, three readable choices, no new concept.
Without this, the best play is always "use your props, then strike", and the duel is a cutscene.

## What is missing

- **The duel's decision** (above). Also: does the specter get harder each act? Is the finale
  different (worth double)?
- **What a guest without the props does.** They still perform for the base and damage; say so on
  the programme, so a missed prop is a setback, not the end of the act.
- **Props for later acts.** All props are laid at the start, but only the next act is shown. Can a
  guest pick up Act IV's crown in round 1, and does it count against the carry limit of 4? Decide:
  either show the whole programme from the start (more planning) or lay each act's props only when
  it is announced (simpler, fairer). I would lay them on announcement.
- **Standing.** The turn order shows Applause, but not what the next act is worth, so the player
  can't judge whether to fight a rival for a prop or keep exploring. Show "Act II: up to 30" on
  the programme.
- **How bots pick a target to kill.** Half the victim's Applause makes the leader everyone's target.
  That is a good catch-up rule if bots use it on purpose, and a lottery if they don't.
- **The first five minutes.** Round 1 must show the programme, point at the nearest prop, and reach
  the first act within ~10 minutes.
- **Session length.** 12 rounds × 7 guests + 4 acts. Nobody has measured how long a night takes.

## What may be irrelevant (candidates to cut or postpone)

| Item | Why | Suggestion |
|---|---|---|
| Board Applause (first visit, event, monster) | A second score source to tune, small by design | Leave it out of the MVP; add it only if the soak shows explorers don't win more |
| Powers on rare props | Brings back a concept the design just removed | Postpone; props do what their item does |
| Tie-break by level | Ties will be rare | Keep "alive at dawn", then stop |
| The "go after someone" request | Kills now have a score reason of their own | Keep (it's built), but don't extend it |
| Monsters | A second roaming threat next to the killer | Keep (they give XP for duel stats), check in the soak they still matter |
| Specter taunts by the model | Flavour | After the duel works; written lines first |

## What to focus on, in order

1. **The duel's decision**, tried in tests and the soak before the duel view is drawn.
2. **The prop race numbers:** two props per act, one or two copies, seven guests, carry 4.
3. **Downtime:** bot duels as one line each; the whole act's results on one review screen.
4. **Applause shown at every moment:** the turn order, the programme, the act's worth.

Everything else (review layout, model lines, art) comes after these four.

## What to verify

**By the soak (`UmbraTool`, 1,000 nights):**

- The duel has a decision: a bot that reads the specter's move beats one that always strikes by a
  clear margin (for example, 20%+ more Applause per act).
- Props matter: a guest with both props averages clearly more Applause in that act than one with
  none, but not so much that one missed prop decides the night.
- The race: in most acts 2–4 guests hold a script prop (not 0, not all 7).
- No runaway leader: the winner's lead is under one act's worth in most nights.
- Kills: at least one death a night, and kills don't decide more than about a third of winners.
- Natures: each wins at least 8% of nights.
- Ghosts: the average ghost reaches the Dressing room before the next act.

**By playtest (the user, on Windows):**

- Within 10 seconds of opening the programme, the player can say what to fetch and where to go.
- A night takes under 60 minutes; less than a third of it is spent watching bots.
- After the first act, the player understands why they scored what they did.
- After a night, the player wants another.

## Kill switch

Stop and rethink the goal if, after the duel change: the "always strike" bot is within 5% of the
best bot (the duel is still a cutscene); or one nature wins over 40% of nights; or a night takes
over 75 minutes with bot duels reduced to one line; or the playtest player can't name what to fetch
next.

## Assumptions and unknowns

- One night is the whole game; no campaign or unlocks. Replay comes from seeds, natures and scripts.
- Nobody has measured how long a round takes now, so the session length is a guess.
- The comparables' complaints come from reviews of games with human or CPU players; this game's bots
  can be faster, which is the gap, not a proof.
- Sales numbers are not a goal; the score leaves monetization out.

## Sources

- Armello critic and user reviews on Metacritic, and review summaries on Vaporlens, Gamebrain,
  GamingTrend and Softpedia.
- Super Mario Party Jamboree reviews: Stuff, Nintendo Wire, Yardbarker, Kotaku; Super Mario Party:
  GamingTrend, Digital Trends.
- Inscryption: Wikipedia, Game Informer, Hardcore Gamer, Vaporlens.
- `2026-11-17-hook-research.md` for Monster Hunter and King of Tokyo.
