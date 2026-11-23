# The campaign's flow (`Flow.cpp`)

`LastCurtain --campaign` (or the editor's Play: "campaign"). The save is `<preferences>/last-curtain/campaign.json`.

1. **Title:** Tiếp tục (with a save), Trò chơi mới, Cài đặt (large text, the old TV screen, worn tape), Thoát.
2. **New game:** a name (up to 16 letters, Vietnamese typing works) and Nữ / Nam; then the prologue and T1.
3. **The loop:** the café (`RunCafeHub`) → its door → the map (tickets; select shows the panel, Vào phòng
   enters) → pre-stage (who fights: the player always, up to the cap; the revive warning) → the fight
   (`RunStage` with the save: versions, boosts, slots) → the result (Về quán / Về bản đồ) or game over
   (Thử lại / Về bản đồ; T1 only Thử lại). Bảo and Bà Mai join the roster after T1 at level 2; a recruit who
   joined in the café joins it at level 3.
4. **Saving:** automatic before the café and the map and after every win; a lost stage keeps nothing but
   the rite's use. No save inside a fight.

`RunScreen(game, screen, data, handle)` is the one small loop every menu uses: it draws a screen until an
action that `handle` does not keep (a toggle or a selection keeps the screen open).

Limits: the rite is offered inside the fight (the rules' Rite), so game over shows no rite box; settings
hold the three switches only (volume, text speed, language later); the floor plan picture is not painted.
