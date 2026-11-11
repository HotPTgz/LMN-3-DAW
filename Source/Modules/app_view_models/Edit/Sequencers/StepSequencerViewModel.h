namespace app_view_models {

namespace IDs {

const juce::Identifier STEP_SEQUENCER_STATE("STEP_SEQUENCER_VIEW_STATE");
const juce::Identifier selectedNoteIndex("selectedNoteIndex");
const juce::Identifier numberOfNotes("numberOfNotes");
const juce::Identifier notesPerMeasure("notesPerMeasure");

} // namespace IDs

class StepSequencerViewModel : public juce::ValueTree::Listener,
                               public FlaggedAsyncUpdater,
                               private tracktion::TransportControl::Listener {
  public:
    StepSequencerViewModel(tracktion::AudioTrack::Ptr t);
    ~StepSequencerViewModel() override;

    int getNumChannels();
    int getNumNotesPerChannel();

    bool hasNoteAt(int channel, int noteIndex);

    void toggleNoteNumberAtSelectedIndex(int noteNumber);

    int noteNumberToChannel(int noteNumber);

    int getSelectedNoteIndex();
    int getNumberOfNotes();

    void incrementSelectedNoteIndex();
    void decrementSelectedNoteIndex();

    void incrementNumberOfNotes();
    void decrementNumberOfNotes();

    int getNotesPerMeasure();
    void incrementNotesPerMeasure();
    void decrementNotesPerMeasure();

    void clearNotesAtSelectedIndex();

    // per-step tricks (the STEP page) for the selected step; with all = true
    // every step changes by the same amount
    Sequencers::StepTricks getTricks(int step);
    void nudgeVelocity(int delta, bool all = false);
    void nudgeTie(int delta, bool all = false);
    void nudgeChance(int delta, bool all = false);
    void nudgeRatchet(int delta, bool all = false);

    // groove (the GROOVE page)
    Sequencers::StepGroove getGroove();
    void nudgeSwing(int delta);
    void nudgeGate(int delta);
    void nudgeDirection(int delta);
    void nudgeHumanize(int delta);

    // passes of the pattern in the clip (more when it changes every time)
    int getPasses();
    bool isPlaying();

    void play();
    void stop();

    class Listener {
      public:
        virtual ~Listener() = default;

        virtual void patternChanged() {}
        virtual void selectedNoteIndexChanged(int newIndex) {}
        virtual void numberOfNotesChanged(int newNumberOfNotes) {}
        virtual void notesPerMeasureChanged(int newNotesPerMeasure) {}
    };

    void addListener(Listener *l);
    void removeListener(Listener *l);

  private:
    const int MIN_NOTE_NUMBER = 5;
    const int MIN_OCTAVE = -4;
    const int MAX_OCTAVE = 4;
    const int NOTES_PER_OCTAVE = 12;
    tracktion::AudioTrack::Ptr track;
    tracktion::MidiClip::Ptr midiClip;

    juce::ValueTree state;
    juce::ValueTree editState;
    app_models::StepSequence stepSequence;

    tracktion::TimePosition midiClipStart;
    tracktion::TimePosition midiClipEnd;

    tracktion::ConstrainedCachedValue<int> selectedNoteIndex;
    tracktion::ConstrainedCachedValue<int> numberOfNotes;

    juce::ListenerList<Listener> listeners;

    // Async update markers
    bool shouldUpdatePattern = false;
    bool shouldUpdateSelectedNoteIndex = false;
    bool shouldUpdateNumberOfNotes = false;
    bool shouldUpdateNotesPerMeasure = false;

    juce::CachedValue<int> notesPerMeasure;
    juce::Array<int> notesPerMeasureOptions = juce::Array<int>({4, 8, 16});

    void handleAsyncUpdate() override;
    void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                  const juce::Identifier &property) override;

    void generateMidiSequence();
    Sequencers::StepInput makeStepInput();
    std::array<Sequencers::StepTricks, 16> readTricks();
    void writeTricks(const std::array<Sequencers::StepTricks, 16> &tricks);
    void updateClipLength();
    void tricksChanged();

    // used for transport changes
    void playbackContextChanged() override {}
    void autoSaveNow() override {}
    void setAllLevelMetersActive(bool) override {}
    void setVideoPosition(tracktion::TimePosition timePosition,
                          bool forceJump) override;
    void startVideo() override {}
    void stopVideo() override {}
    int getZeroBasedOctave();
    static double floorToFraction(double number, double denominator = 1);
};

} // namespace app_view_models
