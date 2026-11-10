#pragma once

namespace internal_plugins {

/**
 * Base class for the built-in synth engines (String, FM, Pulse). Each has 8
 * sound parameters (its own page) and the 8 shared Shape parameters (amp
 * envelope, filter, voices).
 */
class EnginePlugin : public tracktion::Plugin {
  public:
    ~EnginePlugin() override;

    // A knob on the plugin screen: the parameter and its encoder step
    struct Control {
        tracktion::AutomatableParameter::Ptr parameter;
        double interval;
    };
    // Sound page first (8), then the Shape page (8)
    const juce::Array<Control> &getControls() const { return controls; }

    static bool isEngine(const tracktion::Plugin &plugin);

    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void deinitialise() override {}
    void reset() override;
    double getLatencySeconds() override { return 0.0; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }
    bool takesAudioInput() override { return false; }
    bool takesMidiInput() override { return true; }
    bool isSynth() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }
    bool canBeAddedToClip() override { return false; }
    bool canBeAddedToRack() override { return false; }
    bool needsConstantBufferSize() override { return false; }
    double getTailLength() const override;
    void applyToBuffer(const tracktion::PluginRenderContext &) override;
    void restorePluginStateFromValueTree(const juce::ValueTree &) override;

    // Current settings (for the screens)
    float getSoundValue(int index) const;
    engines::ShapeSettings getShape() const;

  protected:
    struct ShapeDefaults {
        float attack, decay, sustain, release; // knob positions 0-1
        float cutoff, resonance, envAmount;    // knob positions 0-1
        int voices;
    };

    EnginePlugin(tracktion::PluginCreationInfo info,
                 engines::PolySynth::Factory factory);

    // Sound page parameters, in order (8 of them)
    void addSoundParam(const juce::String &paramID, const juce::String &name,
                       juce::Range<float> range, float interval,
                       float defaultValue,
                       std::function<juce::String(float)> toText);
    void addSoundChoiceParam(const juce::String &paramID,
                             const juce::String &name,
                             const juce::StringArray &choices,
                             float defaultIndex);
    // The Shape page; call after the sound parameters
    void addShapeParams(const ShapeDefaults &defaults);

  private:
    engines::PolySynth synth;
    double sampleRate = 44100.0;
    juce::Array<Control> controls;
    juce::OwnedArray<juce::CachedValue<float>> values;
    juce::Array<tracktion::AutomatableParameter *> soundParams, shapeParams;
    std::vector<float> scratch;

    tracktion::AutomatableParameter *
    addValueParam(const juce::String &paramID, const juce::String &name,
                  juce::NormalisableRange<float> range, float interval,
                  float defaultValue, std::function<juce::String(float)> toText,
                  std::function<float(const juce::String &)> fromText);
};

//==============================================================================
class StringEnginePlugin : public EnginePlugin {
  public:
    explicit StringEnginePlugin(tracktion::PluginCreationInfo info);
    ~StringEnginePlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("String"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("String"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "String"; }
    juce::String getSelectableDescription() override { return getName(); }
};

class FmEnginePlugin : public EnginePlugin {
  public:
    explicit FmEnginePlugin(tracktion::PluginCreationInfo info);
    ~FmEnginePlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("FM"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("FM"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "FM"; }
    juce::String getSelectableDescription() override { return getName(); }
};

class PulseEnginePlugin : public EnginePlugin {
  public:
    explicit PulseEnginePlugin(tracktion::PluginCreationInfo info);
    ~PulseEnginePlugin() override;
    static const char *getPluginName() { return NEEDS_TRANS("Pulse"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Pulse"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Pulse"; }
    juce::String getSelectableDescription() override { return getName(); }
};

} // namespace internal_plugins
