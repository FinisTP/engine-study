# Engine UI: the text field — verification record

## Record

- Date: 2026-11-23.
- Commit: `issue #UMBRA-321`. Asked for by the user ahead of the campaign plans (the player's typed
  name, Vietnamese included; design `docs/design/2026-11-23-campaign-slice-1.md` §7.9).
- Tester: Claude (remote session), Linux cloud workspace, GCC Debug.
- State: **implemented/unverified** on Windows.

## 1. What changed

- `platform::Input`: `HandleText`, `Text()` (UTF-8 typed this frame; each Backspace adds `\b`, repeats
  included); `EventPump` passes `SDL_EVENT_TEXT_INPUT`; `Window::SetTextInput(on)` starts or stops SDL's
  text input only on a change.
- `ui`: a `field` widget (`id`, `placeholder`, `max` letters, default 24, `action`); `UiInput::text`;
  `UiSystem::FieldText`, `SetFieldText`, `WantsText`. Rules and drawing: `engine/ui/include/UI.md`,
  "Text fields". A `field` style in both skins (`rpg-ui`, Last Curtain).
- Wired: `UiInput::text` in `Screens::MapInput` and `GameUi::MapInput`; `SetTextInput(WantsText())`
  each frame in the Last Curtain's night, match and café loops and the engine app.

## 2. Automatic checks (Linux)

- New: `Input keeps the text typed this frame...` (Umbra.Platform) and `A text field takes typed
  letters, Vietnamese too...` (Umbra.Ui): typing "Bé Tí", a backspace removing "í" (2 bytes), the
  letter limit, control characters dropped, a W/A/S/D-style key that types without moving the focus,
  left/right staying in the field, Enter sending the text, down leaving the field, `SetFieldText`, a
  field without an id rejected. Both watched failing first.
- Full run: `cmake --build build-gcc` then `ctest`: **58/58 passed**.
- Picture: `UmbraTool ui-preview` of a name screen: the focused field with its lit border, the dimmed
  placeholder and the caret. Poppins shows a missing-letter box for "ậ" (the planned font swap).

## 3. Limits

- The caret is always at the end (no moving it); no selection or paste.
- A keyboard that sends a tone mark as its own code point (decomposed) is not joined into one letter;
  Unikey's default (precomposed Unicode) is fine.

## 4. Open: the user's Windows check

No screen uses a field yet; the campaign's name screen (plan 5) is the first. Then, on Windows:
1. Type a name with Unikey (Telex) and with Windows' own Vietnamese keyboard: "Bé Tí", "Lệ Thanh".
2. Backspace removes one letter at a time, tone marks included; the name stops at its limit.
3. Hotkeys (E, L, Space…) don't fire while typing; Enter submits; clicking a button leaves the field.
Result: **not run**.
