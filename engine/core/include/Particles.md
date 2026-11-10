# Particles (S9)

Fire on a torch, sparks from a blow, bubbles over a poisoned wolf, dust under a boot. One small
system does all of them: an **effect file** says how particles are born and how they change; an
**emitter** is one running copy of an effect at a place; the renderer draws every living particle
as a square that always faces the camera.

Code: `Particles.hpp/.cpp` (simulation, no renderer), `engine/scene/.../Vfx.hpp` (files, the `vfx`
component), `engine/render/shaders/Particles.hlsl` and `web/umbra_render.js` (drawing).

## The effect file (`vfx/*.vfx.json`)

```json
{
  "format": "umbra.vfx", "version": 1,
  "rate": 45, "lifetime": [0.35, 0.8], "radius": 0.07,
  "velocity": [0, 1.1, 0], "spread": [0.22, 0.35, 0.22],
  "gravity": 1.2, "drag": 0.8,
  "size": [0.26, 0.04],
  "color": [[4.0, 1.7, 0.35, 0.9], [1.6, 0.15, 0.0, 0.0]],
  "blend": "add", "max": 96
}
```

Every field is optional. A wrong one is reported by name (`vfx.rate`, `vfx.color`, ...), by the
game, the editor and `UmbraTool validate` alike.

| Field | Meaning | Default |
|---|---|---|
| `rate` | particles per second while the emitter runs | 20 |
| `burst` | particles made at once when it starts | 0 |
| `duration` | seconds the emitter runs; 0 = until removed | 0 |
| `lifetime` | `[shortest, longest]` life of a particle, seconds | 0.5, 1 |
| `radius` | particles start inside a ball this big (m) | 0.1 |
| `velocity` | start velocity (m/s) | 0, 1, 0 |
| `spread` | plus or minus this much, per axis | 0.3 each |
| `gravity` | m/s² along Y: negative falls, positive rises (hot air) | 0 |
| `drag` | how fast the velocity dies away (1/s) | 0 |
| `size` | meters across `[at birth, at death]` | 0.2, 0.05 |
| `color` | `[[r,g,b,a] at birth, [r,g,b,a] at death]` | white, fading out |
| `blend` | `"add"` glows (fire, sparks); `"alpha"` covers (smoke, bubbles) | add |
| `ring` | 0 = soft dot; towards 0.95 = hollow ring (bubbles) | 0 |
| `max` | most particles alive at once | 256 |

Colours may go above 1: the scene is drawn in HDR, so a colour of 4 is bright enough to bloom
(`Post.md`). A one-shot (sparks, a fireball's burst) has a `burst` and a short `duration` and
removes itself; an endless one (torch fire) has only a `rate`.

## Where effects come from

| Source | Lives |
|---|---|
| an entity's `vfx` component (`{"effect": "vfx/fire.vfx.json"}`) | while the entity has it, at the entity |
| an animation clip event with an `effect` (`AnimGraph.md`) | one-shot at the entity (dust under a step) |
| an RPG skill's `effect` | one-shot where the blow lands |
| an RPG item's `effect` | one-shot on whoever used it |
| an RPG status's `effect` | on the fighter while the status lasts |

The rules never read effects. They are only looks, so a fight plays the same with or without them.

## One step

Each frame, for every particle, with `dt` the frame's seconds:

```
velocity.y += gravity * dt
velocity   *= max(1 - drag * dt, 0)
position   += velocity * dt
age        += dt            (it dies when age >= its life)
```

Then new ones are made: `rate * dt` of them. That number is rarely whole (45 a second at 60 frames
a second is 0.75 a frame), so the fraction is carried to the next frame (`owed_`); over a second
exactly 45 appear, at any frame rate.

What is drawn is a straight blend over the particle's life, with `t = age / life` (0..1):

```
size  = size.birth  + (size.death  - size.birth)  * t
color = color.birth + (color.death - color.birth) * t
```

## Random but repeatable

Each emitter has its own tiny random generator (xorshift32: three shifts and xors of one 32-bit
number). The system gives emitters the seeds 1, 2, 3, ... in the order they are made. So the same
steps always give the same particles (test T89), which keeps captures and later the deterministic
world (ADR 0011) repeatable. Nothing here uses the clock or `rand()`.

## Drawing

A particle is 48 bytes sent to the GPU as one *instance*: position and size, colour, ring and
blend. The vertex shader makes the square's four corners itself:

```
corner = (-1 or 1, -1 or 1)
world  = position + (cameraRight * corner.x + cameraUp * corner.y) * size / 2
```

`cameraRight` and `cameraUp` are found by turning two points of the screen back into the world
with the inverse view-projection matrix, so the square always faces the screen.

The pixel shader makes the shape from the distance `d` to the square's middle (0 centre, 1 edge):
a soft dot is `(1 - d)²`; a ring is the same edge with the middle cut out.

**One blend mode for glow and cover.** The pass uses *premultiplied* blending:
`result = particle.rgb + background * (1 - particle.a)`.

- cover (`"alpha"`): the shader outputs `rgb * alpha, alpha`: the usual see-through blend.
- glow (`"add"`): it outputs `rgb * alpha, 0`: the background stays whole and light is added.

So fire and bubbles go through the same pipeline in one draw call. Particles are drawn last in the
scene pass: they are hidden by walls in front of them (depth test) but never hide each other by
depth (no depth write). They are sorted far to near on the CPU so covering ones look right. Fog
fades them like everything else, and since they are in the HDR scene, bloom and tone mapping apply.

## Limits

- At most 8192 particles are drawn in a frame (`ParticleSystem::maxDrawn`); the farthest go first.
- Particles are simulated on the CPU. A few thousand cost well under a millisecond; a million would
  need a GPU simulation, which a small RPG does not.
- No textures or sprite sheets yet: shapes are the soft dot and the ring. No collisions.
- Particles are not lit by the scene's lights and cast no shadows; their colour is what you write.
