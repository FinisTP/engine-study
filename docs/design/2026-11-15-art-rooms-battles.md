# The Last Curtain: art, rooms at full size and real-size battles (plan 4)

> **Status:** Approved by the user 2026-11-15 (open questions answered); 4a ready, not started
> **Author:** the user + Claude · **Last updated:** 2026-11-15
> **Quick reference** — Layer: presentation over the night rules · Key deps: `LastCurtain.exe` (plans 2-2c), the pawn figures (`#UMBRA-159`), the art resource plan (Claude Doc), the night's battle rules (`game-design.md` §10)

Marks: **[call]** = Claude's fill-in for the user to change. "User" = decided by the user.

## Summary

The night stays a board game, but it stops looking like coloured boxes. Plan 4 has three parts, each
its own plan and each playable on its own: **4a** the painted table (the art), **4b** walking a
room at full size, **4c** battles in that room. The rules do not change: everything the player sees
at full size is still one board action.

## Decided by the user (2026-11-14 and 15)

- Plan 4 covers art, walking the rooms at full size and battles in the real-size room.
- Art: 1960s Saigon golden-age poster style. 3D only for the guests (one model for all seven, tinted
  by colour, for now) and one shared Taken (tinted in its guest's colour). 2D for room tiles,
  walls, items, props, monsters, portraits and UI. Props are always flat painted (stage flats).
  Painted top-down room tiles. Mostly free and local (ComfyUI on the user's laptop, Meshy Pro).
- One event picture for now.
- Guest colours from the 1960s palette, matched to their story (`#UMBRA-158`); the user confirmed
  the seven colours fit the cast.
- The style board's direction is right (2026-11-15).
- A room at full size uses a fixed corner camera, like a stage.
- A room opens at full size only for Look; battles (the player's and bots' in the player's light)
  play at full size; events stay on the table.
- A free display font.
- Every room in `rooms.csv` (62) gets its own painted tile, and every event (33) its own picture
  (user, 2026-11-15: generation is quick). Feature rooms still stand out: the foyer, the
  under-stage, the seven start rooms and the rooms named in goals get a gold frame and a brighter,
  stronger painting.
- Top-down tiles are painted by ComfyUI over a simple layout sketch drawn by a script
  (image-to-image), since SDXL alone paints rooms in perspective.
- The user leaves the laptop running: all of 4a, 4b and 4c are built without waiting; pictures are
  redone as often as needed (up to 4 tries each), the weak ones listed.
- The look must be uncanny and frightening, not just retro (user, 2026-11-15 midday): the pictures
  are **subtly uncanny** (deep shadows, small wrong details, still readable in the evening), and
  **shaders** carry the night's shift: from 19:00 a **gradual creep** (colours drain, shadows and
  vignette deepen, faint flicker and grain grow), then at the Turn (midnight) a **snap** to
  bloody and bone-chilling: a blood-red grade, red vignette with **drips at the screen edges** and
  dark red **stains spreading on the floors**, a pulse; no gore in the pictures. The effects reach
  the table and the stage; the **UI stays clean**.

## Why

The pawns now move like people (`#UMBRA-159`), but they stand on grey and purple slabs. The house
is pillar 2 ("the building is the board"): it should look like the Saigon Opera House in 1960s
paint, and a fight should look like a fight, not a battle panel over a board.

## Goals, non-goals, anti-goals

- **Goals:** the table looks like a painted model of the house; a room the player enters can be
  seen at full size; a battle plays in that room with the figures' attack, hit and death clips.
- **Non-goals:** new rules; free roaming (the board's moves stay the moves); a second floor; per-guest
  3D models (later, once the shared one has proven the feel); voiced lines.
- **Anti-goals:** a full-size view that lets the player do something the board would not; loading
  pauses between table and room; art that copies *Betrayal at House on the Hill*; the real
  Opera House's look changed beyond its 1960s paint.

## 4a. The painted table

| Today | Plan 4a |
|---|---|
| Room: a coloured box | a painted top-down tile for every room (62), on a thin painted base; feature rooms in a gold frame |
| Wall stub: dark box | a painted wall strip (the room's wall colour with a moulding line) |
| Doorway bar | a painted door on the floor edge |
| Monster: brown or violet box | a standing painted card (a flat cut-out on a little base, like the props) |
| Items in a room: not shown | a small painted token lying on the tile |
| UI: flat panels | the poster frame (style board sb06) as the panel skin, the cream paper colour, a free display face (user) for titles: "Abril Fatface" [call; SIL OFL, its licence beside it], Poppins stays for body text |
| Portraits: none | one painted portrait per guest in the HUD, chat and inspect screens (7) |
| Event picture: none | a picture for every event (33) in the event dialog |

Tiles are 1024 px squares from ComfyUI (SDXL), cut and colour-checked by a small script
(`tools/art/`), stored as JPEG under `apps/LastCurtain/assets/art/`. Each room keeps its kind's
colour as a border, so the board still reads at a glance (the colour coding of plan 2 is not lost).

Counts for 4a: 62 room tiles, 4 wall/door pieces, the monster and minion cards, an item token per
item, 7 portraits, 33 event pictures, a UI skin of 6-8 pieces: about 140 pictures.

## 4b. A room at full size

- **When:** only when the player clicks "Look" on the turn menu in their own room (user). Events
  stay on the table; battles open their room as 4c says. The camera flies down from the table into the room (about 1 s) and back up
  when done. No loading screen: the room is built from the same pieces as its tile.
- **What it is:** the painted floor tile scaled to a 8 × 8 m floor, the walls as tall painted flats
  with doorways where the board has doors, a few prop flats from the room's list (`rooms.csv` gains a
  `props` column [call]), the room's light. The guests in the room stand at full size (the same
  figures, now 1.7 m).
- **What the player can do there:** the same actions as on the table (the turn menu stays), plus
  walking their figure around the room by clicking the floor, to look (no rule effect) [call]. Leaving through a doorway is
  the board's Move and costs a move, as on the table.
- **Camera:** fixed in a high front corner, like a seat in the stalls looking at a stage (user). It
  never goes behind the flats, so they always face the camera. Height and angle are the builder's.

## 4c. Battles in the room

- A battle that involves the player, or that bots fight in the player's light (user), is shown in
  its room at full size, with the same fixed corner camera: the two sides face each other, each attack plays `attack` (`power` for a Taken's
  power), the target plays `hit`, a guest at 0 HP plays `death`, damage numbers float up.
- The battle rules and menu are the existing ones (attack, item, guard, leave after round 1; at most
  3 rounds). Only the presentation moves from the battle panel to the room.
- Monsters are painted cards that tilt toward their target as their "attack" and flash when hit
  (no 3D monsters in plan 4).
- Bots' battles out of sight stay instant, as today.

## Approaches (Claude's recommendation: A)

- **A. 4a, then 4b, then 4c (recommended).** The art lands first on the screen the player already
  uses; 4b reuses those pieces at full size; 4c needs 4b's room.
- B. 4b first with box rooms, art after. Proves the full-size feel sooner, but the room would be
  built twice.
- C. Skip 4b's free walking; show rooms at full size only for events and battles. Smaller; the
  room never becomes a place the player looks around.

## Testing

- 4a: the table capture with tiles (SwiftShader), `ui-preview` of the reskinned screens, the
  asset test (every room kind has a tile, every guest a portrait, every file loads).
- 4b: headless tests that a room's layout matches the board (doorways where the board has doors),
  that walking in the room changes no `NightState`, and that leaving costs exactly one Move.
- 4c: headless tests that the room battle plays the same `Action`s and ends in the same state as the
  panel battle; capture of each clip.
- The user's Windows play check for each part.

## Answers (user, 2026-11-15)

1. The style board's direction is very good.
2. The camera in a full-size room stays in a fixed corner, like a stage.
3. Rooms open at full size only for Look (battles still play at full size, 4c).
4. Bots' battles in the player's light also play at full size.
5. A free display face for titles.
6. The seven guest colours fit the cast.

## What is left to the builder

Tile cutting and colour matching, the exact flat heights, the fly-down camera path, damage-number
style.
