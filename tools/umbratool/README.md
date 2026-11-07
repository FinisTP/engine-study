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

Exit codes: 0 ok, 1 errors found or the fight could not run, 2 bad usage.

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
