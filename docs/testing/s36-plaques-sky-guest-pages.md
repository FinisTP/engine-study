# The Last Curtain: floor plaques, the Saigon sky, guest pages — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-211` (guest pages), `#UMBRA-212` (plaques and sky), `#UMBRA-213` (these records).
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader.
- State: **implemented**; the Windows play check (§4) is open.
- The user's request (21:42) and answers: room names as a **brass plaque** set flat into the floor;
  a **Saigon night sky**, dusk turning to night before midnight and a red moon in storm clouds
  after; the start screen shows portraits and a guest's page (biography, Start, Back) before the
  night begins. Decided here (the user may change them): the biographies in `guests.csv` `bio` are
  drawn from `docs/design/cast.md` (age, career, why they came, talent, quirk, personality; never
  the traitor plot) and are fill-ins for the user to rewrite; "A random guest" also opens the
  chosen guest's page; the sky is a painted backdrop square to the camera, not a dome (the camera
  looks down, so a dome would show mostly its lower half; `Table.md` "Plaques and sky").

## What was built

| Item | Where |
|---|---|
| Guest pages: portraits on the start screen; a click opens `bio` (portrait, style, biography, stats, weapon) with Start the night and Back to the guests (or Esc) | `#UMBRA-211`, T212 |
| Brass plaques: `tools/art/make_plaques.py` draws 62 (one per room); `Table::Plaque` lays them flat near each room's south edge on the table and in the room view; the screen-space room names are gone | `#UMBRA-212`, T213 |
| The sky: two ComfyUI pictures (`art/sky/before.jpg`, `after.jpg`; prompts in `art/README.md`); `Table::Sky` hangs the one for the hour behind the board in both views | `#UMBRA-212`, T214 |

## 1. Automated checks

| Check | Result |
|---|---|
| T212 (start data carries portrait and bio; a chosen guest adds the page) and T213/T214 (`MissingArt` wants the plaques and both skies; the asset test) written first and seen failing | pass |
| Night suite: 230 cases | pass (GCC) |
| Full CTest under GCC, Clang, ASan + UBSan | see §1b |

## 1b. Full runs

`/home/claude/verify.sh` at the final tree: GCC 55/55, Clang 55/55, ASan + UBSan 55/55 with no sanitizer report.

## 2. Pictures (SwiftShader; not a GPU check)

`--seed 7 --player huy`: the board with the dusk sky behind it and a plaque on every tile; `--look`:
the room view with STAGE ARMOURY lying on the floor, readable; the same with the after-midnight sky
forced for one picture (red moon, crimson clouds). `UmbraTool ui-preview start` and `bio`: portraits
in the list; Nhi's page.

## 4. The user's check on Windows

1. Start screen: every guest has a portrait. Click one: their page opens, the night does not start.
   Back returns to the list; Esc does too. Start the night begins it as that guest. "A random
   guest" opens a random guest's page.
2. On the board and in the room view: each opened room's name lies on its floor as a gold-on-black
   plaque near the near edge, readable from the camera, dimmer out of the light. No floating room
   names remain.
3. Behind the board: a dusk sky over Saigon until the Turn; after it, the red moon and storm
   clouds. Drag and zoom: the sky stays behind everything and fills the view (on a screen wider
   than 2.4:1 its edges may show).
