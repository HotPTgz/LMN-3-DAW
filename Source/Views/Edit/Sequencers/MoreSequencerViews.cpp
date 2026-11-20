#include "MoreSequencerViews.h"
#include "PlayfulApp.h"

using namespace Playful;
namespace Seq = app_view_models::Sequencers;

namespace {
juce::String moreNoteName(int midi) {
    return juce::MidiMessage::getMidiNoteName(midi, true, true, 4);
}
juce::String morePercent(double v) {
    return juce::String(juce::roundToInt(v * 100.0)) + "%";
}
const juce::Colour &ringColour(int row) { return Colours::candy(row % 6); }
} // namespace

//==============================================================================
// Euclidean
EuclideanSequencerView::EuclideanSequencerView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void EuclideanSequencerView::turn(int encoder, int direction) {
    if (control()) {
        if (encoder == 0)
            vm.nudgeVelocity(direction);
        else if (encoder == 1)
            vm.nudgeSwing(direction);
    } else {
        switch (encoder) {
        case 0:
            vm.nudgeSound(direction);
            break;
        case 1:
            vm.nudgeHits(direction);
            break;
        case 2:
            vm.nudgeSteps(direction);
            break;
        default:
            vm.nudgeRotate(direction);
        }
    }
    repaint();
}

void EuclideanSequencerView::encoder1ButtonReleased() {
    if (hasFocus()) {
        vm.nextSound();
        repaint();
    }
}

void EuclideanSequencerView::noteOnPressed(int noteNumber) {
    vm.selectForNote(noteNumber);
    repaint();
}

void EuclideanSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void EuclideanSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

juce::String EuclideanSequencerView::getSubtitle() {
    return juce::String(vm.getLengthSteps()) + " steps";
}

juce::String EuclideanSequencerView::getPageLabel() {
    return control() ? juce::String("ctrl")
                     : utf8("keys or press 1: pick a sound  ·  ctrl: more");
}

std::array<KnobInfo, 4> EuclideanSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    if (control()) {
        k[0] = {"VELOCITY", juce::String(vm.getVelocity()),
                vm.getVelocity() / 127.0f, Colours::blue};
        k[1] = {"SWING", morePercent(vm.getSwing()), float(vm.getSwing()),
                Colours::green};
        k[2] = {"", "", 0.0f, Colours::cream, false};
        k[3] = {"", "", 0.0f, Colours::red, false};
        return k;
    }
    int row = vm.getSelected();
    auto &ring = vm.getRing(row);
    k[0] = {utf8("SOUND · NEXT"), vm.getRowName(row), row / 23.0f, Colours::blue};
    k[1] = {"HITS", ring.hits == 0 ? juce::String("off") : juce::String(ring.hits),
            ring.steps > 0 ? ring.hits / float(ring.steps) : 0.0f, Colours::green};
    k[2] = {"STEPS", juce::String(ring.steps), ring.steps / 32.0f,
            Colours::cream};
    k[3] = {"ROTATE", juce::String(ring.rotate),
            ring.steps > 1 ? ring.rotate / float(ring.steps - 1) : 0.0f,
            Colours::red};
    return k;
}

juce::String EuclideanSequencerView::getChangeSignature() {
    juce::String s = PlayfulScreen::getChangeSignature() + "|" +
                     juce::String(vm.getSelected()) + juce::String(int(control()));
    for (int row = 0; row < Seq::drumRows; row++) {
        auto &r = vm.getRing(row);
        s << "," << r.hits << ":" << r.steps << ":" << r.rotate;
    }
    return s;
}

void EuclideanSequencerView::paintStage(juce::Graphics &g, double time) {
    const float cx = 250.0f, cy = 166.0f;
    // which rings to draw: the ones playing, and the one being edited
    juce::Array<int> rows;
    int selected = vm.getSelected();
    for (int row = 0; row < Seq::drumRows; row++)
        if (vm.getRing(row).hits > 0 || row == selected)
            rows.add(row);
    while (rows.size() > 4) { // keep the selected one, drop from the end
        int drop = rows.getLast() == selected ? rows.size() - 2 : rows.size() - 1;
        rows.remove(drop);
    }
    int step = vm.getPlayingStep();
    for (int i = 0; i < rows.size(); i++) {
        int row = rows[i];
        auto &ring = vm.getRing(row);
        float radius = 140.0f - i * 26.0f;
        juce::Path circle;
        circle.addEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
        drawDashedPath(g, circle,
                       row == selected ? Colours::muted : Colours::dim, 2.0f,
                       3.0f, 6.0f);
        auto pattern = Seq::euclid(ring.hits, ring.steps, ring.rotate);
        for (int s = 0; s < pattern.size(); s++) {
            float a = -juce::MathConstants<float>::halfPi +
                      juce::MathConstants<float>::twoPi * s / pattern.size();
            float x = cx + radius * std::cos(a), y = cy + radius * std::sin(a);
            bool now = step >= 0 && step % pattern.size() == s;
            float r = pattern[s] ? (now ? 10.0f : 7.0f) : 3.5f;
            g.setColour(pattern[s] ? ringColour(row) : Colours::greyKey);
            fillEllipseCentred(g, x, y, r, r);
            if (now && pattern[s]) {
                g.setColour(Colours::cream);
                g.drawEllipse(x - 12, y - 12, 24, 24, 2.0f);
            }
        }
    }
    // the hand sweeps round with the steps (a 16-step turn)
    float turn = step >= 0 ? (step % 16) / 16.0f : float(loop(time, 8.0));
    float a = -juce::MathConstants<float>::halfPi +
              juce::MathConstants<float>::twoPi * turn;
    g.setColour(Colours::cream);
    g.drawLine(cx, cy, cx + 116 * std::cos(a), cy + 116 * std::sin(a), 3.0f);
    g.setColour(Colours::blue);
    fillEllipseCentred(g, cx, cy, 26, 26);
    drawFace(g, cx, cy, 0.9f, step >= 0 ? Mood::sing : Mood::smile, time);

    // the ring list
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(470, 20, 300, 270, 20);
    drawText(g, utf8("RINGS · ONE PER KIT SOUND"), 486, 30, 270, 18, 12,
             Colours::muted, false, juce::Justification::left);
    int y = 56;
    juce::Array<int> listed;
    for (int row = 0; row < Seq::drumRows && listed.size() < 7; row++)
        if (vm.getRing(row).hits > 0 || row == selected)
            listed.add(row);
    for (int row : listed) {
        auto &ring = vm.getRing(row);
        if (row == selected) {
            g.setColour(Colours::dim);
            g.fillRoundedRectangle(480, float(y), 280, 30, 10);
        }
        g.setColour(ringColour(row));
        fillEllipseCentred(g, 496, y + 15.0f, 6, 6);
        drawText(g, vm.getRowName(row), 510, float(y), 130, 30, 13,
                 Colours::text, true, juce::Justification::left);
        drawText(g,
                 ring.hits > 0
                     ? juce::String(ring.hits) + " of " + juce::String(ring.steps) +
                           utf8(" · rot ") + juce::String(ring.rotate)
                     : juce::String("off"),
                 620, float(y), 132, 30, 12, Colours::muted, false,
                 juce::Justification::right);
        y += 34;
    }
}

//==============================================================================
// Turing
TuringSequencerView::TuringSequencerView(tracktion::AudioTrack::Ptr t,
                                         app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void TuringSequencerView::turn(int encoder, int direction) {
    if (control()) {
        if (encoder == 0)
            vm.nudgeRate(direction);
        else if (encoder == 1)
            vm.nudgeGate(direction);
    } else {
        switch (encoder) {
        case 0:
            vm.nudgeLock(direction);
            break;
        case 1:
            vm.nudgeLength(direction);
            break;
        case 2:
            vm.nudgeRange(direction);
            break;
        default:
            vm.nudgeScale(direction);
        }
    }
    repaint();
}

void TuringSequencerView::encoder1ButtonReleased() {
    if (hasFocus()) {
        vm.reroll();
        repaint();
    }
}

void TuringSequencerView::noteOnPressed(int noteNumber) {
    vm.notePressed(noteNumber);
    repaint();
}

void TuringSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void TuringSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

static juce::String scaleText(const Seq::TuringInput &in) {
    auto root = moreNoteName(in.root).trimCharactersAtEnd("-0123456789");
    return root + " " +
           internal_plugins::midifx::Scales::getNames()[in.scale].toLowerCase();
}

juce::String TuringSequencerView::getSubtitle() {
    auto &in = vm.getInput();
    return juce::String(in.length) + utf8(" steps · scale lock: ") +
           scaleText(in);
}

juce::String TuringSequencerView::getPageLabel() {
    return control() ? juce::String("ctrl")
                     : utf8("a key: lowest note  ·  press 1: new melody  ·  "
                            "ctrl: more");
}

std::array<KnobInfo, 4> TuringSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    auto &in = vm.getInput();
    if (control()) {
        k[0] = {"RATE", vm.rateNames()[vm.getRateIndex()],
                vm.getRateIndex() / 2.0f, Colours::blue};
        k[1] = {"GATE", morePercent(in.gate), float(in.gate), Colours::green};
        k[2] = {"", "", 0.0f, Colours::cream, false};
        k[3] = {"", "", 0.0f, Colours::red, false};
        return k;
    }
    k[0] = {utf8("LOCK · NEW"), morePercent(in.lock), float(in.lock),
            Colours::blue};
    k[1] = {"LENGTH", juce::String(in.length), in.length / 16.0f, Colours::green};
    k[2] = {"RANGE", juce::String(in.range) + " oct", in.range / 4.0f,
            Colours::cream};
    int scales = internal_plugins::midifx::Scales::getNames().size();
    k[3] = {"SCALE", scaleText(in), in.scale / float(juce::jmax(1, scales - 1)),
            Colours::red};
    return k;
}

juce::String TuringSequencerView::getChangeSignature() {
    auto &in = vm.getInput();
    return PlayfulScreen::getChangeSignature() + "|" + juce::String(in.lock) +
           "," + juce::String(in.length) + "," + juce::String(in.range) + "," +
           juce::String(in.scale) + "," + juce::String(in.root) + "," +
           juce::String(in.seed) + juce::String(int(control()));
}

void TuringSequencerView::paintStage(juce::Graphics &g, double time) {
    auto &in = vm.getInput();
    auto &notes = vm.getNotes();
    const float cx = 240.0f, cy = 160.0f, r = 110.0f;
    int step = vm.getPlayingStep();
    int length = juce::jmax(1, in.length);
    // the current pass of the loop
    int base = step >= 0 ? (step / length) * length : 0;
    int lowest = in.root, highest = in.root + in.range * 12;
    for (int i = 0; i < length; i++) {
        float a = -juce::MathConstants<float>::halfPi +
                  juce::MathConstants<float>::twoPi * i / length;
        float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
        size_t index = size_t(base + i);
        int note = index < notes.size() ? notes[index] : in.root;
        float height = juce::jlimit(0.0f, 1.0f, float(note - lowest) /
                                                    juce::jmax(1, highest - lowest));
        bool now = step >= 0 && step % length == i;
        float size = 10 + 8 * height;
        g.setColour(Colours::purple.withAlpha(0.35f + 0.65f * height));
        g.fillRoundedRectangle(x - size, y - size, size * 2, size * 2, 8);
        if (now || (step < 0 && i == 0)) {
            g.setColour(Colours::cream);
            g.drawRoundedRectangle(x - size - 3, y - size - 3, size * 2 + 6,
                                   size * 2 + 6, 10, 3);
        }
    }
    g.setColour(Colours::panel);
    fillEllipseCentred(g, cx, cy, 62, 62);
    size_t nowIndex = size_t(step >= 0 ? step : 0);
    int playing = nowIndex < notes.size() ? notes[nowIndex] : in.root;
    drawText(g, moreNoteName(playing), cx - 60, cy - 24, 120, 40, 34,
             Colours::text, true);
    drawText(g, step >= 0 ? "playing now" : "first note", cx - 60, cy + 16, 120,
             20, 12, Colours::muted);

    // the robot, with its lock gauge
    const float rx = 600, ry = 120;
    float bob = 2.0f * std::sin(float(time) * 2.0f);
    g.setColour(Colours::green);
    g.fillRoundedRectangle(rx - 50, ry - 40 + bob, 100, 84, 22);
    g.drawLine(rx, ry - 40 + bob, rx, ry - 62 + bob, 4.0f);
    g.setColour(Colours::yellow);
    fillEllipseCentred(g, rx, ry - 66 + bob, 7, 7);
    drawFace(g, rx, ry + bob, 1.1f, step >= 0 ? Mood::sing : Mood::smile, time);
    g.setColour(Colours::dim);
    g.fillRoundedRectangle(rx - 60, ry + 62, 120, 12, 6);
    g.setColour(Colours::yellow);
    g.fillRoundedRectangle(rx - 60, ry + 62, float(120 * in.lock), 12, 6);
    juce::String how = in.lock >= 0.999  ? "never changes"
                       : in.lock >= 0.8  ? "changes rarely"
                       : in.lock >= 0.5  ? "changes sometimes"
                                         : "changes a lot";
    drawText(g, "lock " + morePercent(in.lock) + utf8(" · ") + how, rx - 120,
             ry + 84, 240, 22, 13, Colours::muted);
}

//==============================================================================
// Chords
ChordsSequencerView::ChordsSequencerView(tracktion::AudioTrack::Ptr t,
                                         app_services::MidiCommandManager &mcm)
    : SequencerScreen(mcm), vm(t) {}

void ChordsSequencerView::turn(int encoder, int direction) {
    if (control()) {
        if (encoder == 0)
            vm.nudgePreset(direction);
        else if (encoder == 1)
            vm.nudgeBars(direction);
    } else {
        switch (encoder) {
        case 0:
            vm.nudgeKey(direction);
            break;
        case 1:
            vm.nudgeBar(direction);
            break;
        case 2:
            vm.nudgeChord(direction);
            break;
        default:
            vm.nudgeFollow(direction);
        }
    }
    repaint();
}

void ChordsSequencerView::noteOnPressed(int noteNumber) {
    vm.notePressed(noteNumber);
    repaint();
}

void ChordsSequencerView::playButtonReleased() {
    if (hasFocus())
        vm.play();
}

void ChordsSequencerView::stopButtonReleased() {
    if (hasFocus())
        vm.stop();
}

juce::String ChordsSequencerView::getSubtitle() {
    int bars = vm.getInput().degrees.size();
    int bar = vm.getPlayingBar() >= 0 ? vm.getPlayingBar() : vm.getSelectedBar();
    return "bar " + juce::String(bar + 1) + " of " + juce::String(bars) +
           utf8(" · followers: ") + juce::String(vm.getFollowers());
}

juce::String ChordsSequencerView::getPageLabel() {
    return control() ? juce::String("ctrl")
                     : utf8("a key: the key's root  ·  ctrl: more");
}

std::array<KnobInfo, 4> ChordsSequencerView::getKnobs() {
    std::array<KnobInfo, 4> k;
    auto &in = vm.getInput();
    namespace C = Seq::Chords;
    if (control()) {
        auto &presets = C::getPresetNames();
        k[0] = {"PRESET", presets[vm.getPreset()],
                vm.getPreset() / float(presets.size() - 1), Colours::blue};
        k[1] = {"BARS", juce::String(in.degrees.size()),
                in.degrees.size() / 8.0f, Colours::green};
        k[2] = {"", "", 0.0f, Colours::cream, false};
        k[3] = {"", "", 0.0f, Colours::red, false};
        return k;
    }
    int bar = vm.getSelectedBar();
    int degree = in.degrees[bar];
    k[0] = {"KEY", C::getKeyNames()[in.key], in.key / 23.0f, Colours::blue};
    k[1] = {"BAR", juce::String(bar + 1),
            in.degrees.size() > 1 ? bar / float(in.degrees.size() - 1) : 0.0f,
            Colours::green};
    k[2] = {"CHORD", C::romanName(in.key, degree) + utf8(" · ") +
                         C::chordName(in.key, degree),
            degree / 6.0f, Colours::cream};
    int max = vm.getMaxFollowers();
    k[3] = {"FOLLOW",
            vm.getFollowers() == 0
                ? juce::String(max == 0 ? "no tracks after" : "off")
                : juce::String(vm.getFollowers()) +
                      (vm.getFollowers() == 1 ? " track" : " tracks"),
            vm.getFollowers() / 3.0f, Colours::red};
    return k;
}

juce::String ChordsSequencerView::getChangeSignature() {
    auto &in = vm.getInput();
    juce::String s = PlayfulScreen::getChangeSignature() + "|" +
                     juce::String(in.key) + "," +
                     juce::String(vm.getSelectedBar()) + "," +
                     juce::String(vm.getFollowers()) + "," +
                     juce::String(vm.getPreset()) + juce::String(int(control()));
    for (auto d : in.degrees)
        s << d;
    return s;
}

void ChordsSequencerView::paintStage(juce::Graphics &g, double time) {
    namespace C = Seq::Chords;
    auto &in = vm.getInput();
    int bars = in.degrees.size();
    int playing = vm.getPlayingBar();
    // up to 4 cards at once, scrolled to the bar being edited or played
    int focus = playing >= 0 ? playing : vm.getSelectedBar();
    int first = juce::jlimit(0, juce::jmax(0, bars - 4), focus - 1);
    int shown = juce::jmin(4, bars);
    float cardW = 136, gap = 150;
    for (int i = 0; i < shown; i++) {
        int bar = first + i;
        int degree = in.degrees[bar];
        float x = 24 + i * gap, y = 34;
        bool current = playing >= 0 ? bar == playing : bar == vm.getSelectedBar();
        g.setColour(current ? Colours::dim : Colours::panel);
        g.fillRoundedRectangle(x, y, cardW, 150, 20);
        if (current) {
            g.setColour(Colours::yellow);
            g.drawRoundedRectangle(x + 1.5f, y + 1.5f, cardW - 3, 147, 20, 3);
        }
        drawText(g, "BAR " + juce::String(bar + 1), x, y + 16, cardW, 16, 11,
                 Colours::muted);
        float bounce = current && playing >= 0 ? -4.0f * pingPong(time, 0.5) : 0.0f;
        drawText(g, C::chordName(in.key, degree), x, y + 38 + bounce, cardW, 48,
                 40, Colours::text, true);
        drawText(g, C::romanName(in.key, degree), x, y + 88, cardW, 22, 16,
                 current ? Colours::yellow : Colours::muted, true);
        juce::StringArray names;
        for (int n : C::triad(in.key, degree))
            names.add(moreNoteName(n).trimCharactersAtEnd("-0123456789"));
        drawText(g, names.joinIntoString(" "), x, y + 112, cardW, 18, 12,
                 Colours::muted);
    }
    // where the loop is
    g.setColour(Colours::dim);
    g.fillRoundedRectangle(24, 200, 586, 6, 3);
    double beat = vm.getPlayingBeat();
    if (beat >= 0) {
        g.setColour(Colours::yellow);
        g.fillRoundedRectangle(24, 200, float(586 * beat / (4.0 * bars)), 6, 3);
    }
    // the followers
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(24, 228, 586, 84, 20);
    drawText(g, "FOLLOWING THE PROGRESSION", 40, 238, 400, 16, 12,
             Colours::muted, false, juce::Justification::left);
    auto followers = vm.getFollowerNames();
    if (followers.isEmpty())
        drawText(g,
                 vm.getMaxFollowers() == 0
                     ? "add tracks after this one to have them follow along"
                     : "turn FOLLOW to have the next tracks play along",
                 40, 262, 560, 30, 13, Colours::muted, false,
                 juce::Justification::left);
    static const juce::Colour colours[] = {Colours::green, Colours::blue,
                                           Colours::pink};
    float fx = 40;
    for (int i = 0; i < followers.size(); i++) {
        float w = 18 + font(13).getStringWidthFloat(followers[i]) + 20;
        g.setColour(Colours::dim);
        g.fillRoundedRectangle(fx, 262, w, 32, 12);
        g.setColour(colours[i % 3]);
        fillEllipseCentred(g, fx + 16, 278, 5, 5);
        drawText(g, followers[i], fx + 26, 262, w - 26, 32, 13, Colours::text,
                 false, juce::Justification::left);
        fx += w + 10;
    }
    // the key and presets
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(630, 34, 146, 278, 20);
    drawText(g, "KEY", 644, 46, 120, 16, 12, Colours::muted, false,
             juce::Justification::left);
    drawText(g, C::getKeyNames()[in.key], 644, 64, 128, 28, 20, Colours::text,
             true, juce::Justification::left);
    drawText(g, "PRESETS", 644, 104, 120, 16, 11, Colours::muted, false,
             juce::Justification::left);
    auto &presets = C::getPresetNames();
    for (int i = 0; i < presets.size(); i++)
        drawText(g, presets[i], 644, 124 + i * 24.0f, 128, 20, 13,
                 i == vm.getPreset() ? Colours::text : Colours::muted,
                 i == vm.getPreset(), juce::Justification::left);
}
