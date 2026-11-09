#include "ParameterListPluginViewModel.h"

namespace app_view_models {

ParameterListPluginViewModel::ParameterListPluginViewModel(
    tracktion::Plugin *p, juce::Array<ParameterSpec> s)
    : InternalPluginViewModel(p), specs(std::move(s)) {
    // Parameter values live in CachedValues on the plugin state, so the base
    // class listener already reports changes.
}

ParameterListPluginViewModel::ParameterListPluginViewModel(tracktion::Plugin *p)
    : InternalPluginViewModel(p) {
    for (auto parameter : p->getAutomatableParameters())
        specs.add(spec(parameter->getParameterName(), parameter));
}

int ParameterListPluginViewModel::getNumberOfParameters() {
    return specs.size();
}

juce::String ParameterListPluginViewModel::getParameterName(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].name;
    return InternalPluginViewModel::getParameterName(index);
}

double ParameterListPluginViewModel::getParameterValue(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].parameter->getCurrentValue();
    return 0.0;
}

void ParameterListPluginViewModel::setParameterValue(int index, double value) {
    if (!juce::isPositiveAndBelow(index, specs.size()))
        return;
    auto range = getParameterRange(index);
    specs[index].parameter->setParameter(float(range.clipValue(value)),
                                         juce::sendNotification);
}

juce::Range<double> ParameterListPluginViewModel::getParameterRange(int index) {
    if (!juce::isPositiveAndBelow(index, specs.size()))
        return {0.0, 1.0};
    auto range = specs[index].parameter->getValueRange();
    return {double(range.getStart()), double(range.getEnd())};
}

double ParameterListPluginViewModel::getParameterInterval(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].interval;
    return 0.01;
}

juce::String ParameterListPluginViewModel::getParameterValueText(int index,
                                                                 double value) {
    if (juce::isPositiveAndBelow(index, specs.size())) {
        auto parameter = specs[index].parameter;
        if (parameter->hasLabels())
            return parameter->getLabelForValue(float(value));
        return parameter->valueToString(float(value));
    }
    return InternalPluginViewModel::getParameterValueText(index, value);
}

ParameterListPluginViewModel::ParameterSpec ParameterListPluginViewModel::spec(
    const juce::String &name, tracktion::AutomatableParameter::Ptr parameter,
    double interval) {
    jassert(parameter != nullptr);
    if (interval <= 0.0) {
        if (parameter->isDiscrete())
            interval = 1.0;
        else
            interval = parameter->getValueRange().getLength() / 100.0;
    }
    return {name, parameter, interval};
}

} // namespace app_view_models
