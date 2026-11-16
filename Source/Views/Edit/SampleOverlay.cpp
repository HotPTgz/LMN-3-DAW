#include "SampleOverlay.h"
#include "PlayfulApp.h"

using namespace Playful;

SampleOverlay::SampleOverlay(LiveControls &c) : controls(c) {
    setOpaque(true);
    setVisible(false);
}

void SampleOverlay::update() {
    bool show = controls.getSampleState() != LiveControls::SampleState::idle;
    if (show && !isVisible()) {
        shownAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        setVisible(true);
        toFront(false);
    } else if (!show && isVisible()) {
        setVisible(false);
    }
    if (show)
        repaint();
}

namespace {
void drawMicFriend(juce::Graphics &g, float cx, float cy, float s, float level,
                   double time) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy));

    // sound rings, bigger when it's louder
    for (int i = 0; i < 3; i++) {
        float grow = float(std::fmod(time * 0.8 + i / 3.0, 1.0));
        float r = 70.0f + grow * (40.0f + 90.0f * level);
        g.setColour(
            Colours::pink.withAlpha((1.0f - grow) * (0.25f + 0.6f * level)));
        g.drawEllipse(-r, -r - 20, 2 * r, 2 * r, 4.0f);
    }
    // stand
    g.setColour(Colours::muted);
    g.fillRoundedRectangle(-5, 40, 10, 50, 5);
    g.fillRoundedRectangle(-34, 86, 68, 12, 6);
    // the U holder
    juce::Path holder;
    holder.addCentredArc(0, 0, 54, 54, 0, juce::degreesToRadians(100.0f),
                         juce::degreesToRadians(260.0f), true);
    g.strokePath(holder,
                 juce::PathStrokeType(8.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    // head
    g.setColour(Colours::pink);
    g.fillRoundedRectangle(-40, -96, 80, 128, 40);
    g.setColour(Colours::ink.withAlpha(0.18f));
    for (int i = 0; i < 4; i++)
        g.fillRoundedRectangle(-26, -80 + i * 13.0f, 52, 4, 2);
    drawFace(g, 0, -4, 1.1f, level > 0.15f ? Mood::sing : Mood::smile, time);
}
} // namespace

void SampleOverlay::paint(juce::Graphics &g) {
    double time =
        animationsEnabled()
            ? juce::Time::getMillisecondCounterHiRes() / 1000.0 - shownAt
            : 1.3;
    bool placing =
        controls.getSampleState() == LiveControls::SampleState::placing;
    float level = juce::jlimit(0.0f, 1.0f, std::sqrt(controls.getInputLevel()));
    double seconds = controls.getSampleSeconds();
    double left = juce::jmax(0.0, LiveControls::maxSampleSeconds - seconds);

    std::array<KnobInfo, 4> knobs;
    for (auto &k : knobs)
        k.enabled = false;
    knobs[0] = {"level", juce::String(juce::roundToInt(level * 100)) + "%",
                level, Colours::encoder(0), !placing};
    knobs[1] = {"length", juce::String(seconds, 1) + " s",
                float(seconds / LiveControls::maxSampleSeconds),
                Colours::encoder(1), true};

    paintScreen(
        g, getLocalBounds(), placing ? "Pick a pad" : "Sampling",
        placing ? "drum sampler" : "mic",
        placing ? "MIC: just save it" : "MIC: stop", knobs,
        [&](juce::Graphics &stage) {
            if (placing) {
                drawDrum(stage, 400, 140, 1.0f, time);
                drawText(stage, "Play the key you want it on", 100, 262, 600,
                         28, 21, Colours::text, true);
                drawText(stage, "or press MIC to keep it in samples/Recorded",
                         100, 292, 600, 20, 13, Colours::muted);
                return;
            }
            drawMicFriend(stage, 400, 150, 1.0f, level, time);
            drawText(stage, "Make a sound!", 100, 262, 600, 28, 21,
                     Colours::text, true);
            drawText(
                stage,
                utf8("press MIC when you're done  ·  stops by itself in ") +
                    juce::String(int(std::ceil(left))) + " s",
                100, 292, 600, 20, 13, Colours::muted);
        },
        true, time);
}
