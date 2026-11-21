#include "HelpOverlay.h"
#include "PlayfulStyle.h"

namespace {
using namespace Playful;

void text(juce::Graphics &g, const juce::String &s, float x, float y, float w,
          float h, float size, juce::Colour colour, bool bold = false,
          juce::Justification j = juce::Justification::centredLeft) {
    g.setColour(colour);
    g.setFont(font(size, bold));
    g.drawFittedText(s, juce::Rectangle<float>(x, y, w, h).toNearestInt(), j,
                     juce::jmax(1, int(h / (size * 1.15f))), 0.9f);
}

// The control a row is about: a coloured encoder number, the same with a
// small "ctrl" in front, or a button name in a pill
void drawControl(juce::Graphics &g, const juce::String &control, float x,
                 float y, float h) {
    auto encoderIndex = [](const juce::String &s) {
        return s.length() == 1 && s[0] >= '1' && s[0] <= '4' ? s[0] - '1' : -1;
    };
    const bool withControl = control.startsWith("C") && control.length() == 2;
    int encoder = encoderIndex(withControl ? control.substring(1) : control);
    if (encoder >= 0) {
        float cx = x + (withControl ? 74.0f : 46.0f), cy = y + h / 2.0f,
              r = juce::jmin(15.0f, h * 0.42f);
        if (withControl) {
            juce::Rectangle<float> pill(x, cy - 9.0f, 44.0f, 18.0f);
            g.setColour(Colours::dim);
            g.fillRoundedRectangle(pill, 9.0f);
            text(g, "ctrl", pill.getX(), pill.getY(), pill.getWidth(),
                 pill.getHeight(), 12, Colours::text, true,
                 juce::Justification::centred);
            text(g, "+", x + 44.0f, y, 14.0f, h, 13, Colours::muted, false,
                 juce::Justification::centred);
        }
        g.setColour(Colours::encoder(encoder));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        text(g, juce::String(encoder + 1), cx - r, cy - r, r * 2.0f, r * 2.0f,
             15, Colours::ink, true, juce::Justification::centred);
        return;
    }
    juce::Rectangle<float> pill(x, y + 3.0f, 92.0f, h - 6.0f);
    g.setColour(Colours::dim);
    g.fillRoundedRectangle(pill, (h - 6.0f) / 2.0f);
    text(g, control, pill.getX() + 4, pill.getY(), pill.getWidth() - 8,
         pill.getHeight(), 13, Colours::text, true,
         juce::Justification::centred);
}
} // namespace

void HelpOverlay::show(const HelpCard &c) {
    card = c;
    setVisible(true);
    toFront(false);
    repaint();
}

void HelpOverlay::hide() { setVisible(false); }

void HelpOverlay::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black.withAlpha(0.6f));

    // the card is drawn at 800x480 and scaled to fit, keeping its shape on
    // any screen (the wide Limoncello one too)
    constexpr float cardWidth = 800.0f, cardHeight = 480.0f;
    float scale = juce::jmin(getWidth() / cardWidth, getHeight() / cardHeight);
    g.addTransform(juce::AffineTransform::scale(scale).translated(
        (getWidth() - cardWidth * scale) / 2.0f,
        (getHeight() - cardHeight * scale) / 2.0f));

    juce::Rectangle<float> panel(18, 14, 764, 452);
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(panel, 22.0f);
    g.setColour(Colours::yellow);
    g.drawRoundedRectangle(panel, 22.0f, 2.0f);

    // a little question-mark badge
    g.setColour(Colours::yellow);
    g.fillEllipse(40, 30, 34, 34);
    text(g, "?", 40, 30, 34, 34, 24, Colours::ink, true,
         juce::Justification::centred);

    text(g, card.title, 86, 28, 560, 38, 26, Colours::text, true);
    text(g, utf8("press any button to close"), 520, 30, 240, 20, 12,
         Colours::muted, false, juce::Justification::centredRight);

    float y = 74.0f;
    if (card.about.isNotEmpty()) {
        text(g, card.about, 40, y, 720, 50, 15, Colours::muted);
        y += 58.0f;
    }

    // the rows, in one column, or two when there are many
    const int count = int(card.rows.size());
    const bool twoColumns = count > 7;
    const int perColumn = twoColumns ? (count + 1) / 2 : count;
    const float bottom = card.tip.isNotEmpty() ? 396.0f : 446.0f;
    const float rowHeight =
        perColumn > 0
            ? juce::jlimit(26.0f, 36.0f, (bottom - y) / float(perColumn))
            : 30.0f;
    for (int i = 0; i < count; i++) {
        int column = twoColumns && i >= perColumn ? 1 : 0;
        int row = i - column * perColumn;
        float x = 40.0f + column * 362.0f;
        float rowY = y + row * rowHeight;
        float width = twoColumns ? 340.0f : 720.0f;
        drawControl(g, card.rows[size_t(i)].control, x, rowY, rowHeight);
        text(g, card.rows[size_t(i)].text, x + 104.0f, rowY, width - 104.0f,
             rowHeight, twoColumns ? 13.5f : 15.0f, Colours::text);
    }

    if (card.tip.isNotEmpty()) {
        juce::Rectangle<float> tipBox(36, 404, 728, 48);
        g.setColour(Colours::dim);
        g.fillRoundedRectangle(tipBox, 14.0f);
        text(g, "TIP", 46, 404, 34, 48, 13, Colours::yellow, true,
             juce::Justification::centred);
        text(g, card.tip, 80, 406, 676, 44, 14, Colours::text);
    }
}
