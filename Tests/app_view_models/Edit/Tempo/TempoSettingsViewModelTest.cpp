#include "MockTempoSettingsViewModelListener.h"
#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>
namespace AppViewModelsTests {

class TempoSettingsViewModelTest : public ::testing::Test {
  protected:
    TempoSettingsViewModelTest()
        : edit(tracktion::Edit::createSingleTrackEdit(engine)),
          viewModel(*edit) {}

    void SetUp() override {
        // flush any pending updates
        viewModel.handleUpdateNowIfNeeded();
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
    app_view_models::TempoSettingsViewModel viewModel;
};

using ::testing::_;
TEST_F(TempoSettingsViewModelTest, setBpm) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, bpmChanged(120, _)).Times(1);

    EXPECT_CALL(listener, bpmChanged(80, _)).Times(1);

    viewModel.addListener(&listener);

    viewModel.setBpm(80);
    viewModel.handleUpdateNowIfNeeded();
}

TEST_F(TempoSettingsViewModelTest, setClickTrackGain) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, clickTrackGainChanged(_)).Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(0.5)).Times(1);

    viewModel.addListener(&listener);

    viewModel.setClickTrackGain(0.5);
    viewModel.handleUpdateNowIfNeeded();
}

TEST_F(TempoSettingsViewModelTest, incrementBpm) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, bpmChanged(120, _)).Times(1);

    EXPECT_CALL(listener, bpmChanged(121, _)).Times(1);

    EXPECT_CALL(listener, bpmChanged(viewModel.bpmUpperLimit, _)).Times(1);

    viewModel.addListener(&listener);

    viewModel.incrementBpm();
    viewModel.handleUpdateNowIfNeeded();

    viewModel.setBpm(viewModel.bpmUpperLimit);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.incrementBpm();
    viewModel.handleUpdateNowIfNeeded();
}

TEST_F(TempoSettingsViewModelTest, decrementBpm) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, bpmChanged(120, _)).Times(1);

    EXPECT_CALL(listener, bpmChanged(119, _)).Times(1);

    EXPECT_CALL(listener, bpmChanged(viewModel.bpmLowerLimit, _)).Times(1);

    viewModel.addListener(&listener);

    viewModel.decrementBpm();
    viewModel.handleUpdateNowIfNeeded();

    viewModel.setBpm(viewModel.bpmLowerLimit);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.decrementBpm();
    viewModel.handleUpdateNowIfNeeded();
}

using ::testing::DoubleEq;
using ::testing::DoubleNear;
TEST_F(TempoSettingsViewModelTest, incrementClickTrackGain) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, clickTrackGainChanged(_)).Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(0.5, .0001)))
        .Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(.52, .0001)))
        .Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(
                              viewModel.clickTrackGainUpperLimit, .0001)))
        .Times(1);

    viewModel.addListener(&listener);

    viewModel.setClickTrackGain(0.5);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.incrementClickTrackGain();
    viewModel.handleUpdateNowIfNeeded();

    viewModel.setClickTrackGain(viewModel.clickTrackGainUpperLimit);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.incrementClickTrackGain();
    viewModel.handleUpdateNowIfNeeded();
}

TEST_F(TempoSettingsViewModelTest, decrementClickTrackGain) {
    MockTempoSettingsViewModelListener listener;

    // called once when added listener added
    EXPECT_CALL(listener, clickTrackGainChanged(_)).Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(0.5, .0001)))
        .Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(.48, .0001)))
        .Times(1);

    EXPECT_CALL(listener, clickTrackGainChanged(DoubleNear(
                              viewModel.clickTrackGainLowerLimit, .0001)))
        .Times(1);

    viewModel.addListener(&listener);

    viewModel.setClickTrackGain(0.5);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.decrementClickTrackGain();
    viewModel.handleUpdateNowIfNeeded();

    viewModel.setClickTrackGain(viewModel.clickTrackGainLowerLimit);
    viewModel.handleUpdateNowIfNeeded();

    viewModel.decrementClickTrackGain();
    viewModel.handleUpdateNowIfNeeded();
}

TEST_F(TempoSettingsViewModelTest, clipsAndLoopKeepTheirBeatsWhenTheTempoChanges) {
    auto track = tracktion::getAudioTracks(*edit)[0];
    // a 4 beat clip and loop at 120 bpm (2 seconds)
    auto clip = track->insertMIDIClip(
        tracktion::TimeRange(tracktion::TimePosition(),
                             tracktion::TimeDuration::fromSeconds(2.0)),
        nullptr);
    auto &transport = edit->getTransport();
    transport.setLoopRange(tracktion::TimeRange(
        tracktion::TimePosition(), tracktion::TimeDuration::fromSeconds(2.0)));
    auto &tempo = edit->tempoSequence;
    auto beats = [&](tracktion::TimeRange r) {
        return (tempo.toBeats(r.getEnd()) - tempo.toBeats(r.getStart())).inBeats();
    };
    for (auto change : {0, 1, 2}) {
        if (change == 0)
            viewModel.setBpm(60);
        else if (change == 1)
            viewModel.incrementBpm();
        else
            viewModel.decrementBpm();
        EXPECT_NEAR(beats(clip->getEditTimeRange()), 4.0, 1.0e-6)
            << "the clip lost beats after a tempo change";
        EXPECT_NEAR(beats(transport.getLoopRange()), 4.0, 1.0e-6)
            << "the loop lost beats after a tempo change";
    }
    EXPECT_NEAR(clip->getEditTimeRange().getLength().inSeconds(), 4.0, 1.0e-6);
}

} // namespace AppViewModelsTests
