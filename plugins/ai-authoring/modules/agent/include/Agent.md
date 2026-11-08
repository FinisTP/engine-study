# The agent prompt (roadmap S5, ADR 0008)

One request in plain words ("put a well with a creaky sound next to the elder") becomes a
checked plan of tool calls, which the user approves. The same tools are offered to external
coding agents (Claude Code, Codex, Cursor...) through an MCP server.

```text
              +-------------------- AgentTools (one list) --------------------+
              |  read: list_scenes, scene_outline, get_entity, list_assets,    |
              |        list_components, rpg_outline, rpg_get                   |
              |  change: add_entity, place_model, move_entity, remove_entity,  |
              |        set_field, set_component, remove_component,             |
              |        rpg_set, rpg_add, rpg_remove                            |
              |  generate: generate_texture, generate_sound, generate_model    |
              +---------------------------------------------------------------+
                     ^                                  ^
editor Agent window: Planner (local Ollama) -> Runner   UmbraTool mcp: one call at a time,
UmbraTool agent "<request>" [--apply]                   saved at once (+ build, run_tests)
```

## Tools (`AgentTools.hpp`)

Each tool has a name, a one-line description, a JSON schema for its arguments and a kind:

| Kind | Does | Runs |
|---|---|---|
| read | looks at scenes, assets, components, RPG content | at once |
| change | edits a scene or the RPG content through `authoring::Document` | at once, on working copies |
| generate | makes new files with a local model through ComfyUI (`localai::CreateTexture/Sound/Model`) | on a worker thread, minutes |

`CheckArguments` checks the arguments against the schema before a tool runs (missing, unknown,
wrong type, not in the list, wrong length), and the message goes back to the model to fix.

**Edits.** Change tools never touch a document directly. `Edits` keeps a working copy per
document:

1. `Change`: apply the step's changes to the working copy, then run the document's own
   validator (the same check the game runs when it loads the file). Refused = nothing changes,
   and the step is named with the loader's diagnostics.
2. A later step sees the earlier ones (add an entity, then give it a component).
3. `Commit`: one `Document::Apply` per changed document, all or none. Each document gets **one
   undo step** for the whole plan. If the user edited a document meanwhile, the revision no
   longer matches, the commit is refused and the documents already applied are undone.

## Plan and run (`Agent.hpp`)

**Planner.** The model gets the tool list, the open scene's entities, the RPG ids and the
request, and must answer one JSON object, forced by Ollama's structured output (the tool names
are an enum in the schema, so a small model cannot invent tools):

```json
{"look": [{"tool": "get_entity", "args": {"id": "elder"}}], "summary": "", "steps": []}
{"look": [], "summary": "a well by the elder", "steps": [
  {"tool": "generate_sound", "args": {"idea": "creaky wooden well winch", "kind": "effect"}, "why": "the sound"},
  {"tool": "place_model", "args": {"model": "models/well.glb", "position": [3, 0, 2]}, "why": "the well"},
  {"tool": "set_component", "args": {"id": "$2", "type": "interactable", "fields": {"prompt": "Draw water", "sound": "$1"}}, "why": "use it"}]}
```

- `look`: read tools the model wants first. They run, the results go back, it answers again
  (at most `maxLooks` rounds). This is the usual agent loop, in a few lines of C++ instead of a
  Python framework.
- `"$2"`: the value of step 2 (here the new entity's id; `"$1"` the generated sound's path).
- The plan is then **dry-run** (`CheckPlan`): every step runs on throwaway working copies,
  except generators and the steps that use their files ("checked when it runs"). A malformed
  or failing plan goes back to the model with the reasons (at most `maxFixes` times).

**Runner.** After approval the steps run in order, driven by `Tick()` from the editor's frame,
so documents are only touched on that thread. A generator runs on a worker thread (it only
writes new files). The first failed step stops the rest and nothing is committed; generated
files stay (new files, with provenance). If every step works, `Commit` lands the changes.

Limits: only the project's files; no shell for the in-editor agent; Ollama and ComfyUI on this
computer only (`Umbra.EngineOffline`); generated assets keep their `.source.json`.

## MCP server (`Mcp.hpp`, `UmbraTool mcp`)

MCP is JSON-RPC 2.0; over stdio each message is one line. The server answers `initialize`,
`ping`, `tools/list` and `tools/call`; notifications get no answer; other methods get error
-32601. Each `tools/call` runs one tool on fresh working copies, commits it and **saves the file
at once**. Before each call it re-reads documents another program saved (the editor), and an
open editor reloads a scene that changed on disk if it has no unsaved changes.

With `--build <build folder>` it also offers `build` (`cmake --build`) and `run_tests`
(`ctest`, optional `-R` filter of letters, digits and `._|*-`), so a coding agent can change C++
and check it. The client cannot pass any other command.

Example for Claude Code (`.mcp.json` in the repository root):

```json
{
  "mcpServers": {
    "umbra": {
      "command": "out/build/windows-msvc/tools/umbratool/Debug/UmbraTool.exe",
      "args": ["mcp", "--assets", "projects/reference/assets", "--scene", "scenes/settlement.scene.json",
               "--build", "out/build/windows-msvc"]
    }
  }
}
```

## Without the editor

`UmbraTool agent "<request>" --assets <folder> --scene <scene> [--model qwen3:8b]` prints the
plan and its dry run; `--apply` runs it and saves. Useful to compare models on the same requests.
