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

A deed the player waits on (a walk into a room that does something, a fight, a chat, a power)
shows in steps, once the bot has walked in (the caption waits for the slide): first what it faces
(the room, its event and story, no result: a **preview**); after a tap, its roll on the dice screen
("Le Thanh rolls"); then the result, waiting again. A bot that only waits stays on the table. A
blow in a fight says who hit whom, for how much and the HP left, and puts the damage over the
struck guest for 2 s; an item used is named.

A die tumbles for `diceSeconds`, its face changing less and less often (14 × the square root of
the time gone), lands on its number, holds it 0.4 s, then tells the verdict and the result.

## Chats

After the player answers a question, the dice screen shows the asker's reply: a line from
captions.json `replyYes` or `replyNo` (alternatives split by `|`, one picked by the night's seed),
reworded by the built-in model in the asker's voice when it is ready (`LineKind::Reply`).

## The start screen (the user, 2026-11-15)

The guests are shown with their portraits. Clicking one (or "A random guest") does not start the
night: it opens that guest's page (`bio`): portrait, name, play style, the `bio` from `guests.csv`
(who they are and why they came; never their secret), stats and weapon. "Start the night" begins
it; "Back to the guests" (or Esc) returns to the list. `StartData(def, canContinue, chosen)` adds
the page's data under `bio` when a guest is chosen.
