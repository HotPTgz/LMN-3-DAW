#include "RenderMenuComponent.h"

RenderMenuComponent::RenderMenuComponent(app_services::MidiCommandManager &mcm)
    : midiCommandManager(mcm) {
    setVisible(false);
}

RenderMenuComponent::~RenderMenuComponent() {
    if (midiCommandManager.getFocusedComponent() == this)
        midiCommandManager.setFocusedComponent(previouslyFocused);
}

void RenderMenuComponent::show(bool isLoopRender) {
    loopRender = isLoopRender;
    selectedIndex = 0;
    previouslyFocused = midiCommandManager.getFocusedComponent();
    setVisible(true);
    toFront(false);
    midiCommandManager.setFocusedComponent(this);
    repaint();
    // Close automatically if something else (e.g. a tab button) takes focus
    startTimer(200);
}

void RenderMenuComponent::close() {
    stopTimer();
    setVisible(false);
    if (midiCommandManager.getFocusedComponent() == this)
        midiCommandManager.setFocusedComponent(previouslyFocused);
}

void RenderMenuComponent::timerCallback() {
    if (midiCommandManager.getFocusedComponent() != this) {
        stopTimer();
        setVisible(false);
    }
}

void RenderMenuComponent::paint(juce::Graphics &g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(appLookAndFeel.backgroundColour);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(appLookAndFeel.colour1);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 2.0f);

    auto area = getLocalBounds().reduced(getWidth() / 20, getHeight() / 12);
    auto rowHeight = area.getHeight() / 3;
    juce::Font font(juce::Font::getDefaultMonospacedFontName(),
                    rowHeight * .55f, juce::Font::plain);
    g.setFont(font);

    g.setColour(appLookAndFeel.textColour);
    g.drawText(loopRender ? "Render loop" : "Render",
               area.removeFromTop(rowHeight), juce::Justification::centred);

    for (int i = 0; i < items.size(); i++) {
        auto row = area.removeFromTop(rowHeight).reduced(0, rowHeight / 10);
        if (i == selectedIndex) {
            g.setColour(appLookAndFeel.colour1);
            g.fillRoundedRectangle(row.toFloat(), 4.0f);
        }
        g.setColour(appLookAndFeel.textColour);
        g.drawText(items[i], row, juce::Justification::centred);
    }
}

void RenderMenuComponent::encoder1Increased() {
    selectedIndex = juce::jmin(items.size() - 1, selectedIndex + 1);
    repaint();
}

void RenderMenuComponent::encoder1Decreased() {
    selectedIndex = juce::jmax(0, selectedIndex - 1);
    repaint();
}

void RenderMenuComponent::encoder1ButtonReleased() {
    int choice = selectedIndex;
    close();
    if (onRender != nullptr)
        onRender(choice);
}

void RenderMenuComponent::stopButtonReleased() { close(); }
