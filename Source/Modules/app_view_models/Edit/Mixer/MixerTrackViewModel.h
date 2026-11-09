#pragma once

namespace app_view_models {

namespace IDs {

const juce::Identifier MIXER_TRACK_VIEW_STATE("MIXER_TRACK_VIEW_STATE");

}

class MixerTrackViewModel : public juce::ValueTree::Listener,
                            public FlaggedAsyncUpdater {
  public:
    MixerTrackViewModel(tracktion::Track::Ptr t);
    ~MixerTrackViewModel() override;

    tracktion::VolumeAndPanPlugin *getVolumeAndPanPlugin();

    class Listener {
      public:
        virtual ~Listener() = default;

        virtual void panChanged(double pan) {}
        virtual void volumeChanged(double volume) {}
        virtual void soloStateChanged(bool solo) {}
        virtual void muteStateChanged(bool mute) {}
        // 0-1 send levels for FX A and FX B
        virtual void sendLevelsChanged(double sendA, double sendB) {}
    };

    void addListener(Listener *l);
    void removeListener(Listener *l);

  private:
    tracktion::Track::Ptr track;
    juce::ValueTree state;
    juce::ListenerList<Listener> listeners;

    // Async updater flags
    bool shouldUpdateVolume = false;
    bool shouldUpdatePan = false;
    bool shouldUpdateMute = false;
    bool shouldUpdateSolo = false;
    bool shouldUpdateSends = false;
    void callSendLevelsChanged(Listener &l);

    void handleAsyncUpdate() override;

    void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                  const juce::Identifier &property) override;
    // sends being added or removed
    void valueTreeChildAdded(juce::ValueTree &parent,
                             juce::ValueTree &child) override;
    void valueTreeChildRemoved(juce::ValueTree &parent, juce::ValueTree &child,
                               int index) override;
};

} // namespace app_view_models
