#include "DesktopInput.h"
#include "PlayfulApp.h"
#include "PlayfulStyle.h"

using MCM = app_services::MidiCommandManager;

namespace {
// The LMN-3's 24 keys start at F3 (note 53) at octave 0. The two rows are
// laid out like a piano starting on F: white keys on the letter row, black
// keys on the row above.
const char *lowerRow = "zsxdcfvbhnjm"; // F3 .. E4
const char *upperRow = "q2w3e4rt6y7u"; // F4 .. E5
constexpr double repeatDelay = 0.35, repeatInterval = 0.06;

juce::File helpShownMarker() {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
        .getChildFile("LMN-3")
        .getChildFile("keyboard_help_shown");
}

bool keyDown(int keyCode) {
    return juce::KeyPress::isKeyCurrentlyDown(keyCode);
}

//==============================================================================
// The cheat sheet (F12)
class KeyboardHelp : public juce::Component {
  public:
    std::function<void()> onClose;

    void paint(juce::Graphics &g) override {
        using namespace Playful;
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
        g.addTransform(juce::AffineTransform::scale(
            getWidth() / designWidth, getHeight() / designHeight));

        juce::Rectangle<float> card(30, 22, 740, 436);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(card, 22.0f);
        g.setColour(Colours::yellow);
        g.drawRoundedRectangle(card, 22.0f, 2.0f);

        drawText(g, "Playing the LMN-3 on a computer", 50, 34, 700, 30, 22,
                 Colours::text, true);
        drawText(g, utf8("F12 shows or hides this card  ·  click to close"), 50,
                 62, 700, 20, 13, Colours::muted);

        struct Row {
            const char *what;
            const char *keys;
        };
        const Row left[] = {
            {"Keys, lower octave", "Z S X D C F V B H N J M"},
            {"Keys, upper octave", "Q 2 W 3 E 4 R T 6 Y 7 U"},
            {"Octave down / up", "Left / Right arrow"},
            {"Turn an encoder", "scroll over its knob box"},
            {"Press an encoder", "click its knob box"},
            {"Encoder 1  (down up press)", "8 9 0  or  arrows"},
            {"Encoder 2", "I O P"},
            {"Encoder 3", "K L ;"},
            {"Encoder 4", ", . /"},
            {"Control (hold)", "Shift"},
        };
        const Row right[] = {
            {"Tracks  Plugins  Modifiers", "F1  F2  F3"},
            {"Sequencers  Mixer", "F4  F5"},
            {"Tempo  Settings", "F6  F7"},
            {"Play  Stop  Record", "Space  Esc  Enter"},
            {"Loop In  Loop Out  Loop", "[   ]   Ctrl+L"},
            {"Plus  and  Minus", "=  and  -"},
            {"Save  /  Render", "F8  /  Shift+F8"},
            {"Undo", "Ctrl+Z"},
            {"Cut  Paste  Slice", "Ctrl+X  Ctrl+V  Ctrl+D"},
            {"Hold + while pressing keys", "add notes in sequencers"},
        };

        auto drawColumn = [&](const Row *rows, int count, float x) {
            for (int i = 0; i < count; i++) {
                float y = 98.0f + i * 34.0f;
                drawText(g, utf8(rows[i].what), x, y, 170, 28, 13,
                         Colours::muted, false,
                         juce::Justification::centredLeft);
                drawPill(g, {x + 172, y + 3, 158, 22}, utf8(rows[i].keys),
                         Colours::dim, Colours::text, 12.0f);
            }
        };
        drawColumn(left, 10, 52);
        drawColumn(right, 10, 412);
    }

    void parentSizeChanged() override {
        if (auto parent = getParentComponent())
            setBounds(parent->getLocalBounds());
    }

    void mouseDown(const juce::MouseEvent &) override {
        if (onClose)
            onClose();
    }
};
} // namespace

//==============================================================================
DesktopInput::DesktopInput(VirtualHardware &hw) : hardware(hw) {
    using KP = juce::KeyPress;
    buttons = {
        {KP::F1Key, false, MCM::TRACKS_BUTTON},
        {KP::F2Key, false, MCM::PLUGINS_BUTTON},
        {KP::F3Key, false, MCM::MODIFIERS_BUTTON},
        {KP::F4Key, false, MCM::SEQUENCERS_BUTTON},
        {KP::F5Key, false, MCM::MIXER_BUTTON},
        {KP::F6Key, false, MCM::TEMPO_BUTTON},
        {KP::F7Key, false, MCM::SETTINGS_BUTTON},
        {KP::F8Key, false, MCM::SAVE_BUTTON},
        {KP::spaceKey, false, MCM::PLAY_BUTTON},
        {KP::escapeKey, false, MCM::STOP_BUTTON},
        {KP::returnKey, false, MCM::RECORD_BUTTON},
        {'[', false, MCM::LOOP_IN_BUTTON},
        {']', false, MCM::LOOP_OUT_BUTTON},
        {'+', false, MCM::PLUS_BUTTON}, // the =/+ key
        {KP::numberPadAdd, false, MCM::PLUS_BUTTON},
        {'-', false, MCM::MINUS_BUTTON},
        {KP::numberPadSubtract, false, MCM::MINUS_BUTTON},
        {'x', true, MCM::CUT_BUTTON},
        {'v', true, MCM::PASTE_BUTTON},
        {'d', true, MCM::SLICE_BUTTON},
        {'z', true, MCM::UNDO_BUTTON},
        {'s', true, MCM::SAVE_BUTTON},
        {'l', true, MCM::LOOP_BUTTON},
    };
    encoderKeys = {
        {{'8', '9', '0'}, {'i', 'o', 'p'}, {'k', 'l', ';'}, {',', '.', '/'}}};
    startTimer(4);
}

DesktopInput::~DesktopInput() {
    stopTimer();
    releaseEverything();
    if (window != nullptr)
        window->removeMouseListener(this);
}

void DesktopInput::attachTo(juce::Component *newWindow) {
    if (window != nullptr)
        window->removeMouseListener(this);
    window = newWindow;
    if (window == nullptr)
        return;
    window->addMouseListener(this, true);
    window->setWantsKeyboardFocus(true);
    if (help != nullptr && help->isVisible()) {
        help->setVisible(false);
        toggleHelp();
    }
    showHelpOnFirstRun();
}

int DesktopInput::noteIndexForKey(juce::juce_wchar key) {
    key = juce::CharacterFunctions::toLowerCase(key);
    for (int i = 0; i < 12; i++) {
        if (lowerRow[i] == key)
            return i;
        if (upperRow[i] == key)
            return 12 + i;
    }
    return -1;
}

int DesktopInput::encoderForStripX(float x) {
    // matches Playful::drawKnobStrip
    const float left = 18.0f, gap = 10.0f;
    const float w = (Playful::designWidth - 2 * left - 3 * gap) / 4.0f;
    if (x < 0.0f || x > Playful::designWidth)
        return -1;
    return juce::jlimit(0, 3, int((x - left + gap / 2) / (w + gap)));
}

//==============================================================================
void DesktopInput::timerCallback() {
    if (window == nullptr)
        return;

    if (!juce::Process::isForegroundProcess()) {
        releaseEverything();
        return;
    }

    // keep the keyboard focus on the window, so a list that was clicked
    // doesn't also react to the arrow keys
    if (auto focused = juce::Component::getCurrentlyFocusedComponent())
        if (focused != window.getComponent() && window->isParentOf(focused))
            window->grabKeyboardFocus();

    bool helpNow = keyDown(juce::KeyPress::F12Key);
    if (helpNow && !helpKey)
        toggleHelp();
    helpKey = helpNow;

    if (help != nullptr && help->isVisible()) {
        if (keyDown(juce::KeyPress::escapeKey))
            hideHelp();
        return;
    }

    auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    bool ctrl = mods.isCtrlDown() || mods.isCommandDown();

    // Shift is the LMN-3's Control button
    bool shift = mods.isShiftDown();
    if (shift != controlDown) {
        controlDown = shift;
        hardware.setControl(shift);
    }

    for (auto &b : buttons) {
        bool now = keyDown(b.keyCode) && (b.needsCtrl == ctrl);
        if (now != b.down) {
            b.down = now;
            sendCC(b.cc, now ? 127 : 0);
        }
    }

    // octave
    bool left = keyDown(juce::KeyPress::leftKey),
         right = keyDown(juce::KeyPress::rightKey);
    if ((left && !octaveDownKey) || (right && !octaveUpKey))
        hardware.changeOctave(right ? 1 : -1);
    octaveDownKey = left;
    octaveUpKey = right;

    // encoders, with key repeat
    auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (int i = 0; i < 4; i++) {
        auto &k = encoderKeys[size_t(i)];
        int direction = 0;
        if (!ctrl) {
            if (keyDown(k.increaseKey))
                direction = 1;
            else if (keyDown(k.decreaseKey))
                direction = -1;
            if (i == 0 && direction == 0) {
                if (keyDown(juce::KeyPress::downKey))
                    direction = 1;
                else if (keyDown(juce::KeyPress::upKey))
                    direction = -1;
            }
        }
        if (direction != 0 && direction != k.heldDirection) {
            turnEncoder(i, direction);
            k.nextRepeat = now + repeatDelay;
        } else if (direction != 0 && now >= k.nextRepeat) {
            turnEncoder(i, direction);
            k.nextRepeat = now + repeatInterval;
        }
        k.heldDirection = direction;

        bool press = !ctrl && keyDown(k.pressKey);
        if (press != k.pressDown) {
            k.pressDown = press;
            pressEncoder(i, press);
        }
    }

    // the 24 keys (not while Ctrl is held: Ctrl+X and friends are commands)
    for (int i = 0; i < 24; i++) {
        char key = i < 12 ? lowerRow[i] : upperRow[i - 12];
        bool down = !ctrl && keyDown(key);
        if (down != noteDown[size_t(i)]) {
            noteDown[size_t(i)] = down;
            playNote(i, down);
        }
    }
}

void DesktopInput::sendCC(int cc, int value) {
    hardware.button(cc, value != 0);
}

void DesktopInput::turnEncoder(int index, int direction) {
    hardware.turnEncoder(index, direction);
}

void DesktopInput::pressEncoder(int index, bool down) {
    hardware.pressEncoder(index, down);
}

void DesktopInput::playNote(int index, bool down) {
    if (down)
        notePlaying[size_t(index)] = hardware.noteOn(index);
    else
        hardware.noteOff(notePlaying[size_t(index)]);
}

void DesktopInput::releaseEverything() {
    for (int i = 0; i < 24; i++)
        if (noteDown[size_t(i)]) {
            noteDown[size_t(i)] = false;
            playNote(i, false);
        }
    for (auto &b : buttons)
        if (b.down) {
            b.down = false;
            sendCC(b.cc, 0);
        }
    for (int i = 0; i < 4; i++) {
        encoderKeys[size_t(i)].heldDirection = 0;
        if (encoderKeys[size_t(i)].pressDown) {
            encoderKeys[size_t(i)].pressDown = false;
            pressEncoder(i, false);
        }
    }
    if (controlDown) {
        controlDown = false;
        hardware.setControl(false);
    }
}

//==============================================================================
int DesktopInput::stripEncoderAt(const juce::MouseEvent &e) const {
    if (window == nullptr)
        return -1;
    juce::Component *content = window.getComponent();
    if (auto resizable = dynamic_cast<juce::ResizableWindow *>(content))
        if (auto c = resizable->getContentComponent())
            content = c;
    if (content->getWidth() <= 0 || content->getHeight() <= 0)
        return -1;
    auto p = e.getEventRelativeTo(content).position;
    float x = p.x * Playful::designWidth / content->getWidth();
    float y = p.y * Playful::designHeight / content->getHeight();
    if (y < Playful::stripTop || y > Playful::designHeight)
        return -1;
    return encoderForStripX(x);
}

void DesktopInput::mouseWheelMove(const juce::MouseEvent &e,
                                  const juce::MouseWheelDetails &wheel) {
    if (!mouseEnabled)
        return;
    if (help != nullptr && help->isVisible())
        return;
    int index = stripEncoderAt(e);
    if (index < 0)
        return;
    float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (wheel.isReversed)
        delta = -delta;
    auto &acc = wheelAccumulator[size_t(index)];
    if (std::abs(delta) >= 0.1f) {
        // a mouse wheel: one notch, one step
        acc = 0.0f;
        turnEncoder(index, delta > 0 ? 1 : -1);
        return;
    }
    // a trackpad sends many small movements
    acc += delta;
    if (std::abs(acc) >= 0.1f) {
        turnEncoder(index, acc > 0 ? 1 : -1);
        acc = 0.0f;
    }
}

void DesktopInput::mouseDown(const juce::MouseEvent &e) {
    if (!mouseEnabled)
        return;
    if (help != nullptr && help->isVisible())
        return; // the card closes itself
    mouseEncoder = stripEncoderAt(e);
    if (mouseEncoder >= 0)
        pressEncoder(mouseEncoder, true);
}

void DesktopInput::mouseUp(const juce::MouseEvent &) {
    if (mouseEncoder >= 0)
        pressEncoder(mouseEncoder, false);
    mouseEncoder = -1;
}

//==============================================================================
void DesktopInput::toggleHelp() {
    if (window == nullptr)
        return;
    if (help == nullptr) {
        auto card = std::make_unique<KeyboardHelp>();
        card->onClose = [this] { hideHelp(); };
        help = std::move(card);
    }
    juce::Component *content = window.getComponent();
    if (auto resizable = dynamic_cast<juce::ResizableWindow *>(content))
        if (auto c = resizable->getContentComponent())
            content = c;

    bool show = !help->isVisible() || help->getParentComponent() != content;
    if (show) {
        releaseEverything();
        content->addAndMakeVisible(*help);
        help->setBounds(content->getLocalBounds());
        help->toFront(false);
    } else {
        hideHelp();
    }
}

void DesktopInput::hideHelp() {
    if (help == nullptr)
        return;
    help->setVisible(false);
    // keys still held from closing the card (Esc, F12) must not also act
    for (auto &b : buttons)
        b.down = keyDown(b.keyCode);
}

void DesktopInput::showHelpOnFirstRun() {
#if !JUCE_LINUX
    // On a desktop computer, show the keys the first time. (Not on the Pi:
    // with no keyboard attached there would be no way to close it.)
    auto marker = helpShownMarker();
    if (marker.existsAsFile())
        return;
    marker.getParentDirectory().createDirectory();
    marker.create();
    if (help == nullptr || !help->isVisible())
        toggleHelp();
#endif
}
