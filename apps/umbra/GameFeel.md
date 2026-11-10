# GameFeel: what the screen does when a blow lands (S9)

`GameFeel` turns each blow into feedback. It only reads `GameFlow::Impact` (a point, an amount,
heal / heavy / on the player) and draws; the rules never know about it.

| Feedback | When | How |
|---|---|---|
| Damage number | every blow and heal | rises 0.6 m over 0.9 s, fades in the last third; heals are green `+n` |
| Gold, larger number | a heavy blow | the theme's `strong` font |
| Hit-stop | a heavy blow | the game's time stands still for 0.07 s |
| Camera shake | a heavy blow | 0.25 s, 12 cm at most |
| Red flash | the player is hit | 0.3 s, a quarter-strength red over the screen |

A blow is **heavy** when it takes a quarter of the target's maximum health or more, or finishes it.

## Two clocks

The frame has a real time step and a game time step:

```
gameSeconds = realSeconds * feel.Update(realSeconds)   // 0 during hit-stop, else 1
```

The world, physics, animation and particles use `gameSeconds`, so a freeze stops everything at the
moment of impact. `GameFeel`'s own timers, the renderer and the screens use `realSeconds`, so the
freeze always ends and menus keep working.

## The shake

```
strength = shakeLeft²                 (1 -> 0 over 0.25 s; squared = dies away quickly)
offset   = (sin(71 t), sin(97 t + 1.3), 0) * 0.12 m * strength
```

Two sine waves of different speeds never line up, so it reads as a jolt, not a wobble. The eye and
the target move together: the picture shifts, it does not turn. The shake is applied after the
scene set the camera and is gone the next frame (nothing is stored in the camera).

## Numbers

A number is placed in the world (the middle of whoever was hit, `SceneRuntime::ChestOf`) and turned
into a screen point each frame (`GameUi::ScreenPoint`), so it stays over the fighter while the
camera moves. It is drawn through the UI painter with the theme's fonts, under the game screens.
