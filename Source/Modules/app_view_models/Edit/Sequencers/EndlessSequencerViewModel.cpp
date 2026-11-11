#include "EndlessSequencerViewModel.h"

namespace app_view_models {

namespace {
const juce::Identifier endlessEndlessState("ENDLESS_SEQUENCER");
const juce::Identifier endlessNotesId("notes"), endlessRateId("rate"),
    endlessGateId("gate"), endlessSwingId("swing"),
    endlessTransposeId("transpose");
const double endlessRateBeats[] = {1.0, 0.5, 0.25, 0.125};
} // namespace

const juce::StringArray &EndlessSequencerViewModel::rateNames() {
    static const juce::StringArray names{"1/4", "1/8", "1/16", "1/32"};
    return names;
}

EndlessSequencerViewModel::EndlessSequencerViewModel(
    tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(endlessEndlessState,
                                                            nullptr)),
      clip(t) {
    for (auto n : juce::StringArray::fromTokens(
             state[endlessNotesId].toString(), ",", ""))
        if (n.trim().isNotEmpty())
            input.notes.add(juce::jlimit(-1, 127, n.getIntValue()));
    rateIndex = juce::jlimit(0, 3, int(state.getProperty(endlessRateId, 2)));
    input.gate = double(state.getProperty(endlessGateId, 0.6));
    input.swing = double(state.getProperty(endlessSwingId, 0.0));
    input.transpose = int(state.getProperty(endlessTransposeId, 0));
    changed();
}

void EndlessSequencerViewModel::changed() {
    juce::StringArray notes;
    for (auto n : input.notes)
        notes.add(juce::String(n));
    state.setProperty(endlessNotesId, notes.joinIntoString(","), nullptr);
    state.setProperty(endlessRateId, rateIndex, nullptr);
    state.setProperty(endlessGateId, input.gate, nullptr);
    state.setProperty(endlessSwingId, input.swing, nullptr);
    state.setProperty(endlessTransposeId, input.transpose, nullptr);

    input.stepLength = endlessRateBeats[rateIndex];
    clip.write(Sequencers::generateEndless(input),
               Sequencers::endlessLength(input));
}

void EndlessSequencerViewModel::startRecording() {
    clip.stop();
    recording = true;
    input.notes.clear();
    input.transpose = 0;
    changed();
}

void EndlessSequencerViewModel::finishRecording() { recording = false; }

void EndlessSequencerViewModel::notePressed(int noteNumber) {
    if (recording) {
        if (input.notes.size() < 64)
            input.notes.add(noteNumber);
        changed();
        return;
    }
    // transpose so the first note of the loop lands on the key
    for (auto n : input.notes)
        if (n >= 0) {
            input.transpose = juce::jlimit(-24, 24, noteNumber - n);
            changed();
            return;
        }
}

void EndlessSequencerViewModel::addRest() {
    if (recording && input.notes.size() < 64) {
        input.notes.add(-1);
        changed();
    }
}

void EndlessSequencerViewModel::nudgeTranspose(int delta) {
    input.transpose = juce::jlimit(-24, 24, input.transpose + delta);
    changed();
}

void EndlessSequencerViewModel::resetTranspose() {
    input.transpose = 0;
    changed();
}

void EndlessSequencerViewModel::nudgeRate(int delta) {
    rateIndex = juce::jlimit(0, 3, rateIndex + delta);
    changed();
}

void EndlessSequencerViewModel::nudgeGate(int delta) {
    input.gate = juce::jlimit(0.1, 1.0, input.gate + delta * 0.05);
    changed();
}

void EndlessSequencerViewModel::nudgeSwing(int delta) {
    input.swing = juce::jlimit(0.0, 1.0, input.swing + delta * 0.05);
    changed();
}

void EndlessSequencerViewModel::play() {
    recording = false;
    if (!input.notes.isEmpty())
        clip.play();
}

int EndlessSequencerViewModel::getPlayingIndex() const {
    if (!isPlaying() || input.notes.isEmpty())
        return -1;
    return int(clip.getPositionBeats() / input.stepLength) % input.notes.size();
}

} // namespace app_view_models
