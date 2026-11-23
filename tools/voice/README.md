# Voice test (campaign plan 1)

`vieneu_lines.py` voices the authored lines of the model test set
(`plugins/last-curtain/assets/campaign/model-test/situations.json`) through **your own VieNeu-TTS v3
Turbo** running on this computer, one file per line and voice, for listening. Nothing is voiced in the
game yet: this only answers whether the voices fit. No voice is cloned from a real person.

## Run it (on the PC, with VieNeu open at http://127.0.0.1:7860)

```powershell
pip install gradio_client numpy soundfile pyworld
python tools/voice/vieneu_lines.py --show-api          # first: check VieNeu's parameter names
python tools/voice/vieneu_lines.py --out voice-test    # all 30 lines (about 6 minutes)
python tools/voice/vieneu_lines.py --only ti --ti-pitch 3,4,5 --ti-formant 1.08
```

The script loads VieNeu's model once (`/load_model`), then calls `/wrapper` with the text and the
preset voice name for each line. If VieNeu answers with an error, `--show-api` prints what its API
expects; the two calls are in `run()`.

`{player}` is filled with `--player` (default Lan) and `{anh/chị}` from `--gender`.

## What you get

`voice-test/index.md` lists every file with its speaker, voice and line. Voices come from each voice
sheet's `tts` list: Tí as Quang Sơn and Ngọc Trân, Bà Mai as Ngọc Trân, Bảo as Thái Sơn, Lệ Thanh as
Ngọc Linh and Thục Đoan (pick one).

**Tí's child voice:** each Tí line is also saved pitched up `+3`, `+4`, `+5` semitones
(`...__+4st.wav`). The WORLD vocoder (`pyworld`) splits the voice into pitch, spectral envelope and
noise; the pitch is raised, and the envelope is stretched only a little (`--ti-formant 1.08`), so he
sounds younger rather than sped up. 1.0 keeps an adult's formants; much higher sounds like a cartoon.

Listen and decide: Tí's pitch and formant, Lệ Thanh's voice, whether each region sounds right.

## Tests

`python -m unittest tools/voice/test_vieneu_lines.py` (a fake VieNeu; a synthetic tone for the pitch).
