#pragma once

namespace app_view_models {

// Tombola: balls bounce inside a spinning shape and play their note when they
// hit a wall. The bouncing is worked out ahead for a 4-bar loop and written
// into the clip, so what you see is what plays (and renders).
class TombolaSequencerViewModel {
  public:
    explicit TombolaSequencerViewModel(tracktion::AudioTrack::Ptr t);

    static constexpr int maxBalls = 8;
    const juce::Array<int> &getBalls() const { return input.balls; }
    // a key drops a ball with its note (the oldest goes when full)
    void addBall(int noteNumber);
    void popBall();

    double getSpin() const { return input.spin; }
    void nudgeSpin(int delta);
    double getGravity() const { return input.gravity; }
    void nudgeGravity(int delta);
    int getSides() const { return input.sides; }
    void nudgeSides(int delta);
    double getBounce() const { return input.bounce; }
    void nudgeBounce(int delta);
    void shake();

    void play() { clip.play(); }
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    double getPositionBeats() const { return clip.getPositionBeats(); }
    double getLengthBeats() const { return input.lengthBeats; }

    const Sequencers::TombolaSimulation &getSimulation() const {
        return *simulation;
    }

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::TombolaInput input;
    SequencerClip clip;
    std::unique_ptr<Sequencers::TombolaSimulation> simulation;

    void changed();
};

} // namespace app_view_models
