#include "RenderPlanner.h"

namespace app_view_models {
namespace RenderPlanner {

bool isLoopRender(tracktion::Edit &edit) {
    auto &transport = edit.getTransport();
    return transport.looping.get() &&
           transport.getLoopRange().getLength().inSeconds() > 0.0;
}

tracktion::TimeRange getRenderRange(tracktion::Edit &edit) {
    if (isLoopRender(edit))
        return edit.getTransport().getLoopRange();

    return tracktion::TimeRange(tracktion::TimePosition::fromSeconds(0.0),
                                edit.getLength());
}

static juce::String getProjectName(tracktion::Edit &edit) {
    juce::String name;
    if (edit.editFileRetriever)
        name = edit.editFileRetriever().getFileNameWithoutExtension();

    if (name.isEmpty() || name == "edit")
        name = "LMN-3";

    return name;
}

juce::String getBaseName(tracktion::Edit &edit, RenderType type,
                         juce::Time time) {
    juce::String kind = type == RenderType::mix ? "mix" : "stems";
    if (isLoopRender(edit))
        kind += "-loop";

    return juce::File::createLegalFileName(getProjectName(edit) + "_" + kind +
                                           "_" +
                                           time.formatted("%Y-%m-%d_%H%M%S"));
}

juce::Array<RenderJob> planRender(tracktion::Edit &edit, RenderType type,
                                  const juce::File &rendersDirectory,
                                  juce::Time time) {
    juce::Array<RenderJob> jobs;
    auto allTracks = tracktion::getAllTracks(edit);
    auto baseName = getBaseName(edit, type, time);

    if (type == RenderType::mix) {
        RenderJob job;
        job.file = rendersDirectory.getNonexistentChildFile(baseName, ".wav",
                                                            false);
        for (int i = 0; i < allTracks.size(); i++)
            job.tracksToDo.setBit(i);
        jobs.add(job);
        return jobs;
    }

    auto stemsDirectory =
        rendersDirectory.getNonexistentChildFile(baseName, "", false);
    int stemNumber = 1;
    for (int i = 0; i < allTracks.size(); i++) {
        auto audioTrack = dynamic_cast<tracktion::AudioTrack *>(allTracks[i]);
        if (audioTrack == nullptr || audioTrack->getClips().isEmpty() ||
            audioTrack->isMuted(true))
            continue;

        auto trackName = audioTrack->getName().trim();
        if (trackName.isEmpty())
            trackName = "Track";

        RenderJob job;
        job.file = stemsDirectory.getChildFile(
            juce::String(stemNumber++).paddedLeft('0', 2) + "_" +
            juce::File::createLegalFileName(trackName) + ".wav");
        job.tracksToDo.setBit(i);
        jobs.add(job);
    }

    return jobs;
}

} // namespace RenderPlanner
} // namespace app_view_models
