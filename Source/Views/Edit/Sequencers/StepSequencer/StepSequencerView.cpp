#include "StepSequencerView.h"
#include "PlayfulApp.h"

using namespace Playful;
namespace Seq = app_view_models::Sequencers;

namespace {
const char *pageNames[] = {"NOTES", "STEP", "GROOVE"};
juce::String percent(double v) {
    return juce::String(juce::roundToInt(v * 100.0)) + "%";
}
} // namespace

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

bool StepSequencerView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

bool StepSequencerView::wantsSmoothFrames() {
    return track->edit.getTransport().isPlaying();
}

void StepSequencerView::setPage(int newPage) {
    page = ((newPage % 3) + 3) % 3;
    repaint();
}

juce::String StepSequencerView::getSubtitle() {
    return juce::String(viewModel.getNumberOfNotes()) + " steps" +
           utf8("  ·  ") + juce::String(viewModel.getNotesPerMeasure()) +
           " per bar";
}

juce::String StepSequencerView::getPageLabel() {
    juce::String label = juce::String(page + 1) + "/3";
    if (page == 0)
        return label + utf8("  ·  hold + and press a key: toggle");
    if (page == 1)
        return label + (midiCommandManager.isControlDown
                            ? juce::String("  ctrl: every step")
                            : utf8("  ·  ctrl: pick step / all"));
    return label;
}

std::array<KnobInfo, 4> StepSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    int steps = viewModel.getNumberOfNotes();
    int selected = viewModel.getSelectedNoteIndex();
    bool control = midiCommandManager.isControlDown;

    if (page == 1) {
        auto t = viewModel.getTricks(selected);
        if (control)
            k[0] = {"STEP", juce::String(selected + 1),
                    steps > 1 ? selected / float(steps - 1) : 1.0f,
                    Colours::blue};
        else
            k[0] = {"VELOCITY", juce::String(t.velocity), t.velocity / 127.0f,
                    Colours::blue};
        k[1] = {control ? "LENGTH (ALL)" : "LENGTH",
                juce::String(t.length) + (t.length == 1 ? " step" : " steps"),
                t.length / 16.0f, Colours::green};
        k[2] = {control ? "CHANCE (ALL)" : "CHANCE", percent(t.chance),
                t.chance, Colours::cream};
        k[3] = {control ? utf8("RATCHET (ALL) · PAGE") : utf8("RATCHET · PAGE"),
                utf8("×") + juce::String(t.ratchet), t.ratchet / 4.0f,
                Colours::red};
        return k;
    }

    if (page == 2) {
        auto groove = viewModel.getGroove();
        k[0] = {"SWING", percent(groove.swing), float(groove.swing),
                Colours::blue};
        k[1] = {"GATE", percent(groove.gate), float(groove.gate),
                Colours::green};
        k[2] = {"DIRECTION", Seq::directionName(groove.direction),
                int(groove.direction) / 3.0f, Colours::cream};
        k[3] = {utf8("HUMANIZE · PAGE"), percent(groove.humanize),
                float(groove.humanize), Colours::red};
        return k;
    }

    int perBar = viewModel.getNotesPerMeasure();
    k[0] = {"LENGTH", juce::String(steps) + " steps",
            steps / float(viewModel.getNumNotesPerChannel()), Colours::blue};
    k[1] = {"STEPS PER BAR", juce::String(perBar), perBar / 16.0f,
            Colours::green};
    k[2] = {"STEP", juce::String(selected + 1),
            steps > 1 ? selected / float(steps - 1) : 1.0f, Colours::cream};
    k[3] = {utf8("— · PAGE"), "notes", 0.0f, Colours::red};
    return k;
}

juce::String StepSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (int c = 0; c < viewModel.getNumChannels(); c++)
        for (int n = 0; n < viewModel.getNumNotesPerChannel(); n++)
            if (viewModel.hasNoteAt(c, n))
                s << "," << c << ":" << n;
    for (int n = 0; n < 16; n++) {
        auto t = viewModel.getTricks(n);
        s << ";" << t.velocity << t.length << t.chance << t.ratchet;
    }
    s << page << int(midiCommandManager.isControlDown);
    return s;
}

void StepSequencerView::paintTabs(juce::Graphics &g) {
    float x = 560.0f;
    for (int i = 0; i < 3; i++) {
        float w = 18.0f + juce::String(pageNames[i]).length() * 8.0f;
        drawPill(g, {x, 4.0f, w, 22.0f}, pageNames[i],
                 i == page ? Colours::yellow : Colours::panel,
                 i == page ? Colours::ink : Colours::muted, 11.0f);
        x += w + 8.0f;
    }
}

void StepSequencerView::paintStage(juce::Graphics &g, double time) {
    paintTabs(g);
    if (page == 1)
        paintTricks(g);
    else if (page == 2)
        paintGroove(g, time);
    else
        paintNotes(g, time);
}

void StepSequencerView::paintNotes(juce::Graphics &g, double time) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    const int rows = viewModel.getNumChannels();
    const int cols = viewModel.getNumNotesPerChannel();
    const float x0 = 52.0f, top = 34.0f, bottom = 322.0f;
    const float w = (784.0f - x0) / cols;
    const float h = (bottom - top) / rows;
    int steps = viewModel.getNumberOfNotes();
    int perBar = juce::jmax(1, viewModel.getNotesPerMeasure());
    int selected = viewModel.getSelectedNoteIndex();
    bool playing = track->edit.getTransport().isPlaying();

    std::array<Seq::StepTricks, 16> tricks;
    for (int s = 0; s < 16; s++)
        tricks[size_t(s)] = viewModel.getTricks(s);

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
                const auto &t = tricks[size_t(col)];
                // a tie stretches the note over the next steps
                if (t.length > 1) {
                    float tieWidth =
                        juce::jmin(t.length, cols - col) * w - 4.0f;
                    g.setColour(Colours::candy(row).withAlpha(0.35f));
                    g.fillRoundedRectangle(x + 2.0f, y + 1.0f, tieWidth,
                                           h - 2.0f, h / 2.0f);
                }
                float alpha = 0.35f + 0.65f * t.velocity / 127.0f;
                g.setColour(
                    Colours::candy(row).withAlpha(inPattern ? alpha : 0.25f));
                g.fillRoundedRectangle(cell, h / 2.0f);
                g.setColour(Colours::ink.withAlpha(0.7f));
                for (int r = 1; r < t.ratchet; r++) {
                    float rx = cell.getX() + cell.getWidth() * r / t.ratchet;
                    g.drawLine(rx, cell.getY() + 2, rx, cell.getBottom() - 2,
                               2.0f);
                }
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

    // dice above steps that only sometimes play
    for (int col = 0; col < juce::jmin(steps, 16); col++)
        if (tricks[size_t(col)].chance < 1.0f) {
            float cx = x0 + col * w + w / 2.0f;
            g.setColour(Colours::text);
            g.fillRoundedRectangle(cx - 6.0f, 15.0f, 12.0f, 12.0f, 3.0f);
            g.setColour(Colours::ink);
            fillEllipseCentred(g, cx - 2.5f, 18.5f, 1.4f, 1.4f);
            fillEllipseCentred(g, cx + 2.5f, 23.5f, 1.4f, 1.4f);
        }

    float sx = x0 + selected * w;
    if (playing) {
        g.setColour(Colours::yellow.withAlpha(0.15f));
        g.fillRoundedRectangle(sx, top - 2.0f, w, bottom - top + 4.0f, 8.0f);
        drawBee(g, sx + w / 2.0f, 20.0f, 0.8f, time);
    } else {
        g.setColour(Colours::cream);
        g.drawRoundedRectangle(sx, top - 2.0f, w, bottom - top + 4.0f, 8.0f,
                               3.0f);
    }
}

void StepSequencerView::paintTricks(juce::Graphics &g) {
    const float x0 = 118.0f, w = (786.0f - x0) / 16.0f;
    int steps = viewModel.getNumberOfNotes();
    int selected = viewModel.getSelectedNoteIndex();
    const char *lanes[] = {"VELOCITY", "LENGTH", "CHANCE", "RATCHET"};
    for (int i = 0; i < 4; i++) {
        float y = 34.0f + i * 72.0f;
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(8.0f, y, 784.0f, 64.0f, 16.0f);
        drawText(g, lanes[i], 18, y, 96, 64, 11, Colours::encoder(i), true,
                 juce::Justification::centredLeft);
    }

    for (int s = 0; s < 16; s++) {
        float cx = x0 + s * w + w / 2.0f;
        bool hasNotes = false;
        for (int c = 0; c < viewModel.getNumChannels(); c++)
            hasNotes = hasNotes || viewModel.hasNoteAt(c, s);
        float alpha = s < steps ? (hasNotes ? 1.0f : 0.35f) : 0.15f;
        auto t = viewModel.getTricks(s);

        float vh = t.velocity / 127.0f * 48.0f;
        g.setColour(
            (s == selected ? Colours::cream : Colours::blue).withAlpha(alpha));
        g.fillRoundedRectangle(cx - 9.0f, 34.0f + 56.0f - vh, 18.0f, vh, 6.0f);

        float lw = juce::jmin(t.length, 16 - s) * w - 8.0f;
        g.setColour(Colours::green.withAlpha(alpha));
        g.fillRoundedRectangle(x0 + s * w + 4.0f, 106.0f + 21.0f, lw, 22.0f,
                               11.0f);

        float cy = 178.0f + 32.0f;
        g.setColour(Colours::dim);
        fillEllipseCentred(g, cx, cy, 12.0f, 12.0f);
        juce::Path pie;
        pie.addPieSegment(cx - 12.0f, cy - 12.0f, 24.0f, 24.0f, 0.0f,
                          t.chance * juce::MathConstants<float>::twoPi, 0.0f);
        g.setColour(Colours::cream.withAlpha(alpha));
        g.fillPath(pie);

        for (int r = 0; r < t.ratchet; r++) {
            g.setColour(Colours::red.withAlpha(alpha));
            fillEllipseCentred(g, cx, 250.0f + 52.0f - r * 14.0f, 6.0f, 6.0f);
        }
    }

    g.setColour(Colours::cream);
    g.drawRoundedRectangle(x0 + selected * w + 2.0f, 32.0f, w - 4.0f, 288.0f,
                           12.0f, 3.0f);
}

void StepSequencerView::paintGroove(juce::Graphics &g, double time) {
    auto groove = viewModel.getGroove();
    int steps = viewModel.getNumberOfNotes();
    const float x0 = 150.0f, w = (770.0f - x0) / 16.0f;

    const char *rows[] = {"SWING", "GATE", "DIRECTION", "HUMANIZE"};
    for (int i = 0; i < 4; i++) {
        float y = 34.0f + i * 72.0f;
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(8.0f, y, 784.0f, 64.0f, 16.0f);
        drawText(g, rows[i], 18, y, 120, 64, 11, Colours::encoder(i), true,
                 juce::Justification::centredLeft);
    }

    // swing: off-beats lean later
    for (int s = 0; s < 16; s++) {
        float off = float(Seq::swingOffset(s, groove.swing, 1.0)) * w;
        g.setColour((s % 2 ? Colours::blue : Colours::cream)
                        .withAlpha(s < steps ? 1.0f : 0.3f));
        fillEllipseCentred(g, x0 + s * w + w / 2.0f + off, 66.0f,
                           s % 4 == 0 ? 7.0f : 5.0f, s % 4 == 0 ? 7.0f : 5.0f);
    }

    // gate: how much of each step a note fills
    for (int s = 0; s < 16; s++) {
        g.setColour(Colours::dim);
        g.fillRoundedRectangle(x0 + s * w + 3.0f, 128.0f, w - 6.0f, 16.0f,
                               8.0f);
        g.setColour(Colours::green.withAlpha(s < steps ? 1.0f : 0.3f));
        g.fillRoundedRectangle(x0 + s * w + 3.0f, 128.0f,
                               (w - 6.0f) * float(groove.gate), 16.0f, 8.0f);
    }

    // direction
    const char *labels[] = {"forward", "backward", "ping-pong", "random"};
    for (int i = 0; i < 4; i++) {
        bool on = int(groove.direction) == i;
        drawPill(g, {x0 + i * 156.0f, 194.0f, 140.0f, 28.0f}, labels[i],
                 on ? Colours::cream : Colours::dim,
                 on ? Colours::ink : Colours::muted, 13.0f);
    }

    // humanize: a line that wobbles more
    juce::Path wobble;
    juce::Random random(3);
    for (int i = 0; i <= 60; i++) {
        float x = x0 + i * (620.0f / 60.0f);
        float y = 282.0f + float(groove.humanize) * 18.0f *
                               std::sin(i * 0.9f + float(time) * 3.0f) *
                               (0.5f + random.nextFloat());
        if (i == 0)
            wobble.startNewSubPath(x, y);
        else
            wobble.lineTo(x, y);
    }
    g.setColour(Colours::red);
    g.strokePath(wobble,
                 juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
}

//==============================================================================
void StepSequencerView::noteOnPressed(int noteNumber) {
    if (page == 0 && midiCommandManager.isPlusDown)
        viewModel.toggleNoteNumberAtSelectedIndex(noteNumber);
}

void StepSequencerView::encoder1Increased() {
    if (page == 0)
        viewModel.incrementNumberOfNotes();
    else if (page == 1) {
        if (midiCommandManager.isControlDown)
            viewModel.incrementSelectedNoteIndex();
        else
            viewModel.nudgeVelocity(1);
    } else
        viewModel.nudgeSwing(1);
    repaint();
}

void StepSequencerView::encoder1Decreased() {
    if (page == 0)
        viewModel.decrementNumberOfNotes();
    else if (page == 1) {
        if (midiCommandManager.isControlDown)
            viewModel.decrementSelectedNoteIndex();
        else
            viewModel.nudgeVelocity(-1);
    } else
        viewModel.nudgeSwing(-1);
    repaint();
}

void StepSequencerView::encoder2Increased() {
    if (page == 0)
        viewModel.incrementNotesPerMeasure();
    else if (page == 1)
        viewModel.nudgeTie(1, midiCommandManager.isControlDown);
    else
        viewModel.nudgeGate(1);
    repaint();
}

void StepSequencerView::encoder2Decreased() {
    if (page == 0)
        viewModel.decrementNotesPerMeasure();
    else if (page == 1)
        viewModel.nudgeTie(-1, midiCommandManager.isControlDown);
    else
        viewModel.nudgeGate(-1);
    repaint();
}

void StepSequencerView::encoder3Increased() {
    if (page == 0) {
        if (midiCommandManager.isMinusDown)
            viewModel.clearNotesAtSelectedIndex();
        viewModel.incrementSelectedNoteIndex();
    } else if (page == 1)
        viewModel.nudgeChance(1, midiCommandManager.isControlDown);
    else
        viewModel.nudgeDirection(1);
    repaint();
}

void StepSequencerView::encoder3Decreased() {
    if (page == 0) {
        if (midiCommandManager.isMinusDown)
            viewModel.clearNotesAtSelectedIndex();
        viewModel.decrementSelectedNoteIndex();
    } else if (page == 1)
        viewModel.nudgeChance(-1, midiCommandManager.isControlDown);
    else
        viewModel.nudgeDirection(-1);
    repaint();
}

void StepSequencerView::encoder4Increased() {
    if (page == 1)
        viewModel.nudgeRatchet(1, midiCommandManager.isControlDown);
    else if (page == 2)
        viewModel.nudgeHumanize(1);
    repaint();
}

void StepSequencerView::encoder4Decreased() {
    if (page == 1)
        viewModel.nudgeRatchet(-1, midiCommandManager.isControlDown);
    else if (page == 2)
        viewModel.nudgeHumanize(-1);
    repaint();
}

void StepSequencerView::encoder4ButtonReleased() { setPage(page + 1); }

void StepSequencerView::playButtonReleased() {
    if (hasFocus())
        viewModel.play();
}

void StepSequencerView::stopButtonReleased() {
    if (hasFocus())
        viewModel.stop();
}

void StepSequencerView::minusButtonReleased() {
    if (hasFocus() && page == 0)
        viewModel.clearNotesAtSelectedIndex();
}

void StepSequencerView::notesPerMeasureChanged(int) { repaint(); }
