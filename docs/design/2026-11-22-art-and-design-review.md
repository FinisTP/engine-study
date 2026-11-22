# Review: the art requests and the design changes (2026-11-22)

Checked against the build (`#UMBRA-299`), the existing art and its recipe
(`plugins/last-curtain/apps/LastCurtain/assets/art/README.md`), `cast-2-2026-11-22.md`, and what the
user's ComfyUI has installed (checked live, 2026-11-22 22:45).

**What the PC's ComfyUI has:** SDXL base 1.0 only. No LoRA, no ControlNet model, no IP-Adapter, no
upscale model. It does have birefnet (cut-outs), inpainting nodes, and ExpressionEditor
(LivePortrait faces). RTX 3050 laptop, about 3 GB free: one 1024 × 1024 picture takes about 85 s.

The counts are right: 216 in all and 28 in the first slice. The problems are below, most important
first.

---

## A. The art request contradicts the existing sprites, and says the existing ones win

The art request says to match the five existing figures where they differ from its defaults. They
differ on almost everything, so as written its prompt prefix and spec table would be overruled:

| Art request | The existing five (and the engine) |
|---|---|
| "Painted 2D sprite, warm lamplight, muted colours, dignified" | Vietnamese oil pastel on toned paper; silent, morbid, uncanny ("the dread style") |
| 3/4 front view | Side profile **facing right**; the game mirrors for left |
| 1024 × 1024 source, figure 85% of height | 832 × 1216 page; body packed to 768 px tall |
| A strip of white **paper** over the eyes | A white **khăn tang** (mourning cloth) tied over the eyes, with dark hollows soaked through |
| Portrait 512 × 512, facing slightly left | Portrait 512 × 627, facing right |
| Prompt includes "transparent background" | SDXL can't make transparency: plain background, then birefnet |
| Fight backdrop 1920 × 1080 | 1344 × 768 (SDXL's own size; no upscale model installed) |
| Icon 256 × 256 | 128 × 128 |

**Decision for the user:** keep the dread style, or switch to the warmer, "dignified" look? The new
story (the war's dead, heroes, a Vietnamese audience) may fit the warmer look better. Either way,
rewrite the prefix and the table so they agree.

**Suggestion:** keep the khăn tang cloth instead of a paper strip. It *is* the rite of grief: the
mourning band Vietnamese families wear. SDXL can't draw either from the prompt alone; the existing
recipe paints the cloth and repaints it with inpainting.

## B. Do the forms in the engine, not as separate paintings (saves about 60 images, keeps faces the same)

Without IP-Adapter or a LoRA, SDXL can't paint the same person four times. Each try gives a new face.
The game's sprite shader already has a tint, an opacity and a glow (`MatchTable`: colour = tint and
opacity, emissive = how much tint). So:

- **Glow, translucency and colour** (red specter, pale café ghost, gold freed, green/blue/red
  natures, the allies' pale blue): one painting, tinted in the engine. Matting cuts off a soft glow
  anyway.
- **Red strings:** draw them in the engine as lines from the wrists and shoulders. Strings painted
  "out of frame" stretch the cut-out's bounding box, and the packer then shrinks the figure to fit
  768 px. Engine strings can also be **cut** one by one, which is the final boss's phase-1 mechanic.
- **What the engine can't do is the eyes:** with the cloth or without it. So each recruit needs **2
  paintings + 1 portrait** (bound / freed), not 4. The 15 ordinary ghosts become **5** (one per
  costume, tinted three ways).

| Group | Asked | With engine forms |
|---|---|---|
| Recruits | 40 | 30 |
| Ordinary ghosts | 15 | 5 |
| Allies (`_free`) | 6 | 6, or 4 if `_free` is just the cloth removed |
| Boon givers | 12 | 9 |
| **About** | **216** | **≈ 150** |

## C. The 12 animal ghosts made this afternoon don't fit the new design

`#UMBRA-299` drew the zodiac as animal ghosts (with skulls and red eyes). The new design's zodiac
spirits are **masked human performers** of a circus band, and the new negative prompt forbids
skulls. In the campaign, nothing is an animal ghost.

**Suggestion:** keep them for **quick match**, which keeps the build's rules. Its monsters still use
the shared specter.

## D. Three kinds of red will read alike

Aggressive ordinary ghosts (red glow, red strings), recruit specters (red tint, red strings) and the
zodiacs (stronger red glow, thick strings) are all red with strings. A recruit must be **freed**, not
killed, so the player has to spot it at once. **Suggestion:** give the recruit a mark that can't be
missed, drawn by the engine (a gold knot on its strings, or a gold outline), rather than relying on
shades of red. Likewise, the allies' "pale blue" sits close to the neutral ghosts' "blue-grey".

## E. Sizes and tools

- **1536 × 1536 boss sprites:** too large for SDXL (it duplicates limbs) and for 3 GB. In game, size
  comes from the tiles a monster covers; every sprite is packed to 768 px anyway. Make bosses at
  832 × 1216 or 1024 × 1024.
- **Expressions:** the game uses 7 faces per character (calm, angry, near-dead, happy, scheming,
  afraid, talk). The list asks for one, except for Le Thanh. ExpressionEditor makes them from the
  calm one cheaply. Add them for the player, the allies, Le Thanh and the recruits.
- **File names:** `<group>/<id>_<variant>.png` is fine for ComfyUI's output. A packing step maps it
  to `art/sprites/<id>/body_<face>.png`.
- **The café backdrop:** the café is a walkable 3D room (12 × 8 m). Is `cafe/backdrop` its back wall,
  or does the café become a painted 2D scene? The same goes for the 12 `cafe/area_<n>` decorations
  (props in the room, or painted overlays?). This needs a decision before the art.
- **Hard for SDXL:** the khăn tang (needs inpainting, as before); áo bà ba, khăn rằn and khăn đóng
  (describe how they look, not just their names); text-free newspapers and contracts.
- **Time:** about 150-216 images × 2-3 tries × 85 s, plus inpainting and cut-outs: **12-18 hours
  of GPU time**, and about **2-3 hours for the first slice**. Paid cloud nodes are installed (Bria,
  Recraft, Magnific), but they cost money and would need your OK.

## F. Gaps and contradictions in the design-change document

1. **Le Thanh: hostess or fellow guest?** §3.2 says she "hides as a fellow guest". §2, §3.5,
   `cast-2` and the art list make her the café's **hostess** for the whole campaign.
2. **When the living player falls.** Falls stay down, and a stage is lost only when the whole side
   is down. But the player is the only living one, and the bad ending is "become her audience".
   Should the player falling lose the stage at once (as in Fire Emblem)? This decides how cautious
   play has to be.
3. **Nhi isn't in quick match.** §3.5 lists "Khang, Lan, Huy, Nhi: quick match only", but quick
   match has five: Bảo, Khang, Mai, Lan, Huy.
4. **Bảo and Mai, two looks.** They stay blindfolded performers in quick match and become ghost
   allies (white coat, pale blue) in the campaign. Fine if intended; the art list should say which
   sprite each mode uses.
5. **The "existing art wins" rule** (A above) is a design decision hidden in the art list. It
   belongs in §3.10.
6. **The heroes are the most sensitive art.** Two national martyrs shown as guests of a ghost who
   trapped the dead (and the Đất Đỏ girl "mid-song, barefoot"). The adviser review in risk 2 should
   happen **before** their art, not just before release.
7. **Art waits on open questions.** Bảo and Mai are still "check the proposed allies" (§7.1), and
   both are in the first slice.

## G. Suggested first slice (28 → 24, same coverage)

- **Player:** male, female and their portraits.
- **Allies:** Bảo and Mai, once §7.1 is settled.
- **Le Thanh:** the hostess and her kind portrait.
- **Bé Tí:** bound, freed and the portrait (engine tints give his specter, café ghost and freed forms).
- **The Rat:** the zodiac and its portrait.
- **Ordinary ghosts:** 5 costumes, tinted red and blue-grey in the engine.
- **Backdrops:** 3 at 1344 × 768.
- **The café backdrop:** once its use is decided.

---

## Answers (the user, 2026-11-22 23:43)

- **A:** keep the dark oil pastel; three-quarter view turned right; the khăn tang; the existing
  sizes; the existing cut-out workflow.
- **B:** agreed: the engine draws the forms.
- **C:** the zodiac spirits keep the animal shape, in the intimidating style of the new list. The
  `#UMBRA-299` set is to be remade (skulls, red streaks, strict profile).
- **D:** a recruit is marked by a gold knot.
- **E:** 768 px stays; the café painting is the back wall of the 3D room.

The art list and the design-change document were refined to match (196 images, a first slice of 22;
the savings estimate in B was too high, because boon givers and allies still need a freed painting).
F4 (quick match keeps the old Bảo and Mai) and F5 (the style rule is now in §3.10) are settled.
F1, F2, F3, F6 and F7 remain open.

## Answers (the user, 2026-11-23 00:00)

1. **Mood:** calm, warm, dignified words for heroes, freed spirits, the café wall and the good ending.
2. **Pose:** "75% right" means three-quarter view turned right.
3. **Zodiac props:** yes, as a guide, not a rule.
4. **Docs:** `cast-2` and `campaign-content` were updated to match.
5. **Allies:** they glow light green.
6. **Specter props:** merged into one painting, as proposed.
7. **Café:** a platform in front of the painted stage, and props as cut-out sprites, as proposed.
8. **Open items:**
   - Le Thanh is the hostess.
   - If the living player falls, it is game over: retry the stage, go back to the café, or quit.
   - **Quick match** is a separate, later phase. Its human side picks from 12 (the player, Le Thanh
     and the ten allies); its monster side picks from the 12 zodiacs.
   - The heroes' art needs no adviser review first.
   - The ten recruits are the official allies; Bảo and Mai are not allies.
9. **Concept art first** (art list section 00): 14 pictures and a mock-up style board.

**Follow-ups left:**
- Bảo and Mai: café characters, or dropped?
- Who fills the early party. My proposal: Le Thanh lends two friendly ghosts.
- The game-over screen, and what a retry keeps.
- Allies (light green) next to friendly ghosts (soft green). I read green as "on your side", with a
  name plate on allies.

## Answers (the user, 2026-11-23 00:26)

1. **The early party:** in the third tutorial, Bảo and Mai join temporarily. Stage 1's party is left
   open, with my proposal: they stay one more stage.
2. **Bảo and Mai** are café characters.
3. **Allies glow blue.** Kept apart from the neutral ghosts' blue-grey by a clearer blue and a name
   plate.
4. **Game over:** a retry restarts the stage from its beginning.
5. **Boon givers** carry the gold knot.
