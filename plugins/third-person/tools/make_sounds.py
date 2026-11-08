"""Writes the placeholder sounds in assets/audio (48 kHz, 16-bit, mono WAV).

Run from the plugin folder:  python tools/make_sounds.py
Every sound is made from sine waves and noise, so there is nothing to license.
"""
import math
import random
import struct
import wave

RATE = 48000


def envelope(t, length, attack=0.005):
    # Quick fade in, then an exponential fade out so sounds don't click.
    fade_in = min(1.0, t / attack)
    return fade_in * math.exp(-5.0 * t / length)


def footstep(t, length, state):
    # Low-passed noise: a soft thud.
    state[0] += (random.uniform(-1, 1) - state[0]) * 0.08
    return 2.5 * state[0] * envelope(t, length, 0.002)


def jump(t, length, state):
    # Rising sweep 220 -> 440 Hz (phase is the integral of the frequency).
    state[0] += 2 * math.pi * (220 + 220 * t / length) / RATE
    return 0.5 * math.sin(state[0]) * envelope(t, length)


def interact(t, length, state):
    # Two-note chime: E5 then B5.
    freq = 659.25 if t < length / 2 else 987.77
    local = t if t < length / 2 else t - length / 2
    return 0.4 * math.sin(2 * math.pi * freq * t) * envelope(local, length / 2)


def ui_click(t, length, state):
    return 0.35 * math.sin(2 * math.pi * 1200 * t) * envelope(t, length, 0.001)


def hit(t, length, state):
    # A short punch: a falling 140 -> 60 Hz thump under a burst of bright noise.
    state[0] += 2 * math.pi * (140 - 80 * t / length) / RATE
    noise = random.uniform(-1, 1) * math.exp(-40.0 * t)
    return (0.7 * math.sin(state[0]) + 0.35 * noise) * envelope(t, length, 0.001)


def write(name, generator, seconds):
    random.seed(1)  # same file every run
    state = [0.0]
    count = int(RATE * seconds)
    frames = bytearray()
    for i in range(count):
        value = max(-1.0, min(1.0, generator(i / RATE, seconds, state)))
        frames += struct.pack('<h', int(value * 32767))
    with wave.open(f'assets/audio/{name}.wav', 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(bytes(frames))


write('footstep', footstep, 0.12)
write('jump', jump, 0.25)
write('interact', interact, 0.5)
write('ui_click', ui_click, 0.06)
write('hit', hit, 0.18)
