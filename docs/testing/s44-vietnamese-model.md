# The Last Curtain, campaign plan 1: the Vietnamese model test, the voice test, the fonts — verification record

## Record

- Date: 2026-11-23.
- Commits: `issue #UMBRA-324` to `#UMBRA-330`. Plan: `docs/design/plans/2026-11-23-campaign-plan-1-vietnamese-model.md`;
  design `docs/design/2026-11-23-campaign-slice-1.md` §7.8.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; dependencies built from
  source (`linux-headless-build.md`), llama.cpp `b9030` (CPU only), the real DXC release
  `v1.8.2505` for the shaders.
- State: **implemented/unverified**. The pass needs the user: a real model's report, a native
  speaker's judgement of the lines, the voices heard.

## 1. What changed

- **Fonts** (`#324`): the Last Curtain skin uses Be Vietnam Pro (body, small, strong, tag) and Playfair
  Display (title, essence); Lora stays for headings. Poppins and Abril Fatface, which lack the
  Vietnamese letters (88 and 92 missing), are gone from the app's fonts folder. Pinned in
  `apps/LastCurtain/assets/ui/fonts/README.md`; listed in `THIRD_PARTY_NOTICES.md`.
  **Titles are Playfair Display Regular, not Bold**: Google Fonts ships it as a variable font, the
  engine draws its default (Regular) instance, the upstream static Bold has no Vietnamese, and the
  licence reserves the name, so a bold copy made here would need another name.
  Engine: `ui::MissingGlyphs(ttf, letters)`.
- **The test set** (`#325`): `plugins/last-curtain/assets/campaign/voices/{ti,bao,mai,lethanh}.json`
  (Tí with four memory fragments) and `model-test/` (20 situations, rules, the prompt). The new
  module `talkmind` (`Umbra::TalkMind`, `modules/talkmind/include/TalkMind.md`) loads and validates it.
- **The checks** (`#326`): ten checks in order (`TalkMind.md` "The checks"): empty, bad text (incl.
  control characters and separate tone marks), length, no Vietnamese marks, English, never-say,
  another dialect's words, unknown tokens, a missing fact, an added memory.
- **The run and report** (`#327`): every situation × 3 seeds, timed; the verdict (90% passing,
  under 1.5 s a line on average, under 3 GB of video memory, loads at all); a Markdown report with
  the authored line beside the model's; a JSON summary; a side-by-side comparison.
- **The tool** (`#328`): `UmbraTool talk-mind --model <gguf>` writes `talk-mind-reports/<model>.md`
  and `.json`; `UmbraTool talk-mind compare <a.json> <b.json>...` compares them.
  `town::LlamaGpuFreeBytes` measures the cards' free memory before loading and after the warm-up.
- **The voice script** (`#329`): `tools/voice/vieneu_lines.py` (`tools/voice/README.md`).

## 2. Automatic checks (Linux)

- `Umbra.TalkMind` (new, 8 cases): the set loads; five broken copies each name the file and field;
  `Lower`/`Words`/`Tidy` on Vietnamese; each check fires on its own line (mixed English, Vietnamese
  without marks, the white sand mentioned in visit 1, `ĐỒ NGỐC` against `đồ ngốc`, a Southern word
  from Tí, a decomposed `ư`, a tab, broken UTF-8, `{ten}`, the mother's name missing); every
  authored line passes its own checks; the request; a run with a stand-in model that answers the
  authored lines (100%) and one that answers English (0%); the verdict's four misses; the report and
  the comparison. Each watched failing first.
- `Umbra.Ui`: `MissingGlyphs`. `Umbra.Night`: every font in the Last Curtain skin draws every
  Vietnamese letter (failed on the old skin).
- **Through real llama.cpp:** a tiny random-weights Qwen2 model with a byte vocabulary (made with
  llama.cpp's `gguf-py`, not committed) ran the whole tool: 60 lines, report and summary written,
  0% passing as expected (random bytes: `bad-text` 39, `empty` 21). It showed that the one-line
  grammar lets tabs and control characters through; `bad-text` now catches them.
  A file that is not a model: `cannot load with this build's llama.cpp: ...`, a report saying so, exit 1.
  `compare` on a file that is not a summary: a message, exit 1 (it crashed before the fix).
- `python -m unittest tools/voice/test_vieneu_lines.py`: 6 tests (tokens, 30 jobs, ASCII file names,
  a synthetic 150 Hz voice pitched +4 semitones measures within 3%, only local URLs, one load then
  one call per line against a fake VieNeu).
- Picture: `captures/2026-11-23-vietnamese-fonts.png` (`UmbraTool ui-preview start` with Vietnamese
  texts): marks on every letter, titles and buttons.
- Full run: `cmake --build build-gcc` then `ctest`: **59/59 passed** (58 before + `Umbra.TalkMind`), 1491 s.

## 2b. Arcee-VyLinh, first run (CPU, 2026-11-23)

The user downloaded Arcee-VyLinh 3B (`projects/reference/models/vietnamese.gguf`, Q4_K_M, 1.93 GB;
its header names VyLinh 3B merged with Qwen2.5-3B-Instruct; git ignores it). There was no Windows
build to run it on in the user's absence, so Claude built a static Linux copy of the same test (the
`talkmind` sources and `LlamaMind.cpp` against llama.cpp `b9030`, CPU only, not committed) and ran it in
the user's PC's Linux shell (i5-12500H, 2 threads), in chunks of 10 lines.

- **60 lines, 55% pass the checks** (needs 90%). Report: `s44/arcee-vylinh-cpu-2026-11-23.md`.
  Times (about 11 s a line) are the CPU shell's, not the game's: speed and video memory still need the
  Windows run on the graphics card.
- **Good:** the Vietnamese is natural and keeps each voice ("Con tới rồi hỉ? Ngồi xuống, bà để phần
  cơm cho con đó.", "Đêm đó hả? Bác sĩ cũng có quyền quên vài ca bệnh chớ."); Tí keeps "tui", "mạ",
  "răng", "hè"; no English at all; memories not yet regained never leaked.
- **Failures, by kind:** narration 13 ("Bác sĩ Bảo nói: …", "Tí chào {anh/chị}…"), missing fact 4
  (3 are Lệ Thanh's dance told in the first person instead of as narration), the player's gender
  guessed 3 ("Chào cô {player}", "Chào anh {player}"), tokens invented 2 (`{Tí}`, `{anh}`), a Chinese
  letter 1 ("một阵"), a run-on explanation 1, a Northern "thế" from Tí 1.
- **The checks grew from it** (`#UMBRA-331`): narration, a dropped `{anh/chị}`/`{player}`, and any
  letter the fonts can't draw (which also covers Chinese).
- **A second prompt** (no narration, keep the tokens, no Chinese, spelled out) did not help: 10 of 20
  on seed 1, and the model began wrapping words in braces. The first prompt stays.

Reading: VyLinh writes good Vietnamese but doesn't follow the format reliably at 3B. In the game a
refused line shows the authored one, so 55% would still give variety; but it is under the agreed 90%.
Next: the same run on Windows with the graphics card (speed, memory), then Qwen3 4B and other
candidates side by side; the user decides.

**Found, not caused by this plan:** `UmbraTool ui-preview start` with the shipped `sample.json`
crashes (an image whose path is empty makes it read the assets folder as a file). The capture above
gave each guest a portrait to get round it.

## 3. Limits

- The licence is not checked by the tool. Arcee-VyLinh and Sailor are merged with Qwen models under
  Qwen's research licence (no commercial use without Alibaba's permission); the SEA-LION models are
  under Gemma terms. Fine to test; settle before shipping.
- The dialect check only catches another dialect's marker words; whether a line sounds like Huế,
  Quảng Ngãi or Saigon is the user's ear.
- Video memory is measured as the drop in the cards' free memory, so another program using the card
  at the same time skews it.
- The voice script's calls (`/load_model`, then `/wrapper` with text and voice) follow the user's
  2026-11-23 samples; `--show-api` prints VieNeu's real parameters if they differ.

## 4. Open: the user's checks

1. **Read the Vietnamese** in `assets/campaign/voices/*.json` and `model-test/situations.json` as a
   native speaker: the authored lines, the voice sheets, the never-say and dialect lists in
   `rules.json`. Lệ Thanh's dialect words are a proposal.
2. **Arcee-VyLinh first.** Build with the GPU model (`windows-msvc` preset, `town-model-gpu`), then
   `UmbraTool talk-mind --model <path>\Arcee-VyLinh-3B.Q4_K_M.gguf`; read
   `talk-mind-reports\Arcee-VyLinh-3B.Q4_K_M.md`. Pass: you are satisfied with the lines and the
   verdict says "passes the automatic checks".
3. **Side by side:** the same for Qwen3 4B and any candidate, then
   `UmbraTool talk-mind compare talk-mind-reports\*.json --out talk-mind-reports\compare.md`.
   **You pick the winner**, or decide every line is hand-written.
4. **Voices:** with VieNeu open, `python tools/voice/vieneu_lines.py --show-api`, then the full run;
   listen to `voice-test`. Pick Lệ Thanh's voice and Tí's pitch (+3, +4 or +5) and say if the
   formant (1.08) needs changing.
5. **Fonts on Windows:** start `LastCurtain`; the title and menus show no boxes; titles in Playfair
   Regular look right.

Result: **not run**.
