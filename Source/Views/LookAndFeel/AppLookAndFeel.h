#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

class AppLookAndFeel : public juce::LookAndFeel_V4 {
  public:
    AppLookAndFeel();

    juce::Colour blueColour = juce::Colour(0xff458588);
    juce::Colour greenColour = juce::Colour(0xff689d6a);
    juce::Colour whiteColour = juce::Colour(0xfff9f5d7);
    juce::Colour redColour = juce::Colour(0xffcc241d);
    juce::Colour yellowColour = juce::Colour(0xffd79921);
    juce::Colour orangeColour = juce::Colour(0xffd65d0e);
    juce::Colour purpleColour = juce::Colour(0xffb16286);
    juce::Colour blackColour = juce::Colour(0xff282828);
    juce::Colour greenYellowColour = juce::Colour(0xff98971a);

    // These are just defaults (the playful palette); config.yaml can
    // override them
    juce::Colour backgroundColour = juce::Colour(0xff2b2140);
    juce::Colour textColour = juce::Colour(0xfffff7e6);
    juce::Colour colour1 = juce::Colour(0xff5ec8f2);
    juce::Colour colour2 = juce::Colour(0xff7ee08a);
    juce::Colour colour3 = juce::Colour(0xfffff1c9);
    juce::Colour colour4 = juce::Colour(0xffff6b6b);
    juce::Colour colour5 = juce::Colour(0xffffd93d);
    juce::Colour colour6 = juce::Colour(0xffffa94d);
    juce::Colour colour7 = juce::Colour(0xffb28dff);
    juce::Colour colour8 = juce::Colour(0xffff8fd1);

    juce::Array<juce::Colour> colours = juce::Array<juce::Colour>(
        {colour1, colour2, colour4, colour5, colour6, colour7, colour8});

    [[nodiscard]] juce::Colour getRandomColour() const {
        return colours[juce::Random::getSystemRandom().nextInt(colours.size())];
    }

  private:
    void readColoursFromConfig();
};
