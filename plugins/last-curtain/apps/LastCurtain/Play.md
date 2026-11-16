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
shows once the bot has walked in (the caption waits for the slide). With an event roll: first the
event's card, as for the player, naming the bot and its stat ("Watch the roll"); then its roll on
the dice screen ("Le Thanh rolls"), which tells the verdict ("Le Thanh fails. -2 HP.") with a pass
or fail sound; Continue puts the change over the bot ("-2 HP") and the deed is done: no result
caption after it (user, 2026-11-17). Another roll (a fight, a chat): its caption, then the roll. A
chat between bots (user, 2026-11-17): the request ("Le Thanh asks Dinh Quoc Bao to spare them.",
told once); then the question word for word with the answer chosen ("Dinh Quoc Bao asks Le Thanh:
"..."" / "Le Thanh answers: ..."); then the roll, whose verdict says what came of it ("Le Thanh
fails. Dinh Quoc Bao is not convinced."). A
deed with no roll (an item found, a chat) is told once, result and all: one tap moves on.
A bot that only waits stays on the table. A
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

## Turns, the light and the hunt (the user, 2026-11-16)

- **Whose turn:** `Acting()` is the guest whose turn the screen shows: the player's while they rest
  after a room (the rules have moved on, the screen has not), else the rules'. The arrow and the
  top bar follow it.
- **The banner:** each new turn (round x 8 + turn) opens with a banner, `turnBannerSeconds`
  (options.csv). A bot waits for it; the player need not. The table gestures (the talk clip) and
  a gold ring grows under the figure (`TurnBanner().t`, 0 to 1).
- **The turn order:** the HUD's top right, the acting guest framed.
- **Plain steps go quicker:** a seen bot step where nothing happens holds `plainBeatSeconds`; a
  deed holds `beatSeconds`.
- **The light switch:** `light` in options.csv. Off, the player sees every opened room and every
  guest (`MakeView(..., light)`, `Narrate(..., light)`); the rules do not change. On, a bot that
  walks from the light into a dark room is seen leaving (`Beat::dark`) but what the room did is
  not told: no caption result, no dice, no moment.
- **Clicks:** only the right click inspects. A dialog's whole box takes a click (`action` on its
  root, `engine/ui/include/UI.md` 5). Taps that go on (next, onward, continue, roll, begin, take,
  leave) count once per `tapGap`, so spam clicks skip nothing.
- **The spotlight:** one function, `Play::Spotlight()`, says whose moment is on screen and which
  room to look at; the top bar's portrait, the turn order, the ring and the camera all read it, so
  they never disagree, and `StageCell()` (the full-size moment) is the spotlight's room when it is
  a moment (user, 2026-11-16: no camera logic mixed into the rules). In order: a death in sight
  (staged); a step sliding (that guest, where it goes); a deed that waits on the player (a room's
  screens, a roll, a tap: whoever did it, in its room, even when it ended the turn in the rules; a
  bot's roll too, not the player's room); a chat with the player, a battle in sight (staged); the
  player resting after a room, then the player's own turn (at once, not after the last bot's
  caption); a bot's caption still held; a bot thinking; the bot acting. A bot out of the player's
  light is named but not looked at. The rules' current guest is only the last fallback.
- **Blows (2026-11-17):** each blow of a fight in sight (or one the player gives or takes) is told
  in words on the `blow` screen, one after another, before anything else moves: "Phan Thi Mai
  strikes you for 4 damage. You have 2 HP left." Each shows `blowSeconds` (options.csv); a click
  goes on. A fight that starts and ends in one action (a monster striking first as the player walks
  in) is told too, once the pawn has walked in, and then the fall. The fight's room comes from its
  `Battle` entry and the monster from that room before or after the action, as an ended battle is
  wiped. The full fight view is planned with the user (item 3, 2026-11-17).
- **Deaths announced (2026-11-17):** every living guest's death, seen or not, shows under the top
  bar for 6 s: "<name> is dead", the cause, and how many are still alive (with the death sound).
- **Falls (battle royale, 2026-11-16):** every living guest's death the player sees (in a lit room,
  or their own death or kill) is staged on the `fall` screen in the room it happened (the `Died`
  entry keeps the room in `b`): the striker facing the fallen, and the cause (a blow and its damage,
  a power, the killer, or the house). The night waits until Continue.
- **The battle royale HUD:** the night number; the Quests panel (open quests with their reward, then
  the taken ones and who took them), the player's power and its cooldown; "YOU ARE A GHOST" once dead
  (the player plays on as a ghost); and "The killer is near. Run." when the killer is within
  `killer.hearSteps` rooms, with its steps heard (`killer` cue) when it moves. The killer stands on
  the table in the Taken's figure, dark red on a red ring, when its room is in sight; ghosts are
  their guest's figure, pale blue and glowing.

## Cutscenes

`assets/night/cutscenes.json` holds the scenes played on the real board: `opening` (a new
night, not a restored one), `midnight` (the killer wakes: the `Killer` entry with `b` 0), `dawn`
(the survivors' next night) and `ending_escaped` or `ending_house`. Each shot looks at a place (`board`: the opened room nearest the
middle; `foyer`; `player`; `killer`; `fall`), from a distance and turned by a yaw that both ease
(smoothstep) from the shot's start to its end, at a tilt, with a caption line and a clip for the
shot's figure. `Shot()` gives Main the camera (`TableCamera::SetShot`: the seat at `tilt`, turned
`yaw` degrees around the target); the HUD and labels hide. A click or Space shows the next shot,
Skip or Esc ends the scene; a scene holds the night like any dialog.

## Sounds

Play names cues as things happen (`TakeCues`): step (a slide), dice (a die cast as its screen
opens), door (a room opened), item, hit, death (down or dead), bell (the Turn), turn (a banner).
Main adds click for every UI action. `assets/sounds.csv` maps each cue to a file and a volume
(`Sounds`, on the engine's mixer); a missing file is silent and logged once. The sounds and their
prompts: `assets/audio/README.md`.
