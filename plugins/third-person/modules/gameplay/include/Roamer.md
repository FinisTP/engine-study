# Roaming enemies

User decision 2026-11-07: dungeon enemies run around; some track the player on sight; touching one
starts a fight (in place) with it and up to two more from the area's pool.

## Spawners (scene field)

```json
"spawner": { "pool": ["wolf", "bandit"], "count": 3, "radius": 4, "chase": true, "group": 3 }
```

When an area loads, each spawner entity places `count` roamers, each a random actor from `pool`, at
random points within `radius` of the entity. `group` (1–3) is the most enemies one fight can have.
A pool of one actor with `group: 1` makes a fixed boss (the bandit leader). Defeated roamers are gone
until the area is entered again (user decision), so a fresh set appears on every visit.

## Movement

Each roamer is a physics character (like the player, smaller), so walls and crates stop it.

- **Wander:** pick a random point in the home circle (`angle` uniform, `distance = sqrt(u) * radius`;
  the square root spreads points evenly over the disc instead of bunching them in the middle), walk
  there at 1.5 m/s, pick again after 2–5 s or on arrival.
- **Chase** (`chase: true` only): starts when the player is within 8 m **and** a ray from the
  roamer's eye to the player's eye hits nothing (walls block sight; the player is not a body, so it
  never blocks its own ray). Runs at 3.2 m/s, slower than the player's sprint (6.5 m/s), so the
  player can always outrun it. Gives up beyond 12 m and wanders again.

Random numbers come from one `std::mt19937` owned by the area, seeded at load: world variety, never
inside the combat rules, and repeatable in tests with a fixed seed.

## Contact

The game starts a fight when a roamer's feet come within 1 m of the player's (ground distance) and
the session is exploring. After fleeing, the touched roamer waits 3 s before it can start another
fight, so the player has time to get away.
