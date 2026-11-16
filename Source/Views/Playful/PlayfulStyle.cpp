#include "PlayfulStyle.h"
#include "FontData.h"
#include <app_configuration/app_configuration.h>

namespace Playful {

namespace Colours {
const juce::Colour &encoder(int index) {
    static const juce::Colour colours[] = {blue, green, cream, red};
    return colours[juce::jlimit(0, 3, index)];
}

const juce::Colour &candy(int index) {
    static const juce::Colour colours[] = {pink, yellow, green,
                                           blue, purple, orange};
    return colours[((index % 6) + 6) % 6];
}
} // namespace Colours

juce::Font font(float height, bool bold) {
    static juce::Typeface::Ptr regular =
        juce::Typeface::createSystemTypefaceFor(
            FontData::FredokaRegular_ttf, FontData::FredokaRegular_ttfSize);
    static juce::Typeface::Ptr semiBold =
        juce::Typeface::createSystemTypefaceFor(
            FontData::FredokaSemiBold_ttf, FontData::FredokaSemiBold_ttfSize);
    return juce::Font(bold ? semiBold : regular).withHeight(height);
}

void fillEllipseCentred(juce::Graphics &g, float cx, float cy, float rx,
                        float ry) {
    g.fillEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
}

juce::Path roundedRect(float x, float y, float w, float h, float r) {
    juce::Path p;
    p.addRoundedRectangle(x, y, w, h, juce::jmin(r, w / 2.0f, h / 2.0f));
    return p;
}

float pingPong(double time, double period) {
    double phase = std::fmod(time / period, 1.0);
    return float(0.5 -
                 0.5 * std::cos(phase * juce::MathConstants<double>::twoPi));
}

float loop(double time, double period) {
    return float(std::fmod(time / period, 1.0));
}

float easeInOutSine(float x) {
    return 0.5f - 0.5f * std::cos(x * juce::MathConstants<float>::pi);
}

void drawFace(juce::Graphics &g, float x, float y, float s, Mood mood,
              double time, bool blink) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s).translated(x, y));

    auto strokeCurve = [&](const juce::Path &p, float width) {
        g.strokePath(p,
                     juce::PathStrokeType(width, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    };

    g.setColour(Colours::ink);
    if (mood == Mood::squish) {
        juce::Path eyes;
        eyes.startNewSubPath(-17, -5);
        eyes.lineTo(-9, 0);
        eyes.lineTo(-17, 5);
        eyes.startNewSubPath(17, -5);
        eyes.lineTo(9, 0);
        eyes.lineTo(17, 5);
        strokeCurve(eyes, 3.0f);
    } else if (mood == Mood::sleep) {
        juce::Path eyes;
        eyes.startNewSubPath(-17, 0);
        eyes.quadraticTo(-12, 4, -7, 0);
        eyes.startNewSubPath(7, 0);
        eyes.quadraticTo(12, 4, 17, 0);
        strokeCurve(eyes, 2.5f);
    } else {
        // blink briefly every ~4.2 seconds
        float open = 1.0f;
        if (blink) {
            double phase = std::fmod(time / 4.2, 1.0);
            if (phase > 0.92 && phase < 0.98)
                open = 0.15f;
        }
        fillEllipseCentred(g, -12, 0, 4.5f, 6.0f * open);
        fillEllipseCentred(g, 12, 0, 4.5f, 6.0f * open);
        if (open > 0.5f) {
            g.setColour(Colours::white);
            g.fillEllipse(-10.5f - 1.7f, -2.5f - 1.7f, 3.4f, 3.4f);
            g.fillEllipse(13.5f - 1.7f, -2.5f - 1.7f, 3.4f, 3.4f);
        }
    }

    // blush
    g.setColour(Colours::pink.withAlpha(0.75f));
    fillEllipseCentred(g, -21, 9, 6, 3.5f);
    fillEllipseCentred(g, 21, 9, 6, 3.5f);

    // mouth
    g.setColour(Colours::ink);
    juce::Path mouth;
    switch (mood) {
    case Mood::smile:
        mouth.startNewSubPath(-6, 9);
        mouth.quadraticTo(0, 15, 6, 9);
        strokeCurve(mouth, 2.5f);
        break;
    case Mood::sing:
        fillEllipseCentred(g, 0, 12, 4, 5);
        break;
    case Mood::squish:
        mouth.startNewSubPath(-7, 12);
        mouth.quadraticTo(-3.5f, 8, 0, 12);
        mouth.quadraticTo(3.5f, 16, 7, 12);
        strokeCurve(mouth, 2.5f);
        break;
    case Mood::sleep:
        mouth.startNewSubPath(-4, 11);
        mouth.lineTo(4, 11);
        strokeCurve(mouth, 2.5f);
        break;
    }
}

namespace {
float currentStageWidth = designWidth;
}

float stageWidth() { return currentStageWidth; }

void drawStars(juce::Graphics &g, double time) {
    int count = juce::roundToInt(14 * stageWidth() / designWidth);
    int span = juce::roundToInt(stageWidth() - 20.0f);
    for (int i = 0; i < count; i++) {
        float x = float((i * 137) % span + 10);
        float y = float((i * 71) % 120 + 8);
        float r = 1.2f + float(i % 3) * 0.8f;
        float t = pingPong(time + (i % 7) * 0.35, 2.4);
        float scale = 0.7f + 0.3f * t;
        g.setColour(Colours::cream.withAlpha(0.2f + 0.8f * t));
        fillEllipseCentred(g, x, y, r * scale, r * scale);
    }
}

void drawBackground(juce::Graphics &g) { g.fillAll(Colours::background); }

void drawHeader(juce::Graphics &g, const juce::String &title,
                const juce::String &subtitle, const juce::String &pageLabel,
                float width) {
    g.setColour(Colours::yellow);
    g.fillEllipse(24, 21, 14, 14);

    g.setColour(Colours::text);
    g.setFont(font(22.0f, true));
    g.drawText(title, 48, 12, 400, 32, juce::Justification::centredLeft);

    juce::String right = subtitle;
    if (pageLabel.isNotEmpty())
        right += (right.isEmpty() ? "" : utf8("  ·  ")) + pageLabel;
    if (right.isEmpty())
        return;

    auto f = font(14.0f);
    float w = f.getStringWidthFloat(right) + 24.0f;
    float x = width - 24.0f - w;
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(x, 15, w, 26, 13);
    g.setColour(Colours::muted);
    g.setFont(f);
    g.drawText(right, juce::Rectangle<float>(x, 15, w, 26),
               juce::Justification::centred);
}

void drawKnobStrip(juce::Graphics &g, const std::array<KnobInfo, 4> &knobs) {
    const float gap = 10.0f, left = 18.0f, right = 18.0f;
    const float w = (designWidth - left - right - 3 * gap) / 4.0f;
    const float h = 54.0f;
    const float y =
        stripTop + 4.0f + (designHeight - stripTop - 4.0f - 16.0f - h) / 2.0f;

    for (int i = 0; i < 4; i++) {
        const auto &k = knobs[size_t(i)];
        float x = left + i * (w + gap);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(x, y, w, h, 20);

        // knob ring
        float cx = x + 12 + 17, cy = y + h / 2;
        juce::Colour colour = k.enabled ? k.colour : Colours::dim;
        juce::Path ring;
        ring.addCentredArc(cx, cy, 13, 13, 0, juce::degreesToRadians(-135.0f),
                           juce::degreesToRadians(135.0f), true);
        g.setColour(Colours::dim);
        g.strokePath(ring,
                     juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
        if (k.enabled) {
            juce::Path arc;
            float end =
                -135.0f + 270.0f * juce::jlimit(0.001f, 1.0f, k.normalised);
            arc.addCentredArc(cx, cy, 13, 13, 0,
                              juce::degreesToRadians(-135.0f),
                              juce::degreesToRadians(end), true);
            g.setColour(colour);
            g.strokePath(
                arc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
        }
        g.setColour(colour);
        fillEllipseCentred(g, cx, cy, 5, 5);

        float tx = x + 12 + 34 + 10;
        g.setColour(Colours::muted);
        g.setFont(font(11.0f));
        g.drawText(k.enabled ? k.label.toUpperCase() : utf8("—"),
                   juce::Rectangle<float>(tx, y + 9, w - (tx - x) - 8, 14),
                   juce::Justification::centredLeft, true);
        g.setColour(Colours::text);
        g.setFont(font(17.0f, true));
        g.drawFittedText(
            k.enabled ? k.value : juce::String(),
            juce::Rectangle<float>(tx, y + 23, w - (tx - x) - 8, 22)
                .toNearestInt(),
            juce::Justification::centredLeft, 1, 0.7f);
    }
}

//==============================================================================
bool isWide(float width, float height) {
    return height > 0.0f && width / height > 2.2f;
}

juce::AffineTransform FrameGeometry::toComponent() const {
    return juce::AffineTransform::scale(scale).translated(offsetX, offsetY);
}

juce::Point<float> FrameGeometry::toDesign(juce::Point<float> p) const {
    return {(p.x - offsetX) / scale, (p.y - offsetY) / scale};
}

FrameGeometry frameGeometry(juce::Rectangle<float> bounds) {
    FrameGeometry f;
    f.wide = isWide(bounds.getWidth(), bounds.getHeight());
    f.designW = f.wide ? wideWidth : designWidth;
    f.designH = f.wide ? wideHeight : designHeight;
    f.scale = juce::jmin(bounds.getWidth() / f.designW,
                         bounds.getHeight() / f.designH);
    f.offsetX = bounds.getX() + (bounds.getWidth() - f.designW * f.scale) / 2;
    f.offsetY = bounds.getY() + (bounds.getHeight() - f.designH * f.scale) / 2;
    return f;
}

juce::Rectangle<float> wideStageArea() {
    return {0.0f, wideHeaderHeight, widePanelX, wideHeight - wideHeaderHeight};
}

float wideStageScale() {
    auto area = wideStageArea();
    return juce::jmin(area.getWidth() / designWidth,
                      area.getHeight() / stageHeight);
}

juce::Point<float> wideStageOrigin() {
    auto area = wideStageArea();
    float s = wideStageScale();
    return {area.getX() + (area.getWidth() - designWidth * s) / 2.0f,
            area.getY() + (area.getHeight() - stageHeight * s) / 2.0f};
}

juce::Rectangle<float> wideKnobTile(int index) {
    const float right = 14.0f, bottom = 12.0f, gap = 12.0f;
    const float x0 = widePanelX + 4.0f, y0 = wideHeaderHeight + 6.0f;
    const float w = (wideWidth - right - x0 - gap) / 2.0f;
    const float h = (wideHeight - bottom - y0 - gap) / 2.0f;
    return {x0 + (index % 2) * (w + gap), y0 + (index / 2) * (h + gap), w, h};
}

void drawKnobPanel(juce::Graphics &g, const std::array<KnobInfo, 4> &knobs) {
    for (int i = 0; i < 4; i++) {
        const auto &k = knobs[size_t(i)];
        auto r = wideKnobTile(i);
        juce::Colour colour = k.enabled ? k.colour : Colours::dim;
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(r, 22.0f);

        // the encoder's number, in its colour
        g.setColour(colour.withAlpha(k.enabled ? 1.0f : 0.6f));
        g.fillEllipse(r.getX() + 12, r.getY() + 12, 20, 20);
        g.setColour(Colours::ink);
        g.setFont(font(13.0f, true));
        g.drawText(juce::String(i + 1),
                   juce::Rectangle<float>(r.getX() + 12, r.getY() + 12, 20, 20),
                   juce::Justification::centred);

        // knob ring
        float cx = r.getCentreX(), cy = r.getY() + 54.0f, rad = 27.0f;
        auto stroke = juce::PathStrokeType(8.0f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);
        juce::Path ring;
        ring.addCentredArc(cx, cy, rad, rad, 0, juce::degreesToRadians(-135.0f),
                           juce::degreesToRadians(135.0f), true);
        g.setColour(Colours::dim);
        g.strokePath(ring, stroke);
        if (k.enabled) {
            juce::Path arc;
            float end =
                -135.0f + 270.0f * juce::jlimit(0.001f, 1.0f, k.normalised);
            arc.addCentredArc(cx, cy, rad, rad, 0,
                              juce::degreesToRadians(-135.0f),
                              juce::degreesToRadians(end), true);
            g.setColour(colour);
            g.strokePath(arc, stroke);
        }
        g.setColour(colour);
        fillEllipseCentred(g, cx, cy, 9, 9);

        g.setColour(Colours::muted);
        g.setFont(font(13.0f));
        g.drawFittedText(k.enabled ? k.label.toUpperCase() : utf8("—"),
                         juce::Rectangle<float>(r.getX() + 8, r.getY() + 92,
                                                r.getWidth() - 16, 18)
                             .toNearestInt(),
                         juce::Justification::centred, 1, 0.7f);
        g.setColour(Colours::text);
        g.setFont(font(22.0f, true));
        g.drawFittedText(k.enabled ? k.value : juce::String(),
                         juce::Rectangle<float>(r.getX() + 8, r.getY() + 112,
                                                r.getWidth() - 16, 30)
                             .toNearestInt(),
                         juce::Justification::centred, 1, 0.6f);
    }
}

int knobAt(juce::Rectangle<float> bounds, juce::Point<float> p) {
    auto f = frameGeometry(bounds);
    auto d = f.toDesign(p);
    if (f.wide) {
        for (int i = 0; i < 4; i++)
            if (wideKnobTile(i).expanded(6.0f).contains(d))
                return i;
        return -1;
    }
    // matches drawKnobStrip
    if (d.y < stripTop || d.y > designHeight || d.x < 0.0f || d.x > designWidth)
        return -1;
    const float left = 18.0f, gap = 10.0f;
    const float w = (designWidth - 2 * left - 3 * gap) / 4.0f;
    return juce::jlimit(0, 3, int((d.x - left + gap / 2) / (w + gap)));
}

void paintFrame(juce::Graphics &g, juce::Rectangle<float> bounds,
                const juce::String &title, const juce::String &subtitle,
                const juce::String &pageLabel,
                const std::array<KnobInfo, 4> &knobs, bool stars, double time,
                const std::function<void(juce::Graphics &)> &paintStage,
                bool flexibleStage) {
    juce::Graphics::ScopedSaveState outer(g);
    g.reduceClipRegion(bounds.toNearestInt());
    g.setColour(Colours::background);
    g.fillRect(bounds);

    auto f = frameGeometry(bounds);
    g.addTransform(f.toComponent());
    drawHeader(g, title, subtitle, pageLabel, f.designW);

    {
        juce::Graphics::ScopedSaveState state(g);
        float width = designWidth;
        if (f.wide) {
            auto origin = wideStageOrigin();
            float s = wideStageScale();
            if (flexibleStage) {
                // all the width left of the knobs, at the same scale
                auto area = wideStageArea();
                origin.x = area.getX() + 8.0f;
                width = (area.getWidth() - 16.0f) / s;
            }
            g.addTransform(
                juce::AffineTransform::scale(s).translated(origin.x, origin.y));
        } else {
            g.addTransform(juce::AffineTransform::translation(0, headerHeight));
        }
        g.reduceClipRegion(0, 0, int(std::ceil(width)), int(stageHeight));
        currentStageWidth = width;
        if (stars)
            drawStars(g, time);
        paintStage(g);
        currentStageWidth = designWidth;
    }

    if (f.wide)
        drawKnobPanel(g, knobs);
    else
        drawKnobStrip(g, knobs);
}

bool animationsEnabled() {
    static int cached = -1;
    if (cached < 0) {
        auto configFile = juce::File::getSpecialLocation(
                              juce::File::userApplicationDataDirectory)
                              .getChildFile(JUCE_APPLICATION_NAME_STRING)
                              .getChildFile("config.yaml");
        cached = ConfigurationHelpers::getAnimations(configFile) ? 1 : 0;
    }
    return cached == 1;
}

} // namespace Playful
