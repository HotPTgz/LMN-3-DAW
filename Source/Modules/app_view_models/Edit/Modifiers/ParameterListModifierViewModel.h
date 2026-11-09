#pragma once

namespace app_view_models {

/**
 * View model for a modifier whose controls are a list of its automatable
 * parameters (Random, Envelope Follower, MIDI Tracker, and the knob page of
 * the Step modifier). Ranges and value labels come from the parameters
 * themselves, so "1/16" is shown instead of a raw number.
 */
class ParameterListModifierViewModel : public ModifierViewModel {
  public:
    struct ParameterSpec {
        juce::String name;
        tracktion::AutomatableParameter::Ptr parameter;
        double interval; // step per encoder detent
    };

    ParameterListModifierViewModel(tracktion::Modifier *mod,
                                   juce::Array<ParameterSpec> specs);

    int getNumberOfParameters() override;
    juce::String getParameterName(int index) override;
    double getParameterValue(int index) override;
    void setParameterValue(int index, double value) override;
    juce::Range<double> getParameterRange(int index) override;
    double getParameterInterval(int index) override;
    juce::String getParameterValueText(int index, double value) override;

    // Builds a spec, using 1 for discrete parameters and 1/100 of the range
    // otherwise unless an interval is given
    static ParameterSpec spec(const juce::String &name,
                              tracktion::AutomatableParameter::Ptr parameter,
                              double interval = 0.0);

  private:
    juce::Array<ParameterSpec> specs;
};

// Creates the right view model for any modifier type
std::unique_ptr<ModifierViewModel>
createModifierViewModel(tracktion::Modifier *modifier);

// Sensible starting values for a newly added modifier (e.g. tempo sync)
void applyModifierDefaults(tracktion::Modifier *modifier);

} // namespace app_view_models
