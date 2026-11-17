# Getting sounds onto the LMN-3

The LMN-3 reads its sounds from two folders when it starts:

| Folder | Holds | Used by |
|---|---|---|
| `~/.config/LMN-3/samples` | single sounds (`.wav`), each played across the keys | the **Sampler** |
| `~/.config/LMN-3/drum_kits` | one folder per kit: up to 24 sounds plus a `.yaml` file saying which key plays which | the **Drum Sampler** |

On the other versions the `LMN-3` folder is in a different place:
- **Windows:** `%APPDATA%\LMN-3`. Paste that into the File Explorer address bar.
- **Android:** `Android/data/com.fundamentalfrequency.lmn3/files/LMN-3`, visible when the phone is plugged into a computer in "File transfer" mode. It appears after the app has been opened once.

New sounds show up after LMN-3 is restarted.

This repository has two tools in [`tools/`](../tools) that do the converting and the `.yaml` writing for you. Both need only Python 3, with no extra packages.

## The Starter kit

[`drum_kits/Starter`](../drum_kits/Starter) is a 16-sound kit to get going: two kicks, two snares, clap, rim, two closed hats, open hat, three toms, crash, shaker, cowbell and clave. Every sound was synthesised from scratch by [`tools/make_starter_kit.py`](../tools/make_starter_kit.py), with no recordings, so it's free to use and share.

To install it on the Pi, from the repository folder:

```bash
cp -r drum_kits/Starter ~/.config/LMN-3/drum_kits/
```

## OP-1 patches: `op1_to_lmn3.py`

[`tools/op1_to_lmn3.py`](../tools/op1_to_lmn3.py) converts Teenage Engineering OP-1 patches, such as the thousands shared on [op1.fun](https://op1.fun), and ordinary sample packs.

| What you give it | What you get |
|---|---|
| OP-1 **drum** patch (one `.aif` holding up to 24 sounds) | a drum kit. Sound 1 lands on the LMN-3's first drum key, as on the OP-1 |
| OP-1 **synth sampler** patch (one sound played across the keys) | a `.wav` in `samples` |
| OP-1 **synth engine** preset (FM, Digital, Cluster…) | skipped: these hold only settings for the OP-1's own synths, no audio |
| a folder of ordinary one-shot samples, with `--kit NAME` | one drum kit called NAME |
| a folder of folders of one-shots, with `--kits` | one drum kit per folder, named after it |

**On the Pi** (it writes straight into `~/.config/LMN-3`):

```bash
python3 tools/op1_to_lmn3.py ~/Downloads/op1-patches              # every patch in a folder
python3 tools/op1_to_lmn3.py ~/Downloads/kick-pack.aif            # one patch
python3 tools/op1_to_lmn3.py --kit Lofi ~/Downloads/lofi-drums    # a sample pack -> one kit
python3 tools/op1_to_lmn3.py --kits ~/Downloads/drum-packs        # one kit per sub-folder
```

**On a computer**, to prepare everything first and copy it over afterwards:

```bash
python3 tools/op1_to_lmn3.py --dest LMN3-ready --kits ~/Downloads/drum-packs
```

That makes a `LMN3-ready` folder with `samples` and `drum_kits` inside. Copy both into the LMN-3 folder on the Pi, for example from Windows with PuTTY's `pscp`:

```
pscp -r LMN3-ready\samples LMN3-ready\drum_kits lmn3@<pi-address>:/home/lmn3/.config/LMN-3/
```

…or drag them across with WinSCP. Over Wi-Fi, large copies can drop out. A cable, or a USB stick plugged into the Pi, is more reliable.

### How the kits are put together

- **Key order:** a sample pack's sounds are put on the keys in a sensible order guessed from their file names: kicks, snares, claps, rims, closed hats, open hats, toms, cymbals, percussion, then everything else. Each sound is renamed to what it is (`kick1.wav`, `snare2.wav`…).
- **`sources.txt`:** written in each kit. It lists which original file went on which key.
- **Big packs:** kits with more than 24 sounds are split into `NAME`, `NAME_2`, and so on.
- **Skipped files:** loops, and anything longer than 8 seconds, are left out. Add `--with-loops` to keep them.
- **Existing kits** aren't overwritten unless you add `--force`.

## Writing a kit by hand

A kit is a folder with the sounds and a `.yaml` file **named the same as the folder**. Note 53 is the first drum key at octave +0:

```yaml
name: "MyKit"
mappings:
  - note_number: 53
    file_name: "kick1.wav"
  - note_number: 54
    file_name: "snare1.wav"
```

## About sharing sounds

Free sample packs are usually free to *use in your music*, but most licences don't allow passing the files themselves on, and OP-1 patches belong to whoever made them. That's why this repository ships only the Starter kit, which was made here, plus the tools to convert your own downloads.
