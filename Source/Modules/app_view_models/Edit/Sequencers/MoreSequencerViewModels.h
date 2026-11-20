#pragma once

namespace app_view_models {

// Euclidean: a ring of evenly spread hits for each kit sound
class EuclideanSequencerViewModel {
  public:
    explicit EuclideanSequencerViewModel(tracktion::AudioTrack::Ptr t);

    const Sequencers::EuclidRing &getRing(int row) const {
        return input.rings[size_t(juce::jlimit(0, Sequencers::drumRows - 1, row))];
    }
    int getSelected() const { return selected; }
    juce::String getRowName(int row) const { return rowNames[row]; }
    int getLengthSteps() const { return Sequencers::euclidLengthSteps(input); }
    int getVelocity() const { return input.velocity; }
    double getSwing() const { return input.swing; }

    void nudgeSound(int delta);
    void nextSound(); // the next sound that plays, or the next one
    void selectForNote(int noteNumber);
    void nudgeHits(int delta);
    void nudgeSteps(int delta);
    void nudgeRotate(int delta);
    void nudgeVelocity(int delta);
    void nudgeSwing(int delta);

    void play() { clip.play(); }
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    int getPlayingStep() const; // -1 when stopped

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::EuclidInput input;
    SequencerClip clip;
    juce::StringArray rowNames;
    int selected = 0;
    void changed();
};

//==============================================================================
// Turing: a looping random melody, locked or slowly changing
class TuringSequencerViewModel {
  public:
    explicit TuringSequencerViewModel(tracktion::AudioTrack::Ptr t);

    const Sequencers::TuringInput &getInput() const { return input; }
    const std::vector<int> &getNotes() const { return notes; }
    static const juce::StringArray &rateNames();
    int getRateIndex() const { return rateIndex; }

    void nudgeLock(int delta);
    void nudgeLength(int delta);
    void nudgeRange(int delta);
    void nudgeScale(int delta);
    void nudgeRate(int delta);
    void nudgeGate(int delta);
    void notePressed(int noteNumber); // sets the lowest note (and root)
    void reroll();                    // a new random melody

    void play() { clip.play(); }
    void stop() { clip.stop(); }
    bool isPlaying() const { return clip.isPlaying(); }
    int getPlayingStep() const; // -1 when stopped

  private:
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::TuringInput input;
    std::vector<int> notes;
    SequencerClip clip;
    int rateIndex = 1;
    void changed();
};

//==============================================================================
// Chords: a chord per bar on this track; the next tracks follow along
class ChordsSequencerViewModel {
  public:
    explicit ChordsSequencerViewModel(tracktion::AudioTrack::Ptr t);
    ~ChordsSequencerViewModel();

    const Sequencers::ChordsInput &getInput() const { return input; }
    int getSelectedBar() const { return selectedBar; }
    int getFollowers() const { return int(followers.size()); }
    int getMaxFollowers() const;
    int getPreset() const { return presetIndex; }
    // e.g. "Track 2 · bass: roots"
    juce::StringArray getFollowerNames() const;

    void nudgeKey(int delta);
    void nudgeBar(int delta);
    void nudgeChord(int delta);
    void nudgeFollow(int delta);
    void nudgePreset(int delta);
    void nudgeBars(int delta);
    void notePressed(int noteNumber); // the key's root, keeping major/minor

    void play();
    void stop();
    bool isPlaying() const { return clip.isPlaying(); }
    int getPlayingBar() const; // -1 when stopped
    double getPlayingBeat() const;

  private:
    struct Follower {
        tracktion::AudioTrack::Ptr track;
        std::unique_ptr<SequencerClip> clip;
        Sequencers::ChordRole role;
    };
    tracktion::AudioTrack::Ptr track;
    juce::ValueTree state;
    Sequencers::ChordsInput input;
    SequencerClip clip;
    std::vector<Follower> followers;
    int selectedBar = 0, presetIndex = 0;
    bool soloedFollowers = false;
    juce::Array<tracktion::AudioTrack *> nextTracks() const;
    void setFollowers(int count);
    void changed();
};

} // namespace app_view_models
