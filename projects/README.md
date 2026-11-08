# Projects

One folder per game made with Umbra. The build uses one of them: `UMBRA_PROJECT` in CMake,
`reference` by default (`cmake -DUMBRA_PROJECT=mygame ...`). The tests always use `reference`.

```text
projects/<name>/
  project.json  which plugins the game uses (plugins/README.md)
  assets/   content: scenes, RPG content, models, textures, sounds, learned AI,
            components/*.component.json (schemas of the project's own components)
  engine/   engine extensions only this game needs (C++, may use engine/)
  game/     this game's C++: its components (UMBRA_COMPONENT), may use engine/, game/ and its engine/
```

Layers, from the bottom (each may use only the ones above it in this list):

| Layer | Folder | Holds |
|---|---|---|
| Base engine | `engine/` | core, platform, render, world, physics, assets, scene, audio |
| Plugins | `plugins/<name>/` | RPG rules, combat modes, story, session and saves, learned AI, gameplay, AI tools, web export (each its own folder; `plugins/README.md`) |
| Project engine | `projects/<name>/engine/` | this game's engine extensions |
| Project game | `projects/<name>/game/` | this game's components and rules |

The project's C++ files are compiled into the game program (`apps/umbra/CMakeLists.txt`). New
files are picked up at the next build. The editor's **Code** window lists all four layers and
opens files in Visual Studio 2022; its **New component** button writes a schema and a C++ file
from a template. Why and how: `docs/adr/0004-project-layers.md`, `engine/scene/include/Components.md`.

A new project: copy `reference` (or only its `assets` folder and `project.json`), choose its
plugins in `project.json`, then configure with `-DUMBRA_PROJECT=<name>`.
