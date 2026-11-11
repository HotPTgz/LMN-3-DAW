#pragma once

// Finds VST3 plugins once and remembers them. The list is kept in
// ~/.config/LMN-3/plugins.xml with a fingerprint of the VST3 folders, and the
// folders are only scanned again when something in them changes (a plugin
// added, removed or updated).
namespace app_view_models::PluginScanner {

// ~/.vst3, /usr/lib/vst3 and /usr/local/lib/vst3, those that exist
juce::Array<juce::File> getSearchFolders();

// The plugin bundles in the folders with their modification times
juce::String fingerprint(const juce::Array<juce::File> &folders);

// ~/.config/LMN-3/plugins.xml
juce::File getCacheFile();

// Fills the engine's known plugin list from the cache, or scans the folders
// (and saves the cache) when they changed. Returns true if it scanned.
bool loadOrScan(tracktion::Engine &engine, const juce::File &cacheFile,
                const juce::Array<juce::File> &folders);

// The same with the usual folders and cache file
bool loadOrScan(tracktion::Engine &engine);

} // namespace app_view_models::PluginScanner
