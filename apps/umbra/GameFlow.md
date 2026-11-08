# GameFlow: the reference RPG in the running game

`GameFlow` connects `umbra::game::GameSession` (the headless rules: story, fights, saves) to the
3D world in `SceneRuntime`, to the keyboard and to the game's screens. It adds no rules.
Everything it decides is about input, placement and what the screens show.

## Phases

| Phase     | Player moves?            | Keys                                         | Screen (`ui/*.ui.json`)        |
|-----------|--------------------------|----------------------------------------------|--------------------------------|
| Exploring | yes                      | E use, Tab switch combat mode, I/K/J inventory/skills/quests, Esc pause | `hud`         |
| Talking   | no                       | W/S (or mouse, d-pad) + Enter answer, Esc walk away | `conversation`          |
| Fighting (turn) | no (stands on its arena marker) | W/S + Enter pick an action, Flee       | `fight`, enemy HP on `hud`     |
| Fighting (real time) | yes             | 1-4 skills, Q potion, F flee                 | key help, enemy HP on `hud`    |
| Dead      | no                       | W/S + Enter                                  | `death`                        |
| Start menu (launch, a save exists) | no      | W/S + Enter                                  | `start`                        |

At launch, when the saves folder has a save-stone save or an autosave, a start menu comes first
(`#UMBRA-65`; before, the game always began fresh in the settlement, so a save made in the dungeon
seemed lost). **Continue** loads the newer of the two (file time), **Load the other save** the older
one (only when both exist), **New game** plays on in the start area. Not shown when a scene is named
on the command line (the editor's Play) or in `--benchmark`.

The combat mode can only be switched while exploring (user decision, 2026-11-07).

## Screens (S3)

The game draws its UI with `engine/ui` (`engine/ui/include/UI.md`); the screen files come from
the `rpg-ui` plugin (a project's own `ui/` files win). Each frame, in `Main.cpp`:

1. `GameFlow::UiData()` gives one JSON object with everything the screens bind to (hero, quests,
   bag, equipment, skills, the conversation, the fight, the start menu). `Main.cpp` adds the
   `[E]` prompt (only while exploring), the latest message and asset warnings.
2. `GameFlow::Screen()` names the one game screen this phase needs; `GameUi::ShowGameScreen`
   closes the previous one. The `hud` is always open underneath.
3. `UiSystem::Update` draws every open screen and returns what the player chose.
   `GameFlow::HandleUi` takes the game screens' actions (answers, fight actions, equip, unlock,
   death and start choices); `Main.cpp` takes the pause screen's (resume, restart, quit).

The pause, inventory, skills and quests screens stop the game (no simulation, game sounds
paused). The mouse steers the camera only while no screen takes the input. Inventory, skills and
quests open with I, K and J while exploring, or from the pause screen. The action names are in
`plugins/rpg-ui/assets/ui/README.md`.

## Enemies in the world

- Every `spawner` entity puts `count` roamers into the area when it loads, each a random actor
  from its `pool`. Roamers wander near the spawner; `chase` roamers run at a player they can see
  (`game/gameplay/include/Roamer.md`).
- Touching a roamer (capsules less than 1 m apart) starts a fight with that enemy plus 0 to
  `group - 1` random companions from the same pool.
- Every fight, turn-based or real-time, happens in the battle arena
  (`assets/scenes/arena.scene.json`; user decision 2026-11-07, replacing "fights in place").
  The player starts on the `player` marker, enemies on `enemy_1` .. `enemy_3`, facing the player.
- The dungeon is not reloaded: `SceneRuntime::EnterArena` keeps its world, physics and roamers
  aside, untouched, and `LeaveArena` puts them back, player at the exact spot they left.
- After a win the touched enemy is removed from the dungeon and stays gone until the area is
  loaded again. After a defeat the death menu shows over the arena.
- Fleeing (F, or Flee in the turn menu) always works: back to the dungeon. The touched enemy
  calms down and cannot start a new fight for 3 seconds, so the player can walk away.
- If the arena is replaced mid-fight (restart, hot reload), the fight ends like a flee.
  Loading a save or a new game drops the kept dungeon (the save decides the area).
- Feedback: whoever takes damage flashes red for 0.2 s (`SceneRuntime::Flash`). Animation
  comes later.

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

## Saves

- Save stones (`interactable.save: true`) write the manual save (`manual.sav`, previous one kept
  as `.bak`).
- Walking through a portal writes `autosave.sav` at the arrival point.
- Saves are only made while exploring (`GameSession::MakeSave`).
- Saves live in the per-user folder from `Platform::PreferencesFolder()` + `saves/` (falls back
  to the build's asset cache).
- Loading reads and checks the whole file first (falls back to the backup), then loads the area,
  and only then replaces the RPG state. A failure at any step leaves the game as it was.

## Reloading the RPG content while playing (P7.5)

The game watches `assets/rpg/reference.rpg.json` (`assets::FileWatcher`, which waits until the
file has been stable for 250 ms). A change is applied at the next moment the player is exploring.
Conversations and fights keep references into the content, so it never happens during them.

1. The new file must load on its own (`rpg::LoadContent`). If it has errors, the old content stays
   and the screen says why.
2. The running game must fit the new file. It is saved right now to `saves/reload-check.sav` and
   read back against the new content: the same checks as loading a save. If the hero holds an item
   the new file removed, for example, the old content stays.
3. The content object is replaced **in place**. The session, quest log and combat coordinator all
   refer to that one object, so they see the new definitions at once. The checked state is then
   re-applied and health/energy are clamped to new maximums. The hero keeps their current health,
   level and items: content edits change definitions, not the save.

Scenes and models already reload by themselves (`engine/assets/include/Reload.md`). Together that
means: edit in the editor, press Ctrl+S, and the running game changes within a second.

## Learned enemies (P8A)

An actor with `"policy"` in the RPG content (the bandit leader) uses its learned turn-combat AI
when the model loads and still fits the content (`game/policy/include/Policy.md`):

- **Asking:** when that enemy's turn starts, an observation and mask snapshot goes to the
  model's worker thread.
- **Acting:** after the 0.7 s enemy pause (the deadline), the answer is used if it is for this
  decision and legal. Otherwise the standard AI acts, and the log says so.
- **Display:** the enemy's box on the HUD shows "(learned AI)" in turn mode.
- **Refusal:** after a content reload that changes what it was trained on, the model is refused
  with a message, and the standard AI plays that actor.
- **Real time:** real-time fights always use the standard AI.
