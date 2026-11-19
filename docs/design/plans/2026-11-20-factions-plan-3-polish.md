# Faction matches, Plan 3: the look, the night's touches, and richer rooms and levels

The user's requests of 2026-11-20 02:40-03:02, after seeing the first match screens. Executed inline,
task by task, test first where there is a rule; checked by picture (SwiftShader) where it is a look.
Issues from #276. Tests from T342.

## Done (2026-11-20)

- #276 Pictures keep their shape (`fit`: contain, cover, top) — portraits never squish.
- #277 Shadows under figures; a cold horror grade that deepens toward the end (`match-look.json`; no
  red, the user); optional rooms are only marked.
- #278 Motions and sounds from each told line (T342).
- #279 Camera follow, the turn ring, opening and closing scenes.

## Queue (in this order)

1. **Spoken lines in bubbles** (#280): a short line beside the speaking character (not a dialog box)
   on a blow, a fall, a performance, a level, a hunter's attack. Written lines in `match-words.json`
   first, chosen by unit and round; the built-in model can replace them later. The bubble uses the
   ComfyUI bubble frame when it exists.
2. **Room view and board view, seamless zoom** (#281): the night's `TableCamera` room view; each
   square drawn as a room (floor, walls with doorways, a painted background by kind); battles in their
   room with the sides apart. No sky backdrop (the user).
3. **Event dice** (#282): the event card keeps its percent chance; two dice tumble and land on a
   total mapped to the chance, the target shown first (the user: "Keep percent, show 2d6"). Display
   only; the rules' roll decides.
4. **Treasure cards** (#283): opening an item room lays out 2-5 face-down cards (count rolled,
   mirrored for both halves); the character picks one: mostly items, also essence, XP or a small heal
   (the user). Data: `treasure.csv` (kind, item or amount, weight). Bots pick by value.
5. **Rooms respawn** (#284): a claimed event or item room comes back after N rounds (data) in a random
   free room of the same half, mirrored, never in the centre or on the stage; announced a round ahead
   like a spawn. Marked with a question mark (event) or a chest (item) icon. No traps (the user).
6. **Level-up cards** (#285): each level offers 3 cards (Vampire Survivors style): a new ability or
   board ability of the role, +1 to a stat, or an upgrade of a known ability (+2 power or -1 HP cost);
   all values in data (`levelcards.csv`). Bots pick by value. Replaces `jobs.csv` choices from level 2.
7. **ComfyUI art** (running on the user's PC): UI frames (panel, card, bubble, banner, button), a pick
   backdrop, and icons (attack, defence, HP, essence, level, monster, item, event, move, speed,
   spawn, stage), cut to 9-slice or icon PNGs, added as skin styles and `image` icons.
8. **The record** (#286): `docs/testing/s41-faction-match-screens.md` with the Windows checklist,
   status, roadmap; Clang and ASan suites; a final self-review.

## Rulings carried (ledger: `.superpowers/sdd/2026-11-19-factions-plan-2-screens/progress.md`)

- The same character on both sides shows the same name: the log will add the side ("Huy (theirs)")
  in #280.
- Shadows are opaque two-disc shapes: the sprite surface dithers faint alpha into noise.
