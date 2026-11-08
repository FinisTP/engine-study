# Player controller and orbit camera

`PlayerController` turns intent (move, sprint, jump, look) into movement of a physics character, and places a third-person camera behind it. It knows nothing about keyboards or windows: the app fills `PlayerInput`, and tests drive it from code.

## Angles

Yaw is measured around +Y. **Yaw 0 looks along -Z** (the engine's forward). Positive yaw turns left (counter-clockwise seen from above).

```text
forward(yaw) = (-sin yaw, 0, -cos yaw)        yaw 0 -> (0,0,-1), yaw +90 deg -> (-1,0,0)
right(yaw)   = (-forward.z, 0, forward.x)     yaw 0 -> (1,0,0)
yaw of a direction d = atan2(-d.x, -d.z)
```

Mouse: moving right turns right, so the app passes `yawDelta = -dx * sensitivity`. Moving down looks down: `pitchDelta = -dy * sensitivity`. Pitch is clamped to `[minPitch, maxPitch]` (about -69° to +29°).

## Movement (per fixed step)

1. `move` (x = right, y = forward) is turned into a world direction using the **camera** yaw: W walks away from the camera, whatever the body faces.
2. Diagonal input is normalized, so W+D isn't faster than W.
3. The body turns toward the move direction, smoothly: `facing += shortestAngle(facing, target) * min(turnSpeed * dt, 1)`.
4. `PhysicsWorld::MoveCharacter` does the rest (walls, steps, slopes, gravity, jump).

Jump presses are edge events. The app may render several frames per fixed step or several steps per frame, so `SceneRuntime` keeps a pending jump until a step uses it. That way a press is never lost or applied twice.

## Camera

```text
pivot  = capsule center + (0, cameraHeight, 0)
view   = cos(pitch) * forward(cameraYaw) + (0, sin(pitch), 0)
camera = pivot - view * distance,  looking at pivot
```

With negative pitch the camera sits above and behind the player, looking down at them.

**Wall collision:** a ray is cast from the pivot toward the desired camera position. If it hits something closer than `cameraDistance`, the camera stops 0.2 m in front of the hit (never closer than 0.3 m), so walls never block the view. The player's own capsule isn't hit, because characters are not ray targets.

## Interaction query

A ray from the capsule's front edge at waist height, along the facing direction, up to `interactDistance` (2 m). P4.3 uses it for "press E" interactions.

## Spawning (app convention)

`SceneRuntime` looks for the entity with id **`player`** in the loaded scene:

- its position is where the capsule's **feet** start, its rotation gives the initial facing;
- it's drawn as the player's body: each frame its transform follows the capsule (center height, facing yaw), keeping its authored scale;
- a `collider` on it is ignored with a warning (the character is the collider);
- on a hot reload of the same scene, the player keeps its current position instead of jumping back to the marker.

A scene without a `player` entity is shown from a fixed overview camera.
