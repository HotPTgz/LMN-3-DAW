#pragma once

namespace internal_plugins {

/**
 * Base class for the built-in audio effects. Parameters are added in the
 * constructor; those shown as knobs are listed in getControls() in order
 * (4 on the first page, the rest behind Control).
 */
class AudioFxPlugin : public tracktion::Plugin {
  public:
    ~AudioFxPlugin() override;

    struct Control {
        tracktion::AutomatableParameter::Ptr parameter;
        double interval;
    };
    const juce::Array<Control> &getControls() const { return controls; }
    static bool isAudioFx(const tracktion::Plugin &plugin);

    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void deinitialise() override {}
    double getLatencySeconds() override { return 0.0; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }
    bool takesAudioInput() override { return true; }
    bool takesMidiInput() override { return false; }
    bool producesAudioWhenNoAudioInput() override { return false; }
    bool canBeAddedToClip() override { return false; }
    bool canBeAddedToRack() override { return false; }
    bool needsConstantBufferSize() override { return false; }
    void applyToBuffer(const tracktion::PluginRenderContext &) override;
    void restorePluginStateFromValueTree(const juce::ValueTree &) override;

    // a parameter's value / choice index by ID (for the screens)
    float get(const juce::String &paramID) const;
    int getChoice(const juce::String &paramID) const;

  protected:
    explicit AudioFxPlugin(tracktion::PluginCreationInfo info);

    tracktion::AutomatableParameter *
    addChoice(const juce::String &paramID, const juce::String &name,
              const juce::StringArray &choices, float defaultIndex,
              bool knob = true);
    tracktion::AutomatableParameter *
    addNumber(const juce::String &paramID, const juce::String &name,
              juce::NormalisableRange<float> range, float interval,
              float defaultValue, std::function<juce::String(float)> toText,
              bool knob = true);

    virtual void prepare(double sampleRate) = 0;
    // buffer channels 0 and 1 are the track; anything after is a sidechain
    virtual void process(juce::AudioBuffer<float> &buffer, int start, int n,
                         const fx::BlockTime &time) = 0;

    double sampleRate = 44100.0;

  private:
    juce::Array<Control> controls;
    juce::OwnedArray<juce::CachedValue<float>> values;
    juce::HashMap<juce::String, tracktion::AutomatableParameter *> byID;
    tracktion::tempo::Sequence::Position tempoPosition;
    double freeRunningBeat = 0.0;
};

#define LMN3_FX_NAMES(Class, displayName, shortName, xmlName)                 \
    static const char *getPluginName() { return NEEDS_TRANS(displayName); }    \
    static const char *xmlTypeName;                                            \
    juce::String getName() override { return TRANS(displayName); }             \
    juce::String getPluginType() override { return xmlTypeName; }              \
    juce::String getShortName(int) override { return shortName; }              \
    juce::String getSelectableDescription() override { return getName(); }

class TapePlugin : public AudioFxPlugin {
  public:
    explicit TapePlugin(tracktion::PluginCreationInfo);
    ~TapePlugin() override;
    LMN3_FX_NAMES(TapePlugin, "Tape", "Tape", "lmn3Tape")
    float getWobble() const { return dsp.getWobble(); }

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::Tape dsp;
};

class LofiPlugin : public AudioFxPlugin {
  public:
    explicit LofiPlugin(tracktion::PluginCreationInfo);
    ~LofiPlugin() override;
    LMN3_FX_NAMES(LofiPlugin, "Lo-fi", "Lo-fi", "lmn3Lofi")
    static float rateHz(float knob); // 0-1 to Hz

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::Lofi dsp;
};

class DrivePlugin : public AudioFxPlugin {
  public:
    explicit DrivePlugin(tracktion::PluginCreationInfo);
    ~DrivePlugin() override;
    LMN3_FX_NAMES(DrivePlugin, "Drive", "Drive", "lmn3Drive")

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::Drive dsp;
};

class ResonantFilterPlugin : public AudioFxPlugin {
  public:
    explicit ResonantFilterPlugin(tracktion::PluginCreationInfo);
    ~ResonantFilterPlugin() override;
    LMN3_FX_NAMES(ResonantFilterPlugin, "Filter", "Filter", "lmn3Filter")
    static float cutoffHz(float knob); // 0-1 to Hz
    static const juce::StringArray &getLfoRateNames();
    static double getLfoRateBeats(int index);
    // what the filter is doing now (for the screen)
    std::atomic<float> liveCutoff{1200.0f}, liveEnvelope{0.0f}, liveLfo{0.0f};

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::ResonantFilter dsp;
};

class DuckerPlugin : public AudioFxPlugin {
  public:
    explicit DuckerPlugin(tracktion::PluginCreationInfo);
    ~DuckerPlugin() override;
    LMN3_FX_NAMES(DuckerPlugin, "Ducker", "Duck", "lmn3Ducker")
    void getChannelNames(juce::StringArray *ins,
                         juce::StringArray *outs) override;
    bool canSidechain() override { return true; }

    // The source: 0 = the beat, then the other audio tracks
    juce::StringArray getSourceNames();
    int getSourceIndex();
    void setSourceIndex(int index);
    std::atomic<float> liveGain{1.0f};
    std::atomic<int> hitCount{0};

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::Ducker dsp;
};

class BeatRepeatPlugin : public AudioFxPlugin {
  public:
    explicit BeatRepeatPlugin(tracktion::PluginCreationInfo);
    ~BeatRepeatPlugin() override;
    LMN3_FX_NAMES(BeatRepeatPlugin, "Beat Repeat", "Repeat", "lmn3BeatRepeat")
    // held down on the screen (encoder 1); also automatable
    void setActive(bool shouldRepeat);
    bool isActive() const { return get("active") > 0.5f; }
    std::atomic<int> liveRepeat{-1}; // -1 not repeating, -2 waiting

  protected:
    void prepare(double) override;
    void process(juce::AudioBuffer<float> &, int, int,
                 const fx::BlockTime &) override;

  private:
    fx::BeatRepeat dsp;
};

} // namespace internal_plugins
