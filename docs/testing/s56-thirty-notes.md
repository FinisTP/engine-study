# The Last Curtain: the user's 30 notes (2026-11-24 23:57), Ký ức, slice 1 at 90% — verification record

## Record

- Date: 2026-11-25.
- Requests:
  - the user's 30 notes of 2026-11-24 23:57;
  - Ký ức as a life by year (00:11);
  - slice 1 wins at least 90%, the ramp after it (01:35);
  - no balance runs or rule tests unless asked (02:11).
- Rulings the user answered:
  - weapons carry signature skills;
  - board abilities get two uses a fight, battle abilities keep their HP cost;
  - choices are remembered as small echoes;
  - quests are a tracker plus markers only.
- Commits:
  - `#UMBRA-437`: quick fixes, camera, board panels, Ký ức.
  - `#UMBRA-438`: spoils, board uses, rallying, quests, gear, the player's voice, battle themes, slice 1 at 90%.
- Tester: Claude (remote session). Linux cloud workspace, GCC 13 Debug, SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows.

## 1. What was done

| Note | What |
|---|---|
| 1 | Settings from the title keep the foyer painting behind them. |
| 2 | The worn tape (VHS) starts off. The screen save is now version 2; a version 1 file turns the tape off once. |
| 3 | A man's default name is Hoàng. The placeholder follows the gender. |
| 4 | Sequences close in on a fighter, then pull back out (`"on": "all"`); every sequence ends that way. |
| 5 | A stage opens about 10 tiles across (`stageTilesAcross`). Figures are 0.8 × 3 × 0.625 m at that seat and never shrink when zooming in (`figures.base`). Walls rise only nearer than that. |
| 6 | The battle view shows 10 tiles; each side stands a quarter in from its edge. |
| 7 | Back with nothing to cancel hides the action panel; Back again brings it back. A click on oneself or one's portrait brings it too. |
| 8 | A click on another unit shows its card (skills, items, Move) and its reach. Every order portrait can be clicked. |
| 9 | Armed at a square holding two units: a picker asks which one. |
| 10 | Tutorial enemies keep only close abilities (`meleeEnemies`). |
| 11 | The Keeper first draws breath on round 2 (`breath.first`), with Bảo's warning. |
| 12 | Tí's rope runs from his neck to the Rat as a sagging bead line. |
| 13 | A stage's `spoils` are given on its first win. T2 gives the chè's ingredients. Lệ Thanh's lesson after T2 teaches the chè and opens the counter. Mai's first memory upgrades Tend. |
| 14, 20 | A stage's `leaders` act first: Bảo in T2s; in T2 Mai heals, you step beside the ghost, then Mai joins you. Battle picks follow the guided step. |
| 15 | A dropped item shows its icon; hovering names it. |
| 16 | In a campaign fight a costly board ability is free, twice a fight (`Mode.boardUses`, the abilities.csv `uses` column). Its first line plays on the battle stage. |
| 17 | Each kind of fight has its own theme (tutorial, skirmish, zodiac), and a bass-heavy cut in the battle view. They are made by `tools/audio/campaign-audio.bat` (needs ffmpeg). Until then the old fight theme plays. |
| 18 | The action always takes the camera back. Bots wait `turnPause` / `battleTurnPause` at a new turn. |
| 19 | Rút khỏi trận (Withdraw) ends a battle. It is never a bot's choice. |
| 20 | Mai's Đồng lòng: +1 Defence on her square and next to it. A passive that works shows a card at the top left. |
| 21 | Every ability class has its words. Cơm nguội says it revives. |
| 23 | A foe that can never rise leaves no body. |
| 24 | Dread now dims a sprite instead of turning it white. |
| 25 | `quests.json`: one line at a time in the café, with a mark over the target; the next stage is marked on the map. |
| 26 | The "?" was MSVC turning `\u` escapes into the local code page. C++ text now uses UTF-8 bytes. |
| 27 | Fight or stay is chosen only before a stage, not in the notebook. |
| 28 | T1–T3 and the café's first talk are rewritten for the player. The choices are saved as facts (`player.t1`, `player.trust`, `player.home`, `player.t2`, `player.rite`) and answered later by lines with `when`. Choices made in a stage's scenes now reach the save. |
| 29 | Each café person has default lines that rotate visit by visit (`idle` talks). |
| 30 | Gear in `skills.json`: a weapon brings its skill (Dao → Rạch, Dao mổ, Ná → Ná cao su); armour and accessories add numbers. Gear is worn and taken off in the notebook. An old coat hides in T2s. |
| 00:11 | Ký ức: each character's life by year (Bảo, Mai, Tí). A year's words stay "???" until its skill opens. |
| 01:35 | Every slice 1 stage and refill wins 90% or more of bot runs. Campaign enemies never idle; a boss holding a recruit's rope waits. Retuned: enemy levels, the Rat (attack −3), Stage 1 (level 4, one window), the Keeper's breath (6). |

**Not done:**
- **Note 22**, Octopath / Triangle Strategy polish (for example depth of field): not done.
- **Music:** the themes still need generating on the PC.
- **Art:** the gear and mung-bean icons are not painted; the names show alone.

## 2. Automated

- `ctest -E Balance -j3`: 60/60.
- `UmbraCampaignTests "[campaign-balance]"`: every slice 1 band is at [0.9, 1.0] and holds.
- The faction match's balance was not run: the user moved it to its own task.

## 3. Captures (SwiftShader)

- `captures/s56-stage1.png`: before the walls were lowered.
- `captures/s56-t2.png`.
- `captures/s56-cafe.png`: the café sample, with the quest line.

## 4. Windows check (open)

1. New game as a man: the name box says Hoàng. Settings from the title keep the foyer behind them, and Băng mòn is off.
2. T1:
   - the opening choice (anger or fear);
   - about 10 tiles across;
   - figures about 1.5 tiles tall;
   - zoom in: the figures don't shrink.
3. After T1, the café: a "!" over Lệ Thanh and the quest line. Her talk names the twelve threads and asks who waits at home.
4. T2:
   - Mai acts first and heals; you step beside the ghost; Mai joins and attacks (a two-on-one battle);
   - a Đồng lòng card at the top left;
   - the result gives Đậu xanh and Đường;
   - in the café, Lệ Thanh's lesson opens the counter; brew the chè; Mai's memory plays.
5. T2s:
   - Bảo acts first, and only Dao mổ is lit in the battle;
   - the coat lies on the floor as an icon, with its name on hover;
   - wear it from the notebook's Trang bị.
6. In a battle:
   - the camera closes on the striker, then the one hit, then pulls back out;
   - the sides stand at 25% and 75%;
   - Rút khỏi trận works.
7. On the board:
   - right-click hides the action panel, and again brings it back;
   - a click on a ghost shows its card and reach;
   - two on one square ask which.
8. T3: the row turns red only on round 2, with Bảo's warning.
9. Stage 1: Tí's rope runs to the Rat; clicking his portrait looks at him.
10. Ký ức in the notebook: years, with "???" where the skill is closed.
11. Read the new T1–T3 lines and mark what to change.
12. Run `tools\audio\campaign-audio.bat` (ffmpeg on PATH), then listen in T1, a skirmish and Stage 1, on the board and in the battle view.

## 2026-11-25 02:39 notes (`#UMBRA-442`)

Changed: figures one size (75%, no growth with zoom); the battle camera aimed at its stage (not
pushed in from the board's edge), its distance counting the fighters' step forward, milder
close-ups, 1.0 s in and out; the scene close-up nearer only to 80% and looking at the body; the
notebook's middle and right columns scroll (engine `scroll` node, mouse wheel); one Bà Mai in the
notebook; Mai joins carrying Đánh thường and Chăm sóc only; the quest line in its own panel at the
top of the café; the result shows the spoils with pictures and who fought with XP bars; items use
four shared pictures (weapon, armour, accessory, item) from `items.csv` `sprite`; the Rút khỏi
trận button has its words. The battle view logs when a stage has no `_battle` music (that was why
the music did not change: the battle cuts were never made, the run stopped).

Cloud checks: `UmbraUiTests` (21 cases, the scroll test), `UmbraCampaignTests`, `UmbraCafeTests`
pass; captures of the café (quest panel) and the result screen (`ui-preview`). Not run: a battle
capture (the tutorial waits for input), anything on Windows.

Windows check:
1. T1: figures smaller; zoom out with the wheel and they look smaller.
2. A battle: both sides on screen at about a quarter in; the way in is followable; close-ups stay
   wide enough; the camera turns to whoever acts.
3. A stage's opening lines: the speaker's head is in the picture.
4. T2: one Bà Mai; her detail column scrolls with the wheel; she carries two abilities.
5. The café: the quest panel at the top, apart from the menu.
6. Win a stage: the result shows the spoils and the XP bars.
7. Music: once `tools\audio\battle-music.bat` has made the five candidates
   (`Temp\music-candidates\audio\music\cand_*_battle.ogg` and `_board.ogg`), listen and pick one.
