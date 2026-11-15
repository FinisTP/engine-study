# Game UI (`Umbra::Ui`, roadmap S3)

The game draws its own menus and HUD, the same way on PC (Vulkan) and in the browser (WebGPU).
Screens are **data files**; C++ gives them data and handles what the player chooses. Dear ImGui
stays for debug windows and the editor only.

```
ui/skin.json + fonts ─┐
ui/<name>.ui.json ────┼─> UiSystem ──Update(data, viewport, input)──> core::UiDrawList ──> renderer
game data (JSON) ─────┘        │
                               └──> actions ("equip", "iron_sword") ──> game code
```

The module is headless: it never sees a window, a renderer or ImGui. It writes triangles into
`core::DrawList::ui`; each renderer draws them after the scene (`DiagnosticsOverlay::DrawUi` on
Vulkan, `umbra_gpu_ui` on WebGPU).

## Files

All under a project's or plugin's `assets/ui/` (project files win, see `AssetFolders`).

| File | What |
|---|---|
| `skin.json` | fonts (TTF file + pixel size), the dim color behind modal screens, named styles |
| `<name>.ui.json` | one screen; `<name>` is what the game opens |
| `sample.json` | data for previews only (editor "Game UI" window, `UmbraTool ui-preview`) |

## Screen file

```json
{ "format": "umbra.ui", "version": 1, "modal": true, "dim": true, "back": "close",
  "root": { "type": "stack", "anchor": "center", "style": "panel", "gap": 8, "children": [
    { "type": "text", "style": "title", "text": "Bag ({bagUsed}/{bagSize})" },
    { "type": "list", "items": "{inventory}", "as": "item", "action": "equip", "arg": "{item.id}",
      "entry": { "type": "button", "text": "{item.name} x{item.count}", "disabled": "{item.cannotEquip}" } }
  ] } }
```

- `modal`: takes the keys (screens below get none; clicks follow rule 4 below). `dim`: darkens what is behind it.
- `back`: the action for Esc / gamepad B (`close` closes the screen).

### Widgets

| Type | Draws | Main fields |
|---|---|---|
| `panel` | a box; children placed by their own `anchor` | `children` |
| `stack` | a box; children one after another | `direction`, `gap`, `align`, `children` |
| `text` | text | `text`, `textAlign`, `wrap` |
| `bar` | a filled bar, optional text on it | `value`, `max`, `text` |
| `image` | a picture from the assets | `image` |
| `button` | a choosable box with text | `text`, `action`, `arg` |
| `list` | one `entry` widget per array element; a grid with `columns` > 1 | `items`, `as`, `entry`, `action`, `arg` |
| `spacer` | nothing, takes room | `size` |

Every widget also has `id`, `anchor`, `offset`, `size` (0 = from the content), `style` (default:
the style named like the type), `visible`, `disabled`, `selected`.

### Anchors

`anchor` says where a widget sits inside its parent's padded area: `top-left`, `top`, `top-right`,
`left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right`, or `fill`.

```
x = parent.x + (parent.w - w) * ax + offset.x      ax = 0 left, 0.5 middle, 1 right
y = parent.y + (parent.h - h) * ay + offset.y      ay = 0 top,  0.5 middle, 1 bottom
```

`fill` takes the whole area; `offset` is then a margin on each side. Inside a `stack`, children
are placed in order instead, and `align` lines them up across (start, center, end, stretch).

### Bindings

Text may hold `{path}` parts, replaced every frame from the game's data: `"HP {hero.health}"`.
Inside a list entry, the entry's name (`as`, default `item`) comes first: `{item.name}`, and
`{index}` is its position. `visible`, `disabled`, `selected`, `items`, `value`, `max` are one
binding each; a missing value, `false`, `0`, `""` or an empty array counts as false. A missing
path shows as nothing; it is not an error, so one screen can serve several game states.

### Focus and input

The top modal screen takes the input (`UiInput`, mapped from keys, mouse and gamepad by the game):

1. The choosable widgets (buttons and list entries with an action, not disabled) are collected
   while drawing, in reading order.
2. Next frame, up/left and down/right move the focus through that list (wrapping); the mouse
   focuses what it is over; confirm sends the focused one's action.
3. Back sends the screen's `back` action.
4. A click goes to the topmost screen with a choice under the mouse, modal or not, so a HUD's
   buttons work beside a menu; a dimmed (`dim`) screen blocks clicks on everything below it.

Two actions belong to the UI itself: `close` closes the screen and `open:<screen>` opens one on
top. Everything else goes back to the game from `Update`.

## Skin

```json
"button": { "base": "text", "background": "#25252f", "border": "#4a4636", "borderWidth": 1, "padding": [16, 8],
            "hover": { "background": "#3a3428" }, "disabled": { "color": "#6c6860" } }
```

A style takes the fields of its `base`, then its own. `hover` (focused), `selected` and
`disabled` override fields in that state. Fields: `font`, `color`, `background`, `border`,
`borderWidth`, `padding` (one number, `[x, y]`, or `[left, top, right, bottom]`), `fill` (bars),
`image` + `slice` (a 9-slice picture as the box), `tint`. Colors are `"#rrggbb"`, `"#rrggbbaa"`
or `[r, g, b, a]` from 0 to 1.

## Drawing

`Painter` turns boxes, pictures and text into quads in `core::UiDrawList`:

- Vertices have ImGui's layout (`position`, `uv`, `color` as RGBA bytes), so the Vulkan overlay
  copies them into an ImGui draw list unchanged.
- A new batch starts when the texture or the clip rectangle changes; lists clip their entries.
- Plain colored boxes use a white block in the font atlas, so most of a screen is one texture.

How fonts become that atlas: `Font.md`.

## Raster (previews without a GPU)

`Rasterize` draws a `UiDrawList` into an RGBA picture on the CPU: for each triangle, every pixel
center inside it (edge functions, the same test GPUs use) gets the texture color times the
vertex color, blended over what is there (`out = src * a + dst * (1 - a)`), only inside the
batch's clip rectangle. It is slow and has no anti-aliasing, which is fine for a preview.
`UmbraTool ui-preview hud inventory --assets <folder> --out x.png` uses it, so an AI agent or a
person can check a screen file's layout without starting the game.

## Adding a widget kind

Add the type name to `ParseScreen` (`Screen.cpp`), how big it is to `UiSystem::Measure` and how
it draws to `UiSystem::Place` (`Ui.cpp`), and a line to the table above.
