#include "VirtualHardware.h"

using MCM = app_services::MidiCommandManager;

VirtualHardware::VirtualHardware(tracktion::Engine &e, MCM &mcm)
    : engine(e), midiCommandManager(mcm) {}

void VirtualHardware::createVirtualDevice(tracktion::Engine &engine) {
    auto &dm = engine.getDeviceManager();
    for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        if (auto d = dm.getMidiInDevice(i))
            if (d->getName() == deviceName)
                return; // remembered from an earlier run
    auto result = dm.createVirtualMidiDevice(deviceName);
    if (result.failed())
        juce::Logger::writeToLog("Virtual keyboard input: " +
                                 result.getErrorMessage());
}

void VirtualHardware::sendCC(int cc, int value) {
    midiCommandManager.midiMessageReceived(
        juce::MidiMessage::controllerEvent(1, cc, value), deviceName);
}

void VirtualHardware::button(int cc, bool down) { sendCC(cc, down ? 127 : 0); }

void VirtualHardware::turnEncoder(int index, int direction) {
    static const int ccs[] = {MCM::ENCODER_1, MCM::ENCODER_2, MCM::ENCODER_3,
                              MCM::ENCODER_4};
    if (index >= 0 && index < 4 && direction != 0)
        sendCC(ccs[index], direction > 0 ? 1 : 127);
}

void VirtualHardware::pressEncoder(int index, bool down) {
    static const int ccs[] = {MCM::ENCODER_1_BUTTON, MCM::ENCODER_2_BUTTON,
                              MCM::ENCODER_3_BUTTON, MCM::ENCODER_4_BUTTON};
    if (index >= 0 && index < 4)
        sendCC(ccs[index], down ? 127 : 0);
}

void VirtualHardware::sendNote(const juce::MidiMessage &message) {
    auto &dm = engine.getDeviceManager();
    for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        if (auto d = dm.getMidiInDevice(i))
            if (d->getName() == deviceName)
                d->handleIncomingMidiMessage(message);
}

int VirtualHardware::noteOn(int keyIndex, juce::uint8 velocity) {
    int note = juce::jlimit(0, 127, firstNote + keyIndex + 12 * octave);
    auto message = juce::MidiMessage::noteOn(1, note, velocity);
    sendNote(message);
    // the screens use notes too (drum pads, sequencers, Tombola balls...)
    midiCommandManager.midiMessageReceived(message, deviceName);
    return note;
}

void VirtualHardware::noteOff(int note) {
    sendNote(juce::MidiMessage::noteOff(1, note));
}

void VirtualHardware::changeOctave(int delta) {
    int newOctave = juce::jlimit(-4, 4, octave + delta);
    if (newOctave == octave)
        return;
    octave = newOctave;
    sendCC(MCM::OCTAVE_CHANGE, octave + 4);
    if (onStateChanged)
        onStateChanged();
}

void VirtualHardware::setControl(bool down) {
    if (down == controlDown)
        return;
    controlDown = down;
    sendCC(MCM::CONTROL_BUTTON, down ? 127 : 0);
    if (onStateChanged)
        onStateChanged();
}
