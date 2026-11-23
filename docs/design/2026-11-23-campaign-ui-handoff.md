# The Last Curtain: campaign UI and the design changes of 2026-11-23 evening — handoff

For the development session: everything decided on the evening of 2026-11-23 (after the slice
handoff, `docs/design/2026-11-23-campaign-slice-1-handoff.md`, was approved at 14:03), in one place,
with what to review and the order to build it. **Where this file and the slice handoff disagree, this
file is newer and wins**; the differences are listed in section 2.

---

## 0. Start here

1. Read the repository's `CLAUDE.md` and `docs/CONTEXT.md`, then the slice handoff.
2. Read this file, then the UX docs in section 3 (start with `docs/design/ux/interaction-patterns.md`).
3. Open the mockups (section 4). They show the look and layout; the specs are the source of truth
   for behaviour.
4. **Review first, then build** (section 6): check the specs against the code and the slice design,
   report anything that can't be built as written, then plan one step at a time with the
   superpowers:writing-plans skill, as the slice handoff describes.

**State of the files:** the UX docs in `docs/design/ux/` and this file are on the PC **uncommitted**.
Commit them first (next free number after #333), using the usual `Temp/lcship` route.

---

## 1. What this adds

- A complete UI design for the campaign: screen map, shared input scheme (mouse + keyboard and
  gamepad), and a spec for every screen.
- Game design changes made while designing it: the café brews memories again (with ingredients and
  keepsakes), the café is the hub, a world map with skirmishes, a notebook, biding folded into ending
  the turn, per-stage party size.

---

## 2. Design changes since the slice handoff (decided by the user)

| # | Change | Replaces in the slice handoff |
|---|---|---|
| 1 | **The café is the hub.** Continue lands in the café; its door opens the world map; Back on the map returns to the café. | the flow's "between stages the player picks the next stage, a replay or the café" |
| 2 | **World map:** a painted 2D floor plan of the theatre; rooms are next stage, cleared (replay), skirmish, unknown "?". The café is not a room on it. | the stage list screen |
| 3 | **Skirmishes:** a limited number become available after each stage (data per stage), signalled on the map; each reuses a past map or uses a new small one; **XP from them is capped at the next stage's level cap**; drops are that map's ingredients (random). Losing one keeps nothing and doesn't use it up. | new |
| 4 | **Brewing is back, as memory drinks.** Each recruit's memories return through drinks; **what you serve chooses which memory comes back**, never right or wrong. A memory drink also unlocks an **upgrade version** of one of the recruit's abilities (two versions per upgrade; the choice is final). | "Drink serving is off for now"; "each visit restores two fragments"; "a new visit opens after a won fight" |
| 5 | **Ingredients and keepsakes.** Ingredients drop from monsters (**random**) or lie in rooms; keepsakes are unique, story items in fixed rooms (take-once rule). Both live in the **café's storage**, never in the 4 battle slots. | new |
| 6 | **Item flags:** every item says where it can be used: café, board, fight (a list in data). Café-only items can't go into a battle slot. | new |
| 7 | **General drinks:** an ingredient not spent on a recruit can make a general boost (e.g. ginger water: +2 max HP for anyone), **capped per character** (proposed 3 per stat). **In slice 1.** | "permanent boosts" listed as not in the slice |
| 8 | **Abilities are unlocked by level; ability upgrades by drinks and keepsakes.** | new |
| 9 | **Visitors** (spirits met in fights) have short optional talks that can fill the notebook with hints (where a keepsake or ingredient is). Lệ Thanh teaches the recipes. | guests in the slice were only Bảo and Bà Mai (they stay; visitors are added as content allows) |
| 10 | **Biding is folded into "Kết thúc lượt".** Ending the turn **without having moved or acted** gives 1 AP (cap 2); after a move or action it gives none; the button says which ("Kết thúc lượt +1 AP"). There is no separate Bide command. | "New action `Bide`" (the rule stays; only the command goes) |
| 11 | **Party size is set per stage** in the stage file. | the slice's fixed party |
| 12 | **The fight screen stays as built** (its own view and backdrop); it only gets Vietnamese words, AP costs and the keys. | — |

**For slice 1 specifically:** 2 ingredients (one from a room, one random rat drop), 1 keepsake in
Stage 1, Tí's *Đưa tin* upgrade with its two versions, 1 general drink, the notebook with its three
tabs, the world map, the café hub. Tí's second memory comes from a drink or keepsake, not from a
replay, which also closes the slice handoff's open "Tí's second visit" item.

---

## 3. The UX documents (`docs/design/ux/`, copies in the Project under `claude/last-curtain/`)

| File | What it covers |
|---|---|
| `interaction-patterns.md` | **Read first.** Screen map, layering rules, the input map (P1) and 17 shared patterns, gaps in the engine |
| `hud.md` | The battle board: goals card, order strip with AP pips, command menu beside the unit (Kỹ năng ▸, Vật phẩm ▸), target preview on the target, unit cards on hover, toasts, hint line |
| `fight-screen.md` | Kept as built; the few changes |
| `world-map.md` | The map, room tickets, skirmishes, the selected-room panel |
| `stage-flow.md` | Pre-stage (who fights), stage result, game over with Lễ cầu hồn |
| `cafe.md` | The calm café (hub), talking, brewing (memory and general drinks) |
| `notebook.md` | Sổ tay: Characters (skills nested, upgrade versions, bag, memories), Ingredients, Keepsakes |
| `menus.md` | Title, new game, pause (no Save; Give up in a stage), settings (large text, TV effects, speeds) |

**Input decisions (the user):** mouse + keyboard and gamepad both complete; Xbox glyphs only; right
click cancels; Tab / View opens the notebook from anywhere (read-only and the fight paused in
battle); edge-of-screen panning only in full screen; a large-text option (body 16 → 20 px).

---

## 4. Mockups

**Canvas:** https://claude.ai/artifact/JFnQALDHDUJQVnVjmzeukz ("The Last Curtain — campaign UI
mockups"; private to the user: ask them to share it if your session can't open it). Screens at
1280×720, Vietnamese text:

| Row | Artboards |
|---|---|
| Places | World map · Battle board, command menu (Kỹ năng open) · Battle board, targeting |
| Café and notebook | Café, walking · Café, brewing for Tí · Notebook, Characters · Notebook, Ingredients (in battle, read-only) |
| Menus and flow | Title · New game · Pre-stage · Stage result · Game over with the rite · Settings |

**Visual direction:** Vietnamese lacquerware (sơn mài): red-black lacquer grounds (#170c0a, panels
#24120f), gold-leaf lines and accents (#d4a94a), eggshell text (#f0e5cc), ally pearl blue (#8fb7cf,
round rings), enemy cinnabar (#d65f47, square rings). Fonts as decided: Be Vietnam Pro (body),
Playfair Display (titles). One bold element: rooms on the world map drawn as torn theatre tickets.
These are mockup values; the game's `skin.json` and `match-look.json` hold the real ones.

Mockup text in brackets ("[Đoạn ký ức do bạn viết]") is **the user's to write**; numbers and
ability effects in the mockups are placeholders, not balance.

---

## 5. What the engine and app need (from `interaction-patterns.md` section 4, plus the specs)

- **Input layer:** a board cursor for keys and gamepad (P4); the back stack (P2); keys moved from
  code into per-device key files; device-aware prompts with Xbox glyph art.
- **UI:** a toast queue; menus and sub-menus placed beside a world position (the command menu, unit
  cards, the target bubble); an on-screen keyboard with Vietnamese tone marks for the gamepad;
  a large-text skin swapped at once.
- **Words:** every label in today's screens ("Your side", "Forecast", "Log"…) into the words files.
- **Café:** remove the shift HUD (money, day, clock, cup, orders, patience bars); name plates only
  near the walker; the brewing overlay.
- **New rules and data:** ingredients and keepsakes (drops, room items, café storage), recipes,
  upgrade versions per ability, general drinks with caps, item use flags, skirmishes (count per stage,
  XP cap, map), per-stage party size, biding through Kết thúc lượt.

---

## 6. Suggested order (each its own plan; fold into the slice's plans 2–7 where they overlap)

1. **Commit the docs** (this file and `docs/design/ux/`).
2. **Review:** check each spec against the code and the slice design; list anything unbuildable,
   anything missing, and any conflict. Bring questions to the user before planning.
3. **Rules additions** (with slice plan 2): biding through Kết thúc lượt; per-stage party size;
   item use flags; random drops.
4. **Input foundation:** key files, device detection and prompts, the board cursor, the back stack.
5. **Words and large text:** move labels to the words files (Vietnamese default); the large skin.
6. **Battle HUD** (with slice plan 7): goals card, order strip with AP pips, command menu with
   sub-menus, target preview, unit cards, toasts, hint line; the fight screen's small changes.
7. **Notebook** (Characters, Ingredients, Keepsakes), read-only in battle.
8. **Café hub and brewing** (with slice plan 6): calm HUD, talks, memory drinks, upgrade versions,
   general drinks, the café storage.
9. **World map, skirmishes, pre-stage, stage result, game over** (with slice plan 5).
10. **Title, new game, pause, settings.**

Checks: each spec's acceptance criteria become tests where headless (focus order, back stack, menu
contents, caps, read-only notebook, biding rule) and capture checks in the cloud (SwiftShader,
`UmbraTool ui-preview`) for layout at 1280×720 in both text sizes. **Windows and gamepad play checks
are the user's.**

---

## 7. Still the user's to decide or write

- Tí's memory texts, recipe names, ingredients and keepsakes for the slice (content).
- The general boost caps' exact numbers (proposed 3 per stat per character).
- Skirmish counts per stage and their maps.
- Art: Xbox glyphs, ingredient and keepsake icons, the ticket shapes for the world map, the
  theatre floor plan.
- Not decided (only proposed): cutting recruits from ten to about six to deepen each one's memories
  (`claude/last-curtain/tactics-hooks-research-2026-11-23.md` and the evening's discussion).

## 8. Related documents

- `docs/design/2026-11-23-campaign-slice-1-handoff.md` and `2026-11-23-campaign-slice-1.md`
- Project: `claude/last-curtain/slice-1-eval-2026-11-23.md` (the evaluation),
  `claude/last-curtain/tactics-hooks-research-2026-11-23.md` (why memory-as-growth; no quizzes in the
  café), `claude/last-curtain/slice-1-changes-2026-11-23.md`.
