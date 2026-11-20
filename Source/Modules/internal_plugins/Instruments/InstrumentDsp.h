#pragma once

// Sound of the newer instruments: Wavetable, Mallets and Granular (voices for
// the engine PolySynth) and the Drum Synth kit. No Tracktion here, so it can
// be unit-tested.
namespace internal_plugins {
namespace instruments {

//==============================================================================
// Wavetable: stacks of single-cycle waves, swept with POSITION
//   p: table, position, warp, brightness (knob 0-1), position envelope (-1..1),
//      envelope time (s), detune (cents), level
class Wavetables {
  public:
    static constexpr int numTables = 5, numFrames = 9, size = 2048, numLevels = 5;
    static const juce::StringArray &getNames();
    static const Wavetables &get(); // built once, shared
    // one sample: phase 0-1, frame position 0-1 (blended), for a note at hz
    float sample(int table, float position, double phase, float hz,
                 double sampleRate) const;
    // the loudest-harmonics version of a frame, for drawing
    const std::vector<float> &frame(int table, int frameIndex) const;

  private:
    Wavetables();
    // [table][frame][level] -> size samples
    std::vector<std::vector<float>> data;
    const std::vector<float> &at(int t, int f, int level) const;
};

class WavetableSound : public engines::VoiceSound {
  public:
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;
    static float warpPhase(double phase, float warp);

  private:
    double phase1 = 0, phase2 = 0, sampleRate = 44100.0;
    float frequency = 220.0f, envTime = 0.0f, lowPass = 0.0f;
};

//==============================================================================
// Mallets: struck bars, bells and tines (modal synthesis)
//   p: material, hardness, decay (s), tone, body, strike position, click,
//      level
namespace Mallets {
const juce::StringArray &getMaterialNames(); // Wood, Glass, Metal, Kalimba
struct Material {
    std::vector<float> ratios;  // mode frequency / fundamental
    std::vector<float> decays;  // mode decay relative to the fundamental's
    std::vector<float> levels;
};
const Material &getMaterial(int index);
} // namespace Mallets

class MalletSound : public engines::VoiceSound {
  public:
    static constexpr int maxModes = 8;
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;

  private:
    struct Mode {
        float re = 0, im = 0;      // rotating phasor
        float cosw = 1, sinw = 0;  // rotation per sample
        float decay = 0.999f;      // per sample
        float level = 0;
    };
    Mode modes[maxModes];
    int numModes = 0;
    float clickLevel = 0, clickDecay = 0.9f, clickState = 0;
    juce::Random random{3};
};

//==============================================================================
// Granular: tiny windowed grains of a sample
//   p: position (0-1), size (s), density (grains/s), spray (0-1),
//      pitch (semitones), spread (0-1, random pitch), freeze (0/1), level
struct GrainSample {
    juce::AudioBuffer<float> audio; // mono
    double sampleRate = 44100.0;
    juce::String name;
    std::vector<float> overview; // 96 peak levels, for the screen
    static std::shared_ptr<GrainSample> fromBuffer(juce::AudioBuffer<float> b,
                                                   double sampleRate,
                                                   const juce::String &name);
    // a sung "ah-oh-ee" so Granular makes sound before a sample is chosen
    static std::shared_ptr<GrainSample> builtIn();
};

// The sample the voices play; swapped from the message thread
class GrainSampleSlot {
  public:
    GrainSampleSlot() : current(GrainSample::builtIn()) {}
    void set(std::shared_ptr<const GrainSample> s);
    std::shared_ptr<const GrainSample> get() const; // never null
    // where the play heads are (0-1, or -1), for the screen
    std::atomic<float> playhead{-1.0f};

  private:
    mutable juce::SpinLock lock;
    std::shared_ptr<const GrainSample> current, previous;
};

class GrainSound : public engines::VoiceSound {
  public:
    static constexpr int maxGrains = 24;
    explicit GrainSound(std::shared_ptr<GrainSampleSlot> s)
        : slot(std::move(s)) {}
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;
    int getNumActiveGrains() const;

  private:
    struct Grain {
        double pos = 0, rate = 1;
        int age = 0, length = 0;
        bool active = false;
    };
    std::shared_ptr<GrainSampleSlot> slot;
    std::shared_ptr<const GrainSample> sample;
    Grain grains[maxGrains];
    double sampleRate = 44100.0, untilNext = 0, scan = 0;
    float noteRatio = 1.0f;
    int counter = 0;
    juce::Random random{9};
};

//==============================================================================
// Drum Synth: 8 synthesised drums
class DrumKit {
  public:
    static constexpr int numPads = 8, numParams = 6;
    enum Pad { kick, snare, clap, rim, hat, openHat, tom, cowbell };
    static const juce::StringArray &getPadNames();
    static const juce::StringArray &getParamNames(); // Pitch ... Drive
    // knob positions 0-1: pitch, punch, decay, tone, noise, drive
    using PadParams = std::array<float, numParams>;
    static PadParams defaults(int pad);
    // what a knob means for a pad, e.g. "48 Hz"
    static juce::String describe(int pad, int param, float knob);
    static float pitchHz(int pad, float knob);
    static float decaySeconds(int pad, float knob);
    // the pad a key plays (the keyboard starts at F3 = 53)
    static int padForNote(int note) { return ((note - 53) % 8 + 8) % 8; }

    void prepare(double sampleRate);
    void trigger(int pad, float velocity, const PadParams &params);
    void allOff();
    // adds n samples
    void render(float *left, float *right, int n,
                const std::array<PadParams, numPads> &params);
    // one pad on its own, for the screen's picture
    static std::vector<float> preview(int pad, const PadParams &params,
                                      double sampleRate, int numSamples);

  private:
    struct Voice {
        bool active = false;
        double t = 0;
        float velocity = 1, level = 1;
        double phase[6] = {};
        bool choking = false;
        fx::Svf noiseFilter, bodyFilter;
    };
    Voice voices[numPads];
    double sampleRate = 44100.0;
    juce::Random random{17};
    float renderVoice(int pad, Voice &v, const PadParams &p);
};

} // namespace instruments
} // namespace internal_plugins
