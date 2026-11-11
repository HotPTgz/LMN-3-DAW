#include "StepSequencerView.h"
#include "PlayfulApp.h"

using namespace Playful;

StepSequencerView::StepSequencerView(tracktion::AudioTrack::Ptr p,
                                     app_services::MidiCommandManager &mcm)
    : track(p), viewModel(p), midiCommandManager(mcm) {
    midiCommandManager.addListener(this);
    viewModel.addListener(this);
}

StepSequencerView::~StepSequencerView() {
    viewModel.stop();
    viewModel.removeListener(this);
    midiCommandManager.removeListener(this);
}

bool StepSequencerView::wantsSmoothFrames() {
    return track->edit.getTransport().isPlaying();
}

juce::String StepSequencerView::getSubtitle() {
    return juce::String(viewModel.getNumberOfNotes()) + " steps" +
           utf8("  ·  ") + juce::String(viewModel.getNotesPerMeasure()) +
           " per bar";
}

juce::String StepSequencerView::getPageLabel() {
    return utf8("hold + and press a key: toggle  ·  − clear");
}

std::array<KnobInfo, 4> StepSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    int steps = viewModel.getNumberOfNotes();
    int perBar = viewModel.getNotesPerMeasure();
    int selected = viewModel.getSelectedNoteIndex();
    k[0] = {"LENGTH", juce::String(steps) + " steps",
            steps / float(viewModel.getNumNotesPerChannel()), Colours::blue};
    k[1] = {"STEPS PER BAR", juce::String(perBar), perBar / 16.0f,
            Colours::green};
    k[2] = {"STEP", juce::String(selected + 1),
            steps > 1 ? selected / float(steps - 1) : 1.0f, Colours::cream};
    k[3].enabled = false;
    return k;
}

juce::String StepSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (int c = 0; c < viewModel.getNumChannels(); c++)
        for (int n = 0; n < viewModel.getNumNotesPerChannel(); n++)
            if (viewModel.hasNoteAt(c, n))
                s << "," << c << ":" << n;
    return s;
}

void StepSequencerView::paintStage(juce::Graphics &g, double time) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    const int rows = viewModel.getNumChannels();
    const int cols = viewModel.getNumNotesPerChannel();
    const float x0 = 52.0f, top = 20.0f, bottom = 320.0f;
    const float w = (784.0f - x0) / cols;
    const float h = (bottom - top) / rows;
    int steps = viewModel.getNumberOfNotes();
    int perBar = juce::jmax(1, viewModel.getNotesPerMeasure());
    int selected = viewModel.getSelectedNoteIndex();
    bool playing = track->edit.getTransport().isPlaying();

    for (int row = 0; row < rows; row++) {
        float y = bottom - (row + 1) * h;
        int pitchClass = row % 12;
        bool sharp = juce::String(names[pitchClass]).contains("#");
        if (!sharp)
            drawText(g, names[pitchClass], 4, y, 40, h, 9, Colours::muted,
                     pitchClass == 0, juce::Justification::centredRight);

        for (int col = 0; col < cols; col++) {
            float x = x0 + col * w;
            bool inPattern = col < steps;
            bool on = viewModel.hasNoteAt(row, col);
            juce::Rectangle<float> cell(x + 2.0f, y + 1.0f, w - 4.0f, h - 2.0f);
            if (on) {
                g.setColour(
                    Colours::candy(row).withAlpha(inPattern ? 1.0f : 0.3f));
                g.fillRoundedRectangle(cell, h / 2.0f);
            } else {
                bool altBar = (col / perBar) % 2 == 1;
                auto base = altBar ? Colours::dim : Colours::panel;
                if (sharp)
                    base = base.darker(0.25f);
                g.setColour(inPattern ? base : base.withAlpha(0.3f));
                g.fillRoundedRectangle(cell, 4.0f);
            }
        }
    }

    // the step being edited (or played)
    float sx = x0 + selected * w;
    if (playing) {
        g.setColour(Colours::yellow.withAlpha(0.15f));
        g.fillRoundedRectangle(sx, top - 2.0f, w, bottom - top + 4.0f, 8.0f);
        drawBee(g, sx + w / 2.0f, 9.0f, 0.8f, time);
    } else {
        g.setColour(Colours::cream);
        g.drawRoundedRectangle(sx, top - 2.0f, w, bottom - top + 4.0f, 8.0f,
                               3.0f);
    }
}

void StepSequencerView::noteOnPressed(int noteNumber) {
    if (midiCommandManager.isPlusDown)
        viewModel.toggleNoteNumberAtSelectedIndex(noteNumber);
}

void StepSequencerView::encoder1Increased() {
    viewModel.incrementNumberOfNotes();
}

void StepSequencerView::encoder1Decreased() {
    viewModel.decrementNumberOfNotes();
}

void StepSequencerView::encoder2Increased() {
    viewModel.incrementNotesPerMeasure();
}

void StepSequencerView::encoder2Decreased() {
    viewModel.decrementNotesPerMeasure();
}

void StepSequencerView::encoder3Increased() {
    if (midiCommandManager.isMinusDown)
        viewModel.clearNotesAtSelectedIndex();

    viewModel.incrementSelectedNoteIndex();
}

void StepSequencerView::encoder3Decreased() {
    if (midiCommandManager.isMinusDown)
        viewModel.clearNotesAtSelectedIndex();

    viewModel.decrementSelectedNoteIndex();
}

void StepSequencerView::playButtonReleased() { viewModel.play(); }

void StepSequencerView::stopButtonReleased() { viewModel.stop(); }

void StepSequencerView::minusButtonReleased() {
    viewModel.clearNotesAtSelectedIndex();
}

void StepSequencerView::notesPerMeasureChanged(int) { repaint(); }
