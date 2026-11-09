#include "ParameterListModifierViewModel.h"

namespace app_view_models {

ParameterListModifierViewModel::ParameterListModifierViewModel(
    tracktion::Modifier *mod, juce::Array<ParameterSpec> s)
    : ModifierViewModel(mod), specs(std::move(s)) {}

int ParameterListModifierViewModel::getNumberOfParameters() {
    // the knob screens show at most 8 parameters (2 pages of 4)
    return juce::jmin(8, specs.size());
}

juce::String ParameterListModifierViewModel::getParameterName(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].name;
    return ModifierViewModel::getParameterName(index);
}

double ParameterListModifierViewModel::getParameterValue(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].parameter->getCurrentValue();
    return 0.0;
}

void ParameterListModifierViewModel::setParameterValue(int index,
                                                       double value) {
    if (!juce::isPositiveAndBelow(index, specs.size()))
        return;

    auto range = getParameterRange(index);
    specs[index].parameter->setParameter(float(range.clipValue(value)),
                                         juce::sendNotification);
}

juce::Range<double>
ParameterListModifierViewModel::getParameterRange(int index) {
    if (!juce::isPositiveAndBelow(index, specs.size()))
        return {0.0, 1.0};

    auto range = specs[index].parameter->getValueRange();
    return {double(range.getStart()), double(range.getEnd())};
}

double ParameterListModifierViewModel::getParameterInterval(int index) {
    if (juce::isPositiveAndBelow(index, specs.size()))
        return specs[index].interval;
    return 0.01;
}

juce::String
ParameterListModifierViewModel::getParameterValueText(int index, double value) {
    if (juce::isPositiveAndBelow(index, specs.size())) {
        auto parameter = specs[index].parameter;
        // choice parameters (rate, sync type ...) have text labels
        if (parameter->hasLabels())
            return parameter->getLabelForValue(float(value));
        return parameter->valueToString(float(value));
    }
    return ModifierViewModel::getParameterValueText(index, value);
}

ParameterListModifierViewModel::ParameterSpec
ParameterListModifierViewModel::spec(
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

std::unique_ptr<ModifierViewModel>
createModifierViewModel(tracktion::Modifier *modifier) {
    using Spec = ParameterListModifierViewModel;

    if (auto lfo = dynamic_cast<tracktion::LFOModifier *>(modifier))
        return std::make_unique<LFOModifierViewModel>(lfo);

    if (auto step = dynamic_cast<tracktion::StepModifier *>(modifier))
        return std::make_unique<ParameterListModifierViewModel>(
            modifier, juce::Array<Spec::ParameterSpec>{
                          Spec::spec("Rate", step->rateTypeParam),
                          Spec::spec("Depth", step->depthParam, 0.02),
                          Spec::spec("Steps", step->numStepsParam, 1.0),
                          Spec::spec("Sync", step->syncTypeParam),
                          Spec::spec("Hz", step->rateParam, 0.1),
                      });

    if (auto random = dynamic_cast<tracktion::RandomModifier *>(modifier))
        return std::make_unique<ParameterListModifierViewModel>(
            modifier, juce::Array<Spec::ParameterSpec>{
                          Spec::spec("Rate", random->rateTypeParam),
                          Spec::spec("Depth", random->depthParam),
                          Spec::spec("Smooth", random->smoothParam),
                          Spec::spec("Type", random->typeParam),
                          Spec::spec("Step Depth", random->stepDepthParam),
                          Spec::spec("Shape", random->shapeParam),
                          Spec::spec("Sync", random->syncTypeParam),
                          Spec::spec("Polarity", random->bipolarParam),
                      });

    if (auto envelope =
            dynamic_cast<tracktion::EnvelopeFollowerModifier *>(modifier))
        return std::make_unique<ParameterListModifierViewModel>(
            modifier, juce::Array<Spec::ParameterSpec>{
                          Spec::spec("Depth", envelope->depthParam, 0.02),
                          Spec::spec("Attack", envelope->attackParam, 5.0),
                          Spec::spec("Release", envelope->releaseParam, 10.0),
                          Spec::spec("Gain", envelope->gainDbParam, 0.5),
                          Spec::spec("Offset", envelope->offsetParam),
                          Spec::spec("Hold", envelope->holdParam, 10.0),
                      });

    if (auto tracker = dynamic_cast<tracktion::MIDITrackerModifier *>(modifier))
        return std::make_unique<ParameterListModifierViewModel>(
            modifier,
            juce::Array<Spec::ParameterSpec>{
                Spec::spec("Source", tracker->typeParam),
                Spec::spec("Mode", tracker->modeParam),
                Spec::spec("Root", tracker->relativeRootParam, 1.0),
                Spec::spec("Spread", tracker->relativeSpreadParam, 1.0),
            });

    return std::make_unique<ModifierViewModel>(modifier);
}

void applyModifierDefaults(tracktion::Modifier *modifier) {
    using namespace tracktion::ModifierCommon;

    // Set through the parameters so their current values update straight
    // away (setting the CachedValues only updates them asynchronously)
    auto set = [](tracktion::AutomatableParameter::Ptr &p, float value) {
        p->setParameter(value, juce::sendNotification);
    };

    if (auto lfo = dynamic_cast<tracktion::LFOModifier *>(modifier)) {
        lfo->wave.setValue(1, nullptr);
    } else if (auto step = dynamic_cast<tracktion::StepModifier *>(modifier)) {
        set(step->syncTypeParam, float(transport));
        set(step->rateTypeParam, float(sixteenth));
        set(step->numStepsParam, 16.0f);
        set(step->depthParam, 0.5f);
        // start with a simple rising pattern so the effect is audible
        for (int i = 0; i < 16; i++)
            step->setStep(i, float(i % 4) / 3.0f);
    } else if (auto random =
                   dynamic_cast<tracktion::RandomModifier *>(modifier)) {
        set(random->syncTypeParam, float(transport));
        set(random->rateTypeParam, float(eighth));
        set(random->depthParam, 0.5f);
    } else if (auto envelope =
                   dynamic_cast<tracktion::EnvelopeFollowerModifier *>(
                       modifier)) {
        set(envelope->depthParam, 0.5f);
    } else if (auto tracker =
                   dynamic_cast<tracktion::MIDITrackerModifier *>(modifier)) {
        set(tracker->modeParam,
            float(tracktion::MIDITrackerModifier::relative));
    }
}

} // namespace app_view_models
