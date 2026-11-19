# The bots (`MatchBots.cpp`)

Bots send the same `Action`s as the player, one per call to `ChooseAction`, for `Current(s)`.

## Roles (each call, per faction)

- **Guard**: the character nearest home, while an enemy character is within `board.strongMin`
  steps of the hub and the guardian still stands.
- **Defend**: the character nearest the stage (not the guard, carrying nothing), while an enemy
  carries essence.
- **Carry**: everyone else. Hunt monsters; once carrying, go to the stage and Perform.

## Choosing

1. A level choice: the first ability, sometimes the second.
2. In a battle: each affordable battle ability is scored (average damage, +20 if it would down the
   foe; a shield is worth its power, more when the foe could down you; a heal only below half HP),
   minus half its HP cost.
3. On the board: every square reachable this turn (asking `Check` for each step) is scored as
   **place + the best action there**, and the bot steps toward the best square, then acts, then
   ends its turn.
   - Place: below a third of max HP, home; carrying, the stage (closer is better, more so the more
     it carries); guard, its guardian's square; defend, the nearest enemy carrier or the stage;
     carry, the nearest neutral monster.
   - Actions: Perform (essence x 10); Attack (if it can win in `battle.rounds` rounds: 12 for a
     character plus its essence x 4, or a monster's essence x 3 plus its XP; minus 10 plus its own
     essence x 4 if it could lose); a board Hit that downs (as an attack) or chips (half the
     damage); a Heal on someone below 60% HP; a heal item below half HP; pick up (essence x 2, an
     item 3); an event 1.5. Each less a share of its HP cost.

## Levels

Noise: each value moves by up to `rules.json` `bots.noise[level]` percent (easy 60, medium 25,
hard 0) of its size plus 5, from a number fixed for the turn (the state's random state, the round,
the unit and the square), so a bot does not change its mind between steps.

## Weights

`performPerEssence` 10, `stealPerEssence` 4, `killCharacter` 12, `killMonsterPerEssence` 3,
`stagePull` 2.5, `huntPull` 1.2, `homePull` 3, `guardPull` 2, `defendPull` 2. They are bot
taste, not game rules, so they live in the code; the soak tunes the game's data, not these.

## The soak (`MatchSoak.cpp`, `UmbraTool match-soak`)

`SoakMatches` plays matches on seeds `firstSeed ...`, both sides picked by `RiggedPick` and played by
bots at the given levels, and counts: wins and draws, wins of the side that moves first, essence
gained by source (from the log of downs: monsters, kills of characters, guardians), essence
performed, comebacks (the side behind when two thirds of the rounds have passed wins anyway, out of
the matches that had a leader then), guardians fallen, and the last round the lead changed hands.
`Holds` checks `rules.json` `balance`: no faults; side 0 wins `sideMin`-`sideMax` percent of decided
matches; the first mover wins at most `firstMoverMax` points over half; no source above `sourceMax`
percent; comebacks at least `comebackMin` percent.

`UmbraTool match-soak [--data <folder>] [--runs 200] [--seed 1] [--levels medium,medium]`;
`UmbraTool match-check` loads and checks the data. The CTest `Umbra.Match.Balance` runs 400 matches.

### First reading (2026-11-19, the shipped data, 200 matches, medium vs medium; 400 in brackets where they differ)

| Band | Result |
|---|---|
| side 0 wins (45-55%) | 48.7% |
| first mover (at most 55%) | 51.8% |
| monsters / kills / guardians (none over 60%) | 65.0% / 35.0% / 0% — **fails** |
| comebacks (at least 25%) | 25.4% (23.4% on the CTest's 400 matches — **fails** there) |
| guardians fallen | 0 |
| lead last changed | round 14.7 of 30 |

**Resolved 2026-11-19 (the user: a guardian may fall in one battle).** The Usher became 20 HP, 6
Attack, 2 Defence, worth 30 essence. On the CTest's 400 matches: side 0 wins 49.7%, the first mover
46.0%, monsters 58.1% / kills 36.5% / guardians 5.4%, comebacks 25.7% of 382 led matches,
guardians fell 93 times, the lead last changed on round 15.2. Every band holds (comebacks by a small
margin).

Before that, four data changes did not bring monsters under 60% (guardian 30 HP and Defence 4; moths 1
essence; characters with 80% HP; Attack +1): stolen essence first came from monsters, so monsters
lead unless guardians fall, and these bots never attack a guardian they cannot beat in one battle
(45 HP, 7 Attack, 5 Defence). Open for the user: change the band, make guardians winnable in one
battle, or teach the bots to wear a guardian down over several battles.
