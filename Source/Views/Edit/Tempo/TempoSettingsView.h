#pragma once
#include "PlayfulParamScreen.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

// Tempo, quantize grid, swing and click volume, with a swinging metronome
class TempoSettingsView
    : public PlayfulScreen,
      public app_services::MidiCommandManager::Listener,
      public app_view_models::TempoSettingsViewModel::Listener {
  public:
    TempoSettingsView(tracktion::Edit &e,
                      app_services::MidiCommandManager &mcm);
    ~TempoSettingsView() override;

    void encoder1Increased() override;
    void encoder1Decreased() override;

    // Quantize grid / swing used by Control + Cut on the tracks screen
    void encoder2Increased() override;
    void encoder2Decreased() override;
    void encoder3Increased() override;
    void encoder3Decreased() override;

    void encoder4Increased() override;
    void encoder4Decreased() override;

    void tempoSettingsButtonPressed() override;

    void bpmChanged(const double newBpm, const double newBps) override;
    void clickTrackGainChanged(const double newGain) override;
    void tapModeChanged(const bool newTapMode) override;
    void quantizeSettingsChanged(const juce::String &gridName,
                                 const int swingPercent) override;

  protected:
    juce::String getTitle() override { return "Tempo"; }
    juce::String getSubtitle() override { return "4/4"; }
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool wantsSmoothFrames() override;

  private:
    tracktion::Edit &edit;
    app_view_models::TempoSettingsViewModel viewModel;
    app_services::MidiCommandManager &midiCommandManager;

    double bpm = 120.0;
    double clickGain = 0.6;
    bool tapMode = false;
    juce::String gridName = "1/16";
    int swing = 0;

    bool hasFocus();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TempoSettingsView)
};
