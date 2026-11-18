# The Last Curtain: faction matches, Plan 1 (the 3v3 core, headless) — verification record

## Record

- Date: 2026-11-19 (overnight, the user away)
- Branch: `factions` (from `battle-royale`). Commits `issue #UMBRA-251` to `#UMBRA-260`.
- Tester: Claude (remote session), Linux cloud workspace.
- State: **implemented/unverified** for play: rules, bots and soak are headless; no screen yet (Plan 2).
  The balance CTest fails on two bands (below); left red for the user's decision.
- Design: `docs/design/2026-11-19-factions.md`; plan: `docs/design/plans/2026-11-19-factions-plan-1-core.md`.

## What changed

| Commit | Task | Change | Tests |
|---|---|---|---|
| `#UMBRA-251` | 1 | `assets/match/` data (options.csv, rules.json, characters, jobs, abilities, monsters, items, events) and `LoadMatch` | T274-T276 |
| `#UMBRA-252` | 2 | The mirrored lane board, `Fair`, stage rooms (`Board.md`) | T277-T279 |
| `#UMBRA-253` | 3 | `NewMatch`: sides, `RiggedPick`, speed order, guardians, dens, items, the stage | T280-T282 |
| `#UMBRA-254` | 4 | The turn: move, undo, act, end turn, auto end, the end and the tie rule; `Check` gives the reason | T283-T286 |
| `#UMBRA-255` | 5 | Three-round battles, damage, dice, first strike, the forecast, knock-outs, essence, respawn, the hub | T287-T291 |
| `#UMBRA-256` | 6 | Levels and choices, board abilities, items, events | T292-T295 |
| `#UMBRA-257` | 7 | The stage: Perform, one at a time, announced moves | T296-T298 |
| `#UMBRA-258` | 8 | Monster behaviours, guardians' blows, weak respawn, the great monster | T299-T302 |
| `#UMBRA-259` | 9 | Bots with roles at three levels (`Bots.md`) | T303-T305 |
| `#UMBRA-260` | 10 | The soak, `UmbraTool match-check` and `match-soak` | T306, T307 |

## 1. Automated checks

```bash
bash /home/claude/build.sh gcc                                   # everything, every CTest
CTEST_ARGS="-E Match.Balance" bash /home/claude/build.sh clang
CTEST_ARGS="-E Match.Balance" bash /home/claude/build.sh asan
UmbraTool match-soak --runs 200
```

| Toolchain | Result |
|---|---|
| GCC 13, Debug | 56/57; the one failure is `Umbra.Match.Balance` (below) |
| Clang, Debug (balance excluded) | 56/56 |
| GCC ASan+UBSan (balance excluded) | 56/56 |

## 2. Balance (400 matches, medium against medium)

Side 0 wins 52.0% (band 45-55); the side moving first wins 46.2% (at most 55); essence from monsters
65.4%, kills 34.6%, guardians 0% (none over 60% — **fails**); comebacks 23.4% of 384 led matches
(at least 25% — **fails**); 0 guardians fell; the lead last changed on round 14.7. Four data
variants did not fix it (`Bots.md` "First reading"). Decision for the user: the band, winnable
guardians, or bots that wear a guardian down.

## 3. Art (overnight, ComfyUI on the user's PC)

Bao, Khang (12), Mai and Lan (26) drawn in Huy's style and packed as `art/sprites/<id>/`
(seven faces, body and portrait); `characters.csv` now names each one's own sprite, untinted. The
recipe and its weak points are in `art/README.md`. Nothing shows them in the game yet (Plan 2).

## 4. Open: the Windows checks

Nothing to look at on Windows yet; the screens come with Plan 2. The user can run
`UmbraTool match-soak --runs 200` on Windows after the MSVC build to compare with Linux.
