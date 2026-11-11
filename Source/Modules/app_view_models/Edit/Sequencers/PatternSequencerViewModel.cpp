#include "PatternSequencerViewModel.h"

namespace app_view_models {

namespace {
const juce::Identifier patternPatternState("PATTERN_SEQUENCER");
const juce::Identifier patternChainId("chain"),
    patternStepsPerBarId("stepsPerBar"), patternOctaveId("viewOctave");
juce::Identifier patternPatternId(int i) { return "pattern" + juce::String(i); }
} // namespace

PatternSequencerViewModel::PatternSequencerViewModel(
    tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(patternPatternState,
                                                            nullptr)),
      clip(t) {
    for (int i = 0; i < Sequencers::patternCount; i++)
        input.patterns[size_t(i)] = Sequencers::PatternData::fromString(
            state[patternPatternId(i)].toString());
    input.chain = Sequencers::chainFromString(state[patternChainId].toString());
    input.stepsPerBar = int(state.getProperty(patternStepsPerBarId, 16));
    viewOctave = juce::jlimit(0, 8, int(state.getProperty(patternOctaveId, 5)));
    changed();
}

void PatternSequencerViewModel::changed() {
    for (int i = 0; i < Sequencers::patternCount; i++)
        state.setProperty(patternPatternId(i),
                          input.patterns[size_t(i)].toString(), nullptr);
    state.setProperty(patternChainId, Sequencers::chainToString(input.chain),
                      nullptr);
    state.setProperty(patternStepsPerBarId, input.stepsPerBar, nullptr);
    state.setProperty(patternOctaveId, viewOctave, nullptr);
    clip.write(Sequencers::generatePattern(input),
               Sequencers::patternLength(input));
}

void PatternSequencerViewModel::nudgeEditPattern(int delta) {
    editPattern =
        juce::jlimit(0, Sequencers::patternCount - 1, editPattern + delta);
}

void PatternSequencerViewModel::moveStep(int delta) {
    selectedStep =
        juce::jlimit(0, Sequencers::patternSteps - 1, selectedStep + delta);
}

void PatternSequencerViewModel::toggleNote(int noteNumber) {
    auto &pitches =
        input.patterns[size_t(editPattern)].steps[size_t(selectedStep)];
    if (pitches.contains(noteNumber))
        pitches.removeFirstMatchingValue(noteNumber);
    else
        pitches.add(noteNumber);
    // keep the note in view
    if (noteNumber < viewOctave * 12 || noteNumber >= viewOctave * 12 + 12)
        viewOctave = juce::jlimit(0, 8, noteNumber / 12);
    moveStep(1);
    changed();
}

void PatternSequencerViewModel::clearStep() {
    input.patterns[size_t(editPattern)].steps[size_t(selectedStep)].clear();
    changed();
}

const Sequencers::PatternData &
PatternSequencerViewModel::getPattern(int index) const {
    return input
        .patterns[size_t(juce::jlimit(0, Sequencers::patternCount - 1, index))];
}

void PatternSequencerViewModel::copyToNext() {
    int next = (editPattern + 1) % Sequencers::patternCount;
    input.patterns[size_t(next)] = input.patterns[size_t(editPattern)];
    editPattern = next;
    changed();
}

void PatternSequencerViewModel::nudgeViewOctave(int delta) {
    viewOctave = juce::jlimit(0, 8, viewOctave + delta);
    state.setProperty(patternOctaveId, viewOctave, nullptr);
}

void PatternSequencerViewModel::moveLink(int delta) {
    selectedLink =
        juce::jlimit(0, input.chain.size() - 1, selectedLink + delta);
}

void PatternSequencerViewModel::nudgeLinkPattern(int delta) {
    auto &link = input.chain.getReference(selectedLink);
    link = juce::jlimit(0, Sequencers::patternCount - 1, link + delta);
    changed();
}

void PatternSequencerViewModel::addLink() {
    if (input.chain.size() >= maxLinks)
        return;
    input.chain.insert(selectedLink + 1, editPattern);
    selectedLink++;
    changed();
}

void PatternSequencerViewModel::removeLink() {
    if (input.chain.size() <= 1)
        return;
    input.chain.remove(selectedLink);
    selectedLink = juce::jmin(selectedLink, input.chain.size() - 1);
    changed();
}

void PatternSequencerViewModel::nudgeStepsPerBar(int delta) {
    juce::Array<int> options{4, 8, 16};
    int index = juce::jmax(0, options.indexOf(input.stepsPerBar));
    input.stepsPerBar = options[juce::jlimit(0, 2, index + delta)];
    changed();
}

int PatternSequencerViewModel::getPlayingLink() const {
    if (!isPlaying())
        return -1;
    double linkLength =
        Sequencers::patternSteps * Sequencers::stepBeats(input.stepsPerBar);
    return juce::jlimit(0, input.chain.size() - 1,
                        int(clip.getPositionBeats() / linkLength));
}

int PatternSequencerViewModel::getPlayingStep() const {
    if (!isPlaying())
        return -1;
    double stepLength = Sequencers::stepBeats(input.stepsPerBar);
    return int(clip.getPositionBeats() / stepLength) % Sequencers::patternSteps;
}

} // namespace app_view_models
