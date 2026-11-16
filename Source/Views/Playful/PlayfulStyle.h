#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Shared look for the playful plugin screens. Everything is drawn in a fixed
// 800x480 "design space" (the size of the LMN-3 screen and of the design
// mockups) and scaled to the real component size.
namespace Playful {

constexpr float designWidth = 800.0f;
constexpr float designHeight = 480.0f;
constexpr float headerHeight = 46.0f;
constexpr float stageHeight = 328.0f;
constexpr float stripTop = headerHeight + stageHeight;

namespace Colours {
const juce::Colour blue(0xff5ec8f2);  // encoder 1
const juce::Colour green(0xff7ee08a); // encoder 2
const juce::Colour cream(0xfffff1c9); // encoder 3
const juce::Colour red(0xffff6b6b);   // encoder 4
const juce::Colour yellow(0xffffd93d);
const juce::Colour pink(0xffff8fd1);
const juce::Colour purple(0xffb28dff);
const juce::Colour orange(0xffffa94d);
const juce::Colour ink(0xff2b2140);
const juce::Colour background(0xff2b2140);
const juce::Colour panel(0xff3a2d55);
const juce::Colour muted(0xffcdbfe8);
const juce::Colour dim(0xff4a3d66);
const juce::Colour text(0xfffff7e6);
const juce::Colour white(0xffffffff);
const juce::Colour greyKey(0xff6d5f8a);

// encoder colours in order
const juce::Colour &encoder(int index);
// a cycling set of candy colours
const juce::Colour &candy(int index);
} // namespace Colours

// Text with non-ASCII characters (e.g. the middle dot)
inline juce::String utf8(const char *s) { return juce::String::fromUTF8(s); }

// Fredoka (bundled), regular or semi-bold
juce::Font font(float height, bool bold = false);

enum class Mood { smile, sing, squish, sleep };

// A face (eyes with blinking, blush, mouth) centred at x,y, scaled by s.
// time is used for blinking.
void drawFace(juce::Graphics &g, float x, float y, float s, Mood mood,
              double time, bool blink = true);

// Twinkling stars across the top of the stage (stage coordinates)
void drawStars(juce::Graphics &g, double time);

struct KnobInfo {
    juce::String label;
    juce::String value;
    float normalised = 0.0f; // 0-1 position of the arc
    juce::Colour colour;
    bool enabled = true;
};

void drawBackground(juce::Graphics &g);
void drawHeader(juce::Graphics &g, const juce::String &title,
                const juce::String &subtitle, const juce::String &pageLabel,
                float width = designWidth);
void drawKnobStrip(juce::Graphics &g, const std::array<KnobInfo, 4> &knobs);

//==============================================================================
// Wide screens, such as the 7.9" 1280x400 bar of the LMN-3 v3. They use a
// 1280x400 design space: the header runs across the top, the usual 800x328
// stage is scaled into the left part, and the four knobs sit in a 2x2 panel
// on the right, in the same order as the encoders next to the screen
// (1 2 on top, 3 4 below).
constexpr float wideWidth = 1280.0f;
constexpr float wideHeight = 400.0f;
constexpr float wideHeaderHeight = 44.0f;
constexpr float widePanelX = 984.0f;

// A screen is laid out wide when it is more than 2.2 times as wide as tall
bool isWide(float width, float height);

// How a frame's design space maps onto a component area
struct FrameGeometry {
    bool wide = false;
    float designW = designWidth, designH = designHeight;
    float scale = 1.0f, offsetX = 0.0f, offsetY = 0.0f;
    juce::AffineTransform toComponent() const;
    juce::Point<float> toDesign(juce::Point<float> p) const;
};
FrameGeometry frameGeometry(juce::Rectangle<float> bounds);

// Where the stage goes in the wide design space (the 800x328 stage is
// scaled by wideStageScale and placed at wideStageOrigin)
juce::Rectangle<float> wideStageArea();
float wideStageScale();
juce::Point<float> wideStageOrigin();

// The knob tiles of the wide panel, in wide design coordinates
juce::Rectangle<float> wideKnobTile(int index);
void drawKnobPanel(juce::Graphics &g, const std::array<KnobInfo, 4> &knobs);

// Which knob (0-3) is under a point (component coordinates) of a frame
// drawn into bounds, or -1
int knobAt(juce::Rectangle<float> bounds, juce::Point<float> p);

// Paints a whole frame (background, header, stage, knobs) into bounds, in
// the normal or the wide layout depending on its shape. paintStage draws in
// stage coordinates: 328 tall and stageWidth() wide. That is always 800,
// except for a flexible stage on a wide screen, which gets all the width
// left of the knobs (about 900).
void paintFrame(juce::Graphics &g, juce::Rectangle<float> bounds,
                const juce::String &title, const juce::String &subtitle,
                const juce::String &pageLabel,
                const std::array<KnobInfo, 4> &knobs, bool stars, double time,
                const std::function<void(juce::Graphics &)> &paintStage,
                bool flexibleStage = false);

// The width of the stage being painted (see paintFrame)
float stageWidth();

// Path helpers
juce::Path roundedRect(float x, float y, float w, float h, float r);
void fillEllipseCentred(juce::Graphics &g, float cx, float cy, float rx,
                        float ry);

// Simple animation curves, matching the design mockups
float pingPong(double time, double period); // 0..1..0
float loop(double time, double period);     // 0..1 sawtooth
float easeInOutSine(float x);

// Whether screens should animate (config.yaml: animations: false turns it
// off)
bool animationsEnabled();

} // namespace Playful
