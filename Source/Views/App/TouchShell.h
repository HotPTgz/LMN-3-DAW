#pragma once
#include "VirtualHardware.h"
#include <juce_gui_basics/juce_gui_basics.h>

// The LMN-3 as a touch screen (the Android phone app, or the desktop app with
// LMN3_TOUCH=1): the LMN-3 screen in the middle, its buttons on both sides
// and two octaves of keys along the bottom.
//
//  - drag on one of the four knob boxes at the bottom of the screen to turn
//    that encoder; tap it to press it
//  - Ctrl stays on until it is tapped again
//  - "Big screen" hides the keys so the screen can use the whole height
//  - several fingers work at once (chords, holding + while playing a key...)
class TouchShell : public juce::Component {
  public:
    TouchShell(std::unique_ptr<juce::Component> screen,
               VirtualHardware &hardware);
    ~TouchShell() override;

    juce::Component *getScreen() const { return screen.get(); }

    void paint(juce::Graphics &g) override;
    void resized() override;

    // Which knob box an x position in design space (0..800) belongs to
    static int encoderForDesignX(float x);

  private:
    class Pad;
    class Keyboard;

    struct ScreenListener;
    void screenDown(const juce::MouseEvent &e);
    void screenDrag(const juce::MouseEvent &e);
    void screenUp(const juce::MouseEvent &e);
    int encoderAt(juce::Point<float> screenPosition) const;
    void setBigScreen(bool big);
    void layoutPads();

    std::unique_ptr<juce::Component> screen;
    VirtualHardware &hardware;
    std::unique_ptr<Pad> leftPad, rightPad, bottomLeft, bottomRight;
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<ScreenListener> screenListener;
    bool bigScreen = false;

    struct Drag {
        int encoder = -1;
        juce::Point<float> start;
        float accumulated = 0.0f;
        bool moved = false;
    };
    std::map<int, Drag> drags; // by touch index

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchShell)
};
