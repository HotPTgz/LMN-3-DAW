#include "DrumSequencerViewModel.h"

namespace app_view_models {

namespace {
const juce::Identifier drumDrumState("DRUM_SEQUENCER");
const juce::Identifier drumGridId("grid"), drumFillId("fill"),
    drumLengthId("length"), drumStepsPerBarId("stepsPerBar"),
    drumSwingId("swing"), drumFillEveryId("fillEvery"), drumLevelsId("levels");

template <typename T>
T drumNextOption(const juce::Array<T> &options, T current, int delta) {
    int index = options.indexOf(current);
    if (index < 0)
        index = 0;
    return options[juce::jlimit(0, options.size() - 1, index + delta)];
}
} // namespace

DrumSequencerViewModel::DrumSequencerViewModel(tracktion::AudioTrack::Ptr t)
    : track(t),
      state(track->state.getOrCreateChildWithName(drumDrumState, nullptr)),
      clip(t) {
    input.main = Sequencers::DrumGrid::fromString(state[drumGridId].toString());
    input.fill = Sequencers::DrumGrid::fromString(state[drumFillId].toString());
    input.length = juce::jlimit(16, Sequencers::drumMaxSteps,
                                int(state.getProperty(drumLengthId, 16)));
    input.stepsPerBar = int(state.getProperty(drumStepsPerBarId, 16));
    input.swing = double(state.getProperty(drumSwingId, 0.0));
    input.fillEvery = int(state.getProperty(drumFillEveryId, 0));
    auto levels =
        juce::StringArray::fromTokens(state[drumLevelsId].toString(), ",", "");
    for (int i = 0; i < juce::jmin(levels.size(), Sequencers::drumRows); i++)
        input.levels[size_t(i)] =
            juce::jlimit(0.0f, 1.0f, levels[i].getFloatValue());

    // kit sound names, when the track plays a drum kit
    for (int row = 0; row < Sequencers::drumRows; row++)
        rowNames.add("SOUND " + juce::String(row + 1));
    for (auto sampler :
         track->pluginList.getPluginsOfType<tracktion::SamplerPlugin>())
        for (int i = 0; i < sampler->getNumSounds(); i++) {
            int row = sampler->getKeyNote(i) - Sequencers::firstDrumNote;
            if (juce::isPositiveAndBelow(row, Sequencers::drumRows))
                rowNames.set(row, sampler->getSoundName(i)
                                      .upToLastOccurrenceOf(".", false, false)
                                      .toUpperCase());
        }

    // start on the first sound that has hits
    for (int row = 0; row < Sequencers::drumRows; row++) {
        bool any = false;
        for (auto cell : input.main.cells[size_t(row)])
            any = any || cell > 0;
        if (any) {
            selectedRow = row;
            break;
        }
    }
    changed();
}

Sequencers::DrumGrid &DrumSequencerViewModel::editGrid() {
    return editingFill ? input.fill : input.main;
}

const Sequencers::DrumGrid &DrumSequencerViewModel::editGrid() const {
    return editingFill ? input.fill : input.main;
}

void DrumSequencerViewModel::changed() {
    state.setProperty(drumGridId, input.main.toString(), nullptr);
    state.setProperty(drumFillId, input.fill.toString(), nullptr);
    state.setProperty(drumLengthId, input.length, nullptr);
    state.setProperty(drumStepsPerBarId, input.stepsPerBar, nullptr);
    state.setProperty(drumSwingId, input.swing, nullptr);
    state.setProperty(drumFillEveryId, input.fillEvery, nullptr);
    juce::StringArray levels;
    for (auto level : input.levels)
        levels.add(juce::String(level, 2));
    state.setProperty(drumLevelsId, levels.joinIntoString(","), nullptr);

    double length = input.length * Sequencers::stepBeats(input.stepsPerBar) *
                    Sequencers::drumPasses(input);
    clip.write(Sequencers::generateDrums(input), length);
}

void DrumSequencerViewModel::moveRow(int delta) {
    selectedRow =
        juce::jlimit(0, Sequencers::drumRows - 1, selectedRow + delta);
}

void DrumSequencerViewModel::moveStep(int delta) {
    selectedStep = juce::jlimit(0, input.length - 1, selectedStep + delta);
}

void DrumSequencerViewModel::selectRowForNote(int noteNumber) {
    int row = noteNumber - Sequencers::firstDrumNote;
    if (juce::isPositiveAndBelow(row, Sequencers::drumRows))
        selectedRow = row;
}

void DrumSequencerViewModel::toggleEditingFill() { editingFill = !editingFill; }

int DrumSequencerViewModel::getCell(int row, int step) const {
    if (!juce::isPositiveAndBelow(row, Sequencers::drumRows) ||
        !juce::isPositiveAndBelow(step, Sequencers::drumMaxSteps))
        return 0;
    return editGrid().cells[size_t(row)][size_t(step)];
}

void DrumSequencerViewModel::hitNote(int noteNumber) {
    int row = noteNumber - Sequencers::firstDrumNote;
    if (!juce::isPositiveAndBelow(row, Sequencers::drumRows))
        return;
    selectedRow = row;
    int step = selectedStep;
    if (isPlaying()) {
        // land on the nearest step to the playhead
        double stepLength = Sequencers::stepBeats(input.stepsPerBar);
        step = int(std::round(clip.getPositionBeats() / stepLength)) %
               input.length;
        editGrid().cells[size_t(row)][size_t(step)] = 2;
    } else {
        auto &cell = editGrid().cells[size_t(row)][size_t(step)];
        cell = cell > 0 ? 0 : 2;
    }
    changed();
}

void DrumSequencerViewModel::nudgeVelocity(int delta) {
    auto &cell = editGrid().cells[size_t(selectedRow)][size_t(selectedStep)];
    cell = juce::uint8(juce::jlimit(0, 3, int(cell) + delta));
    changed();
}

void DrumSequencerViewModel::clearSelected() {
    editGrid().cells[size_t(selectedRow)][size_t(selectedStep)] = 0;
    changed();
}

void DrumSequencerViewModel::nudgeLength(int delta) {
    input.length =
        juce::jlimit(16, Sequencers::drumMaxSteps, input.length + delta * 16);
    selectedStep = juce::jmin(selectedStep, input.length - 1);
    changed();
}

void DrumSequencerViewModel::nudgeStepsPerBar(int delta) {
    input.stepsPerBar =
        drumNextOption<int>({4, 8, 16, 32}, input.stepsPerBar, delta);
    changed();
}

void DrumSequencerViewModel::nudgeSwing(int delta) {
    input.swing = juce::jlimit(0.0, 1.0, input.swing + delta * 0.05);
    changed();
}

void DrumSequencerViewModel::nudgeLevel(int delta) {
    auto &level = input.levels[size_t(selectedRow)];
    level = juce::jlimit(0.0f, 1.0f, level + delta * 0.05f);
    changed();
}

void DrumSequencerViewModel::nudgeFillEvery(int delta) {
    input.fillEvery = drumNextOption<int>({0, 2, 4, 8}, input.fillEvery, delta);
    changed();
}

juce::String DrumSequencerViewModel::getRowName(int row) const {
    return rowNames[row];
}

int DrumSequencerViewModel::getPlayingStep() const {
    if (!isPlaying())
        return -1;
    double stepLength = Sequencers::stepBeats(input.stepsPerBar);
    return int(clip.getPositionBeats() / stepLength) % input.length;
}

bool DrumSequencerViewModel::isPlayingFill() const {
    if (!isPlaying())
        return false;
    int passes = Sequencers::drumPasses(input);
    if (passes <= 1)
        return false;
    double passLength = input.length * Sequencers::stepBeats(input.stepsPerBar);
    return int(clip.getPositionBeats() / passLength) == passes - 1;
}

} // namespace app_view_models
