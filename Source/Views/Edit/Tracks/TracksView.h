#pragma once
#include "PlayfulParamScreen.h"
#include <app_navigation/app_navigation.h>
#include <app_services/app_services.h>
#include <app_view_models/app_view_models.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <tracktion_engine/tracktion_engine.h>

/**
 * The tracks screen, playful edition: candy lanes with jelly clips, a chick
 * for the playhead and flags for the loop. Press encoder 1 to zoom into the
 * selected track. Everything is painted here; the controls are the same as
 * before.
 */
class TracksView : public PlayfulScreen,
                   public app_services::MidiCommandManager::Listener,
                   public app_view_models::TracksListViewModel::Listener,
                   public app_view_models::EditItemListViewModel::Listener,
                   public app_view_models::ItemListState::Listener {
  public:
    TracksView(tracktion::Edit &e, app_services::MidiCommandManager &mcm);
    ~TracksView() override;

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder1ButtonReleased() override;

    void encoder2Increased() override;
    void encoder2Decreased() override;

    void encoder3Increased() override;
    void encoder3Decreased() override;
    void encoder3ButtonReleased() override;
    void encoder4ButtonReleased() override;

    void encoder4Increased() override;
    void encoder4Decreased() override;

    void cutButtonReleased() override;
    void pasteButtonReleased() override;

    void sliceButtonReleased() override;

    void plusButtonReleased() override;
    void minusButtonReleased() override;

    void loopInButtonReleased() override;
    void loopOutButtonReleased() override;
    void loopButtonReleased() override;

    void undoButtonReleased() override;

    void recordButtonReleased() override;
    void playButtonReleased() override;
    void stopButtonReleased() override;

    void controlButtonPressed() override { repaint(); }
    void controlButtonReleased() override { repaint(); }

    // ItemListState listener methods
    void selectedIndexChanged(int newIndex) override;

    // EditItemListViewModel listener methods
    void itemsChanged() override;

    // TracksListViewModel listener methods
    void isRecordingChanged(bool isRecording) override;
    void isPlayingChanged(bool isPlaying) override;
    void tracksViewTypeChanged(
        app_view_models::TracksListViewModel::TracksViewType type) override;
    void loopingChanged(bool looping) override;
    void soloStateChanged(bool solo) override;
    void muteStateChanged(bool mute) override;

    app_view_models::TracksListViewModel &getViewModel() { return viewModel; };

  protected:
    juce::String getTitle() override;
    juce::String getSubtitle() override;
    bool hasFlexibleStage() override { return true; }
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override { return false; }
    bool wantsSmoothFrames() override;
    juce::String getChangeSignature() override;

  private:
    tracktion::Edit &edit;
    app_services::MidiCommandManager &midiCommandManager;
    app_services::TimelineCamera camera;
    app_view_models::TracksListViewModel viewModel;

    bool recording = false;
    bool looping = false;
    bool shouldUpdateTrackColour = false;

    bool isZoomed();
    bool hasFocus();
    tracktion::AudioTrack *getTrack(int index);
    tracktion::AudioTrack *getSelectedTrack();
    int getNumTracks();
    juce::String barBeat(tracktion::TimePosition t);

    // timeline drawing
    struct Timeline {
        float x, width;
    };
    float timeToX(const Timeline &tl, double seconds);
    void paintBeats(juce::Graphics &g, const Timeline &tl, float top,
                    float bottom, bool numbers);
    void paintClips(juce::Graphics &g, const Timeline &tl,
                    tracktion::AudioTrack &track, float top, float height,
                    bool big, double time);
    void paintLoopAndPlayhead(juce::Graphics &g, const Timeline &tl,
                              float flagY, float top, float bottom,
                              double time);
    void paintLanes(juce::Graphics &g, double time);
    void paintZoomed(juce::Graphics &g, double time);
    bool isRecordingInto(tracktion::AudioTrack &track);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracksView)
};
