// Plays real edits through the engine, block by block, the way the app does,
// to catch things that only go wrong in playback: notes lost when the song
// starts or loops, notes left hanging after Stop, plugins that go silent or
// blow up (NaN) once they're part of a track.
#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {
using namespace internal_plugins;

namespace {
constexpr int blockSize = 256;

// One audio thread: the test drives the audio callback itself, and many
// engines come and go in one test run
struct SingleThreadedBehaviour : tracktion::EngineBehaviour {
    int getNumberOfCPUsToUseForAudio() override { return 1; }
};

class PlaybackHarness {
  public:
    explicit PlaybackHarness(double rate = 44100.0) : sampleRate(rate) {
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

    // Runs `seconds` of audio, returning the peak of each 50 ms
    std::vector<float> run(double seconds) {
        std::vector<float> peaks;
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        const int blocksPerSlice = int(0.05 * sampleRate / blockSize);
        int numBlocks = int(seconds * sampleRate / blockSize);
        float peak = 0.0f;
        for (int b = 0; b < numBlocks; b++) {
            buffer.clear();
            midi.clear();
            engine.getDeviceManager().getHostedAudioDeviceInterface().processBlock(
                buffer, midi);
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
        return peaks;
    }

    static float maxOf(const std::vector<float> &peaks, size_t from,
                       size_t to) {
        float m = 0.0f;
        for (size_t i = from; i < juce::jmin(to, peaks.size()); i++)
            m = juce::jmax(m, peaks[i]);
        return m;
    }

    void pump() {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }

    const double sampleRate;
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

} // namespace AppViewModelsTests
