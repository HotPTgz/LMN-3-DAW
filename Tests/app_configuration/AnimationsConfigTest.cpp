#include <app_configuration/app_configuration.h>
#include <gtest/gtest.h>

namespace AppConfigurationTests {

class AnimationsConfigTest : public ::testing::Test {
  protected:
    ~AnimationsConfigTest() override { configFile.deleteFile(); }

    bool animationsWith(const juce::String &yaml) {
        configFile.replaceWithText(yaml);
        return ConfigurationHelpers::getAnimations(configFile);
    }

    juce::File configFile = juce::File::createTempFile("lmn3-config-test.yaml");
};

TEST_F(AnimationsConfigTest, onWithoutAConfigFile) {
    configFile.deleteFile();
    EXPECT_TRUE(ConfigurationHelpers::getAnimations(configFile));
}

TEST_F(AnimationsConfigTest, onWhenNotMentioned) {
    EXPECT_TRUE(animationsWith("config:\n  show-title-bar: false\n"));
}

TEST_F(AnimationsConfigTest, canBeTurnedOff) {
    EXPECT_FALSE(animationsWith("config:\n  animations: false\n"));
    EXPECT_TRUE(animationsWith("config:\n  animations: true\n"));
}

TEST_F(AnimationsConfigTest, onWhenTheFileIsBroken) {
    EXPECT_TRUE(animationsWith("config: [unclosed\n"));
    EXPECT_TRUE(animationsWith("config:\n  animations: maybe\n"));
}

} // namespace AppConfigurationTests
