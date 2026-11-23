# The model's Vietnamese test (campaign plan 1)

Before the café's lines are worded by the built-in model, a model must show it can write Vietnamese
in each character's voice without inventing anything. `UmbraTool talk-mind` runs one model file over a
fixed test set and writes a report for the user; several reports can be compared side by side. The user
picks the model (or decides every line is hand-written).

## The data (`plugins/last-curtain/assets/campaign/`)

| File | What it holds |
|---|---|
| `voices/<id>.json` | a voice sheet: name, dialect, one paragraph for the prompt (register, habits, topics), own dialect words (`markers`), words never said, the VieNeu voices to try, memory fragments |
| `model-test/situations.json` | the test set: who speaks, the task, the facts, the authored line (the fallback), words that `must` appear and words that must not (`forbid`) |
| `model-test/rules.json` | the numbers (lengths, the 90% pass share, 1.5 s, 3 GB, seeds), the global never-say list, the English words, each dialect's marker words |
| `model-test/prompt.txt` | the prompt, with `{name}`, `{sheet}`, `{facts}`, `{task}`, `{authored}` filled in |

All Vietnamese here is Claude's draft, for the user's review as a native speaker. Lệ Thanh's dialect
words are a proposal.

A memory not yet regained goes in `forbid`: in Tí's first visit he knows his mother's name and the Tết
promise, not the dawn message or the white sand.
