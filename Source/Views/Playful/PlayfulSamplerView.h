#pragma once
#include "PlayfulParamScreen.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <internal_plugins/internal_plugins.h>
#include <tracktion_engine/tracktion_engine.h>

/**
 * Playful screen for the synth sampler and the drum sampler. Same controls as
 * the classic sampler screen:
 *   encoder 1: turn to pick a sample/kit while browsing, press to browse or
 *              to load (or open a folder)
 *   encoder 2 / 3: start / end of the sample
 *   Control + encoder 4: gain
 *   keys (drum sampler): pick the pad to edit
 */
class PlayfulSamplerView : public PlayfulScreen,
                           public app_view_models::SamplerViewModel::Listener,
                           public app_services::MidiCommandManager::Listener {
  public:
    PlayfulSamplerView(tracktion::SamplerPlugin *sampler,
                       app_services::MidiCommandManager &mcm);
    PlayfulSamplerView(internal_plugins::DrumSamplerPlugin *drumSampler,
                       app_services::MidiCommandManager &mcm);
    ~PlayfulSamplerView() override;

    void sampleChanged() override { repaint(); }
    void sampleExcerptTimesChanged() override { repaint(); }
    void fullSampleThumbnailChanged() override { repaint(); }
    void sampleExcerptThumbnailChanged() override { repaint(); }
    void gainChanged() override { repaint(); }

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder1ButtonReleased() override;
    void encoder2Increased() override;
    void encoder2Decreased() override;
    void encoder3Increased() override;
    void encoder3Decreased() override;
    void encoder4Increased() override;
    void encoder4Decreased() override;
    void controlButtonPressed() override { repaint(); }
    void controlButtonReleased() override { repaint(); }
    void noteOnPressed(int noteNumber) override;

  protected:
    juce::String getTitle() override;
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return !isDrum; }

  private:
    tracktion::SamplerPlugin *samplerPlugin;
    app_services::MidiCommandManager &midiCommandManager;
    std::unique_ptr<app_view_models::SamplerViewModel> viewModel;
    bool isDrum;
    bool browsing = false;

    bool isActive();
    double getLength();
    // peak level of each of numBars slices of the sample (0-1)
    juce::Array<float> getBars(int numBars);
    juce::String getGainText();

    void paintList(juce::Graphics &g, float x, float y, float width, int rows);
    void paintSynth(juce::Graphics &g, double time);
    void paintDrum(juce::Graphics &g, double time);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayfulSamplerView)
};
