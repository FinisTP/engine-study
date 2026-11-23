# Tests for vieneu_lines.py: python -m unittest tools/voice/test_vieneu_lines.py (from the checkout).
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
import pyworld
import soundfile

sys.path.insert(0, str(Path(__file__).parent))
import vieneu_lines as v

CAMPAIGN = Path(__file__).resolve().parents[2] / "plugins/last-curtain/assets/campaign"


def tone(path, hz=150.0, seconds=1.0, rate=24000):
    t = np.arange(int(seconds * rate)) / rate
    # A voiced buzz: a fundamental and a few harmonics.
    wave = sum(np.sin(2 * np.pi * hz * k * t) / k for k in range(1, 6)) * 0.2
    soundfile.write(path, wave.astype(np.float32), rate)
    return path


class FakeClient:
    def __init__(self, wav):
        self.calls, self.wav = [], wav

    def predict(self, *args, api_name):
        self.calls.append((api_name, args))
        return self.wav if api_name == "/wrapper" else "loaded"


class Tests(unittest.TestCase):
    def test_tokens(self):
        self.assertEqual(v.fill_tokens("Chào {anh/chị} {player}!", "Lan", "female"), "Chào chị Lan!")
        self.assertEqual(v.fill_tokens("{Anh/Chị} ơi", "Nam", "male"), "Anh ơi")

    def test_jobs(self):
        jobs = v.jobs(CAMPAIGN, "Lan", "female", "ti", [3, 4, 5])
        # Ti 5 x 2 voices, Bao 5 x 1, Mai 5 x 1, Le Thanh 5 x 2.
        self.assertEqual(len(jobs), 30)
        ti = [j for j in jobs if j.speaker == "ti"]
        self.assertTrue(all(j.pitches == [3, 4, 5] for j in ti))
        self.assertTrue(all(j.pitches == [] for j in jobs if j.speaker != "ti"))
        self.assertNotIn("{", "".join(j.text for j in jobs))

    def test_file_name_is_ascii(self):
        job = v.Job("ti-visit1-mother", "ti", "Ngọc Trân", "x", [])
        self.assertEqual(v.file_name(job, 4), "ti-visit1-mother__Ngoc_Tran__+4st.wav")
        self.assertEqual(v.file_name(job, 0), "ti-visit1-mother__Ngoc_Tran.wav")
        self.assertTrue(v.file_name(v.Job("a", "b", "Đoàn", "", []), 0).isascii())

    def test_shift_raises_pitch(self):
        with tempfile.TemporaryDirectory() as d:
            wave, rate = soundfile.read(tone(Path(d) / "t.wav"))
            out = v.shift(wave, rate, 4, 1.08)
            f0, _ = pyworld.harvest(out.astype(np.float64), rate)
            measured = float(np.median(f0[f0 > 0]))
            self.assertAlmostEqual(measured / (150.0 * 2 ** (4 / 12)), 1.0, delta=0.03)

    def test_only_local_urls(self):
        with self.assertRaises(SystemExit):
            v.check_url("https://example.com:7860")
        v.check_url("http://127.0.0.1:7860")
        v.check_url("http://localhost:7860/")

    def test_run_loads_once_then_one_call_per_job(self):
        with tempfile.TemporaryDirectory() as d:
            client = FakeClient(str(tone(Path(d) / "voice.wav")))
            out = Path(d) / "out"
            args = v.parse(["--campaign", str(CAMPAIGN), "--out", str(out), "--only", "ti", "--ti-pitch", "4"])
            v.run(args, lambda url: client)
            names = [c[0] for c in client.calls]
            self.assertEqual(names[0], "/load_model")
            self.assertEqual(names.count("/load_model"), 1)
            self.assertEqual(names.count("/wrapper"), 10)  # Ti: 5 lines x 2 voices
            self.assertEqual(len(list(out.glob("*+4st.wav"))), 10)
            self.assertIn("ti-greet", (out / "index.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
