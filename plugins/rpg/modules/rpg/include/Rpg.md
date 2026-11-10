# Shared RPG rules (`Umbra::Rpg`)

One rule implementation used by both combat modes, the player, conventional enemies and (later)
learned policies. No rendering, physics, window or randomness: the same inputs always give the
same results, which keeps tests exact and makes training results trustworthy.

```
RpgContent  immutable definitions from assets/rpg/*.rpg.json (statuses, skills, items, actors)
Character   changing state of one placed actor (health, energy, inventory, skills, statuses)
Inventory   add/remove/equip transactions (all-or-nothing)
SkillTree   unlocking skills with points
Rules       ActionIntent -> validate -> commit -> events (shared by both combat controllers)
```

The turn controller (`Umbra::TurnCombat`) and the real-time controller (`Umbra::RealtimeCombat`)
are separate libraries on top of this; neither links the other.

## Content file

```json
{ "format": "umbra.rpg", "version": 1,
  "statuses": [ { "id": "poison", "turns": 3, "seconds": 6, "damagePerTick": 3 } ],
  "skills":   [ { "id": "strike", "modes": ["turn","realtime"], "effects": [ {"kind":"damage","power":8} ],
                  "realtime": { "windup": 0.3, "active": 0.2, "recovery": 0.3, "cooldown": 0.4, "range": 1.8, "arc": 100 } } ],
  "items":    [ { "id": "rusty_sword", "slot": "weapon", "modifiers": [ {"stat":"attack","add":4} ] } ],
  "actors":   [ { "id": "wolf", "stats": { "maxHealth": 45, "attack": 8, "speed": 12 }, "skills": ["bite"] } ] }
```

| Section | Fields |
|---|---|
| status | `id`, `name`, `turns` (1..100) and/or `seconds` (0.1..600), `damagePerTick` (negative heals), `modifiers` |
| skill | `id`, `name`, `cost` (energy), `target` (`enemy` default, `self`), `effects`, `modes` (`turn`, `realtime`), `realtime` timing (required for real-time), `points`, `requires` (skill ids) |
| item | `id`, `name`, `slot` (`weapon`, `armor`, none), `stack` (equipment: 1), `modifiers` (while equipped), `use` (effects on self, consumes one) |
| actor | `id`, `name`, `stats` (`maxHealth` required, `maxEnergy`, `attack`, `defense`, `speed`), `skills` (unlocked at start), `equipment`, `inventory` [`{item,count}`], `inventorySlots`, `skillPoints`, `xp`, `growth`; look in the world (never used by rules): `model` (asset path), `animation` (graph for a rigged model, `engine/scene/include/AnimGraph.md`), `size` (half-height in meters, default 0.8), `color` ([r,g,b,a] tint); `policy` (learned turn-combat AI file, `game/policy/include/Policy.md`) |
| effect | `kind`: `damage`, `heal`, `energy`, `status`; `power`; `status` id |

An actor is an **archetype**: placing it twice gives two characters (`wolf_1`, `wolf_2`) with
separate state (roadmap O06 default).

Loading is all-or-nothing with structured diagnostics (`skill.mode`, `status.clock`,
`status.clock.unsupported`, `ref.skill`, `id.duplicate`, `field.range`, ...). Unknown fields only warn.

### Two clocks, never converted

A status lasts `turns` in turn combat and `seconds` in real-time combat. These are separate fields
with separate types. If a skill usable in real-time applies a status that has no `seconds`, the
content is **rejected** (`status.clock.unsupported`) instead of guessing a conversion. Items can be
used in both modes, so statuses they apply need both clocks.

User decision (2026-11-07): statuses and cooldowns **end when an encounter ends**, and the game
only changes combat mode between encounters, so a status never has to cross from one clock to the other.

## Derived stats

For each stat, over every equipped item and active status:

```
value = (base + sum of add) * (100 + sum of percent) / 100     (integer, rounded toward zero)
```

then at least 0 (max health at least 1). Sums don't depend on order, so equipping A then B gives
exactly the same numbers as B then A, and recomputing never drifts. Example: hero defense 5,
leather armor +3, guarded +100% → (5 + 3) × 200 / 100 = 16.

Derived stats are always recomputed from the sources, never stored, so a bonus cannot be applied
twice.

### Changing a maximum (roadmap T11)

When max health or max energy changes (equip, unequip, status), the current value is **clamped**
into 0..max and otherwise kept. Raising the maximum never heals; lowering it cuts the excess.

## Inventory and equipment

The inventory is a list of stacks, one per slot (`inventorySlots`). Items are identified by
definition id and count only; there are no per-item instance ids yet because nothing needs them
(no durability or random rolls), which also makes saving trivial.

| Operation | Rule |
|---|---|
| `AddItem` | Top up existing stacks to `stack`, then use free slots. If it doesn't all fit, **nothing** is added (user decision 2026-11-07: a full inventory rejects the pickup; the item stays in the world or reward) |
| `RemoveItem` | Takes from the last stacks first; fails without change if there are fewer than asked |
| `Equip` | Removes one item from the inventory into its slot. The old item goes back into the freed place, so equipping never overflows |
| `Unequip` | Fails without change if the inventory is full |

Every operation changes a copy and only replaces the character on success, so a failure can
never leave half a transaction behind (roadmap T46).

## Skill tree

A skill's `requires` list names skills that must be unlocked first, and `points` is what unlocking
costs. `Unlock` checks, in order: known skill, not already unlocked, every prerequisite unlocked,
enough points. Only when all pass are the points spent and the skill added, together, so a
failed or repeated unlock never costs anything (roadmap T12).

The loader rejects requirement loops (`skill.requires.cycle`) with a depth-first search: each skill
is marked "on the current path" while its requirements are visited and "done" afterwards. Meeting a
skill that is still on the path means the path came back to itself. Diamonds (two skills sharing a
prerequisite) are fine because a "done" skill is not visited again.

## Actions (the shared resolver)

```
ActionIntent {id, actor, skill | item, target}
      |  CheckAction   (never changes anything)
      v
   PayCost         energy for a skill, one item for an item
      |
   ApplyEffects    effects on each target in order, then deaths in the same order
      v
   Events          CostPaid, Damage, Heal, StatusApplied, Death, ... (for UI, audio, quests, tests)
```

Both controllers call these same functions; they only differ in **when**. The turn controller
pays and applies at once. The real-time controller pays when the wind-up ends ("commit") and
applies when the hit window finds targets. Damage, costs, statuses and deaths therefore have one
implementation (roadmap T55).

`CheckAction` checks, in order: actor exists and is alive; exactly one of skill/item; the skill is
known, unlocked, allowed in this mode, and affordable (or the item is owned and usable); in turn
combat, the target is a living member of another team. A rejected action has no effect at all
(roadmap T46). A target that died after the choice was made is rejected (`TargetDead`, T07); the
caller picks again.

Each decision has an `id`; controllers keep an `IntentLog` and reject an id they have already
handled (`DuplicateIntent`), so a decision delivered twice applies once.

### Formulas

| Effect | Rule |
|---|---|
| damage | `max(1, power + attacker attack - target defense)`, then cut to the target's remaining health |
| heal / energy | `power`, cut so the value doesn't pass its maximum |
| status | Added with the duration of the current mode (`turns` or `seconds`). Re-applying the same status **refreshes** the duration; it never stacks |
| status tick | `damagePerTick` ignores defense (negative heals). Turn combat: one tick at the start of the affected actor's turn, then one turn is used up. Real-time: one tick per full second active |

There is no randomness: same state + same actions = same result.

### Death

After all effects of one action, every target that was alive and now has 0 health gets one
`Death` event, in target order (T10), and loses its statuses. Effects skip dead targets, so
hitting a corpse does nothing and a death is never reported twice. `WinningTeam` returns the only
team with someone alive (or -1 while the fight goes on).

## Progression (XP and levels, user decision 2026-11-07)

```
XpForLevel(L) = 25 * L * (L - 1)        level 1: 0, 2: 50, 3: 150, 4: 300, ... up to level 20
stat at level L = base + growth * (L - 1)   (actor "growth" block), then modifiers as above
```

Each level gained gives 1 skill point and restores health and energy to the new maximum (a level up
is a deliberate reward; the "never heal on max change" rule is about equipment and statuses).

`AwardVictory` gives the winner the sum of the defeated actors' `xp` and **everything they carried**
(their inventory is the loot, so the bandit leader drops the Sun Relic he holds). Items that don't fit
go to `Character::pending`, which stands for loot left on the ground: nothing is lost, and
`ClaimPending` moves them in once there is room. This keeps the full-inventory decision (a pickup that
doesn't fit is rejected) without ever destroying a quest item.
