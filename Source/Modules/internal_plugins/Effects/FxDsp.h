#pragma once

// Sound processing for the built-in effects (Tape, Lo-fi, Drive, Filter,
// Ducker, Beat Repeat). No Tracktion here, so it can be unit-tested.
namespace internal_plugins {
namespace fx {

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

// Time and tempo for one block
struct BlockTime {
    double sampleRate = 44100.0;
    double startBeat = 0.0;      // beat position at the first sample
    double beatsPerSecond = 2.0; // 120 bpm
};

// One-pole smoothing, to stop knob moves from clicking
class Smoothed {
  public:
    void prepare(double sampleRate, float seconds = 0.02f);
    void reset(float v) { current = target = v; }
    void set(float v) { target = v; }
    float next() { return current += (target - current) * coefficient; }
    float get() const { return current; }

  private:
    float current = 0.0f, target = 0.0f, coefficient = 1.0f;
};

// Delay line read with linear interpolation
class DelayLine {
  public:
    void prepare(int maxSamples);
    void clear();
    void push(float x);
    float read(float delaySamples) const; // 0 = the sample just pushed
  private:
    std::vector<float> buffer;
    int write = 0;
};

// Zero-delay-feedback state variable filter (Simper)
class Svf {
  public:
    enum class Output { lowPass, bandPass, highPass, notch };
    void setup(double sampleRate, float hz, float q);
    float process(float x, Output out);
    void reset() { ic1 = ic2 = 0.0f; }

  private:
    float a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
};

//==============================================================================
class Tape {
  public:
    struct Settings {
        float wow = 0.2f, flutter = 0.1f, saturate = 0.35f, hiss = 0.05f;
        float age = 0.0f;  // dropouts and darker sound
        float tone = 0.8f; // brightness
    };
    void prepare(double sampleRate);
    void process(float *left, float *right, int n, const Settings &s);
    float getWobble() const { return wobble; } // -1..1, for the screen

  private:
    double sampleRate = 44100.0;
    DelayLine delay[2];
    double wowPhase = 0, flutterPhase = 0;
    float lowPass[2] = {}, dropout = 1.0f, dropoutTarget = 1.0f;
    float wobble = 0.0f;
    juce::Random random{11};
    Smoothed drive, hissLevel;
};

//==============================================================================
class Lofi {
  public:
    struct Settings {
        float bits = 8.0f;     // 2-16
        float rateHz = 11025;  // sample rate it is reduced to
        float noise = 0.0f;    // 0-1
        float mix = 1.0f;      // 0-1
        float filterHz = 20000; // low-pass after the crush
    };
    void prepare(double sampleRate);
    void process(float *left, float *right, int n, const Settings &s);
    static float crush(float x, float bits);

  private:
    double sampleRate = 44100.0;
    double phase = 1.0;
    float held[2] = {};
    Svf filter[2];
    juce::Random random{5};
};

//==============================================================================
class Drive {
  public:
    enum Type { soft, hard, fuzz, fold };
    static const juce::StringArray &getTypeNames();
    struct Settings {
        float driveDb = 12.0f; // 0-36
        float tone = 0.6f;     // 0 dark - 1 bright
        int type = soft;
        float mix = 1.0f;
        float outputDb = 0.0f;
    };
    void prepare(double sampleRate);
    void process(float *left, float *right, int n, const Settings &s);
    static float shape(float x, int type);

  private:
    double sampleRate = 44100.0;
    float toneState[2] = {};
    Smoothed gain;
};

//==============================================================================
class ResonantFilter {
  public:
    enum Type { lowPass24, lowPass12, highPass, bandPass, notch };
    static const juce::StringArray &getTypeNames();
    struct Settings {
        float cutoffHz = 1200.0f;
        float resonance = 0.6f; // 0-1
        float envelope = 0.4f;  // -1..1: how far the input level moves it
        float lfo = 0.0f;       // 0-1: how far the LFO moves it
        int type = lowPass24;
        float envRelease = 0.15f; // seconds
        double lfoBeats = 4.0;    // one LFO cycle, in beats
    };
    void prepare(double sampleRate);
    void process(float *left, float *right, int n, const BlockTime &time,
                 const Settings &s);
    float getCurrentCutoff() const { return currentCutoff; }
    float getEnvelope() const { return envelope; }
    float getLfo() const { return lfoValue; }

  private:
    double sampleRate = 44100.0;
    Svf stage1[2], stage2[2];
    float envelope = 0.0f, currentCutoff = 1200.0f, lfoValue = 0.0f;
};

//==============================================================================
class Ducker {
  public:
    static const juce::StringArray &getShapeNames(); // Smooth, Punchy, Gate
    struct Settings {
        float depth = 0.7f;
        float releaseSeconds = 0.18f;
        int shape = 0;
    };
    void prepare(double sampleRate);
    // sidechain: the source track's audio (nullptr: duck on every beat)
    void process(float *left, float *right, const float *sidechainLeft,
                 const float *sidechainRight, int n, const BlockTime &time,
                 const Settings &s);
    float getGain() const { return lastGain; }
    // the gain a time after a hit (for the screen and the tests)
    static float gainAt(float secondsSinceHit, const Settings &s);
    int getHitCount() const { return hits; }

  private:
    double sampleRate = 44100.0;
    double sinceHit = 1.0e9;
    float detector = 0.0f, lastGain = 1.0f;
    bool armed = true;
    int hits = 0;
};

//==============================================================================
class BeatRepeat {
  public:
    static const juce::StringArray &getLengthNames();
    static double getLengthBeats(int index);
    static const juce::StringArray &getRepeatNames(); // last: Hold
    static int getRepeatCount(int index);             // -1: while held
    struct Settings {
        bool active = false;
        double lengthBeats = 0.5;
        int repeats = -1;
        float decay = 0.2f;
        float pitch = 0.0f; // semitones added each repeat
    };
    void prepare(double sampleRate);
    void process(float *left, float *right, int n, const BlockTime &time,
                 const Settings &s);
    bool isRepeating() const { return repeating; }
    bool isWaiting() const { return pending; } // for the next boundary
    int getRepeatIndex() const { return repeatIndex; }

  private:
    double sampleRate = 44100.0;
    std::vector<float> history[2], slice[2];
    int write = 0, sliceLength = 0;
    double readPos = 0, rate = 1;
    int repeatIndex = 0;
    float gain = 1.0f, wet = 0.0f;
    bool wasActive = false, repeating = false, pending = false;
    void startSlice(int length);
};

} // namespace fx
} // namespace internal_plugins
