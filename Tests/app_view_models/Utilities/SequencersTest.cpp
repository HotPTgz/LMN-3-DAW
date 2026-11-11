#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace app_view_models;
namespace Seq = app_view_models::Sequencers;

namespace {
Seq::StepInput stepInput() {
    Seq::StepInput input;
    input.length = 4;
    input.stepsPerBar = 4; // one beat per step
    input.notes[0].add(60);
    input.notes[2].add(64);
    input.notes[2].add(67);
    input.groove.gate = 0.5;
    return input;
}
} // namespace

TEST(SequencerEngineTest, stepPlaysNotesOnTheirSteps) {
    auto notes = Seq::generateStep(stepInput());
    ASSERT_EQ(notes.size(), 3u);
    EXPECT_EQ(notes[0].pitch, 60);
    EXPECT_DOUBLE_EQ(notes[0].start, 0.0);
    EXPECT_DOUBLE_EQ(notes[0].length, 0.5);
    EXPECT_DOUBLE_EQ(notes[1].start, 2.0);
    EXPECT_EQ(Seq::stepPasses(stepInput()), 1);
}

TEST(SequencerEngineTest, stepTricksTieRatchetAndVelocity) {
    auto input = stepInput();
    input.tricks[0].length = 3;
    input.tricks[0].velocity = 50;
    input.tricks[2].ratchet = 2;
    auto notes = Seq::generateStep(input);
    ASSERT_EQ(notes.size(), 5u);            // 1 + 2 pitches x 2 hits
    EXPECT_DOUBLE_EQ(notes[0].length, 2.5); // 2 whole steps + gate
    EXPECT_EQ(notes[0].velocity, 50);
    EXPECT_DOUBLE_EQ(notes[1].start, 2.0);
    EXPECT_DOUBLE_EQ(notes[3].start, 2.5);
    EXPECT_DOUBLE_EQ(notes[1].length, 0.25);
}

TEST(SequencerEngineTest, chanceMakesSeveralPassesAndZeroNeverPlays) {
    auto input = stepInput();
    input.tricks[0].chance = 0.0f;
    EXPECT_EQ(Seq::stepPasses(input), 4);
    auto notes = Seq::generateStep(input);
    for (auto &n : notes)
        EXPECT_NE(n.pitch, 60);
    EXPECT_EQ(notes.size(), 8u); // the 2 chord notes in each of 4 passes
}

TEST(SequencerEngineTest, directionsAndSwing) {
    auto input = stepInput();
    input.groove.direction = Seq::Direction::backward;
    auto notes = Seq::generateStep(input);
    // step 2 is played in slot 1, step 0 in slot 3
    EXPECT_DOUBLE_EQ(notes[0].start, 1.0);
    EXPECT_DOUBLE_EQ(notes.back().start, 3.0);

    input.groove.direction = Seq::Direction::pingPong;
    EXPECT_EQ(Seq::stepPasses(input), 2);
    EXPECT_EQ(Seq::generateStep(input).size(), 6u);

    EXPECT_DOUBLE_EQ(Seq::swingOffset(1, 1.0, 0.5), 0.25);
    EXPECT_DOUBLE_EQ(Seq::swingOffset(2, 1.0, 0.5), 0.0);
}

TEST(SequencerEngineTest, drumGridRoundTripsAndPlays) {
    Seq::DrumInput input;
    input.length = 16;
    input.stepsPerBar = 16;
    input.main.cells[0][0] = 3;
    input.main.cells[1][4] = 1;
    input.main.cells[23][15] = 2;
    auto copy = Seq::DrumGrid::fromString(input.main.toString());
    EXPECT_EQ(copy.cells[0][0], 3);
    EXPECT_EQ(copy.cells[23][15], 2);
    EXPECT_FALSE(copy.isEmpty());

    auto notes = Seq::generateDrums(input);
    ASSERT_EQ(notes.size(), 3u);
    EXPECT_EQ(notes[0].pitch, Seq::firstDrumNote);
    EXPECT_EQ(notes[0].velocity, 127);
    EXPECT_EQ(notes[1].pitch, Seq::firstDrumNote + 1);
    EXPECT_DOUBLE_EQ(notes[1].start, 1.0);
    EXPECT_LT(notes[1].velocity, 60);

    input.levels[0] = 0.5f;
    EXPECT_EQ(Seq::generateDrums(input)[0].velocity, 64);
}

TEST(SequencerEngineTest, drumFillReplacesTheLastPass) {
    Seq::DrumInput input;
    input.main.cells[0][0] = 2;
    input.fill.cells[1][8] = 2;
    input.fillEvery = 4;
    EXPECT_EQ(Seq::drumPasses(input), 4);
    auto notes = Seq::generateDrums(input);
    ASSERT_EQ(notes.size(), 4u); // 3 kicks, then the fill's hit
    EXPECT_EQ(notes.back().pitch, Seq::firstDrumNote + 1);
    EXPECT_DOUBLE_EQ(notes.back().start, 3 * 4.0 + 2.0);
    input.fill = {};
    EXPECT_EQ(Seq::drumPasses(input), 1);
}

TEST(SequencerEngineTest, endlessSkipsRestsAndTransposes) {
    Seq::EndlessInput input;
    input.notes = {60, -1, 64};
    input.stepLength = 0.5;
    input.transpose = 2;
    auto notes = Seq::generateEndless(input);
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].pitch, 62);
    EXPECT_DOUBLE_EQ(notes[1].start, 1.0);
    EXPECT_DOUBLE_EQ(Seq::endlessLength(input), 1.5);
}

TEST(SequencerEngineTest, tombolaIsTheSameEveryTimeAndStaysInside) {
    Seq::TombolaInput input;
    input.balls = {60, 64, 67};
    Seq::TombolaSimulation a(input), b(input);
    ASSERT_FALSE(a.getNotes().empty());
    ASSERT_EQ(a.getNotes().size(), b.getNotes().size());
    for (size_t i = 0; i < a.getNotes().size(); i++) {
        EXPECT_EQ(a.getNotes()[i].pitch, b.getNotes()[i].pitch);
        EXPECT_DOUBLE_EQ(a.getNotes()[i].start, b.getNotes()[i].start);
        EXPECT_LT(a.getNotes()[i].start, input.lengthBeats);
    }
    for (double beat = 0.0; beat < input.lengthBeats; beat += 0.37) {
        auto frame = a.frameAt(beat);
        ASSERT_EQ(frame.balls.size(), 3u);
        for (auto p : frame.balls)
            EXPECT_LT(p.getDistanceFromOrigin(), 1.0f);
    }
    int total = 0;
    for (auto h : a.getHitCounts())
        total += h;
    EXPECT_EQ(total, int(a.getNotes().size()));

    input.seed = 3;
    Seq::TombolaSimulation shaken(input);
    bool different = shaken.getNotes().size() != a.getNotes().size();
    for (size_t i = 0; !different && i < a.getNotes().size(); i++)
        different = shaken.getNotes()[i].start != a.getNotes()[i].start ||
                    shaken.getNotes()[i].pitch != a.getNotes()[i].pitch;
    EXPECT_TRUE(different);

    input.balls.clear();
    EXPECT_TRUE(Seq::TombolaSimulation(input).getNotes().empty());
}

TEST(SequencerEngineTest, patternsPlayInChainOrder) {
    Seq::PatternInput input;
    input.patterns[0].steps[0].add(60);
    input.patterns[1].steps[3].add(62);
    input.patterns[1].steps[3].add(65);
    input.chain = {0, 1, 0};
    input.stepsPerBar = 16;
    auto notes = Seq::generatePattern(input);
    ASSERT_EQ(notes.size(), 4u);
    EXPECT_DOUBLE_EQ(notes[0].start, 0.0);
    EXPECT_DOUBLE_EQ(notes[1].start, 4.0 + 0.75);
    EXPECT_DOUBLE_EQ(notes[3].start, 8.0);
    EXPECT_DOUBLE_EQ(Seq::patternLength(input), 12.0);

    auto copy = Seq::PatternData::fromString(input.patterns[1].toString());
    EXPECT_TRUE(copy.steps[3].contains(65));
    EXPECT_EQ(Seq::chainFromString(Seq::chainToString(input.chain)),
              input.chain);
    EXPECT_EQ(Seq::chainFromString("").size(), 1);
}

//==============================================================================
class SequencerViewModelsTest : public ::testing::Test {
  protected:
    SequencerViewModelsTest()
        : edit(tracktion::Edit::createSingleTrackEdit(engine)) {}
    tracktion::AudioTrack *track() {
        return tracktion::getAudioTracks(*edit)[0];
    }
    int clipNotes() {
        int count = 0;
        for (auto clip : track()->getClips())
            if (auto midi = dynamic_cast<tracktion::MidiClip *>(clip))
                count += midi->getSequence().getNumNotes();
        return count;
    }
    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
};

TEST_F(SequencerViewModelsTest, emptySequencerLeavesNoClip) {
    { DrumSequencerViewModel vm(track()); }
    EXPECT_EQ(track()->getClips().size(), 0);
}

TEST_F(SequencerViewModelsTest, drumsWriteTheClipAndRemember) {
    {
        DrumSequencerViewModel vm(track());
        vm.hitNote(Seq::firstDrumNote);
        vm.moveStep(4);
        vm.hitNote(Seq::firstDrumNote + 2);
        vm.nudgeVelocity(1);
        EXPECT_EQ(vm.getCell(2, 4), 3);
        EXPECT_EQ(clipNotes(), 2);
    }
    EXPECT_EQ(track()->getClips().size(), 1);
    DrumSequencerViewModel again(track());
    EXPECT_EQ(again.getCell(0, 0), 2);
    EXPECT_EQ(again.getCell(2, 4), 3);
}

TEST_F(SequencerViewModelsTest, endlessRecordsThenTransposes) {
    EndlessSequencerViewModel vm(track());
    vm.startRecording();
    vm.notePressed(60);
    vm.addRest();
    vm.notePressed(67);
    vm.finishRecording();
    EXPECT_EQ(vm.getNotes(), juce::Array<int>({60, -1, 67}));
    vm.notePressed(62);
    EXPECT_EQ(vm.getTranspose(), 2);
    EXPECT_EQ(clipNotes(), 2);
    vm.resetTranspose();
    EXPECT_EQ(vm.getTranspose(), 0);
}

TEST_F(SequencerViewModelsTest, tombolaBallsMakeNotes) {
    TombolaSequencerViewModel vm(track());
    for (int i = 0; i < 10; i++)
        vm.addBall(60 + i);
    EXPECT_EQ(vm.getBalls().size(), TombolaSequencerViewModel::maxBalls);
    EXPECT_EQ(vm.getBalls()[0], 62); // the oldest went
    EXPECT_GT(clipNotes(), 0);
    vm.popBall();
    EXPECT_EQ(vm.getBalls().size(), TombolaSequencerViewModel::maxBalls - 1);
}

TEST_F(SequencerViewModelsTest, patternChainEditing) {
    PatternSequencerViewModel vm(track());
    vm.toggleNote(60);
    EXPECT_EQ(vm.getSelectedStep(), 1);
    vm.copyToNext();
    EXPECT_EQ(vm.getEditPattern(), 1);
    EXPECT_TRUE(vm.getPattern(1).steps[0].contains(60));
    vm.addLink();
    vm.addLink();
    EXPECT_EQ(vm.getChain(), juce::Array<int>({0, 1, 1}));
    vm.nudgeLinkPattern(1);
    EXPECT_EQ(vm.getChain()[2], 2);
    vm.removeLink();
    EXPECT_EQ(vm.getChain().size(), 2);
    EXPECT_EQ(clipNotes(), 2);
}

TEST_F(SequencerViewModelsTest, stepTricksPersistAndChangeTheClip) {
    StepSequencerViewModel vm(track());
    vm.toggleNoteNumberAtSelectedIndex(60);
    vm.decrementSelectedNoteIndex();
    vm.nudgeRatchet(2);
    vm.nudgeVelocity(-5);
    auto tricks = vm.getTricks(0);
    EXPECT_EQ(tricks.ratchet, 3);
    EXPECT_EQ(tricks.velocity, 80);
    vm.nudgeChance(-4);
    EXPECT_EQ(vm.getPasses(), 4);
    vm.nudgeDirection(2);
    EXPECT_EQ(vm.getGroove().direction, Seq::Direction::pingPong);
}

} // namespace AppViewModelsTests
