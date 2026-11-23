# Campaign plan 8 (after slice 1 plays): gamepad and keyboard-only input — outline

**Goal:** Everything in the campaign plays with a gamepad alone and with the keyboard alone:
the board cursor, per-device key files, prompts for the device last used with Xbox-style glyphs, and an
on-screen keyboard with Vietnamese tone marks for the name.

**Spec:** `docs/design/ux/interaction-patterns.md` P1, P4, P17 and §4 (gaps); the user, 2026-11-23:
gamepad comes after slice 1 plays; Xbox glyphs only, no Xbox or Microsoft logo; right-click cancels.

**Outline (written as a full plan when slice 1 is accepted; commits continue from plan 7):**

1. `keys-gamepad.json` beside plan 5's `keys-keyboard.json`; the engine's gamepad input mapped to meanings.
2. The last device used, tracked; prompts swap between key names and glyphs (`art/ui/glyphs/`, drawn
   by script per the UI art requests §1.5).
3. The board cursor (P4): one square, held-key repeat 0.25 s then 0.08 s, the camera following, Q/E and
   LB/RB cycling valid targets; on the world map, one ticket.
4. The command menu, sub-menus, notebook, café and menus checked with the gamepad alone (the HUD and
   world-map acceptance criteria for input).
5. The on-screen keyboard with Vietnamese letters and tone marks for the new-game name field.
6. Records: `docs/testing/s52-gamepad.md` (§4: the slice with a gamepad only, on Windows).
