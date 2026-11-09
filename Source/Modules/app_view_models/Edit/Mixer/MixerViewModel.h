#pragma once

namespace app_view_models {

namespace IDs {

const juce::Identifier MIXER_VIEW_STATE("MIXER_VIEW_STATE");

}

class MixerViewModel {
  public:
    explicit MixerViewModel(tracktion::Edit &e);

    void incrementPan();
    void decrementPan();

    void incrementVolume();
    void decrementVolume();

    // Aux send level for the selected track (bus 0 = FX A, 1 = FX B)
    void incrementSend(int bus);
    void decrementSend(int bus);

    void toggleSolo();
    void toggleMute();

  private:
    tracktion::Edit &edit;
    std::unique_ptr<MixerTracksListAdapter> adapter;
    juce::ValueTree state;

  public:
    // Must appear below the other variables since it needs to be initialized
    // last
    EditItemListViewModel listViewModel;
};

} // namespace app_view_models
