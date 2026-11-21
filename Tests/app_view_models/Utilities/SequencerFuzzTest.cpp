#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

// Every sequencer, fed thousands of random settings (and some a damaged
// project file could hold): the notes must always be playable, inside the
// clip, and the generators must never crash or hang.
namespace AppViewModelsTests {

namespace Seq = app_view_models::Sequencers;

namespace {
void checkNotes(const std::vector<Seq::NoteEvent> &notes, double clipBeats,
                const std::string &what) {
    ASSERT_TRUE(std::isfinite(clipBeats)) << what;
    ASSERT_GT(clipBeats, 0.0) << what;
    EXPECT_LE(clipBeats, 64.0 * 4.0 * 8.0) << what << ": a huge clip";
    EXPECT_LE(notes.size(), 20000u) << what;
    for (auto &n : notes) {
        ASSERT_TRUE(juce::isPositiveAndBelow(n.pitch, 128))
            << what << ": pitch " << n.pitch;
        ASSERT_TRUE(n.velocity >= 1 && n.velocity <= 127)
            << what << ": velocity " << n.velocity;
        ASSERT_TRUE(std::isfinite(n.start) && std::isfinite(n.length)) << what;
        ASSERT_GE(n.start, 0.0) << what;
        ASSERT_LT(n.start, clipBeats + 1.0e-9) << what << ": starts after the clip";
        ASSERT_GT(n.length, 0.0) << what << ": a note with no length";
    }
}

int pick(juce::Random &r, int lo, int hi) { return lo + r.nextInt(hi - lo + 1); }
// mostly in range, sometimes just outside it, now and then wild
int wild(juce::Random &r, int lo, int hi) {
    switch (r.nextInt(10)) {
    case 0:
        return lo - 1 - r.nextInt(3);
    case 1:
        return hi + 1 + r.nextInt(3);
    case 2:
        return r.nextBool() ? -1000 : 1000;
    default:
        return pick(r, lo, hi);
    }
}
double wildD(juce::Random &r, double lo, double hi) {
    switch (r.nextInt(10)) {
    case 0:
        return lo - 0.5;
    case 1:
        return hi + 0.5;
    default:
        return lo + r.nextDouble() * (hi - lo);
    }
}
} // namespace

TEST(SequencerFuzzTest, euclid) {
    juce::Random r(1);
    for (int i = 0; i < 3000; i++) {
        int hits = wild(r, 0, 32), steps = wild(r, 1, 32), rot = wild(r, -32, 32);
        auto p = Seq::euclid(hits, steps, rot);
        EXPECT_LE(p.size(), 64) << hits << " " << steps << " " << rot;
        Seq::EuclidInput in;
        for (auto &ring : in.rings) {
            ring.hits = r.nextInt(3) == 0 ? wild(r, 0, 32) : 0;
            ring.steps = wild(r, 1, 32);
            ring.rotate = wild(r, -32, 32);
        }
        in.swing = wildD(r, 0.0, 1.0);
        in.velocity = wild(r, 1, 127);
        int length = Seq::euclidLengthSteps(in);
        EXPECT_TRUE(length >= 1 && length <= 64) << length;
        checkNotes(Seq::generateEuclid(in), length * 0.25, "euclid");
    }
}

TEST(SequencerFuzzTest, turing) {
    juce::Random r(2);
    for (int i = 0; i < 2000; i++) {
        Seq::TuringInput in;
        in.lock = wildD(r, 0.0, 1.0);
        in.length = wild(r, 2, 16);
        in.range = wild(r, 1, 4);
        in.scale = wild(r, 0, 20);
        in.root = wild(r, 0, 120);
        in.stepLength = r.nextBool() ? 0.25 : wildD(r, 0.0625, 1.0);
        in.gate = wildD(r, 0.05, 1.0);
        in.seed = r.nextInt();
        in.bars = wild(r, 1, 16);
        checkNotes(Seq::generateTuring(in), Seq::turingLength(in), "turing");
    }
}

TEST(SequencerFuzzTest, chords) {
    juce::Random r(3);
    for (int i = 0; i < 2000; i++) {
        Seq::ChordsInput in;
        in.key = wild(r, 0, 23);
        int bars = pick(r, 0, 9);
        for (int b = 0; b < bars; b++)
            in.degrees.add(wild(r, 0, 6));
        for (auto role : {Seq::ChordRole::chords, Seq::ChordRole::bassRoots,
                          Seq::ChordRole::arpeggio, Seq::ChordRole::pad})
            checkNotes(Seq::generateChords(in, role), Seq::chordsLength(in),
                       "chords");
    }
}

TEST(SequencerFuzzTest, step) {
    juce::Random r(4);
    for (int i = 0; i < 2000; i++) {
        Seq::StepInput in;
        for (int s = 0; s < 16; s++) {
            int count = pick(r, 0, 3);
            for (int k = 0; k < count; k++)
                in.notes[size_t(s)].add(wild(r, 0, 127));
            auto &t = in.tricks[size_t(s)];
            t.velocity = wild(r, 1, 127);
            t.length = wild(r, 1, 16);
            t.chance = float(wildD(r, 0.0, 1.0));
            t.ratchet = wild(r, 1, 4);
        }
        in.length = wild(r, 1, 16);
        in.stepsPerBar = wild(r, 1, 32);
        in.groove.swing = wildD(r, 0.0, 1.0);
        in.groove.gate = wildD(r, 0.1, 1.0);
        in.groove.direction = Seq::Direction(pick(r, 0, 3));
        in.groove.humanize = wildD(r, 0.0, 1.0);
        int passes = Seq::stepPasses(in);
        double clip = passes * juce::jlimit(1, 16, in.length) *
                      Seq::stepBeats(in.stepsPerBar);
        checkNotes(Seq::generateStep(in, r.nextInt()), clip,
                   "step len " + std::to_string(in.length) + " spb " +
                       std::to_string(in.stepsPerBar));
    }
}

TEST(SequencerFuzzTest, drums) {
    juce::Random r(5);
    for (int i = 0; i < 1000; i++) {
        Seq::DrumInput in;
        for (auto *g : {&in.main, &in.fill})
            for (auto &row : g->cells)
                for (auto &c : row)
                    c = juce::uint8(r.nextInt(8) == 0 ? pick(r, 0, 3) : 0);
        in.length = wild(r, 16, 64);
        in.stepsPerBar = wild(r, 4, 32);
        in.swing = wildD(r, 0.0, 1.0);
        in.fillEvery = wild(r, 0, 8);
        for (auto &l : in.levels)
            l = float(wildD(r, 0.0, 1.0));
        int passes = Seq::drumPasses(in);
        double clip = passes * juce::jlimit(1, Seq::drumMaxSteps, in.length) *
                      Seq::stepBeats(in.stepsPerBar);
        checkNotes(Seq::generateDrums(in), clip,
                   "drums len " + std::to_string(in.length) + " spb " +
                       std::to_string(in.stepsPerBar) + " fill " +
                       std::to_string(in.fillEvery));
    }
}

TEST(SequencerFuzzTest, drumGridSurvivesDamagedText) {
    juce::Random r(6);
    for (int i = 0; i < 500; i++) {
        juce::String s;
        int len = r.nextInt(3000);
        for (int k = 0; k < len; k++)
            s << juce::String::charToString(juce::juce_wchar(
                r.nextBool() ? '0' + r.nextInt(10) : 32 + r.nextInt(95)));
        auto g = Seq::DrumGrid::fromString(s);
        for (auto &row : g.cells)
            for (auto c : row)
                ASSERT_LE(c, 3);
    }
}

TEST(SequencerFuzzTest, endless) {
    juce::Random r(7);
    for (int i = 0; i < 2000; i++) {
        Seq::EndlessInput in;
        int count = pick(r, 0, 64);
        for (int k = 0; k < count; k++)
            in.notes.add(r.nextInt(4) == 0 ? -1 : wild(r, 0, 127));
        in.stepLength = r.nextBool() ? 0.25 : wildD(r, 0.0625, 1.0);
        in.gate = wildD(r, 0.05, 1.0);
        in.swing = wildD(r, 0.0, 1.0);
        in.transpose = wild(r, -24, 24);
        double length = Seq::endlessLength(in);
        auto notes = Seq::generateEndless(in);
        if (count == 0) {
            EXPECT_TRUE(notes.empty());
            continue;
        }
        checkNotes(notes, length, "endless");
    }
}

TEST(SequencerFuzzTest, tombola) {
    juce::Random r(8);
    for (int i = 0; i < 150; i++) {
        Seq::TombolaInput in;
        int count = pick(r, 0, 8);
        for (int k = 0; k < count; k++)
            in.balls.add(wild(r, 0, 127));
        in.spin = wildD(r, -1.0, 1.0);
        in.gravity = wildD(r, 0.0, 1.0);
        in.sides = wild(r, 3, 8);
        in.bounce = wildD(r, 0.0, 1.0);
        in.lengthBeats = r.nextBool() ? 16.0 : wildD(r, 4.0, 32.0);
        in.seed = r.nextInt(100);
        Seq::TombolaSimulation sim(in);
        checkNotes(sim.getNotes(), juce::jmax(0.001, in.lengthBeats), "tombola");
        auto f = sim.frameAt(r.nextDouble() * 40.0 - 4.0);
        EXPECT_TRUE(std::isfinite(f.angle));
        for (auto p : f.balls)
            EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y));
    }
}

TEST(StepSwingTest, ratchetsOnASwungLastStepAllPlay) {
    Seq::StepInput in;
    in.length = 2;
    in.notes[1].add(60);
    in.tricks[1].ratchet = 4;
    in.groove.swing = 1.0; // the second step starts half a step late
    auto notes = Seq::generateStep(in);
    ASSERT_EQ(notes.size(), 4u);
    const double clip = 2.0; // 2 quarter-note steps
    int wrapped = 0;
    for (auto &n : notes) {
        EXPECT_LT(n.start, clip) << "a hit starts after the clip ends";
        wrapped += n.start < 1.0;
    }
    // the step starts at 1.5 beats: hits at 1.5, 1.75, then 2.0 and 2.25
    // come round to 0 and 0.25
    EXPECT_EQ(wrapped, 2) << "the late hits should come round at the start";
}

} // namespace AppViewModelsTests
