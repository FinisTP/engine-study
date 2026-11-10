# Makes audio/swing.wav: a short "whoosh" for weapon swings (S9 clip events). Python 3 + numpy.
#   python make_swing.py out.wav
# 1. White noise from a fixed seed (the same file every time).
# 2. A band-pass whose centre sweeps up then down (the blade passing), built from two one-pole filters.
# 3. A quick rise and a longer fall in loudness; 16-bit mono, 48 kHz.
import sys, wave
import numpy as np

rate, seconds = 48000, 0.28
n = int(rate * seconds)
t = np.arange(n) / rate
noise = np.random.default_rng(7).standard_normal(n)
centre = 900.0 + 2600.0 * np.sin(np.pi * t / seconds) ** 2  # Hz
low = np.zeros(n); high = np.zeros(n)
a = b = 0.0
for i in range(n):
    k = 1.0 - np.exp(-2.0 * np.pi * centre[i] / rate)        # one-pole low-pass at the centre
    a += k * (noise[i] - a)
    k2 = 1.0 - np.exp(-2.0 * np.pi * centre[i] * 0.35 / rate)  # and one far below it
    b += k2 * (a - b)
    low[i], high[i] = a, a - b                                # their difference: a band
envelope = np.minimum(t / 0.05, 1.0) * np.exp(-np.maximum(t - 0.08, 0.0) * 16.0)
sound = high * envelope
sound = sound / np.abs(sound).max() * 0.7
with wave.open(sys.argv[1], 'wb') as out:
    out.setnchannels(1); out.setsampwidth(2); out.setframerate(rate)
    out.writeframes((sound * 32767).astype('<i2').tobytes())
