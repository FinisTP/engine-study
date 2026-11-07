# Umbra creator guide

This guide covers making a small change to the reference RPG, checking it, playing it, and
shipping it. It takes about 20 minutes. All paths are relative to the repository folder. The
commands are for Windows (Developer PowerShell or a plain `cmd` window).

## 0. One-time setup

You need Visual Studio 2022 (C++ desktop workload), the Vulkan SDK and vcpkg (`VCPKG_ROOT` set).
Then build once:

```text
cmake --preset windows-msvc
scripts\build-and-run.bat
```

`build-and-run.bat` does four things:

1. Builds the Release configuration.
2. Runs the tests.
3. Copies a fully passing build to `out\known-good`.
4. Starts the game.

The log is in `out\build-and-run.log`. The first build downloads and compiles the libraries, so
it takes a while; later builds take seconds.

| Program | Path |
|---|---|
| Game | `out\build\windows-msvc\apps\umbra\Release\Umbra.exe [scene]` |
| Editor | `scripts\editor.bat [scene]` |
| Command-line checks | `out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe` |

## 1. Where things are

| What | File | Described in |
|---|---|---|
| Areas (settlement, dungeon, battle arena) | `assets\scenes\*.scene.json` | `engine\scene\include\SceneFormat.md` |
| Items, skills, statuses, actors, quests, conversations | `assets\rpg\reference.rpg.json` | `engine\rpg\include\Rpg.md`, `engine\story\include\Story.md` |
| Models, sounds | `assets\models`, `assets\audio` | `engine\assets\include\AssetPipeline.md`, `engine\audio\include\Audio.md` |

All of it is data. Changing it needs no C++ build.

## 2. Change data in the editor

Start `scripts\editor.bat`, then try these steps.

1. **Add an item.** View → RPG content → *items* → **Add**. Set the id to `bread`, the name to
   `Bread`, and under *Use effects* add `heal` with power `15`.
2. **Give it to the hero.** In the *actors* tab, select `hero` → *Inventory at start* → **Add** →
   pick `bread`, count 2.
3. **Move something in a scene.** Close the RPG window. Click the signpost in the settlement,
   press **W** and drag an arrow, or type a new position in the Inspector.
4. **Save** with Ctrl+S. Ctrl+Z / Ctrl+Y undo and redo each step.
5. **Play** with F5. The editor saves, then starts the game on this scene.

Rules worth knowing:

- An edit the game would not accept is refused, and nothing changes. **Diagnostics** says why.
  For example, deleting an item a quest still needs gives `ref.item`.
- While the game is running, saving in the editor updates it. Scenes reload at once. RPG content
  reloads the next time you are not talking or fighting, as long as your current game still fits
  it. The message on screen tells you either way.
- If you edit a file outside the editor while it is open, the status bar warns you. Saving then
  asks you to reload first, so neither change is lost silently.

## 2b. Write a quest with AI

Umbra can draft side quests with an AI model that runs on your own PC. The draft is checked like
any edit, played through with the real rules, and only added when you approve it.

**Once:** install Ollama from [ollama.com](https://ollama.com), then in a terminal:

```text
ollama pull qwen2.5:7b
```

(about 4.7 GB; a GPU with 6 GB or more makes it fast, a CPU works but slowly).

**Each quest:**

1. In the editor: View → **AI quest draft**.
2. Quest giver: `elder`. Idea: `Wolves scare the villagers away from the old well.`
   Press **Write draft**. The first answer can take a minute while the model loads.
3. Read the preview. Each play-through line should be green. A red line says what's missing,
   for example "no choice with action talk is shown while the quest is active".
   Press **Ask the AI to fix the problems**, or **Write a new draft**.
4. **Approve and add.** The RPG content window opens on the new quest: change any text, reward
   or objective by hand. Ctrl+Z removes the whole quest again.
5. Ctrl+S, then F5: talk to Elder Mara, pick the new line, play the quest.

Rules worth knowing:

- A draft only **adds**: one quest, its lines in the NPC's conversation, and one new choice that
  opens them. It never changes or deletes what you wrote.
- It may only use items, enemies and the NPC that already exist. Add new ones by hand first.
- The play-through assumes you win every fight. Whether the fights are fair is up to you
  (`UmbraTool fight` helps).
- Nothing is sent to the internet: only an Ollama on this computer is accepted.

Without the editor: `UmbraTool draft assets\rpg\reference.rpg.json --npc elder --idea "..."`,
then the same command with `--apply` (see `tools/umbratool/README.md`).

## 3. Find a planted error

Open `assets\rpg\reference.rpg.json` in a text editor. In the hero's `inventory`, change
`"health_potion"` to `"helth_potion"` and save. Then:

```text
UmbraTool validate assets\rpg\reference.rpg.json
```

prints

```text
assets\rpg\reference.rpg.json: error [ref.item] actors.hero.inventory: unknown item 'helth_potion'
1 file(s): 1 error(s), 0 warning(s)
```

The code (`ref.item`) and the field path tell you what is wrong and where. The editor refuses to
open the file with the same message. The running game keeps its old content and shows the error.
Fix the typo and validate again. `UmbraTool validate --json` prints the same thing for scripts.

Other useful commands:

| Command | Use |
|---|---|
| `UmbraTool inspect assets\rpg\reference.rpg.json` | Every id in the file |
| `UmbraTool fight assets\rpg\reference.rpg.json hero wolf wolf` | A quick balance check: the AI fights both sides |
| `UmbraTool schema` | Format versions and where each format is described |

## 4. Change C++ (systems, not content)

Edit a source file, then run `scripts\build-and-run.bat` again. Only what changed is rebuilt:

- A gameplay `.cpp`: a few seconds.
- A widely used header: much longer (measurements in `docs\testing\phase7-creator-workflow.md`).

If the build or a test fails, `out\known-good\Umbra.exe` is still the last version that passed,
together with the assets it passed with.

## 5. Ship it

```text
scripts\package.bat
```

1. Builds Release.
2. Validates the shipped content (it stops if anything is wrong).
3. Writes `out\package\Umbra\` (game, DLLs, Visual C++ runtime, `assets\`, `shaders\`).
4. Zips it to `out\package\Umbra.zip`.

Copy the zip to another PC, unzip, run `Umbra.exe`. Saves go to `%APPDATA%\Umbra\UmbraEngine\saves`.

## Troubleshooting

| Problem | What to do |
|---|---|
| `build-and-run.bat` says BUILD FAILED | Read the first `error` in `out\build-and-run.log`. Meanwhile `out\known-good\Umbra.exe` still runs. |
| The editor refuses an edit | Look at the Diagnostics window: the code names the rule (`ref.*` = something referenced doesn't exist, `id.duplicate`, `field.range`, ...). |
| "Cannot open" a scene in the editor | Run `UmbraTool validate <scene> --assets assets`. The file has an error the game would refuse too. |
| The game ignores RPG content changes | Look at the message on screen: either the file has errors, or your current game uses something the change removed (load a save or start a new game). |
| Play does nothing | The scene couldn't be saved (see Diagnostics), or the game wasn't built. Build once with `build-and-run.bat`. |
| AI draft: "Ollama is not running" | Start Ollama (it runs in the tray after install), or check the address under **Model**. |
| AI draft: "model ... is not installed" | Run `ollama pull qwen2.5:7b` (or the model named under **Model**). |
| AI draft keeps failing the play-through | Make the idea simpler (one or two objectives), press **Ask the AI to fix the problems**, or write the quest by hand in the RPG forms (*quests* and *conversations*). |
| The package won't start on another PC | It needs a GPU with Vulkan 1.3 drivers. Start it from a `cmd` window to see the log messages. |
