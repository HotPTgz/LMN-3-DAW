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

void drawStars(juce::Graphics &g, double time) {
    for (int i = 0; i < 14; i++) {
        float x = float((i * 137) % 780 + 10);
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
                const juce::String &subtitle, const juce::String &pageLabel) {
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
    float x = designWidth - 24.0f - w;
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
