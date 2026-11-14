# The Last Curtain, plan 2: the board on screen — design

Date: 2026-11-14. Status: **written, waiting for the user's approval.** Builds on plan 1 (the night
as a simulation, `plugins/last-curtain`, `#UMBRA-104`-`#UMBRA-118`) and the game design
(`game-design.md`, `cast.md`). Plan 3 (the model's lines) and plan 4 (art) come after.

## Intent

Make the night playable by a person: one guest of the seven, on screen, against six bots, from
19:00 to the end of the hunt. The user's words: "think of it like a tactical turn-based RPG", a
tabletop first. Everything the rules decide stays in plan 1's `LegalActions` / `Apply`; this plan
shows the night and turns clicks and keys into actions. It also adds one new rule the user asked
for: winning guests over by talking.

## Goals

1. A tilted overhead **tabletop** view of the board: rooms as tiles, guests as pawns.
2. **Light and dark**: the player sees what their guest's and their allies' light reaches (`Lit`);
   other opened rooms show only their shape.
3. **The player's turn**: move by clicking a room or with the arrow keys, items, omens, chat, end
   the turn at any time (even with moves or actions left).
4. **Allies** are the player's to move on their own turns.
5. **Bot turns** are readable: the camera goes to the bot, a one-line caption says its decision,
   the pawn slides; about 1.5 s, faster with Space. Outside the light only a new room appears.
6. **Conversation** (new rule, user 2026-11-14): see below.
7. **Battles** in a panel over the board.
8. A **start screen** (choose a guest or random) and an **end screen**; save on quit, Continue.
9. **Concept mockups** of the main screens (layout, not painted art).

## Non-goals

- Model-written lines (plan 3): captions and chat lines come from templates and data for now.
- Real art (plan 4): rooms and pawns are placeholder shapes from code.
- Walking inside full-size rooms, and battles in the full-size room: a later plan.
- Web and Android builds; the editor.

## Anti-goals (would make it a failure even if it works)

- Showing anything the player's light does not reach (who is where, minions, the Taken).
- Bot turns that are slow or unreadable (six of every seven turns are theirs).
- Rules written in the screen code: every rule stays in `modules/night`.
- Touching the reference RPG or `Umbra.exe`.

## Constraints

- Short, simple code; step comments; a markdown file beside code for concepts (CLAUDE.md).
- All gameplay numbers and texts in `plugins/last-curtain/assets/night/`; screens in
  `assets/ui/*.ui.json`.
- Windows/PC first; Vulkan renderer; headless rules and bots must still build without a window.
- The soak test must still hold after the conversation rule.

## Approaches considered

**How it looks** (user's pick: A).
- **A. Tabletop**: an overhead, tilted view of a miniature board; pawns step room to room;
  click a room or doorway. Closest to the rules, cheap on art, bot turns easy to read.
- B. Walking inside: full-size rooms, third-person walking that snaps room to room. More
  atmosphere, far more art and camera work, bot turns harder to follow. Kept as a later polish.

**Where it runs** (user's pick: A).
- **A. Its own program `LastCurtain.exe` in the plugin**, on the engine's window, renderer and
  screen system. The RPG stays untouched; about 200 lines of start-up and loop are written again.
- B. Its own project run by `Umbra.exe`: needs `Umbra.exe` loosened from the RPG plugins first
  (a sizeable refactor of `Main.cpp` and `GameFlow`).
- C. A mode inside the RPG's start menu: quickest, but two games tangled in one program.

## The conversation rule (user, 2026-11-14)

Replaces plan 1's single persuasion roll.

- **Chatting**: the player's guest, in the same room as another guest (not the Taken), may chat
  with them. The chat **uses up the turn**. Each guest has **5 questions** in their pool
  (`questions.csv`); a chat draws one at random (not one already asked of this player tonight,
  while any remain).
- **A question has two answers**: one needs a **physical roll** (d20 + Might), the other a
  **magical roll** (d20 + Sanity). Each answer has its **own target** (easy 10 / normal 13 / hard
  16), so the cost of an answer is its difficulty. The guest's bond toward the player still moves
  the target (`persuade.bond`, easier if they like you, harder if not).
- **A passed roll is one approval.** A failed roll gives nothing (no other cost). **Three
  approvals**, necessarily on three separate turns, and the guest joins the player's group (at
  most two allies, as before). Approvals are kept per guest for the night.
- **Guests speak at the start of their turn**: a bot in the player's room may open a chat itself
  at the start of its turn; it asks the player one of its questions the same way, and a passed
  answer counts the same. It does not use the player's turn. Whether a bot opens a chat is the
  bot's choice (its style and its bond toward the player).
- Only the player recruits; bots never chat with each other (GDD 15, 16).

Data: `questions.csv` with `guest,id,text,physicalText,physicalTarget,magicalText,magicalTarget`
(targets as `easy|normal|hard`); `rules.json` `persuade` becomes `{approvals 3, bond 3}` and loses
`target` and `retryRounds`. The 35 questions are written in each guest's voice from `cast.md`, for
the user to review; no text from the board game.

## The design (the how)

### Parts

```
plugins/last-curtain/
  modules/night/       + the conversation rule (Chat, Answer actions; approvals; questions.csv)
  modules/nightbots/   + a bot's choice to open a chat; soak strategy "guest 0 recruits"
  modules/board/       NEW, headless: what the player may see and the board's layout in metres
  app/                 NEW: LastCurtain.exe — window, renderer, input, camera, screens, the loop
  assets/ui/           NEW: start, hud, actions, chat, battle, end screens; skin
```

- **`board` module (headless, tested):** turns a `NightState` and the player's guest into a
  *view*: which cells are lit, which are only known, which pawns, monsters and the Taken are
  visible, doorways, and for each cell a position in metres. Also the bot-turn script: from the
  log entries of one turn, the steps to animate (camera target, caption, pawn path). No renderer:
  the rules of what is shown are testable like the night.
- **`app/` (`LastCurtain.exe`):** the window, the Vulkan renderer, `engine/ui` screens, input.
  Placeholder meshes made in code (a floor tile, a wall stub, a pawn, a doorway arrow), coloured
  by room kind and guest. Mouse picking: a ray from the camera to the board plane gives the cell.
  The camera eases between the acting guest's cell and a board overview; mouse wheel zooms.
- **Turn flow:** the player's guest and allies: wait for input, `Apply` it. Bots: `ChooseAction`
  on a worker-free loop step, then play the board module's script for that turn (camera, caption,
  slide), then the next. A battle opens the battle screen; its options are `LegalActions`.

### Screens (`assets/ui/`)

| Screen | Shows | Actions |
|---|---|---|
| `start` | the seven guests (name, stats, style), Random, Continue | pick, start |
| `hud` | clock and round; the acting guest; the player's HP, Might, Sanity, Speed, level, buffs; moves left; bag | bag, end turn |
| `actions` | the legal actions for the current guest, besides moving | each action; End turn always |
| `chat` | the guest's question, the two answers with their stat and target, approvals so far | answer |
| `omen` | the omen and its gift; the curse target | take, leave |
| `battle` | both sides with HP and buffs, round of 3, the log | strike (target), item, guard, leave |
| `caption` | the acting bot's decision line | none |
| `end` | the outcome and a short summary from the log | new night, quit |

### Look (placeholder)

Room kinds by colour (foyer gold, plain grey, item blue, event violet, trap red, omen black,
transport teal, bonus green, under-stage deep red); lit rooms full colour, known rooms dark grey,
doorways a faint arrow; pawns coloured per guest with a ring for the player and allies. One warm
key light over the board and a dim ambient; the hunt turns the ambient red.

### Saves

On quit (and at the start of each round) the night is saved with plan 1's `SaveNight` into the
user's saves folder; Continue on the start screen loads it. A save from other data is refused
(plan 1), and Continue is then hidden.

## Concept mockups

Four layout mockups (board view on the player's turn, a bot's turn caption, a chat, a battle
panel) on an editable design canvas the user owns ("The Last Curtain — board screens (concept
mockups)" on claude.ai). They fix the layout and the information on each screen, not the final
art; the chat question in the mockup is a sample, not one of the 35.

## Tests and checks

- `board` module: lit and known cells, nothing outside the light, positions, bot-turn scripts
  from logs (headless, in `Umbra.Night` or a new `Umbra.Board`).
- Conversation rule: approvals across turns, three make an ally, targets per answer and bond, a
  bot-opened chat counts, no chat with the Taken, at most two allies; soak still holds.
- `LastCurtain.exe`: builds on Linux (cloud) and is captured with SwiftShader (`--capture`); the
  real check is the user's on Windows: play a night start to end.

## Left to the builder

The exact camera angles and speeds; mesh sizes; how the caption text is built from a decision;
the bot's chat-opening threshold (a `styles.csv` column); the order of the plan's tasks.
