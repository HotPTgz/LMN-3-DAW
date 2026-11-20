#pragma once
#include <app_services/app_services.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <tracktion_engine/tracktion_engine.h>

/**
 * The LMN-3 v3 controls that work on every screen:
 *
 *  - Joystick click: sustain (like a sustain pedal) while held.
 *  - Control + joystick click: the firmware switches the joystick's up/down
 *    mod wheel on or off and reports it (CC 115); we show a message.
 *  - Control + Tempo: the firmware switches key velocity on or off (off:
 *    every note at the same velocity) and reports it (CC 116); we show a
 *    message.
 *  - MIC: arm the built-in mic on the selected track. The next Record then
 *    records audio from the mic onto that track. MIC again disarms it.
 *    The mic is not played through the speakers (it would howl).
 *  - Control + MIC: sample. Make a sound, press MIC again (or wait 20 s),
 *    and the sound goes into the selected track's sampler: a drum sampler
 *    asks for the key to put it on, a synth sampler plays it across the
 *    keys, and a track with no instrument gets a new sampler. It is always
 *    saved in samples/Recorded too.
 */
class LiveControls : public app_services::MidiCommandManager::Listener,
                     private juce::AudioIODeviceCallback,
                     private juce::Timer {
  public:
    enum class SampleState { idle, recording, placing };

    LiveControls(tracktion::Edit &edit, app_services::MidiCommandManager &mcm,
                 std::function<tracktion::AudioTrack *()> getSelectedTrack,
                 std::function<void(const juce::String &, int)> showMessage);
    ~LiveControls() override;

    // MidiCommandManager::Listener
    void micButtonPressed() override;
    void joystickButtonPressed() override;
    void joystickButtonReleased() override;
    void modWheelStateChanged(bool isOn) override;
    void velocityStateChanged(bool isOn) override;
    void noteOnReceived(int noteNumber) override;

    SampleState getSampleState() const { return sampleState; }
    double getSampleSeconds() const;
    static constexpr double maxSampleSeconds = 20.0;
    // 0-1, peak of the mic input (while armed or sampling)
    float getInputLevel() const { return level.load(); }
    bool isMicArmed() const { return armedTrack != nullptr; }

    // called on the message thread whenever the state above changes
    std::function<void()> onStateChanged;

    // the edit property the Tracks screen reads to show the armed track
    static inline const juce::Identifier micTrackID{"micTrack"};

  private:
    tracktion::Edit &edit;
    app_services::MidiCommandManager &midiCommandManager;
    std::function<tracktion::AudioTrack *()> getSelectedTrack;
    std::function<void(const juce::String &, int)> showMessage;

    // mic recording onto a track
    tracktion::AudioTrack::Ptr armedTrack;
    void armMic();
    void disarmMic();
    tracktion::InputDeviceInstance *findMicInput();
    // opens the sound card's inputs if they aren't (true when open)
    bool ensureInputsOpen();

    // sampling
    SampleState sampleState = SampleState::idle;
    std::vector<float> sampleBuffer;
    std::atomic<int> samplePosition{0};
    std::atomic<int> sampleCapacity{0}; // samples that fit at this rate
    std::atomic<bool> capturing{false};
    std::atomic<float> level{0.0f};
    double captureSampleRate = 44100.0;
    double captureStarted = 0.0;
    juce::File lastSample;
    double placingStarted = 0.0;
    bool tapInstalled = false;
    // LMN3_FAKE_MIC=1: a test tone stands in for the mic (for testing on a
    // computer without one)
    bool fakeMic = false;
    double fakeMicClock = 0.0;
    void startSampling();
    void finishSampling();
    void cancelPlacing();
    juce::File writeSample(int numSamples);
    void useSample(const juce::File &file);
    void putOnDrumPad(int noteNumber);
    void loadIntoSynthSampler(tracktion::SamplerPlugin &sampler,
                              const juce::File &file);
    void installTap(bool shouldBeInstalled);
    void stateChanged();

    void sendSustain(bool down);

    // juce::AudioIODeviceCallback (audio thread)
    void audioDeviceIOCallback(const float **inputChannelData,
                               int numInputChannels, float **outputChannelData,
                               int numOutputChannels, int numSamples) override;
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
    void audioDeviceStopped() override {}

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveControls)
};
