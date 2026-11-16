# The Last Curtain: presentation and UX round — verification record

## Record

- Date: 2026-11-17
- Branch: `battle-royale`. Commits `issue #UMBRA-226` to `#UMBRA-231`.
- Tester: Claude (remote session), Linux cloud workspace; screens checked with `UmbraTool ui-preview`;
  the pass and fail sounds made on the user's PC (Stable Audio 3).
- State: **implemented**; the Windows play check (§4) is open.
- Asked by the user on 2026-11-16 (camera) and 2026-11-17 (items 2, 4, 6, 7, 8, 9 of their list; items
  1, 3, 5 and 10 come after the hook research, the user's order).

## What changed

| Commit | Item | Change | Tests |
|---|---|---|---|
| `#UMBRA-226` | camera | One spotlight (`Play::Spotlight`) for the top bar, turn order, ring, camera and full-size stage | T259 |
| `#UMBRA-227` | 4 | A bot's event: its card, then its roll (verdict in its name, pass/fail sound, stat popup); no repeated result | T260 (replaces T159), T152 |
| `#UMBRA-228` | 7 | Every death announced under the top bar: who, how, how many still alive | T261 |
| `#UMBRA-229` | 8 | A chat between bots: the request, the question and answer, what came of it | T262, T129 |
| `#UMBRA-230` | 9 | Every blow in sight told in words, a one-action fight included, before the fall | T263, T264 |
| `#UMBRA-231` | 2, 6 | A grouped turn menu (Ask, whom, what; Attack; Power; Back) and a quest book | T265, T266, T251 |

## 1. Automated checks

| Check | Result |
|---|---|
| Night suite (GCC) | 212/212 |
| `ui-preview` of `hud`, `blow`, `menu`, `book` with sample data | 0 errors (HUD warnings: no portrait files in the sample) |
| Full CTest under GCC, Clang, ASan + UBSan | 55/55 each (§1b) |

## 1b. Full runs

`/home/claude/verify.sh`: GCC 55/55, Clang 55/55, ASan + UBSan 55/55, no sanitizer report.
`Umbra.Night.Balance` (the soak) passes: the rules did not change in this round.

## 4. The user's check on Windows

On branch `battle-royale`:

1. Your turn: the camera and top portrait are on you at once; after a room resolves (Look around /
   End turn) they stay on you.
2. A bot walks into an event: its card on the right ("<name> faces it", its stat, Watch the roll);
   then its roll, with a chime or a thud; the change shows over the bot; nothing repeats after.
3. A death anywhere: "<name> is dead", the cause, and how many are still alive, under the top bar.
4. Two bots talk: the request, then the question word for word with the answer, then the roll saying
   whether it worked.
5. Fight a guest or walk into a monster: each blow in words ("You strike ... for 3 damage. ... has 5
   HP left."), a click goes on; a deadly first blow is shown before your fall.
6. The menu: Ask... → a guest → what to ask; Attack... → whom; Back.
7. The quest book: "Quests (N open)" opens it; Prev, Next, Close.
