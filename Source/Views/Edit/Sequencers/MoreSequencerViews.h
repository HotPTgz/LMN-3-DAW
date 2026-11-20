#pragma once
#include "PlayfulSequencerViews.h"

//==============================================================================
// Euclidean: rings of evenly spread hits, one per kit sound
class EuclideanSequencerView : public SequencerScreen {
  public:
    EuclideanSequencerView(tracktion::AudioTrack::Ptr t,
                           app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { turn(0, 1); }
    void encoder1Decreased() override { turn(0, -1); }
    void encoder2Increased() override { turn(1, 1); }
    void encoder2Decreased() override { turn(1, -1); }
    void encoder3Increased() override { turn(2, 1); }
    void encoder3Decreased() override { turn(2, -1); }
    void encoder4Increased() override { turn(3, 1); }
    void encoder4Decreased() override { turn(3, -1); }
    void encoder1ButtonReleased() override;
    void noteOnPressed(int noteNumber) override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::EuclideanSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Euclidean"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::EuclideanSequencerViewModel vm;
    void turn(int encoder, int direction);
};

//==============================================================================
// Turing: a looping random melody that slowly changes
class TuringSequencerView : public SequencerScreen {
  public:
    TuringSequencerView(tracktion::AudioTrack::Ptr t,
                        app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { turn(0, 1); }
    void encoder1Decreased() override { turn(0, -1); }
    void encoder2Increased() override { turn(1, 1); }
    void encoder2Decreased() override { turn(1, -1); }
    void encoder3Increased() override { turn(2, 1); }
    void encoder3Decreased() override { turn(2, -1); }
    void encoder4Increased() override { turn(3, 1); }
    void encoder4Decreased() override { turn(3, -1); }
    void encoder1ButtonReleased() override;
    void noteOnPressed(int noteNumber) override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::TuringSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Turing"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::TuringSequencerViewModel vm;
    void turn(int encoder, int direction);
};

//==============================================================================
// Chords: a chord per bar, other tracks follow
class ChordsSequencerView : public SequencerScreen {
  public:
    ChordsSequencerView(tracktion::AudioTrack::Ptr t,
                        app_services::MidiCommandManager &mcm);

    void encoder1Increased() override { turn(0, 1); }
    void encoder1Decreased() override { turn(0, -1); }
    void encoder2Increased() override { turn(1, 1); }
    void encoder2Decreased() override { turn(1, -1); }
    void encoder3Increased() override { turn(2, 1); }
    void encoder3Decreased() override { turn(2, -1); }
    void encoder4Increased() override { turn(3, 1); }
    void encoder4Decreased() override { turn(3, -1); }
    void noteOnPressed(int noteNumber) override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    app_view_models::ChordsSequencerViewModel &getViewModel() { return vm; }

  protected:
    juce::String getTitle() override { return "Chords"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override { return vm.isPlaying(); }
    juce::String getChangeSignature() override;

  private:
    app_view_models::ChordsSequencerViewModel vm;
    void turn(int encoder, int direction);
};
