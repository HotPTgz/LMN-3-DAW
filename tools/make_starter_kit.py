#!/usr/bin/env python3
"""
make_starter_kit.py - writes the "Starter" drum kit in drum_kits/Starter.

Every sound is synthesised from scratch by this script (sine waves, noise and
envelopes; no recordings), so the kit is free to use and share. Run it again
to rebuild the kit; only the Python standard library is needed.

    python3 tools/make_starter_kit.py            # writes drum_kits/Starter
    python3 tools/make_starter_kit.py --dest ~/.config/LMN-3/drum_kits

Sounds, from the LMN-3's first drum key (F3) up:
    kick1 kick2 snare1 snare2 clap1 rim1 hat1 hat2 openhat1 tom1 tom2 tom3
    crash1 shaker1 cowbell1 clave1
"""

import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 44100
FIRST_DRUM_NOTE = 53  # the LMN-3's first drum key
rng = random.Random(3)  # fixed seed: the kit comes out the same every time


def frames(seconds):
    return range(int(RATE * seconds))


def noise():
    return rng.uniform(-1.0, 1.0)


def kick(punch=1.0, length=0.55, low=48.0):
    out, phase = [], 0.0
    for i in frames(length):
        t = i / RATE
        freq = low + 140.0 * punch * math.exp(-t * 32)
        phase += 2 * math.pi * freq / RATE
        click = noise() * 0.3 * math.exp(-t * 400) * punch
        out.append(math.sin(phase) * math.exp(-t * 6.5) + click)
    return out


def snare(tone=190.0, snappy=0.75, length=0.35):
    out, hp = [], 0.0
    for i in frames(length):
        t = i / RATE
        n = noise()
        hp = 0.6 * hp + 0.4 * n  # a little brightness on the snares
        body = math.sin(2 * math.pi * tone * t) * math.exp(-t * 20)
        out.append(body * (1 - snappy) + (n - hp) * snappy * math.exp(-t * 13))
    return out


def clap():
    out = []
    for i in frames(0.4):
        t = i / RATE
        env = sum(math.exp(-(t - d) * 70) for d in (0.0, 0.011, 0.023) if t >= d)
        env += 0.4 * math.exp(-max(0.0, t - 0.03) * 12) * (t >= 0.03)
        out.append(noise() * env)
    return out


def rim():
    return [(math.sin(2 * math.pi * 1650 * i / RATE) * 0.7 +
             math.sin(2 * math.pi * 520 * i / RATE) * 0.3) *
            math.exp(-i / RATE * 90) for i in frames(0.09)]


def hat(decay=45.0, length=0.15):
    out, prev = [], 0.0
    for i in frames(length):
        t = i / RATE
        n = noise()
        out.append((n - prev) * math.exp(-t * decay))  # high-passed noise
        prev = n
    return out


def tom(freq):
    out, phase = [], 0.0
    for i in frames(0.6):
        t = i / RATE
        phase += 2 * math.pi * freq * (1 + 0.6 * math.exp(-t * 18)) / RATE
        out.append(math.sin(phase) * math.exp(-t * 6))
    return out


def crash():
    out, prev = [], 0.0
    for i in frames(1.8):
        t = i / RATE
        n = noise()
        metal = sum(math.sin(2 * math.pi * f * t) for f in (3150, 4270, 5890)) / 3
        out.append(((n - prev) * 0.8 + metal * 0.2) * math.exp(-t * 2.2))
        prev = n
    return out


def shaker():
    out, prev = [], 0.0
    for i in frames(0.18):
        t = i / RATE
        n = noise()
        env = min(1.0, t * 120) * math.exp(-t * 28)
        out.append((n - prev) * env)
        prev = n
    return out


def cowbell():
    return [(math.copysign(1, math.sin(2 * math.pi * 562 * i / RATE)) +
             math.copysign(1, math.sin(2 * math.pi * 845 * i / RATE))) *
            0.5 * math.exp(-i / RATE * 9) for i in frames(0.4)]


def clave():
    return [math.sin(2 * math.pi * 2500 * i / RATE) * math.exp(-i / RATE * 60)
            for i in frames(0.12)]


SOUNDS = [
    ("kick1", lambda: kick()),
    ("kick2", lambda: kick(punch=0.6, length=0.8, low=42)),
    ("snare1", lambda: snare()),
    ("snare2", lambda: snare(tone=230, snappy=0.9, length=0.25)),
    ("clap1", clap),
    ("rim1", rim),
    ("hat1", lambda: hat()),
    ("hat2", lambda: hat(decay=80, length=0.08)),
    ("openhat1", lambda: hat(decay=7, length=0.6)),
    ("tom1", lambda: tom(95)),
    ("tom2", lambda: tom(135)),
    ("tom3", lambda: tom(185)),
    ("crash1", crash),
    ("shaker1", shaker),
    ("cowbell1", cowbell),
    ("clave1", clave),
]


def write_wav(path, samples):
    peak = max(abs(s) for s in samples) or 1.0
    data = b"".join(struct.pack("<h", int(s / peak * 0.89 * 32767))
                    for s in samples)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--dest", default=str(root / "drum_kits"),
                        help="folder to put the Starter kit folder in")
    args = parser.parse_args()

    kit = Path(args.dest).expanduser() / "Starter"
    kit.mkdir(parents=True, exist_ok=True)
    lines = ['name: "Starter"', "mappings:"]
    for i, (name, make) in enumerate(SOUNDS):
        write_wav(kit / (name + ".wav"), make())
        lines.append("  - note_number: %d" % (FIRST_DRUM_NOTE + i))
        lines.append('    file_name: "%s.wav"' % name)
    # LMN-3 needs the .yaml named the same as its folder
    (kit / "Starter.yaml").write_text("\n".join(lines) + "\n")
    print("wrote %d sounds to %s" % (len(SOUNDS), kit))


if __name__ == "__main__":
    main()
