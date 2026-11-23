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

## The checks

`Check` runs these in order; the first that objects names the line's failure:

1. `empty`: nothing came back.
2. `bad-text`: broken UTF-8, a letter the screen's fonts don't draw (control characters such as
   tabs, a separate tone mark U+0300–036F, Chinese or another script: only ASCII, Latin-1, Latin
   Extended, U+1EA0–1EF9 and dashes/quotes U+2010–2026 are drawn), a number longer than 4 digits,
   or markup (`< > * # _`).
3. `length`: fewer than `minLength` or more than `maxLength` letters and spaces (code points).
4. `no-marks`: fewer than `markedShare` of the words carry a Vietnamese letter (anything past
   U+00BF): Vietnamese written without its marks.
5. `english`: a word from the `english` list, or a plain-ASCII word with f, j, w or z, which
   Vietnamese doesn't write.
6. `never`: a phrase from the rules' or the speaker's never-say list.
7. `narration`: the line tells instead of speaking: a double quote, a phrase from `narration`
   ("nói:"), or the speaker's own name among its first four words when the authored line doesn't
   start that way ("Tí chào anh…" said by Tí).
8. `dialect`: a marker word of another dialect that isn't among the speaker's own `markers`.
9. `token`: a `{…}` other than `{player}`, `{anh/chị}`, `{Anh/Chị}`.
10. `lost-token`: the authored line has `{player}` or `{anh/chị}` and the model's line doesn't (it
    guessed the player's gender: "Chào cô {player}").
11. `missing-fact`: a `must` phrase is absent.
12. `added-memory`: a `forbid` phrase is present (a memory not regained yet, or a fact the speaker hides).

The narration, lost-token and bad-text (other scripts) checks came from Arcee-VyLinh's first run
(2026-11-23): "Bác sĩ Bảo nói: …", "Chào cô {player}", "một阵".

Words are compared in lower case (`Lower` knows the Vietnamese capitals: Đ, Ơ, Ư, Ạ…Ỹ) and whole:
`ni` doesn't match inside `nil`, and a phrase like `cát trắng` matches two words in a row.

## The run and the report

`RunTest` asks every situation `seeds` times (seeds `base`, `base + 1`…), times each answer, tidies it
and checks it. `Verdict` says "passes the automatic checks" or lists what it misses: the file can't be
loaded by this build's llama.cpp, fewer than `passShare` of lines pass, the average line is slower
than `maxMs`, or the model takes more than `maxVideoMb` of video memory. The licence is not checked:
the user reads it.

`WriteReport` puts the authored line beside the model's for every line, by speaker, so the user can
judge the wording: the automatic checks are only half of the pass. `Summary` is the same run in a few
numbers; `WriteComparison` puts several summaries side by side.
