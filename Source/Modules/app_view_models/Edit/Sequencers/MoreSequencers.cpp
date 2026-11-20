#include "MoreSequencers.h"

namespace app_view_models::Sequencers {

juce::StringArray kitRowNames(tracktion::AudioTrack &track) {
    juce::StringArray names;
    for (int row = 0; row < drumRows; row++)
        names.add("SOUND " + juce::String(row + 1));
    // the Drum Synth repeats its 8 pads up the keyboard
    if (!track.pluginList
             .getPluginsOfType<internal_plugins::DrumSynthPlugin>()
             .isEmpty()) {
        auto &pads = internal_plugins::instruments::DrumKit::getPadNames();
        for (int row = 0; row < drumRows; row++)
            names.set(row, pads[row % pads.size()].toUpperCase());
    }
    for (auto sampler :
         track.pluginList.getPluginsOfType<tracktion::SamplerPlugin>())
        for (int i = 0; i < sampler->getNumSounds(); i++) {
            int row = sampler->getKeyNote(i) - firstDrumNote;
            if (juce::isPositiveAndBelow(row, drumRows))
                names.set(row, sampler->getSoundName(i)
                                   .upToLastOccurrenceOf(".", false, false)
                                   .toUpperCase());
        }
    return names;
}

//==============================================================================
juce::Array<bool> euclid(int hits, int steps, int rotate) {
    steps = juce::jlimit(1, 32, steps);
    hits = juce::jlimit(0, steps, hits);
    juce::Array<bool> pattern;
    for (int i = 0; i < steps; i++)
        pattern.add(((i * hits) % steps) < hits);
    juce::Array<bool> turned;
    int shift = ((rotate % steps) + steps) % steps;
    for (int i = 0; i < steps; i++)
        turned.add(pattern[(i - shift + steps) % steps]);
    return turned;
}

static int gcdOf(int a, int b) { return b == 0 ? a : gcdOf(b, a % b); }

int euclidLengthSteps(const EuclidInput &input) {
    int length = 0;
    for (auto &ring : input.rings) {
        if (ring.hits <= 0)
            continue;
        int s = juce::jlimit(1, 32, ring.steps);
        length = length == 0 ? s : length / gcdOf(length, s) * s;
        if (length > 64)
            return 64;
    }
    if (length == 0)
        return 16;
    // at least a bar, in whole repeats
    while (length < 16)
        length *= 2;
    return juce::jmin(64, length);
}

std::vector<NoteEvent> generateEuclid(const EuclidInput &input) {
    std::vector<NoteEvent> events;
    const double step = 0.25;
    const int length = euclidLengthSteps(input);
    for (int row = 0; row < drumRows; row++) {
        auto &ring = input.rings[size_t(row)];
        if (ring.hits <= 0)
            continue;
        auto pattern = euclid(ring.hits, ring.steps, ring.rotate);
        for (int i = 0; i < length; i++) {
            int index = i % pattern.size();
            if (!pattern[index])
                continue;
            // the ring's first step is its downbeat: a little accent
            int velocity = juce::jlimit(
                1, 127, input.velocity + (index == 0 ? 15 : 0));
            events.push_back({firstDrumNote + row,
                              i * step + swingOffset(i, input.swing, step),
                              step * 0.5, velocity});
        }
    }
    std::sort(events.begin(), events.end(),
              [](const NoteEvent &a, const NoteEvent &b) {
                  return a.start < b.start;
              });
    return events;
}

//==============================================================================
std::vector<int> turingNotes(const TuringInput &input) {
    const int length = juce::jlimit(2, 16, input.length);
    const int steps = int(std::round(input.bars * 4.0 /
                                     juce::jmax(0.0625, input.stepLength)));
    const juce::uint32 mask = (1u << length) - 1u;
    juce::Random random(input.seed * 7919 + 13);
    juce::uint32 reg = juce::uint32(random.nextInt()) & mask;
    if (reg == 0)
        reg = 1;
    const int span = juce::jlimit(1, 4, input.range) * 12;
    std::vector<int> notes;
    for (int i = 0; i < steps; i++) {
        // the bit going round: flipped now and then unless locked
        juce::uint32 out = (reg >> (length - 1)) & 1u;
        if (random.nextDouble() >= input.lock)
            out ^= 1u;
        reg = ((reg << 1) | out) & mask;
        // the low 8 bits pick the note
        int value = int(reg & 0xffu);
        if (length < 8) // fewer bits: spread them over the range
            value = int((reg * 255u) / juce::jmax(1u, mask));
        int note = input.root + value * span / 256;
        notes.push_back(internal_plugins::midifx::Scales::snapToScale(
            juce::jlimit(0, 127, note), input.root % 12, input.scale));
    }
    return notes;
}

double turingLength(const TuringInput &input) { return input.bars * 4.0; }

std::vector<NoteEvent> generateTuring(const TuringInput &input) {
    std::vector<NoteEvent> events;
    auto notes = turingNotes(input);
    for (size_t i = 0; i < notes.size(); i++)
        events.push_back({notes[i], double(i) * input.stepLength,
                          input.stepLength * juce::jlimit(0.1, 1.0, input.gate),
                          100});
    return events;
}

//==============================================================================
namespace Chords {
static const char *noteNames[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
static const int majorScale[] = {0, 2, 4, 5, 7, 9, 11};
static const int minorScale[] = {0, 2, 3, 5, 7, 8, 10};

const juce::StringArray &getKeyNames() {
    static juce::StringArray names = [] {
        juce::StringArray n;
        for (int i = 0; i < 12; i++)
            n.add(juce::String(noteNames[i]) + " major");
        for (int i = 0; i < 12; i++)
            n.add(juce::String(noteNames[i]) + " minor");
        return n;
    }();
    return names;
}

static int scaleNote(int key, int degree) {
    key = juce::jlimit(0, 23, key);
    const int *scale = key < 12 ? majorScale : minorScale;
    int octave = degree / 7;
    return (key % 12) + scale[degree % 7] + 12 * octave;
}

juce::Array<int> triad(int key, int degree) {
    degree = juce::jlimit(0, 6, degree);
    int root = scaleNote(key, degree);
    // the root near C4: from G3 up to F#4
    int base = 60 + root % 12;
    if (root % 12 >= 7)
        base -= 12;
    juce::Array<int> notes;
    for (int k = 0; k < 3; k++)
        notes.add(base + scaleNote(key, degree + 2 * k) - root);
    return notes;
}

// 0 major, 1 minor, 2 diminished
static int quality(int key, int degree) {
    int root = scaleNote(key, degree);
    int third = scaleNote(key, degree + 2) - root;
    int fifth = scaleNote(key, degree + 4) - root;
    if (fifth == 6)
        return 2;
    return third == 4 ? 0 : 1;
}

juce::String chordName(int key, int degree) {
    int root = scaleNote(key, juce::jlimit(0, 6, degree)) % 12;
    static const char *suffix[] = {"", "m", "\xc2\xb0"};
    return juce::String(noteNames[root]) +
           juce::String::fromUTF8(suffix[quality(key, degree)]);
}

juce::String romanName(int key, int degree) {
    static const char *upper[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    degree = juce::jlimit(0, 6, degree);
    int q = quality(key, degree);
    juce::String r = upper[degree];
    if (q != 0)
        r = r.toLowerCase();
    if (q == 2)
        r << juce::String::fromUTF8("\xc2\xb0");
    return r;
}

const juce::StringArray &getPresetNames() {
    static juce::StringArray names{
        juce::String::fromUTF8("I \xc2\xb7 V \xc2\xb7 vi \xc2\xb7 IV"),
        juce::String::fromUTF8("ii \xc2\xb7 V \xc2\xb7 I \xc2\xb7 I"),
        juce::String::fromUTF8("i \xc2\xb7 VI \xc2\xb7 III \xc2\xb7 VII"),
        juce::String::fromUTF8("I \xc2\xb7 vi \xc2\xb7 IV \xc2\xb7 V"),
        juce::String::fromUTF8("vi \xc2\xb7 IV \xc2\xb7 I \xc2\xb7 V")};
    return names;
}

juce::Array<int> preset(int index) {
    switch (juce::jlimit(0, 4, index)) {
    case 1:
        return {1, 4, 0, 0};
    case 2:
        return {0, 5, 2, 6};
    case 3:
        return {0, 5, 3, 4};
    case 4:
        return {5, 3, 0, 4};
    default:
        return {0, 4, 5, 3};
    }
}
} // namespace Chords

juce::String roleName(ChordRole role) {
    switch (role) {
    case ChordRole::bassRoots:
        return "bass: roots";
    case ChordRole::arpeggio:
        return "arpeggio";
    case ChordRole::pad:
        return "chord pad";
    default:
        return "chords";
    }
}

std::vector<NoteEvent> generateChords(const ChordsInput &input,
                                      ChordRole role) {
    std::vector<NoteEvent> events;
    for (int bar = 0; bar < input.degrees.size(); bar++) {
        auto notes = Chords::triad(input.key, input.degrees[bar]);
        double start = bar * 4.0;
        switch (role) {
        case ChordRole::bassRoots: {
            int root = notes[0] - 24;
            for (int i = 0; i < 8; i++)
                events.push_back({root, start + i * 0.5, 0.4, i % 2 ? 90 : 105});
            break;
        }
        case ChordRole::arpeggio: {
            juce::Array<int> up{notes[0], notes[1], notes[2], notes[0] + 12};
            for (int i = 0; i < 16; i++)
                events.push_back({up[i % 4], start + i * 0.25, 0.2, 95});
            break;
        }
        case ChordRole::pad:
            for (int n : notes)
                events.push_back({n + 12, start, 3.9, 80});
            events.push_back({notes[0], start, 3.9, 70});
            break;
        default: // chords on every beat
            for (int beat = 0; beat < 4; beat++)
                for (int n : notes)
                    events.push_back({n, start + beat, 0.8, beat == 0 ? 105 : 90});
        }
    }
    return events;
}

} // namespace app_view_models::Sequencers
