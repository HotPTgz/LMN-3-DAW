#pragma once

namespace internal_plugins {

#define LMN3_INSTRUMENT_NAMES(displayName, shortName)                          \
    static const char *getPluginName() { return NEEDS_TRANS(displayName); }    \
    static const char *xmlTypeName;                                            \
    juce::String getName() override { return TRANS(displayName); }             \
    juce::String getPluginType() override { return xmlTypeName; }              \
    juce::String getShortName(int) override { return shortName; }              \
    juce::String getSelectableDescription() override { return getName(); }

class WavetableEnginePlugin : public EnginePlugin {
  public:
    explicit WavetableEnginePlugin(tracktion::PluginCreationInfo info);
    ~WavetableEnginePlugin() override;
    LMN3_INSTRUMENT_NAMES("Wavetable", "Wave")
    static float brightnessHz(float knob);
};

class MalletsEnginePlugin : public EnginePlugin {
  public:
    explicit MalletsEnginePlugin(tracktion::PluginCreationInfo info);
    ~MalletsEnginePlugin() override;
    LMN3_INSTRUMENT_NAMES("Mallets", "Mallets")
};

class GranularEnginePlugin : public EnginePlugin {
  public:
    explicit GranularEnginePlugin(tracktion::PluginCreationInfo info);
    ~GranularEnginePlugin() override;
    LMN3_INSTRUMENT_NAMES("Granular", "Grains")

    // the sample: an audio file, or (empty) the built-in voice
    void setSampleFile(const juce::File &file);
    juce::File getSampleFile() const;
    std::shared_ptr<const instruments::GrainSample> getSample() const {
        return slot->get();
    }
    float getPlayhead() const { return slot->playhead.load(); }
    void restorePluginStateFromValueTree(const juce::ValueTree &) override;

  private:
    GranularEnginePlugin(tracktion::PluginCreationInfo info,
                         std::shared_ptr<instruments::GrainSampleSlot> slot);
    std::shared_ptr<instruments::GrainSampleSlot> slot;
    juce::CachedValue<juce::String> sampleFile;
    void loadSample();
};

//==============================================================================
// Drum Synth: 8 pads of synthesised drums, 6 knobs each
class DrumSynthPlugin : public tracktion::Plugin {
  public:
    explicit DrumSynthPlugin(tracktion::PluginCreationInfo info);
    ~DrumSynthPlugin() override;
    LMN3_INSTRUMENT_NAMES("Drum Synth", "Drums")

    using Kit = instruments::DrumKit;
    tracktion::AutomatableParameter *getParameter(int pad, int param) const;
    Kit::PadParams getPadParams(int pad) const;
    // the pad the knobs edit: the last one played
    std::atomic<int> selectedPad{0};
    // when each pad was last hit (ms, Time::getMillisecondCounterHiRes)
    std::array<std::atomic<double>, Kit::numPads> lastHit{};

    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void deinitialise() override {}
    void reset() override { kit.allOff(); }
    double getLatencySeconds() override { return 0.0; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }
    bool takesAudioInput() override { return false; }
    bool takesMidiInput() override { return true; }
    bool isSynth() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }
    bool canBeAddedToClip() override { return false; }
    bool canBeAddedToRack() override { return false; }
    bool needsConstantBufferSize() override { return false; }
    double getTailLength() const override { return 1.5; }
    void applyToBuffer(const tracktion::PluginRenderContext &) override;
    void restorePluginStateFromValueTree(const juce::ValueTree &) override;

  private:
    Kit kit;
    double sampleRate = 44100.0;
    juce::OwnedArray<juce::CachedValue<float>> values;
    tracktion::AutomatableParameter *params[Kit::numPads][Kit::numParams]{};
};

} // namespace internal_plugins
