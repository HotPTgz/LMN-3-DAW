#include "PlayfulSequencerViews.h"
#include "PlayfulApp.h"

using namespace Playful;
namespace Seq = app_view_models::Sequencers;

namespace {
juce::String noteName(int midi) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    return juce::String(names[((midi % 12) + 12) % 12]) +
           juce::String(midi / 12 - 1);
}

juce::String percent(double v) {
    return juce::String(juce::roundToInt(v * 100.0)) + "%";
}

juce::Colour noteColour(int midi) { return Colours::candy((midi % 12) % 6); }

void ladybug(juce::Graphics &g, float cx, float cy, double time) {
    float bob = -2.0f * pingPong(time, 0.4);
    g.setColour(Colours::red);
    fillEllipseCentred(g, cx, cy + bob, 11, 9);
    g.setColour(Colours::ink);
    g.drawLine(cx, cy + bob - 9, cx, cy + bob + 9, 2.0f);
    fillEllipseCentred(g, cx - 4, cy + bob - 2, 2, 2);
    fillEllipseCentred(g, cx + 4, cy + bob + 3, 2, 2);
    fillEllipseCentred(g, cx - 10, cy + bob - 6, 5, 5);
}
} // namespace

//==============================================================================
SequencerScreen::SequencerScreen(app_services::MidiCommandManager &mcm)
    : midiCommandManager(mcm) {
    midiCommandManager.addListener(this);
}

SequencerScreen::~SequencerScreen() { midiCommandManager.removeListener(this); }

bool SequencerScreen::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

//==============================================================================
// Drums
DrumSequencerView::DrumSequencerView(tracktion::AudioTrack::Ptr t,
                                     app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void DrumSequencerView::turn(int encoder, int direction) {
    if (control()) {
        switch (encoder) {
        case 0:
            vm.nudgeLevel(direction);
            break;
        case 1:
            vm.nudgeSwing(direction);
            break;
        case 2:
            vm.nudgeStepsPerBar(direction);
            break;
        default:
            vm.nudgeFillEvery(direction);
            break;
        }
    } else {
        switch (encoder) {
        case 0:
            vm.moveRow(direction);
            break;
        case 1:
            vm.moveStep(direction);
            break;
        case 2:
            vm.nudgeLength(direction);
            break;
        default:
            vm.nudgeVelocity(direction);
            break;
        }
    }
    repaint();
}

void DrumSequencerView::encoder4ButtonReleased() {
    if (control())
        vm.toggleEditingFill();
    else
        vm.clearSelected();
    repaint();
}

void DrumSequencerView::noteOnPressed(int noteNumber) {
    if (midiCommandManager.isPlusDown)
        vm.hitNote(noteNumber);
    else
        vm.selectRowForNote(noteNumber);
    repaint();
}

void DrumSequencerView::minusButtonReleased() {
    if (hasFocus()) {
        vm.clearSelected();
        repaint();
    }
}

void DrumSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void DrumSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

juce::String DrumSequencerView::getTitle() {
    return vm.isEditingFill() ? "Drums" + utf8(" · fill") : "Drums";
}

juce::String DrumSequencerView::getSubtitle() {
    return juce::String(vm.getLength()) + " steps" + utf8("  ·  ") +
           juce::String(vm.getStepsPerBar()) + " per bar";
}

juce::String DrumSequencerView::getPageLabel() {
    return control() ? juce::String("ctrl")
                     : utf8("hold + and press a key: toggle  ·  ctrl: more");
}

std::array<KnobInfo, 4> DrumSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    int row = vm.getSelectedRow(), step = vm.getSelectedStep();
    if (control()) {
        float level = vm.getLevel(row);
        k[0] = {"LEVEL", percent(level), level, Colours::blue};
        k[1] = {"SWING", percent(vm.getSwing()), float(vm.getSwing()),
                Colours::green};
        k[2] = {"STEPS PER BAR", juce::String(vm.getStepsPerBar()),
                vm.getStepsPerBar() / 32.0f, Colours::cream};
        int every = vm.getFillEvery();
        k[3] = {utf8("FILL · EDIT FILL"),
                every > 0 ? "every " + juce::String(every)
                          : juce::String("off"),
                every / 8.0f, Colours::red};
        return k;
    }
    static const char *strengths[] = {"off", "ghost", "normal", "accent"};
    int cell = vm.getCell(row, step);
    k[0] = {"SOUND", vm.getRowName(row), row / 23.0f, Colours::blue};
    k[1] = {"STEP", juce::String(step + 1),
            vm.getLength() > 1 ? step / float(vm.getLength() - 1) : 1.0f,
            Colours::green};
    k[2] = {"LENGTH", juce::String(vm.getLength()) + " steps",
            vm.getLength() / 64.0f, Colours::cream};
    k[3] = {utf8("VELOCITY · CLEAR"), strengths[cell], cell / 3.0f,
            Colours::red};
    return k;
}

juce::String DrumSequencerView::getChangeSignature() {
    auto &input = vm.getInput();
    return PlayfulScreen::getChangeSignature() + "|" +
           juce::String(vm.getSelectedRow()) + "," +
           juce::String(vm.getSelectedStep()) + "|" +
           (vm.isEditingFill() ? input.fill : input.main)
               .toString()
               .substring(0, 1600) +
           juce::String(int(control()));
}

void DrumSequencerView::paintStage(juce::Graphics &g, double time) {
    if (control())
        paintGroove(g, time);
    else
        paintGrid(g, time);
}

void DrumSequencerView::paintGrid(juce::Graphics &g, double time) {
    const float x0 = 110.0f, w = (786.0f - x0) / 16.0f, y0 = 48.0f,
                h = (320.0f - y0) / 8.0f;
    int selectedRow = vm.getSelectedRow(), selectedStep = vm.getSelectedStep();
    int playing = vm.getPlayingStep();
    int page = (playing >= 0 ? playing : selectedStep) / 16;
    int pages = (vm.getLength() + 15) / 16;
    int firstRow = juce::jlimit(0, Seq::drumRows - 8, selectedRow - 3);

    // which 16 steps are on screen
    for (int p = 0; p < 4; p++) {
        g.setColour(p == page   ? Colours::yellow
                    : p < pages ? Colours::muted
                                : Colours::dim);
        g.fillRoundedRectangle(16.0f + p * 22.0f, 14.0f, 18.0f, 10.0f, 5.0f);
    }
    drawText(g,
             "steps " + juce::String(page * 16 + 1) + utf8("–") +
                 juce::String(page * 16 + 16),
             16, 28, 90, 14, 11, Colours::muted, false,
             juce::Justification::centredLeft);

    if (vm.isEditingFill()) {
        juce::Path frame;
        frame.addRoundedRectangle(x0 - 6.0f, y0 - 6.0f, 786.0f - x0 + 10.0f,
                                  h * 8 + 10.0f, 14.0f);
        drawDashedPath(g, frame, Colours::pink, 3.0f, 8.0f, 6.0f);
    }

    for (int r = 0; r < 8; r++) {
        int row = firstRow + r;
        float y = y0 + r * h;
        bool isSelected = row == selectedRow;
        if (isSelected) {
            g.setColour(Colours::dim);
            g.fillRoundedRectangle(6.0f, y, 786.0f, h, 12.0f);
        }
        drawBadge(g, 22.0f, y + h / 2.0f, 11.0f, Colours::candy(row), time);
        drawText(g, vm.getRowName(row), 38.0f, y, 70.0f, h, 11.0f,
                 isSelected ? Colours::text : Colours::muted, isSelected,
                 juce::Justification::centredLeft);

        for (int s = 0; s < 16; s++) {
            int step = page * 16 + s;
            float x = x0 + s * w;
            bool inPattern = step < vm.getLength();
            int cell = inPattern ? vm.getCell(row, step) : 0;
            float pad = cell == 3   ? 3.0f
                        : cell == 2 ? 6.0f
                        : cell == 1 ? 10.0f
                                    : 3.0f;
            juce::Rectangle<float> r2(x + pad, y + pad, w - 2 * pad,
                                      h - 2 * pad);
            if (cell > 0) {
                g.setColour(
                    Colours::candy(row).withAlpha(cell == 1 ? 0.55f : 1.0f));
                g.fillRoundedRectangle(r2,
                                       juce::jmin(9.0f, r2.getHeight() / 2));
                if (cell == 3) {
                    g.setColour(Colours::white.withAlpha(0.85f));
                    fillEllipseCentred(g, r2.getX() + 7, r2.getY() + 7, 3.2f,
                                       3.2f);
                }
            } else {
                g.setColour((s % 4 == 0 ? Colours::dim : Colours::panel)
                                .withAlpha(inPattern ? 0.85f : 0.25f));
                g.fillRoundedRectangle(r2, 9.0f);
            }
        }
    }

    // the cursor and the playhead
    if (selectedStep / 16 == page && selectedRow >= firstRow &&
        selectedRow < firstRow + 8) {
        g.setColour(Colours::cream);
        g.drawRoundedRectangle(x0 + (selectedStep % 16) * w,
                               y0 + (selectedRow - firstRow) * h, w, h, 10.0f,
                               3.0f);
    }
    if (playing >= 0) {
        float px = x0 + (playing % 16) * w;
        g.setColour(Colours::yellow.withAlpha(0.16f));
        g.fillRoundedRectangle(px, y0 - 2.0f, w, h * 8 + 4.0f, 10.0f);
        ladybug(g, px + w / 2.0f, 28.0f, time);
        if (vm.isPlayingFill())
            drawPill(g, {650.0f, 16.0f, 120.0f, 22.0f}, "FILL!", Colours::pink,
                     Colours::ink, 12.0f);
    }
}

void DrumSequencerView::paintGroove(juce::Graphics &g, double time) {
    int row = vm.getSelectedRow();
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(20.0f, 14.0f, 210.0f, 300.0f, 28.0f);
    drawDrum(g, 125.0f, 150.0f, 1.0f, time);
    drawText(g, vm.getRowName(row), 30, 236, 190, 28, 20, Colours::text, true);
    drawText(g, "level " + percent(vm.getLevel(row)), 30, 264, 190, 18, 13,
             Colours::muted);

    // this sound's hits on the current page, leaning by the swing
    int page = vm.getSelectedStep() / 16;
    drawText(g, "HITS" + utf8(" · SWING ") + percent(vm.getSwing()), 250, 26,
             400, 16, 12, Colours::muted, false,
             juce::Justification::centredLeft);
    const float w = 33.5f;
    for (int s = 0; s < 16; s++) {
        int step = page * 16 + s;
        int cell = step < vm.getLength() ? vm.getCell(row, step) : 0;
        float off = (s % 2 == 1) ? float(vm.getSwing()) * (w - 24.0f) : 0.0f;
        float x = 250.0f + s * w + off;
        float velocity = Seq::drumVelocity(cell, vm.getLevel(row)) / 127.0f;
        g.setColour(cell > 0 ? Colours::red.withAlpha(0.45f + 0.55f * velocity)
                             : (s % 4 == 0 ? Colours::dim : Colours::panel));
        g.fillRoundedRectangle(x, 48.0f, 24.0f, 30.0f, 9.0f);
        float bh = juce::jmax(6.0f, velocity * 130.0f);
        g.setColour(cell == 3  ? Colours::cream
                    : cell > 0 ? Colours::blue
                               : Colours::dim);
        g.fillRoundedRectangle(x, 222.0f - bh, 24.0f, bh, 8.0f);
    }
    g.setColour(Colours::dim);
    g.drawLine(248, 226, 784, 226, 3.0f);

    // the fill ticket
    juce::Path ticket;
    ticket.addRoundedRectangle(250.0f, 254.0f, 534.0f, 58.0f, 20.0f);
    g.setColour(Colours::panel);
    g.fillPath(ticket);
    drawDashedPath(g, ticket, Colours::pink, 3.0f, 8.0f, 6.0f);
    drawText(g, "FILL", 272, 262, 100, 16, 13, Colours::pink, true,
             juce::Justification::centredLeft);
    int every = vm.getFillEvery();
    drawText(g,
             every > 0 ? "plays the fill pattern every " + juce::String(every) +
                             " passes"
                       : juce::String("no fill (press 4 to edit the fill)"),
             272, 280, 300, 22, 14, Colours::text, false,
             juce::Justification::centredLeft);
    for (int b = 0; b < 8; b++) {
        bool lit = every > 0 && (b + 1) % every == 0;
        g.setColour(lit ? Colours::pink : Colours::dim);
        fillEllipseCentred(g, 560.0f + b * 28.0f, 283.0f, 7.0f, 7.0f);
    }
}

//==============================================================================
// Endless
EndlessSequencerView::EndlessSequencerView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void EndlessSequencerView::noteOnPressed(int noteNumber) {
    vm.notePressed(noteNumber);
    repaint();
}

void EndlessSequencerView::plusButtonReleased() {
    if (!hasFocus())
        return;
    if (vm.isRecording())
        vm.finishRecording();
    else
        vm.startRecording();
    repaint();
}

void EndlessSequencerView::minusButtonReleased() {
    if (hasFocus()) {
        vm.addRest();
        repaint();
    }
}

void EndlessSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void EndlessSequencerView::stopButtonReleased() {
    if (hasFocus()) {
        vm.finishRecording();
        vm.stop();
        repaint();
    }
}

juce::String EndlessSequencerView::getSubtitle() {
    if (vm.isRecording())
        return "recording";
    return vm.isPlaying() ? "playing" : "stopped";
}

juce::String EndlessSequencerView::getPageLabel() {
    return vm.isRecording() ? utf8("keys: add  ·  − rest  ·  + done")
                            : utf8("+ record new  ·  keys: transpose");
}

std::array<KnobInfo, 4> EndlessSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    int rate = vm.getRateIndex();
    k[0] = {"RATE", vm.rateNames()[rate], rate / 3.0f, Colours::blue};
    k[1] = {"GATE", percent(vm.getGate()), float(vm.getGate()), Colours::green};
    k[2] = {"SWING", percent(vm.getSwing()), float(vm.getSwing()),
            Colours::cream};
    int t = vm.getTranspose();
    k[3] = {utf8("TRANSPOSE · RESET"), (t > 0 ? "+" : "") + juce::String(t),
            (t + 24) / 48.0f, Colours::red};
    return k;
}

juce::String EndlessSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (auto n : vm.getNotes())
        s << "," << n;
    return s;
}

void EndlessSequencerView::paintStage(juce::Graphics &g, double time) {
    const float cx = 330.0f, cy = 172.0f, rx = 262.0f, ry = 122.0f;
    g.setColour(Colours::panel);
    g.drawEllipse(cx - rx, cy - ry, rx * 2, ry * 2, 44.0f);
    {
        juce::Path track;
        track.addEllipse(cx - rx, cy - ry, rx * 2, ry * 2);
        drawDashedPath(g, track, Colours::dim, 3.0f, 4.0f, 12.0f);
    }

    const auto &notes = vm.getNotes();
    int n = notes.size();
    int playing = vm.getPlayingIndex();
    int head = playing >= 0 ? playing : (vm.isRecording() ? n - 1 : 0);
    float r = n <= 12 ? 20.0f : juce::jmax(8.0f, 20.0f * 12.0f / n);

    auto pointFor = [&](int i) {
        float a = -juce::MathConstants<float>::halfPi +
                  i * juce::MathConstants<float>::twoPi / juce::jmax(1, n);
        return juce::Point<float>(cx + rx * std::cos(a), cy + ry * std::sin(a));
    };

    for (int i = 0; i < n; i++) {
        if (i == head)
            continue;
        auto p = pointFor(i);
        if (notes[i] < 0) {
            juce::Path ring;
            ring.addEllipse(p.x - r, p.y - r, r * 2, r * 2);
            drawDashedPath(g, ring, Colours::muted, 3.0f, 4.0f, 5.0f);
            continue;
        }
        g.setColour(noteColour(notes[i] + vm.getTranspose()));
        fillEllipseCentred(g, p.x, p.y, r, r);
        if (r >= 14.0f)
            drawText(
                g, noteName(notes[i] + vm.getTranspose()).dropLastCharacters(1),
                p.x - r, p.y - 9, r * 2, 18, 13, Colours::ink, true);
    }

    if (n > 0) {
        auto p = pointFor(head);
        g.setColour(Colours::green);
        g.drawLine(p.x - 8, p.y - 26, p.x - 16, p.y - 46, 3.0f);
        g.drawLine(p.x + 8, p.y - 26, p.x + 16, p.y - 46, 3.0f);
        g.setColour(Colours::pink);
        fillEllipseCentred(g, p.x - 16, p.y - 48, 5, 5);
        fillEllipseCentred(g, p.x + 16, p.y - 48, 5, 5);
        g.setColour(Colours::green);
        fillEllipseCentred(g, p.x, p.y, 28, 28);
        g.setColour(Colours::cream);
        g.drawEllipse(p.x - 28, p.y - 28, 56, 56, 4.0f);
        drawFace(g, p.x, p.y, 0.9f, playing >= 0 ? Mood::sing : Mood::smile,
                 time);
    }

    // the bubble in the middle
    juce::String big, small;
    if (vm.isRecording()) {
        big = "REC";
        small = juce::String(n) + " so far: play notes, − for a rest";
    } else if (n == 0) {
        big = "+";
        small = "press + and play some notes";
    } else if (playing >= 0) {
        int note = notes[playing];
        big = note < 0 ? juce::String("rest")
                       : noteName(note + vm.getTranspose());
        small = vm.getTranspose() != 0
                    ? "transposed " +
                          juce::String(vm.getTranspose() > 0 ? "+" : "") +
                          juce::String(vm.getTranspose())
                    : juce::String("press a key to transpose");
    } else {
        big = juce::String(n);
        small = "notes in the loop: press play";
    }
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(cx, cy));
        juce::Path cloud;
        cloud.startNewSubPath(-110, 30);
        cloud.quadraticTo(-130, -14, -84, -26);
        cloud.quadraticTo(-74, -72, -18, -64);
        cloud.quadraticTo(20, -92, 64, -62);
        cloud.quadraticTo(118, -70, 120, -18);
        cloud.quadraticTo(150, -4, 128, 34);
        cloud.closeSubPath();
        g.setColour(vm.isRecording() ? Colours::red.withAlpha(0.35f)
                                     : Colours::panel);
        g.fillPath(cloud);
        drawText(g, big, -120, -44, 240, 56, 44, Colours::text, true);
        drawText(g, small, -130, 12, 260, 18, 12, Colours::muted);
    }

    // info card
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(626.0f, 34.0f, 160.0f, 128.0f, 22.0f);
    auto dot = vm.isRecording() ? Colours::red
               : vm.isPlaying() ? Colours::green
                                : Colours::muted;
    g.setColour(dot);
    fillEllipseCentred(g, 650.0f, 60.0f, 8.0f, 8.0f);
    drawText(g, getSubtitle().toUpperCase(), 666, 50, 120, 20, 14,
             Colours::text, true, juce::Justification::centredLeft);
    int rests = 0;
    for (auto note : notes)
        rests += note < 0 ? 1 : 0;
    drawText(g,
             juce::String(n - rests) + " notes" + utf8(" · ") +
                 juce::String(rests) + (rests == 1 ? " rest" : " rests"),
             644, 84, 140, 18, 13, Colours::muted, false,
             juce::Justification::centredLeft);
    drawText(g, "rate " + vm.rateNames()[vm.getRateIndex()], 644, 108, 140, 18,
             13, Colours::muted, false, juce::Justification::centredLeft);
    drawText(g, "gate " + percent(vm.getGate()), 644, 132, 140, 18, 13,
             Colours::muted, false, juce::Justification::centredLeft);
}

//==============================================================================
// Tombola
TombolaSequencerView::TombolaSequencerView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void TombolaSequencerView::noteOnPressed(int noteNumber) {
    vm.addBall(noteNumber);
    repaint();
}

void TombolaSequencerView::minusButtonReleased() {
    if (hasFocus()) {
        vm.popBall();
        repaint();
    }
}

void TombolaSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void TombolaSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

juce::String TombolaSequencerView::getSubtitle() {
    return juce::String(vm.getBalls().size()) + " of " +
           juce::String(vm.maxBalls) + " balls";
}

juce::String TombolaSequencerView::getPageLabel() {
    return utf8("keys: drop a ball  ·  − pop one");
}

std::array<KnobInfo, 4> TombolaSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    double spin = vm.getSpin();
    k[0] = {"SPIN",
            std::abs(spin) < 0.001
                ? juce::String("still")
                : percent(std::abs(spin)) + (spin > 0 ? " right" : " left"),
            float((spin + 1.0) / 2.0), Colours::blue};
    k[1] = {"GRAVITY", percent(vm.getGravity()), float(vm.getGravity()),
            Colours::green};
    k[2] = {"SIDES", juce::String(vm.getSides()), (vm.getSides() - 3) / 5.0f,
            Colours::cream};
    k[3] = {utf8("BOUNCE · SHAKE"), percent(vm.getBounce()),
            float(vm.getBounce()), Colours::red};
    return k;
}

juce::String TombolaSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (auto b : vm.getBalls())
        s << "," << b;
    return s;
}

void TombolaSequencerView::paintStage(juce::Graphics &g, double time) {
    const float cx = 292.0f, cy = 168.0f, radius = 140.0f;
    const auto &sim = vm.getSimulation();
    // follow the song while playing; otherwise show the same throw anyway
    double beat =
        vm.isPlaying()
            ? vm.getPositionBeats()
            : (animationsEnabled() ? std::fmod(time * 2.0, vm.getLengthBeats())
                                   : 0.0);
    auto frame = sim.frameAt(beat);

    auto shape = polygon(cx, cy, radius, vm.getSides(), float(frame.angle));
    g.setColour(Colours::panel);
    g.fillPath(shape);
    g.setColour(Colours::blue);
    g.strokePath(shape, juce::PathStrokeType(9.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    auto inner =
        polygon(cx, cy, radius * 0.88f, vm.getSides(), float(frame.angle));
    {
        // little rivets on the corners
        juce::Path::Iterator it(inner);
        g.setColour(Colours::cream);
        while (it.next())
            if (it.elementType == juce::Path::Iterator::startNewSubPath ||
                it.elementType == juce::Path::Iterator::lineTo)
                fillEllipseCentred(g, it.x1, it.y1, 4.0f, 4.0f);
    }

    // which way it spins
    {
        juce::Path arc;
        bool clockwise = vm.getSpin() >= 0.0;
        arc.addCentredArc(cx, cy, radius + 22.0f, radius + 22.0f, 0.0f,
                          juce::degreesToRadians(-45.0f),
                          juce::degreesToRadians(45.0f), true);
        drawDashedPath(g, arc, Colours::blue.withAlpha(0.7f), 4.0f, 2.0f,
                       10.0f);
        float endAngle = juce::degreesToRadians(clockwise ? 45.0f : -45.0f) -
                         juce::MathConstants<float>::halfPi;
        float ax = cx + (radius + 22.0f) * std::cos(endAngle),
              ay = cy + (radius + 22.0f) * std::sin(endAngle);
        juce::Path head;
        float d = clockwise ? 1.0f : -1.0f;
        head.addTriangle(ax + 8 * d, ay + 6, ax - 6 * d, ay - 8, ax - 8 * d,
                         ay + 10);
        g.setColour(Colours::blue);
        g.fillPath(head);
    }

    const auto &balls = vm.getBalls();
    for (size_t i = 0; i < frame.balls.size() && int(i) < balls.size(); i++) {
        auto p = frame.balls[i];
        drawBadge(g, cx + p.x * radius, cy + p.y * radius, 14.0f,
                  noteColour(balls[int(i)]), time);
    }
    if (balls.isEmpty())
        drawText(g, "press keys to drop\nballs in", cx - 100, cy - 30, 200, 60,
                 16, Colours::muted, true);

    // the balls and how often each one plays
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(560.0f, 24.0f, 220.0f, 284.0f, 26.0f);
    drawText(g, "BALLS" + utf8(" · ") + juce::String(balls.size()) + " of 8",
             580, 38, 180, 16, 12, Colours::muted, false,
             juce::Justification::centredLeft);
    const auto &hits = sim.getHitCounts();
    for (int i = 0; i < balls.size(); i++) {
        float y = 62.0f + i * 30.0f;
        g.setColour(Colours::background);
        g.fillRoundedRectangle(580.0f, y, 180.0f, 26.0f, 12.0f);
        g.setColour(noteColour(balls[i]));
        fillEllipseCentred(g, 598.0f, y + 13.0f, 8.0f, 8.0f);
        drawText(g, noteName(balls[i]), 614, y, 80, 26, 14, Colours::text, true,
                 juce::Justification::centredLeft);
        int count = i < int(hits.size()) ? hits[size_t(i)] : 0;
        drawText(g, juce::String(count) + " hits", 680, y, 72, 26, 11,
                 Colours::muted, false, juce::Justification::centredRight);
    }
}

//==============================================================================
// Pattern
PatternSequencerView::PatternSequencerView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void PatternSequencerView::encoder1Increased() {
    if (control())
        vm.nudgeStepsPerBar(1);
    else
        vm.nudgeEditPattern(1);
    repaint();
}

void PatternSequencerView::encoder1Decreased() {
    if (control())
        vm.nudgeStepsPerBar(-1);
    else
        vm.nudgeEditPattern(-1);
    repaint();
}

void PatternSequencerView::encoder3Increased() {
    if (control())
        vm.nudgeLinkPattern(1);
    else
        vm.moveLink(1);
    repaint();
}

void PatternSequencerView::encoder3Decreased() {
    if (control())
        vm.nudgeLinkPattern(-1);
    else
        vm.moveLink(-1);
    repaint();
}

void PatternSequencerView::encoder3ButtonReleased() {
    if (control())
        vm.removeLink();
    else
        vm.addLink();
    repaint();
}

void PatternSequencerView::noteOnPressed(int noteNumber) {
    if (midiCommandManager.isPlusDown) {
        vm.toggleNote(noteNumber);
        repaint();
    }
}

void PatternSequencerView::minusButtonReleased() {
    if (hasFocus()) {
        vm.clearStep();
        repaint();
    }
}

void PatternSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void PatternSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

juce::String PatternSequencerView::getSubtitle() {
    static const char *letters = "ABCD";
    juce::String s =
        "editing " + juce::String::charToString(letters[vm.getEditPattern()]);
    int link = vm.getPlayingLink();
    if (link >= 0)
        s += utf8("  ·  playing ") +
             juce::String::charToString(letters[vm.getChain()[link]]);
    return s;
}

juce::String PatternSequencerView::getPageLabel() {
    return control() ? juce::String("ctrl")
                     : utf8("hold + and press a key: toggle  ·  ctrl: more");
}

std::array<KnobInfo, 4> PatternSequencerView::getKnobs() {
    static const char *letters = "ABCD";
    std::array<KnobInfo, 4> k;
    const auto &chain = vm.getChain();
    juce::String chainText;
    for (auto c : chain)
        chainText << juce::String::charToString(letters[c]);
    if (control()) {
        k[0] = {"STEPS PER BAR", juce::String(vm.getStepsPerBar()),
                vm.getStepsPerBar() / 16.0f, Colours::blue};
        k[1].enabled = false;
        k[2] = {
            utf8("LINK LETTER · REMOVE"),
            juce::String::charToString(letters[chain[vm.getSelectedLink()]]),
            chain[vm.getSelectedLink()] / 3.0f, Colours::cream};
        k[3].enabled = false;
        return k;
    }
    k[0] = {"PATTERN", juce::String::charToString(letters[vm.getEditPattern()]),
            vm.getEditPattern() / 3.0f, Colours::blue};
    k[1] = {"STEP", juce::String(vm.getSelectedStep() + 1),
            vm.getSelectedStep() / 15.0f, Colours::green};
    k[2] = {utf8("CHAIN · ADD"), chainText,
            chain.size() > 1 ? vm.getSelectedLink() / float(chain.size() - 1)
                             : 1.0f,
            Colours::cream};
    int octave = vm.getViewOctave();
    k[3] = {utf8("OCTAVE · COPY"), "C" + juce::String(octave - 1),
            octave / 8.0f, Colours::red};
    return k;
}

juce::String PatternSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature();
    for (int i = 0; i < Seq::patternCount; i++)
        s << "|" << vm.getPattern(i).toString();
    s << "|" << vm.getSelectedLink() << int(control());
    return s;
}

void PatternSequencerView::paintStage(juce::Graphics &g, double time) {
    static const char *letters = "ABCD";
    const juce::Colour carColours[] = {Colours::pink, Colours::green,
                                       Colours::blue, Colours::purple};
    const auto &chain = vm.getChain();
    int playingLink = vm.getPlayingLink();
    int playingPattern = playingLink >= 0 ? chain[playingLink] : -1;

    drawTrain(g, 72.0f, 96.0f, 0.9f, time);
    g.setColour(Colours::dim);
    g.drawLine(20, 128, 786, 128, 4.0f);

    for (int i = 0; i < Seq::patternCount; i++) {
        float x = 146.0f + i * 162.0f, y = 40.0f;
        if (i == playingPattern) {
            juce::Path ring;
            ring.addRoundedRectangle(x - 7, y - 7, 166, 90, 24);
            drawDashedPath(g, ring, Colours::yellow, 4.0f, 10.0f, 7.0f);
        }
        g.setColour(carColours[i]);
        g.fillRoundedRectangle(x, y, 152, 76, 18);
        if (i == vm.getEditPattern()) {
            g.setColour(Colours::cream);
            g.drawRoundedRectangle(x, y, 152, 76, 18, 4.0f);
        }
        g.setColour(Colours::ink);
        g.fillRoundedRectangle(x + 10, y + 10, 132, 56, 10);
        const auto &pattern = vm.getPattern(i);
        for (int s = 0; s < Seq::patternSteps; s++)
            for (int r = 0; r < 4; r++) {
                bool on = false;
                for (auto p : pattern.steps[size_t(s)])
                    on = on || ((p % 12) / 3) == r;
                g.setColour(on ? Colours::candy(r) : Colours::dim);
                fillEllipseCentred(g, x + 18.0f + s * 7.9f,
                                   y + 56.0f - r * 12.0f, 2.6f, 2.6f);
            }
        g.setColour(Colours::ink);
        fillEllipseCentred(g, x + 30, y + 84, 9, 9);
        fillEllipseCentred(g, x + 122, y + 84, 9, 9);
        g.setColour(Colours::cream);
        fillEllipseCentred(g, x + 76, y - 2, 13, 13);
        drawText(g, juce::String::charToString(letters[i]), x + 63, y - 12, 26,
                 20, 15, Colours::ink, true);
    }

    // the chain
    drawText(g, "CHAIN", 24, 146, 60, 18, 12, Colours::muted, false,
             juce::Justification::centredLeft);
    for (int i = 0; i < chain.size(); i++) {
        float x = 82.0f + i * 42.0f;
        auto fill = i == playingLink            ? Colours::green
                    : i == vm.getSelectedLink() ? Colours::yellow
                                                : Colours::muted;
        juce::Path ticket;
        ticket.startNewSubPath(x, 144);
        ticket.lineTo(x + 34, 144);
        ticket.lineTo(x + 34, 151);
        ticket.quadraticTo(x + 30, 155, x + 34, 159);
        ticket.lineTo(x + 34, 166);
        ticket.lineTo(x, 166);
        ticket.lineTo(x, 159);
        ticket.quadraticTo(x + 4, 155, x, 151);
        ticket.closeSubPath();
        g.setColour(fill);
        g.fillPath(ticket);
        drawText(g, juce::String::charToString(letters[chain[i]]), x, 144, 34,
                 22, 13, Colours::ink, true);
    }

    // the pattern being edited, one octave at a time
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    const float x0 = 52.0f, w = (786.0f - x0) / 16.0f, y0 = 176.0f,
                h = 144.0f / 12.0f;
    int low = vm.getViewOctave() * 12;
    const auto &pattern = vm.getPattern(vm.getEditPattern());
    for (int r = 0; r < 12; r++) {
        float y = y0 + (11 - r) * h;
        bool sharp = juce::String(names[r]).contains("#");
        if (!sharp)
            drawText(g,
                     juce::String(names[r]) +
                         (r == 0 ? juce::String(vm.getViewOctave() - 1) : ""),
                     4, y, 42, h, 9, Colours::muted, r == 0,
                     juce::Justification::centredRight);
        for (int s = 0; s < Seq::patternSteps; s++) {
            juce::Rectangle<float> cell(x0 + s * w + 2, y + 1, w - 4, h - 2);
            if (pattern.steps[size_t(s)].contains(low + r)) {
                g.setColour(noteColour(low + r));
                g.fillRoundedRectangle(cell, h / 2.0f);
            } else {
                auto base = (s / 4) % 2 ? Colours::dim : Colours::panel;
                g.setColour(sharp ? base.darker(0.25f) : base);
                g.fillRoundedRectangle(cell, 4.0f);
            }
        }
    }
    // notes above or below this octave
    for (int s = 0; s < Seq::patternSteps; s++) {
        bool above = false, below = false;
        for (auto p : pattern.steps[size_t(s)]) {
            above = above || p >= low + 12;
            below = below || p < low;
        }
        float cx = x0 + s * w + w / 2.0f;
        g.setColour(Colours::cream);
        if (above) {
            juce::Path t;
            t.addTriangle(cx - 5, y0 - 2, cx + 5, y0 - 2, cx, y0 - 8);
            g.fillPath(t);
        }
        if (below) {
            juce::Path t;
            t.addTriangle(cx - 5, 322, cx + 5, 322, cx, 328);
            g.fillPath(t);
        }
    }

    float sx = x0 + vm.getSelectedStep() * w;
    g.setColour(Colours::cream);
    g.drawRoundedRectangle(sx, y0 - 2, w, 148, 8, 3.0f);
    if (playingPattern == vm.getEditPattern()) {
        float px = x0 + vm.getPlayingStep() * w;
        g.setColour(Colours::yellow.withAlpha(0.18f));
        g.fillRoundedRectangle(px, y0 - 2, w, 148, 8);
    }
}
