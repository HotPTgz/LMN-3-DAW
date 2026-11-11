#pragma once

namespace app_view_models {

// Pattern: four 16-step patterns, A to D, played in a chain of up to 16
// links (e.g. A A B A C D).
class PatternSequencerViewModel {
  public:
    explicit PatternSequencerViewModel(tracktion::AudioTrack::Ptr t);

    int getEditPattern() const { return editPattern; }
    void nudgeEditPattern(int delta);
    int getSelectedStep() const { return selectedStep; }
    void moveStep(int delta);
    // + and a key: toggles that note on the cursor step, then moves on
    void toggleNote(int noteNumber);
    void clearStep();
    const Sequencers::PatternData &getPattern(int index) const;
    // the pattern being edited goes to the next letter (A -> B)
    void copyToNext();

    // view: which octave the grid shows (the lowest C)
    int getViewOctave() const { return viewOctave; }
    void nudgeViewOctave(int delta);

    const juce::Array<int> &getChain() const { return input.chain; }
    int getSelectedLink() const { return selectedLink; }
    void moveLink(int delta);
    void nudgeLinkPattern(int delta);
    // adds a link after the selected one, playing the pattern being edited
    void addLink();
    void removeLink();

    int getStepsPerBar() const { return input.stepsPerBar; }
    void nudgeStepsPerBar(int delta);

    void play() { clip.play(); }
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    // -1 when stopped
    int getPlayingLink() const;
    int getPlayingStep() const;

    static constexpr int maxLinks = 16;

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::PatternInput input;
    SequencerClip clip;
    int editPattern = 0, selectedStep = 0, selectedLink = 0;
    int viewOctave = 5;

    void changed();
};

} // namespace app_view_models
