#include "RenderMenuComponent.h"
#include "PlayfulApp.h"

using namespace Playful;

RenderMenuComponent::RenderMenuComponent(app_services::MidiCommandManager &mcm)
    : midiCommandManager(mcm) {
    setVisible(false);
    setOpaque(true);
}

RenderMenuComponent::~RenderMenuComponent() {
    if (midiCommandManager.getFocusedComponent() == this)
        midiCommandManager.setFocusedComponent(previouslyFocused);
}

void RenderMenuComponent::show(bool isLoopRender,
                               const juce::String &projectName) {
    loopRender = isLoopRender;
    project = projectName;
    selectedIndex = 0;
    baking = false;
    startTime = juce::Time::getMillisecondCounterHiRes();
    previouslyFocused = midiCommandManager.getFocusedComponent();
    setVisible(true);
    toFront(false);
    midiCommandManager.setFocusedComponent(this);
    repaint();
    // Close automatically if something else (e.g. a tab button) takes focus;
    // also keeps the oven animated
    startTimerHz(30);
}

void RenderMenuComponent::showBaking() {
    baking = true;
    repaint();
}

void RenderMenuComponent::close() {
    stopTimer();
    baking = false;
    setVisible(false);
    if (midiCommandManager.getFocusedComponent() == this)
        midiCommandManager.setFocusedComponent(previouslyFocused);
}

void RenderMenuComponent::timerCallback() {
    if (midiCommandManager.getFocusedComponent() != this) {
        stopTimer();
        setVisible(false);
        return;
    }
    if (animationsEnabled())
        repaint();
}

void RenderMenuComponent::paint(juce::Graphics &g) {
    double time =
        animationsEnabled()
            ? (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0
            : 1.3;
    std::array<KnobInfo, 4> knobs;
    knobs[0] = {utf8("WHAT · GO"), selectedIndex == 0 ? "Whole mix" : "Stems",
                float(selectedIndex), Colours::blue};
    for (int i = 1; i < 4; i++)
        knobs[size_t(i)].enabled = false;

    juce::String subtitle =
        (project.isNotEmpty() ? project + utf8("  ·  ") : juce::String()) +
        "stop: cancel";

    paintScreen(
        g, getLocalBounds(), baking ? "Baking..." : "Render", subtitle, {},
        knobs,
        [&](juce::Graphics &stage) {
            drawOven(stage, 190.0f, 150.0f, 1.0f, time, baking);

            if (baking) {
                drawText(stage, "Baking your song...", 380, 110, 380, 30, 22,
                         Colours::text, true, juce::Justification::centredLeft);
                drawText(stage, "this can take a little while", 380, 142, 380,
                         20, 13, Colours::muted, false,
                         juce::Justification::centredLeft);
                stage.setColour(Colours::panel);
                stage.fillRoundedRectangle(380, 190, 380, 18, 9);
                float t = loop(time, 1.6);
                stage.setColour(Colours::green);
                stage.fillRoundedRectangle(380.0f + t * 300.0f, 190.0f, 80.0f,
                                           18.0f, 9.0f);
                return;
            }

            juce::String mixDetail =
                loopRender ? "the loop, one file" : "one file";
            juce::String stemsDetail = loopRender ? "the loop, a file per track"
                                                  : "one file per track";
            drawCard(stage, {380.0f, 70.0f, 380.0f, 44.0f}, "Whole mix",
                     mixDetail, Colours::yellow, selectedIndex == 0, false,
                     time);
            drawCard(stage, {380.0f, 124.0f, 380.0f, 44.0f}, "Stems",
                     stemsDetail, Colours::green, selectedIndex == 1, false,
                     time);
            if (loopRender)
                drawText(stage,
                         utf8("Loop is on: only the part between the flags "
                              "gets baked"),
                         380, 190, 380, 20, 12, Colours::muted, false,
                         juce::Justification::centredLeft);
            drawText(stage, "turn 1 to choose, press 1 to bake", 380, 214, 380,
                     20, 12, Colours::muted, false,
                     juce::Justification::centredLeft);
        },
        true, time);
}

void RenderMenuComponent::encoder1Increased() {
    if (baking)
        return;
    selectedIndex = juce::jmin(1, selectedIndex + 1);
    repaint();
}

void RenderMenuComponent::encoder1Decreased() {
    if (baking)
        return;
    selectedIndex = juce::jmax(0, selectedIndex - 1);
    repaint();
}

void RenderMenuComponent::encoder1ButtonReleased() {
    if (baking)
        return;
    showBaking();
    if (onRender != nullptr)
        onRender(selectedIndex);
}

void RenderMenuComponent::stopButtonReleased() {
    if (!baking)
        close();
}
