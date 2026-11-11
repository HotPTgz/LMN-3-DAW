#pragma once

namespace app_view_models {

// Drums: one row per kit sound (the keys from the drum kit's first note up),
// up to 64 steps, hits at three strengths, swing, a level per sound and a
// fill pattern that replaces the main one every few passes.
class DrumSequencerViewModel {
  public:
    explicit DrumSequencerViewModel(tracktion::AudioTrack::Ptr t);

    // the cursor
    int getSelectedRow() const { return selectedRow; }
    int getSelectedStep() const { return selectedStep; }
    void moveRow(int delta);
    void moveStep(int delta);
    void selectRowForNote(int noteNumber);

    // which grid the cursor edits
    bool isEditingFill() const { return editingFill; }
    void toggleEditingFill();

    // cells of the grid being edited: 0 off, 1 ghost, 2 normal, 3 accent
    int getCell(int row, int step) const;
    // + and a key: toggle that sound on the cursor step, or while playing,
    // put a hit on the step that is playing
    void hitNote(int noteNumber);
    void nudgeVelocity(int delta);
    void clearSelected();

    int getLength() const { return input.length; }
    void nudgeLength(int delta);
    int getStepsPerBar() const { return input.stepsPerBar; }
    void nudgeStepsPerBar(int delta);
    double getSwing() const { return input.swing; }
    void nudgeSwing(int delta);
    float getLevel(int row) const { return input.levels[size_t(row)]; }
    void nudgeLevel(int delta);
    int getFillEvery() const { return input.fillEvery; }
    void nudgeFillEvery(int delta);

    juce::String getRowName(int row) const;

    void play() { clip.play(); }
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    // -1 when not playing
    int getPlayingStep() const;
    bool isPlayingFill() const;

    const Sequencers::DrumInput &getInput() const { return input; }

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::DrumInput input;
    SequencerClip clip;
    juce::StringArray rowNames;
    int selectedRow = 0, selectedStep = 0;
    bool editingFill = false;

    Sequencers::DrumGrid &editGrid();
    const Sequencers::DrumGrid &editGrid() const;
    void changed();
};

} // namespace app_view_models
