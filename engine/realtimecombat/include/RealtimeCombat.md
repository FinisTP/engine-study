# Real-time combat (`Umbra::RealtimeCombat`)

Attack timing, hit detection and cooldowns only. Damage, costs, statuses and death come from the
shared rules (`engine/rpg/include/Rpg.md`). Links `Umbra::Rpg` and nothing else: no turn
controller, world, physics or renderer (`Umbra.RealtimeOnlyDependencies`, roadmap T40).

## One action's life

```
StartAction ── Windup ──► commit ── Active ──► Recovery ──► Idle
  checks only    │       pay cost,   hits      can't act
  (nothing paid) │       cooldown    (once per
                 │       starts       target)
        hit while here: Interrupted, back to Idle, nothing paid, no cooldown
```

User decision 2026-11-07, **pay at commit**: the cost is paid when the wind-up ends. A fighter hit
(damage > 0) during its wind-up is interrupted for free. Once committed there is no refund: if the
attacker dies during Active or Recovery the cost stays spent (roadmap T08), and it is charged exactly
once. At commit the action is checked again (`CheckAction`); if it became illegal (e.g. the energy
was spent meanwhile) it is cancelled without cost.

Timing comes from the skill's `realtime` block (`windup`, `active`, `recovery`, `cooldown` in
seconds). Items use a fixed `itemTiming` (0.5 s wind-up, 0.05 s active, 0.3 s recovery) and affect
the user only.

## Hits

During Active, every update looks for living enemies that are

- within `range` meters (center to center, ground plane x/z), and
- inside the facing cone: the angle between the attacker's forward direction and the direction to
  the target is at most `arc / 2`. With unit vectors that is `dot(toTarget, forward) >= cos(arc / 2)`.
  `arc` 360 hits all around.

Forward for yaw `y` is `(-sin y, -cos y)`, the same convention as the player controller (yaw 0 faces -z).

Each activation keeps a set of fighters it already hit, so a target is hit **once per activation**
no matter how many updates fall inside the hit window (roadmap T06). An enemy who walks into the
swing while it is active is still hit (once). Self skills and items apply to the user once.

Fighters are processed in roster order each update, so two swings landing in the same update
resolve in a fixed order.

## Frame rate

Time left over when a phase ends carries into the next phase within the same update, so a 0.3 s
wind-up commits at 0.3 s whether the game runs at 30 or 144 FPS. Status ticks are per full second
(`rpg::TickStatusesSeconds`). The Active phase always gets at least one hit check even if one update
is longer than the phase.

## Movement belongs to the game

The controller never moves anyone. The caller (gameplay with physics, or a test) reports positions
and facing with `SetPlacement` before each `Update`. In P6 the player's capsule and enemy bodies feed
this; the headless fixtures move fighters in straight lines.

## Conventional enemy

`ChooseRealtimeAction` returns where to walk, where to face and possibly an action to start:
face the nearest living enemy; below 35% health drink a healing item; otherwise start the strongest
ready, affordable skill whose range (×0.9) reaches; walk closer while out of reach and idle. The
action still goes through `StartAction`, so it obeys every rule.

Statuses and cooldowns end with the encounter (user decision 2026-11-07); the cooldown table lives
in the encounter object and disappears with it.
