# The Last Curtain: where things stand (2026-11-14)

For whoever continues this work, in any session. Read this first.

## State

- **Design: approved by the user** (2026-11-14). `game-design.md` (draft 8) and `cast.md` (draft 5).
  The decision log is section 21 of the design; ADR `../adr/0012-last-curtain.md` records the pivot
  from the living town.
- **Plan 1 (the night as a simulation): written, not yet reviewed by the user.**
  `plans/2026-11-14-night-simulation.md`, twelve tasks, commits `#UMBRA-104` to `#UMBRA-115`.
- **Nothing is built.** No code of `plugins/last-curtain` exists.
- Committed as `#UMBRA-103` (documents only).

## Waiting on the user

1. **Does the plan capture what they want?**
2. **The battle rules:** the plan gives the night its own small data-driven battle rules (Might and
   Sanity as two damage types, the starter attacks first, 3 rounds) because the engine's
   turn-combat rules have one Attack and one Defense, order by speed, no round limit, and a state
   too slow to copy for simulation. The design says "the engine's existing turn-based battles".
   Confirm or change.
3. **How to run the plan:** in one session, task by task (recommended), or with a fresh agent per task.

Do not start building before these are answered.

## The user's standing wishes for this game

- Cut any mechanic that serves no purpose or adds cost. Ask before adding one.
- All gameplay data in JSON or CSV under `plugins/last-curtain/assets/night/`; balancing must not
  need code changes. Only a new kind of mechanic may need code.
- Build the whole night as a simulation first and balance it there; the seven Taken powers must
  come out balanced against each other.
- The Saigon Opera House keeps its real name and history; only guests and events are invented.
- Use our own terms (guests, the Taken, the company, the Turn, omen, marked); no text, names,
  scenarios or art from the board game *Betrayal at House on the Hill*.
- Ask the user for any decision that is theirs; mark fill-ins as `[call]`.

## Still marked [call] in the design (numbers for the simulation to settle)

XP steps; guard as a battle option; where debuffs come from; how often relics appear; what each
omen grants and the curse targets; the win-rate band for balancing (40% to 60%); the Taken forms'
stats; the build phases.

## After plan 1

Plan 2: the board on screen (3D board, light and dark, camera on the acting guest, UI, the
player's turn, allies, the battle screen). Plan 3: the model's lines. Plan 4: art. Each is written
when the one before works.

## Other open items from before the pivot

- Windows checks not yet done by the user: `../testing/s24-storyteller.md` §4 (build with MSVC,
  `town-soak`), `../testing/s23-built-in-model.md` (the `warm-up` line of `town-mind`).
- The project document `claude/CONTEXT.md` on claude.ai still describes S25 as "the example town";
  `claude/CLAUDE.md` and this file are current.
