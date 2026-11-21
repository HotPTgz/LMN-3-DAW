// Projects saved by older versions, where Drive and Filter's TYPE knob had
// overwritten the plugin's type, open with the effect back in place
#include <app_services/app_services.h>
#include <gtest/gtest.h>
#include <internal_plugins/internal_plugins.h>

namespace AppServicesTests {

namespace {
juce::ValueTree brokenPlugin(bool drive, const char *number) {
    juce::ValueTree v(tracktion::IDs::PLUGIN);
    v.setProperty(tracktion::IDs::type, number, nullptr);
    if (drive) {
        v.setProperty("drive", 12.0f, nullptr);
        v.setProperty("tone", 0.4f, nullptr);
        v.setProperty("mix", 1.0f, nullptr);
        v.setProperty("output", 0.0f, nullptr);
    } else {
        v.setProperty("cutoff", 0.3f, nullptr);
        v.setProperty("resonance", 0.5f, nullptr);
        v.setProperty("lfoRate", 3.0f, nullptr);
    }
    return v;
}
} // namespace

TEST(ProjectRepairTest, brokenDriveAndFilterGetTheirTypeBack) {
    juce::ValueTree edit(tracktion::IDs::EDIT);
    juce::ValueTree track(tracktion::IDs::TRACK);
    track.appendChild(brokenPlugin(true, "2"), nullptr);
    track.appendChild(brokenPlugin(false, "4"), nullptr);
    juce::ValueTree fine(tracktion::IDs::PLUGIN);
    fine.setProperty(tracktion::IDs::type, "volume", nullptr);
    track.appendChild(fine, nullptr);
    edit.appendChild(track, nullptr);

    EXPECT_EQ(app_services::repairProjectState(edit), 2);
    EXPECT_EQ(track.getChild(0)[tracktion::IDs::type].toString(), "lmn3Drive");
    EXPECT_EQ(float(track.getChild(0)["driveType"]), 2.0f);
    EXPECT_EQ(track.getChild(1)[tracktion::IDs::type].toString(), "lmn3Filter");
    EXPECT_EQ(float(track.getChild(1)["filterType"]), 4.0f);
    EXPECT_EQ(track.getChild(2)[tracktion::IDs::type].toString(), "volume");
    // nothing more to mend the second time
    EXPECT_EQ(app_services::repairProjectState(edit), 0);
}

TEST(ProjectRepairTest, aBrokenProjectOpensWithTheDriveInIt) {
    tracktion::Engine engine{"ENGINE"};
    engine.getPluginManager().createBuiltInType<internal_plugins::DrivePlugin>();
    auto file = juce::File::createTempFile(".tracktionedit");
    {
        auto edit = tracktion::createEmptyEdit(engine, file);
        edit->ensureNumberOfAudioTracks(1);
        auto track = tracktion::getAudioTracks(*edit)[0];
        track->pluginList.insertPlugin(
            edit->getPluginCache().createNewPlugin(
                internal_plugins::DrivePlugin::xmlTypeName, {}),
            0, nullptr);
        tracktion::EditFileOperations(*edit).save(false, true, false);
    }
    // what the old version wrote: the TYPE knob (Fuzz = 2) over the type
    auto xml = file.loadFileAsString().replace("type=\"lmn3Drive\"",
                                               "type=\"2.0\"");
    ASSERT_TRUE(xml.contains("type=\"2.0\"")) << file.loadFileAsString().substring(0, 600);
    ASSERT_TRUE(file.replaceWithText(xml));

    auto edit = app_services::loadProject(engine, file);
    ASSERT_NE(edit, nullptr);
    internal_plugins::DrivePlugin *drive = nullptr;
    for (auto p : tracktion::getAudioTracks(*edit)[0]->pluginList)
        if (auto d = dynamic_cast<internal_plugins::DrivePlugin *>(p))
            drive = d;
    ASSERT_NE(drive, nullptr);
    EXPECT_EQ(drive->getChoice("driveType"), 2);
    edit.reset();
    file.deleteFile();
}

} // namespace AppServicesTests
