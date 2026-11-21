#include "ProjectRepair.h"

namespace app_services {

namespace {
bool hasAny(const juce::ValueTree &v, std::initializer_list<const char *> ids) {
    for (auto id : ids)
        if (v.hasProperty(juce::Identifier(id)))
            return true;
    return false;
}

int repair(juce::ValueTree v) {
    int mended = 0;
    if (v.hasType(tracktion::IDs::PLUGIN)) {
        auto type = v[tracktion::IDs::type].toString();
        // a number where the plugin type should be
        if (type.isNotEmpty() && type.containsOnly("0123456789.-")) {
            const float value = type.getFloatValue();
            // Knobs left at their default aren't saved, so a plugin may have
            // nothing but its TYPE. Recognise it by any other knob it kept;
            // with none, only the Filter has a fifth type (Notch), and
            // otherwise it's most likely the Drive.
            bool filter = hasAny(v, {"cutoff", "resonance", "envelope", "lfo",
                                     "envSpeed", "lfoRate"}) ||
                          (!hasAny(v, {"drive", "tone", "mix", "output"}) &&
                           value > 3.5f);
            v.setProperty(tracktion::IDs::type, filter ? "lmn3Filter" : "lmn3Drive",
                          nullptr);
            v.setProperty(filter ? "filterType" : "driveType", value, nullptr);
            mended++;
        }
    }
    for (auto child : v)
        mended += repair(child);
    return mended;
}
} // namespace

int repairProjectState(juce::ValueTree &editState) { return repair(editState); }

std::unique_ptr<tracktion::Edit> loadProject(tracktion::Engine &engine,
                                             const juce::File &file) {
    auto state = tracktion::loadEditFromFile(engine, file, {});
    if (repairProjectState(state) > 0)
        juce::Logger::writeToLog("Mended old Drive/Filter settings in " +
                                 file.getFileName());
    auto id = tracktion::ProjectItemID::fromProperty(state,
                                                     tracktion::IDs::projectID);
    if (!id.isValid())
        id = tracktion::ProjectItemID::createNewID(0);
    tracktion::Edit::Options options = {engine,
                                        state,
                                        id,
                                        tracktion::Edit::forEditing,
                                        nullptr,
                                        tracktion::Edit::getDefaultNumUndoLevels(),
                                        [file] { return file; },
                                        {}};
    return std::make_unique<tracktion::Edit>(options);
}

} // namespace app_services
