#pragma once

namespace internal_plugins {
namespace midifx {

/**
 * MIDI messages waiting to be sent later (strummed, humanised or echoed
 * notes), kept in absolute seconds since the plugin started.
 */
class MidiScheduler {
  public:
    void add(double time, const juce::MidiMessage &message);
    // moves every message due before blockStart + blockSeconds into midi,
    // timed from the block start
    void flush(tracktion::MidiMessageArray &midi, double blockStart,
               double blockSeconds);
    void clear() { pending.clear(); }
    bool isEmpty() const { return pending.empty(); }
    // the time of the last pending note on for a note (or -1)
    double lastNoteOnTime(int note) const;

  private:
    struct Item {
        double time;
        juce::MidiMessage message;
    };
    std::vector<Item> pending;
};

//==============================================================================
// Note repeat: held notes repeat on the beat grid
namespace NoteRepeat {
const juce::StringArray &getRateNames();
double getRateBeats(int index);
} // namespace NoteRepeat

class NoteRepeatEngine {
  public:
    struct Settings {
        double rateBeats = 0.25;
        double ramp = 0.0;  // -1..1: each repeat gets quieter/louder
        double gate = 0.5;  // proportion of a step
        double swing = 0.0; // 0..0.75: delays every second step
    };
    void process(tracktion::MidiMessageArray &midi, double blockStartBeat,
                 double beatsPerSecond, double blockSeconds,
                 const Settings &settings);
    int getNumHeldNotes() const { return int(held.size()); }

  private:
    struct Held {
        int note, velocity;
        double pressBeat;
        int count;
    };
    struct Sounding {
        int note;
        double offBeat;
    };
    std::vector<Held> held;
    std::vector<Sounding> sounding;
    int channel = 1;
    double lastStepBeat = -1.0e9;
    void noteOffNow(tracktion::MidiMessageArray &midi, int note, double time);
};

//==============================================================================
// Strum: the notes of a chord one after another
namespace Strum {
const juce::StringArray &getDirectionNames(); // Up, Down
const juce::StringArray &getCurveNames();     // Even, Speed up, Slow down
// the offset of note i (0-based) of n, for a gap of `seconds`
double offset(int i, int n, double seconds, int curve);
} // namespace Strum

class StrumEngine {
  public:
    struct Settings {
        double seconds = 0.03; // gap between notes
        int direction = 0;     // 0 up, 1 down
        bool alternate = false;
        int curve = 0;
    };
    void process(tracktion::MidiMessageArray &midi, double blockSeconds,
                 const Settings &settings);

  private:
    MidiScheduler scheduler;
    double now = 0.0;
    bool nextDown = false;
};

//==============================================================================
// Humanise: small random changes to timing, velocity and length
class HumaniseEngine {
  public:
    struct Settings {
        double timingSeconds = 0.01; // up to this much later
        int velocity = 12;           // up to this much louder or quieter
        double length = 0.05;        // up to this proportion longer
        double amount = 0.5;         // scales all three
    };
    explicit HumaniseEngine(juce::int64 seed = 1) : random(seed) {}
    void process(tracktion::MidiMessageArray &midi, double blockSeconds,
                 const Settings &settings);

  private:
    MidiScheduler scheduler;
    double now = 0.0;
    std::array<double, 128> pressTime{}, delayOf{};
    juce::Random random;
};

//==============================================================================
// MIDI echo: repeats each note, quieter (and optionally transposed) each time
namespace Echo {
const juce::StringArray &getTimeNames();
double getTimeBeats(int index);
} // namespace Echo

class EchoEngine {
  public:
    struct Settings {
        double timeBeats = 0.5;
        int repeats = 4;
        double decay = 0.3; // each echo this much quieter
        int transpose = 0;  // semitones added per echo
    };
    void process(tracktion::MidiMessageArray &midi, double beatsPerSecond,
                 double blockSeconds, const Settings &settings);

  private:
    MidiScheduler scheduler;
    double now = 0.0;
};

} // namespace midifx

//==============================================================================
class NoteRepeatPlugin : public MidiFxPlugin {
  public:
    explicit NoteRepeatPlugin(tracktion::PluginCreationInfo info);
    ~NoteRepeatPlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("Note Repeat"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Note Repeat"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Repeat"; }
    juce::String getSelectableDescription() override { return getName(); }
    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> rateValue, rampValue, gateValue, swingValue;
    tracktion::AutomatableParameter::Ptr rate, ramp, gate, swing;
    // for the screen
    std::atomic<int> heldNotes{0};

  private:
    midifx::NoteRepeatEngine engine;
    tracktion::tempo::Sequence::Position tempoPosition;
    double freeRunningBeat = 0.0;
};

class StrumPlugin : public MidiFxPlugin {
  public:
    explicit StrumPlugin(tracktion::PluginCreationInfo info);
    ~StrumPlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("Strum"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Strum"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Strum"; }
    juce::String getSelectableDescription() override { return getName(); }
    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> timeValue, directionValue, alternateValue,
        curveValue;
    tracktion::AutomatableParameter::Ptr time, direction, alternate, curve;

  private:
    midifx::StrumEngine engine;
};

class HumanisePlugin : public MidiFxPlugin {
  public:
    explicit HumanisePlugin(tracktion::PluginCreationInfo info);
    ~HumanisePlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("Humanise"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Humanise"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Human"; }
    juce::String getSelectableDescription() override { return getName(); }
    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> timingValue, velocityValue, lengthValue,
        amountValue;
    tracktion::AutomatableParameter::Ptr timing, velocity, length, amount;

  private:
    midifx::HumaniseEngine engine{juce::Time::currentTimeMillis()};
};

class MidiEchoPlugin : public MidiFxPlugin {
  public:
    explicit MidiEchoPlugin(tracktion::PluginCreationInfo info);
    ~MidiEchoPlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("MIDI Echo"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("MIDI Echo"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Echo"; }
    juce::String getSelectableDescription() override { return getName(); }
    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> timeValue, repeatsValue, decayValue,
        transposeValue;
    tracktion::AutomatableParameter::Ptr time, repeats, decay, transpose;

  private:
    midifx::EchoEngine engine;
    tracktion::tempo::Sequence::Position tempoPosition;
};

} // namespace internal_plugins
