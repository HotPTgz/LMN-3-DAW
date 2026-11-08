#include "TempoSettingsViewModel.h"

namespace app_view_models {

TempoSettingsViewModel::TempoSettingsViewModel(tracktion::Edit &e)
    : edit(e), tempoSequenceState(
                   edit.tempoSequence
                       .getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
                       .state),
      clickTrackState(edit.state.getChildWithName(tracktion::IDs::CLICKTRACK)),
      quantizeState(edit.state.getOrCreateChildWithName(IDs::QUANTIZE_SETTINGS,
                                                        nullptr)) {
    quantizeGridIndex.referTo(quantizeState, IDs::quantizeGridIndex, nullptr,
                              Quantizer::getDefaultGridIndex());
    quantizeSwing.referTo(quantizeState, IDs::quantizeSwing, nullptr, 0);
    tempoSequenceState.addListener(this);
    clickTrackState.addListener(this);
    quantizeState.addListener(this);
}

TempoSettingsViewModel::~TempoSettingsViewModel() {
    tempoSequenceState.removeListener(this);
    clickTrackState.removeListener(this);
    quantizeState.removeListener(this);

    stopTimer();
}

void TempoSettingsViewModel::setClickTrackGain(const double gain) const {
    if (gain <= clickTrackGainUpperLimit && gain >= clickTrackGainLowerLimit)
        edit.clickTrackGain.setValue(gain, nullptr);
}

void TempoSettingsViewModel::setBpm(const double bpm) const {
    if (bpm <= bpmUpperLimit && bpm >= bpmLowerLimit)
        edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.setValue(bpm, nullptr);
}

void TempoSettingsViewModel::incrementBpm() const {
    if (edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.get() < bpmUpperLimit) {
        double newBpm =
            edit.tempoSequence
                .getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
                .bpm.get() +
            1;
        edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.setValue(newBpm, nullptr);
    }
}

void TempoSettingsViewModel::decrementBpm() const {
    if (edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.get() > bpmLowerLimit) {
        double newBpm =
            edit.tempoSequence
                .getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
                .bpm.get() -
            1;
        edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.setValue(newBpm, nullptr);
    }
}

void TempoSettingsViewModel::incrementClickTrackGain() {
    if (edit.clickTrackGain.get() <= clickTrackGainLowerLimit + .01) {
        edit.clickTrackEnabled.setValue(true, nullptr);
    }

    if (double(edit.clickTrackGain.get()) <
        clickTrackGainUpperLimit - clickTrackGainInterval) {
        double newGain = edit.clickTrackGain.get() + clickTrackGainInterval;
        edit.clickTrackGain.setValue(newGain, nullptr);

    } else {
        edit.clickTrackGain.setValue(clickTrackGainUpperLimit, nullptr);
    }
}

void TempoSettingsViewModel::decrementClickTrackGain() {
    if (edit.clickTrackGain.get() >=
        clickTrackGainLowerLimit + clickTrackGainInterval + .01) {
        double newGain = edit.clickTrackGain.get() - clickTrackGainInterval;
        edit.clickTrackGain.setValue(newGain, nullptr);

    } else {
        edit.clickTrackEnabled.setValue(false, nullptr);
        edit.clickTrackGain.setValue(clickTrackGainLowerLimit, nullptr);
    }
}

void TempoSettingsViewModel::handleAsyncUpdate() {
    if (shouldUpdateBPM)
        listeners.call([this](Listener &l) {
            l.bpmChanged(
                edit.tempoSequence
                    .getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
                    .getBpm(),
                edit.tempoSequence.getBeatsPerSecondAt(
                    tracktion::TimePosition::fromSeconds(0.0)));
        });

    if (shouldUpdateClickTrackGain)
        listeners.call([this](Listener &l) {
            l.clickTrackGainChanged(edit.clickTrackGain.get());
        });

    if (shouldUpdateTapMode)
        listeners.call(
            [this](Listener &l) { l.tapModeChanged(tapModeEnabled); });

    if (compareAndReset(shouldUpdateQuantizeSettings))
        listeners.call([this](Listener &l) {
            l.quantizeSettingsChanged(getQuantizeGridName(),
                                      getSwingPercent());
        });
}

void TempoSettingsViewModel::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
    if (treeWhosePropertyHasChanged == tempoSequenceState)
        if (property == tracktion::IDs::bpm)
            markAndUpdate(shouldUpdateBPM);

    if (treeWhosePropertyHasChanged == clickTrackState)
        if (property == tracktion::IDs::level)
            markAndUpdate(shouldUpdateClickTrackGain);

    if (treeWhosePropertyHasChanged == quantizeState)
        markAndUpdate(shouldUpdateQuantizeSettings);
}

void TempoSettingsViewModel::incrementQuantizeGrid() {
    int index = juce::jlimit(0, Quantizer::getGrids().size() - 1,
                             quantizeGridIndex.get() + 1);
    quantizeGridIndex.setValue(index, nullptr);
}

void TempoSettingsViewModel::decrementQuantizeGrid() {
    int index = juce::jlimit(0, Quantizer::getGrids().size() - 1,
                             quantizeGridIndex.get() - 1);
    quantizeGridIndex.setValue(index, nullptr);
}

void TempoSettingsViewModel::incrementSwing() {
    quantizeSwing.setValue(juce::jlimit(0, Quantizer::maxSwingPercent,
                                        quantizeSwing.get() +
                                            Quantizer::swingStepPercent),
                           nullptr);
}

void TempoSettingsViewModel::decrementSwing() {
    quantizeSwing.setValue(juce::jlimit(0, Quantizer::maxSwingPercent,
                                        quantizeSwing.get() -
                                            Quantizer::swingStepPercent),
                           nullptr);
}

juce::String TempoSettingsViewModel::getQuantizeGridName() const {
    const auto &grids = Quantizer::getGrids();
    return grids[juce::jlimit(0, grids.size() - 1, quantizeGridIndex.get())]
        .name;
}

int TempoSettingsViewModel::getSwingPercent() const {
    return juce::jlimit(0, Quantizer::maxSwingPercent, quantizeSwing.get());
}

void TempoSettingsViewModel::addListener(TempoSettingsViewModel::Listener *l) {
    listeners.add(l);
    l->bpmChanged(
        edit.tempoSequence.getBpmAt(tracktion::TimePosition::fromSeconds(0.0)),
        edit.tempoSequence.getBeatsPerSecondAt(
            tracktion::TimePosition::fromSeconds(0.0)));
    l->clickTrackGainChanged(edit.clickTrackGain.get());
    l->quantizeSettingsChanged(getQuantizeGridName(), getSwingPercent());
}

void TempoSettingsViewModel::removeListener(
    TempoSettingsViewModel::Listener *l) {
    listeners.remove(l);
}

void TempoSettingsViewModel::timerCallback() {
    stopTimer();
    beatMeasurements.clear();
    tapModeEnabled = false;
    markAndUpdate(shouldUpdateTapMode);
}

void TempoSettingsViewModel::enableTapMode() {
    if (!tapModeEnabled) {
        tapModeEnabled = true;
        markAndUpdate(shouldUpdateTapMode);
        beatMeasurements.clear();
    }

    startTimer(timeout);
    if (beatMeasurements.size() > 5)
        beatMeasurements.clear();

    beatMeasurements.add(juce::Time::getCurrentTime());

    if (beatMeasurements.size() > 1) {
        double total = 0;
        for (int i = beatMeasurements.size() - 1; i > 0; i--)
            total +=
                (beatMeasurements[i] - beatMeasurements[i - 1]).inSeconds();

        double averageSecondsPerBeat = total / (beatMeasurements.size() - 1);
        double averageBPS = 1.0 / averageSecondsPerBeat;
        int averageBPM = averageBPS * 60;
        int newBpm = juce::jlimit(bpmLowerLimit, bpmUpperLimit, averageBPM);
        edit.tempoSequence.getTempoAt(tracktion::TimePosition::fromSeconds(0.0))
            .bpm.setValue(newBpm, nullptr);
    }
}

} // namespace app_view_models