#include "SequencerClip.h"

namespace app_view_models {

SequencerClip::SequencerClip(tracktion::AudioTrack::Ptr t) : track(t) {
    auto &edit = track->edit;
    auto start = edit.getTransport().getPosition();
    startBeat = edit.tempoSequence.toBeats(start).inBeats();
    auto end = edit.tempoSequence.toTime(
        tracktion::BeatPosition::fromBeats(startBeat + lengthBeats));
    clip = dynamic_cast<tracktion::MidiClip *>(
        track->insertNewClip(tracktion::TrackItem::Type::midi, "sequence",
                             tracktion::TimeRange(start, end), nullptr));
    if (clip != nullptr)
        loopAroundClip(*clip);
}

SequencerClip::~SequencerClip() {
    stop();
    if (clip != nullptr && clip->getSequence().isEmpty()) {
        clip->removeFromParentTrack();
        track->edit.getTransport().looping.setValue(false, nullptr);
    }
}

void SequencerClip::write(const std::vector<Sequencers::NoteEvent> &notes,
                          double newLength) {
    if (clip == nullptr)
        return;
    lengthBeats = juce::jmax(0.25, newLength);
    auto &tempo = track->edit.tempoSequence;
    auto end = tempo.toTime(
        tracktion::BeatPosition::fromBeats(startBeat + lengthBeats));
    if (std::abs(clip->getPosition().getEnd().inSeconds() - end.inSeconds()) >
        1.0e-6) {
        clip->setEnd(end, true);
        loopAroundClip(*clip);
    }

    auto &sequence = clip->getSequence();
    sequence.clear(nullptr);
    for (auto &note : notes) {
        if (!std::isfinite(note.start) || !std::isfinite(note.length) ||
            note.start < 0.0 || note.start >= lengthBeats)
            continue;
        double length = juce::jmin(note.length, lengthBeats - note.start);
        sequence.addNote(
            juce::jlimit(0, 127, note.pitch),
            tracktion::BeatPosition::fromBeats(note.start),
            tracktion::BeatDuration::fromBeats(juce::jmax(0.01, length)),
            juce::jlimit(1, 127, note.velocity), 0, nullptr);
    }
}

void SequencerClip::play() {
    auto &edit = track->edit;
    auto &transport = edit.getTransport();
    if (transport.isPlaying() || clip == nullptr)
        return;
    clickWasEnabled = edit.clickTrackEnabled.get();
    edit.clickTrackEnabled.setValue(false, nullptr);
    loopAroundClip(*clip);
    transport.setPosition(clip->getPosition().getStart());
    soloed = true;
    track->setSolo(true);
    transport.play(false);
}

void SequencerClip::stop() {
    auto &edit = track->edit;
    auto &transport = edit.getTransport();
    if (transport.isPlaying())
        transport.stop(false, false);
    if (soloed) {
        track->setSolo(false);
        edit.clickTrackEnabled.setValue(clickWasEnabled, nullptr);
        soloed = false;
    }
}

bool SequencerClip::isPlaying() const {
    return track->edit.getTransport().isPlaying();
}

double SequencerClip::getPositionBeats() const {
    auto &edit = track->edit;
    double beat = edit.tempoSequence.toBeats(edit.getTransport().getPosition())
                      .inBeats() -
                  startBeat;
    double wrapped = std::fmod(beat, lengthBeats);
    return wrapped < 0.0 ? wrapped + lengthBeats : wrapped;
}

} // namespace app_view_models
