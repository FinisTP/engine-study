# ADR 0008: One prompt that builds things: an editor agent and an MCP server (S5)

- Status: **accepted; implemented 2026-11-08** (`#UMBRA-80`; deferred, then started by the user after P11). The real-model check on Windows is open (`docs/testing/s5-agent-prompt.md`).
- Context: the user asked for an in-engine prompt that sets up a scene, adds sound or a model,
  or implements a feature, with different models working together ("like LangChain").

## Research (2026-11-08)

- Engines expose their editor as **MCP tools** (Model Context Protocol): Unity and Epic ship
  official MCP servers; Godot and Blender have mature third-party ones. Any MCP client (Claude
  Code, Codex, Cursor, local agents) can then inspect and change a live project.
- **Local tool calling** works through Ollama (since 0.3). Qwen3 8B is the usual choice on an
  8 GB GPU; 27-32B models are clearly more reliable on long multi-step chains. Malformed tool
  calls are the main failure of small models.
- **LangChain/LangGraph** (Python) add a standard agent loop, graphs, checkpoints and
  human-in-the-loop. A single-user editor needs one loop (ask with tools -> run the calls ->
  return results -> repeat) and a plan approval step, which is small in C++ and avoids a Python
  runtime in the editor.
- Umbra already has the actions such an agent needs: undoable scene and RPG edits, validators,
  AI texture/model/sound/quest generation, `ui-preview`, the fight simulator, `package-check`.
  "Models working together" = the language model plans and calls tools, and some tools run
  other models (SDXL, TRELLIS.2, ACE-Step, Stable Audio 3).

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| Surface | Both: an in-editor **Agent** window and an **MCP server**, sharing one tool registry |
| Models (in-editor) | Local only: Ollama with a tool-calling model; no cloud |
| Approval | The agent proposes a plan (steps with tool calls); the user approves; the result lands as one undoable edit |
| Scope | The in-editor agent changes data only (scenes, RPG content, screens, generated assets). C++ features come from external coding agents through MCP, which can also build and run tests |

## Planned design

1. **Tool registry** (`engine/authoring` or a tool-side module): each tool has a name, a JSON
   schema, a one-line description and a function over the documents (scene, RPG content, screen
   files) and the AI generators. Read tools (list scenes, inspect entity, list assets, validate,
   preview a screen to PNG) and change tools (add/move entity, add component, set field, generate
   texture/model/sound, use as music/sound). Changes go through the existing documents, so they
   are validated and undoable.
2. **Editor Agent window** (ai-authoring plugin): prompt -> the local model returns a plan of
   tool calls (dry-run: read tools run, change tools are listed) -> the user approves -> the
   change tools run in order -> one undo step for all document edits; generated files are new
   files. Results and failures are shown per step; a failed step stops the rest.
3. **`UmbraTool mcp`**: an MCP server over stdio exposing the same tools to external agents,
   local-only, plus the build/test commands for C++ work. It edits files on disk with the same
   validation; the editor notices changed files as it already does.
4. **Limits**: only the project folder; no shell from the in-editor agent; local network only
   (`Umbra.EngineOffline`); every generated asset keeps its provenance.

## As built (2026-11-08)

- **Tool calling by structured output.** Instead of Ollama's native `tools` field, the model must
  answer one JSON object (`look` / `summary` / `steps`) forced by a JSON schema in which the tool
  names are an enum. Small local models produce malformed native tool calls often; a schema-forced
  answer cannot name an unknown tool. Arguments are still checked against each tool's schema, and
  a broken or failing plan goes back to the model with the reasons. The loop (look -> results ->
  plan -> fix) is the LangChain-style agent loop in about 150 lines of C++.
- **Dry run before approval.** Every step runs on throwaway working copies, so the user sees which
  steps are already known to work; steps that need a generated file are checked when they run.
- **Step references.** `"$<n>"` in an argument stands for step n's result (a new entity id, a
  generated asset path), so one plan can generate a sound and attach it.
- **Generators shared with UmbraTool.** `localai::CreateTexture/Sound/Model` (`Generate.hpp`) now
  hold the whole generation; `UmbraTool texture/sound/model` and the agent's `generate_*` tools use
  them.
- **MCP**: stdio, JSON-RPC 2.0, one message per line; protocol revisions 2025-11-25, 2025-06-18,
  2025-03-26, 2024-11-05 (the client's if known); tools only (no resources or prompts). Each call
  is saved at once; `--build` adds `build` and `run_tests` with fixed commands. The editor reloads a
  scene changed on disk when it has nothing unsaved.
- **Default model** `qwen3:8b` (fits an 8 GB GPU); still to be measured on the task set in
  `docs/testing/s5-agent-prompt.md` §4 against a larger model if the user has one.
- Details: `plugins/ai-authoring/modules/agent/include/Agent.md`.

## Open before implementing (status)

- Which local model to recommend: default `qwen3:8b`; the measurement on the task set is the
  user's Windows check (open).
- MCP transport and revision: stdio, revisions above (done); HTTP later only if needed.
