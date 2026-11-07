# Local AI bridge (`Umbra::LocalAi`)

Talks to an [Ollama](https://ollama.com) server **on this computer** for AI-assisted authoring
(roadmap P8B). Used by the editor and `UmbraTool draft`; the game never links it
(`Umbra.GameExcludesEditor`), and no other engine module opens a network connection
(`Umbra.EngineOffline`).

```text
idea ──MakeDraftRequest──► ChatRequest ──Chat / ChatJob──► answer text
                                                            │ ParseDraft (JSON only, never run)
                                                            ▼
                                     authoring::PlanDraft (QuestDraft.md) ──► changes + play-through
                                                            │ problems?
                                     AddFixRequest ◄────────┘ (old answer + problems, ask again)
```

## The request

`POST /api/chat` with `"stream": true` and `"format"` = `authoring::DraftSchema()` (Ollama's
structured outputs: the model can only produce JSON of that shape). The system message
(`authoring::DraftInstructions`) lists the rules and the ids this content allows.

Reasoning models (qwen3.5 and newer) think before answering by default, which costs minutes for a
draft; the request sends `"think": false`. If a model refuses that field, `Chat` asks once more
without it.

## Installed models

`ListModels` reads `GET /api/tags` (2 s limit). The editor fills its model picker from it when
the window first opens (and on Refresh); `UmbraTool draft` without `--model` uses it too.
`PreferredModel` keeps the wanted model if installed, else picks an installed Qwen, else the
first installed model.

## Limits and failures

| Rule | Why |
|---|---|
| Only `http://localhost`, `127.0.0.1` or `[::1]` (`ParseLocalUrl`); anything else is refused before connecting (`ai.host`) | No hidden cloud fallback (roadmap T58). A cloud adapter would need its own consent and cost design |
| Answer over `maxBytes` (64 KiB) is refused, not cut (`ai.limit`) | Model output is untrusted |
| `timeoutSeconds` (600) covers the whole answer, model loading included (`ai.timeout`) | Big models load slowly the first time |
| `CancelToken::Cancel` closes the socket from another thread, so a waiting read returns at once (`ai.cancel`) | The editor never waits for a model |

Every failure is one diagnostic a creator can act on: `ai.connect` (Ollama not running),
`ai.model` (model not installed: `ollama pull <model>`), `ai.ollama` (Ollama's own error),
`ai.answer` (malformed or cut-off stream), `ai.http`.

## ChatJob

One `Chat` on a worker thread. `Start` refuses while a job runs **or its result hasn't been
taken**, so an old answer can never be mistaken for a new one. The UI polls `Take()` each
frame. The destructor cancels and joins, so closing the editor mid-answer is quick.

## Why Ollama and qwen2.5:7b

User decision 2026-11-07: Ollama on the creator's PC, default model `qwen2.5:7b` (about 4.7 GB,
follows JSON schemas well; Apache-2.0 license). Any installed model works; `qwen3.5:9b` (about
6.6 GB, Apache-2.0) is the newer choice. The model is not shipped with Umbra; creators
install it themselves (`ollama pull qwen2.5:7b`). The model name and address are editable in the
editor (saved in `ai-draft.json` in the preferences folder) and on the command line.
