# ADR 0004: Layers, projects and components (S1)

- Status: **accepted; implemented** (2026-11-08, `#UMBRA-71`, `#UMBRA-72`).
- Context: after P10 the user asked for three things. (1) The editor should list and open the
  project's C++ in the IDE, which needs clear layers: base engine, base game, the game's own engine
  changes, and the game's own code, each with its assets and code. (2) Every entity showed
  interactable/portal/spawner sections, which doesn't scale; use actors with components, as in
  Unreal. (3) A smaller ask: editor changes should reach the web build (done in `#UMBRA-70`).

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| IDE | Visual Studio 2022 |
| Layout | Move folders: `engine/` base engine, `game/` base game, `projects/<name>/{assets,engine,game}`; one CMake build |
| Objects | Components, including custom C++ components; the four built-in parts become components |

## Design

1. **Folders are the layers.** `engine/` (core, platform, render, world, physics, assets, scene,
   audio) never includes from `game/` (rpg, turncombat, realtimecombat, encounter, story, game,
   policy, gameplay). The project's `engine/` and `game/` sit on top. `engine/authoring` and
   `engine/localai` are tool libraries for the editor and UmbraTool. They validate RPG content and
   draft quests, so they know both layers, and they are never linked into the game.
2. **One project per build.** `UMBRA_PROJECT` (default `reference`) selects
   `projects/<name>/`. The game compiles that project's C++ into its own program (not a library,
   so static registrations survive linking) and gets only the project *name*. At run time it
   finds `assets/` next to itself (package) or `projects/<name>/assets` above the build folder.
   The package check now also refuses a program that contains a project's source assets path.
   Tests always use `projects/reference/assets`.
3. **Components.** An entity has `components: {type: fields}`, one of each type. The engine
   handles the built-in four. A project component is a schema
   (`assets/components/<type>.component.json`) plus C++ registered with
   `UMBRA_COMPONENT("type", Class)`. The world stores project components as type + JSON text and
   doesn't interpret them. The game creates the behaviour at load and calls `Update` every frame.
   Older scenes still load, and the editor moves their parts into `components` as one undo step.
   Details: `engine/scene/include/Components.md`.
4. **Editor.** The Inspector shows components with remove buttons and an **Add component** menu,
   with fields drawn from the schema. The **Code** window lists the four layers and opens files
   in VS 2022: `devenv /Edit <file>` for a file, `devenv <source folder>` for the project (CMake
   folder mode), and devenv is found with `vswhere`. **New component** writes a schema and a C++
   file from a template.

## Consequences

- Asset paths in the repository changed from `assets/...` to `projects/reference/assets/...`.
  Paths *inside* content (`scenes/x.scene.json`) did not change.
- One component of each type per entity. A component that moves its entity moves only the
  picture; a static collider stays put.
- A new C++ component needs a game rebuild. Its schema shows in the editor at once.
