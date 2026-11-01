# World ownership and identity

The `World` owns every entity in one loaded scene. Nothing else creates or destroys entities.

## Two kinds of identity

| | Durable id | `Entity` handle |
|---|---|---|
| Type | `std::string`, e.g. `"crate_a"` | `{ slot{index, generation}, world }` |
| Lifetime | Saved in scene and save files | Valid only while the world and entity live |
| Chosen by | The author (or a tool) | The world |
| Unique within | One world | One world |

Files store only durable ids. On load, the world creates entities and new handles. Handles are **never** written to disk.

## Stale and foreign handles

`Entity.slot` is a generational handle (see `engine/core/include/SlotMap.md`):

- Destroying an entity frees its slot. A later entity reusing that slot gets a new generation, so the old handle stays dead.
- `Entity.world` is a unique per-world number. A handle from world A passed to world B is rejected even if index and generation happen to match.
- `Clear()` keeps slot generations, so handles taken before a reset can't revive.

## Storage model

One `Record` per entity holds that entity's data directly. Later subphases add optional fields (transform, renderable) to the record. This is deliberately simpler than an ECS: no archetypes, no queries, no systems. Revisit only if a measured scale or query problem appears (roadmap O01).

## Errors

API calls return an invalid `Entity`, `false` or `nullptr` on bad input (empty or duplicate id, stale or foreign handle). The world does not log; callers decide how to report.
