#include "PowerOffOverlay.h"
#include "PlayfulApp.h"

using namespace Playful;

PowerOffOverlay::PowerOffOverlay() {
    setOpaque(true);
    setVisible(false);
}

void PowerOffOverlay::start(int seconds) {
    holdSeconds = seconds;
    saved = false;
    startTime = juce::Time::getMillisecondCounterHiRes();
    setVisible(true);
    toFront(false);
    startTimerHz(30);
}

void PowerOffOverlay::showSaved(const juce::String &message) {
    saved = true;
    savedMessage = message;
    setVisible(true);
    toFront(false);
    repaint();
}

void PowerOffOverlay::hide() {
    stopTimer();
    setVisible(false);
}

void PowerOffOverlay::paint(juce::Graphics &g) {
    double elapsed =
        (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
    double time = animationsEnabled() ? elapsed : 1.3;
    double left = saved ? 0.0 : juce::jmax(0.0, holdSeconds - elapsed);
    float fraction = float(left / juce::jmax(1, holdSeconds));

    std::array<KnobInfo, 4> knobs;
    for (auto &k : knobs)
        k.enabled = false;

    paintScreen(
        g, getLocalBounds(), "Goodnight", "saving", "hold Control + Stop",
        knobs,
        [&](juce::Graphics &stage) {
            const float cx = 400.0f, cy = 150.0f, r = 104.0f;
            juce::Path track;
            track.addCentredArc(cx, cy, r, r, 0.0f, 0.0f,
                                juce::MathConstants<float>::twoPi, true);
            stage.setColour(Colours::panel);
            stage.strokePath(track, juce::PathStrokeType(16.0f));
            if (fraction > 0.001f) {
                juce::Path ring;
                ring.addCentredArc(cx, cy, r, r, 0.0f, 0.0f,
                                   fraction * juce::MathConstants<float>::twoPi,
                                   true);
                stage.setColour(Colours::yellow);
                stage.strokePath(ring, juce::PathStrokeType(
                                           16.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
            }
            drawMoon(stage, cx + 10.0f, cy, 0.62f, time);

            juce::String message =
                saved ? savedMessage
                      : "Turning off in " + juce::String(int(std::ceil(left))) +
                            "...";
            drawText(stage, message, 100, 272, 600, 26, 20, Colours::text,
                     true);
            drawText(stage,
                     saved ? juce::String("sweet dreams")
                           : juce::String("let go of Control or Stop to "
                                          "cancel"),
                     100, 298, 600, 20, 13, Colours::muted);
        },
        true, time);
}
