# Saves (`Umbra::Game`)

User decision 2026-11-07: **one manual save** written at save stones (E to interact) and **one
autosave** written on every area change. On death the player picks "load save" or "load last
autosave". Saves happen only outside combat, so there are no statuses, cooldowns or pending
decisions to store (roadmap 4.5: persist committed state only).

## File

`manual.sav` / `autosave.sav` in the save folder (the app uses the per-user folder SDL gives it).

```json
{ "format": "umbra.save", "version": 1, "checksum": "9f2c...e1", "payload": "{\"scene\":...}" }
```

The payload is the game state as a JSON **string**, so the checksum covers exactly the bytes that
are read back. It holds: area, player position and yaw, combat mode (`turn`/`realtime`), the hero
(ids and counts only: health, energy, level, xp, skill points, skills, inventory, pending items,
equipment) and each started quest (state, current objective, progress, rewarded flag).

## Writing (all or nothing)

```
1. write  manual.sav.tmp   (fails → existing save untouched)
2. read it back and compare (catches a disk that dropped data)
3. copy   manual.sav → manual.sav.bak   (the previous good save)
4. rename manual.sav.tmp → manual.sav   (replaces it in one step)
```

A crash or full disk during 1–2 leaves only a stray `.tmp`; the real save is still the old one.
`std::filesystem::rename` replaces the target in one call on Windows and Linux; how durable that is
after a power cut depends on the file system, which is why the `.bak` copy exists.

## Reading (validate before use)

| Problem | Result |
|---|---|
| Missing file | `save.missing` |
| Not JSON, wrong format, cut off, checksum mismatch | `save.corrupt` |
| `version` higher than this build knows | `save.newer` (never guess at a newer format) |
| Field missing or wrong type | `save.field` |
| An item, skill, actor or quest the content no longer has | `save.content.missing` (refuse rather than load half a world) |

Nothing is handed back unless every check passes, so a bad file never changes the live game.
`ReadSaveOrBackup` falls back to the `.bak` file with a `save.backup.used` warning.

Version 1 is the first format. When it changes, `ReadSave` gets a migration step for older versions
(read old fields, fill new ones with defaults) and the old bytes stay untouched on disk.
