#pragma once

namespace app_view_models {

// Endless: play notes in one at a time (− for a rest) and they loop. While
// it plays, a key moves the whole loop so its first note lands on that key.
class EndlessSequencerViewModel {
  public:
    explicit EndlessSequencerViewModel(tracktion::AudioTrack::Ptr t);

    bool isRecording() const { return recording; }
    void startRecording();
    void finishRecording();
    // a key: adds the note while recording, else transposes the loop
    void notePressed(int noteNumber);
    void addRest();

    const juce::Array<int> &getNotes() const { return input.notes; }
    int getTranspose() const { return input.transpose; }
    void nudgeTranspose(int delta);
    void resetTranspose();

    static const juce::StringArray &rateNames();
    int getRateIndex() const { return rateIndex; }
    void nudgeRate(int delta);
    double getGate() const { return input.gate; }
    void nudgeGate(int delta);
    double getSwing() const { return input.swing; }
    void nudgeSwing(int delta);

    void play();
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    // the note playing now, -1 when stopped
    int getPlayingIndex() const;

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::EndlessInput input;
    SequencerClip clip;
    int rateIndex = 2;
    bool recording = false;

    void changed();
};

} // namespace app_view_models
