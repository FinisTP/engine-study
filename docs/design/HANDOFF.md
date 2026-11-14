# The Last Curtain: where things stand (2026-11-15, after plan 2b)

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
- **Plan 2 (the board on screen): implemented 2026-11-14**, `#UMBRA-121` to `#UMBRA-131`
  (design `2026-11-14-board-on-screen.md`, plan `plans/2026-11-14-board-on-screen-plan.md`). Push back
  and the conversation rule in the rules; the `board` module (what the player sees, the narrator,
  picking); `LastCurtain.exe` in `plugins/last-curtain/apps/LastCurtain` (`README.md`): table view,
  the player's turn, paced bot turns with captions, chat, event, dice, omen, battle, Turn and end
  screens, start screen, saves and Continue. Linux GCC/Clang/ASan 55/55 and SwiftShader pictures.
  Record and the Windows play checklist: `../testing/s26-board-on-screen.md`.
- **Plan 2b (goals and a bigger board): implemented 2026-11-15**, `#UMBRA-137` to `#UMBRA-145`
  (design `2026-11-14-goals-and-bigger-board.md`, plan `plans/2026-11-14-goals-plan-2b.md`). Omens
  are gone; each guest draws one of its goals (`assets/night/goals.csv`); at midnight the Taken is
  drawn from those who failed and the other failed drop to 1 HP; all done means the company wins with
  no hunt. A 9×9 board of 70 rooms with seven start rooms; a free step back without pacing; chats and
  groups between any guests; bots that work toward goals. The soak checks every band
  (`rules.json` "balance") and holds; the tuning steps are in `Balance.md`. Record and the Windows
  checklist: `../testing/s27-goals-and-board.md`.
- **Next: plan 2c, the screen** (the same design): a turn menu, walking clicks one step at a time,
  a Stay/Back confirm for new rooms, right-click to inspect, input by state, sliding pawns, a
  following camera with drag and Focus, numbered group rings, walls wherever a guest cannot pass.

## Decided by the user (2026-11-14)

1. The plan captures what they want.
2. The night has its own small data-driven battle rules (plan 2 drew its own battle screen).
3. The plan ran in one session, task by task.

## Decided by the user (2026-11-15)

- Room goals were too easy (95%): goal rooms are now dealt within the first 65 rooms; keepsake rooms
  stay within 35.

## Waiting on the user

1. MSVC warnings in `plugins/last-curtain` (not yet reported).
2. The Windows check and a first play of plan 2 (`../testing/s26-board-on-screen.md` §4), and
   whether chatty bots asking in round 1 feels like too much (§3).
3. The Windows check of plan 2b (`../testing/s27-goals-and-board.md` §4) and the fill-ins marked
   [call] there (six new events, the keepsakes' rooms, speeds, Khang distrusting Lan).

Plan 1 is accepted by the user (2026-11-14): MSVC tests and `night-soak` match Linux.

Plan 2 decisions (user, 2026-11-14): a tabletop view first; a separate program; the conversation
rule (5 questions per guest, two answers each with its own target, 3 approvals over separate turns);
End turn at any time; events roll one fixed stat and may push the guest back; battles in the
real-size room come in a later plan.

Decided: level 5 out of reach in a night is intended (levels build character, not min-maxing).
The user lets Claude edit and commit in the checkout directly (2026-11-14).

## The user's standing wishes for this game

- Cut any mechanic that serves no purpose or adds cost. Ask before adding one.
- All gameplay data in JSON or CSV under `plugins/last-curtain/assets/night/`; balancing must not
  need code changes. Only a new kind of mechanic may need code.
- Build the whole night as a simulation first and balance it there; the seven Taken powers must
  come out balanced against each other.
- The Saigon Opera House keeps its real name and history; only guests and events are invented.
- Use our own terms (guests, the Taken, the company, the Turn, goals, marked); no text, names,
  scenarios or art from the board game *Betrayal at House on the Hill*.
- Ask the user for any decision that is theirs; mark fill-ins as `[call]`.

## Still marked [call] in the design (numbers for the simulation to settle)

XP steps; guard as a battle option; where debuffs come from; how often relics appear; what each
omen grants and the curse targets; the win-rate band for balancing (40% to 60%); the Taken forms'
stats; the build phases.

## Next

Plan 3: the model's lines (the captions and chats worded by the built-in model). Plan 4: art,
walking the rooms and battles in the real-size room. Each is written when the one before works.

## Other open items from before the pivot

- Windows checks not yet done by the user: `../testing/s24-storyteller.md` §4 (build with MSVC,
  `town-soak`), `../testing/s23-built-in-model.md` (the `warm-up` line of `town-mind`).
- The project document `claude/CONTEXT.md` on claude.ai still describes S25 as "the example town";
  `claude/CLAUDE.md` and this file are current.
