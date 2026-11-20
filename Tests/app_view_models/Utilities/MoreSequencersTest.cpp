#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

namespace Seq = app_view_models::Sequencers;

//==============================================================================
TEST(EuclideanTest, spreadsHitsEvenly) {
    auto p = Seq::euclid(4, 16, 0);
    ASSERT_EQ(p.size(), 16);
    for (int i = 0; i < 16; i++)
        EXPECT_EQ(p[i], i % 4 == 0) << i;
    auto tresillo = Seq::euclid(3, 8, 0);
    int hits = 0;
    for (auto h : tresillo)
        hits += h;
    EXPECT_EQ(hits, 3);
}

TEST(EuclideanTest, rotateTurnsThePattern) {
    auto a = Seq::euclid(3, 8, 0), b = Seq::euclid(3, 8, 2);
    for (int i = 0; i < 8; i++)
        EXPECT_EQ(b[(i + 2) % 8], a[i]);
    EXPECT_EQ(Seq::euclid(20, 8, 0).size(), 8); // hits clamp to the steps
}

TEST(EuclideanTest, loopFitsEveryRing) {
    Seq::EuclidInput in;
    EXPECT_EQ(Seq::euclidLengthSteps(in), 16); // nothing on: a bar
    in.rings[0] = {4, 16, 0};
    in.rings[1] = {3, 12, 0};
    EXPECT_EQ(Seq::euclidLengthSteps(in), 48);
    in.rings[1] = {2, 5, 0};
    EXPECT_EQ(Seq::euclidLengthSteps(in), 64); // 80 would be too long
}

TEST(EuclideanTest, writesNotesForEachRing) {
    Seq::EuclidInput in;
    in.rings[0] = {4, 16, 0};
    in.rings[4] = {2, 16, 4};
    auto events = Seq::generateEuclid(in);
    int kicks = 0, hats = 0;
    for (auto &e : events) {
        if (e.pitch == Seq::firstDrumNote) {
            EXPECT_NEAR(std::fmod(e.start, 1.0), 0.0, 1.0e-9);
            kicks++;
        }
        if (e.pitch == Seq::firstDrumNote + 4)
            hats++;
    }
    EXPECT_EQ(kicks, 4);
    EXPECT_EQ(hats, 2);
}

//==============================================================================
TEST(TuringTest, fullyLockedRepeats) {
    Seq::TuringInput in;
    in.lock = 1.0;
    in.length = 8;
    auto notes = Seq::turingNotes(in);
    ASSERT_EQ(notes.size(), 64u); // 4 bars of 16ths
    for (size_t i = 8; i < notes.size(); i++)
        EXPECT_EQ(notes[i], notes[i - 8]);
}

TEST(TuringTest, unlockedChangesButStaysInRangeAndScale) {
    Seq::TuringInput in;
    in.lock = 0.5;
    in.range = 1;
    in.root = 48;
    in.scale = 0; // C major
    auto notes = Seq::turingNotes(in);
    bool changed = false;
    for (size_t i = 16; i < notes.size(); i++)
        changed |= notes[i] != notes[i - 16];
    EXPECT_TRUE(changed);
    for (int n : notes) {
        EXPECT_GE(n, 48);
        EXPECT_LE(n, 61);
        EXPECT_EQ(internal_plugins::midifx::Scales::snapToScale(n, 0, 0), n);
    }
}

TEST(TuringTest, rerollGivesADifferentMelody) {
    Seq::TuringInput a, b;
    b.seed = 2;
    EXPECT_NE(Seq::turingNotes(a), Seq::turingNotes(b));
}

//==============================================================================
TEST(ChordsTest, triadsInTheKey) {
    namespace C = Seq::Chords;
    EXPECT_EQ(C::triad(0, 0), juce::Array<int>({60, 64, 67})); // C
    EXPECT_EQ(C::triad(0, 4), juce::Array<int>({55, 59, 62})); // G, below C4
    EXPECT_EQ(C::triad(0, 5), juce::Array<int>({57, 60, 64})); // Am
    EXPECT_EQ(C::chordName(0, 5), "Am");
    EXPECT_EQ(C::romanName(0, 4), "V");
    EXPECT_EQ(C::romanName(0, 6), juce::String::fromUTF8("vii\xc2\xb0"));
    int aMinor = C::getKeyNames().indexOf("A minor");
    EXPECT_EQ(C::chordName(aMinor, 0), "Am");
    EXPECT_EQ(C::chordName(aMinor, 2), "C");
}

TEST(ChordsTest, eachRolePlaysItsPart) {
    Seq::ChordsInput in;
    in.key = 0;
    in.degrees = {0, 4};
    EXPECT_DOUBLE_EQ(Seq::chordsLength(in), 8.0);
    auto bass = Seq::generateChords(in, Seq::ChordRole::bassRoots);
    EXPECT_EQ(bass.size(), 16u);
    EXPECT_EQ(bass[0].pitch, 36);  // C2
    EXPECT_EQ(bass[8].pitch, 31);  // G1
    auto arp = Seq::generateChords(in, Seq::ChordRole::arpeggio);
    EXPECT_EQ(arp.size(), 32u);
    auto pad = Seq::generateChords(in, Seq::ChordRole::pad);
    EXPECT_EQ(pad.size(), 8u);
    EXPECT_GT(pad[0].length, 3.0);
    auto chords = Seq::generateChords(in, Seq::ChordRole::chords);
    EXPECT_EQ(chords.size(), 24u); // 3 notes x 4 beats x 2 bars
}

//==============================================================================
TEST(FingerTest, heldKeyPlaysItsPatternInTime) {
    using namespace internal_plugins::midifx;
    FingerEngine engine;
    FingerEngine::Settings s; // slot 0 = Four floor, key 54
    const double block = 0.01, bps = 2.0;
    std::vector<std::pair<int, double>> ons;
    for (int b = 0; b < 200; b++) { // 2 seconds = 1 bar
        tracktion::MidiMessageArray midi;
        if (b == 0)
            midi.addMidiMessage(juce::MidiMessage::noteOn(1, 54, 1.0f), 0.0,
                                tracktion::MidiMessageArray::notMPE);
        if (b == 199)
            midi.addMidiMessage(juce::MidiMessage::noteOff(1, 54), 0.0,
                                tracktion::MidiMessageArray::notMPE);
        engine.process(midi, b * block * bps, bps, block, s);
        for (auto &m : midi) {
            EXPECT_NE(m.getNoteNumber(), 54); // the key itself is taken
            if (m.isNoteOn())
                ons.push_back({m.getNoteNumber(), b * block + m.getTimeStamp()});
        }
    }
    int kicks = 0;
    for (auto &on : ons)
        if (on.first == 53) {
            EXPECT_NEAR(std::fmod(on.second, 0.5), 0.0, 1.0e-6);
            kicks++;
        }
    EXPECT_EQ(kicks, 4);
}

TEST(FingerTest, otherKeysPassThrough) {
    using namespace internal_plugins::midifx;
    FingerEngine engine;
    FingerEngine::Settings s;
    tracktion::MidiMessageArray midi;
    midi.addMidiMessage(juce::MidiMessage::noteOn(1, 70, 1.0f), 0.0,
                        tracktion::MidiMessageArray::notMPE);
    engine.process(midi, 0.0, 2.0, 0.01, s);
    ASSERT_EQ(midi.size(), 1);
    EXPECT_EQ(midi[0].getNoteNumber(), 70);
    EXPECT_EQ(engine.getHeldMask(), 0u);
}

TEST(FingerTest, variationsChangeThePattern) {
    using namespace internal_plugins::midifx;
    int base = 0, busy = 0;
    for (int step = 0; step < 16; step++) {
        base += Finger::hit(1, 0, 4, step) > 0;
        busy += Finger::hit(1, 1, 4, step) > 0;
    }
    EXPECT_GT(busy, base);
    EXPECT_GT(Finger::hit(0, 3, 1, 15), 0); // the fill's snare
}

} // namespace AppViewModelsTests
