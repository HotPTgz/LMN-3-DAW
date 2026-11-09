#pragma once

namespace internal_plugins {

/**
 * Base class for the MIDI effects (arpeggiator, scale lock, chord). They sit
 * before the instrument in a track's plugin list and change the notes that
 * reach it. They have no audio.
 */
class MidiFxPlugin : public tracktion::Plugin {
  public:
    explicit MidiFxPlugin(tracktion::PluginCreationInfo info);
    ~MidiFxPlugin() override;

    // A knob on the plugin screen: the parameter and its encoder step
    struct Control {
        tracktion::AutomatableParameter::Ptr parameter;
        double interval;
    };
    const juce::Array<Control> &getControls() const { return controls; }

    static bool isMidiFx(const tracktion::Plugin &plugin);

    void initialise(const tracktion::PluginInitialisationInfo &) override {}
    void deinitialise() override {}
    double getLatencySeconds() override { return 0.0; }
    int getNumOutputChannelsGivenInputs(int) override { return 0; }
    void getChannelNames(juce::StringArray *, juce::StringArray *) override {}
    bool takesAudioInput() override { return false; }
    bool takesMidiInput() override { return true; }
    bool canBeAddedToClip() override { return false; }
    bool canBeAddedToRack() override { return false; }
    bool needsConstantBufferSize() override { return false; }
    bool producesAudioWhenNoAudioInput() override { return false; }
    void restorePluginStateFromValueTree(const juce::ValueTree &) override;

  protected:
    // Adds a parameter with a list of labelled choices (e.g. "Up", "Down")
    tracktion::AutomatableParameter *
    addChoiceParam(const juce::String &paramID, const juce::String &name,
                   const juce::StringArray &choices,
                   juce::CachedValue<float> &value, float defaultIndex);

    // Adds a numeric parameter
    tracktion::AutomatableParameter *
    addNumberParam(const juce::String &paramID, const juce::String &name,
                   juce::Range<float> range, float interval,
                   juce::CachedValue<float> &value, float defaultValue,
                   std::function<juce::String(float)> toText);

    static int choice(tracktion::AutomatableParameter *p) {
        return juce::roundToInt(p->getCurrentValue());
    }

  private:
    juce::Array<Control> controls;
    juce::Array<juce::CachedValue<float> *> cachedValues;
};

//==============================================================================
class ArpeggiatorPlugin : public MidiFxPlugin {
  public:
    explicit ArpeggiatorPlugin(tracktion::PluginCreationInfo info);
    ~ArpeggiatorPlugin() override;

    static const char *getPluginName() { return NEEDS_TRANS("Arpeggiator"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Arpeggiator"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Arp"; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise(const tracktion::PluginInitialisationInfo &) override;
    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> modeValue, rateValue, octavesValue, gateValue,
        latchValue;
    tracktion::AutomatableParameter::Ptr mode, rate, octaves, gate, latch;

  private:
    midifx::ArpEngine engine;
    tracktion::tempo::Sequence::Position tempoPosition;
    double freeRunningBeat = 0.0;
};

//==============================================================================
class ScaleLockPlugin : public MidiFxPlugin {
  public:
    explicit ScaleLockPlugin(tracktion::PluginCreationInfo info);
    ~ScaleLockPlugin() override;

    static const char *getPluginName() { return NEEDS_TRANS("Scale Lock"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Scale Lock"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Scale"; }
    juce::String getSelectableDescription() override { return getName(); }

    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> rootValue, scaleValue;
    tracktion::AutomatableParameter::Ptr root, scale;

  private:
    // remembers which note each key was snapped to, so its note off matches
    int mappedNote[16][128];
};

//==============================================================================
class ChordPlugin : public MidiFxPlugin {
  public:
    explicit ChordPlugin(tracktion::PluginCreationInfo info);
    ~ChordPlugin() override;

    static const char *getPluginName() { return NEEDS_TRANS("Chord"); }
    static const char *xmlTypeName;
    juce::String getName() override { return TRANS("Chord"); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getShortName(int) override { return "Chord"; }
    juce::String getSelectableDescription() override { return getName(); }

    void applyToBuffer(const tracktion::PluginRenderContext &) override;

    juce::CachedValue<float> chordValue, inversionValue, spreadValue;
    tracktion::AutomatableParameter::Ptr chord, inversion, spread;

  private:
    // the chord notes played for each key, so its note off stops them all
    std::array<std::array<juce::Array<int>, 128>, 16> sounding;
};

} // namespace internal_plugins
