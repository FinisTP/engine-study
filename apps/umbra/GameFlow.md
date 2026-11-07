# GameFlow: the reference RPG in the running game

`GameFlow` connects `umbra::game::GameSession` (the headless rules: story, fights, saves) to the
3D world in `SceneRuntime` and to the keyboard. It adds no rules. Everything it decides is
about input, placement and the screen.

## Phases

| Phase     | Player moves?            | Keys                                         | Screen                         |
|-----------|--------------------------|----------------------------------------------|--------------------------------|
| Exploring | yes                      | E use, Tab switch combat mode                | status lines, quest            |
| Talking   | no                       | W/S + Enter answer, Esc walk away            | dialogue window                |
| Fighting (turn) | no (fight in place) | W/S + Enter pick an action, F flee          | action menu, enemy HP, log     |
| Fighting (real time) | yes             | 1-4 skills, Q potion, F flee                 | key help, enemy HP, log        |
| Dead      | no                       | W/S + Enter                                  | Load save / Load autosave / New game |

The combat mode can only be switched while exploring (user decision, 2026-11-07).

## Enemies in the world

- Every `spawner` entity puts `count` roamers into the area when it loads, each a random actor
  from its `pool`. Roamers wander near the spawner; `chase` roamers run at a player they can see
  (`engine/gameplay/include/Roamer.md`).
- Touching a roamer (capsules less than 1 m apart) starts a fight with that enemy plus 0 to
  `group - 1` random companions from the same pool. Companions appear next to it, on the side
  away from the player. Nobody is teleported: the fight happens where it started.
- Defeated enemies are removed from the world and stay gone until the area is loaded again.
- Fleeing always works. Companions vanish; the touched enemy stays and cannot start a new
  fight for 3 seconds, so the player can get away.
- If the world is replaced mid-fight (restart, hot reload, checkpoint), the fight is abandoned
  like a flee.

## Turn fights

On the hero's turn the menu lists `TurnEncounter::LegalActions()` ("Wait" last) plus Flee.
Enemy turns wait 0.7 s, then `ChooseTurnAction` decides, through the same `Submit` as the
player. Everything goes through `EncounterCoordinator::Submit` with a fresh decision id.

## Real-time fights

Each frame:

1. Placement: the player's feet and facing, and each enemy roamer's feet and facing, go into
   `RealtimeEncounter::SetPlacement`. Movement belongs to physics; the fight only reads it.
2. Player keys start actions: 1-4 are the hero's unlocked real-time skills in alphabetical
   order, Q uses the first usable item in the bag.
3. Each living enemy asks `ChooseRealtimeAction`; its `move` becomes the roamer's velocity
   order (2.5 m/s), its `act` is submitted.
4. `Update(seconds)` advances wind-ups, hits and cooldowns.
5. If every living enemy is more than 12 m away, the player has fled.

## Saves

- Save stones (`interactable.save: true`) write the manual save (`manual.sav`, previous one kept
  as `.bak`).
- Walking through a portal writes `autosave.sav` at the arrival point.
- Saves are only made while exploring (`GameSession::MakeSave`).
- Saves live in the per-user folder from `Platform::PreferencesFolder()` + `saves/` (falls back
  to the build's asset cache).
- Loading reads and checks the whole file first (falls back to the backup), then loads the area,
  and only then replaces the RPG state. A failure at any step leaves the game as it was.
