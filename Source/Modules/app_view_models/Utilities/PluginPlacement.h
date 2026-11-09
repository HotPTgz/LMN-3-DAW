#pragma once

namespace app_view_models {
namespace PluginPlacement {

// Index of the track's instrument, or -1
int findInstrumentIndex(tracktion::PluginList &list);

/**
 * Adds a plugin to a track in the right place:
 *  - MIDI FX go after any existing MIDI FX and before the instrument, so
 *    they change the notes the instrument plays
 *  - an instrument replaces the existing instrument, or goes after the MIDI FX
 *  - effects go before the track's volume and level meter plugins
 */
void insertPlugin(tracktion::Track &track, tracktion::Plugin::Ptr plugin);

} // namespace PluginPlacement
} // namespace app_view_models
