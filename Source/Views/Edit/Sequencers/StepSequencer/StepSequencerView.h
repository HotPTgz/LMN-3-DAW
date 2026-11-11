#pragma once
#include "PlayfulParamScreen.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

// The step sequencer: a grid of gumdrops (rows = notes, columns = steps) with
// a bee flying over the step that is playing
class StepSequencerView
    : public PlayfulScreen,
      public app_services::MidiCommandManager::Listener,
      public app_view_models::StepSequencerViewModel::Listener {
  public:
    StepSequencerView(tracktion::AudioTrack::Ptr p,
                      app_services::MidiCommandManager &mcm);
    ~StepSequencerView() override;

    void noteOnPressed(int noteNumber) override;

    void encoder1Increased() override;
    void encoder1Decreased() override;

    void encoder2Increased() override;
    void encoder2Decreased() override;

    void encoder3Increased() override;
    void encoder3Decreased() override;

    void playButtonReleased() override;
    void stopButtonReleased() override;

    void minusButtonReleased() override;

    void patternChanged() override { repaint(); }
    void selectedNoteIndexChanged(int) override { repaint(); }
    void numberOfNotesChanged(int) override { repaint(); }
    void notesPerMeasureChanged(int newNotesPerMeasure) override;

  protected:
    juce::String getTitle() override { return "Step Sequencer"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override;
    juce::String getChangeSignature() override;

  private:
    tracktion::AudioTrack::Ptr track;
    app_view_models::StepSequencerViewModel viewModel;
    app_services::MidiCommandManager &midiCommandManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerView)
};
