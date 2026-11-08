# P9 — PC release gate: verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-64` (P9.1–P9.4)
- Tester: Claude (remote session), Linux cloud workspace; Windows steps by the user (§4).
- State: **in progress. The release tools and headless checks are in place and pass on Linux
  (44/44 CTest, GCC/Clang/ASan). The gate itself needs the Windows runs in §4: the benchmark on the
  reference PC, a package built and checked, a clean-install run, the full player journey, and the
  still-open P3/P4/P6/P7/P8A checklists. Nothing in §4 is marked passed from a Linux run.**

## What was built

| Task | Where | Evidence |
|---|---|---|
| P9.1 Performance measurement | `Umbra --benchmark [seconds]` (`apps/umbra/Benchmark.md`): scripted walk through settlement, dungeon and arena at 1920×1080, warm-up excluded, `benchmark.json` with GPU, CPU threads, RAM, peak memory, frame-time average/p50/p95/p99/max per area, CPU work, budget verdict. `core::Summarize` (nearest-rank percentiles); `Renderer::DeviceName/Extent`; `Platform::CpuThreads/SystemRamMb/PeakMemoryMb` | `ProfilingTests` (percentiles), `BenchmarkTests` (areas, warm-up, report, budget) |
| P9.2 Long-run state | 300 fights in both modes (every third real time), groups of 1–3, each won fight saved, loaded, applied and saved again (byte-identical), a lost fight reloads the save, every 25th save cut in half and rescued from its backup | `SoakTests.cpp` (T74), 0.8 s |
| P9.3 Package composition | `UmbraTool package-check` (`engine/authoring/include/Package.md`); `scripts\package.bat` runs it and refuses to zip on errors. The game no longer contains build paths it depends on: assets are found at run time (next to the program, or upwards in a development build), MSVC PDB path is the file name only. `THIRD_PARTY_NOTICES.md` + `licenses/` (vcpkg copyright files) in the package; AI concept pictures excluded | `PackageTests.cpp` (T75), `Umbra.Tool.PackageCheckRefusesNonPackage` |
| P9.4 Records | this file, ROADMAP P9 status/T74–T75/O18, creator guide §5 | docs |

## 1. Automated checks (Linux, 2026-11-08)

| Configuration | Result |
|---|---|
| GCC 13, Debug | 44/44 CTest entries pass |
| Clang 18, Debug | 44/44 |
| GCC + ASan/UBSan | 44/44 |

The Linux build has no GPU, so the benchmark itself has not run anywhere yet.

## 2. Exit evidence (ROADMAP §8 P9) — where each part stands

| Exit evidence | Evidence so far | Still needed (Windows) |
|---|---|---|
| Creator walkthrough | P7 tools on Linux; the user ran the editor and the P8B/P8C/P8D AI windows (2026-11-08) | `phase7-creator-workflow.md` §4 |
| Full player journey | `GameSessionTests` (reference journey headless); P6 game played partly (2026-11-07) | `phase6-reference-game.md` §4, then §4 item 6 below on the package |
| Independent-mode fixtures | `Umbra.TurnOnlyDependencies`, `Umbra.RealtimeOnlyDependencies`, `Umbra.HeadlessDependencies` | the same CTests in the Windows build |
| Accepted policy evaluation | `p8a-learned-enemy.md`: accepted model, matched probes; 1 of 2 seeds | open: recipe robustness (second seed); not a packaging blocker |
| Saved-state recovery | `SaveGameTests` (damaged/foreign/newer saves, backups, failed writes), T74 long run | a save/quit/reload on the package (§4 item 6) |
| Graphical/physics checks | P3/P4 headless tests | `phase3-assets-world.md` §4, `phase4-sandbox.md` §5 (GPU, resize, audio) |
| Packaging/license inventory | T75, notices, install rules | §4 items 2–4 |
| Measured performance report | the benchmark tool | §4 item 1 on the reference PC |

## 3. P9-blocking test cases (ROADMAP §10)

| Test | Evidence | State |
|---|---|---|
| T18 save schema | `SaveGameTests` (older/newer/foreign refused, original kept) | covered headlessly |
| T31 inference impact on the whole game | P8A decider timings; the benchmark excludes fights (§5) | **open**: a fight against the bandit leader with the stats overlay on Windows (§4 item 7) |
| T37 non-ASCII paths and text | ids are separate from names; paths go through `std::filesystem` with UTF-8 from SDL | **open**: run the package from a folder with Vietnamese letters (§4 item 5) |
| T38 floating-point divergence | turn rules use integers (same results everywhere; W0 matched in WebAssembly); real-time rules use floats with no replay | real-time replay not built; accepted as a limitation for a single-player PC game (no cross-machine comparison needed) |
| T43 clean install without developer paths | T75 (`package.buildpath`, `package.path`), run-time asset lookup | **open**: §4 items 3–4 |
| T45 incomplete package | `package.content` / `package.policy` refuse a package missing required content; the game reports missing assets on screen | covered for packaging; there is no updater to test |
| T53 editor/training leak into the game | `Umbra.GameExcludesEditor`, `Umbra.PolicyDependencies`, `package.forbidden` | covered |
| T59 untrusted model package | policy loader checks (P8A); AI provenance warnings | covered |
| T65 non-commercial AI asset packaged | `material.license` (validate), `package.ai-license` | covered |

## 4. Windows checklist (open)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | `out\build\windows-msvc\apps\umbra\Release\Umbra.exe --benchmark 30` with nothing else running | three areas, then it quits; `%APPDATA%\Umbra\UmbraEngine\benchmark.json`. Send it: it names the GPU and decides O18 | **Passed** (user, 2026-11-08): §3b |
| 2 | `scripts\package.bat` | `PACKAGE OK`; the log shows `package-check` warnings only (AI leftovers in `assets\models` will be listed) | |
| 3 | Look in `out\package\Umbra` | `Umbra.exe`, DLLs, `assets\`, `shaders\`, `licenses\`, `THIRD_PARTY_NOTICES.md`; no `UmbraEditor`, `UmbraTool`, `.py`, `.bat` | |
| 4 | Copy `Umbra.zip` to another PC (or a user account) without Visual Studio, unzip, run | starts, offline (network unplugged) | |
| 5 | Unzip into `C:\Trò chơi\` and run | starts, saves work | |
| 6 | Package: talk to Mara, accept, fight (turn and real time), save at the stone, quit, start again, **Continue** | state restored in the area where you saved; quest continues | First try: restart began in the settlement (no load at launch) → `#UMBRA-65`; re-check |
| 7 | Package: fight the bandit leader with the stats overlay | frame times stay within the budget during the learned enemy's turns | |
| 8 | Package: `Umbra.exe --benchmark 60` twice | similar numbers; `peakMemoryMb` does not grow | 30 s + 60 s runs: same numbers, 249 → 250 MB (§3b) |

## 3b. Results from the user's PC (2026-11-08)

**Benchmark** (user ran items 1 and 8; files in this conversation, not committed). Reference PC:
NVIDIA GeForce RTX 3050 Laptop GPU, 16 CPU threads, 16 GB RAM, 1920×1080.

| Run | Area | Frames | Average ms (FPS) | p95 ms | p99 ms | Max ms | CPU work ms |
|---|---|---|---|---|---|---|---|
| 30 s | settlement | 8 629 | 3.48 (288) | 5.65 | 6.20 | 23.79 | 1.76 |
| 30 s | dungeon | 8 616 | 3.48 (287) | 5.65 | 6.08 | 47.57 | 1.77 |
| 30 s | arena | 8 641 | 3.47 (288) | 5.64 | 6.06 | 6.70 | 1.73 |
| 60 s | settlement | 17 281 | 3.47 (288) | 5.60 | 6.07 | 7.08 | 1.74 |
| 60 s | dungeon | 17 281 | 3.47 (288) | 5.60 | 6.08 | 11.57 | 1.75 |
| 60 s | arena | 17 281 | 3.47 (288) | 5.62 | 6.06 | 7.24 | 1.75 |

- Every area is far inside the proposed budget (average 4.8× under 16.67 ms, p99 5.4× under 33.3 ms).
- Peak memory 249 MB (30 s) and 250 MB (60 s): no growth between runs or across three area loads.
- The same ~288 FPS everywhere, with CPU work ~1.75 ms, means the frame rate is set by the 1 ms
  sleep per frame (Windows timer granularity) and presenting, not by the scenes; the GPU has room.
- Single slow frames (47.6 ms, 23.8 ms) appeared only in the first run: first-use costs right after
  the warm-up window; they did not repeat in the 60 s run.

**Play-through finding:** saving in the dungeon and restarting the game started fresh in the
settlement. The game had no way to load at launch (only from the death menu). Fixed in `#UMBRA-65`:
a start menu offers Continue (newest save), Load the other save, New game.

## 5. Limits, honestly

- The benchmark has no RPG layer (no fights, so the learned policy is not measured by it; item 7
  covers that by hand) and measures CPU frame time, which includes waiting for the GPU but does not
  separate GPU time.
- The program still contains source file names (assertion and crash locations, MSVC/vcpkg debug
  information); `package-check` reports how many. They reveal the project folder but are not used.
- Meshy model terms depend on the plan that made them (free plan: CC BY 4.0, needs credit); the
  notices say so but cannot check it.
- Budgets are proposals until the reference PC is measured (O18).
