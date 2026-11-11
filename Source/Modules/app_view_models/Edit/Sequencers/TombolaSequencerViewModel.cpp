#include "TombolaSequencerViewModel.h"

namespace app_view_models {

namespace {
const juce::Identifier tombolaTombolaState("TOMBOLA_SEQUENCER");
const juce::Identifier tombolaBallsId("balls"), tombolaSpinId("spin"),
    tombolaGravityId("gravity"), tombolaSidesId("sides"),
    tombolaBounceId("bounce"), tombolaSeedId("seed");
} // namespace

TombolaSequencerViewModel::TombolaSequencerViewModel(
    tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(tombolaTombolaState,
                                                            nullptr)),
      clip(t) {
    for (auto n : juce::StringArray::fromTokens(
             state[tombolaBallsId].toString(), ",", ""))
        if (n.trim().isNotEmpty() && input.balls.size() < maxBalls)
            input.balls.add(juce::jlimit(0, 127, n.getIntValue()));
    input.spin = double(state.getProperty(tombolaSpinId, 0.4));
    input.gravity = double(state.getProperty(tombolaGravityId, 0.35));
    input.sides = int(state.getProperty(tombolaSidesId, 6));
    input.bounce = double(state.getProperty(tombolaBounceId, 0.7));
    input.seed = int(state.getProperty(tombolaSeedId, 0));
    changed();
}

void TombolaSequencerViewModel::changed() {
    juce::StringArray balls;
    for (auto b : input.balls)
        balls.add(juce::String(b));
    state.setProperty(tombolaBallsId, balls.joinIntoString(","), nullptr);
    state.setProperty(tombolaSpinId, input.spin, nullptr);
    state.setProperty(tombolaGravityId, input.gravity, nullptr);
    state.setProperty(tombolaSidesId, input.sides, nullptr);
    state.setProperty(tombolaBounceId, input.bounce, nullptr);
    state.setProperty(tombolaSeedId, input.seed, nullptr);

    simulation = std::make_unique<Sequencers::TombolaSimulation>(input);
    clip.write(simulation->getNotes(), input.lengthBeats);
}

void TombolaSequencerViewModel::addBall(int noteNumber) {
    if (input.balls.size() >= maxBalls)
        input.balls.remove(0);
    input.balls.add(noteNumber);
    changed();
}

void TombolaSequencerViewModel::popBall() {
    if (!input.balls.isEmpty()) {
        input.balls.removeLast();
        changed();
    }
}

void TombolaSequencerViewModel::nudgeSpin(int delta) {
    input.spin = juce::jlimit(-1.0, 1.0, input.spin + delta * 0.05);
    changed();
}

void TombolaSequencerViewModel::nudgeGravity(int delta) {
    input.gravity = juce::jlimit(0.0, 1.0, input.gravity + delta * 0.05);
    changed();
}

void TombolaSequencerViewModel::nudgeSides(int delta) {
    input.sides = juce::jlimit(3, 8, input.sides + delta);
    changed();
}

void TombolaSequencerViewModel::nudgeBounce(int delta) {
    input.bounce = juce::jlimit(0.0, 1.0, input.bounce + delta * 0.05);
    changed();
}

void TombolaSequencerViewModel::shake() {
    input.seed++;
    changed();
}

} // namespace app_view_models
