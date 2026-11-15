#pragma once
#include "VirtualHardware.h"
#include <app_services/app_services.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

// Lets a computer stand in for the LMN-3 hardware, to try the app on a
// Windows (or Linux/Mac) desktop:
//
//  - two rows of letter keys are the 24 keys, played into the selected
//    track through a virtual MIDI input ("Computer keyboard")
//  - other keys are the buttons, sent to the MidiCommandManager as the same
//    CC messages the hardware sends
//  - the mouse wheel over one of the four knob boxes turns that encoder, and
//    a click presses it
//  - F12 shows a cheat sheet
//
// Keys are polled (not taken from key events) so that holding several keys at
// once works the same as on the hardware, e.g. holding + and pressing a note.
class DesktopInput : private juce::Timer, private juce::MouseListener {
  public:
    explicit DesktopInput(VirtualHardware &hardware);
    ~DesktopInput() override;

    // Off when the on-screen touch controls handle the mouse
    void setMouseEnabled(bool enabled) { mouseEnabled = enabled; }

    // The window to listen to (it is rebuilt when another project opens)
    void attachTo(juce::Component *window);

    // Exposed for tests: which of the 24 keys a character plays (-1 = none),
    // and which encoder box an x position in the knob strip belongs to
    static int noteIndexForKey(juce::juce_wchar key);
    static int encoderForStripX(float designX);

  private:
    struct Button {
        int keyCode;
        bool needsCtrl;
        int cc;
        bool down = false;
    };

    struct EncoderKeys {
        int decreaseKey, increaseKey, pressKey;
        int heldDirection = 0;
        double nextRepeat = 0.0;
        bool pressDown = false;
    };

    void timerCallback() override;
    void mouseWheelMove(const juce::MouseEvent &,
                        const juce::MouseWheelDetails &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;

    void sendCC(int cc, int value);
    void turnEncoder(int index, int direction);
    void pressEncoder(int index, bool down);
    void playNote(int index, bool down);
    void releaseEverything();
    int stripEncoderAt(const juce::MouseEvent &e) const;
    void toggleHelp();
    void hideHelp();
    void showHelpOnFirstRun();

    VirtualHardware &hardware;
    juce::Component::SafePointer<juce::Component> window;
    std::unique_ptr<juce::Component> help;

    std::vector<Button> buttons;
    std::array<EncoderKeys, 4> encoderKeys;
    std::array<bool, 24> noteDown{};
    std::array<int, 24> notePlaying{};
    bool controlDown = false, octaveDownKey = false, octaveUpKey = false,
         helpKey = false, upKey = false, downKey = false;
    bool mouseEnabled = true;
    std::array<float, 4> wheelAccumulator{};
    int mouseEncoder = -1;
};
