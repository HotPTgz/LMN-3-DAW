#include "MidiFxLogic.h"

namespace internal_plugins {
namespace midifx {

//==============================================================================
namespace Scales {

struct Scale {
    const char *name;
    std::vector<int> intervals;
};

static const std::vector<Scale> &getScales() {
    static const std::vector<Scale> scales{
        {"Major", {0, 2, 4, 5, 7, 9, 11}},
        {"Minor", {0, 2, 3, 5, 7, 8, 10}},
        {"Dorian", {0, 2, 3, 5, 7, 9, 10}},
        {"Phrygian", {0, 1, 3, 5, 7, 8, 10}},
        {"Lydian", {0, 2, 4, 6, 7, 9, 11}},
        {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}},
        {"Locrian", {0, 1, 3, 5, 6, 8, 10}},
        {"Harm Minor", {0, 2, 3, 5, 7, 8, 11}},
        {"Major Pent", {0, 2, 4, 7, 9}},
        {"Minor Pent", {0, 3, 5, 7, 10}},
        {"Blues", {0, 3, 5, 6, 7, 10}},
        {"Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    };
    return scales;
}

const juce::StringArray &getNames() {
    static const juce::StringArray names = [] {
        juce::StringArray n;
        for (auto &s : getScales())
            n.add(s.name);
        return n;
    }();
    return names;
}

const juce::StringArray &getRootNames() {
    static const juce::StringArray names{"C",  "C#", "D",  "D#", "E",  "F",
                                         "F#", "G",  "G#", "A",  "A#", "B"};
    return names;
}

static bool isInScale(int pitchClass, const Scale &scale) {
    return std::find(scale.intervals.begin(), scale.intervals.end(),
                     pitchClass) != scale.intervals.end();
}

int snapToScale(int note, int root, int scaleIndex) {
    const auto &scales = getScales();
    scaleIndex = juce::jlimit(0, int(scales.size()) - 1, scaleIndex);
    root = ((root % 12) + 12) % 12;
    const auto &scale = scales[size_t(scaleIndex)];

    auto pitchClassOf = [root](int n) { return (((n - root) % 12) + 12) % 12; };

    if (isInScale(pitchClassOf(note), scale))
        return note;

    // nearest scale note; prefer up on a tie
    for (int distance = 1; distance <= 6; distance++) {
        if (note + distance <= 127 && isInScale(pitchClassOf(note + distance), scale))
            return note + distance;
        if (note - distance >= 0 && isInScale(pitchClassOf(note - distance), scale))
            return note - distance;
    }
    return note;
}

} // namespace Scales

//==============================================================================
namespace Chords {

struct Chord {
    const char *name;
    std::vector<int> intervals;
};

static const std::vector<Chord> &getChords() {
    static const std::vector<Chord> chords{
        {"Major", {0, 4, 7}},       {"Minor", {0, 3, 7}},
        {"7", {0, 4, 7, 10}},       {"Maj7", {0, 4, 7, 11}},
        {"Min7", {0, 3, 7, 10}},    {"Sus2", {0, 2, 7}},
        {"Sus4", {0, 5, 7}},        {"Dim", {0, 3, 6}},
        {"Aug", {0, 4, 8}},         {"Add9", {0, 4, 7, 14}},
        {"Min9", {0, 3, 7, 10, 14}}, {"5th", {0, 7}},
        {"Octave", {0, 12}},
    };
    return chords;
}

const juce::StringArray &getNames() {
    static const juce::StringArray names = [] {
        juce::StringArray n;
        for (auto &c : getChords())
            n.add(c.name);
        return n;
    }();
    return names;
}

const juce::StringArray &getInversionNames() {
    static const juce::StringArray names{"Root", "1st", "2nd", "3rd"};
    return names;
}

const juce::StringArray &getSpreadNames() {
    static const juce::StringArray names{"Close", "Open"};
    return names;
}

juce::Array<int> build(int rootNote, int chordIndex, int inversion,
                       int spread) {
    const auto &chords = getChords();
    chordIndex = juce::jlimit(0, int(chords.size()) - 1, chordIndex);

    juce::Array<int> notes;
    for (int interval : chords[size_t(chordIndex)].intervals)
        notes.add(rootNote + interval);

    // inversions: move the lowest note up an octave, repeatedly
    inversion = juce::jlimit(0, notes.size() - 1, inversion);
    for (int i = 0; i < inversion; i++) {
        notes.sort();
        notes.set(0, notes[0] + 12);
    }
    notes.sort();

    // open voicing: move the second-lowest note up an octave
    if (spread > 0 && notes.size() >= 3) {
        notes.set(1, notes[1] + 12);
        notes.sort();
    }

    juce::Array<int> clipped;
    for (int n : notes)
        if (n >= 0 && n <= 127)
            clipped.addIfNotAlreadyThere(n);
    return clipped;
}

} // namespace Chords

//==============================================================================
namespace Arp {

const juce::StringArray &getModeNames() {
    static const juce::StringArray names{"Up", "Down", "Up/Down", "Random",
                                         "As played"};
    return names;
}

const juce::StringArray &getRateNames() {
    static const juce::StringArray names{"1/4",  "1/8T", "1/8",
                                         "1/16T", "1/16", "1/32"};
    return names;
}

double getRateBeats(int rateIndex) {
    static const double beats[] = {1.0, 1.0 / 3.0, 0.5, 1.0 / 6.0, 0.25, 0.125};
    return beats[juce::jlimit(0, 5, rateIndex)];
}

} // namespace Arp

//==============================================================================
juce::Array<int> ArpEngine::buildSequence(const Settings &settings) const {
    juce::Array<int> base;
    for (auto &h : held)
        base.add(h.note);

    if (settings.mode != Arp::asPlayed)
        base.sort();

    juce::Array<int> sequence;
    int octaves = juce::jlimit(1, 4, settings.octaves);
    for (int octave = 0; octave < octaves; octave++)
        for (int n : base)
            if (n + 12 * octave <= 127)
                sequence.add(n + 12 * octave);
    return sequence;
}

int ArpEngine::nextNote(const Settings &settings, int &velocity) {
    auto sequence = buildSequence(settings);
    if (sequence.isEmpty())
        return -1;

    int size = sequence.size();
    int index = 0;

    switch (settings.mode) {
    case Arp::down:
        index = size - 1 - (stepIndex % size);
        stepIndex++;
        break;
    case Arp::upDown:
        if (size == 1) {
            index = 0;
        } else {
            // up then down, without repeating the top and bottom notes
            int period = 2 * (size - 1);
            int position = stepIndex % period;
            index = position < size ? position : period - position;
        }
        stepIndex++;
        break;
    case Arp::random:
        index = random.nextInt(size);
        break;
    case Arp::up:
    case Arp::asPlayed:
    default:
        index = stepIndex % size;
        stepIndex++;
        break;
    }

    int note = sequence[index];
    // use the velocity the source key was played with (the same note, or
    // the note it was transposed from by the octave setting)
    velocity = 100;
    for (auto &h : held) {
        if (note >= h.note && (note - h.note) % 12 == 0) {
            velocity = h.velocity;
            break;
        }
    }
    return note;
}

void ArpEngine::addNoteOff(tracktion::MidiMessageArray &midi, double time) {
    if (soundingNote >= 0) {
        midi.addMidiMessage(
            juce::MidiMessage::noteOff(midiChannel, soundingNote),
            juce::jmax(0.0, time), tracktion::MidiMessageArray::notMPE);
        soundingNote = -1;
    }
}

void ArpEngine::reset(tracktion::MidiMessageArray &midi, double time) {
    addNoteOff(midi, time);
    held.clear();
    keysDown.clear();
    stepIndex = 0;
    lastStepBeat = -1.0;
}

void ArpEngine::process(tracktion::MidiMessageArray &midi,
                        double blockStartBeat, double beatsPerSecond,
                        double blockSeconds, const Settings &settings) {
    // Take the incoming notes out of the buffer, updating the held notes
    tracktion::MidiMessageArray passThrough;
    bool hadAllNotesOff = midi.isAllNotesOff;

    for (auto &m : midi) {
        if (m.isNoteOn()) {
            midiChannel = m.getChannel();
            // with latch, a new chord after all keys were released replaces
            // the old one
            if (settings.latch && keysDown.isEmpty())
                held.clear();
            keysDown.addIfNotAlreadyThere(m.getNoteNumber());

            bool found = false;
            for (auto &h : held)
                if (h.note == m.getNoteNumber())
                    found = true;
            if (!found) {
                held.add({m.getNoteNumber(), m.getVelocity()});
                // restart the pattern when the first note is pressed
                if (held.size() == 1)
                    stepIndex = 0;
            }
        } else if (m.isNoteOff()) {
            keysDown.removeFirstMatchingValue(m.getNoteNumber());
            if (!settings.latch) {
                for (int i = held.size(); --i >= 0;)
                    if (held.getReference(i).note == m.getNoteNumber())
                        held.remove(i);
            }
        } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
            hadAllNotesOff = true;
            passThrough.add(m);
        } else {
            passThrough.add(m);
        }
    }

    midi.clear();
    midi.isAllNotesOff = hadAllNotesOff;
    midi.mergeFrom(passThrough);

    if (hadAllNotesOff) {
        reset(midi, 0.0);
        return;
    }

    // latch turned off while nothing is held down
    if (!settings.latch && keysDown.isEmpty())
        held.clear();

    if (beatsPerSecond <= 0.0 || blockSeconds <= 0.0)
        return;

    const double rate = juce::jmax(1.0e-3, settings.rateBeats);
    const double blockEndBeat = blockStartBeat + blockSeconds * beatsPerSecond;
    auto beatToTime = [&](double beat) {
        return (beat - blockStartBeat) / beatsPerSecond;
    };

    // pending note off from the previous block
    if (soundingNote >= 0 && noteOffBeat < blockEndBeat) {
        addNoteOff(midi, beatToTime(noteOffBeat));
    }

    if (held.isEmpty()) {
        addNoteOff(midi, 0.0);
        return;
    }

    // every step boundary inside this block
    double firstStep = std::ceil(blockStartBeat / rate - 1.0e-9) * rate;
    for (double stepBeat = firstStep; stepBeat < blockEndBeat - 1.0e-9;
         stepBeat += rate) {
        if (std::abs(stepBeat - lastStepBeat) < 1.0e-9)
            continue; // already played (block boundaries can repeat a beat)
        lastStepBeat = stepBeat;

        double time = beatToTime(stepBeat);
        addNoteOff(midi, time);

        int velocity = 100;
        int note = nextNote(settings, velocity);
        if (note < 0)
            break;

        midi.addMidiMessage(juce::MidiMessage::noteOn(midiChannel, note,
                                                      juce::uint8(velocity)),
                            juce::jmax(0.0, time),
                            tracktion::MidiMessageArray::notMPE);
        soundingNote = note;
        noteOffBeat = stepBeat +
                      rate * juce::jlimit(0.05, 1.0, settings.gate) - 1.0e-6;

        if (noteOffBeat < blockEndBeat)
            addNoteOff(midi, beatToTime(noteOffBeat));
    }

    midi.sortByTimestamp();
}

} // namespace midifx
} // namespace internal_plugins
