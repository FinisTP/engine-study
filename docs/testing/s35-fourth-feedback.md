# The Last Curtain: the fourth round of feedback (17 items) — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-203` … `#UMBRA-209`.
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader.
- State: **implemented**; the Windows play check (§4) is open.
- The user's feedback (20:27), 17 items; decisions taken here (the user may change them): the
  Taken's stories in `guests.csv` `takenStory` are drawn from `docs/design/cast.md`'s Traitor forms
  (fill-ins for the user to rewrite); "leave it" puts a found item on the floor of its room.

## What was built

| Items | Where |
|---|---|
| 1: the top bar stays the player's during the rest after a room; 12: the camera goes to the player at once on their turn; 17: a guest at 0 HP is *down* (lying down, standing at midnight with 1 HP): now said in the result and as YOU ARE DOWN in the HUD | `#UMBRA-203`, T164 |
| 3: Done in the walk picker; 9: Leave it beside Take it (`LeaveItem`); 11: a bot that only waits stays on the table | `#UMBRA-204`, T165 |
| 2, 4, 5: a bot walks into the room first, then its caption names what it faces, then its roll on the dice ("Le Thanh rolls"), then the result; dice slow to a stop, hold their number 0.4 s, then tell | `#UMBRA-205`, T159, T166 |
| 14: after midnight the Taken knows where everyone is and everyone knows where the Taken is; 15: bots never walk back into a room in the same turn (`Guest::visited`); save version 5; rebalanced (The Folded Girl HP 40, line damage 11, The Opera Specter HP 26) | `#UMBRA-206`, T167; soak holds on seeds 1, 20001, 80001 |
| 13: the Turn's screen tells who the Taken was and what it has become | `#UMBRA-207`, T168 |
| 6: each figure's portrait and name over its arrow; 7: room names at the near edge of rooms in light, the player's goal rooms glow; 8: whole 1 m walls in the room view, with gaps only where a guest can pass; 16: a drag in the room view follows the view's slant | `#UMBRA-208`, T169, T170 |
| 10: a chat with the player shows its room from the moment it opens | `#UMBRA-209`, T171 |

## 1. Automated checks

| Check | Result |
|---|---|
| T164-T171 (written first and seen failing, except T171 and the camera's T169 partly) | pass |
| Night suite: 227 cases | pass (GCC) |
| Soak, 1,000 nights on seeds 1, 20001, 80001 | holds |
| Full CTest under GCC, Clang, ASan + UBSan | 55/55 each (§1b) |

## 1b. Full runs

`/home/claude/verify.sh` at `#UMBRA-209`: GCC 55/55, Clang 55/55, ASan + UBSan 55/55 with no sanitizer report.

## 4. The user's check on Windows

1. On a bot's turn in your light: it walks in first; then what it faces; then its die tumbling to a
   stop; then the result. A bot that waits stays on the table.
2. Your turn: the camera comes to you at once; the top bar stays yours while you look around after
   a room; Done leaves the walk picker; Leave it puts a found item down.
3. Names and portraits over everyone; room names; your goal room glows; walls whole in the room
   view; a drag there slides along the view.
4. After midnight: the Taken shows wherever it is; bots head for it and no longer pace.
5. The Turn screen tells the Taken's story (rewrite the stories in `guests.csv` as you like).
6. Knocked to 0 HP: the result says you are down, the HUD says so.
