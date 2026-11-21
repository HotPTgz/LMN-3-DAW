// Plays real edits through the engine, block by block, the way the app does,
// to catch things that only go wrong in playback: notes lost when the song
// starts or loops, notes left hanging after Stop, plugins that go silent or
// blow up (NaN) once they're part of a track.
#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {
using namespace internal_plugins;

namespace {

// One audio thread: the test drives the audio callback itself, and many
// engines come and go in one test run
struct SingleThreadedBehaviour : tracktion::EngineBehaviour {
    int getNumberOfCPUsToUseForAudio() override { return 1; }
};

// Sits between the MIDI effects and the instrument and watches the notes:
// remembers the longest time any note was held
class MidiProbePlugin : public tracktion::Plugin {
  public:
    explicit MidiProbePlugin(tracktion::PluginCreationInfo info) : Plugin(info) {}
    ~MidiProbePlugin() override { notifyListenersOfDeletion(); }
    static const char *getPluginName() { return "Probe"; }
    static const char *xmlTypeName;
    juce::String getName() override { return "Probe"; }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }
    bool takesMidiInput() override { return true; }
    bool takesAudioInput() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return false; }
    bool needsConstantBufferSize() override { return false; }
    void initialise(const tracktion::PluginInitialisationInfo &i) override {
        sampleRate = i.sampleRate;
    }
    void deinitialise() override {}
    void applyToBuffer(const tracktion::PluginRenderContext &fc) override {
        const double blockStart = samples / sampleRate;
        if (fc.bufferForMidiMessages != nullptr) {
            if (fc.bufferForMidiMessages->isAllNotesOff)
                onSince.fill(-1.0);
            for (auto &m : *fc.bufferForMidiMessages) {
                if (m.isAllNotesOff() || m.isAllSoundOff())
                    onSince.fill(-1.0);
                if (!m.isNoteOnOrOff())
                    continue;
                auto &since = onSince[size_t(m.getNoteNumber())];
                if (m.isNoteOn()) {
                    if (since < 0.0)
                        since = blockStart + m.getTimeStamp();
                } else
                    since = -1.0;
            }
        }
        samples += fc.bufferNumSamples;
        const double now = samples / sampleRate;
        for (int n = 0; n < 128; n++)
            if (onSince[size_t(n)] >= 0.0 && now - onSince[size_t(n)] > longest) {
                longest = now - onSince[size_t(n)];
                longestNote = n;
            }
    }
    std::array<double, 128> onSince = [] {
        std::array<double, 128> a;
        a.fill(-1.0);
        return a;
    }();
    std::atomic<double> longest{0.0};
    int longestNote = -1;
    double sampleRate = 44100.0;
    juce::int64 samples = 0;
};
const char *MidiProbePlugin::xmlTypeName = "lmn3TestProbe";

class PlaybackHarness {
  public:
    explicit PlaybackHarness(double rate = 44100.0, int block = 256)
        : sampleRate(rate), blockSize(block) {
        auto &pm = engine.getPluginManager();
        pm.createBuiltInType<DrumSamplerPlugin>();
        pm.createBuiltInType<ArpeggiatorPlugin>();
        pm.createBuiltInType<ScaleLockPlugin>();
        pm.createBuiltInType<ChordPlugin>();
        pm.createBuiltInType<NoteRepeatPlugin>();
        pm.createBuiltInType<StrumPlugin>();
        pm.createBuiltInType<HumanisePlugin>();
        pm.createBuiltInType<MidiEchoPlugin>();
        pm.createBuiltInType<FingerPlugin>();
        pm.createBuiltInType<TapePlugin>();
        pm.createBuiltInType<LofiPlugin>();
        pm.createBuiltInType<DrivePlugin>();
        pm.createBuiltInType<ResonantFilterPlugin>();
        pm.createBuiltInType<DuckerPlugin>();
        pm.createBuiltInType<BeatRepeatPlugin>();
        pm.createBuiltInType<WavetableEnginePlugin>();
        pm.createBuiltInType<MalletsEnginePlugin>();
        pm.createBuiltInType<GranularEnginePlugin>();
        pm.createBuiltInType<DrumSynthPlugin>();
        pm.createBuiltInType<StringEnginePlugin>();
        pm.createBuiltInType<FmEnginePlugin>();
        pm.createBuiltInType<PulseEnginePlugin>();
        pm.createBuiltInType<MidiProbePlugin>();

        tracktion::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = sampleRate;
        p.blockSize = blockSize;
        p.fixedBlockSize = true;
        // (this engine version passes min(channels, inputs) as the number
        // of outputs, so keep the default 2 inputs)
        engine.getDeviceManager().getHostedAudioDeviceInterface().initialise(p);

        edit = tracktion::Edit::createSingleTrackEdit(engine);
        edit->tempoSequence.getTempos()[0]->setBpm(120.0);
        edit->getTransport().ensureContextAllocated();
        pump();
    }

    tracktion::AudioTrack &track(int index = 0) {
        edit->ensureNumberOfAudioTracks(index + 1);
        return *tracktion::getAudioTracks(*edit)[index];
    }

    tracktion::Plugin *add(const juce::String &type, int trackIndex = 0) {
        auto plugin = edit->getPluginCache().createNewPlugin(type, {});
        auto &list = track(trackIndex).pluginList;
        // in front of the track's own volume and meter, like the app
        int index = 0;
        while (index < list.size() &&
               list[index]->getPluginType() != "volume" &&
               list[index]->getPluginType() != "level")
            index++;
        list.insertPlugin(plugin, index, nullptr);
        pump();
        return plugin.get();
    }

    // notes: {pitch, start beat, length in beats}
    void addNotes(std::initializer_list<std::array<double, 3>> notes,
                  double clipBeats, int trackIndex = 0) {
        auto &t = track(trackIndex);
        auto range = tracktion::TimeRange(
            tracktion::TimePosition(),
            tracktion::TimeDuration::fromSeconds(clipBeats / 2.0));
        auto clip = t.insertMIDIClip(range, nullptr);
        for (auto &n : notes)
            clip->getSequence().addNote(
                int(n[0]), tracktion::BeatPosition::fromBeats(n[1]),
                tracktion::BeatDuration::fromBeats(n[2]), 100, 0, nullptr);
        pump();
    }

    void loop(double fromBeat, double toBeat) {
        auto &transport = edit->getTransport();
        transport.setLoopRange(tracktion::TimeRange(
            tracktion::TimePosition::fromSeconds(fromBeat / 2.0),
            tracktion::TimePosition::fromSeconds(toBeat / 2.0)));
        transport.looping = true;
        pump();
    }

    void play() {
        auto &transport = edit->getTransport();
        transport.setPosition(tracktion::TimePosition());
        transport.play(false);
        pump();
    }

    void stop() {
        edit->getTransport().stop(false, false);
        pump();
    }

    // Sends what's played on the keys to a track, like the app does for the
    // selected track
    void routeKeysTo(int trackIndex) {
        auto &dm = engine.getDeviceManager();
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
            if (auto in = dm.getMidiInDevice(i)) {
                in->setEndToEndEnabled(true);
                in->setEnabled(true);
            }
        // the input devices were enabled after the edit started playing
        edit->getTransport().freePlaybackContext();
        edit->getTransport().ensureContextAllocated();
        pump();
        for (auto instance : edit->getAllInputDevices())
            if (instance->getInputDevice().isMidi()) {
                instance->setTargetTrack(track(trackIndex), 0, true);
                instance->setRecordingEnabled(track(trackIndex), true);
            }
        edit->restartPlayback();
        pump();
    }

    // keys to play during the next run(): seconds from its start, message
    std::vector<std::pair<double, juce::MidiMessage>> keys;

    // Runs `seconds` of audio, returning the peak of each 50 ms
    std::vector<float> run(double seconds) {
        std::vector<float> peaks;
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        const int blocksPerSlice =
            juce::jmax(1, int(0.05 * sampleRate / blockSize));
        int numBlocks = int(seconds * sampleRate / blockSize);
        float peak = 0.0f;
        for (int b = 0; b < numBlocks; b++) {
            buffer.clear();
            midi.clear();
            const double blockStart = b * blockSize / sampleRate;
            for (auto &k : keys) {
                int pos = juce::roundToInt((k.first - blockStart) * sampleRate);
                if (pos >= 0 && pos < blockSize)
                    midi.addEvent(k.second, pos);
            }
            engine.getDeviceManager().getHostedAudioDeviceInterface().processBlock(
                buffer, midi);
            if (b % 8 == 0)
                pump(1);
            for (int c = 0; c < 2; c++)
                for (int i = 0; i < blockSize; i++) {
                    float s = buffer.getSample(c, i);
                    if (!std::isfinite(s))
                        nonFinite = true;
                    else
                        peak = juce::jmax(peak, std::abs(s));
                }
            if ((b + 1) % blocksPerSlice == 0) {
                peaks.push_back(peak);
                peak = 0.0f;
            }
        }
        keys.clear();
        return peaks;
    }

    static float maxOf(const std::vector<float> &peaks, size_t from,
                       size_t to) {
        float m = 0.0f;
        for (size_t i = from; i < juce::jmin(to, peaks.size()); i++)
            m = juce::jmax(m, peaks[i]);
        return m;
    }

    void pump(int ms = 20) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
    }

    std::vector<int> clipNotes(int trackIndex) {
        std::vector<int> notes;
        for (auto clip : track(trackIndex).getClips())
            if (auto midiClip = dynamic_cast<tracktion::MidiClip *>(clip))
                for (auto n : midiClip->getSequence().getNotes())
                    notes.push_back(n->getNoteNumber());
        return notes;
    }

    // Like picking a new sample rate or buffer size in Settings while the
    // song plays
    void switchDevice(double rate, int block) {
        sampleRate = rate;
        blockSize = block;
        const bool wasPlaying = edit->getTransport().isPlaying();
        tracktion::HostedAudioDeviceInterface::Parameters p;
        p.sampleRate = rate;
        p.blockSize = block;
        p.fixedBlockSize = true;
        engine.getDeviceManager().getHostedAudioDeviceInterface().initialise(p);
        pump(50);
        // restarting the device stops the song, as it does in the app: play
        // on
        if (wasPlaying)
            edit->getTransport().play(false);
        pump();
    }

    double sampleRate;
    int blockSize;
    tracktion::Engine engine{"ENGINE", nullptr,
                             std::make_unique<SingleThreadedBehaviour>()};
    std::unique_ptr<tracktion::Edit> edit;
    bool nonFinite = false;
};
} // namespace

//==============================================================================
// Every instrument sounds when its clip plays, and goes quiet afterwards
class InstrumentPlaybackTest : public ::testing::TestWithParam<const char *> {};

TEST_P(InstrumentPlaybackTest, soundsAndStops) {
    PlaybackHarness h;
    h.add(GetParam());
    h.addNotes({{{53, 0.0, 1.0}}, {{60, 0.0, 1.0}}}, 4.0); // 2 s clip
    h.play();
    auto peaks = h.run(10.0);
    EXPECT_FALSE(h.nonFinite);
    EXPECT_GT(PlaybackHarness::maxOf(peaks, 0, 10), 0.001f)
        << "silent while its notes play";
    EXPECT_LT(PlaybackHarness::maxOf(peaks, 180, 200), 0.001f)
        << "still sounding 9 s after its notes ended";
}

// Android phones usually run at 48 kHz
TEST_P(InstrumentPlaybackTest, soundsAndStopsAt48k) {
    PlaybackHarness h(48000.0);
    h.add(GetParam());
    h.addNotes({{{53, 0.0, 1.0}}, {{60, 0.0, 1.0}}}, 4.0);
    h.play();
    auto peaks = h.run(10.0);
    EXPECT_FALSE(h.nonFinite);
    EXPECT_GT(PlaybackHarness::maxOf(peaks, 0, 10), 0.001f);
    EXPECT_LT(PlaybackHarness::maxOf(peaks, 180, 200), 0.001f);
}

INSTANTIATE_TEST_SUITE_P(
    All, InstrumentPlaybackTest,
    ::testing::Values(StringEnginePlugin::xmlTypeName,
                      FmEnginePlugin::xmlTypeName,
                      PulseEnginePlugin::xmlTypeName,
                      WavetableEnginePlugin::xmlTypeName,
                      MalletsEnginePlugin::xmlTypeName,
                      GranularEnginePlugin::xmlTypeName,
                      DrumSynthPlugin::xmlTypeName, "4osc"));

//==============================================================================
// Every MIDI effect in front of an instrument: a recorded chord that starts
// right at the beginning plays back, and Stop leaves nothing hanging
class MidiFxPlaybackTest : public ::testing::TestWithParam<const char *> {};

TEST_P(MidiFxPlaybackTest, recordedChordPlaysAndStopSilences) {
    PlaybackHarness h;
    h.add(GetParam());
    h.add(PulseEnginePlugin::xmlTypeName);
    // a chord held for the whole 4-beat clip
    h.addNotes({{{60, 0.0, 4.0}}, {{64, 0.0, 4.0}}, {{67, 0.0, 4.0}}}, 4.0);
    h.play();
    auto playing = h.run(1.0);
    EXPECT_FALSE(h.nonFinite);
    EXPECT_GT(PlaybackHarness::maxOf(playing, 0, 20), 0.001f)
        << "the recorded chord is silent on playback";

    h.stop();
    auto stopped = h.run(4.0);
    EXPECT_LT(PlaybackHarness::maxOf(stopped, 60, 80), 0.001f)
        << "still sounding 3 s after Stop";
}

INSTANTIATE_TEST_SUITE_P(All, MidiFxPlaybackTest,
                         ::testing::Values(ArpeggiatorPlugin::xmlTypeName,
                                           ScaleLockPlugin::xmlTypeName,
                                           ChordPlugin::xmlTypeName,
                                           NoteRepeatPlugin::xmlTypeName,
                                           StrumPlugin::xmlTypeName,
                                           HumanisePlugin::xmlTypeName,
                                           MidiEchoPlugin::xmlTypeName));

TEST(LoopPlaybackTest, arpeggioPlaysOnEveryPassOfTheLoop) {
    PlaybackHarness h;
    h.add(ArpeggiatorPlugin::xmlTypeName);
    h.add(PulseEnginePlugin::xmlTypeName);
    h.addNotes({{{60, 0.0, 1.0}}, {{64, 0.0, 1.0}}}, 4.0);
    h.loop(0.0, 4.0); // 2 s
    h.play();
    auto peaks = h.run(6.5);
    for (int pass = 0; pass < 3; pass++)
        EXPECT_GT(PlaybackHarness::maxOf(peaks, size_t(pass * 40),
                                         size_t(pass * 40 + 10)),
                  0.001f)
            << "pass " << pass;
}

TEST(FingerPlaybackTest, recordedPatternKeyPlaysTheDrums) {
    PlaybackHarness h;
    h.add(FingerPlugin::xmlTypeName);
    h.add(DrumSynthPlugin::xmlTypeName);
    h.addNotes({{{54, 0.0, 4.0}}}, 4.0); // F#3 = the first pattern key
    h.play();
    auto playing = h.run(2.0);
    EXPECT_GT(PlaybackHarness::maxOf(playing, 0, 40), 0.001f);
    h.stop();
    auto stopped = h.run(3.0);
    EXPECT_LT(PlaybackHarness::maxOf(stopped, 40, 60), 0.001f);
}

TEST(FingerKitTest, followsTheKitWhenItChanges) {
    PlaybackHarness h;
    auto finger = dynamic_cast<FingerPlugin *>(h.add(FingerPlugin::xmlTypeName));
    auto sampler =
        dynamic_cast<DrumSamplerPlugin *>(h.add(DrumSamplerPlugin::xmlTypeName));
    ASSERT_NE(finger, nullptr);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(finger->getKitNotes()[0], 53); // nothing named yet

    // a kit loads after Finger was set up: the kick is on G3
    sampler->addSound("", "kick1", 0.0, 1.0, 0.0f);
    sampler->setSoundParams(0, 55, 55, 55);
    h.pump();
    EXPECT_EQ(finger->getKitNotes()[0], 55);

    // and moves when the kit changes again
    sampler->setSoundParams(0, 60, 60, 60);
    h.pump();
    EXPECT_EQ(finger->getKitNotes()[0], 60);
}

TEST(DuckerPlaybackTest, ducksWhenAnotherTrackHits) {
    PlaybackHarness h;
    // track 1: a kick on every beat; track 2: a held chord with a Ducker
    h.add(DrumSynthPlugin::xmlTypeName, 0);
    h.addNotes({{{53, 0.0, 0.25}}, {{53, 1.0, 0.25}}, {{53, 2.0, 0.25}},
                {{53, 3.0, 0.25}}},
               4.0, 0);
    h.add(PulseEnginePlugin::xmlTypeName, 1);
    auto ducker = dynamic_cast<DuckerPlugin *>(h.add(DuckerPlugin::xmlTypeName, 1));
    ASSERT_NE(ducker, nullptr);
    h.addNotes({{{48, 0.0, 4.0}}}, 4.0, 1);

    auto names = ducker->getSourceNames();
    int kickTrack = -1;
    for (int i = 1; i < names.size(); i++)
        if (names[i].contains(h.track(0).getName()))
            kickTrack = i;
    ASSERT_GT(kickTrack, 0) << names.joinIntoString(", ").toStdString();
    ducker->setSourceIndex(kickTrack);
    h.pump();
    EXPECT_EQ(ducker->getSourceIndex(), kickTrack);

    h.play();
    float lowest = 1.0f;
    for (int i = 0; i < 30; i++) {
        h.run(0.05);
        lowest = juce::jmin(lowest, ducker->liveGain.load());
    }
    EXPECT_FALSE(h.nonFinite);
    EXPECT_GT(ducker->hitCount.load(), 0) << "never heard the kick track";
    EXPECT_LT(lowest, 0.6f) << "never ducked";
}

TEST(BeatRepeatPlaybackTest, holdRepeatsAndLetGoStops) {
    PlaybackHarness h;
    h.add(PulseEnginePlugin::xmlTypeName);
    auto repeat =
        dynamic_cast<BeatRepeatPlugin *>(h.add(BeatRepeatPlugin::xmlTypeName));
    ASSERT_NE(repeat, nullptr);
    h.addNotes({{{48, 0.0, 0.5}}, {{55, 1.0, 0.5}}, {{60, 2.0, 0.5}}}, 4.0);
    h.play();
    h.run(0.3);
    repeat->setActive(true);
    bool repeated = false;
    for (int i = 0; i < 20 && !repeated; i++) {
        h.run(0.05);
        repeated = repeat->liveRepeat.load() >= 0;
    }
    EXPECT_TRUE(repeated) << "never started repeating while held";
    repeat->setActive(false);
    h.run(0.3);
    EXPECT_LT(repeat->liveRepeat.load(), 0) << "kept repeating after let go";
    EXPECT_FALSE(h.nonFinite);
}

//==============================================================================
// Every effect passes sound through and stays finite
class EffectPlaybackTest : public ::testing::TestWithParam<const char *> {};

TEST_P(EffectPlaybackTest, passesSoundAndStaysFinite) {
    PlaybackHarness h;
    h.add(PulseEnginePlugin::xmlTypeName);
    h.add(GetParam());
    h.addNotes({{{48, 0.0, 4.0}}, {{55, 0.0, 4.0}}}, 4.0);
    h.play();
    auto peaks = h.run(2.0);
    EXPECT_FALSE(h.nonFinite);
    EXPECT_GT(PlaybackHarness::maxOf(peaks, 0, 40), 0.001f);
    EXPECT_LT(PlaybackHarness::maxOf(peaks, 0, 40), 8.0f);
}

INSTANTIATE_TEST_SUITE_P(All, EffectPlaybackTest,
                         ::testing::Values(TapePlugin::xmlTypeName,
                                           LofiPlugin::xmlTypeName,
                                           DrivePlugin::xmlTypeName,
                                           ResonantFilterPlugin::xmlTypeName,
                                           DuckerPlugin::xmlTypeName,
                                           BeatRepeatPlugin::xmlTypeName));

//==============================================================================
// Rendering a song to a WAV goes through the engine offline: recorded parts
// must come out through their MIDI effects there too
class MidiFxRenderTest : public ::testing::TestWithParam<const char *> {};

TEST_P(MidiFxRenderTest, recordedChordIsInTheRender) {
    PlaybackHarness h;
    if (juce::String(GetParam()).isNotEmpty())
        h.add(GetParam());
    h.add(PulseEnginePlugin::xmlTypeName);
    h.addNotes({{{60, 0.0, 4.0}}, {{64, 0.0, 4.0}}}, 4.0);
    h.edit->getTransport().stop(false, false);

    auto file = juce::File::createTempFile(".wav");
    juce::BigInteger tracks;
    tracks.setBit(0);
    bool ok = tracktion::Renderer::renderToFile(
        "Render", file, *h.edit,
        tracktion::TimeRange(tracktion::TimePosition(),
                             tracktion::TimePosition::fromSeconds(2.0)),
        tracks, true, {}, false);
    ASSERT_TRUE(ok);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formats.createReaderFor(file));
    ASSERT_NE(reader, nullptr);
    juce::AudioBuffer<float> audio(int(reader->numChannels),
                                   int(reader->lengthInSamples));
    reader->read(&audio, 0, audio.getNumSamples(), 0, true, true);
    // the first half second
    EXPECT_GT(audio.getMagnitude(0, juce::jmin(audio.getNumSamples(), 22050)),
              0.001f);
    reader.reset();
    file.deleteFile();
}

INSTANTIATE_TEST_SUITE_P(All, MidiFxRenderTest,
                         ::testing::Values("", ArpeggiatorPlugin::xmlTypeName,
                                           NoteRepeatPlugin::xmlTypeName,
                                           StrumPlugin::xmlTypeName,
                                           HumanisePlugin::xmlTypeName,
                                           MidiEchoPlugin::xmlTypeName));

//==============================================================================
// Stress: every plugin with its knobs thrown around at random while notes
// play, at odd block sizes and sample rates. Nothing may blow up (NaN, huge
// levels) or crash.
namespace {
struct StressCase {
    const char *type;
    bool isInstrument;
};
const StressCase stressCases[] = {
    {StringEnginePlugin::xmlTypeName, true},
    {FmEnginePlugin::xmlTypeName, true},
    {PulseEnginePlugin::xmlTypeName, true},
    {WavetableEnginePlugin::xmlTypeName, true},
    {MalletsEnginePlugin::xmlTypeName, true},
    {GranularEnginePlugin::xmlTypeName, true},
    {DrumSynthPlugin::xmlTypeName, true},
    {TapePlugin::xmlTypeName, false},
    {LofiPlugin::xmlTypeName, false},
    {DrivePlugin::xmlTypeName, false},
    {ResonantFilterPlugin::xmlTypeName, false},
    {DuckerPlugin::xmlTypeName, false},
    {BeatRepeatPlugin::xmlTypeName, false},
    {ArpeggiatorPlugin::xmlTypeName, false},
    {ScaleLockPlugin::xmlTypeName, false},
    {ChordPlugin::xmlTypeName, false},
    {NoteRepeatPlugin::xmlTypeName, false},
    {StrumPlugin::xmlTypeName, false},
    {HumanisePlugin::xmlTypeName, false},
    {MidiEchoPlugin::xmlTypeName, false},
    {FingerPlugin::xmlTypeName, false},
};
struct StressConfig {
    double rate;
    int block;
};
const StressConfig stressConfigs[] = {
    {44100.0, 256}, {48000.0, 17}, {96000.0, 1024}, {22050.0, 64}};
} // namespace

class StressTest
    : public ::testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(StressTest, randomKnobsNeverBlowUp) {
    auto c = stressCases[std::get<0>(GetParam())];
    auto config = stressConfigs[std::get<1>(GetParam())];
    PlaybackHarness h(config.rate, config.block);
    auto plugin = h.add(c.type);
    if (!c.isInstrument)
        h.add(PulseEnginePlugin::xmlTypeName);
    if (juce::String(c.type) == MidiEchoPlugin::xmlTypeName ||
        juce::String(c.type) == ArpeggiatorPlugin::xmlTypeName)
        ; // MIDI effects go before the instrument: add() keeps the order
    // dense notes and chords, high and low
    h.addNotes({{{36, 0.0, 0.5}}, {{48, 0.5, 0.25}}, {{60, 1.0, 2.0}},
                {{64, 1.0, 2.0}}, {{67, 1.0, 2.0}}, {{96, 2.0, 0.1}},
                {{100, 2.1, 0.1}}, {{24, 3.0, 1.0}}, {{54, 0.0, 4.0}}},
               4.0);
    h.loop(0.0, 4.0);
    h.play();

    juce::Random random(42);
    auto params = plugin->getAutomatableParameters();
    float loudest = 0.0f;
    for (int round = 0; round < 40; round++) {
        for (auto p : params)
            if (p->paramID != "active" || round % 3 == 0)
                p->setNormalisedParameter(random.nextFloat(),
                                          juce::sendNotificationSync);
        if (auto rep = dynamic_cast<BeatRepeatPlugin *>(plugin))
            rep->setActive(round % 4 != 3);
        auto peaks = h.run(0.1);
        loudest = juce::jmax(loudest, PlaybackHarness::maxOf(peaks, 0, 100));
    }
    // and every knob at its ends
    for (float end : {0.0f, 1.0f}) {
        for (auto p : params)
            p->setNormalisedParameter(end, juce::sendNotificationSync);
        auto peaks = h.run(0.5);
        loudest = juce::jmax(loudest, PlaybackHarness::maxOf(peaks, 0, 100));
    }
    h.stop();
    h.run(0.2);
    EXPECT_FALSE(h.nonFinite) << c.type << " made NaN or infinity";
    EXPECT_LT(loudest, 16.0f) << c.type << " got far too loud";
}

INSTANTIATE_TEST_SUITE_P(
    All, StressTest,
    ::testing::Combine(::testing::Range(0, int(std::size(stressCases))),
                       ::testing::Range(0, int(std::size(stressConfigs)))),
    [](const ::testing::TestParamInfo<std::tuple<int, int>> &info) {
        auto c = stressCases[std::get<0>(info.param)];
        auto config = stressConfigs[std::get<1>(info.param)];
        return std::string(c.type) + "_" +
               std::to_string(int(config.rate)) + "_" +
               std::to_string(config.block);
    });

//==============================================================================
// Modifiers: an LFO, Step, Random, Envelope or MIDI tracker on every knob of
// every plugin, while notes play. Modulated knobs change on the audio thread.
// Then save and reload (the modifiers must still be attached), and delete the
// plugin while it plays with its modifiers still on.
namespace {
const char *const modifierTypes[] = {"LFO", "STEP", "RANDOM",
                                     "ENVELOPEFOLLOWER", "MIDITRACKER"};
} // namespace

class ModifierTest : public ::testing::TestWithParam<std::tuple<int, int>> {};

TEST_P(ModifierTest, modulatedKnobsPlaySaveAndDelete) {
    auto c = stressCases[std::get<0>(GetParam())];
    const juce::Identifier modifierType = modifierTypes[std::get<1>(GetParam())];
    auto file = juce::File::createTempFile(".tracktionedit");
    int attached = 0;
    {
        PlaybackHarness h(48000.0, 200);
        auto plugin = h.add(c.type);
        if (!c.isInstrument)
            h.add(PulseEnginePlugin::xmlTypeName);
        h.addNotes({{{48, 0.0, 0.5}}, {{60, 1.0, 1.0}}, {{64, 1.0, 1.0}},
                    {{55, 2.0, 2.0}}, {{72, 3.5, 0.25}}},
                   4.0);
        auto &track = h.track();
        for (auto p : plugin->getAutomatableParameters()) {
            auto modifier = track.getModifierList().insertModifier(
                juce::ValueTree(modifierType), -1, nullptr);
            ASSERT_NE(modifier, nullptr);
            app_view_models::applyModifierDefaults(modifier.get());
            // full depth, so knobs swing over their whole range
            for (auto mp : modifier->getAutomatableParameters())
                if (mp->paramID.containsIgnoreCase("depth"))
                    mp->setParameter(mp->valueRange.end, juce::sendNotificationSync);
            p->addModifier(*modifier);
            attached++;
        }
        h.loop(0.0, 4.0);
        h.play();
        auto peaks = h.run(4.0);
        EXPECT_FALSE(h.nonFinite) << c.type << " made NaN with " << modifierType.toString();
        EXPECT_LT(PlaybackHarness::maxOf(peaks, 0, peaks.size()), 16.0f);
        h.stop();
        h.edit->editFileRetriever = [file] { return file; };
        ASSERT_TRUE(
            tracktion::EditFileOperations(*h.edit).save(false, true, false));

        // delete the plugin while playing, modifiers still attached
        h.play();
        h.run(0.3);
        plugin->deleteFromParent();
        h.pump(50);
        h.run(0.5);
        EXPECT_FALSE(h.nonFinite);
        // and the modifiers too
        for (auto m : track.getModifierList().getModifiers())
            m->remove();
        h.pump(50);
        h.run(0.3);
    }
    PlaybackHarness h2;
    auto loaded = tracktion::loadEditFromFile(h2.engine, file);
    ASSERT_NE(loaded, nullptr);
    int found = 0;
    for (auto t : tracktion::getAudioTracks(*loaded))
        for (auto p : t->pluginList)
            if (p->getPluginType() == juce::String(c.type))
                for (auto param : p->getAutomatableParameters())
                    found += param->getModifiers().size();
    EXPECT_EQ(found, attached) << c.type << ": modifiers lost after reload";
    loaded.reset();
    file.deleteFile();
}

INSTANTIATE_TEST_SUITE_P(
    All, ModifierTest,
    ::testing::Combine(::testing::Range(0, int(std::size(stressCases))),
                       ::testing::Range(0, int(std::size(modifierTypes)))),
    [](const ::testing::TestParamInfo<std::tuple<int, int>> &info) {
        return std::string(stressCases[std::get<0>(info.param)].type) + "_" +
               modifierTypes[std::get<1>(info.param)];
    });

//==============================================================================
// Changing the sample rate and buffer size while the song plays, the way the
// Settings screen does: every plugin must keep playing, in tune and in time,
// without reading past buffers sized for the old settings.
class DeviceSwitchTest : public ::testing::TestWithParam<int> {};

TEST_P(DeviceSwitchTest, keepsPlayingAcrossRateAndBufferChanges) {
    auto c = stressCases[GetParam()];
    PlaybackHarness h(44100.0, 256);
    auto plugin = h.add(c.type);
    if (!c.isInstrument)
        h.add(PulseEnginePlugin::xmlTypeName);
    if (auto rep = dynamic_cast<BeatRepeatPlugin *>(plugin))
        rep->setActive(true);
    h.addNotes({{{48, 0.0, 0.5}}, {{60, 0.5, 1.0}}, {{64, 0.5, 1.0}},
                {{55, 1.5, 0.5}}, {{54, 0.0, 2.0}}, {{57, 1.0, 1.0}}},
               2.0);
    h.loop(0.0, 2.0);
    h.play();
    h.run(1.0);
    const std::pair<double, int> configs[] = {
        {96000.0, 1024}, {22050.0, 64}, {48000.0, 17}, {192000.0, 512},
        {44100.0, 2048}, {8000.0, 32},  {48000.0, 256}};
    for (auto [rate, block] : configs) {
        h.switchDevice(rate, block);
        auto peaks = h.run(1.2);
        EXPECT_FALSE(h.nonFinite) << c.type << " at " << rate << "/" << block;
        EXPECT_LT(PlaybackHarness::maxOf(peaks, 0, peaks.size()), 16.0f)
            << c.type << " at " << rate << "/" << block;
        if (juce::String(c.type) != DuckerPlugin::xmlTypeName)
            EXPECT_GT(PlaybackHarness::maxOf(peaks, 4, peaks.size()), 1.0e-4f)
                << c.type << " went silent at " << rate << "/" << block;
    }
}

INSTANTIATE_TEST_SUITE_P(
    All, DeviceSwitchTest, ::testing::Range(0, int(std::size(stressCases))),
    [](const ::testing::TestParamInfo<int> &info) {
        return std::string(stressCases[info.param].type);
    });

// Every plugin in a WAV render (Control + Save) sounds like it does when the
// song plays: not silent, no NaN, about as loud. Tempo-synced effects (Beat
// Repeat, Ducker, Note Repeat, Finger...) must follow the song in a render
// too, where no one presses Play.
class RenderEveryPluginTest : public ::testing::TestWithParam<int> {};

TEST_P(RenderEveryPluginTest, renderSoundsLikePlayback) {
    auto c = stressCases[GetParam()];
    PlaybackHarness h;
    auto plugin = h.add(c.type);
    if (!c.isInstrument)
        h.add(PulseEnginePlugin::xmlTypeName);
    if (auto rep = dynamic_cast<BeatRepeatPlugin *>(plugin))
        rep->setActive(true);
    h.addNotes({{{48, 0.0, 1.0}}, {{60, 0.0, 4.0}}, {{64, 1.0, 3.0}},
                {{54, 0.0, 4.0}}, {{57, 2.0, 2.0}}, {{67, 3.0, 1.0}}},
               4.0);
    // live first
    h.play();
    auto live = h.run(2.0);
    h.stop();
    h.run(0.3);

    auto file = juce::File::createTempFile(".wav");
    juce::BigInteger tracks;
    tracks.setBit(0);
    ASSERT_TRUE(tracktion::Renderer::renderToFile(
        "Render", file, *h.edit,
        tracktion::TimeRange(tracktion::TimePosition(),
                             tracktion::TimePosition::fromSeconds(2.0)),
        tracks, true, {}, false));
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    ASSERT_NE(reader, nullptr);
    juce::AudioBuffer<float> audio(int(reader->numChannels),
                                   int(reader->lengthInSamples));
    reader->read(&audio, 0, audio.getNumSamples(), 0, true, true);
    reader.reset();
    file.deleteFile();

    const int slice = 2205; // 50 ms at 44.1 kHz, like the live peaks
    std::vector<float> rendered;
    bool finite = true;
    for (int start = 0; start + slice <= audio.getNumSamples(); start += slice) {
        float peak = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ch++)
            for (int i = start; i < start + slice; i++) {
                float v = audio.getSample(ch, i);
                finite = finite && std::isfinite(v);
                peak = juce::jmax(peak, std::abs(v));
            }
        rendered.push_back(peak);
    }
    auto mean = [](const std::vector<float> &v) {
        double sum = 0;
        for (auto x : v)
            sum += x;
        return v.empty() ? 0.0 : sum / double(v.size());
    };
    const double liveMean = mean(live), renderMean = mean(rendered);
    EXPECT_TRUE(finite) << c.type;
    EXPECT_GT(renderMean, 1.0e-4) << c.type << " is silent in the render";
    EXPECT_GT(liveMean, 1.0e-4) << c.type << " is silent live";
    EXPECT_LT(renderMean, liveMean * 3.0 + 1.0e-3)
        << c.type << ": render much louder than playback";
    EXPECT_GT(renderMean, liveMean / 3.0)
        << c.type << ": render much quieter than playback (live " << liveMean
        << ", render " << renderMean << ")";
}

INSTANTIATE_TEST_SUITE_P(
    All, RenderEveryPluginTest, ::testing::Range(0, int(std::size(stressCases))),
    [](const ::testing::TestParamInfo<int> &info) {
        return std::string(stressCases[info.param].type);
    });

//==============================================================================
// Random editing, the way the screens do it, mixed with random undos, some
// while the song plays: add and delete tracks and plugins, move and bypass
// plugins, add modifiers, cut, paste, slice, join, quantize, run sequencers.
// Nothing may crash, and the audio must stay finite.
class UndoFuzzTest : public ::testing::TestWithParam<int> {};

TEST_P(UndoFuzzTest, randomEditsAndUndos) {
    const int seed = GetParam();
    PlaybackHarness h;
    app_services::TimelineCamera camera(7.0);
    app_view_models::TracksListViewModel tracks(*h.edit, camera);
    auto settle = [&] {
        tracks.handleUpdateNowIfNeeded();
        tracks.listViewModel.handleUpdateNowIfNeeded();
        tracks.listViewModel.itemListState.handleUpdateNowIfNeeded();
        h.pump(5);
    };
    settle();
    h.edit->ensureNumberOfAudioTracks(3);
    h.addNotes({{{60, 0.0, 1.0}}, {{64, 1.0, 1.0}}, {{54, 0.0, 4.0}}}, 8.0, 0);
    settle();
    juce::Random random(seed);
    auto selectedTrack = [&]() -> tracktion::AudioTrack * {
        return dynamic_cast<tracktion::AudioTrack *>(
            tracks.listViewModel.getSelectedItem());
    };
    std::vector<std::string> log;
    for (int step = 0; step < 250; step++) {
        int op = random.nextInt(22);
        log.push_back(std::to_string(op));
        h.edit->getUndoManager().beginNewTransaction();
        auto track = selectedTrack();
        switch (op) {
        case 0:
            tracks.addTrack();
            break;
        case 1:
            if (tracktion::getAudioTracks(*h.edit).size() > 1)
                tracks.deleteSelectedTrack();
            break;
        case 2:
        case 3: {
            int n = tracks.listViewModel.itemListState.listSize;
            if (n > 0)
                tracks.listViewModel.itemListState.setSelectedItemIndex(
                    random.nextInt(n));
            break;
        }
        case 4:
            tracks.cutSelectedTracksClipAtPlayHead();
            break;
        case 5:
            tracks.pasteClipboardContentToTrackAtPlayhead();
            break;
        case 6:
            tracks.splitSelectedTracksClipAtPlayHead();
            break;
        case 7:
            tracks.mergeSelectedTracksClipsAtPlayhead();
            break;
        case 8:
            tracks.quantizeSelectedTracksClipAtPlayhead();
            break;
        case 9:
            for (int k = random.nextInt(8); k-- > 0;)
                random.nextBool() ? tracks.nudgeTransportForwardToNearestBeat()
                                  : tracks.nudgeTransportBackwardToNearestBeat();
            break;
        case 10:
        case 11:
            if (track != nullptr) {
                app_view_models::AvailablePluginsViewModel available(track);
                available.setSelectedCategoryIndex(random.nextInt(3));
                auto names = available.getPluginNames();
                if (names.size() > 0) {
                    available.setSelectedPluginIndex(random.nextInt(names.size()));
                    available.addSelectedPluginToTrack();
                }
            }
            break;
        case 12:
        case 13:
            if (track != nullptr) {
                app_view_models::TrackPluginsListViewModel list(track);
                int n = list.listViewModel.itemListState.listSize;
                if (n > 0)
                    list.listViewModel.itemListState.setSelectedItemIndex(
                        random.nextInt(n));
                switch (random.nextInt(4)) {
                case 0:
                    list.deleteSelectedPlugin();
                    break;
                case 1:
                    list.moveSelectedPluginUp();
                    break;
                case 2:
                    list.moveSelectedPluginDown();
                    break;
                default:
                    list.toggleSelectedPluginEnabled();
                }
            }
            break;
        case 14:
            if (track != nullptr) {
                // a modifier on a random knob of a random plugin
                juce::Array<tracktion::Plugin *> plugins;
                for (auto p : track->pluginList)
                    if (!p->getAutomatableParameters().isEmpty())
                        plugins.add(p);
                if (!plugins.isEmpty()) {
                    auto plugin = plugins[random.nextInt(plugins.size())];
                    auto params = plugin->getAutomatableParameters();
                    auto param = params[random.nextInt(params.size())];
                    const juce::Identifier types[] = {
                        "LFO", "STEP", "RANDOM", "ENVELOPEFOLLOWER", "MIDITRACKER"};
                    auto m = track->getModifierList().insertModifier(
                        juce::ValueTree(types[random.nextInt(5)]), -1,
                        nullptr);
                    if (m != nullptr) {
                        app_view_models::applyModifierDefaults(m.get());
                        param->addModifier(*m);
                    }
                }
            }
            break;
        case 15:
            if (track != nullptr) {
                app_view_models::StepSequencerViewModel stepVm(track);
                for (int k = 0; k < 4; k++) {
                    for (int n = random.nextInt(5); n-- > 0;)
                        stepVm.incrementSelectedNoteIndex();
                    stepVm.toggleNoteNumberAtSelectedIndex(53 + random.nextInt(24));
                }
                stepVm.nudgeRatchet(random.nextInt(3) - 1);
                stepVm.nudgeSwing(random.nextInt(21) - 10);
                if (random.nextBool())
                    stepVm.incrementNumberOfNotes();
            }
            break;
        case 16:
            if (track != nullptr) {
                app_view_models::EuclideanSequencerViewModel e(track);
                e.selectForNote(53 + random.nextInt(8));
                e.nudgeHits(random.nextInt(9) - 2);
                e.nudgeSteps(random.nextInt(9) - 4);
            }
            break;
        case 17:
            tracks.toggleMute();
            break;
        case 18:
            if (h.edit->getTransport().isPlaying())
                h.stop();
            else
                h.play();
            break;
        case 19:
        case 20:
        case 21:
            for (int k = 1 + random.nextInt(3); k-- > 0;)
                tracks.undo();
            break;
        }
        settle();
        h.run(0.03);
        if (std::getenv("LMN3_DEBUG") && step % 50 == 49) {
            int plugins = 0, clips = 0, modifiers = 0;
            for (auto t : tracktion::getAudioTracks(*h.edit)) {
                plugins += t->pluginList.size();
                clips += t->getClips().size();
                modifiers += t->getModifierList().getModifiers().size();
            }
            std::cout << "step " << step << ": tracks "
                      << tracktion::getAudioTracks(*h.edit).size() << " plugins "
                      << plugins << " clips " << clips << " modifiers "
                      << modifiers << "\n";
        }
        ASSERT_FALSE(h.nonFinite) << "after ops " << [&] {
            std::string t;
            for (auto &l : log)
                t += l + " ";
            return t;
        }();
    }
    // and undo everything that's left
    for (int k = 0; k < 600 && h.edit->getUndoManager().canUndo(); k++) {
        tracks.undo();
        if (k % 20 == 0) {
            settle();
            h.run(0.02);
        }
    }
    settle();
    h.run(0.1);
    EXPECT_FALSE(h.nonFinite);
}

INSTANTIATE_TEST_SUITE_P(Seeds, UndoFuzzTest, ::testing::Range(1, 9));

//==============================================================================
// A real project with every plugin, sequencer and modifier, damaged at
// random (junk values, settings missing, unknown plugin types, parts deleted
// or doubled, as an old or newer build, a bad copy or a hand edit might
// leave it), then opened and played. Nothing may crash.
namespace {
void damage(juce::ValueTree v, juce::Random &r, int &budget) {
    if (budget <= 0)
        return;
    for (int i = 0; i < v.getNumProperties() && budget > 0; i++) {
        if (r.nextInt(25) != 0)
            continue;
        budget--;
        auto name = v.getPropertyName(i);
        switch (r.nextInt(7)) {
        case 0:
            v.removeProperty(name, nullptr);
            break;
        case 1:
            v.setProperty(name, "junk", nullptr);
            break;
        case 2:
            v.setProperty(name, -1.0e9, nullptr);
            break;
        case 3:
            v.setProperty(name, 1.0e9, nullptr);
            break;
        case 4:
            v.setProperty(name, "", nullptr);
            break;
        case 5:
            v.setProperty(name, "nan", nullptr);
            break;
        default:
            v.setProperty(name, r.nextInt(200) - 100, nullptr);
        }
    }
    for (int i = v.getNumChildren(); --i >= 0 && budget > 0;) {
        auto child = v.getChild(i);
        int roll = r.nextInt(60);
        if (roll == 0) {
            v.removeChild(i, nullptr);
            budget--;
        } else if (roll == 1) {
            v.addChild(child.createCopy(), i, nullptr);
            budget--;
        } else if (roll == 2 && child.hasType(tracktion::IDs::PLUGIN)) {
            child.setProperty(tracktion::IDs::type, "lmn3FromTheFuture", nullptr);
            budget--;
        } else {
            damage(child, r, budget);
        }
    }
}
} // namespace

class DamagedContentTest : public ::testing::TestWithParam<int> {};

TEST_P(DamagedContentTest, opensAndPlays) {
    static juce::File source = [] {
        auto file = juce::File::createTempFile(".tracktionedit");
        PlaybackHarness h;
        int t = 0;
        for (auto &c : stressCases) {
            auto plugin = h.add(c.type, t % 6);
            auto params = plugin->getAutomatableParameters();
            if (!params.isEmpty()) {
                auto m = h.track(t % 6).getModifierList().insertModifier(
                    juce::ValueTree(modifierTypes[t % 5]), -1, nullptr);
                app_view_models::applyModifierDefaults(m.get());
                params[0]->addModifier(*m);
            }
            t++;
        }
        h.addNotes({{{60, 0.0, 1.0}}, {{54, 0.0, 4.0}}}, 4.0, 0);
        auto t1 = tracktion::getAudioTracks(*h.edit)[1];
        {
            app_view_models::EuclideanSequencerViewModel e(t1);
            e.selectForNote(55);
            e.nudgeHits(3);
        }
        {
            app_view_models::StepSequencerViewModel st(tracktion::getAudioTracks(*h.edit)[2]);
            st.toggleNoteNumberAtSelectedIndex(60);
        }
        {
            app_view_models::TuringSequencerViewModel tu(tracktion::getAudioTracks(*h.edit)[3]);
            tu.reroll();
        }
        h.edit->editFileRetriever = [file] { return file; };
        tracktion::EditFileOperations(*h.edit).save(false, true, false);
        return file;
    }();
    ASSERT_TRUE(source.existsAsFile());
    auto xml = juce::parseXML(source);
    ASSERT_NE(xml, nullptr);
    auto state = juce::ValueTree::fromXml(*xml);
    juce::Random r(GetParam());
    int budget = 5 + r.nextInt(30);
    damage(state, r, budget);
    auto file = juce::File::createTempFile(".tracktionedit");
    if (auto out = state.createXml())
        out->writeTo(file);

    PlaybackHarness h;
    h.edit.reset();
    h.edit = app_services::loadProject(h.engine, file);
    ASSERT_NE(h.edit, nullptr);
    h.edit->getTransport().ensureContextAllocated();
    h.pump();
    // the screens read the tracks and sequencers
    for (auto track : tracktion::getAudioTracks(*h.edit)) {
        app_view_models::TrackPluginsListViewModel list(track);
        app_view_models::EuclideanSequencerViewModel e(track);
        app_view_models::TuringSequencerViewModel tu(track);
        app_view_models::ChordsSequencerViewModel ch(track);
        app_view_models::StepSequencerViewModel st(track);
    }
    h.play();
    h.run(1.0);
    h.stop();
    h.run(0.1);
    EXPECT_FALSE(h.nonFinite) << "seed " << GetParam();
    h.edit.reset();
    file.deleteFile();
}

// (600 seeds were run once; these 40 plus the ones that found the clip and
// modifier crashes stay in the suite)
static std::vector<int> damagedContentSeeds() {
    std::vector<int> seeds;
    for (int i = 1; i <= 40; i++)
        seeds.push_back(i);
    for (int i : {90, 218, 290})
        seeds.push_back(i);
    return seeds;
}
INSTANTIATE_TEST_SUITE_P(Seeds, DamagedContentTest,
                         ::testing::ValuesIn(damagedContentSeeds()));

//==============================================================================
// Save and reload: every plugin keeps every knob, and the sequencers keep
// their settings
class SaveReloadTest : public ::testing::TestWithParam<int> {};

TEST_P(SaveReloadTest, everyKnobComesBack) {
    auto c = stressCases[GetParam()];
    auto file = juce::File::createTempFile(".tracktionedit");
    std::map<juce::String, float> saved;
    {
        PlaybackHarness h;
        auto plugin = h.add(c.type);
        juce::Random random(7);
        for (auto p : plugin->getAutomatableParameters()) {
            p->setNormalisedParameter(random.nextFloat(),
                                      juce::sendNotificationSync);
            saved[p->paramID] = p->getCurrentValue();
        }
        h.pump();
        h.edit->editFileRetriever = [file] { return file; };
        ASSERT_TRUE(tracktion::EditFileOperations(*h.edit).save(false, true,
                                                                  false));
    }
    PlaybackHarness h2;
    auto loaded = tracktion::loadEditFromFile(h2.engine, file);
    ASSERT_NE(loaded, nullptr);
    tracktion::Plugin *found = nullptr;
    for (auto t : tracktion::getAudioTracks(*loaded))
        for (auto p : t->pluginList)
            if (p->getPluginType() == juce::String(c.type))
                found = p;
    if (found == nullptr)
        std::cout << file.loadFileAsString() << std::endl;
    ASSERT_NE(found, nullptr) << c.type << " missing after reload";
    for (auto p : found->getAutomatableParameters()) {
        auto it = saved.find(p->paramID);
        if (it == saved.end())
            continue;
        if (p->paramID == "active")
            continue; // Beat Repeat's hold isn't meant to come back on
        EXPECT_NEAR(p->getCurrentValue(), it->second,
                    1.0e-3f * juce::jmax(1.0f, std::abs(it->second)))
            << c.type << " " << p->paramID;
    }
    if (found == nullptr)
        std::cout << file.loadFileAsString().substring(0, 4000) << std::endl;
    loaded.reset();
    file.deleteFile();
}

INSTANTIATE_TEST_SUITE_P(
    All, SaveReloadTest, ::testing::Range(0, int(std::size(stressCases))),
    [](const ::testing::TestParamInfo<int> &info) {
        return std::string(stressCases[info.param].type);
    });

TEST(SaveReloadSequencersTest, sequencersKeepTheirSettings) {
    auto file = juce::File::createTempFile(".tracktionedit");
    {
        PlaybackHarness h;
        h.add(DrumSynthPlugin::xmlTypeName, 0);
        h.add(PulseEnginePlugin::xmlTypeName, 1);
        h.add(PulseEnginePlugin::xmlTypeName, 2);
        auto t0 = tracktion::getAudioTracks(*h.edit)[0];
        {
            app_view_models::EuclideanSequencerViewModel e(t0);
            e.selectForNote(55);
            e.nudgeHits(3);
            e.nudgeSteps(-4);
            e.nudgeRotate(2);
            e.nudgeVelocity(-2);
            e.nudgeSwing(5);
        }
        auto t1 = tracktion::getAudioTracks(*h.edit)[1];
        {
            app_view_models::TuringSequencerViewModel t(t1);
            t.nudgeLock(-3);
            t.nudgeLength(-5);
            t.nudgeRange(1);
            t.nudgeScale(2);
            t.nudgeRate(1);
            t.nudgeGate(3);
            t.notePressed(52);
            t.reroll();
        }
        {
            app_view_models::ChordsSequencerViewModel ch(t1);
            ch.nudgeKey(5);
            ch.nudgeBars(2);
            ch.nudgeBar(1);
            ch.nudgeChord(2);
            ch.nudgeFollow(1);
        }
        h.edit->editFileRetriever = [file] { return file; };
        ASSERT_TRUE(tracktion::EditFileOperations(*h.edit).save(false, true,
                                                                  false));
    }
    PlaybackHarness h2;
    auto loaded = tracktion::loadEditFromFile(h2.engine, file);
    ASSERT_NE(loaded, nullptr);
    auto tracks = tracktion::getAudioTracks(*loaded);
    ASSERT_GE(tracks.size(), 3);
    {
        app_view_models::EuclideanSequencerViewModel e(tracks[0]);
        auto &ring = e.getRing(2);
        EXPECT_EQ(ring.steps, 12);
        EXPECT_EQ(ring.hits, juce::jmin(12, 3));
        EXPECT_EQ(ring.rotate, 2);
        EXPECT_EQ(e.getVelocity(), 90);
        EXPECT_NEAR(e.getSwing(), 0.25, 1.0e-9);
    }
    {
        app_view_models::TuringSequencerViewModel t(tracks[1]);
        EXPECT_EQ(t.getInput().length, 11);
        EXPECT_EQ(t.getInput().range, 3);
        EXPECT_EQ(t.getInput().root, 52);
        EXPECT_EQ(t.getInput().seed, 2);
        EXPECT_NEAR(t.getInput().lock, 0.70, 1.0e-9);
    }
    {
        app_view_models::ChordsSequencerViewModel ch(tracks[1]);
        EXPECT_EQ(ch.getInput().key, 5);
        EXPECT_EQ(ch.getInput().degrees.size(), 6);
        EXPECT_EQ(ch.getFollowers(), 1);
    }
    loaded.reset();
    file.deleteFile();
}

//==============================================================================
// Playing the keys and recording through MIDI effects
TEST(LivePlayingTest, keysPlayThroughTheArpeggiatorWhileStopped) {
    PlaybackHarness h;
    h.add(ArpeggiatorPlugin::xmlTypeName);
    h.add(PulseEnginePlugin::xmlTypeName);
    h.routeKeysTo(0);
    h.run(0.2);
    h.keys = {{0.1, juce::MidiMessage::noteOn(1, 60, 0.8f)},
              {0.1, juce::MidiMessage::noteOn(1, 64, 0.8f)},
              {1.0, juce::MidiMessage::noteOff(1, 60)},
              {1.0, juce::MidiMessage::noteOff(1, 64)}};
    auto peaks = h.run(2.5);
    EXPECT_GT(PlaybackHarness::maxOf(peaks, 2, 20), 0.001f) << "silent while held";
    EXPECT_LT(PlaybackHarness::maxOf(peaks, 40, 50), 0.001f)
        << "still playing after the keys were let go";
}

// (Recording itself can't be tested here: the engine's stand-in MIDI input
// doesn't record.)
TEST(LivePlayingTest, fingerPlaysWhileAPatternKeyIsHeld) {
    PlaybackHarness h;
    h.add(FingerPlugin::xmlTypeName);
    h.add(DrumSynthPlugin::xmlTypeName);
    h.routeKeysTo(0);
    h.play();
    h.keys = {{0.1, juce::MidiMessage::noteOn(1, 54, 0.8f)},
              {1.6, juce::MidiMessage::noteOff(1, 54)}};
    auto peaks = h.run(6.0);
    EXPECT_GT(PlaybackHarness::maxOf(peaks, 4, 30), 0.001f);
    // the last hits ring out (a kick takes a couple of seconds), but no new
    // ones start
    for (size_t i = 60; i + 1 < peaks.size(); i++)
        EXPECT_LE(peaks[i + 1], peaks[i] * 1.05f + 1.0e-5f)
            << "a new hit " << (i + 1) * 0.05 << " s in, after the key was let go";
}

//==============================================================================
// Stuck notes: hold keys, turn every knob of a MIDI effect while they're
// held (the chord type, the scale, the echo transpose...), let go. Every note
// must still stop. The transport is stopped, so no "all notes off" from Play
// or Stop can hide a note that never got its note-off.
class StuckNoteTest : public ::testing::TestWithParam<std::tuple<int, int>> {};

namespace {
const char *const stuckNoteTypes[] = {
    ArpeggiatorPlugin::xmlTypeName, ScaleLockPlugin::xmlTypeName,
    ChordPlugin::xmlTypeName,       NoteRepeatPlugin::xmlTypeName,
    StrumPlugin::xmlTypeName,       HumanisePlugin::xmlTypeName,
    MidiEchoPlugin::xmlTypeName,    FingerPlugin::xmlTypeName};
} // namespace

TEST_P(StuckNoteTest, turningKnobsWhileHoldingNeverLeavesANoteOn) {
    const juce::String type = stuckNoteTypes[std::get<0>(GetParam())];
    const int seed = std::get<1>(GetParam());
    PlaybackHarness h;
    auto plugin = h.add(type);
    h.add(PulseEnginePlugin::xmlTypeName);
    h.routeKeysTo(0);
    h.run(0.2);

    juce::Random random(seed);
    auto turnKnobs = [&] {
        if (std::getenv("LMN3_NOKNOBS")) return;
        for (auto p : plugin->getAutomatableParameters())
            p->setNormalisedParameter(p->paramID == "latch" ? 0.0f
                                                             : random.nextFloat(),
                                      juce::sendNotificationSync);
    };
    const int held[] = {48, 55, 60, 64, 67};
    for (int n : held)
        h.keys.push_back({0.05 + 0.01 * (n % 3),
                          juce::MidiMessage::noteOn(1, n, 0.8f)});
    h.run(0.4);
    for (int round = 0; round < 6; round++) {
        turnKnobs();
        h.run(0.13);
    }
    for (int n : held)
        h.keys.push_back({0.02 * (n % 4), juce::MidiMessage::noteOff(1, n)});
    turnKnobs();
    h.run(0.2);
    turnKnobs();
    // long enough for any echo or strum to finish and the synth to ring out
    auto peaks = h.run(12.0);
    EXPECT_LT(PlaybackHarness::maxOf(peaks, peaks.size() - 20, peaks.size()),
              1.0e-4f)
        << type << " left a note sounding (seed " << seed << ")";
    EXPECT_FALSE(h.nonFinite);
}

// The same while the song loops: notes that cross the loop end, knobs
// turning all the time. The probe flags any note held far longer than the
// music asks for (no note here is longer than half a second).
TEST_P(StuckNoteTest, loopingWithKnobsTurningNeverLeavesANoteOn) {
    const juce::String type = stuckNoteTypes[std::get<0>(GetParam())];
    const int seed = std::get<1>(GetParam());
    PlaybackHarness h(48000.0, seed % 2 == 0 ? 128 : 333);
    auto plugin = h.add(type);
    auto probe = dynamic_cast<MidiProbePlugin *>(h.add(MidiProbePlugin::xmlTypeName));
    h.add(PulseEnginePlugin::xmlTypeName);
    h.addNotes({{{48, 0.0, 0.5}}, {{60, 1.5, 0.5}}, {{64, 1.5, 0.5}},
                {{67, 1.75, 0.25}}, {{55, 1.0, 1.0}}, {{58, 0.98, 0.5}},
                {{62, 0.25, 0.1}}, {{54, 0.5, 0.5}}, {{56, 1.0, 0.75}}},
               2.0);
    h.loop(0.0, 2.0);
    h.play();
    juce::Random random(seed);
    for (int round = 0; round < 60; round++) {
        if (!std::getenv("LMN3_NOKNOBS"))
            for (auto p : plugin->getAutomatableParameters())
                if (random.nextInt(3) == 0)
                    p->setNormalisedParameter(
                        p->paramID == "latch" ? 0.0f : random.nextFloat(),
                        juce::sendNotificationSync);
        h.run(0.25);
    }
    ASSERT_NE(probe, nullptr);
    EXPECT_GT(probe->longest.load(), 0.02) << "the probe saw no notes";
    EXPECT_LT(probe->longest.load(), 3.0)
        << type << ": note " << probe->longestNote << " was held for "
        << probe->longest.load() << " s (seed " << seed << ")";
    EXPECT_FALSE(h.nonFinite);
}

INSTANTIATE_TEST_SUITE_P(
    All, StuckNoteTest,
    ::testing::Combine(::testing::Range(0, int(std::size(stuckNoteTypes))),
                       ::testing::Range(1, 6)),
    [](const ::testing::TestParamInfo<std::tuple<int, int>> &info) {
        return std::string(stuckNoteTypes[std::get<0>(info.param)]) + "_" +
               std::to_string(std::get<1>(info.param));
    });

} // namespace AppViewModelsTests


