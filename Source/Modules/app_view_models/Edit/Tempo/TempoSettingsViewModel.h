#pragma once

namespace app_view_models {

class TempoSettingsViewModel : public juce::ValueTree::Listener,
                               public FlaggedAsyncUpdater,
                               public juce::Timer {
  public:
    explicit TempoSettingsViewModel(tracktion::Edit &e);
    ~TempoSettingsViewModel() override;

    void setBpm(double bpm) const;

    void setClickTrackGain(double gain) const;

    void incrementBpm() const;
    void decrementBpm() const;

    void incrementClickTrackGain();
    void decrementClickTrackGain();

    void enableTapMode();

    // Quantize settings (stored per project)
    void incrementQuantizeGrid();
    void decrementQuantizeGrid();
    void incrementSwing();
    void decrementSwing();
    juce::String getQuantizeGridName() const;
    int getSwingPercent() const;

    class Listener {
      public:
        virtual ~Listener() = default;

        virtual void bpmChanged(const double newBpm, const double newBps) {}
        virtual void clickTrackGainChanged(const double newGain) {}
        virtual void tapModeChanged(const bool newTapMode) {}
        virtual void quantizeSettingsChanged(const juce::String &gridName,
                                             const int swingPercent) {}
    };

    void addListener(Listener *l);
    void removeListener(Listener *l);

    void timerCallback() override;

    int bpmUpperLimit = 220;
    int bpmLowerLimit = 20;

    double clickTrackGainUpperLimit = 1.0;
    double clickTrackGainLowerLimit = 0.2;
    double clickTrackGainInterval = .02;

  private:
    tracktion::Edit &edit;
    juce::ValueTree tempoSequenceState;
    juce::ValueTree clickTrackState;
    juce::ValueTree quantizeState;
    juce::CachedValue<int> quantizeGridIndex;
    juce::CachedValue<int> quantizeSwing;
    juce::ListenerList<Listener> listeners;

    juce::Array<juce::Time> beatMeasurements;
    bool tapModeEnabled = false;
    int timeout = 3000;

    // Async update markers
    bool shouldUpdateBPM = false;
    bool shouldUpdateClickTrackGain = false;
    bool shouldUpdateTapMode = false;
    bool shouldUpdateQuantizeSettings = false;

    void handleAsyncUpdate() override;
    void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                  const juce::Identifier &property) override;
};

} // namespace app_view_models
