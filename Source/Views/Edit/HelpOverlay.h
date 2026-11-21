#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// What a screen is for and what its controls do, shown on Control + Play
struct HelpCard {
    struct Row {
        // "1"-"4": an encoder (turn, and press if the text says so);
        // "C1"-"C4": that encoder while holding Control; anything else is
        // shown as a button name ("+", "Key", "Rec"...)
        juce::String control;
        juce::String text;
    };

    juce::String title;
    juce::String about; // a sentence or two
    std::vector<Row> rows;
    juce::String tip;
};

// The card for whatever screen is showing (the component the encoders and
// buttons are going to, or something inside it)
HelpCard helpCardFor(juce::Component *focused);

// Full-screen help card. Any button, encoder or key closes it.
class HelpOverlay : public juce::Component {
  public:
    void show(const HelpCard &card);
    void hide();

    void paint(juce::Graphics &g) override;

  private:
    HelpCard card;
};
