# The Last Curtain: where things stand (2026-11-14)

For whoever continues this work, in any session. Read this first.

## State

- **Design: approved by the user** (2026-11-14). `game-design.md` (draft 8) and `cast.md` (draft 5).
  The decision log is section 21 of the design; ADR `../adr/0012-last-curtain.md` records the pivot
  from the living town.
- **Plan 1 (the night as a simulation): implemented 2026-11-14**, `#UMBRA-104` to `#UMBRA-115`
  (`plans/2026-11-14-night-simulation.md`). `plugins/last-curtain`: rules (`modules/night`,
  `Night.md`), bots and soak test (`modules/nightbots`, `Balance.md`), tools `UmbraTool night-check`,
  `night-run`, `night-soak`. Linux GCC: every test passes and the first balance pass holds. The
  Windows check and two balance points for the user are in `../testing/s25-night-simulation.md`.
- The decisions taken while building are listed in that record and in the session's report.
- **Revision `#UMBRA-116` (user, 2026-11-14 evening):** no hidden marked guest; a failed curse
  roll (plain d20, target rising with omens and hours) turns that guest at once and time jumps to
  midnight; no fights between guests before midnight; the Taken's own minions after midnight,
  each taking a share of its HP. Record: §5 of `../testing/s25-night-simulation.md`.

## Decided by the user (2026-11-14)

1. The plan captures what they want.
2. The night has its own small data-driven battle rules (plan 2 reuses the engine's battle screen).
3. The plan ran in one session, task by task.

## Waiting on the user

1. The Windows check again for `#UMBRA-116` (`../testing/s25-night-simulation.md` §4, §5).
2. Level 5 is still out of reach (about 3 level-ups a night for the whole table).
3. Go-ahead to write plan 2 (the board on screen).

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
