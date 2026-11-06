# StrideAnimator: walking without a skeleton

P4 uses procedural animation only (user decision O03): the player's body is one rigid
mesh, so "animation" means moving and tilting that mesh a little while it walks.
Two effects are enough to read as walking: a **bob** (up and down once per step) and a
**lean** (tilt forward with speed).

## Stride phase

`phase` is an angle that grows with distance traveled, not with time, so the bob
matches the feet at any speed:

```
phase += pi * speed * dt / stepLength
```

Each footstep adds `pi`. `|sin(phase)|` goes 0 → 1 → 0 over one step, so

```
bob = bobHeight * amount * |sin(phase)|
```

is lowest when a foot lands (`phase` = k·pi) and highest mid-step. Crossing a multiple
of `pi` is therefore the moment to play a footstep sound; `Update` returns true then.
In the air the phase stops, so no footsteps play while jumping.

## Smoothing

`amount` (0..1) and `lean` ease toward their targets instead of jumping:

```
follow = 1 - exp(-smoothing * dt)
value += (target - value) * follow
```

Using `exp` makes the result the same at 30 or 144 FPS: two half-length frames move the
value exactly as far as one full-length frame. (A plain `value += (target - value) * k`
would ease faster at higher frame rates.)

Targets: `amount → speed / fullBobSpeed` on the ground (0 in the air), and
`lean → maxLean * speed / fullLeanSpeed`. Both are clamped to their maximum.

## Applying the pose

`SceneRuntime::SyncPlayerVisual` adds `bob` to the body's height and rotates it by
`lean` around its own right axis (after turning it to the facing yaw). The physics
capsule never moves: the pose is visual only.
