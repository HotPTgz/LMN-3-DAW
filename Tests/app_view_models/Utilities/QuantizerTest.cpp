#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using app_view_models::Quantizer::quantizeBeat;

TEST(QuantizerTest, snapsToNearestStraightGridPoint) {
    EXPECT_DOUBLE_EQ(quantizeBeat(0.10, 0.25, 0), 0.0);
    EXPECT_DOUBLE_EQ(quantizeBeat(0.13, 0.25, 0), 0.25);
    EXPECT_DOUBLE_EQ(quantizeBeat(0.99, 0.25, 0), 1.0);
    EXPECT_DOUBLE_EQ(quantizeBeat(3.6, 1.0, 0), 4.0);
    EXPECT_DOUBLE_EQ(quantizeBeat(2.0, 0.5, 0), 2.0);
}

TEST(QuantizerTest, neverReturnsNegativePositions) {
    EXPECT_DOUBLE_EQ(quantizeBeat(-0.4, 1.0, 0), 0.0);
}

TEST(QuantizerTest, swingDelaysOddSteps) {
    // 1/8 grid, 50% swing: offbeat 8ths move by 0.5 * 0.5 / 2 = 0.125 beats
    EXPECT_DOUBLE_EQ(quantizeBeat(0.5, 0.5, 50), 0.625);
    EXPECT_DOUBLE_EQ(quantizeBeat(0.6, 0.5, 50), 0.625);
    // even steps (on the beat) don't move
    EXPECT_DOUBLE_EQ(quantizeBeat(1.02, 0.5, 50), 1.0);
}

TEST(QuantizerTest, twoThirdsSwingIsTripletShuffle) {
    // swing of 2/3 * 100 on a 1/8 grid puts the offbeat on the last 8th-note
    // triplet (2/3 of a beat). Integer percent gives 66%, very close.
    EXPECT_NEAR(quantizeBeat(0.5, 0.5, 66), 2.0 / 3.0, 0.01);
}

TEST(QuantizerTest, swingIsClampedToMaximum) {
    EXPECT_DOUBLE_EQ(quantizeBeat(0.5, 0.5, 200),
                     quantizeBeat(0.5, 0.5,
                                  app_view_models::Quantizer::maxSwingPercent));
}

TEST(QuantizerTest, tripletGrid) {
    EXPECT_NEAR(quantizeBeat(0.30, 1.0 / 3.0, 0), 1.0 / 3.0, 1e-9);
    EXPECT_NEAR(quantizeBeat(0.70, 1.0 / 3.0, 0), 2.0 / 3.0, 1e-9);
}

TEST(QuantizerTest, gridsAreOrderedAndDefaultIsSixteenth) {
    const auto &grids = app_view_models::Quantizer::getGrids();
    ASSERT_GT(grids.size(), 0);
    EXPECT_EQ(grids[app_view_models::Quantizer::getDefaultGridIndex()].name,
              "1/16");
}

class QuantizeClipTest : public ::testing::Test {
  protected:
    QuantizeClipTest() : edit(tracktion::Edit::createSingleTrackEdit(engine)) {}

    tracktion::MidiClip *makeClip(double startSeconds) {
        auto track = tracktion::getAudioTracks(*edit)[0];
        auto clip = track->insertNewClip(
            tracktion::TrackItem::Type::midi, "clip",
            tracktion::TimeRange(
                tracktion::TimePosition::fromSeconds(startSeconds),
                tracktion::TimePosition::fromSeconds(startSeconds + 8.0)),
            nullptr);
        return dynamic_cast<tracktion::MidiClip *>(clip);
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
};

TEST_F(QuantizeClipTest, movesNoteStartsAndKeepsLengths) {
    auto clip = makeClip(0.0);
    ASSERT_NE(clip, nullptr);
    auto &sequence = clip->getSequence();
    sequence.addNote(60, tracktion::BeatPosition::fromBeats(0.27),
                     tracktion::BeatDuration::fromBeats(0.4), 100, 0, nullptr);
    sequence.addNote(62, tracktion::BeatPosition::fromBeats(1.0),
                     tracktion::BeatDuration::fromBeats(0.3), 100, 0, nullptr);

    int moved = app_view_models::Quantizer::quantizeClip(*clip, 0.25, 0,
                                                         &edit->getUndoManager());
    EXPECT_EQ(moved, 1);

    auto notes = sequence.getNotes();
    ASSERT_EQ(notes.size(), 2);
    EXPECT_NEAR(notes[0]->getStartBeat().inBeats(), 0.25, 1e-9);
    EXPECT_NEAR(notes[0]->getLengthBeats().inBeats(), 0.4, 1e-9);
    EXPECT_NEAR(notes[1]->getStartBeat().inBeats(), 1.0, 1e-9);
}

TEST_F(QuantizeClipTest, alignsToEditGridNotClipStart) {
    // At 120 bpm, a clip starting at 0.3 s starts at beat 0.6. A note at the
    // very start of the clip is at edit beat 0.6, which snaps to 0.5 on a
    // 1/8 grid, i.e. before the clip content start, so it clamps to 0.
    // A note at content beat 0.95 (edit beat 1.55) should snap to edit beat
    // 1.5, i.e. content beat 0.9.
    auto clip = makeClip(0.3);
    ASSERT_NE(clip, nullptr);
    auto &sequence = clip->getSequence();
    sequence.addNote(60, tracktion::BeatPosition::fromBeats(0.95),
                     tracktion::BeatDuration::fromBeats(0.25), 100, 0, nullptr);

    app_view_models::Quantizer::quantizeClip(*clip, 0.5, 0, nullptr);

    double contentStart = clip->getContentStartBeat().inBeats();
    double editBeat =
        contentStart + sequence.getNotes()[0]->getStartBeat().inBeats();
    EXPECT_NEAR(std::fmod(editBeat, 0.5), 0.0, 1e-9);
}

TEST_F(QuantizeClipTest, canBeUndone) {
    auto clip = makeClip(0.0);
    ASSERT_NE(clip, nullptr);
    auto &sequence = clip->getSequence();
    sequence.addNote(60, tracktion::BeatPosition::fromBeats(0.27),
                     tracktion::BeatDuration::fromBeats(0.4), 100, 0, nullptr);

    edit->getUndoManager().beginNewTransaction("Quantize");
    app_view_models::Quantizer::quantizeClip(*clip, 0.25, 0,
                                             &edit->getUndoManager());
    EXPECT_NEAR(sequence.getNotes()[0]->getStartBeat().inBeats(), 0.25, 1e-9);

    edit->getUndoManager().undo();
    EXPECT_NEAR(sequence.getNotes()[0]->getStartBeat().inBeats(), 0.27, 1e-9);
}

} // namespace AppViewModelsTests
