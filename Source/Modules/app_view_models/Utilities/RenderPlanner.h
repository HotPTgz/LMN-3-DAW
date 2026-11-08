#pragma once

namespace app_view_models {
namespace RenderPlanner {

enum class RenderType { mix, stems };

struct RenderJob {
    juce::File file;
    juce::BigInteger tracksToDo; // indices into tracktion::getAllTracks(edit)
};

// The loop range when looping is enabled, otherwise the whole edit
tracktion::TimeRange getRenderRange(tracktion::Edit &edit);
bool isLoopRender(tracktion::Edit &edit);

// "<project>_mix_<timestamp>" or "<project>_mix-loop_<timestamp>", where the
// project name is taken from the edit file
juce::String getBaseName(tracktion::Edit &edit, RenderType type,
                         juce::Time time);

/**
 * Works out the files to render.
 *  - mix:   one file containing every track
 *  - stems: one file per audio track that has clips and isn't muted, inside
 *           a "<project>_stems_<timestamp>" folder, named "01_<track>.wav"
 */
juce::Array<RenderJob> planRender(tracktion::Edit &edit, RenderType type,
                                  const juce::File &rendersDirectory,
                                  juce::Time time);

} // namespace RenderPlanner
} // namespace app_view_models
