#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace internal_plugins::midifx;

namespace {
struct Event {
    bool on;
    int note, velocity;
    double time; // absolute seconds
};

// Runs blocks of `block` seconds through an engine; `input` lists the
// messages to put into each block (by block index)
template <typename Process>
std::vector<Event>
run(int numBlocks, double block,
    std::map<int, std::vector<juce::MidiMessage>> input, Process process) {
    std::vector<Event> out;
    for (int b = 0; b < numBlocks; b++) {
        tracktion::MidiMessageArray midi;
        for (auto &m : input[b])
            midi.addMidiMessage(m, m.getTimeStamp(),
                                tracktion::MidiMessageArray::notMPE);
        process(midi, b);
        for (auto &m : midi)
            if (m.isNoteOnOrOff())
                out.push_back({m.isNoteOn(), m.getNoteNumber(),
                               m.isNoteOn() ? m.getVelocity() : 0,
                               b * block + m.getTimeStamp()});
    }
    return out;
}

juce::MidiMessage on(int note, int velocity, double time) {
    auto m = juce::MidiMessage::noteOn(1, note, juce::uint8(velocity));
    m.setTimeStamp(time);
    return m;
}
juce::MidiMessage off(int note, double time) {
    auto m = juce::MidiMessage::noteOff(1, note);
    m.setTimeStamp(time);
    return m;
}
std::vector<Event> ons(const std::vector<Event> &events) {
    std::vector<Event> r;
    for (auto &e : events)
        if (e.on)
            r.push_back(e);
    return r;
}
// every note on has a matching later note off, and nothing hangs
void expectBalanced(const std::vector<Event> &events) {
    std::map<int, int> open;
    for (auto &e : events) {
        if (e.on)
            open[e.note]++;
        else
            open[e.note]--;
        EXPECT_GE(open[e.note], 0) << "note off before on for " << e.note;
    }
    for (auto &kv : open)
        EXPECT_EQ(kv.second, 0) << "hanging note " << kv.first;
}
} // namespace

//==============================================================================
// 120 bpm: 2 beats per second; blocks of 10 ms
TEST(NoteRepeatTest, repeatsOnTheGridWhileHeld) {
    NoteRepeatEngine engine;
    NoteRepeatEngine::Settings s;
    s.rateBeats = 0.25; // 1/16 = 0.125 s
    const double block = 0.01;
    auto events =
        run(110, block, {{0, {on(60, 100, 0.0)}}, {100, {off(60, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int b) {
                engine.process(m, b * block * 2.0, 2.0, block, s);
            });
    auto hits = ons(events);
    // the key at 0, then 0.125, 0.25 ... 0.875 (key up at 1.0)
    ASSERT_EQ(hits.size(), 8u);
    for (size_t i = 0; i < hits.size(); i++)
        EXPECT_NEAR(hits[i].time, i * 0.125, 1.0e-6);
    expectBalanced(events);
    EXPECT_EQ(engine.getNumHeldNotes(), 0);
}

TEST(NoteRepeatTest, rampChangesVelocity) {
    NoteRepeatEngine engine;
    NoteRepeatEngine::Settings s;
    s.rateBeats = 0.5;
    s.ramp = -1.0;
    const double block = 0.01;
    auto events =
        run(60, block, {{0, {on(60, 100, 0.0)}}, {55, {off(60, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int b) {
                engine.process(m, b * block * 2.0, 2.0, block, s);
            });
    auto hits = ons(events);
    ASSERT_GE(hits.size(), 3u);
    EXPECT_EQ(hits[0].velocity, 100);
    EXPECT_LT(hits[1].velocity, hits[0].velocity);
    EXPECT_LT(hits[2].velocity, hits[1].velocity);
}

TEST(NoteRepeatTest, swingDelaysEverySecondStep) {
    NoteRepeatEngine engine;
    NoteRepeatEngine::Settings s;
    s.rateBeats = 0.5; // 0.25 s
    s.swing = 0.5;     // odd steps a quarter of a step late
    const double block = 0.01;
    auto events =
        run(110, block, {{0, {on(60, 100, 0.0)}}, {100, {off(60, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int b) {
                engine.process(m, b * block * 2.0, 2.0, block, s);
            });
    auto hits = ons(events);
    ASSERT_GE(hits.size(), 3u);
    EXPECT_NEAR(hits[1].time, 0.25 + 0.0625, 1.0e-6);
    EXPECT_NEAR(hits[2].time, 0.5, 1.0e-6);
    expectBalanced(events);
}

TEST(NoteRepeatTest, allNotesOffStopsEverything) {
    NoteRepeatEngine engine;
    NoteRepeatEngine::Settings s;
    const double block = 0.01;
    auto events = run(30, block, {{0, {on(60, 100, 0.0)}}},
                      [&](tracktion::MidiMessageArray &m, int b) {
                          if (b == 20)
                              m.isAllNotesOff = true;
                          engine.process(m, b * block * 2.0, 2.0, block, s);
                      });
    expectBalanced(events);
    EXPECT_EQ(engine.getNumHeldNotes(), 0);
}

//==============================================================================
TEST(StrumTest, spreadsAChordUpwards) {
    StrumEngine engine;
    StrumEngine::Settings s;
    s.seconds = 0.03;
    const double block = 0.01;
    auto events = run(
        40, block,
        {{0, {on(67, 90, 0.0), on(60, 90, 0.0), on(64, 90, 0.0)}},
         {30, {off(60, 0.0), off(64, 0.0), off(67, 0.0)}}},
        [&](tracktion::MidiMessageArray &m, int) {
            engine.process(m, block, s);
        });
    auto hits = ons(events);
    ASSERT_EQ(hits.size(), 3u);
    EXPECT_EQ(hits[0].note, 60);
    EXPECT_EQ(hits[1].note, 64);
    EXPECT_EQ(hits[2].note, 67);
    EXPECT_NEAR(hits[1].time, 0.03, 1.0e-6);
    EXPECT_NEAR(hits[2].time, 0.06, 1.0e-6);
    expectBalanced(events);
}

TEST(StrumTest, downAndAlternate) {
    StrumEngine engine;
    StrumEngine::Settings s;
    s.alternate = true;
    const double block = 0.01;
    auto chord = [](double) {
        return std::vector<juce::MidiMessage>{on(60, 90, 0.0), on(64, 90, 0.0)};
    };
    auto events =
        run(60, block,
            {{0, chord(0)},
             {20, {off(60, 0.0), off(64, 0.0)}},
             {30, chord(0)},
             {50, {off(60, 0.0), off(64, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int) {
                engine.process(m, block, s);
            });
    auto hits = ons(events);
    ASSERT_EQ(hits.size(), 4u);
    EXPECT_EQ(hits[0].note, 60); // up first
    EXPECT_EQ(hits[2].note, 64); // then down
    expectBalanced(events);
}

TEST(StrumTest, quickReleaseNeverComesBeforeTheNote) {
    StrumEngine engine;
    StrumEngine::Settings s;
    s.seconds = 0.2;
    const double block = 0.01;
    // released after 5 ms, but the top note is strummed 0.4 s later
    auto events = run(
        80, block,
        {{0, {on(60, 90, 0.0), on(64, 90, 0.0), on(67, 90, 0.0),
              off(60, 0.005), off(64, 0.005), off(67, 0.005)}}},
        [&](tracktion::MidiMessageArray &m, int) {
            engine.process(m, block, s);
        });
    expectBalanced(events);
}

TEST(StrumTest, curvesKeepTheEnds) {
    for (int curve = 0; curve < 3; curve++) {
        EXPECT_DOUBLE_EQ(Strum::offset(0, 4, 0.03, curve), 0.0);
        EXPECT_NEAR(Strum::offset(3, 4, 0.03, curve), 0.09, 1.0e-9);
    }
    EXPECT_LT(Strum::offset(1, 4, 0.03, 2), Strum::offset(1, 4, 0.03, 0));
    EXPECT_GT(Strum::offset(1, 4, 0.03, 1), Strum::offset(1, 4, 0.03, 0));
}

//==============================================================================
TEST(HumaniseTest, staysWithinLimitsAndBalanced) {
    HumaniseEngine engine(42);
    HumaniseEngine::Settings s;
    s.timingSeconds = 0.02;
    s.velocity = 20;
    s.amount = 1.0;
    const double block = 0.01;
    std::map<int, std::vector<juce::MidiMessage>> input;
    for (int i = 0; i < 16; i++) {
        input[i * 10].push_back(on(60 + i % 4, 80, 0.0));
        input[i * 10 + 5].push_back(off(60 + i % 4, 0.0));
    }
    auto events = run(200, block, input,
                      [&](tracktion::MidiMessageArray &m, int) {
                          engine.process(m, block, s);
                      });
    auto hits = ons(events);
    ASSERT_EQ(hits.size(), 16u);
    bool anyMoved = false;
    for (size_t i = 0; i < hits.size(); i++) {
        double late = hits[i].time - i * 0.1;
        EXPECT_GE(late, -1.0e-9);
        EXPECT_LE(late, 0.02 + 1.0e-9);
        EXPECT_GE(hits[i].velocity, 60);
        EXPECT_LE(hits[i].velocity, 100);
        anyMoved |= late > 1.0e-4 || hits[i].velocity != 80;
    }
    EXPECT_TRUE(anyMoved);
    expectBalanced(events);
}

TEST(HumaniseTest, zeroAmountChangesNothing) {
    HumaniseEngine engine(3);
    HumaniseEngine::Settings s;
    s.amount = 0.0;
    const double block = 0.01;
    auto events =
        run(20, block, {{0, {on(60, 77, 0.004)}}, {10, {off(60, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int) {
                engine.process(m, block, s);
            });
    auto hits = ons(events);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].time, 0.004, 1.0e-6);
    EXPECT_EQ(hits[0].velocity, 77);
}

//==============================================================================
TEST(EchoTest, echoesGetQuieterAndTranspose) {
    EchoEngine engine;
    EchoEngine::Settings s;
    s.timeBeats = 0.5; // 0.25 s at 120 bpm
    s.repeats = 3;
    s.decay = 0.5;
    s.transpose = 7;
    const double block = 0.01;
    auto events =
        run(150, block, {{0, {on(60, 100, 0.0)}}, {10, {off(60, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int) {
                engine.process(m, 2.0, block, s);
            });
    auto hits = ons(events);
    ASSERT_EQ(hits.size(), 4u);
    EXPECT_EQ(hits[1].note, 67);
    EXPECT_EQ(hits[2].note, 74);
    EXPECT_EQ(hits[3].note, 81);
    EXPECT_EQ(hits[1].velocity, 50);
    EXPECT_EQ(hits[2].velocity, 25);
    EXPECT_NEAR(hits[1].time, 0.25, 1.0e-6);
    EXPECT_NEAR(hits[3].time, 0.75, 1.0e-6);
    expectBalanced(events);
}

TEST(EchoTest, stopsAtTheTopOfTheRange) {
    EchoEngine engine;
    EchoEngine::Settings s;
    s.repeats = 8;
    s.decay = 0.0;
    s.transpose = 12;
    const double block = 0.01;
    auto events =
        run(500, block, {{0, {on(100, 100, 0.0)}}, {10, {off(100, 0.0)}}},
            [&](tracktion::MidiMessageArray &m, int) {
                engine.process(m, 2.0, block, s);
            });
    for (auto &e : events)
        EXPECT_LE(e.note, 127);
    EXPECT_EQ(ons(events).size(), 3u); // 100, 112, 124
    expectBalanced(events);
}

} // namespace AppViewModelsTests
