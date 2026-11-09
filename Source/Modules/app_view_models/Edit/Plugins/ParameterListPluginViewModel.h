#pragma once

namespace app_view_models {

/**
 * Knob-screen view model for a plugin whose controls are a list of its
 * automatable parameters. Ranges and value labels come from the parameters,
 * so new plugins don't need a hand-written view model.
 */
class ParameterListPluginViewModel : public InternalPluginViewModel {
  public:
    struct ParameterSpec {
        juce::String name;
        tracktion::AutomatableParameter::Ptr parameter;
        double interval; // step per encoder detent
    };

    ParameterListPluginViewModel(tracktion::Plugin *p,
                                 juce::Array<ParameterSpec> specs);

    // All of the plugin's automatable parameters, in order
    explicit ParameterListPluginViewModel(tracktion::Plugin *p);

    int getNumberOfParameters() override;
    juce::String getParameterName(int index) override;
    double getParameterValue(int index) override;
    void setParameterValue(int index, double value) override;
    juce::Range<double> getParameterRange(int index) override;
    double getParameterInterval(int index) override;
    juce::String getParameterValueText(int index, double value) override;

    static ParameterSpec spec(const juce::String &name,
                              tracktion::AutomatableParameter::Ptr parameter,
                              double interval = 0.0);

  private:
    juce::Array<ParameterSpec> specs;
};

} // namespace app_view_models
