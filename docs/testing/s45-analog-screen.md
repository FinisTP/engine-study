# The analog screen (CRT and VHS) — verification record

## Record

- Date: 2026-11-23.
- Commit: `issue #UMBRA-333`. Asked for by the user on 2026-11-23 ("a CRT and VHS screen shader for a
  more analog horror feeling, with an option to turn it off"). The user's answers: it covers the whole
  screen, menus and text included; on by default; switched in the pause menu and saved; CRT and VHS
  as two switches, each with a strength.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug, the real DXC `v1.8.2505`,
  SwiftShader (Subzero backend) built from source under `xvfb-run`.
- State: **implemented/unverified** on Windows.

## 1. What changed

- **Engine** (`engine/core/include/Post.md` "6. The analog screen"): `PostSettings::crt` and `vhs`
  (0 = off .. 1); `AnalogOn`, `PackAnalog`, `CrtWarp`. Vulkan: `VulkanPost` keeps a screen picture per
  swapchain image and a screen pass compatible with the swapchain's; when the analog screen is on, the
  composite and the overlay draw there, then `AnalogPS` (`Post.hlsl`) draws it onto the swapchain.
  Off, the frame is drawn exactly as before. Browser: `analogFs` in `umbra_render.js`, the frame and
  UI into `UG.screen`, `umbra_gpu_analog` onto the canvas.
- **The Last Curtain:** `options.csv` `crt` 0.6 and `vhs` 0.45 (the designer's strengths);
  `Analog.hpp` keeps the player's two switches in `<preferences>/last-curtain/screen.json` (on until
  turned off); the pause menu (night and match) has "Old TV screen: on/off" and "Worn tape: on/off";
  the night, the match and the café apply them every frame.

## 2. Checks (Linux)

- `Umbra.Core` `[post]`: off by default, packing (clamped strengths, seconds wrapped at 1000), the
  bulge (centre kept, edges out, corners off the glass, none at strength 0).
- `Umbra.Night` `[options]`: strengths from `options.csv`; switches start on; one switch off is saved
  at once and read back; a broken file keeps the defaults; no preferences folder still works, unsaved.
- Shaders: `AnalogPS` compiles with the real DXC; `analogFs` and the whole post WGSL validate with
  `naga` 30.0.2.
- Pictures (SwiftShader): `captures/2026-11-23-analog-cafe.png` (curve, black rim, scanlines,
  fringe, grain and line jitter over the room, labels and HUD; text readable),
  `captures/2026-11-23-analog-match.png` (the tracking band tearing through the board),
  `captures/2026-11-23-analog-pause.png` (the pause menu's switches). With both switches off
  (`captures/2026-11-23-analog-off-cafe.png`), compared with a build without this change: the only
  pixels that differ are where the café seats its random guests; the frame's edges are identical.
- Full run: `cmake --build build-gcc` then `ctest -E Balance`: **57/57 passed** (the two balance soaks left out: no rule changed; they passed earlier the same day, 59/59 in `s44`).

**Not run:** the Khronos validation layer (not built in this workspace: the barriers between the
screen pass and the analog pass are reasoned, not checked by it); the browser build (no Emscripten
here: `RendererWeb.cpp`'s two new calls are not compiled; the WGSL is validated).

## 3. Limits

- The pass costs one more full-screen draw with 9 texture reads per pixel; not measured on a real card.
- The strengths are per game (options.csv), not per scene; the café has no pause menu yet, so its
  switches are set from the night's or the match's pause menu (they share one file).
- Pause text is English like the rest of the pause menu until the Vietnamese words set (campaign plan 5).

## 4. Open: the user's Windows check

1. Start `LastCurtain` (café, then a match): the curved, scanlined, grainy look over everything;
   Vietnamese and English text still readable; the tracking band rolls down about every 16 seconds.
2. Pause (Esc) in a match: turn "Old TV screen" off, then "Worn tape" off: each goes at once; the
   picture is then the old one. Quit and start again: still off. Turn both back on.
3. Resize, minimize and restore the window with the effect on: no black screen, no crash.
4. Too strong or too weak? Change `crt` / `vhs` in `apps/LastCurtain/assets/options.csv`.
5. Frame rate with and without the effect on the RTX 3050.

Result: **not run**.
