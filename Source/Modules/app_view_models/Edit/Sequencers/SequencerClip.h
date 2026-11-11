#pragma once

namespace app_view_models {

/**
 * The MIDI clip a sequencer writes into: made at the playhead when the
 * sequencer opens, looped, and filled with the sequencer's notes. Play solos
 * the track and plays the loop; when the sequencer closes an empty clip is
 * removed again. Like the step sequencer, this makes ordinary clips, so they
 * play any instrument and render with the song.
 */
class SequencerClip {
  public:
    explicit SequencerClip(tracktion::AudioTrack::Ptr t);
    ~SequencerClip();

    // replaces the clip's notes and sets its length (both in beats)
    void write(const std::vector<Sequencers::NoteEvent> &notes,
               double lengthBeats);

    void play();
    void stop();
    bool isPlaying() const;

    // where the playhead is inside the loop, in beats (wraps)
    double getPositionBeats() const;
    double getLengthBeats() const { return lengthBeats; }

    tracktion::AudioTrack &getTrack() { return *track; }
    tracktion::MidiClip *getClip() { return clip.get(); }

  private:
    tracktion::AudioTrack::Ptr track;
    tracktion::MidiClip::Ptr clip;
    double startBeat = 0.0;
    double lengthBeats = 4.0;
    bool clickWasEnabled = true;
    bool soloed = false;
};

} // namespace app_view_models
