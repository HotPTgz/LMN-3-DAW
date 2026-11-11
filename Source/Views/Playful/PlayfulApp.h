#pragma once
#include "PlayfulStyle.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Drawing helpers and little characters for the playful app screens
// (tracks, mixer, lists, tempo...). Everything is in design coordinates.
namespace Playful {

// Paints a whole playful screen (header, stage, knob strip) into bounds,
// for components that can't derive from PlayfulScreen (e.g. overlays)
void paintScreen(juce::Graphics &g, juce::Rectangle<int> bounds,
                 const juce::String &title, const juce::String &subtitle,
                 const juce::String &pageLabel,
                 const std::array<KnobInfo, 4> &knobs,
                 const std::function<void(juce::Graphics &)> &paintStage,
                 bool stars, double time);

// Colours a track can be given with encoder 4 on the tracks screen
const juce::Array<juce::Colour> &trackPalette();
// "pink", "blue"... for a palette colour, or "custom"
juce::String colourName(juce::Colour c);
// The next / previous palette colour (an unknown colour starts at pink)
juce::Colour nextTrackColour(juce::Colour current, int direction);

// A small round critter with a face, used as an icon for list items and
// track numbers
void drawBadge(juce::Graphics &g, float cx, float cy, float r,
               juce::Colour colour, double time = 0.0);

// A rounded list card with a badge, title and a right-aligned detail
void drawCard(juce::Graphics &g, juce::Rectangle<float> r,
              const juce::String &title, const juce::String &detail,
              juce::Colour iconColour, bool selected, bool off = false,
              double time = 0.0);

// A short pill with centred text
void drawPill(juce::Graphics &g, juce::Rectangle<float> r,
              const juce::String &text, juce::Colour fill,
              juce::Colour textColour, float fontSize = 12.0f);

void drawDashedLine(juce::Graphics &g, juce::Line<float> line,
                    juce::Colour colour, float thickness = 2.5f,
                    float dash = 4.0f, float gap = 6.0f);
void drawDashedPath(juce::Graphics &g, const juce::Path &path,
                    juce::Colour colour, float thickness = 2.5f,
                    float dash = 4.0f, float gap = 6.0f);

// Centred text helper
void drawText(juce::Graphics &g, const juce::String &text, float x, float y,
              float w, float h, float size, juce::Colour colour,
              bool bold = false,
              juce::Justification just = juce::Justification::centred);

// Characters, each drawn around cx,cy
void drawCat(juce::Graphics &g, float cx, float cy, float s, double time);
void drawBlob(juce::Graphics &g, float cx, float cy, float s,
              juce::Colour colour, double time);
void drawBallFriend(juce::Graphics &g, float cx, float cy, float r,
                    juce::Colour colour, double time);
void drawBee(juce::Graphics &g, float cx, float cy, float s, double time);
void drawChick(juce::Graphics &g, float cx, float cy, float s, double time,
               bool hopping);
void drawRobot(juce::Graphics &g, float cx, float cy, float s, double time);
void drawOven(juce::Graphics &g, float cx, float cy, float s, double time,
              bool baking);
void drawMoon(juce::Graphics &g, float cx, float cy, float s, double time);
void drawFlag(juce::Graphics &g, float x, float y, bool pointsRight,
              juce::Colour colour);
void drawDrum(juce::Graphics &g, float cx, float cy, float s, double time);
void drawCaterpillar(juce::Graphics &g, float cx, float cy, float s,
                     double time);
void drawTombola(juce::Graphics &g, float cx, float cy, float s, double time);
void drawTrain(juce::Graphics &g, float cx, float cy, float s, double time);
// a regular polygon with a vertex at the top, turned by angle (radians)
juce::Path polygon(float cx, float cy, float radius, int sides, float angle);

} // namespace Playful
