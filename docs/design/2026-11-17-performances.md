# The Last Curtain: the performances — design

2026-11-17. Replaces the battle royale's goal (`2026-11-16-battle-royale.md`) on branch
`battle-royale`; the co-op game stays on `main`. The research behind it:
`2026-11-17-hook-research.md`. Calls I made that the user has not ruled on are marked **[call]**.

## Intent

The battle royale's goal, outlive everyone, gave players no reason to explore or level up
(the user, 2026-11-17). The new goal tells the player at every moment what to do next.

**The fiction.** The seven guests are the souls of past performers, trapped in the Saigon Opera
House. Tonight the house stages a programme for an unseen audience. Each act, every performer faces a
specter of the opera alone on the stage. The audience's Applause is all that counts: at dawn, the
best performer walks out (the user's ideas 1, 3, 5 and 6, and a light part of 2).

**The loop**, repeated three to five times a night:

1. **The programme** shows the next act one act ahead: which specter, and the props its script calls
   for ("Act II, the Duel: the Specter of the Prompter. Bring a blade and a mask.").
2. **Between acts**, guests roam the house to find those props, and other things that make them
   stronger. Props are scarce, so rivals race and fight for them. The killer and monsters roam; nobody
   can play safe.
3. **At curtain**, every living guest is called to the stage and faces the specter alone, in a
   turn-based duel. Applause for surviving, for damage, and for each script prop used.
4. **At dawn**, after the finale, the most Applause wins.

**Three things a new player learns:** Applause (the score), the script (what to bring), the
performance (the duel).

## Goals, non-goals, anti-goals

- **Goals:** the player always knows what to look for and why; exploring, items, levels and fights
  all feed the performance; each act is a set piece worth watching; one night, one winner.
- **Non-goals:** a new board, new rooms or a new art style; multiplayer; hidden roles.
- **Anti-goals:** a long list of concepts to remember; a request a bot wins by luck of position; an
  accidental death that ends the evening; the night depending on the AI model.

## The rules

### The night and the acts

- One night, 20:00 to dawn, 12 rounds of 45 minutes, as now.
- **Acts** at fixed rounds, in data (`rules.json` `acts`): **[call]** four acts, after rounds 3, 6, 9
  and 12, the last being the finale at dawn.
- Each act's **script** comes from `scripts.csv`: its specter, its props (two by default), its text,
  and its Applause values. The next act is shown from the start of the previous one, so there are
  always about three rounds to prepare.
- The **dawn rollover** to a second night is removed: survivors no longer play on.

### Applause

- The score, per guest, shown in the turn order bar beside each portrait.
- Earned from the performance (most of it), and a little from the board: a first visit to a room,
  an event passed, a monster beaten (values in `rules.json`, small). **[call]**
- **Killing a guest takes half their Applause** (the user).
- The winner at dawn is the guest with the most Applause; a tie goes to the guest alive at dawn, then
  the higher level. **[call]**

### Props

- A prop is an item with a `prop` flag in `items.csv` (a blade, a mask, a lantern, a shawl, the
  crown...). Most props also keep an ordinary use (a blade still adds Might in a fight).
- When an act is announced, each prop its script calls for is laid in one or two rooms (`copies` in
  `items.csv`), so nobody picks up a later act's props by chance in round 1. Props are carried like items (the carry limit holds), dropped on death,
  can be asked for ("Ask X for an item"), and taken from the floor.
- The programme panel shows the next act's props and whether you hold them.

### The performance (the duel)

- At curtain the board pauses. Every living guest (not ghosts) performs in turn order, alone against
  the act's specter, on the **duel view** (the user's item 3): only the stage, both sides' HP and
  stats, each side acting in turn.
- **[call]** A duel lasts three exchanges. Before each, the specter **shows its next move** as an
  icon: a heavy blow, a light blow, or a taunt (the evaluation, 2026-11-17). The guest then picks
  one: **Strike** (Might roll; best against a light blow), **Hold** (Sanity roll to halve the blow;
  best against a heavy one), or **Use a prop** (counts for the script, does what the prop does, and
  earns a bonus when used on the move its script names, such as the mask on the taunt). The
  specter then acts.
- Applause: a base for finishing the duel standing, plus the damage dealt, plus a bonus for each
  script prop used. All values per script, in `scripts.csv`.
- **[call]** A duel is not lethal: a guest whose HP runs out is **booed off** (no further Applause
  that act, HP left at 1). Deaths stay on the board, from rivals, the killer and traps, so the acts
  remain a stage, not a meat grinder.
- Bots perform too, by simple rules; each bot duel is one result line, shown together on the review
  (no waiting through bot duels: the evaluation's top gap).
- After each act, the **review**: everyone's Applause for the act, ranked, in a short screen. Words by
  the built-in model when present (a critic's lines), written lines otherwise.

### Death and the Dressing room

- A guest killed on the board becomes a ghost where they fell (the user). They drop their items; the
  killer takes half their Applause.
- A ghost walks the board (it cannot attack or be attacked **[call]**, to keep ghosts from deciding
  who wins) and comes back to life by reaching the **Dressing room**, or at once with a rare item.
  Revived with half HP.
- A ghost does not perform: dying just before an act costs that act's Applause.

### The killer and monsters (kept)

- The killer still wakes at midnight and kills in the rooms it enters; it never enters the stage and
  pauses during acts. Monsters still appear by the hour.

### Persuading (kept, aimed at props)

- The three requests stay: spare me, an item (now the way to get a prop from a rival), go after
  someone. The question and answer stay.

## Keep, change, remove

| Area | Keep | Change | Remove |
|---|---|---|---|
| Board | Whole map at the start, 45 rooms, spawns far apart, light | Props laid by the night's scripts | |
| Clock | 12 rounds, 45 minutes | Acts at fixed rounds (data) | Dawn rollover to another night |
| Goal | | Most Applause at dawn | Last guest alive wins; house win |
| Rooms, events, traps | All | Small Applause for a first visit, an event passed | |
| Items | All | `prop` and `copies` columns; props drop on death | |
| Fights between guests | Attack, battle, blows told | A kill takes half the victim's Applause | |
| Killer, monsters | All | Killer pauses during acts | |
| Ghosts | Ghost where you fall | Revive at the Dressing room or with an item; no attacking | Ghosts playing for the house; respawn at the under-stage |
| Quests and powers | | **[call]** quests become act scripts; powers move onto rare props | `quests.csv`, the quest book, guest powers |
| Persuading | Requests, questions, natures | "An item" is how to bargain for a prop | |
| Screens | Spotlight, menu groups, blows, obituary, events | Quest book becomes the **programme** (next act, its specter, its props, yours ticked); turn order shows Applause | Quest panel |
| New screens | | **Duel view** (item 3), act **review** | |
| Bots | Natures, truces, killer fear | Score props for the next act highest; perform duels by simple rules | Quest goals |
| AI model | Lines for chats | Specter taunts and the critic's review, with written fallbacks | |
| Saves | | Version 7: Applause, act, prop flags | |
| Cast | The 7 guests, their stats and natures | A roster of 10 (3 new); guests per night in `rules.json` | |
| Pawns | The 3D board, the Taken model | Guests drawn as 2D sprites (Huy's for all until proven) | `guest.glb` for guests |

## Art and motion (plan D, the user, 2026-11-17)

There is no budget for a 3D model and animations per character, so the performances use **no new
models and no new animation clips**. Characters become 2D sprites:

- **One painted picture per guest**, cut out of its poster (birefnet in ComfyUI). The portraits
  are head-and-shoulders now; full-body pictures are a later choice.
- **Expressions by LivePortrait** (ComfyUI-AdvancedLivePortrait, on the user's PC): it moves the
  painted face instead of repainting it, so the guest stays the same person. Per guest: calm,
  angry, near-dead, happy, scheming, afraid, and a talking (mouth open) frame. Settings in
  `tools/art/lastcurtain_expressions.json`. Tested on Khang and Thanh: all read; a heavily painted
  face (Khang) needs the stronger settings.
- **Actions by engine motion** on the flat sprite, no frames: fight = lean in and slam, squash on
  the hit; dodge = skew and side hop; flee = flip and dart off; walk = bob; talk = swap the mouth
  frame; death = dissolve. Pallor and anger flush are tints, not new pictures.
- **Specters** are pictures too (one each), shown with the same motion. The duel view is a 2D
  screen: stage backdrop, both sprites, HP, stats, the specter's move icon.
- The act's opening shot reuses the camera cutscenes (`cutscenes.json`) and the existing Taken
  model; bot duels are one result line each, shown together on the review screen.

**The template (the user, 2026-11-18).** Hoang Gia Huy's sprite (`art/sprites/huy`, made as in
`art/README.md` "Sprites") stands in for every guest, tinted by guest colour, through the whole
implementation stage. The other guests are drawn only once the motion works in the game.

**The cast becomes a roster of 10** (the user, 2026-11-18), one per life stage and gender, and the
number of guests in a night is a setting in `rules.json` (default 7, as now). The 7 current guests
keep their slots: Nhi (girl), Thanh (young woman), Lan (woman), Mai (old woman), Huy (young man),
Khang (man), Bao (old man). New: a boy, a teen boy, a teen girl. **[call]**: the user did not rule on
this mapping or on Huy's name (kept as in `guests.csv`).

New art in total: 10 bodies and portraits × 7 faces, 4 specter pictures, 1 stage backdrop, 3 move
icons.
Rejected: SDXL repainting the face (a different person each time, expressions weak); Qwen-Image-Edit
and Wan video (too big for the user's 4 GB graphics card); a Meshy model per guest (credits).

## Data (all tunable without code)

- `rules.json` `acts`: the rounds of each act, Applause values for the board, the half-on-kill share.
- `scripts.csv`: id, name, specter, props (ids), text, base, perDamage, perProp.
- `specters.csv`: id, name, hp, might, sanity, the blows it deals, its taunt lines.
- `items.csv`: `prop` (0/1), `copies`.
- Art: one picture per specter (the existing pipeline), the stage backdrop for the duel view.

## Balance targets (the soak)

- The winner's lead over second place is less than one act's Applause in most games (no runaway
  leader). **[call]**
- Every guest uses at least one script prop in most acts; a guest who explores more scores more.
- Each nature wins sometimes (no nature under 8%). **[call]**
- Deaths still happen (at least one a night), but most Applause comes from the acts.

## What it builds on

The duel view is the user's item 3. Rerolls (item 5) fit inside the duel later (for example, one
reroll per act). The stage API for tutorials (item 1) comes after, so the five tutorial stages can be
built on this game: walking and props, a duel, a rival, a death and the Dressing room, a full act.

## Left to the builder

The duel's exact numbers, the review's layout, how bots choose between props and fights, the
specters' and scripts' content, and the order of work, all inside the targets above.
