#include "AvailablePluginParametersListViewModel.h"

namespace app_view_models {

AvailablePluginParametersListViewModel::AvailablePluginParametersListViewModel(
    tracktion::AudioTrack::Ptr t, tracktion::Plugin::Ptr p)
    : track(t), plugin(p),
      state(track->state.getOrCreateChildWithName(
          IDs::AVAILABLE_PLUGIN_PARAMETERS_LIST_VIEW_STATE, nullptr)),
      itemListState(state, plugin->getAutomatableParameters().size()) {}

juce::StringArray AvailablePluginParametersListViewModel::getItemNames() {
    juce::StringArray itemNames;
    for (auto parameter : plugin->getAutomatableParameters())
        itemNames.add(parameter->getParameterName());

    return itemNames;
}

tracktion::AutomatableParameter::Ptr
AvailablePluginParametersListViewModel::getSelectedItem() {
    return plugin
        ->getAutomatableParameters()[itemListState.getSelectedItemIndex()];
}

tracktion::Modifier *
AvailablePluginParametersListViewModel::addModifierToSelectedParameter(
    juce::Identifier modifierID) {
    if (auto pluginParameter = getSelectedItem()) {
        auto modifier = track->getModifierList().insertModifier(
            juce::ValueTree(modifierID), -1, nullptr);
        applyModifierDefaults(modifier.get());
        pluginParameter->addModifier(*modifier);
        return modifier;
    }

    return nullptr;
}

} // namespace app_view_models