#pragma once

// Note generators for the Euclidean, Turing and Chords sequencers. Plain data
// in, notes out (beats from the start of the clip), like SequencerEngine.
namespace app_view_models::Sequencers {

// The name of each kit row (notes from 53 up) for a track: the drum kit's
// sample names, the Drum Synth's pads, or "SOUND n"
juce::StringArray kitRowNames(tracktion::AudioTrack &track);

//==============================================================================
// Euclidean: each kit sound gets a ring of steps with its hits spread evenly
struct EuclidRing {
    int hits = 0;   // 0 = the sound is off
    int steps = 16; // 1-32
    int rotate = 0; // steps the pattern is turned
};

// The ring's pattern: hits as evenly spread as they can be over the steps
juce::Array<bool> euclid(int hits, int steps, int rotate);

struct EuclidInput {
    std::array<EuclidRing, drumRows> rings;
    double swing = 0.0;  // 0-1
    int velocity = 100;  // 1-127, the first hit of each ring a little louder
};

// Steps in the loop: enough for every ring to come round (at most 64)
int euclidLengthSteps(const EuclidInput &input);
std::vector<NoteEvent> generateEuclid(const EuclidInput &input);

//==============================================================================
// Turing: a looping random melody from a shift register. Fully locked it
// repeats; unlocked, bits flip now and then and it slowly changes.
struct TuringInput {
    double lock = 0.85;  // 0-1: chance a bit stays as it is
    int length = 16;     // 2-16 steps in the loop
    int range = 2;       // octaves, 1-4
    int scale = 0;       // a scale from the MIDI FX Scales list
    int root = 48;       // the lowest note
    double stepLength = 0.25; // beats
    double gate = 0.5;
    int seed = 1;
    int bars = 4;        // how long the clip is
};

// The note of every step of the clip
std::vector<int> turingNotes(const TuringInput &input);
std::vector<NoteEvent> generateTuring(const TuringInput &input);
double turingLength(const TuringInput &input);

//==============================================================================
// Chords: a chord per bar; the next tracks can follow along
namespace Chords {
// keys: "C major" ... "B minor" (24)
const juce::StringArray &getKeyNames();
// a bar's chord: scale degree 0-6 in the key
juce::Array<int> triad(int key, int degree); // MIDI notes around C4
juce::String chordName(int key, int degree); // "G", "Am", "B°"
juce::String romanName(int key, int degree); // "V", "vi", "vii°"
const juce::StringArray &getPresetNames();
juce::Array<int> preset(int index); // degrees, one per bar
} // namespace Chords

enum class ChordRole { chords, bassRoots, arpeggio, pad };
juce::String roleName(ChordRole role);

struct ChordsInput {
    int key = 0;              // index into Chords::getKeyNames()
    juce::Array<int> degrees; // one per bar
};

std::vector<NoteEvent> generateChords(const ChordsInput &input, ChordRole role);
inline double chordsLength(const ChordsInput &input) {
    return 4.0 * juce::jmax(1, input.degrees.size());
}

} // namespace app_view_models::Sequencers
