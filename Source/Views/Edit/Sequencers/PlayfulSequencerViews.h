#pragma once
#include "PlayfulParamScreen.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <tracktion_engine/tracktion_engine.h>

// Shared bits of the sequencer screens
class SequencerScreen : public PlayfulScreen,
                        public app_services::MidiCommandManager::Listener {
  public:
    explicit SequencerScreen(app_services::MidiCommandManager &mcm);
    ~SequencerScreen() override;

    void controlButtonPressed() override { repaint(); }
    void controlButtonReleased() override { repaint(); }

  protected:
    app_services::MidiCommandManager &midiCommandManager;
    bool hasFocus();
    bool control() const { return midiCommandManager.isControlDown; }
};

//==============================================================================
// Drums: a row per kit sound, up to 64 steps
class DrumSequencerView : public SequencerScreen {
  public:
    DrumSequencerView(tracktion::AudioTrack::Ptr t,
                      app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { turn(0, 1); }
    void encoder1Decreased() override { turn(0, -1); }
    void encoder2Increased() override { turn(1, 1); }
    void encoder2Decreased() override { turn(1, -1); }
    void encoder3Increased() override { turn(2, 1); }
    void encoder3Decreased() override { turn(2, -1); }
    void encoder4Increased() override { turn(3, 1); }
    void encoder4Decreased() override { turn(3, -1); }
    void encoder4ButtonReleased() override;
    void noteOnPressed(int noteNumber) override;
    void minusButtonReleased() override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::DrumSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override;
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::DrumSequencerViewModel vm;
    void turn(int encoder, int direction);
    void paintGrid(juce::Graphics &g, double time);
    void paintGroove(juce::Graphics &g, double time);
};

//==============================================================================
// Endless: play notes in, it loops them
class EndlessSequencerView : public SequencerScreen {
  public:
    EndlessSequencerView(tracktion::AudioTrack::Ptr t,
                         app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { vm.nudgeRate(1); }
    void encoder1Decreased() override { vm.nudgeRate(-1); }
    void encoder2Increased() override { vm.nudgeGate(1); }
    void encoder2Decreased() override { vm.nudgeGate(-1); }
    void encoder3Increased() override { vm.nudgeSwing(1); }
    void encoder3Decreased() override { vm.nudgeSwing(-1); }
    void encoder4Increased() override { vm.nudgeTranspose(1); }
    void encoder4Decreased() override { vm.nudgeTranspose(-1); }
    void encoder4ButtonReleased() override { vm.resetTranspose(); }
    void noteOnPressed(int noteNumber) override;
    void plusButtonReleased() override;
    void minusButtonReleased() override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::EndlessSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Endless"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::EndlessSequencerViewModel vm;
};

//==============================================================================
// Tombola: balls bouncing in a spinning shape
class TombolaSequencerView : public SequencerScreen {
  public:
    TombolaSequencerView(tracktion::AudioTrack::Ptr t,
                         app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { vm.nudgeSpin(1); }
    void encoder1Decreased() override { vm.nudgeSpin(-1); }
    void encoder2Increased() override { vm.nudgeGravity(1); }
    void encoder2Decreased() override { vm.nudgeGravity(-1); }
    void encoder3Increased() override { vm.nudgeSides(1); }
    void encoder3Decreased() override { vm.nudgeSides(-1); }
    void encoder4Increased() override { vm.nudgeBounce(1); }
    void encoder4Decreased() override { vm.nudgeBounce(-1); }
    void encoder4ButtonReleased() override { vm.shake(); }
    void noteOnPressed(int noteNumber) override;
    void minusButtonReleased() override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::TombolaSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Tombola"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::TombolaSequencerViewModel vm;
};

//==============================================================================
// Pattern: four patterns A to D in a chain
class PatternSequencerView : public SequencerScreen {
  public:
    PatternSequencerView(tracktion::AudioTrack::Ptr t,
                         app_services::MidiCommandManager &mcm);

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder2Increased() override { vm.moveStep(1); }
    void encoder2Decreased() override { vm.moveStep(-1); }
    void encoder3Increased() override;
    void encoder3Decreased() override;
    void encoder3ButtonReleased() override;
    void encoder4Increased() override { vm.nudgeViewOctave(1); }
    void encoder4Decreased() override { vm.nudgeViewOctave(-1); }
    void encoder4ButtonReleased() override { vm.copyToNext(); }
    void noteOnPressed(int noteNumber) override;
    void minusButtonReleased() override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::PatternSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Pattern"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::PatternSequencerViewModel vm;
};
