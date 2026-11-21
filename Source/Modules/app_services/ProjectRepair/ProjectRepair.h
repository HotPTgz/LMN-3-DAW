#pragma once

namespace app_services {

// Opens a project, first mending what older versions of the app got wrong
// in saved files:
//
//  - Drive and Filter stored their TYPE knob under the same name as the
//    plugin's own type, so changing it overwrote "lmn3Drive"/"lmn3Filter"
//    with a number and the effect vanished when the project was opened
//    again. Such a plugin is recognised by its other settings, gets its type
//    back, and its TYPE knob keeps the value it had. (No other plugin's
//    type was ever stored as a number.)
//
// A file that can't be read at all (empty, cut off by a crash or a power
// cut, or not a project) opens as a new, empty song instead of crashing.
// The damaged file is kept next to it as "<name> (damaged).tracktionedit.bak"
// so nothing is lost. startedEmpty (if given) says whether that happened.
std::unique_ptr<tracktion::Edit> loadProject(tracktion::Engine &engine,
                                             const juce::File &file,
                                             bool *startedEmpty = nullptr);

// The mending on its own: returns how many plugins were mended
int repairProjectState(juce::ValueTree &editState);

} // namespace app_services
