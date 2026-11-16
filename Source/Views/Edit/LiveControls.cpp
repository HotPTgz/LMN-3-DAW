#include "LiveControls.h"
#include <app_configuration/app_configuration.h>
#include <app_view_models/app_view_models.h>
#include <internal_plugins/internal_plugins.h>

namespace {
bool isPlayableMidiInput(tracktion::InputDevice &device) {
    auto type = device.getDeviceType();
    return type == tracktion::InputDevice::physicalMidiDevice ||
           type == tracktion::InputDevice::virtualMidiDevice;
}

juce::String trackLabel(tracktion::Edit &edit, tracktion::AudioTrack &track) {
    int index = tracktion::getAudioTracks(edit).indexOf(&track);
    return index >= 0 ? "track " + juce::String(index + 1) : track.getName();
}

double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }
} // namespace

LiveControls::LiveControls(
    tracktion::Edit &e, app_services::MidiCommandManager &mcm,
    std::function<tracktion::AudioTrack *()> selected,
    std::function<void(const juce::String &, int)> message)
    : edit(e), midiCommandManager(mcm), getSelectedTrack(std::move(selected)),
      showMessage(std::move(message)) {
    midiCommandManager.addListener(this);
    edit.state.removeProperty(micTrackID, nullptr);
    fakeMic =
        juce::SystemStats::getEnvironmentVariable("LMN3_FAKE_MIC", "") == "1";
    // allocated once, so the audio thread never sees it move (20 s at 96 kHz)
    sampleBuffer.assign(size_t(96000 * maxSampleSeconds), 0.0f);
}

LiveControls::~LiveControls() {
    onStateChanged = nullptr;
    showMessage = [](const juce::String &, int) {};
    midiCommandManager.removeListener(this);
    stopTimer();
    installTap(false);
    if (armedTrack != nullptr)
        disarmMic();
}

void LiveControls::stateChanged() {
    if (onStateChanged)
        onStateChanged();
}

//==============================================================================
// Joystick

void LiveControls::sendSustain(bool down) {
    // The notes reach the instrument straight from the LMN-3's MIDI input, so
    // the pedal goes the same way. Only one input is used, so it isn't sent
    // twice.
    auto &dm = edit.engine.getDeviceManager();
    tracktion::MidiInputDevice *target = nullptr;
    for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        if (auto d = dm.getMidiInDevice(i))
            if (d->isEnabled() && isPlayableMidiInput(*d))
                if (target == nullptr ||
                    d->getDeviceType() ==
                        tracktion::InputDevice::physicalMidiDevice)
                    target = d;
    if (target != nullptr)
        target->handleIncomingMidiMessage(
            juce::MidiMessage::controllerEvent(1, 64, down ? 127 : 0));
}

void LiveControls::joystickButtonPressed() {
    // with Control held the firmware uses the click itself (mod wheel)
    if (midiCommandManager.isControlDown)
        return;
    sendSustain(true);
}

void LiveControls::joystickButtonReleased() { sendSustain(false); }

void LiveControls::modWheelStateChanged(bool isOn) {
    showMessage(isOn ? "Joystick up/down: mod wheel on"
                     : "Joystick up/down: off",
                1400);
}

//==============================================================================
// MIC

void LiveControls::micButtonPressed() {
    if (sampleState == SampleState::recording) {
        finishSampling();
        return;
    }
    if (sampleState == SampleState::placing) {
        cancelPlacing();
        return;
    }
    if (midiCommandManager.isControlDown) {
        startSampling();
        return;
    }
    if (edit.getTransport().isRecording())
        return; // don't change inputs in the middle of a take
    if (armedTrack != nullptr)
        disarmMic();
    else
        armMic();
}

bool LiveControls::ensureInputsOpen() {
    auto &juceDeviceManager = edit.engine.getDeviceManager().deviceManager;
    auto device = juceDeviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return false;
    if (device->getActiveInputChannels().countNumberOfSetBits() > 0)
        return true;
    int available = device->getInputChannelNames().size();
    if (available == 0)
        return false;
    auto setup = juceDeviceManager.getAudioDeviceSetup();
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setRange(0, juce::jmin(2, available), true);
    auto error = juceDeviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        juce::Logger::writeToLog("LiveControls: can't open the inputs: " +
                                 error);
    device = juceDeviceManager.getCurrentAudioDevice();
    bool open = device != nullptr &&
                device->getActiveInputChannels().countNumberOfSetBits() > 0;
    if (open) {
        // let Tracktion see the new inputs right away
        juceDeviceManager.dispatchPendingMessages();
        edit.getTransport().freePlaybackContext();
        edit.getTransport().ensureContextAllocated();
    }
    return open;
}

tracktion::InputDeviceInstance *LiveControls::findMicInput() {
    if (!ensureInputsOpen())
        return nullptr;
    auto &dm = edit.engine.getDeviceManager();
    for (int i = 0; i < dm.getNumWaveInDevices(); i++)
        if (auto d = dm.getWaveInDevice(i))
            if (!d->isEnabled())
                d->setEnabled(true);

    edit.getTransport().ensureContextAllocated();
    for (auto instance : edit.getAllInputDevices())
        if (instance->getInputDevice().getDeviceType() ==
            tracktion::InputDevice::waveDevice)
            return instance;
    return nullptr;
}

void LiveControls::armMic() {
    auto track = getSelectedTrack();
    if (track == nullptr)
        return;
    auto mic = findMicInput();
    if (mic == nullptr) {
        showMessage("No mic input found", 1600);
        return;
    }
    // never play the mic through the speakers: it would howl
    if (auto wave =
            dynamic_cast<tracktion::WaveInputDevice *>(&mic->getInputDevice()))
        wave->setEndToEnd(false);

    mic->setTargetTrack(*track, 0, true);
    mic->setRecordingEnabled(*track, true);
    // record sound, not (empty) notes, on this track
    for (auto instance : edit.getAllInputDevices())
        if (isPlayableMidiInput(instance->getInputDevice()))
            instance->setRecordingEnabled(*track, false);
    edit.restartPlayback();

    armedTrack = track;
    edit.state.setProperty(micTrackID, track->itemID.toString(), nullptr);
    installTap(true);
    showMessage("Mic on " + trackLabel(edit, *track) + ": press Record", 1600);
    stateChanged();
}

void LiveControls::disarmMic() {
    if (auto mic = findMicInput()) {
        if (armedTrack != nullptr) {
            mic->setRecordingEnabled(*armedTrack, false);
            mic->removeTargetTrack(*armedTrack);
        }
    }
    // notes record on the selected track again
    if (auto selected = getSelectedTrack())
        for (auto instance : edit.getAllInputDevices())
            if (isPlayableMidiInput(instance->getInputDevice()))
                instance->setRecordingEnabled(*selected, true);
    edit.restartPlayback();

    armedTrack = nullptr;
    edit.state.removeProperty(micTrackID, nullptr);
    if (sampleState == SampleState::idle)
        installTap(false);
    showMessage("Mic off", 1000);
    stateChanged();
}

//==============================================================================
// Sampling

double LiveControls::getSampleSeconds() const {
    return samplePosition.load() / juce::jmax(1.0, captureSampleRate);
}

void LiveControls::installTap(bool shouldBeInstalled) {
    if (shouldBeInstalled == tapInstalled)
        return;
    auto &juceDeviceManager = edit.engine.getDeviceManager().deviceManager;
    if (shouldBeInstalled)
        juceDeviceManager.addAudioCallback(this);
    else
        juceDeviceManager.removeAudioCallback(this);
    tapInstalled = shouldBeInstalled;
    if (shouldBeInstalled)
        startTimerHz(30);
    else {
        stopTimer();
        level = 0.0f;
    }
}

void LiveControls::startSampling() {
    ensureInputsOpen();
    auto device =
        edit.engine.getDeviceManager().deviceManager.getCurrentAudioDevice();
    juce::Logger::writeToLog(
        "LiveControls: sampling from " +
        (device != nullptr
             ? device->getName() + " inputs " +
                   juce::String(
                       device->getActiveInputChannels().countNumberOfSetBits())
             : juce::String("no device")));
    bool hasInput = device != nullptr &&
                    device->getActiveInputChannels().countNumberOfSetBits() > 0;
    if (!hasInput && !fakeMic) {
        showMessage("No mic input found", 1600);
        return;
    }
    captureSampleRate =
        device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    fakeMicClock = now();
    sampleCapacity = juce::jmin(int(sampleBuffer.size()),
                                int(captureSampleRate * maxSampleSeconds));
    samplePosition = 0;
    captureStarted = now();
    installTap(true);
    capturing = true;
    sampleState = SampleState::recording;
    stateChanged();
}

void LiveControls::finishSampling() {
    capturing = false;
    int numSamples = samplePosition.load();
    sampleState = SampleState::idle;
    if (armedTrack == nullptr)
        installTap(false);

    auto file = writeSample(numSamples);
    if (!file.existsAsFile()) {
        showMessage("Nothing heard: try again closer to the mic", 1800);
        stateChanged();
        return;
    }
    lastSample = file;
    useSample(file);
    stateChanged();
}

juce::File LiveControls::writeSample(int numSamples) {
    numSamples = juce::jlimit(0, sampleCapacity.load(), numSamples);
    // trim the silence before and after the sound
    const float threshold = juce::Decibels::decibelsToGain(-42.0f);
    int first = 0, last = numSamples - 1;
    while (first < numSamples &&
           std::abs(sampleBuffer[size_t(first)]) < threshold)
        first++;
    while (last > first && std::abs(sampleBuffer[size_t(last)]) < threshold)
        last--;
    if (last - first < int(captureSampleRate * 0.02))
        return {};
    first = juce::jmax(0, first - int(captureSampleRate * 0.005));
    last = juce::jmin(numSamples - 1, last + int(captureSampleRate * 0.15));
    int length = last - first + 1;

    // bring it up to just under full scale (at most +24 dB)
    float peak = 0.0f;
    for (int i = first; i <= last; i++)
        peak = juce::jmax(peak, std::abs(sampleBuffer[size_t(i)]));
    float gain = juce::jmin(juce::Decibels::decibelsToGain(24.0f),
                            juce::Decibels::decibelsToGain(-1.0f) /
                                juce::jmax(peak, 1.0e-6f));

    juce::AudioBuffer<float> out(1, length);
    for (int i = 0; i < length; i++) {
        float fade = 1.0f;
        int fromEnd = length - 1 - i;
        int fadeLength = int(captureSampleRate * 0.01);
        if (fromEnd < fadeLength)
            fade = float(fromEnd) / float(fadeLength);
        out.setSample(0, i, sampleBuffer[size_t(first + i)] * gain * fade);
    }

    auto dir =
        ConfigurationHelpers::getSamplesDirectory().getChildFile("Recorded");
    dir.createDirectory();
    auto file = dir.getNonexistentChildFile(
        "mic " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H%M%S"),
        ".wav", false);

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
    if (stream == nullptr)
        return {};
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), captureSampleRate, 1, 24, {}, 0));
    if (writer == nullptr)
        return {};
    stream.release(); // the writer owns it now
    writer->writeFromAudioSampleBuffer(out, 0, length);
    writer.reset();
    return file;
}

void LiveControls::loadIntoSynthSampler(tracktion::SamplerPlugin &sampler,
                                        const juce::File &file) {
    if (sampler.getNumSounds() <= 0)
        sampler.addSound(file.getFullPathName(),
                         file.getFileNameWithoutExtension(), 0.0, 0.0, 1.0);
    sampler.setSoundMedia(0, file.getFullPathName());
    sampler.setSoundParams(0, 60, 0, 127);
    sampler.setSoundGains(0, 0.0f, 0.0f);
    sampler.setSoundExcerpt(
        0, 0, tracktion::AudioFile(sampler.engine, file).getLength());
    // so the Sampler screen shows (and keeps) this file
    sampler.state
        .getOrCreateChildWithName(
            app_view_models::IDs::SYNTH_SAMPLER_VIEW_STATE, nullptr)
        .setProperty(app_view_models::IDs::curFilePathID,
                     file.getFullPathName(), nullptr);
}

void LiveControls::useSample(const juce::File &file) {
    auto track = getSelectedTrack();
    juce::String name = "Recorded/" + file.getFileNameWithoutExtension();
    if (track == nullptr) {
        showMessage("Saved as " + name, 1800);
        return;
    }

    auto &plugins = track->pluginList;
    if (plugins.findFirstPluginOfType<internal_plugins::DrumSamplerPlugin>() !=
        nullptr) {
        sampleState = SampleState::placing;
        placingStarted = now();
        startTimerHz(30);
        return; // the next key played picks the pad
    }

    for (auto plugin : plugins)
        if (auto sampler = dynamic_cast<tracktion::SamplerPlugin *>(plugin)) {
            loadIntoSynthSampler(*sampler, file);
            showMessage("In the sampler: play it on the keys", 1800);
            return;
        }

    if (app_view_models::PluginPlacement::findInstrumentIndex(plugins) < 0) {
        auto plugin = edit.getPluginCache().createNewPlugin(
            tracktion::SamplerPlugin::xmlTypeName, {});
        if (auto sampler =
                dynamic_cast<tracktion::SamplerPlugin *>(plugin.get())) {
            app_view_models::PluginPlacement::insertPlugin(*track, plugin);
            loadIntoSynthSampler(*sampler, file);
            showMessage("New sampler on " + trackLabel(edit, *track) +
                            ": play it on the keys",
                        2000);
            return;
        }
    }

    showMessage("Saved as " + name, 1800);
}

void LiveControls::noteOnReceived(int noteNumber) {
    if (sampleState == SampleState::placing)
        putOnDrumPad(noteNumber);
}

void LiveControls::putOnDrumPad(int noteNumber) {
    sampleState = SampleState::idle;
    if (armedTrack == nullptr)
        installTap(false);
    auto track = getSelectedTrack();
    auto drums =
        track != nullptr
            ? track->pluginList
                  .findFirstPluginOfType<internal_plugins::DrumSamplerPlugin>()
            : nullptr;
    if (drums == nullptr || !lastSample.existsAsFile()) {
        stateChanged();
        return;
    }

    int index = -1;
    for (int i = 0; i < drums->getNumSounds(); i++)
        if (drums->getKeyNote(i) == noteNumber)
            index = i;
    if (index < 0) {
        drums->addSound(lastSample.getFullPathName(),
                        lastSample.getFileNameWithoutExtension(), 0.0, 0.0,
                        1.0);
        index = drums->getNumSounds() - 1;
        drums->setSoundParams(index, noteNumber, noteNumber, noteNumber);
    }
    drums->setSoundMedia(index, lastSample.getFullPathName());
    drums->setSoundGains(index, 0.0f, 0.0f);
    drums->setSoundExcerpt(
        index, 0, tracktion::AudioFile(edit.engine, lastSample).getLength());
    drums->setSoundOpenEnded(index, true);
    // show that pad on the drum sampler screen (and refresh its names)
    if (noteNumber >= 53 && noteNumber <= 76) {
        auto view = drums->state.getOrCreateChildWithName(
            app_view_models::IDs::DRUM_SAMPLER_VIEW_STATE, nullptr);
        view.setProperty(app_view_models::IDs::selectedSoundIndex,
                         noteNumber - 53, nullptr);
        view.sendPropertyChangeMessage(
            app_view_models::IDs::selectedSoundIndex);
    }

    showMessage(
        "On the " +
            juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 4) +
            " pad",
        1400);
    stateChanged();
}

void LiveControls::cancelPlacing() {
    sampleState = SampleState::idle;
    if (armedTrack == nullptr)
        installTap(false);
    showMessage("Saved as Recorded/" + lastSample.getFileNameWithoutExtension(),
                1800);
    stateChanged();
}

void LiveControls::timerCallback() {
    if (fakeMic && capturing) {
        // a "boop" every second, written as if it came from the sound card
        double t1 = now() - captureStarted;
        int target =
            juce::jmin(sampleCapacity.load(), int(t1 * captureSampleRate));
        int pos = samplePosition.load();
        float peak = 0.0f;
        for (; pos < target; pos++) {
            double t = pos / captureSampleRate;
            double phase = std::fmod(t, 1.0) - 0.4;
            float s = phase > 0.0
                          ? float(0.4 *
                                  std::sin(2 * juce::MathConstants<double>::pi *
                                           220 * t) *
                                  std::exp(-phase * 6))
                          : 0.0f;
            sampleBuffer[size_t(pos)] = s;
            peak = juce::jmax(peak, std::abs(s));
        }
        samplePosition = pos;
        level = juce::jmax(peak, level.load() * 0.9f);
    }
    if (sampleState == SampleState::recording &&
        (!capturing || getSampleSeconds() >= maxSampleSeconds - 0.01))
        finishSampling();
    else if (sampleState == SampleState::placing &&
             now() - placingStarted > 12.0)
        cancelPlacing();
    // the overlay redraws from here (level meter, timer)
    if (sampleState != SampleState::idle)
        stateChanged();
    else if (!tapInstalled)
        stopTimer();
}

//==============================================================================
// Audio thread

void LiveControls::audioDeviceAboutToStart(juce::AudioIODevice *) {}

void LiveControls::audioDeviceIOCallback(const float **input, int numInputs,
                                         float **output, int numOutputs,
                                         int numSamples) {
    // this callback only listens: it adds nothing to the output
    for (int c = 0; c < numOutputs; c++)
        if (output[c] != nullptr)
            juce::FloatVectorOperations::clear(output[c], numSamples);

    float peak = 0.0f;
    bool write = capturing.load() && numInputs > 0;
    int pos = samplePosition.load();
    int capacity = sampleCapacity.load();
    for (int i = 0; i < numSamples; i++) {
        // the Codec Zero's mic can arrive on either channel: use the louder
        float s = 0.0f;
        for (int c = 0; c < juce::jmin(2, numInputs); c++)
            if (input[c] != nullptr && std::abs(input[c][i]) > std::abs(s))
                s = input[c][i];
        peak = juce::jmax(peak, std::abs(s));
        if (write && pos < capacity)
            sampleBuffer[size_t(pos++)] = s;
    }
    if (write) {
        samplePosition = pos;
        if (pos >= capacity)
            capturing = false;
    }
    // a meter that falls back slowly
    float old = level.load();
    level = juce::jmax(peak, old * 0.9f);
}
