#include "ExternalPluginViewModel.h"

namespace app_view_models {

bool ExternalPluginViewModel::isHidden(const juce::String &name) {
    auto lower = name.toLowerCase().trim();
    return lower.isEmpty() || lower.startsWith("midi cc") ||
           lower == "bypass" || lower.startsWith("unused");
}

ExternalPluginViewModel::ExternalPluginViewModel(tracktion::Plugin::Ptr p,
                                                 const KnobMaps::Map &map)
    : plugin(p) {
    juce::Array<tracktion::AutomatableParameter *> parameters;
    juce::StringArray names;
    for (auto parameter : plugin->getAutomatableParameters()) {
        if (isHidden(parameter->getParameterName()))
            continue;
        parameters.add(parameter);
        names.add(parameter->getParameterName());
    }

    juce::Array<int> used;
    if (map.isValid()) {
        for (auto &mapPage : map.pages) {
            Page page;
            page.name = mapPage.name.toUpperCase();
            page.fromMap = true;
            bool any = false;
            for (size_t i = 0; i < mapPage.knobs.size() && i < 8; i++) {
                int index = KnobMaps::findParameter(names, mapPage.knobs[i]);
                if (index < 0)
                    continue;
                page.knobs[i] = {mapPage.knobs[i].label, parameters[index]};
                used.addIfNotAlreadyThere(index);
                any = true;
            }
            if (any)
                pages.push_back(page);
        }
        mapped = !pages.empty();
    }

    // everything else, 8 to a page
    Page page;
    int slot = 0, count = 0;
    for (int i = 0; i < parameters.size(); i++) {
        if (used.contains(i))
            continue;
        if (slot == 0)
            page = Page{(mapped ? "MORE " : "PAGE ") + juce::String(++count),
                        {},
                        false};
        page.knobs[size_t(slot)] = {names[i].toUpperCase(), parameters[i]};
        if (++slot == 8) {
            pages.push_back(page);
            slot = 0;
        }
    }
    if (slot > 0)
        pages.push_back(page);
}

float ExternalPluginViewModel::getValue(
    tracktion::AutomatableParameter &parameter) {
    return parameter.getCurrentNormalisedValue();
}

juce::String
ExternalPluginViewModel::getText(tracktion::AutomatableParameter &parameter) {
    auto text = parameter.getCurrentValueAsString().trim();
    if (text.isEmpty())
        text =
            juce::String(juce::roundToInt(getValue(parameter) * 100.0f)) + "%";
    return text;
}

void ExternalPluginViewModel::nudge(tracktion::AutomatableParameter &parameter,
                                    int direction) {
    float step = 0.01f;
    if (parameter.isDiscrete() && parameter.getNumberOfStates() > 1)
        step = 1.0f / float(parameter.getNumberOfStates() - 1);
    parameter.setNormalisedParameter(
        juce::jlimit(0.0f, 1.0f, getValue(parameter) + step * direction),
        juce::sendNotification);
}

tracktion::ExternalPlugin *ExternalPluginViewModel::getExternal() {
    return dynamic_cast<tracktion::ExternalPlugin *>(plugin.get());
}

int ExternalPluginViewModel::getNumPresets() {
    if (auto external = getExternal())
        return external->getNumPrograms();
    return 0;
}

juce::String ExternalPluginViewModel::getPresetName(int index) {
    if (auto external = getExternal()) {
        auto name = external->getProgramName(index).trim();
        return name.isNotEmpty() ? name : "Preset " + juce::String(index + 1);
    }
    return {};
}

int ExternalPluginViewModel::getCurrentPreset() {
    if (auto external = getExternal())
        return external->getCurrentProgram();
    return 0;
}

void ExternalPluginViewModel::loadPreset(int index) {
    if (auto external = getExternal())
        if (juce::isPositiveAndBelow(index, getNumPresets()))
            external->setCurrentProgram(index, true);
}

} // namespace app_view_models
