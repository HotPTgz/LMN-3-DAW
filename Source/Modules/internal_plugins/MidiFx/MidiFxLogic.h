#pragma once

namespace internal_plugins {
namespace midifx {

//==============================================================================
// Scale lock
namespace Scales {
const juce::StringArray &getNames();
const juce::StringArray &getRootNames();

// Snaps a MIDI note to the nearest note of the scale (ties go up).
// The last scale ("Chromatic") leaves notes unchanged.
int snapToScale(int note, int root, int scaleIndex);
} // namespace Scales

//==============================================================================
// Chords
namespace Chords {
const juce::StringArray &getNames();
const juce::StringArray &getInversionNames();
const juce::StringArray &getSpreadNames();

// Notes of a chord built on rootNote, clipped to 0-127.
// inversion 0-3 moves the lowest notes up an octave; spread 1 (open) moves
// the second-lowest note up an octave.
juce::Array<int> build(int rootNote, int chordIndex, int inversion,
                       int spread);
} // namespace Chords

//==============================================================================
// Arpeggiator
namespace Arp {
enum Mode { up = 0, down, upDown, random, asPlayed };
const juce::StringArray &getModeNames();
const juce::StringArray &getRateNames();
double getRateBeats(int rateIndex); // step length in quarter-note beats
} // namespace Arp

/**
 * The arpeggiator itself, independent of Tracktion's plugin classes so it can
 * be unit tested. Positions are in quarter-note beats.
 *
 * process() consumes note on/off messages from the block, generates the
 * arpeggio for the block and passes every other message through.
 */
class ArpEngine {
  public:
    struct Settings {
        int mode = Arp::up;
        double rateBeats = 0.25;
        int octaves = 1;
        double gate = 0.5; // proportion of a step
        bool latch = false;
    };

    // blockStartBeat: beat position of the block start
    // beatsPerSecond: tempo
    // blockSeconds: block length
    void process(tracktion::MidiMessageArray &midi, double blockStartBeat,
                 double beatsPerSecond, double blockSeconds,
                 const Settings &settings);

    // Stops the sounding note and forgets held notes (transport stop etc.)
    void reset(tracktion::MidiMessageArray &midi, double time);

    int getNumHeldNotes() const { return held.size(); }

  private:
    struct HeldNote {
        int note;
        int velocity;
    };

    juce::Array<HeldNote> held;    // in the order they were pressed
    juce::Array<int> keysDown;     // physical keys still down (for latch)
    int soundingNote = -1;
    int midiChannel = 1;
    double noteOffBeat = 0.0;
    int stepIndex = 0;
    bool goingUp = true;
    double lastStepBeat = -1.0;
    juce::Random random;

    juce::Array<int> buildSequence(const Settings &settings) const;
    int nextNote(const Settings &settings, int &velocity);
    void addNoteOff(tracktion::MidiMessageArray &midi, double time);
};

} // namespace midifx
} // namespace internal_plugins
