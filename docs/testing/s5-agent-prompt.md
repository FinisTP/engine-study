# S5 — Agent prompt (editor Agent window, MCP server): verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-80` (S5.1–S5.4), ADR 0008
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan). No
  Ollama or ComfyUI models can be downloaded there: the planner is tested against a scripted
  stand-in Ollama, the generators only for "ComfyUI not running".
- State: **implemented; verified headlessly.** Real-model task set and the editor window are for
  the user on Windows (§4).

## What was built

| Task | Where |
|---|---|
| S5.1 Tool registry | `plugins/ai-authoring/modules/agent` (`Umbra::Agent`): `AgentTools.hpp/.cpp` (20 tools, schemas, `CheckArguments`, `Edits`, `FileWorkspace`); `authoring::Document::Check`; `localai::CreateTexture/Sound/Model` (`Generate.hpp`), now also used by `UmbraTool texture/sound/model` |
| S5.2 Agent loop | `Agent.hpp/.cpp`: `Planner` (schema-forced JSON answer, look rounds, dry run `CheckPlan`, fixes), `Runner` (in order, generators on a worker thread, stop at the first failure, `Commit` = one undo step per document), `$<n>` step references |
| S5.3 Editor window | `plugins/ai-authoring/editor/AgentPanel.cpp` (prompt, model picker, plan table with dry-run states, approve/discard, stop); the editor reloads a scene changed on disk when nothing is unsaved |
| S5.4 MCP server | `Mcp.hpp/.cpp` + `UmbraTool mcp` (stdio JSON-RPC 2.0; `initialize`, `ping`, `tools/list`, `tools/call`; `--build` adds `build`, `run_tests`); `UmbraTool agent` (plan, `--apply`) |

How it works: `plugins/ai-authoring/modules/agent/include/Agent.md`.

## 1. Automated checks

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **49/49 / 49/49 / 49/49** |
| Plugin sets (turn-only + learned AI; real-time only) | **37/37, 33/33** |
| T83 tools: list scenes/entities/assets/components (a plugin's `music`)/RPG ids; every tool has an object schema | pass |
| T83 arguments: missing, unknown, wrong type, wrong length, not in the list each named; `"$1"` accepted | pass |
| T83 edits: an entity gets a free id (`well_2`), a component, a turn, a placed model, a new RPG item; the documents stay unchanged until commit; a missing model file (`asset.missing`) and an item without id are refused; commit = one undo step per document, Undo restores the scene exactly | pass |
| T83 conflict: the user edits the scene before commit -> commit refused, the RPG part undone too | pass |
| T83 dry run: generator and the step using its file "checked when it runs"; a step using only a new entity id is checked; `$9` and an unknown id fail; nothing marked dirty | pass |
| T83 planner (stand-in Ollama): a look round (results sent back), a plan with a missing argument (reason sent back), then a checked plan; the request forces the answer schema and carries the open scene; a model that never fixes its plan stops after 2 fixes (`agent.plan`); a non-local Ollama is refused (`ai.host`) | pass |
| T83 runner: a two-step plan with a reference and a project component commits once and undoes in one step; a failing middle step leaves the documents unchanged (done / failed / skipped); a saved run reaches the file | pass |
| T83 MCP: initialize (the client's revision), no answer to a notification, tools/list, a scene edit saved to disk, a bad call `isError` with the reason, -32601 and -32700, a file changed on disk re-read before the next call, the stdio loop with a `build` stand-in | pass |
| `Umbra.Tool.AgentLocalOnly`: `UmbraTool agent` with a non-local host -> `ai.host` | pass |
| Real stdio session: `UmbraTool mcp --assets <copy> --build build-gcc` fed initialize, tools/list (22 tools), `place_model` (wolf, saved), `run_tests` filter `Umbra.Core` (passed) | pass |

## 2. Not verifiable in the cloud

- Plan quality of a real local model (which requests it understands, how often it needs fixes).
- The editor window itself (ImGui on the PC) and the editor's reload after an MCP edit.
- The generators with real ComfyUI models inside a plan.
- A real MCP client (Claude Code, Codex, Cursor) talking to `UmbraTool mcp`.

## 3. Known limits

- The in-editor agent changes only the open scene and the RPG content; screens (`ui/*.ui.json`)
  have no tools yet.
- Each document gets its own undo step; a plan that changes both the scene and the RPG content
  needs Ctrl+Z in each.
- A small model (8B) may need the fix round often; plans with many steps are more reliable with a
  larger model.

## 4. Windows checklist (user)

1. Rebuild (`scripts\build-and-run.bat`), then `ollama pull qwen3:8b` (and, if the GPU allows, a
   larger model such as `qwen3:14b` to compare).
2. Editor: open `scenes/settlement.scene.json`, open **Agent** (toolbar or View). Run these
   requests one by one; for each, note whether the plan was right first time, after fixes, or
   failed, and the time:
   1. "Put a crate next to the well."
   2. "Make the signpost say 'East: the old dungeon' when used."
   3. "Add a bread item that heals 10 HP and can stack to 5."
   4. "Move the elder two meters closer to the save stone and turn her to face it."
   5. "Add calm village background music to this scene." (generates with ACE-Step: minutes)
   6. "Put a well with a creaky sound next to the elder." (generates a sound effect)
3. After each approved plan: the scene view shows the change; **Ctrl+Z** undoes it in one step;
   Save, then Play (F5) and try it in the game.
4. **Discard** a plan: nothing changes. **Stop** during a generation: nothing changes.
5. MCP: add the `.mcp.json` from `Agent.md` to the repository root, start Claude Code there, ask it
   to "list the scenes and put a barrel by the gate in the settlement" with the editor open on
   that scene: the editor reloads and shows the barrel.
6. `UmbraTool agent "Put a crate next to the well." --assets projects\reference\assets --scene scenes/settlement.scene.json --model <model>`
   prints the same kind of plan; compare models this way.

Report the table from step 2 so the default model can be chosen (ADR 0008).
