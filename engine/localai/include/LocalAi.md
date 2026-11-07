# Local AI bridge (`Umbra::LocalAi`)

Talks to an [Ollama](https://ollama.com) server **on this computer** for AI-assisted authoring
(roadmap P8B), and to [ComfyUI](https://www.comfy.org) for textures (P8C, section at the end). Used by the editor and `UmbraTool draft`; the game never links it
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
| Only `http://localhost`, `127.0.0.1` or `[::1]` (`ParseLocalUrl`; `localhost` is dialed as `127.0.0.1`, because Windows tries `::1` first and Ollama listens on IPv4 by default); anything else is refused before connecting (`ai.host`) | No hidden cloud fallback (roadmap T58). A cloud adapter would need its own consent and cost design |
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

## ComfyUI (textures, roadmap P8C)

`ComfyUi.hpp`, same rules as Ollama: local addresses only, every failure named, cancel at once.

```text
POST /prompt  {"prompt": TextureWorkflow(request), "client_id": "umbra"}  -> prompt_id
GET  /history/<id>   twice a second until it has the id (or the time limit: POST /interrupt)
GET  /view?filename=..&subfolder=..&type=temp   -> PNG bytes (at most 32 MiB) -> DecodeImage
```

`TextureWorkflow` is the plain text-to-image graph: CheckpointLoaderSimple → two CLIPTextEncode →
EmptyLatentImage 1024² → KSampler (dpmpp_2m, karras, 25 steps, cfg 7) → VAEDecode → PreviewImage
(a temporary file, so nothing piles up in ComfyUI's output folder). It needs no custom nodes;
seamlessness is done afterwards by `assets::MakeSeamless`.

Failures: `ai.connect` (not running; names both ports), `ai.model` (the checkpoint isn't in
`models/checkpoints`, from ComfyUI's validation error), `ai.comfyui` (refused graph or a failed
run, with ComfyUI's message, e.g. out of GPU memory), `ai.timeout`, `ai.limit`, `ai.answer`,
`ai.cancel` (also sends `/interrupt`). `ListCheckpoints` reads `/object_info/CheckpointLoaderSimple`
in both of ComfyUI's list formats; `FindComfyUi` tries port 8000 (Desktop), then 8188.

`ImageJob` runs a batch (the variations) on one worker thread and stops at the first failure.
`TextureAssistant.hpp` holds the shared steps: project style (`assets/materials/style.json`),
seeds, `FinishTexture` (resize, seamless, seam score) and `SaveTextureMaterial` (image, then
material with its `generated` block; temporary file + rename; free names only).

Default model: SDXL base 1.0 at 1024 px (user decision 2026-11-07, 8 GB GPU; CreativeML
OpenRAIL++-M, outputs may be sold). Not shipped with Umbra: creators install ComfyUI and the model.

## 3D models (roadmap P8D)

`GenerateModel` uploads a picture (`POST /upload/image`), runs the creator's exported image-to-3D
workflow with the run's values filled in, and downloads the `.glb` its save node lists. Queue,
wait, cancel and download are the same code as for pictures (`RunGraph`); `ModelJob` and
`ImageJob` are one `BatchJob` template. Time limit 30 minutes, size limit 256 MiB; a workflow
whose model files aren't installed gives `ai.model` with ComfyUI's message. Details in `ModelGen.md`.
