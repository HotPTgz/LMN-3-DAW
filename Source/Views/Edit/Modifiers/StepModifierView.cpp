#include "StepModifierView.h"
#include "PlayfulScenes.h"

using namespace Playful;

StepModifierView::StepModifierView(tracktion::StepModifier *mod,
                                   app_services::MidiCommandManager &mcm)
    : stepModifier(mod),
      viewModel(app_view_models::createModifierViewModel(mod)),
      midiCommandManager(mcm) {
    midiCommandManager.addListener(this);
    viewModel->addListener(this);
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

juce::String StepModifierView::parameterText(int index) {
    return viewModel->getParameterValueText(
        index, viewModel->getParameterValue(index));
}

float StepModifierView::parameterPosition(int index) {
    auto range = viewModel->getParameterRange(index);
    if (range.getLength() <= 0)
        return 0.0f;
    return float((viewModel->getParameterValue(index) - range.getStart()) /
                 range.getLength());
}

juce::String StepModifierView::getSubtitle() {
    return juce::String(getNumSteps()) + " steps";
}

juce::String StepModifierView::getPageLabel() {
    return midiCommandManager.isControlDown ? "ctrl" : "ctrl: steps/sync";
}

juce::String StepModifierView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    s << "|" << stepModifier->getCurrentStep() << "|" << selectedStep;
    for (int i = 0; i < getNumSteps(); i++)
        s << "|" << stepModifier->getStep(i);
    return s;
}

std::array<KnobInfo, 4> StepModifierView::getKnobs() {
    std::array<KnobInfo, 4> knobs;
    for (int i = 0; i < 4; i++)
        knobs[size_t(i)].colour = Colours::encoder(i);

    float value = juce::jlimit(0.0f, 1.0f, stepModifier->getStep(selectedStep));
    bool control = midiCommandManager.isControlDown;
    knobs[0] = {"STEP", juce::String(selectedStep + 1),
                getNumSteps() > 1 ? float(selectedStep) / (getNumSteps() - 1)
                                  : 0.0f,
                Colours::encoder(0)};
    knobs[1] = {control ? "VALUE (FINE)" : "VALUE",
                juce::String(juce::roundToInt(value * 100)) + "%", value,
                Colours::encoder(1)};
    if (control) {
        knobs[2] = {"STEPS", parameterText(stepsIndex),
                    parameterPosition(stepsIndex), Colours::encoder(2)};
        knobs[3] = {"SYNC", parameterText(syncIndex),
                    parameterPosition(syncIndex), Colours::encoder(3)};
    } else {
        knobs[2] = {"RATE", parameterText(rateIndex),
                    parameterPosition(rateIndex), Colours::encoder(2)};
        knobs[3] = {"DEPTH",
                    juce::String(juce::roundToInt(
                        viewModel->getParameterValue(depthIndex) * 100)) +
                        "%",
                    parameterPosition(depthIndex), Colours::encoder(3)};
    }
    return knobs;
}

void StepModifierView::paintStage(juce::Graphics &g, double time) {
    using juce::AffineTransform;
    int numSteps = getNumSteps();
    int numJars = juce::jmax(16, numSteps);
    float spacing = 720.0f / numJars;
    float jarWidth = spacing * 0.84f;
    int playing = stepModifier->getCurrentStep();

    static const juce::Colour candy[] = {Colours::pink,   Colours::yellow,
                                         Colours::green,  Colours::blue,
                                         Colours::purple, Colours::orange};
    auto jarX = [&](int i) { return 40 + i * spacing; };
    for (int i = 0; i < numJars; i++) {
        float x = jarX(i);
        bool on = i < numSteps;
        bool isSelected = i == selectedStep;
        auto jar = juce::Rectangle<float>(x, 96, jarWidth, 190);
        float radius = juce::jmin(12.0f, jarWidth / 3);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(jar, radius);
        float value =
            on ? juce::jlimit(0.0f, 1.0f, stepModifier->getStep(i)) : 0.0f;
        float fill = juce::jmax(14.0f, value * 186);
        g.setColour(on ? candy[i % 6] : Colours::dim);
        g.fillRoundedRectangle(x, 96 + 190 - fill, jarWidth, fill, radius);
        g.setColour(isSelected ? Colours::cream : Colours::dim);
        g.drawRoundedRectangle(jar, radius, isSelected ? 4.0f : 2.0f);
        g.setColour(Colours::muted);
        g.fillRoundedRectangle(x + jarWidth * 0.1f, 88, jarWidth * 0.8f, 10, 5);
        // a mark under every beat
        if (on && i % 4 == 0) {
            g.setColour(Colours::muted.withAlpha(0.6f));
            g.fillRoundedRectangle(x, 294, jarWidth, 4, 2);
        }
    }

    // flag on the selected jar
    {
        float fx = jarX(selectedStep) + jarWidth / 2;
        juce::Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(juce::jmin(fx, 640.0f), 4));
        auto flag = PlayfulScenes::svg("M0 0 L26 8 L0 16 Z");
        g.setColour(Colours::red);
        g.fillPath(flag);
        g.setColour(Colours::cream);
        g.strokePath(flag, juce::PathStrokeType(3, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        PlayfulScenes::strokePath(g, PlayfulScenes::svg("M0 0 L0 38"), 3);
        PlayfulScenes::text(
            g,
            "step " + juce::String(selectedStep + 1) + utf8(" · ") +
                juce::String(juce::roundToInt(
                    stepModifier->getStep(selectedStep) * 100)) +
                "%",
            30, 14, 14, Colours::text);
    }

    // the hamster hops along the playing step, or naps on the first jar
    bool isPlaying = playing >= 0 && playing < numSteps &&
                     stepModifier->edit.getTransport().isPlaying();
    int jar = isPlaying ? playing : 0;
    float hop = isPlaying ? -18.0f * pingPong(time, 0.3) : 0.0f;
    juce::Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::scale(juce::jmin(1.0f, spacing / 45))
                       .translated(jarX(jar) + jarWidth / 2, 70 + hop));
    g.setColour(Colours::orange);
    fillEllipseCentred(g, 0, 0, 22, 17);
    fillEllipseCentred(g, -14, -14, 7, 7);
    fillEllipseCentred(g, 14, -14, 7, 7);
    g.setColour(Colours::pink);
    fillEllipseCentred(g, -14, -14, 3.5f, 3.5f);
    fillEllipseCentred(g, 14, -14, 3.5f, 3.5f);
    g.setColour(Colours::cream);
    fillEllipseCentred(g, 0, 6, 12, 8);
    drawFace(g, 0, -3, 0.55f, isPlaying ? Mood::smile : Mood::sleep, time);
}

void StepModifierView::nudgeStep(float delta) {
    float value =
        juce::jlimit(0.0f, 1.0f, stepModifier->getStep(selectedStep) + delta);
    stepModifier->setStep(selectedStep, value);
    repaint();
}

void StepModifierView::nudgeParameter(int index, int direction) {
    viewModel->setParameterValue(
        index, viewModel->getParameterValue(index) +
                   direction * viewModel->getParameterInterval(index));
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
        nudgeParameter(
            midiCommandManager.isControlDown ? stepsIndex : rateIndex, 1);
}

void StepModifierView::encoder3Decreased() {
    if (hasFocus())
        nudgeParameter(
            midiCommandManager.isControlDown ? stepsIndex : rateIndex, -1);
}

void StepModifierView::encoder4Increased() {
    if (hasFocus())
        nudgeParameter(
            midiCommandManager.isControlDown ? syncIndex : depthIndex, 1);
}

void StepModifierView::encoder4Decreased() {
    if (hasFocus())
        nudgeParameter(
            midiCommandManager.isControlDown ? syncIndex : depthIndex, -1);
}

void StepModifierView::parametersChanged() { repaint(); }
