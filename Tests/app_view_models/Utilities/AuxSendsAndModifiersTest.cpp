#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

class AuxSendsTest : public ::testing::Test {
  protected:
    AuxSendsTest() : edit(tracktion::Edit::createSingleTrackEdit(engine)) {
        edit->ensureNumberOfAudioTracks(2);
    }

    tracktion::AudioTrack *track(int i) {
        return tracktion::getAudioTracks(*edit)[i];
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
};

using namespace app_view_models;

TEST_F(AuxSendsTest, firstNudgeCreatesSendAndReturnTrack) {
    EXPECT_EQ(AuxSends::findReturnTrack(*edit, 0), nullptr);
    EXPECT_FLOAT_EQ(AuxSends::getSendLevel(*track(0), 0), 0.0f);

    ASSERT_TRUE(AuxSends::nudgeSendLevel(*track(0), 0, AuxSends::levelStep));

    auto fxA = AuxSends::findReturnTrack(*edit, 0);
    ASSERT_NE(fxA, nullptr);
    EXPECT_EQ(fxA->getName(), "FX A");
    EXPECT_TRUE(AuxSends::isReturnTrack(*fxA));
    EXPECT_EQ(fxA->pluginList.getPluginsOfType<tracktion::ReverbPlugin>().size(),
              1);
    EXPECT_FLOAT_EQ(AuxSends::getSendLevel(*track(0), 0), AuxSends::levelStep);
}

TEST_F(AuxSendsTest, sendIsPostFader) {
    AuxSends::nudgeSendLevel(*track(0), 0, 0.5f);
    auto &list = track(0)->pluginList;
    auto send = AuxSends::findSend(*track(0), 0);
    auto volume = EngineHelpers::getVolumeAndPanPluginForTrack(track(0));
    ASSERT_NE(send, nullptr);
    EXPECT_EQ(list.indexOf(send), list.indexOf(volume) + 1);
}

TEST_F(AuxSendsTest, busesAreIndependentAndReused) {
    AuxSends::nudgeSendLevel(*track(0), 0, 0.2f);
    AuxSends::nudgeSendLevel(*track(1), 0, 0.4f);
    AuxSends::nudgeSendLevel(*track(0), 1, 0.6f);

    // one return track per bus, not per send
    int returns = 0;
    for (auto t : tracktion::getAudioTracks(*edit))
        returns += AuxSends::isReturnTrack(*t) ? 1 : 0;
    EXPECT_EQ(returns, 2);

    EXPECT_NEAR(AuxSends::getSendLevel(*track(0), 0), 0.2f, 1e-6);
    EXPECT_NEAR(AuxSends::getSendLevel(*track(1), 0), 0.4f, 1e-6);
    EXPECT_NEAR(AuxSends::getSendLevel(*track(0), 1), 0.6f, 1e-6);
    EXPECT_EQ(AuxSends::findReturnTrack(*edit, 1)->getName(), "FX B");
}

TEST_F(AuxSendsTest, levelIsClampedAndTurningDownDoesNotCreateSend) {
    EXPECT_TRUE(AuxSends::nudgeSendLevel(*track(0), 0, -0.1f));
    EXPECT_EQ(AuxSends::findSend(*track(0), 0), nullptr);
    EXPECT_EQ(AuxSends::findReturnTrack(*edit, 0), nullptr);

    AuxSends::nudgeSendLevel(*track(0), 0, 5.0f);
    EXPECT_FLOAT_EQ(AuxSends::getSendLevel(*track(0), 0), 1.0f);
}

TEST_F(AuxSendsTest, returnTracksAndMasterCannotHaveSends) {
    AuxSends::nudgeSendLevel(*track(0), 0, 0.2f);
    auto fxA = AuxSends::findReturnTrack(*edit, 0);
    EXPECT_FALSE(AuxSends::nudgeSendLevel(*fxA, 0, 0.2f));
    EXPECT_EQ(AuxSends::findSend(*fxA, 0), nullptr);

    if (auto master = edit->getMasterTrack())
        EXPECT_FALSE(AuxSends::nudgeSendLevel(*master, 0, 0.2f));
}

//==============================================================================
class ModifierViewModelTest : public ::testing::Test {
  protected:
    ModifierViewModelTest()
        : edit(tracktion::Edit::createSingleTrackEdit(engine)),
          track(tracktion::getAudioTracks(*edit)[0]) {}

    tracktion::Modifier::Ptr add(const juce::Identifier &type) {
        auto m = track->getModifierList().insertModifier(juce::ValueTree(type),
                                                         -1, nullptr);
        applyModifierDefaults(m.get());
        return m;
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
    tracktion::AudioTrack *track;
};

TEST_F(ModifierViewModelTest, listContainsAllModifierTypes) {
    ModifierList list(*edit);
    auto ids = list.getModifierIdentifiers();
    for (auto id : {tracktion::IDs::LFO, tracktion::IDs::STEP,
                    tracktion::IDs::RANDOM, tracktion::IDs::ENVELOPEFOLLOWER,
                    tracktion::IDs::MIDITRACKER})
        EXPECT_TRUE(ids.contains(id)) << id.toString();
}

TEST_F(ModifierViewModelTest, stepDefaultsAreTempoSyncedSixteenths) {
    auto m = add(tracktion::IDs::STEP);
    auto vm = createModifierViewModel(m.get());
    ASSERT_GE(vm->getNumberOfParameters(), 4);
    EXPECT_EQ(vm->getParameterName(0), "Rate");
    EXPECT_EQ(vm->getParameterValueText(0, vm->getParameterValue(0)), "1/16");
    EXPECT_EQ(vm->getParameterValueText(3, vm->getParameterValue(3)),
              "Transport");
    auto step = dynamic_cast<tracktion::StepModifier *>(m.get());
    EXPECT_FLOAT_EQ(step->numSteps.get(), 16.0f);
}

TEST_F(ModifierViewModelTest, settingValuesIsClampedToRange) {
    auto m = add(tracktion::IDs::RANDOM);
    auto vm = createModifierViewModel(m.get());
    int depth = 1;
    EXPECT_EQ(vm->getParameterName(depth), "Depth");
    vm->setParameterValue(depth, 5.0);
    EXPECT_DOUBLE_EQ(vm->getParameterValue(depth),
                     vm->getParameterRange(depth).getEnd());
}

TEST_F(ModifierViewModelTest, everyModifierTypeGetsKnobs) {
    for (auto id : {tracktion::IDs::LFO, tracktion::IDs::STEP,
                    tracktion::IDs::RANDOM, tracktion::IDs::ENVELOPEFOLLOWER,
                    tracktion::IDs::MIDITRACKER}) {
        auto m = add(id);
        auto vm = createModifierViewModel(m.get());
        EXPECT_GT(vm->getNumberOfParameters(), 0) << id.toString();
        EXPECT_LE(vm->getNumberOfParameters(), 8) << id.toString();
    }
}

TEST_F(ModifierViewModelTest, lfoShowsWaveNames) {
    auto m = add(tracktion::IDs::LFO);
    auto vm = createModifierViewModel(m.get());
    EXPECT_EQ(vm->getParameterValueText(0, 1.0),
              tracktion::LFOModifier::getWaveNames()[1]);
}

} // namespace AppViewModelsTests
