# Game session (`Umbra::Game`)

One object with the player's whole RPG state (hero, quests, combat mode) and the rules that connect
story, combat and saves. It has no world or renderer: the app feeds it input and world facts
("the player touched a wolf at this spot"), and the tests drive it the same way.

```
Exploring ──Talk──► Talking ──last line / walk away──► Exploring
Exploring ──StartFight──► Fighting ──won / Flee──► Exploring
                                  └──lost──► Dead ──ApplySave (load save / autosave)──► Exploring
```

| Rule | Where it comes from |
|---|---|
| Mode (`Turn` default, `Realtime`) changes only while exploring | user decision 2026-11-07 |
| A fight = the touched enemy + 0..group-1 random companions from the area pool (`PickGroup`) | user decision 2026-11-07 |
| Fleeing always works: the fight ends, no experience, no loot | user decision 2026-11-07 |
| Victory: `AwardVictory` (xp, the enemies' items, pending if no room), then one `Defeated` quest event per enemy and an `ItemsChanged` event | P6.2 |
| Defeat (`Winner() != hero team`): a `PlayerDied` quest event (fails `failOn: death` quests), phase `Dead` | user decision 2026-11-07 |
| Saves only while exploring (`MakeSave` returns nothing otherwise) | roadmap 4.5: safe boundary |
| `ApplySave` replaces hero, quests and mode and drops any fight or conversation | P6.3 |

The fight itself runs in `Combat()` (the P5 coordinator): the app reads whose turn it is or places
fighters each frame, and submits decisions with `NextDecisionId()` for the current `FightId()`.
`CheckFightEnd()` is called after every update and returns the result exactly once.

The only randomness is `PickGroup` (and the roamers' wandering, owned by the world): a
`std::mt19937` seeded by the caller. Combat rules stay deterministic.

Reloading after a win restores the pre-fight save, so the same fight can be won again, but its
reward is paid once per playthrough of that save: tests check that a reload-and-retry gives exactly
the same experience and loot, never double.
