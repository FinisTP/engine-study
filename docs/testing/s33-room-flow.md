# The Last Curtain: the room's flow, rest, the eight rooms around, clicks — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-183` (clicks), `#UMBRA-184` (bots' walks), `#UMBRA-185` (rest),
  `#UMBRA-186` (the eight rooms around), `#UMBRA-187` (the room's flow).
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader and `ui-preview`.
- State: **implemented**; the Windows play check (§4) is open.
- The user's feedback (16:31): bots' walks need no full room view, only their deeds; the player
  must be able to look around the room just entered; the full view shows the 8 rooms around;
  Continue in the full view did not take clicks; Focus did nothing; rooms, items and events were
  not explained (a trap's dice appeared with nothing before them).
- User decisions: after a room, only Look around and End turn (the rules are unchanged); the centre
  room fills the view, the neighbours show around it, the wheel pulls back; the room plays step by
  step, each step waiting for the player.

## What was built

| Item | Where |
|---|---|
| A click goes to the topmost screen with a choice under it; a dimmed screen blocks those below (Continue, Skip and Focus beside a menu; it was the UI's rule that only the top modal screen took clicks) | `#UMBRA-183`, `engine/ui` (`UI.md` rule 4) |
| A bot's walk stays on the table; only a deed opens the room and waits | `#UMBRA-184`, `Play::Notable` |
| Rest: Look around / End turn after a room resolved; the bots wait | `#UMBRA-185`, `rest` screen |
| The stage draws the 3×3 around the room: back walls 1.2 m, unopened rooms dark, unlit ones dim, guests and monster cards in the neighbours; the wheel pulls the seat back | `#UMBRA-186`, `Table::StageRoom` (`Table.md`) |
| The room's flow: arrival (kind, room, a line) → the card (lasting note, or "your Speed 6 carries you through", or the roll and odds) or the find → a die that tumbles for `diceSeconds` → the result in plain words with the HP left; the HUD flashes an HP change; bots' captions name the event they faced | `#UMBRA-187`, `Play.md`, `arrive`, `event`, `found`, `dice`, `caption` screens, captions `pass<Stat>`/`fail<Stat>` |

## 1. Automated checks

| Check | Result |
|---|---|
| UI: a HUD button takes a click beside a modal menu; a dimmed dialog blocks it | pass (RED first: the HUD got nothing) |
| T150: every bot beat that waits is a deed, never a plain step | pass (RED first) |
| T151: after a room resolves, the rest menu; Look shows the room; End turn lets the bots go | pass (RED first) |
| T152: a lasting trap plays arrival → card (it stays) → tumbling die (no verdict, no Continue) → plain result → rest; a met condition clears with no die; an item room shows the find; a bot's roll names its event | pass (RED first: dice came straight after the move) |

## 2. Pictures

`--look`: the room at full size with the opened room beside it past the lowered back wall.
`ui-preview` of `event` (a lasting trap: its note, the roll in words, the odds) and `arrive`.

## 4. The user's check on Windows

1. On a bot's turn: walks stay on the table; an item, roll, fight or chat opens the room and waits.
   Continue, Skip and Focus all take clicks.
2. Walk into an event, trap or item room: the arrival banner, then the card (or the find), Roll,
   the die tumbling about a second, the result in words, the HP flash, then Look around / End turn.
3. Walk back into a lasting trap: its card says it is still there.
4. The full view shows the rooms around; the wheel pulls back to see them all.
