# Umbra creator guide

This guide covers making a small change to the reference RPG, checking it, playing it, and
shipping it. It takes about 20 minutes. All paths are relative to the repository folder. The
commands are for Windows (Developer PowerShell or a plain `cmd` window). The reference game's
content is in `projects/reference/assets`; the folder layout is in
`projects/README.md`.

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
| Areas (settlement, dungeon, battle arena) | `projects\reference\assets\scenes\*.scene.json` | `engine\scene\include\SceneFormat.md` |
| Items, skills, statuses, actors, quests, conversations | `projects\reference\assets\rpg\reference.rpg.json` | `game\rpg\include\Rpg.md`, `game\story\include\Story.md` |
| Models, sounds | `projects\reference\assets\models`, `projects\reference\assets\audio` | `engine\assets\include\AssetPipeline.md`, `engine\audio\include\Audio.md` |

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
ollama pull qwen3.5:9b
```

(about 6.6 GB; a GPU with 8 GB makes it fast, a CPU works but slowly. `qwen2.5:7b`, 4.7 GB, is
the smaller alternative. Any installed model shows up in the window's **Model** list.)

**Each quest:**

1. In the editor: the **AI quest draft** button in the menu bar (or View → AI quest draft).
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

Without the editor: `UmbraTool draft projects\reference\assets\rpg\reference.rpg.json --npc elder --idea "..."`,
then the same command with `--apply` (see `tools/umbratool/README.md`).

## 2c. Make textures with AI

**Once:**

1. Install **ComfyUI Desktop** from [comfy.org](https://www.comfy.org) and start it.
2. Download `sd_xl_base_1.0.safetensors` (Stability AI's SDXL base 1.0, about 7 GB) and put it in
   ComfyUI's `models/checkpoints` folder. How long one texture takes on an 8 GB GPU is measured in the P8C.0 check (`docs/testing/p8c-ai-textures.md`).

**Each texture:**

1. In the editor, click the object to cover (a wall, the ground). Its texture should repeat, so
   start with boxes like the settlement's walls.
2. **AI texture** (menu bar). The window finds ComfyUI and selects the SDXL model.
3. Looks like: `mossy cobblestone`. Meters per repeat: `2`. Variations: `4`. **Generate**.
4. Click through the variations: each shows on the object in the scene view, repeating at its
   real size. One marked "may show" has a visible seam.
5. **Apply to object**. Ctrl+Z takes it off again; Ctrl+S saves the scene.

What you get: `projects/reference/assets/textures/mossy_cobblestone.png` and `projects/reference/assets/materials/mossy_cobblestone.material.json`.
The material records the model, prompt and seed, so the texture can be made again. The **Material**
picker in the inspector puts the same material on other objects.

Rules worth knowing:

- Every prompt gets the project style from `projects/reference/assets/materials/style.json`. Change it once to change the
  look of every new texture.
- Hand-made textures work the same way: a PNG with power-of-two sides (64–2048) and a material file
  (`engine/assets/include/Material.md`; `projects/reference/assets/materials/sample_bricks.material.json` is one).
- Only a ComfyUI on this computer is used; nothing is uploaded.
- `scripts\package.bat` warns about textures made with models whose license forbids selling.

Without the editor: `UmbraTool texture "mossy cobblestone" --assets projects\reference\assets --count 2`.

## 2d. Make props with AI

**Once** (needs ComfyUI from §2c):

1. Follow `tools/comfyui/README.md`: open ComfyUI's template **Pixal3D & TRELLIS.2: Image to Model**,
   download its models (about 10 GB), switch it to TRELLIS.2, run it once, then **Workflow → Export (API)**
   into `tools/comfyui/` in this project (any file name; pick it under **ComfyUI 3D workflow** in the
   AI model window if there are several).

**Each prop:**

1. In the editor: **AI model** (menu bar).
2. Looks like: `wooden barrel with iron bands`. **Make concept pictures**; **View** opens each in your
   image viewer. Pick one. (Or **Use my picture** and give a PNG/JPEG of one object on a plain background.)
3. **Make 3D model**. This takes minutes; you may close the window or even the editor (ComfyUI keeps
   working, and the next start saves the model). The model lands in `projects/reference/assets/models/` with its concept picture
   and a `.source.json` saying how it was made.
4. Set **Height in the scene** (meters), then **Place in scene**: it appears at the centre of the view,
   standing on the ground. Ctrl+Z removes it; Ctrl+S saves the scene. It gets a box collider fitted to its
   size (green outline when selected); after scaling it, press **Fit to model** in the inspector.

Rules worth knowing:

- About 10 000 triangles per prop by default (the **Triangles** slider), with a 1024 px texture.
- The concept style (`objectPrompt`, `objectNegative` in `projects/reference/assets/materials/style.json`) asks for one object
  on a plain background, which is what image-to-3D needs.
- Characters still come from Meshy for now.

Without the editor: `UmbraTool model --idea "stone lantern" --assets projects\reference\assets` (or `--image my.png`).

## 3. Find a planted error

Open `projects\reference\assets\rpg\reference.rpg.json` in a text editor. In the hero's `inventory`, change
`"health_potion"` to `"helth_potion"` and save. Then:

```text
UmbraTool validate projects\reference\assets\rpg\reference.rpg.json
```

prints

```text
projects\reference\assets\rpg\reference.rpg.json: error [ref.item] actors.hero.inventory: unknown item 'helth_potion'
1 file(s): 1 error(s), 0 warning(s)
```

The code (`ref.item`) and the field path tell you what is wrong and where. The editor refuses to
open the file with the same message. The running game keeps its old content and shows the error.
Fix the typo and validate again. `UmbraTool validate --json` prints the same thing for scripts.

Other useful commands:

| Command | Use |
|---|---|
| `UmbraTool inspect projects\reference\assets\rpg\reference.rpg.json` | Every id in the file |
| `UmbraTool fight projects\reference\assets\rpg\reference.rpg.json hero wolf wolf` | A quick balance check: the AI fights both sides |
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
3. Writes `out\package\Umbra\` (game, DLLs, Visual C++ runtime, `assets\`, `shaders\`,
   `licenses\` and `THIRD_PARTY_NOTICES.md`). AI concept pictures stay behind.
4. Checks the folder (`UmbraTool package-check`): no editor or tools, a license notice for every
   DLL, no keys or absolute paths, the content and enemy policy present. Errors stop it; warnings
   (e.g. AI working files in `assets\models`) are listed in `out\package.log`.
5. Zips it to `out\package\Umbra.zip`.

How fast is it? `Umbra.exe --benchmark` walks through the three areas and writes
`%APPDATA%\Umbra\UmbraEngine\benchmark.json` (`apps/umbra/Benchmark.md`).

Copy the zip to another PC, unzip, run `Umbra.exe`. Saves go to `%APPDATA%\Umbra\UmbraEngine\saves`;
when one exists, the game starts with a menu: Continue, Load the other save, New game.

## 6. The browser version

`plugins/web-export/web/README.md`. Short version: put a web build in `out\web`, double-click
`scripts\serve-web.bat`, play in Chrome or Edge. The page has **Export saves** / **Import saves**
(browsers may clear site data) and says **Ready offline** once it can start without a network.
Upload the files of `out\web` to any static web host to share it.

## Troubleshooting

| Problem | What to do |
|---|---|
| `build-and-run.bat` says BUILD FAILED | Read the first `error` in `out\build-and-run.log`. Meanwhile `out\known-good\Umbra.exe` still runs. |
| The editor refuses an edit | Look at the Diagnostics window: the code names the rule (`ref.*` = something referenced doesn't exist, `id.duplicate`, `field.range`, ...). |
| "Cannot open" a scene in the editor | Run `UmbraTool validate <scene> --assets projects\reference\assets`. The file has an error the game would refuse too. |
| The game ignores RPG content changes | Look at the message on screen: either the file has errors, or your current game uses something the change removed (load a save or start a new game). |
| Play does nothing | The scene couldn't be saved (see Diagnostics), or the game wasn't built. Build once with `build-and-run.bat`. |
| AI draft: "Ollama is not running" | Start Ollama (it runs in the tray after install), or check the address under **Model**. |
| AI draft: "model ... is not installed" | Pick an installed model in the **Model** list (press **Refresh**), or run `ollama pull qwen3.5:9b`. |
| `LNK1104: cannot open file ...exe`, "CreateProcess: Access is denied", or a program in `out\build` vanishes | Windows security is scanning, blocking or removing freshly built programs. Run `build-and-run.bat` again; if it repeats, add a Windows Security exclusion for the project's `out` folder (Virus & threat protection → Manage settings → Exclusions). |
| AI draft keeps failing the play-through | Make the idea simpler (one or two objectives), press **Ask the AI to fix the problems**, or write the quest by hand in the RPG forms (*quests* and *conversations*). |
| AI texture: "ComfyUI is not running" | Start ComfyUI Desktop, then press **Find**. A command-line ComfyUI listens on 8188: that is tried too. |
| AI texture: "ComfyUI has no model ..." | Put `sd_xl_base_1.0.safetensors` in ComfyUI's `models/checkpoints` folder and press **Find**. |
| AI texture: "out of memory" | Close other GPU programs, use 1 variation, or a smaller model (SD 1.5). |
| AI model: "no workflow file" / "UI workflow" | Export the TRELLIS.2 template with **Workflow → Export (API)** into `tools/comfyui/` (`tools/comfyui/README.md`), then **Refresh** in the AI model window. |
| AI model takes very long | Normal for minutes on 8 GB. Lower the upsample resolution to 1024 in the workflow (`tools/comfyui/README.md`). You can close the editor: ComfyUI finishes and the next start saves the model. |
| AI model: "ComfyUI no longer has this job" | ComfyUI was restarted or its queue cleared during the run. Make the model again. |
| AI model: "A model the workflow needs isn't installed" | Download the file ComfyUI names into the folder the template's note lists, restart ComfyUI. |
| The package won't start on another PC | It needs a GPU with Vulkan 1.3 drivers. Start it from a `cmd` window to see the log messages. |
