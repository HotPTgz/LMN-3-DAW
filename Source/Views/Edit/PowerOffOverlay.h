#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Full-screen "Goodnight" shown while Control + Stop are held: a sleepy moon
// in a ring that empties as the countdown runs out.
class PowerOffOverlay : public juce::Component, private juce::Timer {
  public:
    PowerOffOverlay();

    void start(int seconds);
    void showSaved(const juce::String &message);
    void hide();

    void paint(juce::Graphics &g) override;

  private:
    double startTime = 0.0;
    int holdSeconds = 3;
    bool saved = false;
    juce::String savedMessage;

    void timerCallback() override { repaint(); }
};
