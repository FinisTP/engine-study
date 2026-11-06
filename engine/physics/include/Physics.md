# Physics adapter (Jolt)

`Umbra::Physics` wraps [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.6 (vcpkg `joltphysics`) behind `PhysicsWorld`. No Jolt type appears in `Physics.hpp`. Gameplay code sees only engine handles (`core::BodyHandle`, `core::CharacterHandle`), plain `Shape`/`BodyDesc` structs and glm math. That keeps the rest of the engine independent of the library: swapping it, or a web port without it, touches one `.cpp` file.

## What it offers

| Feature | API | Notes |
|---|---|---|
| Static / kinematic / dynamic bodies | `CreateBody`, `DestroyBody`, `Get/SetBodyTransform` | Box, sphere, vertical capsule. Invalid sizes give an invalid handle. |
| Ray queries | `Raycast` | Closest hit: body handle, point, surface normal, distance. Characters are not hit. |
| Walking characters | `CreateCharacter`, `MoveCharacter`, `TeleportCharacter` | Jolt `CharacterVirtual`: slides along walls, climbs steps up to `stepHeight`, sticks to slopes up to `maxSlopeDegrees`, jumps. |
| Debug view | `ForEachBody` | Shapes and transforms for drawing outlines (`gameplay::AddDebugLines`). |

## Layers

Two object layers keep the broad phase cheap:

| | static | moving |
|---|---|---|
| **static** | no (level geometry never moves) | yes |
| **moving** (kinematic, dynamic, characters) | yes | yes |

## Stepping

Call `Step(1/60)` at a **fixed** rate. The app's `SceneRuntime::Simulate` accumulates frame time and runs whole steps (at most 5 per frame; after a stall the backlog is dropped instead of causing a "spiral of death"). A fixed step makes stacking and contacts behave the same on fast and slow machines.

Characters move in `MoveCharacter`, called once per fixed step with the desired horizontal velocity:

```text
grounded:  vertical = jump ? jumpSpeed : 0
airborne:  vertical += gravity.y * dt
then Jolt's ExtendedUpdate moves the capsule, sliding along contacts, stepping up ledges and sticking to the floor
```

## Ownership

- One `PhysicsWorld` per loaded scene. `SceneRuntime` creates a new one together with each new `World` and swaps both at once, so bodies from an old scene can't leak into a new one.
- Jolt has process-wide state (allocator, factory, type registry). It's created by the first `PhysicsWorld` and destroyed with the last (reference count under a mutex).
- Jolt's job system runs **single-threaded** (`JobSystemSingleThreaded`): simple and deterministic. A thread pool is a later optimization, only if profiling asks for it.
- Body handles are generational (`SlotMap`); Jolt's body user data stores our handle so ray hits map back to it.

## Known limits (deferred on purpose)

- No triangle-mesh colliders yet; level geometry is built from boxes. Mesh colliders come when real level art needs them.
- No collision events or triggers yet. Interaction (P4.3) uses raycasts; portals (P4.4) use simple distance checks.
- Characters don't collide with each other.
- Jolt's own debug renderer is not compiled in (vcpkg feature `debugrenderer` off); outlines come from our shape data instead.
- Jolt is built with AVX2 by default. Very old CPUs without AVX2 would need a different build.
