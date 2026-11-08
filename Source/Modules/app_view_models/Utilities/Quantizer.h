#pragma once

namespace app_view_models {
namespace IDs {
const juce::Identifier QUANTIZE_SETTINGS("QUANTIZE_SETTINGS");
const juce::Identifier quantizeGridIndex("quantizeGridIndex");
const juce::Identifier quantizeSwing("quantizeSwing");
} // namespace IDs

namespace Quantizer {

struct Grid {
    juce::String name;
    double beats; // grid spacing in quarter-note beats
    bool isTriplet;
};

// Available grids, coarsest first
const juce::Array<Grid> &getGrids();
int getDefaultGridIndex(); // 1/16

constexpr int maxSwingPercent = 75;
constexpr int swingStepPercent = 1;

/**
 * Snaps a position (in beats from the start of the edit) to the grid.
 *
 * Swing (0-75%) delays every second grid step by
 * swingPercent/100 * gridBeats/2, so 0% is straight and ~66% gives a triplet
 * shuffle. Swing is ignored for triplet grids.
 */
double quantizeBeat(double beat, double gridBeats, int swingPercent);

/**
 * Quantizes the start of every note in the clip to the grid, aligned to the
 * edit's beat grid, keeping note lengths unchanged.
 * Returns the number of notes that moved.
 */
int quantizeClip(tracktion::MidiClip &clip, double gridBeats, int swingPercent,
                 juce::UndoManager *undoManager);

} // namespace Quantizer
} // namespace app_view_models
