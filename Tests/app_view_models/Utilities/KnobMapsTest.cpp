#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace app_view_models;

TEST(KnobMapsTest, parsesPagesAndKnobs) {
    auto map = KnobMaps::parse(R"(
plugin: Test Synth
match: [testsynth, test synth]
pages:
  - name: Filter
    knobs:
      - CUTOFF = Cutoff
      - RES = Resonance|Reso
      - ""
      - Drive
)");
    ASSERT_TRUE(map.isValid());
    EXPECT_EQ(map.plugin, "Test Synth");
    ASSERT_EQ(map.pages.size(), 1u);
    auto &page = map.pages[0];
    EXPECT_EQ(page.name, "Filter");
    ASSERT_EQ(page.knobs.size(), 4u);
    EXPECT_EQ(page.knobs[0].label, "CUTOFF");
    EXPECT_EQ(page.knobs[1].names, juce::StringArray({"Resonance", "Reso"}));
    EXPECT_TRUE(page.knobs[2].isEmpty());
    // a bare name is its own label
    EXPECT_EQ(page.knobs[3].label, "DRIVE");
    EXPECT_EQ(page.knobs[3].names[0], "Drive");
}

TEST(KnobMapsTest, badTextGivesAnInvalidMap) {
    EXPECT_FALSE(KnobMaps::parse("pages: [: : :").isValid());
    EXPECT_FALSE(KnobMaps::parse("just some words").isValid());
    EXPECT_FALSE(KnobMaps::parse("plugin: X\npages: []").isValid());
}

TEST(KnobMapsTest, matchesPluginNamesInAnyCase) {
    auto map = KnobMaps::parse("plugin: OB-Xd\nmatch: [obxd, ob-xd]\n"
                               "pages:\n  - name: A\n    knobs: [Cutoff]\n");
    EXPECT_TRUE(map.matches("OB-Xd"));
    EXPECT_TRUE(map.matches("Obxd"));
    EXPECT_TRUE(map.matches("discoDSP OB-Xd 2"));
    EXPECT_FALSE(map.matches("Dexed"));
}

TEST(KnobMapsTest, findsParametersIgnoringCaseAndSpaces) {
    juce::StringArray names{"ALGORITHM", "OP1 Output Level", "Lfo Rate"};
    KnobMaps::Knob level{"OP 1", {"OP1 OUTPUT LEVEL"}};
    KnobMaps::Knob speed{"SPEED", {"LFO SPEED", "Lfo Rate"}};
    KnobMaps::Knob missing{"X", {"Nope"}};
    EXPECT_EQ(KnobMaps::findParameter(names, level), 1);
    EXPECT_EQ(KnobMaps::findParameter(names, speed), 2);
    EXPECT_EQ(KnobMaps::findParameter(names, missing), -1);
}

TEST(KnobMapsTest, builtInMapsAreValid) {
    for (auto &[fileName, text] : KnobMaps::getDefaultMaps()) {
        auto map = KnobMaps::parse(text);
        EXPECT_TRUE(map.isValid()) << fileName;
        for (auto &page : map.pages)
            EXPECT_LE(page.knobs.size(), 8u) << fileName << " " << page.name;
    }
    auto obxd = KnobMaps::parse(KnobMaps::getDefaultMaps()[0].second);
    EXPECT_TRUE(obxd.matches("Obxd"));
    EXPECT_EQ(obxd.pages.size(), 7u);
    auto dexed = KnobMaps::parse(KnobMaps::getDefaultMaps()[1].second);
    EXPECT_TRUE(dexed.matches("Dexed"));
    EXPECT_EQ(dexed.pages[0].knobs[0].label, "ALGORITHM");
}

TEST(KnobMapsTest, installsDefaultsWithoutOverwritingEdits) {
    juce::TemporaryFile temp;
    auto dir = temp.getFile();
    dir.createDirectory();
    KnobMaps::installDefaults(dir);
    auto obxdFile = dir.getChildFile("OB-Xd.yaml");
    ASSERT_TRUE(obxdFile.existsAsFile());
    ASSERT_TRUE(dir.getChildFile("Dexed.yaml").existsAsFile());

    obxdFile.replaceWithText("plugin: OB-Xd\nmatch: [obxd]\npages:\n"
                             "  - name: MINE\n    knobs: [Cutoff]\n");
    KnobMaps::installDefaults(dir);
    auto map = KnobMaps::findMapFor("Obxd", dir);
    ASSERT_TRUE(map.isValid());
    EXPECT_EQ(map.pages[0].name, "MINE");
    EXPECT_FALSE(KnobMaps::findMapFor("Surge XT", dir).isValid());
    dir.deleteRecursively();
}

class ExternalPluginViewModelTest : public ::testing::Test {
  protected:
    ExternalPluginViewModelTest()
        : edit(tracktion::Edit::createSingleTrackEdit(engine)) {
        auto track = tracktion::getAudioTracks(*edit)[0];
        reverb = edit->getPluginCache().createNewPlugin(
            tracktion::ReverbPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin(reverb, 0, nullptr);
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
    tracktion::Plugin::Ptr reverb;
};

TEST_F(ExternalPluginViewModelTest, mapPagesFirstThenEverythingElse) {
    auto map = KnobMaps::parse(R"(
plugin: Reverb
pages:
  - name: SPACE
    knobs:
      - ROOM = Room Size
      - ""
      - WET = Wet Level
      - GONE = Not A Parameter
)");
    ExternalPluginViewModel vm(reverb, map);
    ASSERT_TRUE(vm.hasMap());
    auto &pages = vm.getPages();
    ASSERT_GE(pages.size(), 2u);
    EXPECT_EQ(pages[0].name, "SPACE");
    EXPECT_EQ(pages[0].knobs[0].label, "ROOM");
    ASSERT_NE(pages[0].knobs[0].parameter, nullptr);
    EXPECT_EQ(pages[0].knobs[0].parameter->getParameterName(), "Room Size");
    EXPECT_EQ(pages[0].knobs[1].parameter, nullptr);
    EXPECT_EQ(pages[0].knobs[3].parameter, nullptr);
    EXPECT_EQ(pages[1].name, "MORE 1");

    // every parameter shows up exactly once
    int count = 0;
    for (auto &page : pages)
        for (auto &knob : page.knobs)
            if (knob.parameter != nullptr) {
                count++;
                if (&page != &pages[0])
                    EXPECT_NE(knob.parameter->getParameterName(), "Room Size");
            }
    int visible = 0;
    for (auto p : reverb->getAutomatableParameters())
        if (!ExternalPluginViewModel::isHidden(p->getParameterName()))
            visible++;
    EXPECT_EQ(count, visible);
}

TEST_F(ExternalPluginViewModelTest, withoutAMapEveryParameterEightToAPage) {
    ExternalPluginViewModel vm(reverb, {});
    EXPECT_FALSE(vm.hasMap());
    ASSERT_FALSE(vm.getPages().empty());
    EXPECT_EQ(vm.getPages()[0].name, "PAGE 1");
    EXPECT_EQ(vm.getPages()[0].knobs[0].label,
              reverb->getAutomatableParameters()[0]
                  ->getParameterName()
                  .toUpperCase());
}

TEST_F(ExternalPluginViewModelTest, nudgeMovesAndClamps) {
    ExternalPluginViewModel vm(reverb, {});
    auto parameter = vm.getPages()[0].knobs[0].parameter;
    ASSERT_NE(parameter, nullptr);
    parameter->setNormalisedParameter(0.5f, juce::sendNotification);
    ExternalPluginViewModel::nudge(*parameter, 1);
    EXPECT_NEAR(ExternalPluginViewModel::getValue(*parameter), 0.51f, 0.001f);
    for (int i = 0; i < 200; i++)
        ExternalPluginViewModel::nudge(*parameter, -1);
    EXPECT_FLOAT_EQ(ExternalPluginViewModel::getValue(*parameter), 0.0f);
    EXPECT_TRUE(ExternalPluginViewModel::getText(*parameter).isNotEmpty());
}

TEST_F(ExternalPluginViewModelTest, builtInPluginsHaveNoPresets) {
    ExternalPluginViewModel vm(reverb, {});
    EXPECT_EQ(vm.getNumPresets(), 0);
    vm.loadPreset(3); // harmless
}

TEST_F(ExternalPluginViewModelTest, hidesMidiCcHelpers) {
    EXPECT_TRUE(ExternalPluginViewModel::isHidden("MIDI CC 0|7"));
    EXPECT_TRUE(ExternalPluginViewModel::isHidden("Bypass"));
    EXPECT_TRUE(ExternalPluginViewModel::isHidden("Unused 1"));
    EXPECT_FALSE(ExternalPluginViewModel::isHidden("Cutoff"));
}

TEST(PluginScannerTest, scansOnceThenUsesTheCache) {
    tracktion::Engine engine{"ENGINE"};
    juce::TemporaryFile tempDir, tempCache(".xml");
    auto folder = tempDir.getFile();
    folder.createDirectory();
    auto cache = tempCache.getFile();
    juce::Array<juce::File> folders{folder};

    EXPECT_TRUE(PluginScanner::loadOrScan(engine, cache, folders));
    EXPECT_TRUE(cache.existsAsFile());
    EXPECT_FALSE(PluginScanner::loadOrScan(engine, cache, folders));

    // a new plugin in the folder means a new scan
    auto before = PluginScanner::fingerprint(folders);
    folder.getChildFile("New.vst3").createDirectory();
    EXPECT_NE(PluginScanner::fingerprint(folders), before);
    EXPECT_TRUE(PluginScanner::loadOrScan(engine, cache, folders));
    EXPECT_FALSE(PluginScanner::loadOrScan(engine, cache, folders));

    // so does an updated plugin, even if only a file inside it changed
    auto library = folder.getChildFile("New.vst3").getChildFile("lib.so");
    library.replaceWithText("v1");
    library.setLastModificationTime(juce::Time::getCurrentTime() +
                                    juce::RelativeTime::minutes(1));
    EXPECT_TRUE(PluginScanner::loadOrScan(engine, cache, folders));
    library.setLastModificationTime(juce::Time::getCurrentTime() +
                                    juce::RelativeTime::minutes(2));
    EXPECT_TRUE(PluginScanner::loadOrScan(engine, cache, folders));
    EXPECT_FALSE(PluginScanner::loadOrScan(engine, cache, folders));
    folder.deleteRecursively();
}

} // namespace AppViewModelsTests
