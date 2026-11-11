#pragma once

// Knob maps: small YAML files that put the useful parameters of a VST3
// plugin on the encoders, grouped into named pages of up to 8 knobs (the
// first 4 on the encoders, the next 4 while holding Control).
//
//   plugin: OB-Xd
//   match: [obxd, ob-xd]          # parts of the plugin's name, any case
//   pages:
//     - name: FILTER
//       knobs:
//         - CUTOFF = Cutoff         # label = parameter name
//         - RES = Resonance|Reso    # | lists other spellings
//         - ""                      # an empty slot
//
// The maps live in ~/.config/LMN-3/knob_maps; the built-in ones (OB-Xd and
// Dexed) are written there the first time, so they can be edited.
namespace app_view_models::KnobMaps {

struct Knob {
    juce::String label;
    juce::StringArray names; // the parameter name and other spellings
    bool isEmpty() const { return names.isEmpty(); }
};

struct Page {
    juce::String name;
    std::vector<Knob> knobs; // at most 8
};

struct Map {
    juce::String plugin;
    juce::StringArray match;
    std::vector<Page> pages;

    bool isValid() const { return !pages.empty(); }
    // whether this map is for the plugin with the given name
    bool matches(const juce::String &pluginName) const;
};

// Parses a map; returns an invalid map (no pages) if the text can't be read
Map parse(const juce::String &yamlText);

// Lowercase letters and digits only, so "OP1 OUTPUT LEVEL" matches
// "OP1 Output Level"
juce::String normalise(const juce::String &name);

// The index in parameterNames of the parameter a knob controls, or -1
int findParameter(const juce::StringArray &parameterNames, const Knob &knob);

// The built-in maps: file name -> YAML text
const std::vector<std::pair<juce::String, juce::String>> &getDefaultMaps();

// ~/.config/LMN-3/knob_maps
juce::File getDirectory();

// Writes the built-in maps that aren't in the folder yet
void installDefaults(const juce::File &directory);

// The map for a plugin from the folder, or an invalid map
Map findMapFor(const juce::String &pluginName, const juce::File &directory);

} // namespace app_view_models::KnobMaps
