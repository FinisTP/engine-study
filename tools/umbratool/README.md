# UmbraTool

Headless content checks and test runs. It uses the same loaders as the game and the same edit
layer as the editor (`engine/authoring`), so all three report the same diagnostic codes.

| Command | What it does |
|---|---|
| `UmbraTool validate <file>... [--assets <folder>] [--content <rpg>] [--json]` | Checks scenes (including every model they draw), RPG content and saves. A save needs `--content`. |
| `UmbraTool inspect <file> [--json]` | Lists what a scene or RPG file holds, by id. |
| `UmbraTool schema [--json]` | Format versions this build reads, and the document describing each. |
| `UmbraTool fight <rpg> <hero> <enemy>... [--json]` | A turn fight with the conventional AI on both sides, e.g. to check balance after editing stats. |
| `UmbraTool policy-check <policy> --content <rpg> [--json]` | Is a learned enemy model readable, and does it still fit the content (same actions, same fingerprint)? The same check the game runs before using it. |
| `UmbraTool draft <rpg> --npc <npc> (--idea "<text>" \| --from <draft>) [--apply] [--json]` | AI quest draft (P8B): asks the local Ollama model, or reads a saved draft, then prints what would be added, the play-through and problems. Changes the file only with `--apply` and no errors (old file kept as `<rpg>.before-draft`). Options: `--model` (default `qwen2.5:7b`), `--host` (default `http://localhost:11434`, local only), `--scenes` (default `<rpg folder>/../scenes`), `--fix <n>` (ask the model to fix problems up to n times, default 1), `--save-draft <file>`. |

Exit codes: 0 ok, 1 errors found (or the fight couldn't run, or the draft has errors), 2 bad usage.

`--json` prints `{"files", "errors", "warnings", "diagnostics": [{"severity", "code", "file",
"field", "message"}]}`. That's the same shape as `UmbraSceneCheck --json`, so scripts and agents
can read both.

Example:

```text
> UmbraTool validate assets/rpg/reference.rpg.json
1 file(s): 0 error(s), 0 warning(s)
> UmbraTool fight assets/rpg/reference.rpg.json hero wolf wolf
Hero side wins after 6 round(s), 15 action(s)
  hero_1: 56 HP, 30 EN
  wolf_2: 0 HP, 0 EN
  wolf_3: 0 HP, 0 EN
```

Drafting a quest (rules in `engine/authoring/include/QuestDraft.md`):

```text
> UmbraTool draft assets/rpg/reference.rpg.json --npc elder --idea "wolves scare people away from the well"
Asking qwen2.5:7b... 41 s, 1873 characters
New quest "Wolves at the Well" (wolves_at_the_well)
  ...
Play-through:
  ok   Talking to elder can start the quest.
  ok   Objective 1 (wolf) can be completed.
  ok   Objective 2 (elder) can be completed.
  ok   The quest completes and pays its reward.
1 file(s): 0 error(s), 0 warning(s)
Nothing changed. Add --apply to add this quest.
```

`--json` prints `{"quest", "conversation", "preview", "play", "changes", "applied", "diagnostics"}`.
