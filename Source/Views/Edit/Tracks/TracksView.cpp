#include "TracksView.h"
#include "PlayfulApp.h"
#include <app_navigation/app_navigation.h>

using namespace Playful;

namespace {
constexpr int maxLanes = 8;
constexpr float laneTop = 38.0f, lanePitch = 36.0f, laneHeight = 30.0f;
} // namespace

TracksView::TracksView(tracktion::Edit &e,
                       app_services::MidiCommandManager &mcm)
    : edit(e), midiCommandManager(mcm), camera(7), viewModel(e, camera) {
    midiCommandManager.addListener(this);
    // since this is the initial view we will manually set it to be the focused
    // component
    midiCommandManager.setFocusedComponent(this);

    viewModel.addListener(this);
    viewModel.listViewModel.addListener(this);
    viewModel.listViewModel.itemListState.addListener(this);
}

TracksView::~TracksView() {
    midiCommandManager.removeListener(this);
    viewModel.removeListener(this);
    viewModel.listViewModel.removeListener(this);
    viewModel.listViewModel.itemListState.removeListener(this);
}

//==============================================================================
bool TracksView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

bool TracksView::isZoomed() {
    return viewModel.getTracksViewType() ==
           app_view_models::TracksListViewModel::TracksViewType::SINGLE_TRACK;
}

int TracksView::getNumTracks() {
    return viewModel.listViewModel.getAdapter()->size();
}

tracktion::AudioTrack *TracksView::getTrack(int index) {
    if (!juce::isPositiveAndBelow(index, getNumTracks()))
        return nullptr;
    return dynamic_cast<tracktion::AudioTrack *>(
        viewModel.listViewModel.getAdapter()->getItemAtIndex(index));
}

tracktion::AudioTrack *TracksView::getSelectedTrack() {
    return dynamic_cast<tracktion::AudioTrack *>(
        viewModel.listViewModel.getSelectedItem());
}

juce::String TracksView::barBeat(tracktion::TimePosition t) {
    auto bb = edit.tempoSequence.toBarsAndBeats(t);
    return juce::String(bb.bars + 1) + "." +
           juce::String(bb.getWholeBeats() + 1);
}

bool TracksView::wantsSmoothFrames() {
    return edit.getTransport().isPlaying() || recording;
}

//==============================================================================
juce::String TracksView::getTitle() {
    if (isZoomed())
        if (auto track = getSelectedTrack())
            return track->getName();
    return "Tracks";
}

juce::String TracksView::getSubtitle() {
    auto &transport = edit.getTransport();
    auto position = transport.getPosition();
    int bpm = juce::roundToInt(
        edit.tempoSequence.getBeatsPerSecondAt(position) * 60.0);
    int octave =
        edit.state.getChildWithName(app_view_models::IDs::EDIT_VIEW_STATE)
            .getProperty(app_view_models::IDs::currentOctave, 0);
    juce::String s = "bar " + barBeat(position) + utf8("  ·  ") +
                     juce::String(bpm) + " bpm" + utf8("  ·  oct ") +
                     (octave >= 0 ? "+" : "") + juce::String(octave);
    if (looping)
        s += utf8("  ·  loop");
    // the track the mic is armed on (LMN-3 v3 MIC key)
    auto micTrack =
        edit.state.getProperty(juce::Identifier("micTrack")).toString();
    if (micTrack.isNotEmpty())
        for (int i = 0; i < getNumTracks(); i++)
            if (auto track = getTrack(i))
                if (track->itemID.toString() == micTrack)
                    s += utf8("  ·  mic → ") + juce::String(i + 1);
    return s;
}

juce::String TracksView::getPageLabel() {
    if (recording)
        return utf8("● rec");
    if (edit.getTransport().isPlaying())
        return utf8("▶ play");
    return isZoomed() ? "press 1: all tracks" : "press 1: zoom";
}

std::array<KnobInfo, 4> TracksView::getKnobs() {
    std::array<KnobInfo, 4> k;
    bool control = midiCommandManager.isControlDown;
    auto &transport = edit.getTransport();
    double length = juce::jmax(8.0, edit.getLength().inSeconds());
    int n = getNumTracks();
    int selected = viewModel.listViewModel.itemListState.getSelectedItemIndex();
    auto track = getSelectedTrack();

    k[0] = {utf8("TRACK · ZOOM"),
            track != nullptr ? track->getName().trimCharactersAtStart("Track ")
                             : juce::String("-"),
            n > 1 ? float(selected) / float(n - 1) : 1.0f, Colours::blue};

    auto loopPoint =
        control ? transport.loopPoint1.get() : transport.loopPoint2.get();
    k[1] = {control ? "LOOP IN" : "LOOP OUT",
            looping ? "bar " + barBeat(loopPoint) : juce::String("off"),
            float(juce::jlimit(0.0, 1.0, loopPoint.inSeconds() / length)),
            Colours::green};

    auto position = transport.getPosition();
    k[2] = {
        control ? juce::String("PLAYHEAD (BEATS)") : utf8("PLAYHEAD · SOLO"),
        edit.getTimecodeFormat().getString(edit.tempoSequence, position, false),
        float(juce::jlimit(0.0, 1.0, position.inSeconds() / length)),
        Colours::cream};

    auto colour = track != nullptr ? track->getColour() : Colours::pink;
    int colourIndex = trackPalette().indexOf(colour);
    k[3] = {utf8("COLOUR · MUTE"), colourName(colour),
            colourIndex >= 0 ? colourIndex / 7.0f : 0.0f, Colours::red};
    return k;
}

juce::String TracksView::getChangeSignature() {
    // also notice clip edits (quantize, cut, paste...) and mute / solo
    juce::String s = PlayfulScreen::getChangeSignature();
    for (int i = 0; i < getNumTracks(); i++) {
        if (auto track = getTrack(i)) {
            s << "|" << int(track->isMuted(false)) << int(track->isSolo(false))
              << track->getColour().toString();
            for (auto clip : track->getClips()) {
                s << ":" << clip->getPosition().getStart().inSeconds() << ","
                  << clip->getPosition().getEnd().inSeconds();
                if (auto midiClip = dynamic_cast<tracktion::MidiClip *>(clip))
                    s << "," << midiClip->getSequence().getNumNotes();
            }
        }
    }
    s << "|" << int(isZoomed());
    return s;
}

namespace {
// Peak levels of an audio file, worked out once per file (for drawing audio
// clips such as mic recordings)
const std::vector<float> &wavePeaks(tracktion::Engine &engine,
                                    const juce::File &file) {
    static std::map<juce::String, std::vector<float>> cache;
    auto key = file.getFullPathName() + "|" +
               juce::String(file.getLastModificationTime().toMilliseconds());
    auto found = cache.find(key);
    if (found != cache.end())
        return found->second;
    std::vector<float> peaks;
    std::unique_ptr<juce::AudioFormatReader> reader(
        engine.getAudioFileFormatManager().readFormatManager.createReaderFor(
            file));
    if (reader != nullptr && reader->lengthInSamples > 0) {
        const int bins = 400;
        juce::int64 perBin =
            std::max<juce::int64>(1, reader->lengthInSamples / bins);
        for (int b = 0; b < bins; b++) {
            juce::Range<float> levels[2];
            reader->readMaxLevels(b * perBin, perBin, levels,
                                  juce::jmin(2, int(reader->numChannels)));
            float peak = 0.0f;
            for (int c = 0; c < juce::jmin(2, int(reader->numChannels)); c++)
                peak =
                    juce::jmax(peak, levels[c].getEnd(), -levels[c].getStart());
            peaks.push_back(juce::jmin(1.0f, peak));
        }
        // seconds per bin, stored at the end
        peaks.push_back(float(perBin / reader->sampleRate));
    }
    return cache[key] = std::move(peaks);
}

void drawWaveClip(juce::Graphics &g, tracktion::WaveAudioClip &clip,
                  juce::Rectangle<float> r, juce::Colour colour, bool big) {
    auto &peaks = wavePeaks(clip.edit.engine, clip.getOriginalFile());
    if (peaks.size() < 2)
        return;
    int bins = int(peaks.size()) - 1;
    double binSeconds = peaks.back();
    double offset = clip.getPosition().getOffset().inSeconds();
    double length = clip.getPosition().getLength().inSeconds();
    auto area = r.reduced(big ? 10.0f : 3.0f, big ? 10.0f : 3.0f);
    float mid = area.getCentreY(), half = area.getHeight() / 2.0f;
    g.setColour(big ? colour : Colours::ink.withAlpha(0.55f));
    float step = big ? 3.0f : 2.0f;
    for (float x = area.getX(); x < area.getRight(); x += step) {
        double t = offset + length * (x - area.getX()) / area.getWidth();
        int bin = int(t / binSeconds);
        if (bin < 0 || bin >= bins)
            continue;
        float h = juce::jmax(1.0f, peaks[size_t(bin)] * half);
        g.fillRoundedRectangle(x, mid - h, step * 0.6f, 2 * h, step * 0.3f);
    }
}
} // namespace

//==============================================================================
float TracksView::timeToX(const Timeline &tl, double seconds) {
    return tl.x + float(camera.timeToX(seconds, tl.width));
}

bool TracksView::isRecordingInto(tracktion::AudioTrack &track) {
    if (!edit.getTransport().isRecording())
        return false;
    for (auto in : edit.getAllInputDevices())
        if (in->isRecordingActive() &&
            &track == in->getTargetTracks().getFirst())
            return true;
    return false;
}

void TracksView::paintBeats(juce::Graphics &g, const Timeline &tl, float top,
                            float bottom, bool numbers) {
    double start = camera.getCenter() - camera.getScope() / 2.0;
    double end = camera.getCenter() + camera.getScope() / 2.0;
    auto firstBeat =
        int(std::ceil(edit.tempoSequence
                          .toBeats(tracktion::TimePosition::fromSeconds(
                              juce::jmax(0.0, start)))
                          .inBeats()));
    auto lastBeat = int(std::floor(
        edit.tempoSequence.toBeats(tracktion::TimePosition::fromSeconds(end))
            .inBeats()));
    for (int beat = firstBeat; beat <= lastBeat; beat++) {
        auto t = edit.tempoSequence.toTime(
            tracktion::BeatPosition::fromBeats(double(beat)));
        float x = timeToX(tl, t.inSeconds());
        if (x < tl.x - 1.0f || x > tl.x + tl.width + 1.0f)
            continue;
        bool bar = beat % 4 == 0;
        g.setColour(bar ? Colours::muted.withAlpha(0.22f)
                        : Colours::dim.withAlpha(0.6f));
        g.fillRect(x - (bar ? 1.0f : 0.5f), top, bar ? 2.0f : 1.0f,
                   bottom - top);
        if (numbers && bar)
            drawText(g, juce::String(beat / 4 + 1), x + 3.0f, top - 15.0f,
                     30.0f, 14.0f, 12.0f, Colours::muted, false,
                     juce::Justification::centredLeft);
    }
}

void TracksView::paintClips(juce::Graphics &g, const Timeline &tl,
                            tracktion::AudioTrack &track, float top,
                            float height, bool big, double time) {
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(juce::Rectangle<float>(tl.x - 2.0f, top - 4.0f,
                                              tl.width + 4.0f, height + 8.0f)
                           .getSmallestIntegerContainer());

    auto colour = track.getColour();
    bool muted = track.isMuted(false);
    double viewStart = camera.getCenter() - camera.getScope() / 2.0;
    double viewEnd = camera.getCenter() + camera.getScope() / 2.0;
    auto &tempo = edit.tempoSequence;

    for (auto clip : track.getClips()) {
        auto pos = clip->getPosition();
        if (pos.getEnd().inSeconds() < viewStart ||
            pos.getStart().inSeconds() > viewEnd)
            continue;
        float x1 = timeToX(tl, pos.getStart().inSeconds());
        float x2 = timeToX(tl, pos.getEnd().inSeconds());
        juce::Rectangle<float> r(x1, top, juce::jmax(4.0f, x2 - x1), height);

        if (big) {
            g.setColour(Colours::panel);
            g.fillRoundedRectangle(r, 16.0f);
            g.setColour(colour.withAlpha(muted ? 0.35f : 1.0f));
            g.drawRoundedRectangle(r.reduced(1.5f), 16.0f, 3.0f);
        } else {
            g.setColour(colour.withAlpha(muted ? 0.35f : 0.9f));
            g.fillRoundedRectangle(r, juce::jmin(10.0f, r.getHeight() / 2.0f));
        }

        if (auto waveClip = dynamic_cast<tracktion::WaveAudioClip *>(clip)) {
            drawWaveClip(g, *waveClip, r, colour, big);
            continue;
        }
        auto midiClip = dynamic_cast<tracktion::MidiClip *>(clip);
        if (midiClip == nullptr || !midiClip->hasValidSequence())
            continue;
        auto &notes = midiClip->getSequence().getNotes();
        if (notes.isEmpty())
            continue;

        int lowest = 127, highest = 0;
        for (auto note : notes) {
            lowest = juce::jmin(lowest, note->getNoteNumber());
            highest = juce::jmax(highest, note->getNoteNumber());
        }
        lowest -= 1;
        highest += 1;
        float noteTop = r.getY() + (big ? 14.0f : 6.0f);
        float noteRange = r.getHeight() - (big ? 28.0f : 12.0f);

        for (auto note : notes) {
            double startBeat = midiClip->getStartBeat().inBeats() +
                               note->getStartBeat().inBeats() -
                               midiClip->getOffsetInBeats().inBeats();
            double endBeat = midiClip->getStartBeat().inBeats() +
                             note->getEndBeat().inBeats() -
                             midiClip->getOffsetInBeats().inBeats();
            double s =
                tempo.toTime(tracktion::BeatPosition::fromBeats(startBeat))
                    .inSeconds();
            double e = tempo.toTime(tracktion::BeatPosition::fromBeats(endBeat))
                           .inSeconds();
            if (e < viewStart || s > viewEnd)
                continue;
            s = juce::jmax(s, pos.getStart().inSeconds());
            e = juce::jmin(e, pos.getEnd().inSeconds());
            float nx1 = timeToX(tl, s), nx2 = timeToX(tl, e);
            float frac = 1.0f - float(note->getNoteNumber() - lowest) /
                                    float(juce::jmax(1, highest - lowest));
            float ny = noteTop + frac * noteRange;
            if (big) {
                float h = juce::jlimit(5.0f, 10.0f, noteRange / 14.0f);
                g.setColour(Colours::candy(note->getNoteNumber() % 6)
                                .withAlpha(0.35f + 0.65f * note->getVelocity() /
                                                       127.0f));
                g.fillRoundedRectangle(nx1 + 1.0f, ny - h / 2.0f,
                                       juce::jmax(h, nx2 - nx1 - 2.0f), h,
                                       h / 2.0f);
            } else {
                g.setColour(Colours::ink.withAlpha(0.55f));
                g.fillRoundedRectangle(nx1, ny - 1.6f,
                                       juce::jmax(3.2f, nx2 - nx1 - 1.0f), 3.2f,
                                       1.6f);
            }
        }
    }

    // the clip being recorded right now
    if (isRecordingInto(track)) {
        if (auto epc = edit.getTransport().getCurrentPlaybackContext()) {
            double t1 = edit.getTransport().getTimeWhenStarted().inSeconds();
            double t2 = juce::jmax(t1, epc->getUnloopedPosition().inSeconds());
            if (epc->isLooping()) {
                auto loopTimes = epc->getLoopTimes();
                if (t2 >= loopTimes.getEnd().inSeconds()) {
                    t1 = juce::jmin(t1, loopTimes.getStart().inSeconds());
                    t2 = loopTimes.getEnd().inSeconds();
                }
            }
            float x1 = timeToX(tl, t1), x2 = timeToX(tl, t2);
            float pulse = 0.55f + 0.25f * pingPong(time, 1.0);
            g.setColour(Colours::red.withAlpha(pulse));
            g.fillRoundedRectangle(x1, top, juce::jmax(4.0f, x2 - x1), height,
                                   juce::jmin(10.0f, height / 2.0f));
        }
    }
}

void TracksView::paintLoopAndPlayhead(juce::Graphics &g, const Timeline &tl,
                                      float flagY, float top, float bottom,
                                      double time) {
    auto &transport = edit.getTransport();
    auto inRange = [&](float x) {
        return x >= tl.x - 1.0f && x <= tl.x + tl.width + 1.0f;
    };

    if (looping) {
        float x1 = timeToX(tl, transport.loopPoint1->inSeconds());
        float x2 = timeToX(tl, transport.loopPoint2->inSeconds());
        float a = juce::jlimit(tl.x, tl.x + tl.width, x1);
        float b = juce::jlimit(tl.x, tl.x + tl.width, x2);
        if (b > a) {
            g.setColour(Colours::green.withAlpha(0.3f));
            g.fillRoundedRectangle(a, flagY + 6.0f, b - a, 12.0f, 6.0f);
        }
        if (inRange(x1))
            drawFlag(g, x1, flagY, true, Colours::green);
        if (inRange(x2))
            drawFlag(g, x2, flagY, false, Colours::green);
    }

    float x = timeToX(tl, transport.getCurrentPosition());
    if (inRange(x)) {
        drawDashedLine(g, {x, top, x, bottom}, Colours::cream, 2.5f, 3.0f,
                       6.0f);
        drawChick(g, x, top - 6.0f, 1.0f, time, transport.isPlaying());
    }
}

void TracksView::paintLanes(juce::Graphics &g, double time) {
    Timeline tl{92.0f, 644.0f};
    int n = getNumTracks();
    int selected = viewModel.listViewModel.itemListState.getSelectedItemIndex();
    int first = juce::jlimit(0, juce::jmax(0, n - maxLanes), selected - 3);
    float bottom = laneTop + juce::jmin(maxLanes, juce::jmax(1, n)) * lanePitch;

    paintBeats(g, tl, laneTop - 2.0f, bottom, false);

    for (int i = first; i < juce::jmin(n, first + maxLanes); i++) {
        auto track = getTrack(i);
        if (track == nullptr)
            continue;
        float y = laneTop + (i - first) * lanePitch;
        bool isSelected = i == selected;

        g.setColour(isSelected ? Colours::dim.withAlpha(0.9f)
                               : Colours::panel.withAlpha(0.85f));
        g.fillRoundedRectangle(58.0f, y, 726.0f, laneHeight, 12.0f);
        if (isSelected) {
            g.setColour(Colours::cream);
            g.drawRoundedRectangle(58.0f, y, 726.0f, laneHeight, 12.0f, 3.0f);
        }

        drawBadge(g, 34.0f, y + laneHeight / 2.0f, 13.0f, track->getColour(),
                  time);
        drawText(g, track->getName().trimCharactersAtStart("Track "), 62.0f, y,
                 26.0f, laneHeight, 13.0f, Colours::muted, true);

        paintClips(g, tl, *track, y + 4.0f, laneHeight - 8.0f, false, time);

        if (track->isMuted(false))
            drawPill(g, {742.0f, y + 6.0f, 34.0f, 18.0f}, "M", Colours::red,
                     Colours::ink, 11.0f);
        else if (track->isSolo(false))
            drawPill(g, {742.0f, y + 6.0f, 34.0f, 18.0f}, "S", Colours::yellow,
                     Colours::ink, 11.0f);
    }

    if (n > maxLanes) {
        // where we are in a long list of tracks
        for (int i = 0; i < n; i++) {
            bool shown = i >= first && i < first + maxLanes;
            g.setColour(shown ? Colours::muted : Colours::dim);
            fillEllipseCentred(
                g, 10.0f,
                laneTop + 8.0f + i * (bottom - laneTop - 16.0f) / float(n - 1),
                2.5f, 2.5f);
        }
    }

    paintLoopAndPlayhead(g, tl, 4.0f, 36.0f, bottom, time);
}

void TracksView::paintZoomed(juce::Graphics &g, double time) {
    Timeline tl{92.0f, 686.0f};
    auto track = getSelectedTrack();
    if (track == nullptr)
        return;

    paintBeats(g, tl, 40.0f, 320.0f, true);

    drawBadge(g, 44.0f, 150.0f, 22.0f, track->getColour(), time);
    drawText(g, track->getName().trimCharactersAtStart("Track "), 14.0f, 176.0f,
             60.0f, 24.0f, 16.0f, Colours::muted, true);
    if (track->isMuted(false))
        drawPill(g, {24.0f, 206.0f, 40.0f, 18.0f}, "M", Colours::red,
                 Colours::ink, 11.0f);
    else if (track->isSolo(false))
        drawPill(g, {24.0f, 206.0f, 40.0f, 18.0f}, "S", Colours::yellow,
                 Colours::ink, 11.0f);

    paintClips(g, tl, *track, 40.0f, 270.0f, true, time);
    paintLoopAndPlayhead(g, tl, 0.0f, 30.0f, 320.0f, time);
}

void TracksView::paintStage(juce::Graphics &g, double time) {
    if (isZoomed())
        paintZoomed(g, time);
    else
        paintLanes(g, time);
}

//==============================================================================
void TracksView::encoder1Increased() {
    if (hasFocus())
        viewModel.listViewModel.itemListState.setSelectedItemIndex(
            viewModel.listViewModel.itemListState.getSelectedItemIndex() + 1);
}

void TracksView::encoder1Decreased() {
    if (hasFocus())
        viewModel.listViewModel.itemListState.setSelectedItemIndex(
            viewModel.listViewModel.itemListState.getSelectedItemIndex() - 1);
}

void TracksView::encoder1ButtonReleased() {
    if (hasFocus()) {
        using Type = app_view_models::TracksListViewModel::TracksViewType;
        viewModel.setTracksViewType(isZoomed() ? Type::MULTI_TRACK
                                               : Type::SINGLE_TRACK);
        repaint();
    }
}

void TracksView::encoder2Increased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.nudgeLoopInForwardToNearestBeat();
        else
            viewModel.nudgeLoopOutForwardToNearestBeat();
    }
}

void TracksView::encoder2Decreased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.nudgeLoopInBackwardToNearestBeat();
        else
            viewModel.nudgeLoopOutBackwardToNearestBeat();
    }
}

void TracksView::encoder3Increased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.nudgeTransportForwardToNearestBeat();
        else
            viewModel.nudgeTransportForward();
    }
}

void TracksView::encoder3Decreased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.nudgeTransportBackwardToNearestBeat();
        else
            viewModel.nudgeTransportBackward();
    }
}

void TracksView::encoder3ButtonReleased() { viewModel.toggleSolo(); }

void TracksView::encoder4Increased() {
    viewModel.setSelectedTrackColour(
        nextTrackColour(viewModel.getSelectedTrackColour(), 1));
}

void TracksView::encoder4Decreased() {
    viewModel.setSelectedTrackColour(
        nextTrackColour(viewModel.getSelectedTrackColour(), -1));
}

void TracksView::encoder4ButtonReleased() { viewModel.toggleMute(); }

void TracksView::cutButtonReleased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.quantizeSelectedTracksClipAtPlayhead();
        else
            viewModel.cutSelectedTracksClipAtPlayHead();
    }
}

void TracksView::pasteButtonReleased() {
    if (hasFocus())
        viewModel.pasteClipboardContentToTrackAtPlayhead();
}

void TracksView::sliceButtonReleased() {
    if (hasFocus()) {
        if (midiCommandManager.isControlDown)
            viewModel.mergeSelectedTracksClipsAtPlayhead();
        else
            viewModel.splitSelectedTracksClipAtPlayHead();
    }
}

void TracksView::plusButtonReleased() {
    if (hasFocus()) {
        viewModel.addTrack();
        shouldUpdateTrackColour = true;
    }
}

void TracksView::minusButtonReleased() {
    if (hasFocus())
        viewModel.deleteSelectedTrack();
}

void TracksView::loopInButtonReleased() {
    if (hasFocus())
        viewModel.setLoopIn();
}

void TracksView::loopOutButtonReleased() {
    if (hasFocus())
        viewModel.setLoopOut();
}

void TracksView::loopButtonReleased() {
    if (hasFocus())
        viewModel.toggleLooping();
}

void TracksView::recordButtonReleased() {
    if (hasFocus())
        viewModel.startRecording();
}

void TracksView::playButtonReleased() {
    if (hasFocus())
        viewModel.startPlaying();
}

void TracksView::stopButtonReleased() {
    if (hasFocus())
        viewModel.stopRecordingOrPlaying();
}

void TracksView::undoButtonReleased() {
    if (hasFocus())
        viewModel.undo();
}

//==============================================================================
void TracksView::selectedIndexChanged(int) { repaint(); }

void TracksView::itemsChanged() {
    if (shouldUpdateTrackColour) {
        if (auto track = getSelectedTrack()) {
            const auto &palette = trackPalette();
            track->setColour(palette[juce::Random::getSystemRandom().nextInt(
                palette.size())]);
        }
        shouldUpdateTrackColour = false;
    }
    repaint();
}

void TracksView::isRecordingChanged(bool isRecording) {
    recording = isRecording;
    repaint();
}

void TracksView::isPlayingChanged(bool) { repaint(); }

void TracksView::tracksViewTypeChanged(
    app_view_models::TracksListViewModel::TracksViewType) {
    repaint();
}

void TracksView::loopingChanged(bool isLooping) {
    looping = isLooping;
    repaint();
}

void TracksView::soloStateChanged(bool) { repaint(); }

void TracksView::muteStateChanged(bool) { repaint(); }
