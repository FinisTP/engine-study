# The Last Curtain: title, new game, pause and settings

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23
- **Platform Target:** Windows PC; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** UX Spec (four small screens) · **Mockups:** artboards *Title*, *New game*, *Settings*
- **Patterns:** P1–P3, P8, P17

## A. Title

**Purpose:** get into the game in one press.
**Layout:** the game's name over the theatre's foyer backdrop; a short column: **Tiếp tục** (only
with a save; focused; lands in the café), **Trò chơi mới**, **Cài đặt**, **Thoát**. Nothing else (no news, no version
text except a small build number bottom right).
**Interaction:** menu focus (P3). Trò chơi mới over an existing save asks to confirm (P8: the old
save is replaced).
**Acceptance:** with a save, Enter on the title continues; without one, Trò chơi mới is focused.

## B. New game (Khách lạ)

**Purpose:** set the name and gender the story will use, in under 30 s.
**Layout:** one panel: "Tên của bạn" text field (P17, max 16 letters, Vietnamese letters allowed);
two large choices **Nam** / **Nữ** with the player's two sprites; **Bắt đầu** (disabled until a name
is typed, reason in the tip). A one-line note: "Tên và cách xưng hô dùng trong lời thoại."
**Interaction:** keyboard types; gamepad A on the field opens the on-screen keyboard; ←/→ picks the
gender; Bắt đầu with Enter / A.
**Acceptance:** a name with tone marks (e.g. "Hưởng") is saved and shown correctly in the first
dialog line; the gender picked changes `{anh/chị}` in lines.

## C. Pause (Tạm dừng)

**Purpose:** stop, change settings, or leave safely.
**Layout:** a small centred column. Contents differ by place:

| In a stage | On the world map / in the café |
|---|---|
| Tiếp tục | Tiếp tục |
| Nhật ký (the fight's log) | — |
| Cài đặt | Cài đặt |
| Phím điều khiển (controls) | Phím điều khiển |
| Bỏ cuộc (confirm: the stage is lost) | Về màn hình chính (saved already) |
| — | Thoát game (saved already) |

- **No Save button anywhere** (saving is automatic outside stages; no mid-stage save).
- Today's Old TV / Worn tape switches move to Settings.
**Acceptance:** Esc / Menu opens it from any place with nothing else open; Back resumes; Bỏ cuộc
always asks to confirm with the focus on Không.

## D. Settings (Cài đặt)

**Purpose:** make the game comfortable; every change shows at once.
**Layout:** one panel, rows of label + control, grouped:
- **Hiển thị:** Toàn màn hình (bật/tắt), Cỡ chữ (Thường / Lớn), Màn hình TV cũ (bật/tắt), Băng
  mòn (bật/tắt).
- **Âm thanh:** Nhạc, Hiệu ứng (sliders 0–100).
- **Trò chơi:** Tốc độ chữ (Chậm / Vừa / Nhanh / Ngay), Tốc độ lượt địch (Thường / Nhanh), Ngôn ngữ
  (Tiếng Việt / English, for development).
- **Phím điều khiển:** a read-only table of P1 for the device last used (remapping later).
**Interaction:** ↑↓ rows, ←→ change a value (or click); changes apply at once and save; Back closes.
**Acceptance:** switching Cỡ chữ to Lớn re-lays every open screen at once; every row works with
keys, mouse and gamepad.

## Shared

- Transitions: 0.2 s fade; Pause dims the place behind it.
- Large text: body 16 → 20 px; rows grow; the Settings panel scrolls if needed.
- Localization: menu labels at most 20 letters.
- Open questions: none.
