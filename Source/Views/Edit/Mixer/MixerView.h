#pragma once
#include "PlayfulParamScreen.h"
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <tracktion_engine/tracktion_engine.h>

/**
 * The mixer, playful edition: each track is a jar of jelly. The jelly level
 * is the volume, the little face underneath leans with the pan, the dots in
 * the jar are the A / B sends and the thin bars on the side are the live
 * level.
 */
class MixerView : public PlayfulScreen,
                  public app_services::MidiCommandManager::Listener,
                  public app_view_models::EditItemListViewModel::Listener,
                  public app_view_models::ItemListState::Listener {
  public:
    MixerView(tracktion::Edit &e, app_services::MidiCommandManager &mcm);
    ~MixerView() override;
    bool hasFlexibleStage() override { return true; }

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder1ButtonReleased() override;

    // aux send level for the selected track: FX A, or FX B with Control
    void encoder2Increased() override;
    void encoder2Decreased() override;

    void encoder3Increased() override;
    void encoder3Decreased() override;

    void encoder4Increased() override;
    void encoder4Decreased() override;

    void encoder3ButtonReleased() override;
    void encoder4ButtonReleased() override;

    void controlButtonPressed() override { repaint(); }
    void controlButtonReleased() override { repaint(); }

    // ItemListState listener methods
    void selectedIndexChanged(int newIndex) override;

    // EditItemListViewModel listener methods
    void itemsChanged() override;

  protected:
    juce::String getTitle() override { return "Mixer"; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override;
    juce::String getChangeSignature() override;

  private:
    tracktion::Edit &edit;
    app_services::MidiCommandManager &midiCommandManager;
    app_view_models::MixerViewModel viewModel;

    // live levels for one track (keeps the measuring plugin alive)
    struct Meter {
        tracktion::Track *track = nullptr;
        tracktion::Plugin::Ptr plugin;
        tracktion::LevelMeasurer *measurer = nullptr;
        tracktion::LevelMeasurer::Client client;
        float levelDb[2] = {-100.0f, -100.0f};
        ~Meter();
    };
    std::vector<std::unique_ptr<Meter>> meters;
    void buildMeters();
    Meter *meterFor(tracktion::Track *track);

    int getNumTracks();
    tracktion::Track *getTrack(int index);
    tracktion::Track *getSelectedTrack();
    bool hasFocus();

    void paintJar(juce::Graphics &g, tracktion::Track &track, float x,
                  bool selected, double time);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerView)
};
