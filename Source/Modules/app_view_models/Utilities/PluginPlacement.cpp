#include "PluginPlacement.h"

namespace app_view_models {
namespace PluginPlacement {

int findInstrumentIndex(tracktion::PluginList &list) {
    for (int i = 0; i < list.size(); i++)
        if (list[i]->isSynth())
            return i;
    return -1;
}

// Number of MIDI FX at the start of the list
static int countLeadingMidiFx(tracktion::PluginList &list) {
    int count = 0;
    while (count < list.size() &&
           internal_plugins::MidiFxPlugin::isMidiFx(*list[count]))
        count++;
    return count;
}

void insertPlugin(tracktion::Track &track, tracktion::Plugin::Ptr plugin) {
    auto &list = track.pluginList;
    int instrumentIndex = findInstrumentIndex(list);

    if (internal_plugins::MidiFxPlugin::isMidiFx(*plugin)) {
        int index = instrumentIndex >= 0 ? instrumentIndex
                                         : countLeadingMidiFx(list);
        list.insertPlugin(plugin, index, nullptr);
        return;
    }

    if (plugin->isSynth()) {
        int index = countLeadingMidiFx(list);
        if (instrumentIndex >= 0) {
            index = instrumentIndex;
            list[instrumentIndex]->removeFromParent();
        }
        list.insertPlugin(plugin, index, nullptr);
        return;
    }

    // always insert effects before the volume and level plugins
    // (the 2 default plugins on every track)
    list.insertPlugin(plugin, juce::jmax(0, list.size() - 2), nullptr);
}

} // namespace PluginPlacement
} // namespace app_view_models
