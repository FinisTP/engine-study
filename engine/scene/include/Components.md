# Components

An entity is an id, a transform, an optional model, and **components**: the parts that give it
behaviour. This is the same idea as Unreal's actor components or Unity's components. Each entity
lists only the components it has, one of each type:

```json
{ "id": "save_stone", "model": "models/stone.glb",
  "components": {
    "collider":     {},
    "interactable": { "prompt": "Save", "save": true },
    "spin":         { "speed": 30 } } }
```

## Built-in components (engine)

| Type | What it does | Code |
|---|---|---|
| `collider` | physics body | `world::Collider`, `gameplay/PhysicsBridge` |
| `interactable` | use with **E**: message, sound, conversation, save stone | `world::Interactable`, `gameplay/Interaction` |
| `portal` | walking in loads another scene | `world::Portal` |
| `spawner` | an enemy area | `world::Spawner`, `SceneRuntime` |

Their fields are in `SceneFormat.md`.

## Project components (your game)

A project component has two parts:

1. **A schema** in the project's assets: `components/<type>.component.json`. The editor builds
   its fields and its "Add component" entry from it, and the scene loader checks values with it.

   ```json
   { "format": "umbra.component", "version": 1, "type": "spin", "label": "Spin",
     "description": "Turns the entity around the up axis.",
     "fields": { "speed": { "kind": "number", "label": "Degrees per second", "default": 45, "min": -720, "max": 720 } } }
   ```

   | Field kind | Editor widget | Value |
   |---|---|---|
   | `number` / `int` | drag / whole-number box (`min`, `max`) | number |
   | `bool` | checkbox | true/false |
   | `string` | text box | text |
   | `asset` | list of asset files (only under `folder`, if given) | asset path |
   | `actor` | list of RPG actors | actor id |
   | `scene` | list of scenes | scene path |

2. **C++ behaviour** in the project's game folder (`projects/<name>/game/*.cpp`), compiled into
   the game program:

   ```cpp
   #include <GameComponents.hpp>
   class Spin final : public umbra::game::GameComponent {
   public:
       explicit Spin(const nlohmann::json &fields) : speed_(fields.value("speed", 45.0F)) {}
       void Update(umbra::game::ComponentContext &c) override { /* turn c.entity in c.world */ }
   private:
       float speed_;
   };
   UMBRA_COMPONENT("spin", Spin);
   ```

How it runs:

1. The scene loads. Each project component is stored on its entity as type + JSON text
   (`world::CustomComponent`). The world doesn't interpret it.
2. `SceneRuntime` calls `ComponentSet::Create`. Each registered type gets an object made from its
   fields, then `Start` runs once.
3. `Update` runs every frame with the frame's seconds, then the world's transforms are refreshed
   so the change is drawn the same frame.
4. A type with no C++ behaviour is only data. The game logs one warning and keeps running.

Why the code goes into the program and not into a library: `UMBRA_COMPONENT` registers the type
through a static object. A linker drops object files from a static library that nothing refers
to, and that would silently lose the registration.

Limits: one component of each type per entity. A component that moves its entity only moves the
picture: a static collider stays where it was.
