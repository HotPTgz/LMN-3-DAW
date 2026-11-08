#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using app_view_models::RenderPlanner::RenderType;

class RenderPlannerTest : public ::testing::Test {
  protected:
    RenderPlannerTest() {
        rendersDirectory.createDirectory();
        auto editFile = tempRoot.getChildFile("My Song.tracktionedit");
        edit = tracktion::createEmptyEdit(engine, editFile);
        edit->ensureNumberOfAudioTracks(3);
    }

    ~RenderPlannerTest() override { tempRoot.deleteRecursively(); }

    void addClip(int trackIndex) {
        auto track = tracktion::getAudioTracks(*edit)[trackIndex];
        track->insertNewClip(
            tracktion::TrackItem::Type::midi, "clip",
            tracktion::TimeRange(tracktion::TimePosition::fromSeconds(0.0),
                                 tracktion::TimePosition::fromSeconds(4.0)),
            nullptr);
    }

    int indexInAllTracks(int audioTrackIndex) {
        auto audioTrack = tracktion::getAudioTracks(*edit)[audioTrackIndex];
        return tracktion::getAllTracks(*edit).indexOf(audioTrack);
    }

    tracktion::Engine engine{"ENGINE"};
    juce::File tempRoot = juce::File::createTempFile("lmn3-render-test");
    juce::File rendersDirectory = tempRoot.getChildFile("renders");
    std::unique_ptr<tracktion::Edit> edit;
    juce::Time time{2026, 10, 8, 13, 5, 12}; // month is 0-based
};

TEST_F(RenderPlannerTest, mixRendersAllTracksIntoOneNamedFile) {
    auto jobs = app_view_models::RenderPlanner::planRender(
        *edit, RenderType::mix, rendersDirectory, time);
    ASSERT_EQ(jobs.size(), 1);
    EXPECT_EQ(jobs[0].file.getFileName(), "My Song_mix_2026-11-08_130512.wav");
    EXPECT_EQ(jobs[0].file.getParentDirectory(), rendersDirectory);
    EXPECT_EQ(jobs[0].tracksToDo.countNumberOfSetBits(),
              tracktion::getAllTracks(*edit).size());
}

TEST_F(RenderPlannerTest, stemsSkipEmptyAndMutedTracks) {
    addClip(0);
    addClip(2);
    addClip(1);
    tracktion::getAudioTracks(*edit)[1]->setMute(true);
    tracktion::getAudioTracks(*edit)[2]->setName("Bass/Synth");

    auto jobs = app_view_models::RenderPlanner::planRender(
        *edit, RenderType::stems, rendersDirectory, time);
    ASSERT_EQ(jobs.size(), 2);

    EXPECT_EQ(jobs[0].file.getParentDirectory().getFileName(),
              "My Song_stems_2026-11-08_130512");
    EXPECT_TRUE(jobs[0].file.getFileName().startsWith("01_"));
    EXPECT_EQ(jobs[0].tracksToDo.countNumberOfSetBits(), 1);
    EXPECT_TRUE(jobs[0].tracksToDo[indexInAllTracks(0)]);

    // Track names are made safe for file names
    EXPECT_TRUE(jobs[1].file.getFileName().startsWith("02_"));
    EXPECT_FALSE(jobs[1].file.getFileName().containsChar('/'));
    EXPECT_TRUE(jobs[1].tracksToDo[indexInAllTracks(2)]);
}

TEST_F(RenderPlannerTest, usesLoopRangeWhenLooping) {
    addClip(0);
    auto &transport = edit->getTransport();
    transport.setLoopRange(
        tracktion::TimeRange(tracktion::TimePosition::fromSeconds(1.0),
                             tracktion::TimePosition::fromSeconds(3.0)));

    transport.looping.setValue(false, nullptr);
    EXPECT_FALSE(app_view_models::RenderPlanner::isLoopRender(*edit));
    EXPECT_DOUBLE_EQ(app_view_models::RenderPlanner::getRenderRange(*edit)
                         .getStart()
                         .inSeconds(),
                     0.0);

    transport.looping.setValue(true, nullptr);
    EXPECT_TRUE(app_view_models::RenderPlanner::isLoopRender(*edit));
    auto range = app_view_models::RenderPlanner::getRenderRange(*edit);
    EXPECT_DOUBLE_EQ(range.getStart().inSeconds(), 1.0);
    EXPECT_DOUBLE_EQ(range.getEnd().inSeconds(), 3.0);

    auto jobs = app_view_models::RenderPlanner::planRender(
        *edit, RenderType::mix, rendersDirectory, time);
    EXPECT_EQ(jobs[0].file.getFileName(),
              "My Song_mix-loop_2026-11-08_130512.wav");
}

TEST_F(RenderPlannerTest, doesNotOverwriteExistingRenders) {
    rendersDirectory.getChildFile("My Song_mix_2026-11-08_130512.wav").create();
    auto jobs = app_view_models::RenderPlanner::planRender(
        *edit, RenderType::mix, rendersDirectory, time);
    EXPECT_FALSE(jobs[0].file.exists());
}

} // namespace AppViewModelsTests
