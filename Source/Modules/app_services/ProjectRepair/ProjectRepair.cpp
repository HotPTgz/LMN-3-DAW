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

// Clip times a damaged file can hold: a length Tracktion would correct
// while the song is being built (below zero, not a number), which then
// notifies a clip list that isn't finished yet and crashes; or a clip so far
// out (a billion seconds) that setting up playback runs out of memory. Put
// them back inside the longest song Tracktion allows.
static int mendClipTimes(juce::ValueTree v) {
    int mended = 0;
    if (v.getType().toString().endsWith("CLIP")) {
        const double maxEnd = tracktion::Edit::getMaximumEditEnd().inSeconds();
        auto number = [&v](const juce::Identifier &id) {
            return v.getProperty(id).toString().getDoubleValue();
        };
        auto fix = [&](const juce::Identifier &id, double value) {
            v.setProperty(id, value, nullptr);
            mended++;
        };
        double start = number(tracktion::IDs::start);
        if (v.hasProperty(tracktion::IDs::start) &&
            !(start >= 0.0 && start <= maxEnd))
            fix(tracktion::IDs::start, start = 0.0);
        double length = number(tracktion::IDs::length);
        if (v.hasProperty(tracktion::IDs::length) &&
            !(length >= 0.0 && start + length <= maxEnd))
            fix(tracktion::IDs::length, 0.0);
        double offset = number(tracktion::IDs::offset);
        if (v.hasProperty(tracktion::IDs::offset) &&
            !(std::abs(offset) <= maxEnd))
            fix(tracktion::IDs::offset, 0.0);
    }
    for (auto child : v)
        mended += mendClipTimes(child);
    return mended;
}

// Every plugin and modifier knob back inside its range. Knobs normally can't
// leave it, but a damaged file can hold anything, and some of Tracktion's
// modifiers then compute a step number from it and read outside their step
// list.
static int mendKnobValues(tracktion::Edit &edit) {
    int mended = 0;
    auto mend = [&mended](tracktion::AutomatableEditItem &item) {
        for (auto p : item.getAutomatableParameters()) {
            auto range = p->valueRange;
            float value = p->getCurrentBaseValue();
            if (std::isfinite(value) && value >= range.start && value <= range.end)
                continue;
            float fixed = std::isfinite(value)
                              ? juce::jlimit(range.start, range.end, value)
                              : juce::jlimit(range.start, range.end,
                                             p->getDefaultValue());
            p->setParameter(fixed, juce::dontSendNotification);
            mended++;
        }
    };
    for (auto track : tracktion::getAllTracks(edit)) {
        for (auto plugin : track->pluginList)
            mend(*plugin);
        for (auto m : track->getModifierList().getModifiers())
            mend(*m);
    }
    for (auto plugin : edit.getMasterPluginList())
        mend(*plugin);
    return mended;
}

std::unique_ptr<tracktion::Edit> loadProject(tracktion::Engine &engine,
                                             const juce::File &file,
                                             bool *startedEmpty) {
    auto state = tracktion::loadEditFromFile(engine, file, {});
    // Tracktion makes a fresh song for an empty file, but hands back nothing
    // for a damaged one (and anything at all for XML that isn't a project);
    // an Edit made from that crashes
    const bool unreadable = !state.isValid() ||
                            !state.hasType(tracktion::IDs::EDIT) ||
                            state.getNumChildren() == 0;
    if (startedEmpty != nullptr)
        *startedEmpty = unreadable;
    if (unreadable) {
        if (file.existsAsFile() && file.getSize() > 0) {
            auto backup = file.getSiblingFile(file.getFileNameWithoutExtension() +
                                              " (damaged).tracktionedit.bak");
            if (backup.exists())
                backup = backup.getNonexistentSibling(true);
            file.copyFileTo(backup);
            juce::Logger::writeToLog("Could not read " + file.getFullPathName() +
                                     "; kept it as " + backup.getFileName() +
                                     " and started an empty song");
        }
        state = juce::ValueTree(tracktion::IDs::EDIT);
        state.setProperty(tracktion::IDs::appVersion,
                          engine.getPropertyStorage().getApplicationVersion(),
                          nullptr);
    }
    if (repairProjectState(state) > 0)
        juce::Logger::writeToLog("Mended old Drive/Filter settings in " +
                                 file.getFileName());
    if (int mended = mendClipTimes(state))
        juce::Logger::writeToLog("Mended " + juce::String(mended) +
                                 " clip times in " + file.getFileName());
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
    auto edit = std::make_unique<tracktion::Edit>(options);
    if (int mended = mendKnobValues(*edit))
        juce::Logger::writeToLog("Mended " + juce::String(mended) +
                                 " knob values in " + file.getFileName());
    return edit;
}

} // namespace app_services
