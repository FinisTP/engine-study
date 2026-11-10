# GameFlow: the reference RPG in the running game

`GameFlow` connects `umbra::game::GameSession` (the headless rules: story, fights, saves) to the
3D world in `SceneRuntime`, to the keyboard and to the game's screens. It adds no rules.
Everything it decides is about input, placement and what the screens show.

## Phases

| Phase     | Player moves?            | Keys                                         | Screen (`ui/*.ui.json`)        |
|-----------|--------------------------|----------------------------------------------|--------------------------------|
| Exploring | yes                      | E use, Tab switch combat mode, I/K/J/H inventory/skills/quests/hotkeys, Esc pause | `hud` |
| Talking   | no                       | W/S (or mouse, d-pad) + Enter answer, Esc walk away | `conversation`          |
| Fighting (turn) | no (stands on its arena marker) | Attack → move → target arrow, Item, Flee | `fight`, `fight-moves`, `fight-target`, `fight-items` |
| Fighting (real time) | yes             | 1-4 hotkeys (skills or items, set with H), F flee | hotkey bar, enemy HP on `hud` |
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

On the hero's turn the menu has three steps, all built from `TurnEncounter::LegalActions()`, so
everything offered is legal:

1. `fight`: Attack, Item (greyed out with no usable item), Flee.
2. `fight-moves`: each attack skill once (with its energy cost); `fight-items`: each usable item.
3. `fight-target`: an arrow over each fighter the chosen attack may hit. The arrows' screen
   points come from projecting a point above each fighter's head with the scene camera
   (`GameUi::ScreenPoint`); left/right or the mouse moves between them, Enter attacks.

Esc steps back one menu (`back`). An item acts at once.
Enemy turns wait 0.7 s, then `ChooseTurnAction` decides, through the same `Submit` as the
player. Everything goes through `EncounterCoordinator::Submit` with a fresh decision id.

## Real-time fights

Each frame:

1. Placement: the player's feet and facing, and each enemy roamer's feet and facing, go into
   `RealtimeEncounter::SetPlacement`. Movement belongs to physics; the fight only reads it.
2. Player keys start actions: 1-4 use what is on that hotkey, a skill or an item. The
   hotkeys screen (H, or the pause menu) sets them; they are kept in saves. A new game (or an
   older save) starts with the first three real-time skills and a potion on key 4.
3. Each living enemy asks `ChooseRealtimeAction`; its `move` becomes the roamer's velocity
   order (2.5 m/s), its `act` is submitted.
4. `Update(seconds)` advances wind-ups, hits and cooldowns.

## Blows (S9)

The rules decide a blow at once: health is already lower when the event arrives. The screen shows
it **when the swing lands**, so the sound, the sparks, the number and the health bar agree with the
animation.

1. A `Damage` event makes a `Blow` (attacker, target, amount, skill) and triggers the attacker's
   `attack` state.
2. The blow waits for the attacker's clip event named `hit` (`AnimGraph.md`, "Events"). An attacker
   without a rig or without that event lands it after at most 1.3 s; an attack with no attacker in
   the world (poison) lands at once.
3. Landing: the hit sound (the skill's `sound`, else `audio/hit.wav`), the target's white flash and
   `hit` state, the skill's `effect` (else `vfx/hit.vfx.json`) on its chest, and an `Impact` for
   `GameFeel.md` (number, and for a heavy blow hit-stop and shake).
4. A `Death` event marks the waiting blow as the finishing one: the enemy falls when it lands.

A decided fight stays on screen until its last blow has landed. In a turn fight the next enemy's
pause starts after the previous blow landed. Statuses with an `effect` show it over the fighter
while they last; an item's `effect` plays once on whoever used it.

`--demo <skill>` (with `--capture`) plays a turn fight by itself with that skill, for pictures and
checks without a keyboard; such a capture keeps the screens and numbers.

## Music and fight sounds (S4)

- Music: each scene's **Music** component (third-person plugin) names a looping `.ogg`. When the
  live scene changes (portal, the arena, restart) `Main.cpp` crossfades to the new scene's track
  over 1.5 s; a scene without one fades the music out. Fights happen in the arena scene, so its
  track is the battle music.
- Sounds: every hit plays the skill's `"sound"` from the RPG content, or `audio/hit.wav`; using an
  item plays its `"sound"` if it has one. GameFlow collects them while reading fight events
  (`TakeSounds`) and `Main.cpp` plays them on the game bus (paused with the game).

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
