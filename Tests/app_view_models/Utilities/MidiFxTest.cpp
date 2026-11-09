#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace internal_plugins::midifx;

//==============================================================================
TEST(ScaleLockTest, notesInScaleAreUnchanged) {
    // C major: C D E F G A B
    for (int note : {60, 62, 64, 65, 67, 69, 71, 72})
        EXPECT_EQ(Scales::snapToScale(note, 0, 0), note);
}

TEST(ScaleLockTest, snapsToNearestWithTiesGoingUp) {
    EXPECT_EQ(Scales::snapToScale(61, 0, 0), 62); // C# -> D (tie goes up)
    EXPECT_EQ(Scales::snapToScale(66, 0, 0), 67); // F# -> G (tie goes up)
    // A minor pentatonic: A C D E G. A# is one above A, two below C -> A
    int minorPent = Scales::getNames().indexOf("Minor Pent");
    EXPECT_EQ(Scales::snapToScale(70, 9, minorPent), 69);
}

TEST(ScaleLockTest, rootTransposesTheScale) {
    // D major has F# and C#
    EXPECT_EQ(Scales::snapToScale(66, 2, 0), 66);
    EXPECT_EQ(Scales::snapToScale(61, 2, 0), 61);
    EXPECT_EQ(Scales::snapToScale(65, 2, 0), 66); // F -> F#
}

TEST(ScaleLockTest, chromaticIsOff) {
    int chromatic = Scales::getNames().indexOf("Chromatic");
    for (int note = 0; note < 128; note++)
        EXPECT_EQ(Scales::snapToScale(note, 5, chromatic), note);
}

//==============================================================================
TEST(ChordTest, buildsBasicTriads) {
    EXPECT_EQ(Chords::build(60, 0, 0, 0), juce::Array<int>({60, 64, 67}));
    EXPECT_EQ(Chords::build(60, 1, 0, 0), juce::Array<int>({60, 63, 67}));
}

TEST(ChordTest, inversionsMoveLowNotesUp) {
    EXPECT_EQ(Chords::build(60, 0, 1, 0), juce::Array<int>({64, 67, 72}));
    EXPECT_EQ(Chords::build(60, 0, 2, 0), juce::Array<int>({67, 72, 76}));
}

TEST(ChordTest, openVoicingSpreadsTheChord) {
    EXPECT_EQ(Chords::build(60, 0, 0, 1), juce::Array<int>({60, 67, 76}));
}

TEST(ChordTest, notesStayInMidiRange) {
    for (int n : Chords::build(126, Chords::getNames().indexOf("Min9"), 0, 1))
        EXPECT_LE(n, 127);
}

//==============================================================================
class ArpEngineTest : public ::testing::Test {
  protected:
    struct Event {
        bool on;
        int note;
        double time;
    };

    static std::vector<Event> events(const tracktion::MidiMessageArray &midi) {
        std::vector<Event> result;
        for (auto &m : midi)
            if (m.isNoteOn() || m.isNoteOff())
                result.push_back(
                    {m.isNoteOn(), m.getNoteNumber(), m.getTimeStamp()});
        return result;
    }

    static std::vector<int> noteOns(const std::vector<Event> &e) {
        std::vector<int> notes;
        for (auto &ev : e)
            if (ev.on)
                notes.push_back(ev.note);
        return notes;
    }

    static void press(tracktion::MidiMessageArray &midi,
                      std::initializer_list<int> notes, double time = 0.0) {
        for (int n : notes)
            midi.addMidiMessage(
                juce::MidiMessage::noteOn(1, n, juce::uint8(90)), time,
                tracktion::MidiMessageArray::notMPE);
    }

    static void release(tracktion::MidiMessageArray &midi,
                        std::initializer_list<int> notes, double time = 0.0) {
        for (int n : notes)
            midi.addMidiMessage(juce::MidiMessage::noteOff(1, n), time,
                                tracktion::MidiMessageArray::notMPE);
    }

    ArpEngine engine;
    ArpEngine::Settings settings; // up, 1/16, 1 octave, 50% gate
};

TEST_F(ArpEngineTest, playsSixteenthsUpwardsAtTempo) {
    tracktion::MidiMessageArray midi;
    press(midi, {67, 60, 64});

    // 120 bpm = 2 beats per second; 1 second block = 2 beats = 8 sixteenths
    engine.process(midi, 0.0, 2.0, 1.0, settings);
    auto e = events(midi);

    EXPECT_EQ(noteOns(e), std::vector<int>({60, 64, 67, 60, 64, 67, 60, 64}));

    int index = 0;
    for (auto &ev : e) {
        if (ev.on) {
            EXPECT_NEAR(ev.time, index * 0.125, 1e-9);
            index++;
        }
    }

    // 50% gate: first note off half a step after the first note on
    for (auto &ev : e)
        if (!ev.on && ev.note == 60) {
            EXPECT_NEAR(ev.time, 0.0625, 1e-4);
            break;
        }

    // the original key presses are not passed through
    int ons = 0;
    for (auto &ev : e)
        ons += ev.on ? 1 : 0;
    EXPECT_EQ(ons, 8);
}

TEST_F(ArpEngineTest, continuesAcrossBlocksWithoutRepeatingSteps) {
    tracktion::MidiMessageArray block1, block2;
    press(block1, {60, 64, 67});
    // two half-second blocks = one one-second block
    engine.process(block1, 0.0, 2.0, 0.5, settings);
    engine.process(block2, 1.0, 2.0, 0.5, settings);

    auto all = noteOns(events(block1));
    auto second = noteOns(events(block2));
    all.insert(all.end(), second.begin(), second.end());
    EXPECT_EQ(all, std::vector<int>({60, 64, 67, 60, 64, 67, 60, 64}));

    // block 2 timestamps are relative to its own start
    EXPECT_NEAR(events(block2).front().time, 0.0, 1e-9);
}

TEST_F(ArpEngineTest, downAndUpDownModes) {
    tracktion::MidiMessageArray midi;
    settings.mode = Arp::down;
    press(midi, {60, 64, 67});
    engine.process(midi, 0.0, 2.0, 0.5, settings);
    EXPECT_EQ(noteOns(events(midi)), std::vector<int>({67, 64, 60, 67}));

    ArpEngine upDown;
    tracktion::MidiMessageArray midi2;
    settings.mode = Arp::upDown;
    press(midi2, {60, 64, 67});
    upDown.process(midi2, 0.0, 2.0, 1.0, settings);
    EXPECT_EQ(noteOns(events(midi2)),
              std::vector<int>({60, 64, 67, 64, 60, 64, 67, 64}));
}

TEST_F(ArpEngineTest, octavesExtendThePattern) {
    tracktion::MidiMessageArray midi;
    settings.octaves = 2;
    press(midi, {60, 64});
    engine.process(midi, 0.0, 2.0, 0.5, settings);
    EXPECT_EQ(noteOns(events(midi)), std::vector<int>({60, 64, 72, 76}));
}

TEST_F(ArpEngineTest, stopsWhenKeysReleasedWithoutLatch) {
    tracktion::MidiMessageArray block1, block2;
    press(block1, {60, 64});
    engine.process(block1, 0.0, 2.0, 0.5, settings);

    release(block2, {60, 64});
    engine.process(block2, 1.0, 2.0, 0.5, settings);

    EXPECT_TRUE(noteOns(events(block2)).empty());
    EXPECT_EQ(engine.getNumHeldNotes(), 0);
}

TEST_F(ArpEngineTest, latchKeepsPlayingAndNewChordReplaces) {
    settings.latch = true;
    tracktion::MidiMessageArray block1, block2, block3;
    press(block1, {60, 64});
    release(block1, {60, 64}, 0.01);
    engine.process(block1, 0.0, 2.0, 0.5, settings);

    engine.process(block2, 1.0, 2.0, 0.5, settings);
    EXPECT_EQ(noteOns(events(block2)), std::vector<int>({60, 64, 60, 64}));

    press(block3, {62});
    engine.process(block3, 2.0, 2.0, 0.5, settings);
    EXPECT_EQ(noteOns(events(block3)), std::vector<int>({62, 62, 62, 62}));
}

TEST_F(ArpEngineTest, followsTheGridWhenStartedMidBeat) {
    // block starts at beat 0.1, so the first step is at beat 0.25
    tracktion::MidiMessageArray midi;
    press(midi, {60});
    engine.process(midi, 0.1, 2.0, 0.5, settings);
    auto e = events(midi);
    ASSERT_FALSE(e.empty());
    EXPECT_NEAR(e.front().time, (0.25 - 0.1) / 2.0, 1e-9);
}

TEST_F(ArpEngineTest, allNotesOffResets) {
    tracktion::MidiMessageArray block1, block2;
    press(block1, {60});
    engine.process(block1, 0.0, 2.0, 0.5, settings);
    block2.isAllNotesOff = true;
    engine.process(block2, 1.0, 2.0, 0.5, settings);
    EXPECT_EQ(engine.getNumHeldNotes(), 0);
    EXPECT_TRUE(noteOns(events(block2)).empty());
}

//==============================================================================
class PluginPlacementTest : public ::testing::Test {
  protected:
    PluginPlacementTest()
        : edit(tracktion::Edit::createSingleTrackEdit(engine)),
          track(tracktion::getAudioTracks(*edit)[0]) {
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ArpeggiatorPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ScaleLockPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ChordPlugin>();
    }

    tracktion::Plugin::Ptr create(const juce::String &type) {
        return edit->getPluginCache().createNewPlugin(type, {});
    }

    juce::StringArray types() {
        juce::StringArray t;
        for (auto p : track->pluginList)
            t.add(p->getPluginType());
        return t;
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
    tracktion::AudioTrack *track;
};

TEST_F(PluginPlacementTest, midiFxGoBeforeTheInstrumentInOrderAdded) {
    using namespace app_view_models;
    PluginPlacement::insertPlugin(
        *track, create(tracktion::FourOscPlugin::xmlTypeName));
    PluginPlacement::insertPlugin(
        *track, create(internal_plugins::ChordPlugin::xmlTypeName));
    PluginPlacement::insertPlugin(
        *track, create(internal_plugins::ArpeggiatorPlugin::xmlTypeName));

    auto t = types();
    EXPECT_EQ(t[0], internal_plugins::ChordPlugin::xmlTypeName);
    EXPECT_EQ(t[1], internal_plugins::ArpeggiatorPlugin::xmlTypeName);
    EXPECT_EQ(t[2], tracktion::FourOscPlugin::xmlTypeName);
}

TEST_F(PluginPlacementTest, newInstrumentReplacesOldOneAfterMidiFx) {
    using namespace app_view_models;
    PluginPlacement::insertPlugin(
        *track, create(internal_plugins::ScaleLockPlugin::xmlTypeName));
    PluginPlacement::insertPlugin(
        *track, create(tracktion::FourOscPlugin::xmlTypeName));
    PluginPlacement::insertPlugin(
        *track, create(tracktion::SamplerPlugin::xmlTypeName));

    auto t = types();
    EXPECT_EQ(t[0], internal_plugins::ScaleLockPlugin::xmlTypeName);
    EXPECT_EQ(t[1], tracktion::SamplerPlugin::xmlTypeName);
    EXPECT_FALSE(t.contains(tracktion::FourOscPlugin::xmlTypeName));
}

TEST_F(PluginPlacementTest, effectsGoBeforeVolumeAndMeter) {
    using namespace app_view_models;
    int before = track->pluginList.size(); // volume + meter
    PluginPlacement::insertPlugin(*track,
                                  create(tracktion::ReverbPlugin::xmlTypeName));
    auto t = types();
    EXPECT_EQ(track->pluginList.size(), before + 1);
    EXPECT_EQ(t[t.size() - 3], tracktion::ReverbPlugin::xmlTypeName);
}

TEST_F(PluginPlacementTest, midiFxControlsHaveReadableLabels) {
    auto plugin = create(internal_plugins::ArpeggiatorPlugin::xmlTypeName);
    auto arp =
        dynamic_cast<internal_plugins::ArpeggiatorPlugin *>(plugin.get());
    ASSERT_NE(arp, nullptr);
    EXPECT_EQ(arp->getControls().size(), 5);
    EXPECT_EQ(arp->rate->getCurrentValueAsString(), "1/16");
    EXPECT_EQ(arp->mode->valueToString(2.0f), "Up/Down");
    EXPECT_EQ(arp->gate->getCurrentValueAsString(), "50%");
}

} // namespace AppViewModelsTests
