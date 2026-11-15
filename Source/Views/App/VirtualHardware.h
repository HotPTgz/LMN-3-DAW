#pragma once
#include <app_services/app_services.h>
#include <tracktion_engine/tracktion_engine.h>

// Stands in for the LMN-3's hardware when there is none: the computer
// keyboard (DesktopInput) and the on-screen controls (TouchShell) both play
// through it. Buttons and encoders become the same CC messages the hardware
// sends; the 24 keys play into a virtual MIDI input routed to the selected
// track, and are also passed to the screens (drum pads, sequencers...).
class VirtualHardware {
  public:
    static constexpr const char *deviceName = "Computer keyboard";

    VirtualHardware(tracktion::Engine &engine,
                    app_services::MidiCommandManager &midiCommandManager);

    // Creates the virtual MIDI input. Call once at start-up, before the
    // edit's inputs are set up.
    static void createVirtualDevice(tracktion::Engine &engine);

    void button(int cc, bool down);
    void turnEncoder(int index, int direction); // +1 = clockwise
    void pressEncoder(int index, bool down);

    // keyIndex 0..23 is the LMN-3's keys from F; returns the note played
    int noteOn(int keyIndex, juce::uint8 velocity = 100);
    void noteOff(int note);

    int getOctave() const { return octave; }
    void changeOctave(int delta);

    bool isControlDown() const { return controlDown; }
    void setControl(bool down);

    // octave or Control changed (to update on-screen controls)
    std::function<void()> onStateChanged;

    static constexpr int firstNote = 53; // F3

  private:
    void sendCC(int cc, int value);
    void sendNote(const juce::MidiMessage &message);

    tracktion::Engine &engine;
    app_services::MidiCommandManager &midiCommandManager;
    int octave = 0;
    bool controlDown = false;
};
