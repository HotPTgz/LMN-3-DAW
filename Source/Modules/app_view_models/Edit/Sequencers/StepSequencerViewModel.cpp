namespace app_view_models {
// For playing clip only!!
// https://forum.juce.com/t/createeditforpreviewingclip-how-is-it-used/32757
StepSequencerViewModel::StepSequencerViewModel(tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(
                    IDs::STEP_SEQUENCER_STATE, nullptr)),
      editState(track->edit.state.getChildWithName(IDs::EDIT_VIEW_STATE)),
      stepSequence(state.getOrCreateChildWithName(
          app_models::IDs::STEP_SEQUENCE, nullptr)) {
    jassert(state.hasType(IDs::STEP_SEQUENCER_STATE));
    jassert(editState.hasType(IDs::EDIT_VIEW_STATE));

    state.addListener(this);
    // So we can listen for octave changes
    // Note that editState must be a member of this class for the listener to
    // work
    // https://forum.juce.com/t/adding-listener-to-valuetree-child-does-not-work/25229
    // An alternative would be to listen to the midi command manager octave
    // change, but I think I like letting the editTabBarView solely do the
    // listening for that, and listen to the state directly here
    editState.addListener(this);

    std::function<int(int)> numberOfNotesConstrainer = [this](int param) {
        // numberOfNotes cannot be less than 1
        // it also cannot be greater than the maximum number of notes allowed
        if (param <= 1)
            return 1;
        else if (param >= app_models::StepChannel::maxNumberOfNotes)
            return app_models::StepChannel::maxNumberOfNotes;
        else
            return param;
    };

    numberOfNotes.setConstrainer(numberOfNotesConstrainer);
    numberOfNotes.referTo(state, IDs::numberOfNotes, nullptr,
                          app_models::StepChannel::maxNumberOfNotes);

    std::function<int(int)> selectedNoteIndexConstrainer = [this](int param) {
        // selected index cannot be less than 0
        // it also cannot be greater than or equal to the number of notes
        if (param <= 0)
            return 0;
        else if (param >= numberOfNotes.get())
            return numberOfNotes.get() - 1;
        else
            return param;
    };

    selectedNoteIndex.setConstrainer(selectedNoteIndexConstrainer);
    selectedNoteIndex.referTo(state, IDs::selectedNoteIndex, nullptr, 0);

    notesPerMeasure.referTo(state, IDs::notesPerMeasure, nullptr, 4);

    double secondsPerBeat = 1.0 / track->edit.tempoSequence.getBeatsPerSecondAt(
                                      tracktion::TimePosition::fromSeconds(0));

    // Midi clip
    midiClipStart = track->edit.getTransport().getPosition();
    midiClipEnd = tracktion::TimePosition::fromSeconds(
        midiClipStart.inSeconds() +
        (numberOfNotes.get() * (4.0 / double(notesPerMeasure.get())) *
         secondsPerBeat));
    const tracktion::TimeRange midiClipTimeRange =
        tracktion::TimeRange(midiClipStart, midiClipEnd);
    midiClip = dynamic_cast<tracktion::MidiClip *>(track->insertNewClip(
        tracktion::TrackItem::Type::midi, "step", midiClipTimeRange, nullptr));

    // the clip may hold more than one pass (chance, random order...)
    updateClipLength();
    generateMidiSequence();

    loopAroundClip(*midiClip);

    track->edit.getTransport().addListener(this);
}

StepSequencerViewModel::~StepSequencerViewModel() {
    stop();

    generateMidiSequence();

    // if the sequence is empty, delete the clip
    // and disable looping
    if (midiClip->getSequence().isEmpty()) {
        midiClip->removeFromParentTrack();
        track->edit.getTransport().looping.setValue(false, nullptr);
    }

    state.removeListener(this);
    editState.removeListener(this);
    track->edit.state.getChildWithName(IDs::EDIT_VIEW_STATE)
        .removeListener(this);
    track->edit.getTransport().removeListener(this);
}

int StepSequencerViewModel::getNumChannels() {
    return app_models::StepChannel::maxNumberOfChannels;
}

int StepSequencerViewModel::getNumNotesPerChannel() {
    return app_models::StepChannel::maxNumberOfNotes;
}

bool StepSequencerViewModel::hasNoteAt(int channel, int noteIndex) {
    return stepSequence.getChannel(channel)->getNote(noteIndex);
}

void StepSequencerViewModel::toggleNoteNumberAtSelectedIndex(int noteNumber) {
    if (!track->edit.getTransport().isPlaying()) {
        int channel = noteNumberToChannel(noteNumber);
        stepSequence.getChannel(channel)->setNote(
            selectedNoteIndex.get(), !stepSequence.getChannel(channel)->getNote(
                                         selectedNoteIndex.get()));
        incrementSelectedNoteIndex();
    }
}

int StepSequencerViewModel::noteNumberToChannel(int noteNumber) {
    // Note numbers range from 5 to 124
    // Subtract the min note number from the note so that its zero based
    int zeroBasedNoteNumber = noteNumber - MIN_NOTE_NUMBER;
    return zeroBasedNoteNumber - (NOTES_PER_OCTAVE * getZeroBasedOctave());
}

int StepSequencerViewModel::getSelectedNoteIndex() {
    return selectedNoteIndex.get();
}

int StepSequencerViewModel::getNumberOfNotes() { return numberOfNotes.get(); }

void StepSequencerViewModel::incrementSelectedNoteIndex() {
    if (!track->edit.getTransport().isPlaying())
        selectedNoteIndex.setValue(selectedNoteIndex.get() + 1, nullptr);
}

void StepSequencerViewModel::decrementSelectedNoteIndex() {
    if (!track->edit.getTransport().isPlaying())
        selectedNoteIndex.setValue(selectedNoteIndex.get() - 1, nullptr);
}

void StepSequencerViewModel::incrementNumberOfNotes() {
    if (!track->edit.getTransport().isPlaying())
        numberOfNotes.setValue(numberOfNotes.get() + 1, nullptr);
}

void StepSequencerViewModel::decrementNumberOfNotes() {
    if (!track->edit.getTransport().isPlaying())
        numberOfNotes.setValue(numberOfNotes.get() - 1, nullptr);
}

int StepSequencerViewModel::getNotesPerMeasure() {
    return notesPerMeasure.get();
}

void StepSequencerViewModel::incrementNotesPerMeasure() {
    if (!track->edit.getTransport().isPlaying()) {
        int currentIndex =
            notesPerMeasureOptions.indexOf(notesPerMeasure.get());

        if (currentIndex != -1) {
            int newIndex;
            if (currentIndex == notesPerMeasureOptions.size() - 1)
                newIndex = 0;
            else
                newIndex = currentIndex + 1;

            notesPerMeasure.setValue(notesPerMeasureOptions[newIndex], nullptr);
        }
    }
}

void StepSequencerViewModel::decrementNotesPerMeasure() {
    if (!track->edit.getTransport().isPlaying()) {
        int currentIndex =
            notesPerMeasureOptions.indexOf(notesPerMeasure.get());

        if (currentIndex != -1) {
            int newIndex;
            if (currentIndex == 0)
                newIndex = notesPerMeasureOptions.size() - 1;
            else
                newIndex = currentIndex - 1;

            notesPerMeasure.setValue(notesPerMeasureOptions[newIndex], nullptr);
        }
    }
}

void StepSequencerViewModel::clearNotesAtSelectedIndex() {
    if (!track->edit.getTransport().isPlaying())
        for (int i = 0; i < app_models::StepChannel::maxNumberOfChannels; i++)
            stepSequence.getChannel(i)->setNote(selectedNoteIndex, false);
}

void StepSequencerViewModel::play() {
    // In the future maybe only the midi clip currently generated by the
    // sequencer should be played not the whole edit. Might be challenging
    // though. https://forum.juce.com/t/how-to-render-a-midiclip-any-hints/31004
    // https://forum.juce.com/t/createeditforpreviewingclip-how-is-it-used/32757/11
    if (!track->edit.getTransport().isPlaying()) {
        generateMidiSequence();
        track->edit.clickTrackEnabled.setValue(false, nullptr);
        track->edit.getTransport().setCurrentPosition(
            midiClipStart.inSeconds());
        track->setSolo(true);
        track->edit.getTransport().play(false);
    }
}

void StepSequencerViewModel::stop() {
    if (track->edit.getTransport().isPlaying()) {
        track->edit.clickTrackEnabled.setValue(true, nullptr);
        track->setSolo(false);
        track->edit.getTransport().stop(false, false);
    }
}

void StepSequencerViewModel::handleAsyncUpdate() {
    if (compareAndReset(shouldUpdatePattern)) {
        listeners.call([this](Listener &l) { l.patternChanged(); });
    }

    if (compareAndReset(shouldUpdateSelectedNoteIndex))
        listeners.call([this](Listener &l) {
            l.selectedNoteIndexChanged(selectedNoteIndex.get());
        });

    if (compareAndReset(shouldUpdateNumberOfNotes)) {
        listeners.call([this](Listener &l) {
            l.numberOfNotesChanged(numberOfNotes.get());
        });

        if (selectedNoteIndex.get() >= numberOfNotes) {
            selectedNoteIndex.setValue(numberOfNotes.get() - 1, nullptr);
        }
    }

    if (compareAndReset(shouldUpdateNotesPerMeasure))
        listeners.call([this](Listener &l) {
            l.notesPerMeasureChanged(notesPerMeasure.get());
        });
}

void StepSequencerViewModel::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
    if (treeWhosePropertyHasChanged.hasType(app_models::IDs::STEP_CHANNEL))
        if (property == app_models::IDs::stepPattern)
            markAndUpdate(shouldUpdatePattern);

    if (treeWhosePropertyHasChanged == state) {
        if (property == IDs::selectedNoteIndex)
            markAndUpdate(shouldUpdateSelectedNoteIndex);

        if (property == IDs::numberOfNotes) {
            updateClipLength();

            markAndUpdate(shouldUpdateNumberOfNotes);
        }

        if (property == IDs::notesPerMeasure) {
            updateClipLength();

            markAndUpdate(shouldUpdateNotesPerMeasure);
        }
    }

    if (treeWhosePropertyHasChanged.hasType(IDs::EDIT_VIEW_STATE)) {
        if (property == IDs::currentOctave) {
            // Might need to detect changes in here at some point, for now do
            // nothing
        }
    }
}

void StepSequencerViewModel::addListener(Listener *l) {
    listeners.add(l);
    l->patternChanged();
    l->selectedNoteIndexChanged(selectedNoteIndex.get());
    l->numberOfNotesChanged(numberOfNotes.get());
    l->notesPerMeasureChanged(notesPerMeasure.get());
}

void StepSequencerViewModel::removeListener(Listener *l) {
    listeners.remove(l);
}

Sequencers::StepInput StepSequencerViewModel::makeStepInput() {
    Sequencers::StepInput input;
    for (int i = 0; i < getNumChannels(); i++)
        for (int j = 0; j < getNumNotesPerChannel(); j++)
            if (hasNoteAt(i, j)) {
                // Need to get the pitch based on the sequence position and
                // current octave remember that we need to add the min note
                // number to get things correct since the min note number
                // possible is not 0, its 5
                int pitch = i + (NOTES_PER_OCTAVE * getZeroBasedOctave()) +
                            MIN_NOTE_NUMBER;
                input.notes[size_t(j)].add(pitch);
            }
    input.tricks = readTricks();
    input.length = numberOfNotes.get();
    input.stepsPerBar = notesPerMeasure.get();
    input.groove = getGroove();
    return input;
}

void StepSequencerViewModel::generateMidiSequence() {
    auto &sequence = midiClip->getSequence();
    sequence.clear(nullptr);
    for (auto &note : Sequencers::generateStep(makeStepInput()))
        sequence.addNote(note.pitch,
                         tracktion::BeatPosition::fromBeats(note.start),
                         tracktion::BeatDuration::fromBeats(note.length),
                         note.velocity, 1, nullptr);
}

void StepSequencerViewModel::updateClipLength() {
    double secondsPerBeat =
        1.0 / track->edit.tempoSequence.getBeatsPerSecondAt(
                  tracktion::TimePosition::fromSeconds(0.0));
    midiClipEnd = tracktion::TimePosition::fromSeconds(
        midiClipStart.inSeconds() +
        (numberOfNotes.get() * (4.0 / double(notesPerMeasure.get())) *
         secondsPerBeat * getPasses()));
    if (midiClip != nullptr) {
        midiClip->setEnd(midiClipEnd, true);
        loopAroundClip(*midiClip);
    }
}

int StepSequencerViewModel::getPasses() {
    return Sequencers::stepPasses(makeStepInput());
}

bool StepSequencerViewModel::isPlaying() {
    return track->edit.getTransport().isPlaying();
}

//==============================================================================
namespace {
const juce::Identifier stepTricksId("stepTricks"), stepSwingId("swing"),
    stepGateId("gate"), stepDirectionId("direction"),
    stepHumanizeId("humanize");
} // namespace

std::array<Sequencers::StepTricks, 16> StepSequencerViewModel::readTricks() {
    std::array<Sequencers::StepTricks, 16> tricks;
    auto steps =
        juce::StringArray::fromTokens(state[stepTricksId].toString(), ";", "");
    for (int i = 0; i < juce::jmin(16, steps.size()); i++) {
        auto parts = juce::StringArray::fromTokens(steps[i], ",", "");
        if (parts.size() < 4)
            continue;
        auto &t = tricks[size_t(i)];
        t.velocity = juce::jlimit(1, 127, parts[0].getIntValue());
        t.length = juce::jlimit(1, 16, parts[1].getIntValue());
        t.chance = juce::jlimit(0.0f, 1.0f, parts[2].getFloatValue());
        t.ratchet = juce::jlimit(1, 4, parts[3].getIntValue());
    }
    return tricks;
}

void StepSequencerViewModel::writeTricks(
    const std::array<Sequencers::StepTricks, 16> &tricks) {
    juce::StringArray steps;
    for (auto &t : tricks)
        steps.add(juce::String(t.velocity) + "," + juce::String(t.length) +
                  "," + juce::String(t.chance, 2) + "," +
                  juce::String(t.ratchet));
    state.setProperty(stepTricksId, steps.joinIntoString(";"), nullptr);
}

void StepSequencerViewModel::tricksChanged() {
    updateClipLength();
    generateMidiSequence();
    markAndUpdate(shouldUpdatePattern);
}

Sequencers::StepTricks StepSequencerViewModel::getTricks(int step) {
    return readTricks()[size_t(juce::jlimit(0, 15, step))];
}

namespace {
template <typename F>
void stepChangeTricks(std::array<Sequencers::StepTricks, 16> &tricks,
                      int selected, bool all, F change) {
    for (int i = 0; i < 16; i++)
        if (all || i == selected)
            change(tricks[size_t(i)]);
}
} // namespace

void StepSequencerViewModel::nudgeVelocity(int delta, bool all) {
    auto tricks = readTricks();
    stepChangeTricks(tricks, selectedNoteIndex.get(), all, [delta](auto &t) {
        t.velocity = juce::jlimit(1, 127, t.velocity + delta * 4);
    });
    writeTricks(tricks);
    tricksChanged();
}

void StepSequencerViewModel::nudgeTie(int delta, bool all) {
    auto tricks = readTricks();
    stepChangeTricks(tricks, selectedNoteIndex.get(), all, [delta](auto &t) {
        t.length = juce::jlimit(1, 16, t.length + delta);
    });
    writeTricks(tricks);
    tricksChanged();
}

void StepSequencerViewModel::nudgeChance(int delta, bool all) {
    auto tricks = readTricks();
    stepChangeTricks(tricks, selectedNoteIndex.get(), all, [delta](auto &t) {
        t.chance = juce::jlimit(0.0f, 1.0f, t.chance + delta * 0.05f);
    });
    writeTricks(tricks);
    tricksChanged();
}

void StepSequencerViewModel::nudgeRatchet(int delta, bool all) {
    auto tricks = readTricks();
    stepChangeTricks(tricks, selectedNoteIndex.get(), all, [delta](auto &t) {
        t.ratchet = juce::jlimit(1, 4, t.ratchet + delta);
    });
    writeTricks(tricks);
    tricksChanged();
}

Sequencers::StepGroove StepSequencerViewModel::getGroove() {
    Sequencers::StepGroove groove;
    groove.swing = double(state.getProperty(stepSwingId, 0.0));
    groove.gate = double(state.getProperty(stepGateId, 0.9));
    groove.direction = Sequencers::Direction(
        juce::jlimit(0, 3, int(state.getProperty(stepDirectionId, 0))));
    groove.humanize = double(state.getProperty(stepHumanizeId, 0.0));
    return groove;
}

void StepSequencerViewModel::nudgeSwing(int delta) {
    state.setProperty(stepSwingId,
                      juce::jlimit(0.0, 1.0, getGroove().swing + delta * 0.05),
                      nullptr);
    tricksChanged();
}

void StepSequencerViewModel::nudgeGate(int delta) {
    state.setProperty(stepGateId,
                      juce::jlimit(0.1, 1.0, getGroove().gate + delta * 0.05),
                      nullptr);
    tricksChanged();
}

void StepSequencerViewModel::nudgeDirection(int delta) {
    state.setProperty(stepDirectionId,
                      juce::jlimit(0, 3, int(getGroove().direction) + delta),
                      nullptr);
    tricksChanged();
}

void StepSequencerViewModel::nudgeHumanize(int delta) {
    state.setProperty(
        stepHumanizeId,
        juce::jlimit(0.0, 1.0, getGroove().humanize + delta * 0.05), nullptr);
    tricksChanged();
}

void StepSequencerViewModel::setVideoPosition(
    tracktion::TimePosition timePosition, bool forceJump) {
    // find beat of current time relative to the start of the midi clip
    // round it down to nearest whole beat
    // then account for the notes per measure
    auto time = timePosition.inSeconds();
    auto beats = track->edit.tempoSequence.toBeats(
        tracktion::TimePosition::fromSeconds(time - midiClipStart.inSeconds()));
    double beatTime =
        floorToFraction(beats.inBeats(), double(notesPerMeasure.get()) / 4.0);
    int note = (beatTime * notesPerMeasure.get()) / 4.0;
    // the clip can hold several passes; show the step that is playing
    int length = juce::jmax(1, numberOfNotes.get());
    int pass = juce::jmax(0, note) / length,
        slot = juce::jmax(0, note) % length;
    auto direction = getGroove().direction;
    if (direction == Sequencers::Direction::backward ||
        (direction == Sequencers::Direction::pingPong && pass % 2 == 1))
        slot = length - 1 - slot;
    selectedNoteIndex.setValue(slot, nullptr);
}

double StepSequencerViewModel::floorToFraction(double number,
                                               double denominator) {
    // https://stackoverflow.com/questions/14903379/rounding-to-nearest-fraction-half-quarter-etc
    double x = number * denominator;
    x = floor(x);
    x = x / denominator;
    return x;
}

int StepSequencerViewModel::getZeroBasedOctave() {
    int currentOctave =
        editState.getProperty(app_view_models::IDs::currentOctave);
    int zeroBasedOctave = currentOctave;
    if (MIN_OCTAVE < 0) {
        zeroBasedOctave = currentOctave + abs(MIN_OCTAVE);
    }

    return zeroBasedOctave;
}

} // namespace app_view_models
