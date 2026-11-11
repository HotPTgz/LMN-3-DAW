#pragma once

namespace app_view_models {

/**
 * Puts the parameters of a VST3 plugin on the encoders: the pages from its
 * knob map first (if there is one), then every other parameter, 8 to a page.
 * Also lists and loads the plugin's presets (its programs).
 */
class ExternalPluginViewModel {
  public:
    struct Knob {
        juce::String label;
        tracktion::AutomatableParameter::Ptr parameter;
    };

    struct Page {
        juce::String name;
        std::array<Knob, 8> knobs;
        bool fromMap = false;
    };

    ExternalPluginViewModel(tracktion::Plugin::Ptr p, const KnobMaps::Map &map);

    const std::vector<Page> &getPages() const { return pages; }
    bool hasMap() const { return mapped; }
    tracktion::Plugin &getPlugin() { return *plugin; }

    // values (normalised 0-1) and nudging by one encoder detent
    static float getValue(tracktion::AutomatableParameter &parameter);
    static juce::String getText(tracktion::AutomatableParameter &parameter);
    static void nudge(tracktion::AutomatableParameter &parameter,
                      int direction);

    // presets
    int getNumPresets();
    juce::String getPresetName(int index);
    int getCurrentPreset();
    void loadPreset(int index);

    // parameters the screens leave out (MIDI CC helpers, bypass)
    static bool isHidden(const juce::String &parameterName);

  private:
    tracktion::Plugin::Ptr plugin;
    std::vector<Page> pages;
    bool mapped = false;

    tracktion::ExternalPlugin *getExternal();
};

} // namespace app_view_models
