"""Voice test for The Last Curtain (campaign plan 1, tools/voice/README.md).

Voices the authored lines of the model test set through the user's own local VieNeu-TTS
(Gradio, default http://127.0.0.1:7860), one sound file per line and voice, for listening.
Ti's lines are also pitched up (WORLD vocoder: F0 raised, formants moved a little) so he
sounds like a child rather than sped up. Nothing is sent anywhere but that local address.
"""
import argparse
import json
import shutil
import sys
import unicodedata
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import urlparse

import numpy as np


@dataclass
class Job:
    situation: str
    speaker: str
    voice: str  # VieNeu preset
    text: str
    pitches: list  # semitones to make from this line (Ti only)


def fill_tokens(text, player, gender):
    address = "anh" if gender == "male" else "chị"
    return (text.replace("{player}", player)
            .replace("{anh/chị}", address)
            .replace("{Anh/Chị}", address.capitalize()))


def jobs(campaign, player, gender, pitch_speaker, pitches):
    # 1. Each voice sheet's preset voices; 2. each situation's authored line in each of them.
    voices = {}
    for f in sorted((campaign / "voices").glob("*.json")):
        sheet = json.loads(f.read_text(encoding="utf-8"))
        voices[sheet["id"]] = sheet["tts"]
    situations = json.loads((campaign / "model-test/situations.json").read_text(encoding="utf-8"))["situations"]
    out = []
    for s in situations:
        for voice in voices.get(s["speaker"], []):
            shifts = list(pitches) if s["speaker"] == pitch_speaker else []
            out.append(Job(s["id"], s["speaker"], voice, fill_tokens(s["authored"], player, gender), shifts))
    return out


def ascii_name(text):
    text = text.replace("Đ", "D").replace("đ", "d")
    text = "".join(c for c in unicodedata.normalize("NFD", text) if not unicodedata.combining(c))
    return "".join(c if c.isalnum() or c in "-_" else "_" for c in text)


def file_name(job, semitones):
    pitch = f"__+{semitones}st" if semitones else ""
    return f"{job.situation}__{ascii_name(job.voice)}{pitch}.wav"


def shift(samples, rate, semitones, formant):
    """Raise the pitch by `semitones`; stretch the spectral envelope by `formant` (1.0 keeps the formants)."""
    import pyworld
    x = np.asarray(samples, dtype=np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    # 1. Split the voice into pitch (f0), envelope (sp) and noise (ap).
    f0, t = pyworld.harvest(x, rate)
    sp = pyworld.cheaptrick(x, f0, t, rate)
    ap = pyworld.d4c(x, f0, t, rate)
    # 2. Pitch up; 3. move the envelope's peaks (formants) up by `formant` along the frequency axis.
    f0 = f0 * 2 ** (semitones / 12)
    bins = np.arange(sp.shape[1])
    sp = np.array([np.interp(bins / formant, bins, frame) for frame in sp])
    # 4. Put it back together.
    return pyworld.synthesize(f0, sp, ap, rate).astype(np.float32)


def check_url(url):
    host = urlparse(url).hostname
    if host not in ("127.0.0.1", "localhost"):
        sys.exit(f"Refusing {url}: this script only talks to a VieNeu running on this computer (127.0.0.1 or localhost).")


def audio_path(result):
    # Gradio returns a file path, or a list/tuple holding one.
    items = result if isinstance(result, (list, tuple)) else [result]
    for item in items:
        if isinstance(item, dict) and "path" in item:
            item = item["path"]
        if isinstance(item, str) and Path(item).is_file():
            return Path(item)
    raise RuntimeError(f"VieNeu returned no sound file: {result!r} (run with --show-api to see its parameters)")


def parse(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    root = Path(__file__).resolve().parents[2]
    p.add_argument("--url", default="http://127.0.0.1:7860")
    p.add_argument("--campaign", type=Path, default=root / "plugins/last-curtain/assets/campaign")
    p.add_argument("--out", type=Path, default=Path("voice-test"))
    p.add_argument("--player", default="Lan", help="the name put in {player}")
    p.add_argument("--gender", choices=["female", "male"], default="female", help="fills {anh/chị}")
    p.add_argument("--ti-pitch", default="3,4,5", help="semitones for Ti's child voice, comma separated")
    p.add_argument("--ti-formant", type=float, default=1.08, help="formant stretch for Ti (1.0 = none)")
    p.add_argument("--only", help="one speaker id (ti, bao, mai, lethanh)")
    p.add_argument("--show-api", action="store_true", help="print VieNeu's API and stop")
    return p.parse_args(argv)


def run(args, connect):
    import soundfile
    check_url(args.url)
    pitches = [int(s) for s in args.ti_pitch.split(",") if s.strip()]
    todo = [j for j in jobs(args.campaign, args.player, args.gender, "ti", pitches) if not args.only or j.speaker == args.only]
    client = connect(args.url)
    if args.show_api:
        client.view_api()
        return
    # 1. One session: load the model once, then one line at a time.
    client.predict(api_name="/load_model")
    args.out.mkdir(parents=True, exist_ok=True)
    rows = []
    for i, job in enumerate(todo, 1):
        print(f"[{i}/{len(todo)}] {job.situation} as {job.voice}", flush=True)
        made = audio_path(client.predict(job.text, job.voice, api_name="/wrapper"))
        target = args.out / file_name(job, 0)
        shutil.copyfile(made, target)
        rows.append((target.name, job, 0))
        # 2. Ti's child voice: pitched copies of the same line.
        if job.pitches:
            samples, rate = soundfile.read(target)
            for semitones in job.pitches:
                pitched = args.out / file_name(job, semitones)
                soundfile.write(pitched, shift(samples, rate, semitones, args.ti_formant), rate)
                rows.append((pitched.name, job, semitones))
    # 3. An index to listen from.
    with open(args.out / "index.md", "w", encoding="utf-8") as f:
        f.write("| File | Speaker | Voice | Pitch | Line |\n|---|---|---|---|---|\n")
        for name, job, semitones in rows:
            f.write(f"| {name} | {job.speaker} | {job.voice} | {'+' + str(semitones) if semitones else '-'} | {job.text} |\n")
    print(f"wrote {len(rows)} files and index.md to {args.out}")


def main():
    from gradio_client import Client
    run(parse(sys.argv[1:]), Client)


if __name__ == "__main__":
    main()
