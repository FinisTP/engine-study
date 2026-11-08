# Plugins

Game systems and tools, one folder each (why and how: `docs/adr/0005-plugins.md`). A project
turns them on in `projects/<name>/project.json`; required plugins are added automatically.

| Plugin | What it is |
|---|---|
| `rpg` | RPG rules: content, characters, inventory, skill tree, shared resolver; editor RPG forms |
| `story` | Quests and conversations |
| `turn-combat` | Turn-based combat; `UmbraTool fight` |
| `realtime-combat` | Real-time combat |
| `rpg-session` | Encounters, the game session and saves (needs one combat mode or both) |
| `third-person` | Player, roaming enemies, physics bridge, interaction, areas; default sounds; the Music component |
| `rpg-ui` | The game's screens (HUD, menus, conversation, fight, inventory, skills, quests), skin and fonts; assets only (`engine/ui/include/UI.md`) |
| `learned-ai` | Learned turn-based enemy; `UmbraTool policy-check`; Python training |
| `ai-authoring` | Editor/tool only: local AI quest drafts, textures, 3D models, music and sound effects |
| `web-export` | The browser build's page and scripts |

Inside a plugin:

```text
plugin.json   name, label, description, kind (game|tool), requires, requiresAny, modules, links
modules/<m>/  static libraries (CMakeLists.txt each)
game/ editor/ tool/   C++ compiled into the game, the editor or UmbraTool; registers itself:
              UMBRA_COMPONENT, UMBRA_EDITOR_PANEL, UMBRA_TOOL_COMMAND
assets/       files the project may override (same path in the project wins)
tests/ tools/ its tests and developer tools
```

Code that only works when an optional plugin is on checks `#if UMBRA_PLUGIN_<NAME>`
(e.g. `UMBRA_PLUGIN_TURN_COMBAT`).
