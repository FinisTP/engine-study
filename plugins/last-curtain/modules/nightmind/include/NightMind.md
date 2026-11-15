# The model's lines (plan 3)

The built-in model (the living town's `Mind`, S23) rewords a few lines of the night so the guests
sound like themselves. It only words; the night's rules never see its answer.

## Steps

1. The night makes its template line, as before (`Narrate`, `captions.json`, `questions.csv`).
2. `Request(def, ask)` builds the prompt: the kind's task, three worked examples (their order turns
   with the seed), then ours: the speaker and their tone (`voice`, or `takenVoice` once Taken,
   from `guests.csv`), who the line is about with their pronouns, and the template line.
   A grammar lets the model write only one plain sentence (two for the Turn), no digits.
3. The answer goes through `Guard`; anything it refuses, and anything late, shows the template.

## Line kinds

| Kind | Speaker | Never reworded |
|---|---|---|
| Caption | the narrator | the result line (dice, damage, items) |
| Question | the guest asking | the two answers |
| GoalDone / GoalFailed | the guest | who did what |
| Turn | the guest, then its Taken form | who turned |
| Taken | the Taken form | the result line |

## The guard

In order; the first that objects wins:

1. Plain text: 8 to 120 characters (230 for the Turn), a capital first, `.`, `!` or `?` last,
   only letters, spaces and `, : ' - . ! ?` (captions use colons: "opens a door: the Rope room").
2. Names: every guest or Taken named in the template is still named in full. No other guest
   (even by the given name alone, "Mai"), no other Taken and no example person is added.
3. Pronouns: "he" or "she" only if someone the line is about is one (or the speaker, except in a
   caption, which is narration). "They" is always fine.
4. Added words: no "not", "never", "no"... the template lacks (or dropping one it has), in a
   caption, question or goal line; no outcome ("dies", "misses", "wins"...) in a caption or a
   Taken line; no stat or hint ("Might", "Sanity", "easy", "safe"...) in a question.

The examples use people who are not guests (Co Hoa, Ong Lam, Chi Tam): a small model sometimes
copies an example, and check 2 catches it.
