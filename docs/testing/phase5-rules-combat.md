# Phase 5 — Shared RPG rules and two independent combat modes: verification record

## Record

- Date: 2026-11-07
- Implemented in commits `issue #UMBRA-26` (P5.1) through `issue #UMBRA-33` (P5.8), on `main`
- Tester: Claude (remote session), automated Linux checks only
- State: **implemented; verified headlessly on Linux; Windows gate open.** Phase 5 is pure rules
  with no window, GPU or audio, so its Windows gate is the build and CTest only. It is not yet
  wired into the 3D game: that is P6.

User decisions used (2026-11-07): speed-ordered rounds with one action per fighter; real-time cost
paid at commit (end of wind-up), free interruption during wind-up, no refund after commit; statuses
and cooldowns end with the encounter (modes change only between encounters); a full inventory
rejects the pickup. W0 moved to after P5.

## 1. Automated checks run (Linux, cloud workspace)

| Configuration | Result |
|---|---|
| GCC 13.3, `-Wall -Wextra -Wpedantic` | Build OK, 0 new warnings, 24/24 CTest entries pass |
| Clang 18 | Build OK, 0 new warnings, 24/24 pass |
| GCC 13.3 + ASan/UBSan (LeakSanitizer on) | Build OK, 24/24 pass, no reports |

New CTest entries: `Umbra.Rpg`, `Umbra.TurnCombat`, `Umbra.RealtimeCombat`, `Umbra.Encounter`,
`Umbra.TurnOnlyDependencies`, `Umbra.RealtimeOnlyDependencies`. `Umbra.HeadlessDependencies`
also covers Rpg, TurnCombat, RealtimeCombat and Encounter.

## 2. Roadmap test cases

| Case | Evidence | Status |
|---|---|---|
| T06 repeated contact | Many updates inside one hit window hit once (`RealtimeEncounterTests`) | Passed |
| T07 target dies after selection | Turn: `TargetDead` rejection; real-time: dead targets skipped (`RulesTests`) | Passed |
| T08 interrupted after commit | Attacker killed after commit: cost spent once, no refund, no hit | Passed |
| T09 status across a mode change | Modes change only between encounters; `Finish` clears statuses (`EncounterTests`) | Passed by design + test |
| T10 several deaths in one update | Deaths after all effects, in target order, once each | Passed |
| T11 equipment changes a maximum | Clamp, never heal (`ContentTests`, `InventoryTests`) | Passed |
| T12 cyclic tree / repeated unlock | Loops rejected on load; repeat unlock costs nothing (`SkillTreeTests`) | Passed |
| T39 turn-only fixture | `UmbraTurnCombatTests` links no real-time, world, physics or renderer (`Umbra.TurnOnlyDependencies`) | Passed |
| T40 real-time-only fixture | Mirror check `Umbra.RealtimeOnlyDependencies` | Passed |
| T46 illegal action partially mutates | Every rejection leaves state untouched (`RulesTests`, `InventoryTests`, controllers) | Passed |
| T47 legal at snapshot, illegal at commit | Re-check at real-time commit; stale encounter ids rejected | Passed |
| T49 durable identity after reset | Characters hold ids and counts only; reset rebuilds from definitions | Passed |
| T55 training rules drift | `TurnEnvironment` replay through a plain `TurnEncounter` gives identical fighters | Passed |
| T60 step/turn mismatch | One accepted step = one turn; rejected steps use nothing | Passed |

Exit gate items: shared damage/cost/death rules exist once (`engine/rpg/src/Rules.cpp`); a status
used by a mode it has no clock for fails validation (`status.clock.unsupported`).

**Regression values:** turn AI-vs-AI (hero vs wolf + bandit) ends after 26 actions in round 12 with
the hero at 32 health and no potions left. These are integers with no randomness, so they must match
on Windows too. The real-time fixture uses float trigonometry, so its test allows a margin
(Linux: 398 frames, hero at 39 health).

## 3. Windows gate (to run, then fill in)

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug     # or: ctest --test-dir out\build\windows-msvc -C Debug
```

- [ ] MSVC Debug: all 24 CTest entries pass (the turn regression numbers must match exactly)
- [ ] `windows-clang` and `windows-msvc-asan`: all pass

## 4. Known gaps (deliberately left for later)

- Not connected to the 3D game: no enemies in the scenes, no combat HUD, no encounter triggers.
  P6 wires `Umbra::Encounter` to gameplay (player capsule positions feed `SetPlacement`).
- No experience/levels or rewards (P6), no respec, no per-item instances, no randomness (crits, misses).
- No mid-fight mode switch (excluded by the user decision; roadmap 4.3 keeps it possible later).
- `TurnEnvironment` has no observation encoding, rewards or seeds: those are the P8A contract.
- Real-time hits use center distance and a cone, not physics shapes.
