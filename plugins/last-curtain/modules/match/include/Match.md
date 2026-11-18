# The faction match, as built

The rules of `docs/design/2026-11-19-factions.md`, headless: `MatchDef` is the data, `MatchState`
one copyable value, and only `Apply` changes it. Bots and the app send the same `Action`s.

## Data: which file owns which number (`assets/match/`)

| File | Holds |
|---|---|
| `options.csv` | `id,value`: rounds (the countdown), side (characters per side), autoEndTurn (0/1), bots (easy, medium, hard) |
| `rules.json` | board (size, lanes, one unit per square, den and event distances, counts per half), stage (rounds open), respawn (turns away), battle (rounds, minimum damage), levels (XP steps, HP per level, XP for a kill), great monster (wake round), carry, bot noise per level, the soak's balance bands |
| `characters.csv` | id, name, role, the five stats (hp, attack, defence, speed, move), sprite, tint |
| `abilities.csv` | id, name, use (battle, board, passive), class (strike, shield, heal, hit, firststrike), hpCost, power, dice (`2d6` or none), range, animation (a `motions.json` name) |
| `jobs.csv` | role, level, ability, choice (a second ability to pick between, or none); level 1 = starting abilities |
| `monsters.csv` | id, name, tier (weak, strong, great, guardian), behaviour (stay, flee, stage, guard), stats, essence, xp, respawnRounds, abilities (`a;b`, the first is its strike), sprite, tint |
| `items.csv` | id, name, text, sprite, class (heal, xp), amount |
| `events.csv` | id, name, text, two choices: label, chance (percent), success, failure (`none`, `hp:-3`, `essence:2`, `xp:2`, `item:<id>`) |

The loader refuses a role without a free (0 HP) level-1 Strike, so a fighter at 1 HP can always act.
