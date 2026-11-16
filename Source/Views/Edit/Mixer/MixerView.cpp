#include "MixerView.h"
#include "PlayfulApp.h"

using namespace Playful;
namespace AuxSends = app_view_models::AuxSends;
namespace EngineHelpers = app_view_models::EngineHelpers;

namespace {
constexpr float jarPitch = 86.0f, jarWidth = 62.0f, jarTop = 30.0f,
                jarHeight = 226.0f;

juce::String panText(float pan) {
    int amount = juce::roundToInt(std::abs(pan) * 100.0f);
    if (amount == 0)
        return "C";
    return (pan < 0.0f ? "L " : "R ") + juce::String(amount);
}

juce::String shortName(tracktion::Track &track) {
    if (track.isMasterTrack())
        return "Master";
    return track.getName().trimCharactersAtStart("Track ");
}
} // namespace

MixerView::Meter::~Meter() {
    if (measurer != nullptr)
        measurer->removeClient(client);
}

MixerView::MixerView(tracktion::Edit &e, app_services::MidiCommandManager &mcm)
    : edit(e), midiCommandManager(mcm), viewModel(edit) {
    viewModel.listViewModel.addListener(this);
    viewModel.listViewModel.itemListState.addListener(this);
    midiCommandManager.addListener(this);
    buildMeters();
}

MixerView::~MixerView() {
    midiCommandManager.removeListener(this);
    viewModel.listViewModel.removeListener(this);
    viewModel.listViewModel.itemListState.removeListener(this);
}

bool MixerView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

int MixerView::getNumTracks() {
    return viewModel.listViewModel.getAdapter()->size();
}

tracktion::Track *MixerView::getTrack(int index) {
    if (!juce::isPositiveAndBelow(index, getNumTracks()))
        return nullptr;
    return dynamic_cast<tracktion::Track *>(
        viewModel.listViewModel.getAdapter()->getItemAtIndex(index));
}

tracktion::Track *MixerView::getSelectedTrack() {
    return dynamic_cast<tracktion::Track *>(
        viewModel.listViewModel.getSelectedItem());
}

void MixerView::buildMeters() {
    meters.clear();
    for (int i = 0; i < getNumTracks(); i++) {
        auto track = getTrack(i);
        if (track == nullptr)
            continue;
        auto meter = std::make_unique<Meter>();
        meter->track = track;
        if (track->isMasterTrack()) {
            if (auto epc = edit.getCurrentPlaybackContext())
                meter->measurer = &epc->masterLevels;
        } else if (auto audioTrack =
                       dynamic_cast<tracktion::AudioTrack *>(track)) {
            if (auto levelMeter = audioTrack->getLevelMeterPlugin()) {
                meter->plugin = levelMeter;
                meter->measurer = &levelMeter->measurer;
            }
        }
        if (meter->measurer != nullptr)
            meter->measurer->addClient(meter->client);
        meters.push_back(std::move(meter));
    }
}

MixerView::Meter *MixerView::meterFor(tracktion::Track *track) {
    for (auto &meter : meters)
        if (meter->track == track)
            return meter.get();
    return nullptr;
}

bool MixerView::wantsSmoothFrames() { return edit.getTransport().isPlaying(); }

//==============================================================================
juce::String MixerView::getSubtitle() {
    int audioTracks = juce::jmax(0, getNumTracks() - 1);
    return juce::String(audioTracks) +
           (audioTracks == 1 ? " track" : " tracks") + " + master";
}

juce::String MixerView::getPageLabel() { return "dots: sends A, B"; }

std::array<KnobInfo, 4> MixerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    auto track = getSelectedTrack();
    int n = getNumTracks();
    int selected = viewModel.listViewModel.itemListState.getSelectedItemIndex();
    k[0] = {"TRACK", track != nullptr ? shortName(*track) : juce::String("-"),
            n > 1 ? float(selected) / float(n - 1) : 1.0f, Colours::blue};

    int bus = midiCommandManager.isControlDown ? 1 : 0;
    bool canSend = track != nullptr && !track->isMasterTrack() &&
                   !AuxSends::isReturnTrack(*track);
    float send = canSend ? AuxSends::getSendLevel(*track, bus) : 0.0f;
    k[1] = {bus == 0 ? "SEND A" : "SEND B",
            juce::String(juce::roundToInt(send * 100.0f)) + "%", send,
            Colours::green, canSend};

    auto plugin = track != nullptr
                      ? EngineHelpers::getVolumeAndPanPluginForTrack(track)
                      : nullptr;
    float pan = plugin != nullptr ? plugin->getPan() : 0.0f;
    k[2] = {utf8("PAN · SOLO"), panText(pan), (pan + 1.0f) / 2.0f,
            Colours::cream, plugin != nullptr};

    float db = plugin != nullptr ? plugin->getVolumeDb() : 0.0f;
    float volume = plugin != nullptr
                       ? plugin->volParam->getCurrentNormalisedValue()
                       : 0.0f;
    k[3] = {utf8("VOLUME · MUTE"),
            db <= -99.0f ? utf8("−∞ dB") : juce::String(db, 1) + " dB", volume,
            Colours::red, plugin != nullptr};
    return k;
}

juce::String MixerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (int i = 0; i < getNumTracks(); i++)
        if (auto track = getTrack(i)) {
            if (auto plugin =
                    EngineHelpers::getVolumeAndPanPluginForTrack(track))
                s << "|" << plugin->getVolumeDb() << "," << plugin->getPan();
            s << int(track->isMuted(false)) << int(track->isSolo(false));
            if (!track->isMasterTrack())
                s << AuxSends::getSendLevel(*track, 0)
                  << AuxSends::getSendLevel(*track, 1);
        }
    return s;
}

//==============================================================================
void MixerView::paintStage(juce::Graphics &g, double time) {
    // 9 jars on the 800 stage, more on a wide screen
    const int maxJars =
        juce::jlimit(1, 12, int((stageWidth() - 40.0f) / jarPitch));
    int n = getNumTracks();
    int selected = viewModel.listViewModel.itemListState.getSelectedItemIndex();
    int first = juce::jlimit(0, juce::jmax(0, n - maxJars), selected - 4);

    for (int i = first; i < juce::jmin(n, first + maxJars); i++)
        if (auto track = getTrack(i))
            paintJar(g, *track,
                     24.0f + (stageWidth() - designWidth) / 2.0f +
                         (i - first) * jarPitch,
                     i == selected, time);

    if (first > 0)
        drawText(g, utf8("‹"), 2, 110, 20, 40, 28, Colours::muted, true);
    if (first + maxJars < n)
        drawText(g, utf8("›"), stageWidth() - 20.0f, 110, 20, 40, 28,
                 Colours::muted, true);
}

void MixerView::paintJar(juce::Graphics &g, tracktion::Track &track, float x,
                         bool selected, double time) {
    float cx = x + jarPitch / 2.0f - 12.0f;
    float left = cx - jarWidth / 2.0f;
    bool master = track.isMasterTrack();
    auto colour = master ? Colours::cream : track.getColour();
    auto plugin = EngineHelpers::getVolumeAndPanPluginForTrack(&track);
    bool muted = track.isMuted(false);

    // name
    drawText(g, shortName(track), cx - 42.0f, 2.0f, 84.0f, 18.0f,
             selected ? 14.0f : 13.0f,
             selected ? Colours::text : Colours::muted, true);

    // lid and jar
    g.setColour(Colours::muted);
    g.fillRoundedRectangle(cx - 34.0f, 20.0f, 68.0f, 12.0f, 6.0f);
    juce::Rectangle<float> body(left, jarTop, jarWidth, jarHeight);
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(body, 20.0f);

    // jelly
    float volume = plugin != nullptr
                       ? plugin->volParam->getCurrentNormalisedValue()
                       : 0.0f;
    float jellyTop = body.getBottom() - 6.0f - volume * (jarHeight - 16.0f);
    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path jar;
        jar.addRoundedRectangle(body.reduced(4.0f), 17.0f);
        g.reduceClipRegion(jar);

        float wobble = 3.0f * std::sin(float(time) * 3.0f + cx * 0.05f);
        juce::Path jelly;
        jelly.startNewSubPath(left, body.getBottom());
        jelly.lineTo(left, jellyTop + wobble);
        jelly.quadraticTo(cx, jellyTop - wobble, left + jarWidth,
                          jellyTop + wobble);
        jelly.lineTo(left + jarWidth, body.getBottom());
        jelly.closeSubPath();
        g.setColour(colour.withAlpha(muted ? 0.3f : 0.9f));
        g.fillPath(jelly);

        // bubbles
        for (int b = 0; b < 3; b++) {
            float t = loop(time + b * 0.9 + cx * 0.01, 2.7);
            float by =
                body.getBottom() - 10.0f -
                t * juce::jmax(0.0f, body.getBottom() - jellyTop - 16.0f);
            g.setColour(Colours::white.withAlpha(0.35f * (1.0f - t)));
            fillEllipseCentred(g, left + 16.0f + b * 12.0f, by, 3.0f, 3.0f);
        }

        // sends as dots at the bottom
        if (!master && !app_view_models::AuxSends::isReturnTrack(track)) {
            float a = AuxSends::getSendLevel(track, 0);
            float b = AuxSends::getSendLevel(track, 1);
            if (a > 0.0f) {
                g.setColour(Colours::blue);
                fillEllipseCentred(g, cx - 10.0f, body.getBottom() - 16.0f,
                                   3.0f + 5.0f * a, 3.0f + 5.0f * a);
                g.setColour(Colours::ink);
                fillEllipseCentred(g, cx - 10.0f, body.getBottom() - 16.0f,
                                   1.3f, 1.3f);
            }
            if (b > 0.0f) {
                g.setColour(Colours::pink);
                fillEllipseCentred(g, cx + 10.0f, body.getBottom() - 16.0f,
                                   3.0f + 5.0f * b, 3.0f + 5.0f * b);
                g.setColour(Colours::ink);
                fillEllipseCentred(g, cx + 10.0f, body.getBottom() - 16.0f,
                                   1.3f, 1.3f);
            }
        }
    }

    // live level, two thin bars inside the right edge
    if (auto meter = meterFor(&track)) {
        for (int ch = 0; ch < 2; ch++) {
            float db = meter->client.getAndClearAudioLevel(ch).dB;
            if (db < meter->levelDb[ch])
                db = juce::jmax(db, meter->levelDb[ch] - 1.5f);
            meter->levelDb[ch] = db;
            float h = juce::jlimit(0.0f, 1.0f, (db + 40.0f) / 43.0f) *
                      (jarHeight - 24.0f);
            if (h <= 0.5f)
                continue;
            g.setColour(db > 0.0f    ? Colours::red
                        : db > -6.0f ? Colours::yellow
                                     : Colours::green);
            g.fillRoundedRectangle(left + jarWidth - 13.0f + ch * 5.0f,
                                   body.getBottom() - 12.0f - h, 3.0f, h, 1.5f);
        }
    }

    if (selected) {
        g.setColour(Colours::cream);
        g.drawRoundedRectangle(body, 20.0f, 3.0f);
    }

    if (muted)
        drawPill(g, {cx - 17.0f, jarTop + 10.0f, 34.0f, 16.0f}, "M",
                 Colours::red, Colours::ink, 11.0f);
    else if (track.isSolo(false))
        drawPill(g, {cx - 17.0f, jarTop + 10.0f, 34.0f, 16.0f}, "S",
                 Colours::yellow, Colours::ink, 11.0f);

    // the little face leans with the pan
    float pan = plugin != nullptr ? plugin->getPan() : 0.0f;
    drawBadge(g, cx + pan * 16.0f, jarTop + jarHeight + 22.0f, 13.0f, colour,
              time);
}

//==============================================================================
void MixerView::encoder1Increased() {
    if (hasFocus())
        viewModel.listViewModel.itemListState.setSelectedItemIndex(
            viewModel.listViewModel.itemListState.getSelectedItemIndex() + 1);
}

void MixerView::encoder1Decreased() {
    if (hasFocus())
        viewModel.listViewModel.itemListState.setSelectedItemIndex(
            viewModel.listViewModel.itemListState.getSelectedItemIndex() - 1);
}

void MixerView::encoder1ButtonReleased() {}

void MixerView::encoder2Increased() {
    viewModel.incrementSend(midiCommandManager.isControlDown ? 1 : 0);
}

void MixerView::encoder2Decreased() {
    viewModel.decrementSend(midiCommandManager.isControlDown ? 1 : 0);
}

void MixerView::encoder3Increased() { viewModel.incrementPan(); }

void MixerView::encoder3Decreased() { viewModel.decrementPan(); }

void MixerView::encoder4Increased() { viewModel.incrementVolume(); }

void MixerView::encoder4Decreased() { viewModel.decrementVolume(); }

void MixerView::encoder3ButtonReleased() { viewModel.toggleSolo(); }

void MixerView::encoder4ButtonReleased() { viewModel.toggleMute(); }

void MixerView::selectedIndexChanged(int) { repaint(); }

void MixerView::itemsChanged() {
    buildMeters();
    repaint();
}
