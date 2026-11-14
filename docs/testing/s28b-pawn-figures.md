# The Last Curtain: guest colours and pawn figures — verification record

## Record

- Date: 2026-11-15 (overnight, the user away)
- Commits: `issue #UMBRA-158` (guest colours), `#UMBRA-159` (the figures), `#UMBRA-160` (these
  records and the plan 3 and 4 drafts). Built in a cloud copy; delivered to the PC with `git am`.
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, GCC ASan+UBSan);
  pictures with SwiftShader under `xvfb-run`. MSVC, a real GPU and a person watching the figures
  move have not run this yet.
- State: **implemented**; the Windows look check (§4) is open.
- User decisions (2026-11-15): one model for all seven guests (Hoang Gia Huy's), differing by
  colour; one shared Taken model that follows its guest's colour; seven colours from the 1960s
  palette matched to the story; up to 300 Meshy credits overnight.

## What was built

| Part | Where |
|---|---|
| `color` column in `guests.csv` (RRGGBB): Nhi jade, Thanh lacquer red, Bao cream-grey, Khang mustard, Mai ochre, Huy ink blue, Lan rose [call] | `#UMBRA-158`, `NightData.cpp` |
| The guest model and the Taken model, rigged and packed with 7 and 6 clips | `#UMBRA-159`, `apps/LastCurtain/assets/models/` (`README.md` there) |
| Their animation graphs (idle, walk, attack, hit, death, plus talk / power) | `assets/animations/guest.anim.json`, `taken.anim.json` |
| Pawns drawn as tinted skinned figures; a sliding pawn faces its step and walks; a turned guest becomes the Taken model in its colour; a model that fails to load falls back to the box | `Table.{hpp,cpp}`, `Table.md` §2 |

Meshy: 134 credits used of the 300 allowed (926 → 792): two concepts, two image-to-3d, two remesh,
two rigs, 12 library clips (two of them to replace the first idle, a wide fighting stance, with
"Idle 1"; the Taken keeps the fighting stance).

## 1. Automated checks

| Check | Command | Result |
|---|---|---|
| T130 colours load; a bad colour is refused with `guests.csv:1:color` | `UmbraNightTests "[data]"` | pass (RED first: no `color` field) |
| T131 both models load and every clip their graphs play exists; idle, then walk with speed | `UmbraNightTests "[pawn]"` | pass (shown failing when a graph names a missing clip) |
| Full suites | `/home/claude/verify.sh` (GCC, Clang, GCC ASan+UBSan) | GCC 55/55, Clang 55/55, GCC ASan+UBSan 55/55 |

## 2. Pictures (SwiftShader)

`LastCurtain --seed 7 --player huy --capture x.png --frames 40`, with the camera moved in to 5 m for
the picture (not committed): the figures stand in a calm idle, tinted, facing the camera; the
Taken model (drawn for Thanh by a temporary patch) is taller, masked, in red. A walking figure was
not caught in a still.


## 4. The user's check on Windows

1. Build and run `LastCurtain.exe`; start as any guest.
2. The seven pawns are figures in their colours, standing idle; the colours are told apart at the
   default zoom, also in the red hunt light.
3. Bots walk: the figure faces each step and plays its walk; it is back to idle when it stops.
4. After the Turn the Taken is the tall masked figure in its guest's colour.
5. Does the shared model give the feel you wanted, before per-guest models are made?
6. Frame time with seven skinned figures (the stutter check of S23 is separate).
