#!/usr/bin/env python3
"""
op1_to_lmn3.py - use OP-1 patches (e.g. from op1.fun) on the LMN-3.

  * OP-1 drum patches (one .aif holding up to 24 sounds) become LMN-3 drum
    kits: a folder in ~/.config/LMN-3/drum_kits with one .wav per sound and
    the .yaml mapping file. Sound 1 plays on the LMN-3's first drum key, just
    like on the OP-1.
  * OP-1 synth sampler patches (one sound played across the keys) are copied
    to ~/.config/LMN-3/samples as .wav files.
  * OP-1 synth engine presets (FM, Digital, Cluster, ...) only hold settings
    for the OP-1's own synths, not audio, so they are skipped.

  * Folders of ordinary one-shot samples (.wav or .aif, e.g. a free drum
    pack) become LMN-3 drum kits with --kit or --kits. Sounds are put in a
    sensible key order guessed from their names: kicks, snares, claps, rims,
    closed hats, open hats, toms, cymbals, percussion, then everything else.

Usage:
    python3 op1_to_lmn3.py PATCH.aif [MORE.aif ...]
    python3 op1_to_lmn3.py ~/Downloads/op1-patches        (a whole folder)

    python3 op1_to_lmn3.py --kit MyKit ~/Downloads/pack   (one kit from a folder)
    python3 op1_to_lmn3.py --kits ~/Downloads/pack        (a kit per sub-folder)

Options:
    --dest DIR      LMN-3 settings folder (default: ~/.config/LMN-3)
    --force         replace kits/samples that already exist
    --kit NAME      build one drum kit called NAME from the one-shots given
    --kits          build one drum kit per folder of one-shots, named after it
    --with-loops    also use files that look like loops (skipped by default)

Only uses the Python standard library. Restart LMN-3 afterwards: it picks up
new samples and kits when it starts.
"""

import argparse
import json
import math
import re
import shutil
import struct
import sys
import wave
from pathlib import Path

# OP-1 drum start/end points: 0x7FFFFFFE spans 12 seconds of 44.1 kHz mono
# 16-bit audio, which works out to 4058 units per sample frame.
OP1_UNITS_PER_FRAME = 4058
FIRST_DRUM_NOTE = 53  # the LMN-3's first drum key (F3)
NUM_DRUM_SOUNDS = 24
SYNTH_ENGINE_TYPES = {"cluster", "digital", "dna", "drwave", "dsynth", "fm",
                      "phase", "pulse", "string", "voltage", "dimension",
                      "vocoder", "iter"}


class Op1Error(Exception):
    pass


# --------------------------------------------------------------------------
# AIFF / AIFF-C reading


def _ieee_extended(b):
    """80-bit IEEE 754 extended float (the AIFF sample rate)."""
    exponent = ((b[0] & 0x7F) << 8) | b[1]
    mantissa = int.from_bytes(b[2:10], "big")
    if exponent == 0 and mantissa == 0:
        return 0.0
    sign = -1 if b[0] & 0x80 else 1
    return sign * mantissa * math.pow(2.0, exponent - 16383 - 63)


def read_aiff(path):
    """Returns (frames, channels, sample_width, rate, metadata) where frames
    is little-endian PCM bytes and metadata is the OP-1 JSON (or None)."""
    data = Path(path).read_bytes()
    if len(data) < 12 or data[0:4] != b"FORM" or data[8:12] not in (b"AIFF", b"AIFC"):
        raise Op1Error("not an AIFF file")
    is_aifc = data[8:12] == b"AIFC"

    channels = frames = bits = None
    rate = 44100.0
    little_endian = False
    sound = None
    metadata = None

    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"COMM":
            channels, frames, bits = struct.unpack(">hIh", body[0:8])
            rate = _ieee_extended(body[8:18])
            if is_aifc and len(body) >= 22:
                compression = body[18:22]
                if compression == b"sowt":
                    little_endian = True
                elif compression not in (b"NONE", b"twos"):
                    raise Op1Error("unsupported AIFF-C compression %r"
                                   % compression.decode("latin-1"))
        elif cid == b"SSND":
            offset = struct.unpack(">I", body[0:4])[0]
            sound = body[8 + offset:]
        elif cid == b"APPL" and body[0:4] == b"op-1":
            text = body[4:].split(b"\x00")[0].decode("utf-8", "replace")
            try:
                metadata = json.loads(text)
            except ValueError:
                metadata = None
        pos += 8 + size + (size & 1)  # chunks are padded to even sizes

    if channels is None or sound is None:
        raise Op1Error("missing COMM or SSND chunk")
    if bits not in (8, 16, 24, 32):
        raise Op1Error("unsupported bit depth %d" % bits)

    width = bits // 8
    sound = sound[:frames * channels * width]
    if width == 1:
        # AIFF 8-bit is signed, WAV 8-bit is unsigned
        sound = bytes((b + 128) & 0xFF for b in sound)
    elif not little_endian:
        swapped = bytearray(len(sound))
        for i in range(width):
            swapped[i::width] = sound[width - 1 - i::width]
        sound = bytes(swapped)
    return sound, channels, width, int(round(rate)), metadata


def write_wav(path, pcm, channels, width, rate):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(width)
        w.setframerate(rate)
        w.writeframes(pcm)


def reverse_frames(pcm, frame_size):
    frames = [pcm[i:i + frame_size] for i in range(0, len(pcm), frame_size)]
    return b"".join(reversed(frames))


# --------------------------------------------------------------------------
# Conversion


def safe_name(text, fallback):
    name = re.sub(r"[^A-Za-z0-9 _.-]+", "", str(text)).strip(" .")
    name = re.sub(r"\s+", "_", name)
    return name or fallback


def convert_drum(path, pcm, channels, width, rate, meta, dest, force):
    kit_name = safe_name(Path(path).stem, "op1_kit")
    kit_dir = dest / "drum_kits" / kit_name
    if kit_dir.exists() and not force:
        return "skipped (kit %s already exists, use --force)" % kit_name
    kit_dir.mkdir(parents=True, exist_ok=True)

    frame_size = channels * width
    total_frames = len(pcm) // frame_size
    starts = list(meta.get("start", []))[:NUM_DRUM_SOUNDS]
    ends = list(meta.get("end", []))[:NUM_DRUM_SOUNDS]

    # positions assume 44.1 kHz; scale if the file uses another rate
    scale = rate / 44100.0

    def to_frame(value):
        frame = int(round(int(value) / OP1_UNITS_PER_FRAME * scale))
        return max(0, min(total_frames, frame))

    silent = "empty.wav"
    files = []
    seen = {}
    used_silent = False
    for i in range(NUM_DRUM_SOUNDS):
        if i >= len(starts) or i >= len(ends):
            files.append(silent)
            used_silent = True
            continue
        a, b = to_frame(starts[i]), to_frame(ends[i])
        backwards = a > b  # the OP-1 plays a slice backwards when end < start
        lo, hi = min(a, b), max(a, b)
        if hi - lo < 16:
            files.append(silent)
            used_silent = True
            continue
        key = (lo, hi, backwards)
        if key in seen:  # several keys sharing one sound
            files.append(seen[key])
            continue
        chunk = pcm[lo * frame_size:hi * frame_size]
        if backwards:
            chunk = reverse_frames(chunk, frame_size)
        name = "%02d.wav" % (i + 1)
        write_wav(kit_dir / name, chunk, channels, width, rate)
        seen[key] = name
        files.append(name)

    if used_silent:
        # keeps all 24 keys in place so the pads line up with the OP-1 keys
        write_wav(kit_dir / silent, b"\x00" * frame_size * 441, channels,
                  width, rate)

    lines = ['name: "%s"' % kit_name, "mappings:"]
    for i, name in enumerate(files):
        lines.append("  - note_number: %d" % (FIRST_DRUM_NOTE + i))
        lines.append('    file_name: "%s"' % name)
    # LMN-3 needs the .yaml named the same as its folder
    (kit_dir / (kit_name + ".yaml")).write_text("\n".join(lines) + "\n")

    sounds = len(set(f for f in files if f != silent))
    return "drum kit %s (%d sounds)" % (kit_name, sounds)


def convert_sample(path, pcm, channels, width, rate, dest, force):
    name = safe_name(Path(path).stem, "op1_sample")
    out = dest / "samples" / (name + ".wav")
    if out.exists() and not force:
        return "skipped (sample %s already exists, use --force)" % out.name
    out.parent.mkdir(parents=True, exist_ok=True)
    write_wav(out, pcm, channels, width, rate)
    return "sample %s" % out.name


def convert(path, dest, force):
    pcm, channels, width, rate, meta = read_aiff(path)
    kind = (meta or {}).get("type", "")
    if kind == "drum":
        return convert_drum(path, pcm, channels, width, rate, meta, dest, force)
    if kind in SYNTH_ENGINE_TYPES:
        return "skipped (OP-1 %s synth preset, no sound to use)" % kind
    # "sampler" patches, and plain AIFF files without OP-1 data
    return convert_sample(path, pcm, channels, width, rate, dest, force)


# --------------------------------------------------------------------------
# Drum kits from ordinary one-shot samples (--kit / --kits)

ONE_SHOT_TYPES = (".wav", ".aif", ".aiff")
MAX_ONE_SHOT_SECONDS = 8.0
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# (role, short name used for the file and the sequencer row, words that
# give it away). Checked in order, so "snare rim" is a rim and "open hat"
# isn't a closed hat. Short words must match a whole word of the name,
# longer ones may appear anywhere in it.
ROLES = [
    ("open hat", "openhat", ["ohh", "oh", "openhat", "openhh"]),
    ("rim", "rim", ["rim", "rimshot", "rs", "sidestick", "stick", "xstick"]),
    ("kick", "kick", ["kick", "kik", "kck", "bd", "bassdrum", "kd"]),
    ("snare", "snare", ["snare", "snr", "sd", "sn"]),
    ("clap", "clap", ["clap", "clp", "cp", "handclap"]),
    ("closed hat", "hat", ["hihat", "hat", "hh", "chh", "ch", "closedhat",
                           "closed", "hats"]),
    ("tom", "tom", ["tom", "toms", "tm", "lt", "mt", "ht", "floortom"]),
    ("crash", "crash", ["crash", "cymbal", "cym", "splash", "china", "cy"]),
    ("ride", "ride", ["ride", "rd"]),
    ("percussion", "perc", ["perc", "percussion", "shaker", "shk", "tamb",
                            "tambourine", "cowbell", "cb", "conga", "bongo",
                            "clave", "claves", "block", "woodblock", "guiro",
                            "cabasa", "maraca", "maracas", "triangle", "bell",
                            "timbale", "agogo", "snap", "fingersnap"]),
]
# the order sounds go on the keys, starting at F3
KEY_ORDER = ["kick", "snare", "clap", "rim", "closed hat", "open hat", "tom",
             "crash", "ride", "percussion"]
LONG_WORD = 4  # words this long or longer may appear inside other words
WHOLE_WORD_ONLY = {"ride", "bell", "block", "snap", "stick", "open"}


def note_name(number):
    return "%s%d" % (NOTE_NAMES[number % 12], number // 12 - 1)


def words_of(text):
    """'ClosedHat_02' -> ['closed', 'hat', '02', 'closedhat02']"""
    text = re.sub(r"([a-z])([A-Z])", r"\1 \2", text)
    text = re.sub(r"([A-Za-z])([0-9])|([0-9])([A-Za-z])",
                  lambda m: " ".join(g for g in m.groups() if g), text)
    words = [w for w in re.split(r"[^a-z0-9]+", text.lower()) if w]
    return words


def guess_role(path):
    """Index into ROLES (len(ROLES) = unknown), from the file name and, if
    that says nothing, the folder it's in ("Kicks/001.wav")."""
    both = "".join(words_of(path.stem) + words_of(path.parent.name))
    if "open" in words_of(path.stem) and ("hat" in both or "hh" in both):
        return 0  # "Open 1" in a "Hats" folder
    for text in (path.stem, path.parent.name):
        words = words_of(text)
        joined = "".join(words)
        for i, (_, _, keys) in enumerate(ROLES):
            for key in keys:
                if key in words or (len(key) >= LONG_WORD and key in joined
                                     and key not in WHOLE_WORD_ONLY):
                    return i
    return len(ROLES)


def natural_key(path):
    return [int(t) if t.isdigit() else t.lower()
            for t in re.split(r"(\d+)", path.name)]


def wav_seconds(path):
    """Length of a WAV file, read from its header so any WAV flavour works
    (24-bit, float, WAVE_FORMAT_EXTENSIBLE). None if it can't be read."""
    data = Path(path).read_bytes()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        return None
    pos, rate, block, size = 12, None, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        length = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        if cid == b"fmt ":
            _, _, rate, _, block = struct.unpack(
                "<HHIIH", data[pos + 8:pos + 22])
        elif cid == b"data":
            size = min(length, len(data) - pos - 8)
        pos += 8 + length + (length & 1)
    if not rate or not block or size is None:
        return None
    return size / float(block) / rate


def gather_one_shots(folder_or_files, with_loops, recursive=True):
    """Returns (usable, skipped) where skipped holds (path, reason)."""
    usable, skipped = [], []
    for item in folder_or_files:
        if item.is_dir():
            found = item.rglob("*") if recursive else item.iterdir()
            candidates = [f for f in found if f.is_file()]
        else:
            candidates = [item]
        for f in candidates:
            if f.suffix.lower() not in ONE_SHOT_TYPES or f.name.startswith("."):
                continue
            words = words_of(f.stem) + words_of(f.parent.name)
            if not with_loops and ("loop" in words or "loops" in words
                                   or "bpm" in words):
                skipped.append((f, "looks like a loop"))
                continue
            try:
                if f.suffix.lower() == ".wav":
                    seconds = wav_seconds(f)
                    if seconds is None:
                        raise Op1Error("not a WAV file it can read")
                else:
                    pcm, channels, width, rate, meta = read_aiff(f)
                    if (meta or {}).get("type") == "drum":
                        raise Op1Error("an OP-1 drum kit: convert it "
                                       "without --kit")
                    seconds = len(pcm) / float(channels * width * rate)
            except (Op1Error, OSError, ValueError, struct.error) as e:
                skipped.append((f, str(e)))
                continue
            if not with_loops and seconds > MAX_ONE_SHOT_SECONDS:
                skipped.append((f, "%.0f s long, too long for a drum hit"
                                % seconds))
                continue
            usable.append(f)
    return usable, skipped


def build_kit(name, sounds, dest, force):
    """Writes one kit (at most NUM_DRUM_SOUNDS sounds, already in order)."""
    kit_name = safe_name(name, "kit")
    kit_dir = dest / "drum_kits" / kit_name
    if kit_dir.exists() and not force:
        return "skipped (kit %s already exists, use --force)" % kit_name, []
    if kit_dir.exists():
        for old in kit_dir.iterdir():
            if old.is_file():
                old.unlink()
    kit_dir.mkdir(parents=True, exist_ok=True)

    counts = {}
    lines = ['name: "%s"' % kit_name, "mappings:"]
    layout = []
    for i, (role, source) in enumerate(sounds):
        short = ROLES[role][1] if role < len(ROLES) else safe_name(
            source.stem, "sound").lower()[:12]
        counts[short] = counts.get(short, 0) + 1
        file_name = "%s%d.wav" % (short, counts[short])
        target = kit_dir / file_name
        if source.suffix.lower() == ".wav":
            shutil.copyfile(source, target)
        else:
            pcm, channels, width, rate, _ = read_aiff(source)
            write_wav(target, pcm, channels, width, rate)
        note = FIRST_DRUM_NOTE + i
        lines.append("  - note_number: %d" % note)
        lines.append('    file_name: "%s"' % file_name)
        layout.append((note, file_name, source))

    # LMN-3 needs the .yaml named the same as its folder
    (kit_dir / (kit_name + ".yaml")).write_text("\n".join(lines) + "\n")
    (kit_dir / "sources.txt").write_text("".join(
        "%-4s %-14s %s\n" % (note_name(n), f, s) for n, f, s in layout))
    return "drum kit %s (%d sound%s)" % (kit_name, len(sounds),
                                         "" if len(sounds) == 1 else "s"), layout


def make_kits(name, folders, dest, force, with_loops, recursive=True):
    """One kit from everything in `folders`; split into NAME, NAME_2, ...
    when there are more than 24 sounds. Returns the number of kits made."""
    files, skipped = gather_one_shots(folders, with_loops, recursive)
    for f, why in skipped:
        print("  skipped %s (%s)" % (f.name, why))
    if not files:
        print("%s: no usable one-shots found" % name)
        return 0

    def place(role):
        return KEY_ORDER.index(ROLES[role][0]) if role < len(ROLES) else 99

    ordered = sorted(((guess_role(f), f) for f in files),
                     key=lambda rf: (place(rf[0]), natural_key(rf[1])))
    parts = [ordered[i:i + NUM_DRUM_SOUNDS]
             for i in range(0, len(ordered), NUM_DRUM_SOUNDS)]
    if len(parts) > 1:
        print("%s: %d sounds, more than the %d keys, so making %d kits"
              % (name, len(ordered), NUM_DRUM_SOUNDS, len(parts)))
    made = 0
    for n, part in enumerate(parts):
        kit = name if n == 0 else "%s_%d" % (name, n + 1)
        result, layout = build_kit(kit, part, dest, force)
        print("%s: %s" % (kit, result))
        for note, file_name, source in layout:
            print("    %-4s %-14s <- %s" % (note_name(note), file_name,
                                           source.name))
        if layout:
            made += 1
    return made


def kit_folders(roots):
    """Every folder (roots included) that directly holds audio files."""
    found = []
    for root in roots:
        for folder in [root] + sorted(p for p in root.rglob("*")
                                      if p.is_dir()):
            if any(f.is_file() and f.suffix.lower() in ONE_SHOT_TYPES
                   for f in folder.iterdir()):
                found.append(folder)
    return found


def main():
    parser = argparse.ArgumentParser(
        description="Turn OP-1 patches into LMN-3 drum kits and samples.")
    parser.add_argument("inputs", nargs="+",
                        help=".aif files or folders containing them "
                             "(with --kit/--kits: .wav/.aif one-shots or "
                             "folders of them)")
    parser.add_argument("--dest", default="~/.config/LMN-3",
                        help="LMN-3 settings folder (default ~/.config/LMN-3)")
    parser.add_argument("--force", action="store_true",
                        help="replace existing kits/samples")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--kit", metavar="NAME",
                       help="make one drum kit called NAME from the "
                            ".wav/.aif one-shots given")
    group.add_argument("--kits", action="store_true",
                       help="make one drum kit per folder of one-shots, "
                            "named after the folder")
    parser.add_argument("--with-loops", action="store_true",
                        help="also use loops and sounds over 8 seconds")
    args = parser.parse_args()
    dest = Path(args.dest).expanduser()

    if args.kit or args.kits:
        inputs = []
        for item in args.inputs:
            p = Path(item).expanduser()
            if p.exists():
                inputs.append(p)
            else:
                print("not found: %s" % p)
        if not inputs:
            return 1
        made = 0
        if args.kit:
            made = make_kits(args.kit, inputs, dest, args.force,
                             args.with_loops)
        else:
            folders = kit_folders([p for p in inputs if p.is_dir()])
            if not folders:
                print("No folders with .wav or .aif files found.")
                return 1
            used = set()
            for folder in folders:
                name = safe_name(folder.name, "kit")
                if name in used:  # two folders called "Kit 1"
                    name = safe_name(folder.parent.name + "_" + folder.name,
                                     "kit")
                used.add(name)
                made += make_kits(name, [folder], dest, args.force,
                                  args.with_loops, recursive=False)
        print("\nDone: %d drum kit%s made in %s. Restart LMN-3 to see them."
              % (made, "" if made == 1 else "s", dest / "drum_kits"))
        return 0

    files = []
    for item in args.inputs:
        p = Path(item).expanduser()
        if p.is_dir():
            files += sorted(f for f in p.rglob("*")
                            if f.suffix.lower() in (".aif", ".aiff"))
        elif p.exists():
            files.append(p)
        else:
            print("not found: %s" % p)

    if not files:
        print("No .aif files found.")
        return 1

    made = 0
    for f in files:
        try:
            result = convert(f, dest, args.force)
        except (Op1Error, OSError, ValueError, struct.error) as e:
            result = "skipped (%s)" % e
        if not result.startswith("skipped"):
            made += 1
        print("%s: %s" % (f.name, result))

    print("\nDone: %d of %d files converted. Restart LMN-3 to see them."
          % (made, len(files)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
