#include "TempoSettingsView.h"
#include "PlayfulApp.h"

using namespace Playful;

TempoSettingsView::TempoSettingsView(tracktion::Edit &e,
                                     app_services::MidiCommandManager &mcm)
    : edit(e), viewModel(edit), midiCommandManager(mcm) {
    midiCommandManager.addListener(this);
    // the listener callbacks fill in the current values
    viewModel.addListener(this);
}

TempoSettingsView::~TempoSettingsView() {
    midiCommandManager.removeListener(this);
    viewModel.removeListener(this);
}

bool TempoSettingsView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

bool TempoSettingsView::wantsSmoothFrames() {
    // keep the metronome swinging in time with the song
    return edit.getTransport().isPlaying();
}

juce::String TempoSettingsView::getPageLabel() {
    return tapMode ? "tap the tempo button!" : "tempo button: tap";
}

std::array<KnobInfo, 4> TempoSettingsView::getKnobs() {
    std::array<KnobInfo, 4> k;
    float bpmNorm = float((bpm - viewModel.bpmLowerLimit) /
                          (viewModel.bpmUpperLimit - viewModel.bpmLowerLimit));
    k[0] = {"BPM", juce::String(juce::roundToInt(bpm)), bpmNorm, Colours::blue};
    static const juce::StringArray grids{"1/4",  "1/8",   "1/8T",
                                         "1/16", "1/16T", "1/32"};
    k[1] = {"QUANTIZE", gridName,
            float(juce::jmax(0, grids.indexOf(gridName))) / 5.0f,
            Colours::green};
    k[2] = {"SWING", juce::String(swing) + "%", swing / 100.0f, Colours::cream};
    float gainNorm = float((clickGain - viewModel.clickTrackGainLowerLimit) /
                           (viewModel.clickTrackGainUpperLimit -
                            viewModel.clickTrackGainLowerLimit));
    k[3] = {"CLICK", juce::String(juce::roundToInt(gainNorm * 100.0f)) + "%",
            gainNorm, Colours::red};
    return k;
}

void TempoSettingsView::paintStage(juce::Graphics &g, double time) {
    // where the stick is: follows the song while playing, else the clock
    auto &transport = edit.getTransport();
    double beats =
        transport.isPlaying()
            ? edit.tempoSequence.toBeats(transport.getPosition()).inBeats()
            : (animationsEnabled() ? time * bpm / 60.0 : 0.5);
    float angle = juce::degreesToRadians(
        20.0f * float(std::sin(beats * juce::MathConstants<double>::pi)));

    // metronome, hinged at its pivot near the bottom
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(150.0f, 290.0f));
        juce::Path body;
        body.startNewSubPath(-70, 0);
        body.lineTo(-34, -230);
        body.lineTo(34, -230);
        body.lineTo(70, 0);
        body.closeSubPath();
        g.setColour(Colours::orange);
        g.fillPath(body, juce::AffineTransform());
        g.setColour(Colours::red);
        g.fillRoundedRectangle(-80, -4, 160, 22, 10);
        drawFace(g, 0, -38, 1.1f, Mood::smile, time);

        juce::Graphics::ScopedSaveState pendulum(g);
        g.addTransform(juce::AffineTransform::rotation(angle, 0.0f, -80.0f));
        g.setColour(Colours::ink);
        g.drawLine(0, -80, 0, -214, 6.0f);
        g.setColour(Colours::cream);
        g.fillRoundedRectangle(-14, -170, 28, 22, 8);
    }
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(150.0f, 290.0f));
        g.setColour(Colours::ink);
        fillEllipseCentred(g, 0, -80, 7, 7);
        g.setColour(Colours::cream);
        fillEllipseCentred(g, 0, -80, 3, 3);
    }

    // the BPM cloud
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(470.0f, 120.0f));
        juce::Path cloud;
        cloud.startNewSubPath(-160, 50);
        cloud.quadraticTo(-180, -10, -120, -20);
        cloud.quadraticTo(-110, -80, -40, -70);
        cloud.quadraticTo(-10, -110, 50, -80);
        cloud.quadraticTo(120, -100, 140, -30);
        cloud.quadraticTo(190, -20, 170, 50);
        cloud.closeSubPath();
        g.setColour(tapMode ? Colours::dim : Colours::panel);
        g.fillPath(cloud);
        drawText(g, juce::String(juce::roundToInt(bpm)), -150, -56, 300, 86, 80,
                 Colours::text, true);
        drawText(g, "BPM", -60, 26, 120, 18, 14, Colours::muted);
    }

    // the quantize grid, with swing pushing the off-beats along
    {
        int dots = gridName.startsWith("1/4")   ? 4
                   : gridName.startsWith("1/8") ? 8
                                                : 16;
        drawText(g,
                 "grid " + gridName + utf8("  ·  swing ") +
                     juce::String(swing) + "%",
                 330, 220, 420, 18, 12, Colours::muted, false,
                 juce::Justification::centredLeft);
        float spacing = 420.0f / dots;
        for (int i = 0; i < dots; i++) {
            float offset = (i % 2 == 1) ? swing / 100.0f * 0.5f : 0.0f;
            float x = 336.0f + (i + offset) * spacing;
            bool downbeat = i % (dots / 4) == 0;
            g.setColour(i % 2 == 1 ? Colours::green : Colours::cream);
            fillEllipseCentred(g, x, 258, downbeat ? 7.0f : 5.0f,
                               downbeat ? 7.0f : 5.0f);
        }
    }

    // click speaker
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(740.0f, 70.0f));
        juce::Path speaker;
        speaker.startNewSubPath(-14, -10);
        speaker.lineTo(-4, -10);
        speaker.lineTo(8, -22);
        speaker.lineTo(8, 22);
        speaker.lineTo(-4, 10);
        speaker.lineTo(-14, 10);
        speaker.closeSubPath();
        g.setColour(Colours::red);
        g.fillPath(speaker);
        float norm = float((clickGain - viewModel.clickTrackGainLowerLimit) /
                           (viewModel.clickTrackGainUpperLimit -
                            viewModel.clickTrackGainLowerLimit));
        int waves = juce::roundToInt(1.0f + norm * 2.0f);
        for (int w = 0; w < waves; w++) {
            float r = 14.0f + w * 9.0f;
            juce::Path arc;
            arc.addCentredArc(4.0f, 0.0f, r, r, 0.0f,
                              juce::degreesToRadians(45.0f),
                              juce::degreesToRadians(135.0f), true);
            g.strokePath(
                arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
        }
    }

    // tap button
    g.setColour(tapMode ? Colours::yellow : Colours::panel);
    fillEllipseCentred(g, 740.0f, 150.0f, 24.0f, 24.0f);
    drawText(g, "tap", 716, 138, 48, 24, 13,
             tapMode ? Colours::ink : Colours::cream, true);
}

void TempoSettingsView::encoder1Increased() {
    if (hasFocus())
        viewModel.incrementBpm();
}

void TempoSettingsView::encoder1Decreased() {
    if (hasFocus())
        viewModel.decrementBpm();
}

void TempoSettingsView::encoder2Increased() {
    if (hasFocus())
        viewModel.incrementQuantizeGrid();
}

void TempoSettingsView::encoder2Decreased() {
    if (hasFocus())
        viewModel.decrementQuantizeGrid();
}

void TempoSettingsView::encoder3Increased() {
    if (hasFocus())
        viewModel.incrementSwing();
}

void TempoSettingsView::encoder3Decreased() {
    if (hasFocus())
        viewModel.decrementSwing();
}

void TempoSettingsView::encoder4Increased() {
    if (hasFocus())
        viewModel.incrementClickTrackGain();
}

void TempoSettingsView::encoder4Decreased() {
    if (hasFocus())
        viewModel.decrementClickTrackGain();
}

void TempoSettingsView::tempoSettingsButtonPressed() {
    viewModel.enableTapMode();
}

void TempoSettingsView::bpmChanged(const double newBpm, const double) {
    bpm = newBpm;
    repaint();
}

void TempoSettingsView::clickTrackGainChanged(const double newGain) {
    clickGain = newGain;
    repaint();
}

void TempoSettingsView::tapModeChanged(const bool newTapMode) {
    tapMode = newTapMode;
    repaint();
}

void TempoSettingsView::quantizeSettingsChanged(const juce::String &name,
                                                const int swingPercent) {
    gridName = name;
    swing = swingPercent;
    repaint();
}
