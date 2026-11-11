#include "PlayfulApp.h"

namespace Playful {

namespace {
juce::PathStrokeType roundStroke(float w) {
    return juce::PathStrokeType(w, juce::PathStrokeType::curved,
                                juce::PathStrokeType::rounded);
}
} // namespace

void paintScreen(juce::Graphics &g, juce::Rectangle<int> bounds,
                 const juce::String &title, const juce::String &subtitle,
                 const juce::String &pageLabel,
                 const std::array<KnobInfo, 4> &knobs,
                 const std::function<void(juce::Graphics &)> &paintStage,
                 bool stars, double time) {
    juce::Graphics::ScopedSaveState outer(g);
    g.reduceClipRegion(bounds);
    g.setColour(Colours::background);
    g.fillRect(bounds);
    float scale = juce::jmin(bounds.getWidth() / designWidth,
                             bounds.getHeight() / designHeight);
    float offsetX =
        bounds.getX() + (bounds.getWidth() - designWidth * scale) / 2.0f;
    float offsetY =
        bounds.getY() + (bounds.getHeight() - designHeight * scale) / 2.0f;
    g.addTransform(
        juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    drawHeader(g, title, subtitle, pageLabel);
    {
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::translation(0, headerHeight));
        g.reduceClipRegion(0, 0, int(designWidth), int(stageHeight));
        if (stars)
            drawStars(g, time);
        paintStage(g);
    }
    drawKnobStrip(g, knobs);
}

const juce::Array<juce::Colour> &trackPalette() {
    static const juce::Array<juce::Colour> palette{
        Colours::pink,   Colours::yellow, Colours::green, Colours::blue,
        Colours::purple, Colours::orange, Colours::red,   Colours::cream};
    return palette;
}

juce::String colourName(juce::Colour c) {
    static const char *names[] = {"pink",   "yellow", "green", "blue",
                                  "purple", "orange", "red",   "cream"};
    int index = trackPalette().indexOf(c);
    return index >= 0 ? juce::String(names[index]) : juce::String("custom");
}

juce::Colour nextTrackColour(juce::Colour current, int direction) {
    const auto &palette = trackPalette();
    int index = palette.indexOf(current);
    if (index < 0)
        return palette[0];
    int size = palette.size();
    return palette[((index + direction) % size + size) % size];
}

void drawBadge(juce::Graphics &g, float cx, float cy, float r,
               juce::Colour colour, double time) {
    g.setColour(colour);
    fillEllipseCentred(g, cx, cy, r, r);

    float eye = r * 0.16f;
    float open = 1.0f;
    double phase = std::fmod(time / 4.2 + cx * 0.0013, 1.0);
    if (time > 0.0 && phase > 0.93 && phase < 0.98)
        open = 0.2f;
    g.setColour(Colours::ink);
    fillEllipseCentred(g, cx - r * 0.34f, cy - r * 0.08f, eye, eye * open);
    fillEllipseCentred(g, cx + r * 0.34f, cy - r * 0.08f, eye, eye * open);

    juce::Path mouth;
    mouth.startNewSubPath(cx - r * 0.22f, cy + r * 0.28f);
    mouth.quadraticTo(cx, cy + r * 0.48f, cx + r * 0.22f, cy + r * 0.28f);
    g.strokePath(mouth, roundStroke(juce::jmax(1.2f, r * 0.13f)));
}

void drawCard(juce::Graphics &g, juce::Rectangle<float> r,
              const juce::String &title, const juce::String &detail,
              juce::Colour iconColour, bool selected, bool off, double time) {
    g.setColour(selected ? Colours::blue : Colours::panel);
    g.fillRoundedRectangle(r, juce::jmin(16.0f, r.getHeight() / 2.8f));

    float badgeR = juce::jmin(12.0f, r.getHeight() * 0.3f);
    drawBadge(g, r.getX() + 10.0f + badgeR, r.getCentreY(), badgeR,
              off ? Colours::dim : iconColour, time);

    float textX = r.getX() + 20.0f + badgeR * 2.0f;
    auto textColour = selected ? Colours::ink : Colours::text;
    if (off)
        textColour = textColour.withAlpha(0.5f);

    float detailWidth = 0.0f;
    if (detail.isNotEmpty()) {
        auto f = font(12.0f);
        detailWidth = juce::jmin(r.getWidth() * 0.45f,
                                 f.getStringWidthFloat(detail) + 8.0f);
        g.setFont(f);
        g.setColour(selected ? Colours::ink : Colours::muted);
        g.drawText(detail,
                   juce::Rectangle<float>(r.getRight() - 14.0f - detailWidth,
                                          r.getY(), detailWidth, r.getHeight()),
                   juce::Justification::centredRight, true);
    }

    g.setColour(textColour);
    g.setFont(font(15.0f, selected));
    g.drawText(title,
               juce::Rectangle<float>(
                   textX, r.getY(), r.getRight() - textX - detailWidth - 18.0f,
                   r.getHeight()),
               juce::Justification::centredLeft, true);
}

void drawPill(juce::Graphics &g, juce::Rectangle<float> r,
              const juce::String &text, juce::Colour fill,
              juce::Colour textColour, float fontSize) {
    g.setColour(fill);
    g.fillRoundedRectangle(r, r.getHeight() / 2.0f);
    g.setColour(textColour);
    g.setFont(font(fontSize, true));
    g.drawText(text, r, juce::Justification::centred, true);
}

void drawDashedPath(juce::Graphics &g, const juce::Path &path,
                    juce::Colour colour, float thickness, float dash,
                    float gap) {
    float lengths[] = {dash, gap};
    juce::Path dashed;
    roundStroke(thickness).createDashedStroke(dashed, path, lengths, 2);
    g.setColour(colour);
    g.fillPath(dashed);
}

void drawDashedLine(juce::Graphics &g, juce::Line<float> line,
                    juce::Colour colour, float thickness, float dash,
                    float gap) {
    juce::Path p;
    p.startNewSubPath(line.getStart());
    p.lineTo(line.getEnd());
    drawDashedPath(g, p, colour, thickness, dash, gap);
}

void drawText(juce::Graphics &g, const juce::String &text, float x, float y,
              float w, float h, float size, juce::Colour colour, bool bold,
              juce::Justification just) {
    g.setColour(colour);
    g.setFont(font(size, bold));
    g.drawText(text, juce::Rectangle<float>(x, y, w, h), just, true);
}

//==============================================================================
void drawCat(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    float bob = -4.0f * pingPong(time, 2.6);
    g.addTransform(
        juce::AffineTransform::scale(s).translated(cx, cy + bob * s));

    const juce::Colour fur(0xffcdbfe8);
    // tail
    juce::Path tail;
    tail.startNewSubPath(46, 30);
    float sway = 8.0f * (pingPong(time, 1.8) - 0.5f);
    tail.quadraticTo(84, 20 + sway, 70, -14 + sway);
    g.setColour(fur);
    g.strokePath(tail, roundStroke(12.0f));

    // ears
    juce::Path ears;
    ears.addTriangle(-52, -18, -40, -66, -14, -34);
    ears.addTriangle(52, -18, 40, -66, 14, -34);
    g.fillPath(ears);
    juce::Path inner;
    inner.addTriangle(-42, -26, -37, -52, -24, -34);
    inner.addTriangle(42, -26, 37, -52, 24, -34);
    g.setColour(Colours::pink);
    g.fillPath(inner);

    // head
    g.setColour(fur);
    fillEllipseCentred(g, 0, 4, 58, 46);
    drawFace(g, 0, 6, 1.15f, Mood::smile, time);

    // whiskers
    g.setColour(Colours::ink.withAlpha(0.5f));
    for (int side : {-1, 1}) {
        g.drawLine(side * 30.0f, 14, side * 56.0f, 8, 1.6f);
        g.drawLine(side * 30.0f, 19, side * 56.0f, 21, 1.6f);
    }
}

void drawBlob(juce::Graphics &g, float cx, float cy, float s,
              juce::Colour colour, double time) {
    juce::Graphics::ScopedSaveState save(g);
    float squash = 0.06f * (pingPong(time, 1.4) - 0.5f);
    g.addTransform(
        juce::AffineTransform::scale(s * (1.0f + squash), s * (1.0f - squash))
            .translated(cx, cy));
    juce::Path blob;
    blob.startNewSubPath(-50, 34);
    blob.cubicTo(-58, -6, -40, -46, 0, -46);
    blob.cubicTo(40, -46, 58, -6, 50, 34);
    blob.quadraticTo(36, 28, 24, 34);
    blob.quadraticTo(12, 40, 0, 34);
    blob.quadraticTo(-12, 28, -24, 34);
    blob.quadraticTo(-36, 40, -50, 34);
    blob.closeSubPath();
    g.setColour(colour);
    g.fillPath(blob);
    g.setColour(Colours::white.withAlpha(0.5f));
    fillEllipseCentred(g, -24, -24, 8, 5);
    drawFace(g, 0, -4, 1.0f, Mood::smile, time);
}

void drawBallFriend(juce::Graphics &g, float cx, float cy, float r,
                    juce::Colour colour, double time) {
    float bob = -6.0f * pingPong(time, 2.6);
    g.setColour(colour);
    fillEllipseCentred(g, cx, cy + bob, r, r);
    g.setColour(Colours::white.withAlpha(0.45f));
    fillEllipseCentred(g, cx - r * 0.45f, cy + bob - r * 0.5f, r * 0.18f,
                       r * 0.12f);
    drawFace(g, cx, cy + bob, r / 30.0f, Mood::smile, time);
}

void drawBee(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    float hover = -3.0f * pingPong(time, 0.4);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy + hover));
    g.setColour(Colours::white.withAlpha(0.8f));
    float flap = 2.0f + 2.0f * pingPong(time, 0.15);
    fillEllipseCentred(g, -5, -10, 6, flap + 2);
    fillEllipseCentred(g, 5, -10, 6, flap + 2);
    g.setColour(Colours::yellow);
    fillEllipseCentred(g, 2, 0, 12, 10);
    g.setColour(Colours::ink);
    g.fillRect(0.0f, -9.0f, 3.0f, 18.0f);
    g.fillRect(6.0f, -8.0f, 3.0f, 16.0f);
    g.drawLine(13.0f, 0.0f, 17.0f, 0.0f, 1.6f);
    g.setColour(Colours::yellow);
    fillEllipseCentred(g, -9, 0, 7.5f, 7.5f);
    drawFace(g, -9, -0.5f, 0.27f, Mood::smile, time);
}

void drawChick(juce::Graphics &g, float cx, float cy, float s, double time,
               bool hopping) {
    juce::Graphics::ScopedSaveState save(g);
    float hop = hopping ? -4.0f * pingPong(time, 0.5) : 0.0f;
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy + hop));
    g.setColour(Colours::yellow);
    fillEllipseCentred(g, 0, -6, 11, 10);
    g.setColour(Colours::orange);
    juce::Path beak;
    beak.addTriangle(9, -8, 16, -6, 9, -4);
    g.fillPath(beak);
    g.setColour(Colours::ink);
    fillEllipseCentred(g, 4, -9, 1.8f, 1.8f);
    g.setColour(Colours::orange);
    g.drawLine(-3, 3, -4, 7, 2.0f);
    g.drawLine(3, 3, 4, 7, 2.0f);
}

void drawRobot(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    float bob = -5.0f * pingPong(time, 2.6);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy + bob));

    // antenna
    float sway = 6.0f * (pingPong(time, 1.6) - 0.5f);
    g.setColour(Colours::muted);
    g.drawLine(0, -44, sway, -62, 4.0f);
    g.setColour(Colours::green);
    fillEllipseCentred(g, sway, -66, 6, 6);

    // head and ears
    g.setColour(Colours::red);
    g.fillRoundedRectangle(-44, -48, 88, 56, 18);
    g.fillRoundedRectangle(-52, -34, 10, 22, 5);
    g.fillRoundedRectangle(42, -34, 10, 22, 5);
    drawFace(g, 0, -20, 0.85f, Mood::smile, time);

    // body
    g.setColour(Colours::red.darker(0.25f));
    g.fillRoundedRectangle(-34, 6, 68, 40, 14);
    g.setColour(Colours::ink);
    g.fillRoundedRectangle(-24, 14, 48, 22, 7);
    float scan = 18.0f * (pingPong(time, 2.4) - 0.5f);
    g.setColour(Colours::green);
    g.drawLine(-14 + scan, 25, -4 + scan, 20, 2.5f);
    g.drawLine(-4 + scan, 20, 6 + scan, 30, 2.5f);
}

void drawOven(juce::Graphics &g, float cx, float cy, float s, double time,
              bool baking) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy));

    g.setColour(Colours::orange);
    g.fillRoundedRectangle(-90, -78, 180, 150, 28);
    // face on top
    drawFace(g, 0, -56, 0.8f, baking ? Mood::sing : Mood::smile, time);

    // window with a glow
    g.setColour(Colours::ink);
    g.fillRoundedRectangle(-70, -34, 140, 90, 18);
    float glow = baking ? 0.5f + 0.4f * pingPong(time, 1.2) : 0.35f;
    g.setColour(Colours::red.withAlpha(glow * 0.6f));
    g.fillRoundedRectangle(-58, -24, 116, 70, 12);

    // bread
    g.setColour(Colours::cream);
    juce::Path bread;
    bread.addRoundedRectangle(-34.0f, 4.0f, 68.0f, 30.0f, 12.0f, 12.0f, true,
                              true, false, false);
    bread.addEllipse(-34.0f, -6.0f, 68.0f, 28.0f);
    g.fillPath(bread);
    g.setColour(Colours::orange.darker(0.2f));
    g.drawLine(-14, 2, -6, -6, 3.0f);
    g.drawLine(0, 4, 8, -4, 3.0f);
    g.drawLine(14, 6, 20, 0, 3.0f);

    if (baking) {
        for (int i = 0; i < 3; i++) {
            float t = loop(time + i * 0.5, 1.5);
            g.setColour(Colours::cream.withAlpha(0.7f * (1.0f - t)));
            fillEllipseCentred(g, -30.0f + i * 30.0f, -86.0f - t * 40.0f,
                               6.0f + t * 4.0f, 6.0f + t * 4.0f);
        }
    }

    // feet
    g.setColour(Colours::red);
    g.fillRoundedRectangle(-70, 70, 30, 10, 5);
    g.fillRoundedRectangle(40, 70, 30, 10, 5);
}

void drawMoon(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    float bob = -6.0f * pingPong(time, 3.0);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy + bob));
    juce::Path bite;
    bite.addEllipse(-28, -88, 150, 150);
    g.setColour(Colours::cream);
    {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path outer;
        outer.addEllipse(-76, -76, 152, 152);
        g.reduceClipRegion(outer);
        g.fillPath(outer);
        g.setColour(Colours::background);
        g.fillPath(bite);
    }
    drawFace(g, -44, 4, 0.75f, Mood::sleep, time);

    for (int i = 0; i < 3; i++) {
        float t = loop(time + i * 0.8, 2.4);
        g.setColour(Colours::muted.withAlpha(1.0f - t));
        g.setFont(font(14.0f + i * 4.0f, true));
        g.drawText("z",
                   juce::Rectangle<float>(40.0f + i * 14.0f + t * 10.0f,
                                          -70.0f - i * 18.0f - t * 20.0f, 20,
                                          20),
                   juce::Justification::centred);
    }
}

void drawFlag(juce::Graphics &g, float x, float y, bool pointsRight,
              juce::Colour colour) {
    g.setColour(colour);
    g.drawLine(x, y, x, y + 26, 2.5f);
    juce::Path flag;
    float d = pointsRight ? 14.0f : -14.0f;
    flag.addTriangle(x, y, x + d, y + 5, x, y + 10);
    g.fillPath(flag);
}

juce::Path polygon(float cx, float cy, float radius, int sides, float angle) {
    juce::Path p;
    for (int i = 0; i < sides; i++) {
        float a = angle - juce::MathConstants<float>::halfPi +
                  i * juce::MathConstants<float>::twoPi / sides;
        juce::Point<float> pt(cx + radius * std::cos(a),
                              cy + radius * std::sin(a));
        if (i == 0)
            p.startNewSubPath(pt);
        else
            p.lineTo(pt);
    }
    p.closeSubPath();
    return p;
}

void drawDrum(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy));
    // sticks take turns
    for (int side : {-1, 1}) {
        float hit = pingPong(time + (side > 0 ? 0.25 : 0.0), 0.5);
        float a = juce::degreesToRadians(side * (8.0f + 18.0f * hit));
        juce::Graphics::ScopedSaveState stick(g);
        g.addTransform(
            juce::AffineTransform::rotation(a, side * 14.0f, -40.0f));
        g.setColour(Colours::orange);
        g.drawLine(side * 14.0f, -40.0f, side * 56.0f, -86.0f, 7.0f);
        g.setColour(Colours::yellow);
        fillEllipseCentred(g, side * 58.0f, -89.0f, 8.0f, 8.0f);
    }
    g.setColour(Colours::red);
    g.fillRoundedRectangle(-54, -34, 108, 78, 12);
    juce::Path zig;
    zig.startNewSubPath(-54, -26);
    zig.lineTo(-27, 38);
    zig.lineTo(0, -26);
    zig.lineTo(27, 38);
    zig.lineTo(54, -26);
    g.setColour(Colours::cream);
    g.strokePath(zig, roundStroke(4.0f));
    g.setColour(Colours::red.darker(0.2f));
    fillEllipseCentred(g, 0, 44, 54, 13);
    g.setColour(Colours::cream);
    fillEllipseCentred(g, 0, -34, 54, 16);
    fillEllipseCentred(g, 0, 8, 22, 22);
    drawFace(g, 0, 6, 0.8f, Mood::smile, time);
}

void drawCaterpillar(juce::Graphics &g, float cx, float cy, float s,
                     double time) {
    juce::Graphics::ScopedSaveState save(g);
    float wiggle = 4.0f * pingPong(time, 1.0);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx + wiggle, cy));
    float xs[] = {-66, -42, -18, 6}, ys[] = {8, 0, 6, 0};
    for (int i = 0; i < 4; i++) {
        g.setColour(i % 2 == 0 ? Colours::yellow : Colours::green);
        fillEllipseCentred(g, xs[i], ys[i], 17, 17);
        g.setColour(Colours::ink);
        g.drawLine(xs[i] - 5, ys[i] + 16, xs[i] - 7, ys[i] + 24, 3.0f);
        g.drawLine(xs[i] + 5, ys[i] + 16, xs[i] + 7, ys[i] + 24, 3.0f);
    }
    g.setColour(Colours::green);
    g.drawLine(36, -26, 28, -50, 3.0f);
    g.drawLine(50, -24, 58, -48, 3.0f);
    g.setColour(Colours::pink);
    fillEllipseCentred(g, 28, -52, 5, 5);
    fillEllipseCentred(g, 58, -50, 5, 5);
    g.setColour(Colours::green);
    fillEllipseCentred(g, 42, -6, 24, 24);
    drawFace(g, 44, -6, 0.85f, Mood::smile, time);
}

void drawTombola(juce::Graphics &g, float cx, float cy, float s, double time) {
    float angle = float(time * 0.7);
    auto shape = polygon(cx, cy, 60.0f * s, 6, angle);
    g.setColour(Colours::panel);
    g.fillPath(shape);
    g.setColour(Colours::blue);
    g.strokePath(shape,
                 juce::PathStrokeType(7.0f * s, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
    float bob = 4.0f * pingPong(time, 0.8);
    drawBadge(g, cx - 16.0f * s, cy + 22.0f * s - bob, 14.0f * s, Colours::pink,
              time);
    drawBadge(g, cx + 14.0f * s, cy + 28.0f * s, 14.0f * s, Colours::yellow,
              time);
    drawBadge(g, cx + 4.0f * s, cy - 6.0f * s + bob, 14.0f * s, Colours::green,
              time);
}

void drawTrain(juce::Graphics &g, float cx, float cy, float s, double time) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(s).translated(cx, cy));
    for (int i = 0; i < 2; i++) {
        float t = loop(time + i * 0.8, 1.6);
        g.setColour(Colours::cream.withAlpha(0.8f * (1.0f - t)));
        fillEllipseCentred(g, -34.0f - t * 12.0f, -66.0f - t * 30.0f,
                           8.0f + t * 4.0f, 8.0f + t * 4.0f);
    }
    g.setColour(Colours::ink);
    g.fillRoundedRectangle(-42, -54, 18, 26, 5);
    g.setColour(Colours::orange);
    g.fillRoundedRectangle(-46, -58, 26, 8, 4);
    g.setColour(Colours::blue);
    g.fillRoundedRectangle(-56, -30, 76, 52, 16);
    g.setColour(Colours::purple);
    g.fillRoundedRectangle(14, -56, 46, 78, 12);
    g.setColour(Colours::cream);
    g.fillRoundedRectangle(24, -46, 26, 22, 7);
    for (float wx : {-34.0f, 38.0f}) {
        g.setColour(Colours::ink);
        fillEllipseCentred(g, wx, 28, 13, 13);
        g.setColour(Colours::cream);
        fillEllipseCentred(g, wx, 28, 5, 5);
    }
    drawFace(g, -20, -4, 0.85f, Mood::smile, time);
}

} // namespace Playful
