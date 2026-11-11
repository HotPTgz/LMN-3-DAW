#include "PluginTreeGroup.h"
#include "PluginTreeItem.h"
#include <internal_plugins/internal_plugins.h>

namespace app_view_models {

PluginTreeGroup::PluginTreeGroup(tracktion::Edit &e)
    : name("Plugins"), edit(e) {
    scanForPlugins();

    // we need to add the app internal plugins to the cache:
    // edit.engine.getPluginManager().createBuiltInType<internal_plugins::DrumSamplerPlugin>();

    auto &list = edit.engine.getPluginManager().knownPluginList;

    {
        // Add instruments group
        // Add instruments and effects group
        auto instrumentsFolder = new PluginTreeGroup(edit, "Instruments");
        auto effectsFolder = new PluginTreeGroup(edit, "Effects");
        auto midiFxFolder = new PluginTreeGroup(edit, "MIDI FX");
        // Populate the built in plugins
        // num is used to provide a unique identifier for the built in plugins
        int num = 1;
        // auto builtinFolder = new PluginTreeGroup(edit, "Builtin Plugins");
        addSubItem(instrumentsFolder);
        addSubItem(effectsFolder);
        addSubItem(midiFxFolder);
        instrumentsFolder->populateBuiltInInstruments(num);
        effectsFolder->populateBuiltInEffects(num);
        midiFxFolder->populateBuiltInMidiFx(num);

        instrumentsFolder->populateExternalInstruments(list);
        effectsFolder->populateExternalEffects(list);
    }
}

PluginTreeGroup::PluginTreeGroup(tracktion::Edit &e, const juce::String &s)
    : name(s), edit(e) {
    jassert(name.isNotEmpty());
}

juce::String PluginTreeGroup::getUniqueName() const { return name; }

void PluginTreeGroup::populateExternalInstruments(juce::KnownPluginList &list) {
    for (const auto &description : list.getTypes()) {
        if (description.isInstrument)
            addSubItem(new PluginTreeItem(description));
    }
}

void PluginTreeGroup::populateExternalEffects(juce::KnownPluginList &list) {
    for (const auto &description : list.getTypes()) {
        if (!description.isInstrument)
            addSubItem(new PluginTreeItem(description));
    }
}

template <class FilterClass>
void addInternalPlugin(PluginTreeBase &item, int &num, bool synth = false,
                       juce::String name = FilterClass::getPluginName()) {
    item.addSubItem(new PluginTreeItem(juce::String(num++) + "_trkbuiltin",
                                       name, FilterClass::xmlTypeName, synth,
                                       false));
}

void PluginTreeGroup::populateBuiltInInstruments(int &num) {
    addInternalPlugin<tracktion::FourOscPlugin>(*this, num, true);
    addInternalPlugin<tracktion::SamplerPlugin>(*this, num, true);
    addInternalPlugin<internal_plugins::DrumSamplerPlugin>(*this, num, true);
    addInternalPlugin<internal_plugins::StringEnginePlugin>(*this, num, true);
    addInternalPlugin<internal_plugins::FmEnginePlugin>(*this, num, true);
    addInternalPlugin<internal_plugins::PulseEnginePlugin>(*this, num, true);
}

void PluginTreeGroup::populateBuiltInEffects(int &num) {
    //        addInternalPlugin<tracktion::VolumeAndPanPlugin>(*this,
    //        num); addInternalPlugin<tracktion::LevelMeterPlugin>(*this,
    //        num);
    addInternalPlugin<tracktion::EqualiserPlugin>(*this, num);
    addInternalPlugin<tracktion::ReverbPlugin>(*this, num);
    addInternalPlugin<tracktion::DelayPlugin>(*this, num);
    addInternalPlugin<tracktion::ChorusPlugin>(*this, num);
    addInternalPlugin<tracktion::PhaserPlugin>(*this, num);
    addInternalPlugin<tracktion::CompressorPlugin>(*this, num, false,
                                                   "Compressor");
    addInternalPlugin<tracktion::PitchShiftPlugin>(*this, num, false,
                                                   "Pitch Shift");
    addInternalPlugin<tracktion::LowPassPlugin>(*this, num, false, "LPF/HPF");
    //        addInternalPlugin<tracktion::MidiModifierPlugin>(*this,
    //        num);
    //        addInternalPlugin<tracktion::MidiPatchBayPlugin>(*this,
    //        num); addInternalPlugin<tracktion::PatchBayPlugin>(*this,
    //        num); addInternalPlugin<tracktion::AuxSendPlugin>(*this,
    //        num); addInternalPlugin<tracktion::AuxReturnPlugin>(*this,
    //        num); addInternalPlugin<tracktion::TextPlugin>(*this, num);
    //        addInternalPlugin<tracktion::FreezePointPlugin>(*this,
    //        num);
}

void PluginTreeGroup::populateBuiltInMidiFx(int &num) {
    addInternalPlugin<internal_plugins::ArpeggiatorPlugin>(*this, num);
    addInternalPlugin<internal_plugins::ScaleLockPlugin>(*this, num);
    addInternalPlugin<internal_plugins::ChordPlugin>(*this, num);
}

void PluginTreeGroup::scanForPlugins() const {
    // scans the VST3 folders only when they changed since the last time
    PluginScanner::loadOrScan(edit.engine);
}

} // namespace app_view_models