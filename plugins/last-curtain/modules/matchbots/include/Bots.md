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
