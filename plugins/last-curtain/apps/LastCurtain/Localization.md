# Localization (the user, 2026-11-24 19:40)

Every short text the game shows lives in one table, `assets/localization.csv`:

```
key,vi,en
match.log.Blow,"{a} đánh trúng {b}, mất {c} HP.","{a} hits {b} for {c}."
campaign.close,Đóng,Close
```

- **Keys** are dotted paths. The first part says who reads it: `match.*` the fight's words (what was
  `match-words.json` / `.vi.json`), `campaign.*` the campaign's screens (what was `campaign-words.json`).
- **A screen's words** are every row under a prefix, as nested JSON (`Localization::Words`): `match.log.Blow`
  becomes `words["log"]["Blow"]`. Keys that are all numbers (`match.lines.Down.0`, `.1`) become a list.
- **The language** is the settings' switch (`screen.json` "english"); Vietnamese unless English is chosen.
  A row with no English shows its Vietnamese; a missing row shows its key, so it is easy to spot.
- **The guard** (`tests/LocalizationTests.cpp`): every row has both languages, and the campaign's screens
  (`ui/*.ui.json`) write no text of their own: only `{words.*}` tokens, data, numbers, signs and HP/XP/Lv.

Placeholders in braces (`{name}`, `{n}`) are filled by the code; keep them in both languages. Text inside
the scene files (dialogue) carries its own `vi` and `en` per line; everything shorter is here.
