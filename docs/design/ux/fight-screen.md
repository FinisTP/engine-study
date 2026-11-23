# The Last Curtain: fight screen

- **Status:** Kept as built · **Author:** Claude, with the user · **Last Updated:** 2026-11-23 (rev. 2)
- **Template:** UX Spec (changes only)

## Decision

**The fight screen keeps its own view and backdrop as built** (`match-battle.ui.json`, the stage's
backdrop behind the two fighters; the user, 2026-11-23). No redesign.

## Changes it still needs for the campaign

1. **Words:** every label from the words files, Vietnamese by default ("Round r of n" → "Hiệp r / n").
2. **AP costs:** a pick that costs AP shows it; with too little AP it is greyed with the reason.
3. **Keys:** 1–3 pick, hold Space / A to hurry, Tab / View opens the notebook (read-only, the fight
   paused), as in `interaction-patterns.md`.
4. **Large text:** pick labels and card numbers grow with the setting.

## Acceptance Criteria

1. No English with `language = vi`.
2. A pick needing more AP than the fighter has is greyed and says why.
3. Each pick works with its number key, the mouse and the gamepad.
