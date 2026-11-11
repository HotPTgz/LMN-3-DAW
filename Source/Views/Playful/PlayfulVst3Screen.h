#pragma once
#include "PlayfulParamScreen.h"
#include <app_view_models/app_view_models.h>

/**
 * The playful screen for VST3 plugins: a robot stands for the plugin, the
 * pages from its knob map run along the top as tabs, and the 8 values of the
 * page are shown as candy bars. Press encoder 1 to browse the plugin's
 * presets (turn 1: preset, turn 2: bank of 16, press 1: load, press 2 or
 * Stop: back).
 */
class PlayfulVst3Screen : public PlayfulParamScreen {
  public:
    PlayfulVst3Screen(
        app_services::MidiCommandManager &mcm,
        std::shared_ptr<app_view_models::ExternalPluginViewModel> viewModel);

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder2Increased() override;
    void encoder2Decreased() override;
    void encoder3Increased() override;
    void encoder3Decreased() override;
    void encoder4Increased() override;
    void encoder4Decreased() override;
    void encoder1ButtonReleased() override;
    void encoder2ButtonReleased() override;
    void stopButtonReleased() override;
    void plusButtonReleased() override;
    void minusButtonReleased() override;

    bool isBrowsing() const { return browsing; }
    void openBrowser();
    void closeBrowser();

  protected:
    juce::String getTitle() override;
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    juce::String getChangeSignature() override;

  private:
    app_services::MidiCommandManager &mcm;
    std::shared_ptr<app_view_models::ExternalPluginViewModel> viewModel;
    bool browsing = false;
    int browseIndex = 0;

    static constexpr int presetsPerBank = 16;
    void moveBrowse(int delta);
};

// The playful screen for a VST3 plugin, or nullptr if it isn't one
std::unique_ptr<juce::Component>
createPlayfulVst3View(tracktion::Plugin *plugin,
                      app_services::MidiCommandManager &mcm);
