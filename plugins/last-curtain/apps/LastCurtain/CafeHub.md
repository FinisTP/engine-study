# The campaign café (`CafeHub`, `CafeHubApp`)

Calm by design: nothing counts time or money and nothing fails (campaign plan 6).

1. **People.** Lệ Thanh, Bảo, Bà Mai and every recruit who joined. A gold button means a new talk
   (`OpenTalks`, at most three new a visit). Clicking plays the first open talk's scene.
2. **Brewing.** Known recipes, memory drinks first. A row lists what it lacks and where that is found
   (stages that hold or drop it, "?" when none). A memory drink plays its memory's scene; the scene's last
   answer picks the upgrade version for good. A general drink asks whom it is for and is refused at the
   cap (`options.csv` `boostCapPerStat`).
3. **Keepsakes.** A keepsake in storage that would bring back a joined recruit's memory shows as
   "Đưa kỷ vật"; it stays in storage afterwards (the notebook shows it).
4. **Scenes** run on the dialog screen (`RunScene`); their chosen facts go to `SceneDone`.

`LastCurtain --campaign-cafe` opens it with a sample save after Stage 1 (recipes known, ginger, sugar,
the folded note). The walking room of the career café is not used yet.

## The save

`SaveCampaign` writes `campaign.json` (format `umbra.lc-campaign`, version 2): the player, roster, rooms
(cleared, refill, taken), chapter, the rite (unlocked, used), storage, recipes, memories, talks, recruits,
facts, versions, boosts, slots and party. It writes a temp file and renames it over the old one, so a write
that stops halfway leaves the last good save. `LoadCampaign` refuses another version and never deletes a bad
file; the error names the file and says it was kept.
