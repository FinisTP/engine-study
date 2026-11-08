# Benchmark (`Umbra --benchmark [seconds]`, roadmap P9)

A repeatable performance run for the release gate. Run it from a package or a Release build:

```text
Umbra.exe --benchmark        (20 s per area)
Umbra.exe --benchmark 60     (60 s per area)
```

## What it does

1. Opens a 1920 x 1080 window and loads the settlement, then the dungeon, then the battle arena.
2. In each area the player walks forward while the camera turns slowly (0.6 rad/s), so it walks a
   circle and the view sweeps the whole area. No RPG layer: no fights, quests or saves start, so
   every run is the same.
3. The first 2 seconds of each area are not counted (loading, first-use costs).
4. Writes `benchmark.json` to the preferences folder (`%APPDATA%\Umbra\UmbraEngine\` on Windows; the log
   names the file) and prints it, then quits.

## The numbers

| Field | Meaning |
|---|---|
| `averageMs`, `fps` | mean frame time and its frames per second |
| `p50Ms`, `p95Ms`, `p99Ms` | nearest-rank percentiles: 99 % of frames took at most `p99Ms` (hitches show here, not in the average) |
| `maxMs` | the single worst frame |
| `cpuWorkAverageMs` | the frame's CPU work without the 1 ms idle sleep at the end of each frame (includes waiting for the GPU to free a frame) |
| `peakMemoryMb` | the most memory the game used during the whole run (all three areas loaded one after another): it should not grow from run to run |
| `withinBudget` | `averageMs <= 16.67` (60 FPS) and `p99Ms <= 33.33` (no frame slower than 30 FPS for 99 % of frames) |

The budget is the proposal in ROADMAP O18 for the reference PC; it is only meaningful together
with the `gpu`, `cpuThreads`, `ramMb` and `resolution` the report records. The game prefers the
mailbox present mode (no vsync wait) and sleeps 1 ms per frame, so frame rates above ~500 FPS are
not measured exactly; that is far beyond the budget.

## Screenshots (`--capture`, `--hour`, S6)

```
Umbra.exe --capture shot.png          (save the 90th frame as a PNG, then quit)
Umbra.exe --capture shot.png 200      (the 200th frame)
Umbra.exe --hour 21                   (pin the time of day to 21:00; the clock doesn't run)
Umbra.exe --capture night.png --hour 22 scenes/dungeon.scene.json
```

A capture shows only the 3D view (no HUD or diagnostics window), so pictures of the same scene can be
compared. In the browser the same options go in the address: `index.html?args=--hour%2012`
(capture is not available there).
