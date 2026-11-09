#include "StepModifierView.h"

StepModifierView::StepModifierView(tracktion::StepModifier *mod,
                                   app_services::MidiCommandManager &mcm)
    : stepModifier(mod), viewModel(app_view_models::createModifierViewModel(mod)),
      midiCommandManager(mcm) {
    midiCommandManager.addListener(this);
    viewModel->addListener(this);
    // follow the playing step
    startTimerHz(20);
}

StepModifierView::~StepModifierView() {
    midiCommandManager.removeListener(this);
    viewModel->removeListener(this);
}

bool StepModifierView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

int StepModifierView::getNumSteps() {
    return juce::jlimit(1, int(tracktion::StepModifier::maxNumSteps),
                        juce::roundToInt(stepModifier->numSteps.get()));
}

void StepModifierView::timerCallback() {
    int current = stepModifier->getCurrentStep();
    if (current != playingStep) {
        playingStep = current;
        repaint();
    }
}

void StepModifierView::paint(juce::Graphics &g) {
    g.fillAll(appLookAndFeel.backgroundColour);

    auto area = getLocalBounds().reduced(getWidth() / 20, getHeight() / 20);
    auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(),
                           getHeight() * .07f, juce::Font::plain);
    g.setFont(font);

    // title
    g.setColour(appLookAndFeel.textColour);
    g.drawText("Step", area.removeFromTop(int(getHeight() * .1f)),
               juce::Justification::centred);

    // two lines of settings under the bars
    auto info = area.removeFromBottom(int(getHeight() * .2f));
    auto text = [this](int index) {
        return viewModel->getParameterValueText(
            index, viewModel->getParameterValue(index));
    };
    auto lineHeight = info.getHeight() / 2;
    g.drawText("Step " + juce::String(selectedStep + 1) + ": " +
                   juce::String(stepModifier->getStep(selectedStep), 2),
               info.removeFromTop(lineHeight), juce::Justification::centred);
    g.drawText("Rate " + text(rateIndex) + "  Depth " +
                   juce::String(viewModel->getParameterValue(depthIndex), 2) +
                   "  Sync " + text(syncIndex),
               info, juce::Justification::centred);
    area.removeFromBottom(int(getHeight() * .03f)); // room for beat marks

    // bars
    int numSteps = getNumSteps();
    float barWidth = area.getWidth() / float(numSteps);
    for (int i = 0; i < numSteps; i++) {
        auto column = juce::Rectangle<float>(area.getX() + i * barWidth,
                                             float(area.getY()), barWidth,
                                             float(area.getHeight()))
                          .reduced(juce::jmax(1.0f, barWidth * .1f), 0.0f);
        float value = juce::jlimit(0.0f, 1.0f, stepModifier->getStep(i));
        auto bar = column.withTrimmedTop(column.getHeight() * (1.0f - value));

        g.setColour(appLookAndFeel.colour1.withAlpha(0.25f));
        g.fillRect(column);

        auto colour = (i == playingStep) ? appLookAndFeel.colour8
                                         : appLookAndFeel.colour1;
        g.setColour(colour);
        g.fillRect(bar);

        if (i == selectedStep) {
            g.setColour(appLookAndFeel.textColour);
            g.drawRect(column, 2.0f);
        }

        // mark every beat (4 steps)
        if (i % 4 == 0) {
            g.setColour(appLookAndFeel.textColour.withAlpha(0.5f));
            g.fillRect(column.getX(), column.getBottom() + 2.0f,
                       column.getWidth(), 2.0f);
        }
    }
}

void StepModifierView::resized() { repaint(); }

void StepModifierView::nudgeStep(float delta) {
    float value = juce::jlimit(0.0f, 1.0f,
                               stepModifier->getStep(selectedStep) + delta);
    stepModifier->setStep(selectedStep, value);
    repaint();
}

void StepModifierView::nudgeParameter(int index, int direction) {
    viewModel->setParameterValue(index,
                                 viewModel->getParameterValue(index) +
                                     direction *
                                         viewModel->getParameterInterval(index));
    if (selectedStep >= getNumSteps())
        selectedStep = getNumSteps() - 1;
    repaint();
}

void StepModifierView::encoder1Increased() {
    if (hasFocus()) {
        selectedStep = juce::jmin(getNumSteps() - 1, selectedStep + 1);
        repaint();
    }
}

void StepModifierView::encoder1Decreased() {
    if (hasFocus()) {
        selectedStep = juce::jmax(0, selectedStep - 1);
        repaint();
    }
}

void StepModifierView::encoder2Increased() {
    if (hasFocus())
        nudgeStep(midiCommandManager.isControlDown ? 0.01f : 0.05f);
}

void StepModifierView::encoder2Decreased() {
    if (hasFocus())
        nudgeStep(midiCommandManager.isControlDown ? -0.01f : -0.05f);
}

void StepModifierView::encoder3Increased() {
    if (hasFocus())
        nudgeParameter(midiCommandManager.isControlDown ? stepsIndex
                                                        : rateIndex,
                       1);
}

void StepModifierView::encoder3Decreased() {
    if (hasFocus())
        nudgeParameter(midiCommandManager.isControlDown ? stepsIndex
                                                        : rateIndex,
                       -1);
}

void StepModifierView::encoder4Increased() {
    if (hasFocus())
        nudgeParameter(midiCommandManager.isControlDown ? syncIndex
                                                        : depthIndex,
                       1);
}

void StepModifierView::encoder4Decreased() {
    if (hasFocus())
        nudgeParameter(midiCommandManager.isControlDown ? syncIndex
                                                        : depthIndex,
                       -1);
}

void StepModifierView::parametersChanged() { repaint(); }
