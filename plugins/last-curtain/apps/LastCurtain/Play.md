# Play: how a turn is shown

`Play` is the night as the person plays it, without a window. The rules (`modules/night`) decide
everything at once; `Play` shows it one step at a time and holds the bots while it does.

## The room's flow (the user, 2026-11-15)

When the player walks into a room that does something, the rules have already resolved it.
`Play::Do` reads what happened from the new log entries and shows it in order, each step waiting
for the player:

1. **Arrival** (`arrive`): the room's kind (EVENT, TRAP, ITEM, BONUS ROOM), its name and one line.
2. **The card** (`event`): the event drawn, or the lasting one armed there (`RoomEvent`). A lasting
   one says it stays until someone passes its roll; one whose condition the player meets says so,
   and there is no die. Otherwise: the stat, the target, the chance, what a pass and a fail do.
   **Or the find** (`found`): an item room's item, its picture and what it does.
3. **The die** (`dice`): it tumbles for `diceSeconds` (options.csv; the face flickers, nothing is
   told), then lands: the sum, passed or failed, and the result in plain words (captions.json
   `pass<Stat>` / `fail<Stat>`, then the effect, and the HP left). The HUD shows an HP change for
   2 s.
4. **Rest** (`rest`): the rules ended the turn, but the player may still Look around the room
   before End turn; the bots wait.

With `moments` on, each step shows the room at full size with the eight around it (`StageCell`).

## Bots

A bot's beat the player can see is captioned. A beat where something is done (an item, a roll, a
fight, a fall, a chat, a clearing) waits for Continue with `tapToContinue` and is shown at full
size with `moments`; the caption names the event the bot faced. A walk alone stays on the table.
Skip runs the bots to the player's turn with no waits.

Before a deed the player will see (a walk into a room that does something, a fight, a chat, a
power), the bot's action is tried on a copy of the night (the rules are deterministic, so the copy
comes out the same): if it is a deed, its caption is shown first as a **preview** (what the bot
faces: the room, its event and story, no result) and waits for a tap; then the action is applied
and its result shown. A blow in a fight says who hit whom, for how much and the HP left, and puts
the damage over the struck guest for 2 s; an item used is named.

## Chats

After the player answers a question, the dice screen shows the asker's reply: a line from
captions.json `replyYes` or `replyNo` (alternatives split by `|`, one picked by the night's seed),
reworded by the built-in model in the asker's voice when it is ready (`LineKind::Reply`).
