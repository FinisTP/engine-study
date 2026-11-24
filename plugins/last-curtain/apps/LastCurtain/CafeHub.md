# The campaign café (`CafeHub`, `CafeHubApp`)

Calm by design: nothing counts time or money and nothing fails (campaign plan 6).

0. **The room** (`CampaignRoom`, the user 2026-11-24). The career café's room (`assets/cafe/room.json`)
   with places and spots from `assets/campaign/cafe-room.json`. The player and the allies can walk
   (WASD, the arrows, or a click on the floor); Tab or "Đổi người" hands the walk to the next ally, who
   walks on from the same spot while the one who walked goes back to its place. Lệ Thanh keeps the counter.
   E (or Space) uses what is nearest: a person (talk), the counter (brew), the notice board (notebook), the
   door (leave). `CafeRoom::DrawCampaign` draws it with each cast sprite; names float over heads.
1. **People.** Lệ Thanh, Bảo, Bà Mai and every recruit who joined. A gold name means a new talk
   (`OpenTalks`, at most three new a visit). E next to them, or a click on the name, plays the first
   open talk's scene; with nothing new a line says so.
2. **Brewing.** Known recipes, memory drinks first. A row lists what it lacks and where that is found
   (stages that hold or drop it, "?" when none). A memory drink plays its memory's scene; the scene's last
   answer picks the upgrade version for good. A general drink asks whom it is for and is refused at the
   cap (`options.csv` `boostCapPerStat`).
3. **Keepsakes.** A keepsake in storage that would bring back a joined recruit's memory shows as
   "Đưa kỷ vật"; it stays in storage afterwards (the notebook shows it).
4. **Scenes** run on the dialog screen (`RunScene`); their chosen facts go to `SceneDone`.

`LastCurtain --campaign-cafe` opens it with a sample save after Stage 1 (recipes known, ginger, sugar,
the folded note).

## The save

`SaveCampaign` writes `campaign.json` (format `umbra.lc-campaign`, version 2): the player, roster, rooms
(cleared, refill, taken), chapter, the rite (unlocked, used), storage, recipes, memories, talks, recruits,
facts, versions, boosts, slots and party. It writes a temp file and renames it over the old one, so a write
that stops halfway leaves the last good save. `LoadCampaign` refuses another version and never deletes a bad
file; the error names the file and says it was kept.

## Progress

A won stage shares its `xp` with who fought: 100 XP a level, up to 15 (about a level a stage, slice design
14). A skirmish gives none past the next unwon story stage's recommended level. Finds go to storage, picked
take-once items are remembered, the room is cleared, the fight leaves its facts (won, fell, freed, ratStole),
T3 unlocks the rite. A story win refills every cleared skirmish with its next twist. A lost stage keeps
nothing but the rite's use.
