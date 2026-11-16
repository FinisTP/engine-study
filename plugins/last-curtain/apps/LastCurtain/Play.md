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
("Le Thanh rolls"); then the result, waiting again. A deed with no roll (an item found, a chat)
is told once, result and all: one tap moves on (user, 2026-11-16). A bot that only waits stays on the table. A
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
- **Who is acting on screen:** while a deed is still shown (a roll, a caption waiting for a tap, a
  held beat, a step sliding, a room's screens), the top bar, turn order, the ring and the camera
  stay on the guest who did it (`Play::Presenting`), even when that deed ended the turn in the
  rules; the next guest takes over when it is done (user, 2026-11-16).
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
