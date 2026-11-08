# Fonts: from a TTF file to text on screen

The GPU cannot read a font file. `FontAtlas` turns each font of the skin into small pictures of
its letters (glyphs), all packed into one texture, and remembers where each one is.

## Building the atlas

1. **Check the file.** A TrueType/OpenType file starts with one of four tags (`00 01 00 00`,
   `true`, `OTTO`, `ttcf`). stb_truetype trusts its input completely, so anything else is
   refused before it is parsed.
2. **Scale.** Font files measure in their own units (often 2048 per em).
   `stbtt_ScaleForPixelHeight` gives the factor that makes ascent − descent equal the pixel size
   asked for in the skin. Ascent (top of tall letters above the baseline) and line height are
   rounded to whole pixels so lines don't blur.
3. **Pack.** `stbtt_PackFontRanges` draws every character of these ranges into a 1024 × 1024
   alpha image, side by side with a 1 pixel gap:

   | Range | For |
   |---|---|
   | 20–7E | ASCII (must fit) |
   | A0–24F | Latin-1 and Latin Extended (é, ñ, ő ...) |
   | 1EA0–1EF9 | Vietnamese (ạ, ế, ữ ...) |
   | 2010–2026 | dashes, quotes, … |

   If something doesn't fit, it tries 2048, then 4096. A character the font lacks is skipped.
4. **White block.** The 2 × 2 pixels in the bottom-right corner are set to fully opaque. Plain
   boxes sample there, so a box and text can share one texture and one draw batch. The packer
   fills from the top-left, so the corner is checked to still be free.
5. **Upload format.** RGB is white everywhere and alpha holds the glyph coverage. The shader
   multiplies by the vertex color, so one atlas gives text in any color.

## Drawing text

For each code point (UTF-8 decoded by `NextCodePoint`) the glyph gives:

- `min`/`max`: the quad's corners relative to the pen position on the baseline,
- `uvMin`/`uvMax`: where it is in the atlas,
- `advance`: how far the pen moves right.

```
pen.x ──► glyph quad = pen + min .. pen + max
          baseline y = top of the line + ascent
```

Wrapping (`FontAtlas::Lines`) splits at spaces: words are added while the measured width stays
under the limit; a word longer than the whole line goes alone on a line and sticks out. `\n`
always breaks.

Measuring (`FontAtlas::Measure`) uses the same advances, so layout and drawing agree. Each line
starts at a whole-pixel position and glyphs were packed at their final size, so they map 1:1
onto screen pixels and stay sharp.

## Fonts in the reference game

Poppins (body text) and Lora (titles), both SIL Open Font License 1.1; the license files sit next
to the fonts in `plugins/rpg-ui/assets/ui/fonts/`.
