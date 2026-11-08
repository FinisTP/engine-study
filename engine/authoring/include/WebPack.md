# Web asset pack (`PackWebAssets`, `UmbraTool web-pack`)

The browser build does not link the assets into the program any more. The page loads two files:

| File | Content |
|---|---|
| `assets.bin` | every file of the `assets` folder, back to back, sorted by path |
| `assets.json` | `{"format":"umbra.web-assets","version":1,"size":<bytes>,"hash":"<12 hex>","files":[{"path","offset","size"}]}` |

Left out: AI working files (`*.concept.png`, `*.api.json`, `comfy-export*`), the same ones the
PC package leaves out.

Steps:

1. Collect the files and sort them, so the same folder always gives the same package.
2. Write both files as `.tmp`, then rename them, so a page never sees half of a new pair.
3. Hash = FNV-1a 64 over every path and its bytes (a fingerprint, not security), first 12 hex digits.
4. `sw.js` holds `const VERSION = '<program>';` (from `stamp.cmake`). The packer writes
   `'<program>+<assets hash>'`, replacing an older `+...`. A different version makes the service
   worker download a fresh offline copy.

The page (`umbra-page.js`, `loadAssets`) fetches both before `main`, checks the format and that
`assets.bin` has exactly `size` bytes, and writes each file to `/assets/<path>`.
