// Control + Play opens the help card; the next button, encoder or key
// closes it and does nothing else
#include <app_services/app_services.h>
#include <gtest/gtest.h>

namespace AppServicesTests {
using MCM = app_services::MidiCommandManager;

namespace {
// A screen that counts what reaches it, and the edit view that opens help
struct Screen : juce::Component, MCM::Listener {
    int plays = 0, stops = 0, turns = 0, notes = 0;
    void playButtonReleased() override { plays++; }
    void stopButtonReleased() override { stops++; }
    void encoder1Increased() override { turns++; }
    void noteOnPressed(int) override { notes++; }
};
struct HelpHost : MCM::Listener {
    MCM &mcm;
    int opened = 0, closed = 0;
    explicit HelpHost(MCM &m) : mcm(m) {}
    void helpButtonPressed() override {
        opened++;
        mcm.isHelpOpen = true;
    }
    void helpClosed() override { closed++; }
};

juce::MidiMessage cc(int number, int value) {
    return juce::MidiMessage::controllerEvent(1, number, value);
}
} // namespace

class HelpButtonTest : public ::testing::Test {
  protected:
    HelpButtonTest() : mcm(engine), host(mcm) {
        mcm.setFocusedComponent(&screen);
        mcm.addListener(&host);
    }
    ~HelpButtonTest() override { mcm.removeListener(&host); }
    void send(const juce::MidiMessage &m) { mcm.midiMessageReceived(m, "test"); }
    void openHelp() {
        send(cc(MCM::CONTROL_BUTTON, 127));
        send(cc(MCM::PLAY_BUTTON, 127));
        send(cc(MCM::PLAY_BUTTON, 0));
        send(cc(MCM::CONTROL_BUTTON, 0));
    }

    tracktion::Engine engine{"ENGINE"};
    MCM mcm;
    Screen screen;
    HelpHost host;
};

TEST_F(HelpButtonTest, controlPlayOpensHelpInsteadOfPlaying) {
    openHelp();
    EXPECT_EQ(host.opened, 1);
    EXPECT_TRUE(mcm.isHelpOpen);
    EXPECT_EQ(screen.plays, 0);
    EXPECT_FALSE(mcm.isControlDown);
}

TEST_F(HelpButtonTest, nextButtonOnlyClosesIt) {
    openHelp();
    send(cc(MCM::STOP_BUTTON, 127));
    send(cc(MCM::STOP_BUTTON, 0));
    EXPECT_EQ(host.closed, 1);
    EXPECT_FALSE(mcm.isHelpOpen);
    EXPECT_EQ(screen.stops, 0);
    // and afterwards buttons work again
    send(cc(MCM::STOP_BUTTON, 127));
    send(cc(MCM::STOP_BUTTON, 0));
    EXPECT_EQ(screen.stops, 1);
}

TEST_F(HelpButtonTest, encoderAndKeyCloseIt) {
    openHelp();
    send(cc(MCM::ENCODER_1, 1));
    EXPECT_EQ(host.closed, 1);
    EXPECT_EQ(screen.turns, 0);

    openHelp();
    send(juce::MidiMessage::noteOn(1, 60, 0.8f));
    send(juce::MidiMessage::noteOff(1, 60));
    EXPECT_EQ(host.closed, 2);
    EXPECT_EQ(screen.notes, 0);
}

TEST_F(HelpButtonTest, playAloneStillPlays) {
    send(cc(MCM::PLAY_BUTTON, 127));
    send(cc(MCM::PLAY_BUTTON, 0));
    EXPECT_EQ(screen.plays, 1);
    EXPECT_EQ(host.opened, 0);
}

} // namespace AppServicesTests
