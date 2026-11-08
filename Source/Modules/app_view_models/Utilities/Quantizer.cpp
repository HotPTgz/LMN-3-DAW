#include "Quantizer.h"

namespace app_view_models {
namespace Quantizer {

const juce::Array<Grid> &getGrids() {
    static const juce::Array<Grid> grids = []() {
        juce::Array<Grid> g;
        g.add(Grid{"1/4", 1.0, false});
        g.add(Grid{"1/8", 0.5, false});
        g.add(Grid{"1/8T", 1.0 / 3.0, true});
        g.add(Grid{"1/16", 0.25, false});
        g.add(Grid{"1/16T", 1.0 / 6.0, true});
        g.add(Grid{"1/32", 0.125, false});
        return g;
    }();
    return grids;
}

int getDefaultGridIndex() { return 3; }

static double swungStepPosition(juce::int64 step, double gridBeats,
                                double swingOffset) {
    double position = double(step) * gridBeats;
    if (step % 2 != 0)
        position += swingOffset;
    return position;
}

double quantizeBeat(double beat, double gridBeats, int swingPercent) {
    if (gridBeats <= 0.0)
        return beat;

    swingPercent = juce::jlimit(0, maxSwingPercent, swingPercent);
    const double swingOffset = swingPercent / 100.0 * gridBeats / 2.0;

    // The nearest swung grid point is one of the steps around the beat.
    // Swing only moves odd steps later, so check the step below, the step
    // above and the one after that.
    auto lowerStep = juce::int64(std::floor(beat / gridBeats));
    double best = swungStepPosition(lowerStep, gridBeats, swingOffset);
    for (juce::int64 step = lowerStep - 1; step <= lowerStep + 2; step++) {
        double candidate = swungStepPosition(step, gridBeats, swingOffset);
        if (std::abs(candidate - beat) < std::abs(best - beat))
            best = candidate;
    }

    return juce::jmax(0.0, best);
}

int quantizeClip(tracktion::MidiClip &clip, double gridBeats, int swingPercent,
                 juce::UndoManager *undoManager) {
    auto &sequence = clip.getSequence();
    const double contentStartBeat = clip.getContentStartBeat().inBeats();

    int moved = 0;
    // Copy the list, since moving notes can re-sort the sequence
    auto notes = sequence.getNotes();
    for (auto note : notes) {
        const double editBeat = contentStartBeat + note->getStartBeat().inBeats();
        const double quantizedEditBeat =
            quantizeBeat(editBeat, gridBeats, swingPercent);
        const double newStartBeat =
            juce::jmax(0.0, quantizedEditBeat - contentStartBeat);

        if (std::abs(newStartBeat - note->getStartBeat().inBeats()) > 1.0e-6) {
            note->setStartAndLength(
                tracktion::BeatPosition::fromBeats(newStartBeat),
                note->getLengthBeats(), undoManager);
            moved++;
        }
    }

    return moved;
}

} // namespace Quantizer
} // namespace app_view_models
