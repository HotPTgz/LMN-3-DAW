#pragma once
#include "AppLookAndFeel.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Editor for the Step modulator.
//   encoder 1: select step
//   encoder 2: step value (hold Control for fine steps)
//   encoder 3: rate (1/4, 1/8, 1/16 ...)   Control + encoder 3: number of steps
//   encoder 4: depth                        Control + encoder 4: sync type
class StepModifierView : public juce::Component,
                         public app_services::MidiCommandManager::Listener,
                         public app_view_models::ModifierViewModel::Listener,
                         private juce::Timer {
  public:
    StepModifierView(tracktion::StepModifier *mod,
                     app_services::MidiCommandManager &mcm);
    ~StepModifierView() override;

    void paint(juce::Graphics &g) override;
    void resized() override;

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder2Increased() override;
    void encoder2Decreased() override;
    void encoder3Increased() override;
    void encoder3Decreased() override;
    void encoder4Increased() override;
    void encoder4Decreased() override;

    void parametersChanged() override;

  private:
    tracktion::StepModifier::Ptr stepModifier;
    std::unique_ptr<app_view_models::ModifierViewModel> viewModel;
    app_services::MidiCommandManager &midiCommandManager;
    AppLookAndFeel appLookAndFeel;

    int selectedStep = 0;
    int playingStep = -1;

    // indices into the view model's parameters
    static constexpr int rateIndex = 0;
    static constexpr int depthIndex = 1;
    static constexpr int stepsIndex = 2;
    static constexpr int syncIndex = 3;

    bool hasFocus();
    int getNumSteps();
    void nudgeStep(float delta);
    void nudgeParameter(int index, int direction);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepModifierView)
};
