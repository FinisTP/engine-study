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
| actor | `id`, `name`, `stats` (`maxHealth` required, `maxEnergy`, `attack`, `defense`, `speed`), `skills` (unlocked at start), `equipment`, `inventory` [`{item,count}`], `inventorySlots`, `skillPoints` |
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
