# The Last Curtain, plan 4a: the painted table — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-170` … `#UMBRA-173`, `#UMBRA-176` (the mood), `#UMBRA-182` (second tries' prompts), `#UMBRA-188` (the
  pictures and these records). Built in a cloud copy; delivered to the PC with `git am`. Design
  `docs/design/2026-11-15-art-rooms-battles.md`; plan `docs/design/plans/2026-11-15-painted-table-plan-4a.md`.
- Tester: Claude (remote session), Linux cloud workspace; pictures painted by ComfyUI (SDXL base) on
  the user's laptop (RTX 3050 4 GB, about a minute a picture), driven from the built-in browser.
  Captures with SwiftShader. MSVC, a real GPU and the user's eye have not checked this yet.
- State: **implemented**; the Windows look check (§4) is open.
- User decisions (2026-11-15): a painted tile for every room (62) and a picture for every event
  (33); feature rooms stand out (the foyer, the under-stage, the start rooms, the goal rooms);
  tiles painted over layout sketches; redo pictures freely; a free display face.

## What was built

| Part | Where |
|---|---|
| Prompts from the data, one per picture, with seeds | `#UMBRA-170`, `tools/art/make_prompts.py`, `tools/art/lastcurtain_prompts.json` |
| Pictures named by data id; featured cells (start rooms rebuilt from the seed) | `#UMBRA-171`, `Art.{hpp,cpp,md}` |
| Painted tiles in kind-coloured frames, gold for featured rooms; monster cards; item tokens; the view names rooms, monster kinds and floor items | `#UMBRA-172`, `Table.cpp`, `BoardView` |
| Portraits (HUD, chat), event pictures, Abril Fatface titles | `#UMBRA-173`, `Play::UiData`, `hud`, `chat`, `event`, `skin.json` |
| The night's mood (user, midday): a creep from 19:00 (post grade drains, colder, darker corners, the lamp flickers), at the Turn a snap to a blood colour table, a breathing red light, drips at the top of the screen and the `dread` surface shader's stains spreading over the floors; the UI stays clean | `#UMBRA-176`, `NightMood` in `Art.cpp`, `assets/shaders/dread.surface.hlsl` (plugin), `tools/art/make_mood_art.py`, `dread` screen |
| The pictures, redone subtly uncanny (user): 62 rooms, 33 events, 7 portraits, 19 items, 10 monster and minion cards, 3 UI pieces | `#UMBRA-188`, `apps/LastCurtain/assets/art/` (`README.md` there); second tries in `tools/art/lastcurtain_redos.json` |

## 1. Automated checks

| Check | Result |
|---|---|
| T140 featured cells (foyer, under-stage, goal rooms, start rooms, same after a save); `ArtPath`; `MissingArt` | pass (RED first) |
| T141 every picture the data needs is in `assets/art` (`MissingArt` empty) | pass (failed until the pictures were cut in) |
| T142 the view's floor items and monster kinds | pass (RED first) |
| T143 the event's picture, the asker's and the player's portraits in `UiData` | pass (RED first) |
| T145 the mood: plain at round 1, the full creep before midnight with no blood, blood from the Turn growing to dawn | pass (RED first) |
| `dread.surface.hlsl` compiles for Vulkan (DXC + `spirv-val`) and converts to WGSL (naga 30) | pass |

## 2. Pictures

- Reviewed on contact sheets on the user's PC: the first pass of all 134 was usable but 14 were
  tried again (the auditorium and loose stairs drawn from the side; the armoury, pit cover and
  fencing hall too bloody; three events with garbled poster text; six items that were not the
  object, or had blood or a landscape). The second tries were all used.
- Monster cards are kept whole (keying the cream background out made holes in them); items keep
  the keyed edge.
- `LastCurtain --seed 7 --player huy --capture x.png --frames 60` (SwiftShader): painted tiles in
  their frames, gold for the foyer and start rooms, the portrait in the HUD; with `--look` the
  armoury at full size with its painting and the room beside it.

## 4. The user's check on Windows

1. Run `LastCurtain.exe`: lit rooms are paintings in their kind's colour frame; the foyer, your
   start room, the other start rooms and goal rooms have gold frames; rooms out of your light are dim.
2. Monsters stand as framed painted cards; items lie on the floor as small tokens.
3. The HUD and chats show portraits; events show their picture; titles use the new face.
4. The mood over a night: does the creep from 19:00 feel uncanny without hiding the board? At the
   Turn, does the snap to blood (grade, breathing light, drips, stains spreading) feel bone-chilling?
   Is any of it too much to play under?
5. Which pictures should be redone (the art README lists the ones already redone)?
