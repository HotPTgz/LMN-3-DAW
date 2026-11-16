#pragma once

namespace internal_plugins {
namespace engines {

/**
 * The sound of the built-in engines (String, FM, Pulse), kept independent of
 * Tracktion so it can be unit tested. Each engine is a VoiceSound; the
 * PolySynth plays up to 8 of them with a shared amp envelope and filter
 * (the "Shape" page).
 */

// Number of sound parameters per engine (4 knobs + 4 with Control)
constexpr int numSoundParams = 8;
constexpr int maxVoices = 8;

// The Shape page, in real units
struct ShapeSettings {
    float attack = 0.01f;   // seconds
    float decay = 0.5f;     // seconds
    float sustain = 0.7f;   // 0-1
    float release = 0.3f;   // seconds
    float cutoff = 8000.0f; // Hz
    float resonance = 0.2f; // 0-1
    float envAmount = 0.2f; // 0-1: how far the envelope opens the filter
    int voices = 6;
};

// Knob position (0-1) <-> seconds for envelope times: fine control of short
// times, up to 5 s
inline float envelopeSeconds(float knob) { return knob * knob * 5.0f; }
// Knob position (0-1) <-> filter cutoff, 20 Hz - 20 kHz
inline float cutoffHz(float knob) {
    return 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, knob));
}

//==============================================================================
class Envelope {
  public:
    void noteOn();
    void noteOff();
    void reset() { stage = idle, level = 0.0f; }
    float next(const ShapeSettings &shape, double sampleRate);
    bool isActive() const { return stage != idle; }
    bool isReleasing() const { return stage == release; }
    float getLevel() const { return level; }

  private:
    enum Stage { idle, attack, decay, sustain, release };
    Stage stage = idle;
    float level = 0.0f;
};

// State-variable low-pass filter (topology-preserving transform)
class Filter {
  public:
    void reset() { ic1 = ic2 = 0.0f; }
    void setParams(float cutoff, float resonance, double sampleRate);
    float process(float x);

  private:
    float g = 0.5f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0.0f, ic2 = 0.0f;
};

//==============================================================================
// One note of an engine's raw sound (before the envelope and filter)
class VoiceSound {
  public:
    virtual ~VoiceSound() = default;
    // p: the engine's sound parameters in real units
    virtual void start(float frequency, float previousFrequency, float velocity,
                       double sampleRate, const float *p) = 0;
    virtual void stop() {}
    virtual float next(const float *p) = 0;
};

class PolySynth {
  public:
    using Factory = std::function<std::unique_ptr<VoiceSound>()>;

    explicit PolySynth(Factory factory);

    void prepare(double sampleRate);
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();
    // sustain pedal: while down, released keys keep sounding
    void setSustain(bool down);

    // Adds numSamples of sound to left/right
    void render(float *left, float *right, int numSamples, const float *p,
                const ShapeSettings &shape);

    int getNumActiveVoices() const;

  private:
    struct Voice {
        std::unique_ptr<VoiceSound> sound;
        Envelope amp;
        Filter filter;
        int note = -1;
        float velocity = 0.0f;
        juce::uint64 age = 0;
        float pan = 0.5f;
        bool sustained = false; // key released while the pedal was down
    };
    bool sustainDown = false;

    std::vector<Voice> voices;
    double sampleRate = 44100.0;
    juce::uint64 counter = 0;
    float lastFrequency = 0.0f;
    int voiceLimit = maxVoices;
    // DC blocker on the output (FM feedback and uneven pulses add DC)
    float dcInL = 0, dcOutL = 0, dcInR = 0, dcOutR = 0;

    Voice *findVoiceToUse(int note);
};

float midiNoteToHz(float note);

//==============================================================================
// String: a plucked string (Karplus-Strong)
//   p: decay (s), brightness, pick position, body, detune, stiffness,
//      pluck noise, level
class StringSound : public VoiceSound {
  public:
    StringSound();
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;

  private:
    struct Line {
        std::vector<float> buffer;
        float length = 100.0f; // samples, fractional
        int write = 0;
        float last = 0.0f;
        float allpassState = 0.0f;
        float feedback = 0.99f;
        float tick(float damping, float stiffness);
    };
    Line lines[2];
    float frequency = 220.0f;
    double sampleRate = 44100.0;
    std::vector<float> excitation; // scratch space, allocated once
    juce::Random random;
    // body resonances
    float b1[2]{}, b2[2]{};
    float bodyCoeffs[2][3]{};
    float bodyFilter(int i, float x);
};

// FM: four sine operators in one of 8 algorithms
//   p: algorithm (0-7), op 2 ratio, amount, feedback, op 3 ratio,
//      op 4 ratio, mod decay (s), detune
class FmSound : public VoiceSound {
  public:
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;

    struct Algorithm {
        const char *name;
        std::vector<std::pair<int, int>> links; // modulator -> target (1-4)
        std::vector<int> carriers;              // operators you hear
    };
    static const std::vector<Algorithm> &getAlgorithms();

  private:
    double phases[4]{};
    float outputs[4]{};
    float feedbackState = 0.0f;
    float frequency = 220.0f;
    double sampleRate = 44100.0;
    float modLevel = 1.0f;
};

// Pulse: band-limited pulse wave with pulse-width modulation and a sub
//   p: width, PWM depth, PWM rate (Hz), sub, glide (s), detune, noise, drive
class PulseSound : public VoiceSound {
  public:
    void start(float frequency, float previousFrequency, float velocity,
               double sampleRate, const float *p) override;
    float next(const float *p) override;

  private:
    double phase1 = 0, phase2 = 0, subPhase = 0, lfoPhase = 0;
    float targetFrequency = 220.0f, frequency = 220.0f;
    double sampleRate = 44100.0;
    juce::Random random;
    static float polyBlep(double t, double dt);
    static float pulse(double phase, double dt, float width);
};

} // namespace engines
} // namespace internal_plugins
