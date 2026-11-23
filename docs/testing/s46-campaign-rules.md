# The Last Curtain, campaign plan 2: the fight rules — verification record

## Record

- Date: 2026-11-23.
- Commits: `issue #UMBRA-334` to `#UMBRA-339` (rules), `#UMBRA-340` (plans 3–7 and this record).
  Plan: `docs/design/plans/2026-11-23-campaign-plan-2-rules.md`; spec: `docs/design/2026-11-23-campaign-slice-1.md` §7.1–7.3.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader (Subzero) under `xvfb-run` for the picture.
- State: **implemented/unverified** on Windows.

## 1. What changed (`modules/match/include/Match.md`)

- **Modes** in `rules.json`: `faction` (the old match, unchanged in play) and `campaign`. A mode turns
  rules on and off: perform, respawn, biding, falls, the gold knot, the rite, the win (sides down, or
  the bosses down), the round limit.
- **One order** (§7.1): characters and monsters in one Speed order, sorted again each round (ties fixed
  by the match's seed); a monster acts at its own place; the order strip shows monsters by name.
- **Falls** (campaign): at 0 HP a unit kneels on its square and keeps it; it cannot act; a revive spell
  or item (beside the user) stands it up at a share of its HP; a remove ability takes it off the board.
- **Biding and AP** (campaign): Bide stores one AP (up to the cap) instead of acting; Extra spends AP for
  another action in the same turn; abilities may cost AP; Give AP passes it on.
- **Freeing** (campaign): a strung recruit must be dropped, then Cut beside it (the gold knot); freed,
  it leaves the board. The bosses' win needs every recruit freed first; a boss falling while a recruit
  is strung loses ("Trapped"). The player falling loses.
- **The rite**: offered once a fight (after it is unlocked) when the party is about to lose; it stands
  the fallen up.
- Save version 4. Words for the new refusals and log lines in `match-words.json`.

## 2. Checks (Linux)

- `Umbra.Match`: 103 test cases (rulings in the ledger: re-sort every round, spawns act from the next
  round, Squares is geometry only, a revive item targets a fallen ally beside the user, a fight with no
  boss is never won by bosses).
- `Umbra.Match.Balance` (the faction soak, 400 matches): side 0 wins **49.1%** (band 45–55), first mover
  50.4%, comebacks **26.5%** (needs ≥ 25; was 29.9% before the one order), 1 draw, 0 faults. No retune.
- Picture: `captures/2026-11-23-one-order-match.png` (the order strip with monsters between characters).
- Full run on 2026-11-23: `cmake --build build-gcc`, then `ctest`: **59/59 passed** (both soaks
  included; the run was cut after test 36 and tests 37–59 were run again on the same build).

**Not run:** Clang and ASan builds this time (GCC only); MSVC.

## 3. Limits

- No campaign fight exists yet to play: the campaign mode is reached only by tests until plan 4's
  stage files. The faction match is the only thing a player can see change (the one order).
- The round banner shows `TurnsLeft`, which is −1 in a campaign fight; the campaign banner is plan 5/7.
- Bots do not bide, cut or use the rite yet (plan 4 adds the enemy side's brain).

## 4. Open: the user's Windows check

1. Build and run `ctest --preset windows-msvc-debug`: `Umbra.Match` and `Umbra.Match.Balance` pass
   (the soak takes long).
2. Play a faction match in `LastCurtain`: monsters now take their turn at their place in the order
   strip, between the characters, by Speed. Does the order read clearly? Is the match still fair?

Result: **not run**.
