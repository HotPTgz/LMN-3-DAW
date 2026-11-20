#include "TouchShell.h"
#include "PlayfulStyle.h"

using MCM = app_services::MidiCommandManager;

namespace {
// How far a finger moves (in the screen's 800x480 design space) for one
// encoder step
constexpr float dragStep = 9.0f;
constexpr float tapSlop = 6.0f;
// How long a finger rests on a knob box before it counts as holding the
// encoder down
constexpr juce::uint32 holdDelayMs = 300;

juce::Colour buttonColour() { return Playful::Colours::panel; }
} // namespace

//==============================================================================
// A grid of buttons
class TouchShell::Pad : public juce::Component {
  public:
    enum class Kind { cc, control, octaveDown, octaveUp, bigScreen };

    struct Spec {
        juce::String label;
        Kind kind = Kind::cc;
        int cc = 0;
        juce::Colour textColour = Playful::Colours::muted;
    };

    Pad(VirtualHardware &hw, std::function<void()> toggleBigScreen)
        : hardware(hw), onBigScreen(std::move(toggleBigScreen)) {}

    void setButtons(std::vector<Spec> newSpecs, int newColumns) {
        releaseAll();
        specs = std::move(newSpecs);
        columns = newColumns;
        pressCount.assign(specs.size(), 0);
        repaint();
    }

    bool bigScreenOn = false;

    void paint(juce::Graphics &g) override {
        for (size_t i = 0; i < specs.size(); i++) {
            auto r = cellBounds(int(i)).reduced(3.0f);
            bool lit =
                pressCount[i] > 0 ||
                (specs[i].kind == Kind::control && hardware.isControlDown()) ||
                (specs[i].kind == Kind::bigScreen && bigScreenOn);
            g.setColour(lit ? Playful::Colours::greyKey : buttonColour());
            g.fillRoundedRectangle(r, juce::jmin(10.0f, r.getHeight() * 0.25f));
            if (specs[i].kind == Kind::control && hardware.isControlDown()) {
                g.setColour(Playful::Colours::yellow);
                g.drawRoundedRectangle(
                    r, juce::jmin(10.0f, r.getHeight() * 0.25f), 2.0f);
            }
            g.setColour(specs[i].textColour);
            g.setFont(Playful::font(
                juce::jlimit(11.0f, 22.0f, r.getHeight() * 0.36f), true));
            g.drawFittedText(labelFor(specs[i]), r.toNearestInt(),
                             juce::Justification::centred, 2, 0.8f);
        }
    }

    void mouseDown(const juce::MouseEvent &e) override {
        int i = indexAt(e.position);
        if (i < 0)
            return;
        held[e.source.getIndex()] = i;
        press(i, true);
    }

    void mouseUp(const juce::MouseEvent &e) override {
        auto it = held.find(e.source.getIndex());
        if (it == held.end())
            return;
        int i = it->second;
        held.erase(it);
        press(i, false);
    }

    void releaseAll() {
        for (auto &h : held)
            press(h.second, false);
        held.clear();
    }

  private:
    juce::String labelFor(const Spec &s) const {
        if (s.kind == Kind::bigScreen)
            return bigScreenOn ? "Keys" : "Big screen";
        if (s.kind == Kind::octaveDown || s.kind == Kind::octaveUp) {
            auto o = hardware.getOctave();
            return s.label + "\n" + (o > 0 ? "+" : "") + juce::String(o);
        }
        return s.label;
    }

    juce::Rectangle<float> cellBounds(int i) const {
        int rows = (int(specs.size()) + columns - 1) / columns;
        float w = getWidth() / float(columns), h = getHeight() / float(rows);
        return {float(i % columns) * w, float(i / columns) * h, w, h};
    }

    int indexAt(juce::Point<float> p) const {
        for (size_t i = 0; i < specs.size(); i++)
            if (cellBounds(int(i)).contains(p))
                return int(i);
        return -1;
    }

    void press(int i, bool down) {
        if (i < 0 || i >= int(specs.size()))
            return;
        pressCount[size_t(i)] =
            juce::jmax(0, pressCount[size_t(i)] + (down ? 1 : -1));
        auto &s = specs[size_t(i)];
        switch (s.kind) {
        case Kind::cc:
            // only the first finger down and the last one up count
            if ((down && pressCount[size_t(i)] == 1) ||
                (!down && pressCount[size_t(i)] == 0))
                hardware.button(s.cc, down);
            break;
        case Kind::control:
            if (down)
                hardware.setControl(!hardware.isControlDown());
            break;
        case Kind::octaveDown:
            if (down)
                hardware.changeOctave(-1);
            break;
        case Kind::octaveUp:
            if (down)
                hardware.changeOctave(1);
            break;
        case Kind::bigScreen:
            if (down && onBigScreen)
                juce::MessageManager::callAsync(onBigScreen);
            break;
        }
        repaint();
    }

    VirtualHardware &hardware;
    std::function<void()> onBigScreen;
    std::vector<Spec> specs;
    std::vector<int> pressCount;
    int columns = 1;
    std::map<int, int> held; // touch index -> button
};

//==============================================================================
// Two octaves of keys starting on F, like the LMN-3
class TouchShell::Keyboard : public juce::Component {
  public:
    explicit Keyboard(VirtualHardware &hw) : hardware(hw) {}

    ~Keyboard() override { releaseAll(); }

    void paint(juce::Graphics &g) override {
        using namespace Playful;
        for (int w = 0; w < 14; w++) {
            auto r = whiteBounds(w).reduced(1.5f, 0.0f);
            int key = whiteKeys[w % 7] + 12 * (w / 7);
            g.setColour(downCount[size_t(key)] > 0 ? Colours::encoder(key % 4)
                                                   : Colours::text);
            g.fillRoundedRectangle(r.withTrimmedTop(-8.0f), 7.0f);
            if (key % 12 == 7) { // C
                int octaveNumber = 4 + key / 12 + hardware.getOctave();
                g.setColour(Colours::dim);
                g.setFont(font(juce::jlimit(11.0f, 18.0f, r.getWidth() * 0.32f),
                               true));
                g.drawText("C" + juce::String(octaveNumber),
                           r.removeFromBottom(r.getHeight() * 0.22f),
                           juce::Justification::centred);
            }
        }
        for (int b = 0; b < 10; b++) {
            auto r = blackBounds(b);
            int key = blackKeys[b % 5] + 12 * (b / 5);
            g.setColour(downCount[size_t(key)] > 0 ? Colours::encoder(key % 4)
                                                   : Colours::ink);
            g.fillRoundedRectangle(r.withTrimmedTop(-8.0f), 6.0f);
            g.setColour(Colours::dim);
            g.drawRoundedRectangle(r.withTrimmedTop(-8.0f), 6.0f, 1.0f);
        }
    }

    void mouseDown(const juce::MouseEvent &e) override {
        int key = keyAt(e.position);
        if (key >= 0)
            start(e.source.getIndex(), key);
    }

    void mouseDrag(const juce::MouseEvent &e) override {
        // sliding a finger along the keys plays each key it reaches
        int key = keyAt(e.position);
        auto it = playing.find(e.source.getIndex());
        int current = it == playing.end() ? -1 : it->second.key;
        if (key == current)
            return;
        stop(e.source.getIndex());
        if (key >= 0)
            start(e.source.getIndex(), key);
    }

    void mouseUp(const juce::MouseEvent &e) override {
        stop(e.source.getIndex());
    }

    void releaseAll() {
        while (!playing.empty())
            stop(playing.begin()->first);
    }

  private:
    struct Playing {
        int key, note;
    };

    void start(int touch, int key) {
        playing[touch] = {key, hardware.noteOn(key)};
        downCount[size_t(key)]++;
        repaint();
    }

    void stop(int touch) {
        auto it = playing.find(touch);
        if (it == playing.end())
            return;
        hardware.noteOff(it->second.note);
        downCount[size_t(it->second.key)]--;
        playing.erase(it);
        repaint();
    }

    juce::Rectangle<float> whiteBounds(int w) const {
        float kw = getWidth() / 14.0f;
        return {w * kw, 0.0f, kw, float(getHeight())};
    }

    juce::Rectangle<float> blackBounds(int b) const {
        // black keys sit on the line after these white keys: F G A, C D
        static const int after[] = {0, 1, 2, 4, 5};
        float kw = getWidth() / 14.0f;
        float x = (after[b % 5] + 7 * (b / 5) + 1) * kw;
        return {x - kw * 0.32f, 0.0f, kw * 0.64f, getHeight() * 0.6f};
    }

    int keyAt(juce::Point<float> p) const {
        for (int b = 0; b < 10; b++)
            if (blackBounds(b).contains(p))
                return blackKeys[b % 5] + 12 * (b / 5);
        for (int w = 0; w < 14; w++)
            if (whiteBounds(w).contains(p))
                return whiteKeys[w % 7] + 12 * (w / 7);
        return -1;
    }

    // key numbers (0 = F) of the white and black keys in one octave
    static constexpr int whiteKeys[7] = {0, 2, 4, 6, 7, 9, 11};
    static constexpr int blackKeys[5] = {1, 3, 5, 8, 10};

    VirtualHardware &hardware;
    std::map<int, Playing> playing; // touch index -> key
    std::array<int, 24> downCount{};
};

//==============================================================================
struct TouchShell::ScreenListener : juce::MouseListener {
    explicit ScreenListener(TouchShell &s) : shell(s) {}
    void mouseDown(const juce::MouseEvent &e) override { shell.screenDown(e); }
    void mouseDrag(const juce::MouseEvent &e) override { shell.screenDrag(e); }
    void mouseUp(const juce::MouseEvent &e) override { shell.screenUp(e); }
    TouchShell &shell;
};

TouchShell::TouchShell(std::unique_ptr<juce::Component> s, VirtualHardware &hw)
    : screen(std::move(s)), hardware(hw) {
    auto toggle = [this] { setBigScreen(!bigScreen); };
    leftPad = std::make_unique<Pad>(hardware, toggle);
    rightPad = std::make_unique<Pad>(hardware, toggle);
    bottomLeft = std::make_unique<Pad>(hardware, toggle);
    bottomRight = std::make_unique<Pad>(hardware, toggle);
    keyboard = std::make_unique<Keyboard>(hardware);

    addAndMakeVisible(*screen);
    for (auto *c :
         {(juce::Component *)leftPad.get(), (juce::Component *)rightPad.get(),
          (juce::Component *)bottomLeft.get(),
          (juce::Component *)bottomRight.get(),
          (juce::Component *)keyboard.get()})
        addAndMakeVisible(c);

    screenListener = std::make_unique<ScreenListener>(*this);
    screen->addMouseListener(screenListener.get(), true);

    hardware.onStateChanged = [this] {
        for (auto *p : {leftPad.get(), rightPad.get(), bottomLeft.get(),
                        bottomRight.get()})
            p->repaint();
        keyboard->repaint();
    };
    layoutPads();
}

TouchShell::~TouchShell() {
    hardware.onStateChanged = nullptr;
    screen->removeMouseListener(screenListener.get());
    stopTimer();
    for (auto &d : drags)
        if (d.second.held)
            hardware.pressEncoder(d.second.encoder, false);
}

void TouchShell::layoutPads() {
    using K = Pad::Kind;
    using namespace Playful;
    std::vector<Pad::Spec> tabs = {
        {"Tracks", K::cc, MCM::TRACKS_BUTTON},
        {"Plugins", K::cc, MCM::PLUGINS_BUTTON},
        {"Modifiers", K::cc, MCM::MODIFIERS_BUTTON},
        {"Seq", K::cc, MCM::SEQUENCERS_BUTTON},
        {"Mixer", K::cc, MCM::MIXER_BUTTON},
        {"Tempo", K::cc, MCM::TEMPO_BUTTON},
        {"Settings", K::cc, MCM::SETTINGS_BUTTON},
        {"Save", K::cc, MCM::SAVE_BUTTON},
    };
    std::vector<Pad::Spec> transport = {
        {"Play", K::cc, MCM::PLAY_BUTTON, Colours::green},
        {"Stop", K::cc, MCM::STOP_BUTTON},
        {"Rec", K::cc, MCM::RECORD_BUTTON, Colours::red},
        {"Loop in", K::cc, MCM::LOOP_IN_BUTTON},
        {"Loop", K::cc, MCM::LOOP_BUTTON},
        {"Loop out", K::cc, MCM::LOOP_OUT_BUTTON},
        {"Cut", K::cc, MCM::CUT_BUTTON},
        {"Paste", K::cc, MCM::PASTE_BUTTON},
        {"Slice", K::cc, MCM::SLICE_BUTTON},
        {"Ctrl", K::control, 0, Colours::yellow},
        {utf8("\xe2\x88\x92"), K::cc, MCM::MINUS_BUTTON, Colours::text},
        {"+", K::cc, MCM::PLUS_BUTTON, Colours::text},
    };
    Pad::Spec undo{"Undo", K::cc, MCM::UNDO_BUTTON};
    Pad::Spec big{"", K::bigScreen, 0, Colours::pink};
    Pad::Spec octDown{"Oct " + utf8("\xe2\x88\x92"), K::octaveDown, 0,
                      Colours::blue};
    Pad::Spec octUp{"Oct +", K::octaveUp, 0, Colours::blue};

    for (auto *p :
         {leftPad.get(), rightPad.get(), bottomLeft.get(), bottomRight.get()})
        p->bigScreenOn = bigScreen;

    if (bigScreen) {
        tabs.push_back(undo);
        tabs.push_back(big);
        leftPad->setButtons(tabs, 2);
        rightPad->setButtons(transport, 2);
    } else {
        leftPad->setButtons(tabs, 2);
        rightPad->setButtons(transport, 3);
        bottomLeft->setButtons({octDown, undo}, 1);
        bottomRight->setButtons({octUp, big}, 1);
    }
    keyboard->setVisible(!bigScreen);
    bottomLeft->setVisible(!bigScreen);
    bottomRight->setVisible(!bigScreen);
}

void TouchShell::setBigScreen(bool big) {
    if (big == bigScreen)
        return;
    keyboard->releaseAll();
    bigScreen = big;
    layoutPads();
    resized();
}

void TouchShell::paint(juce::Graphics &g) {
    g.fillAll(Playful::Colours::background.darker(0.35f));
}

void TouchShell::resized() {
    auto area = getLocalBounds().toFloat().reduced(6.0f);
    const float gap = 6.0f;

    if (!bigScreen) {
        auto bottom = area.removeFromBottom(area.getHeight() * 0.36f);
        area.removeFromBottom(gap);
        auto sideWidth =
            juce::jmin(bottom.getWidth() * 0.08f, bottom.getHeight() * 0.9f);
        bottomLeft->setBounds(bottom.removeFromLeft(sideWidth).toNearestInt());
        bottomRight->setBounds(
            bottom.removeFromRight(sideWidth).toNearestInt());
        bottom.reduce(gap, 0.0f);
        keyboard->setBounds(bottom.toNearestInt());
    }

    // the screen keeps the LMN-3's shape, as big as the height allows while
    // leaving room for the buttons on both sides
    float screenHeight = area.getHeight();
    float screenWidth =
        screenHeight * Playful::designWidth / Playful::designHeight;
    float minSide = area.getWidth() * (bigScreen ? 0.12f : 0.16f);
    if (screenWidth > area.getWidth() - 2 * (minSide + gap)) {
        screenWidth = area.getWidth() - 2 * (minSide + gap);
        screenHeight =
            screenWidth * Playful::designHeight / Playful::designWidth;
    }
    float side = (area.getWidth() - screenWidth) / 2.0f - gap;
    leftPad->setBounds(area.withWidth(side).toNearestInt());
    rightPad->setBounds(
        area.withTrimmedLeft(area.getWidth() - side).toNearestInt());
    screen->setBounds(juce::Rectangle<float>(screenWidth, screenHeight)
                          .withCentre(area.getCentre())
                          .toNearestInt());
}

//==============================================================================
int TouchShell::encoderForDesignX(float x) {
    // matches Playful::drawKnobStrip
    const float left = 18.0f, gap = 10.0f;
    const float w = (Playful::designWidth - 2 * left - 3 * gap) / 4.0f;
    if (x < 0.0f || x > Playful::designWidth)
        return -1;
    return juce::jlimit(0, 3, int((x - left + gap / 2) / (w + gap)));
}

int TouchShell::encoderAt(juce::Point<float> p) const {
    if (screen->getWidth() <= 0 || screen->getHeight() <= 0)
        return -1;
    float x = p.x * Playful::designWidth / screen->getWidth();
    float y = p.y * Playful::designHeight / screen->getHeight();
    if (y < Playful::stripTop || y > Playful::designHeight)
        return -1;
    return encoderForDesignX(x);
}

void TouchShell::screenDown(const juce::MouseEvent &e) {
    auto p = e.getEventRelativeTo(screen.get()).position;
    Drag d;
    d.encoder = encoderAt(p);
    d.start = p;
    d.downTime = juce::Time::getMillisecondCounter();
    if (d.encoder >= 0) {
        drags[e.source.getIndex()] = d;
        startTimer(40);
    }
}

// A finger that stays still on a knob box holds the encoder down until it
// lifts, like keeping a real encoder pressed. A quick tap is a press and
// release, and a finger that moves turns it.
void TouchShell::timerCallback() {
    const auto now = juce::Time::getMillisecondCounter();
    bool waiting = false;
    for (auto &entry : drags) {
        auto &d = entry.second;
        if (d.moved || d.held)
            continue;
        if (now - d.downTime >= holdDelayMs) {
            d.held = true;
            hardware.pressEncoder(d.encoder, true);
        } else {
            waiting = true;
        }
    }
    if (!waiting)
        stopTimer();
}

void TouchShell::screenDrag(const juce::MouseEvent &e) {
    auto it = drags.find(e.source.getIndex());
    if (it == drags.end())
        return;
    auto &d = it->second;
    if (d.held)
        return; // held down: moving the finger doesn't turn it
    auto p = e.getEventRelativeTo(screen.get()).position;
    float scale = Playful::designWidth / juce::jmax(1, screen->getWidth());
    // right or up turns clockwise
    auto delta = (p - d.start) * scale;
    if (!d.moved && delta.getDistanceFromOrigin() < tapSlop)
        return;
    d.moved = true;
    float travel = delta.x - delta.y;
    d.start = p;
    d.accumulated += travel;
    while (std::abs(d.accumulated) >= dragStep) {
        int direction = d.accumulated > 0 ? 1 : -1;
        hardware.turnEncoder(d.encoder, direction);
        d.accumulated -= direction * dragStep;
    }
}

void TouchShell::screenUp(const juce::MouseEvent &e) {
    auto it = drags.find(e.source.getIndex());
    if (it == drags.end())
        return;
    if (it->second.held) {
        hardware.pressEncoder(it->second.encoder, false);
    } else if (!it->second.moved) {
        hardware.pressEncoder(it->second.encoder, true);
        hardware.pressEncoder(it->second.encoder, false);
    }
    drags.erase(it);
}
