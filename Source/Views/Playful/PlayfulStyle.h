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
                const juce::String &subtitle, const juce::String &pageLabel);
void drawKnobStrip(juce::Graphics &g, const std::array<KnobInfo, 4> &knobs);

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
