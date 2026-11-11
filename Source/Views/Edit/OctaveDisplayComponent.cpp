#include "OctaveDisplayComponent.h"
#include "PlayfulApp.h"

using namespace Playful;

OctaveDisplayComponent::OctaveDisplayComponent() {}

void OctaveDisplayComponent::paint(juce::Graphics &g) {
    auto bounds = getLocalBounds().toFloat();
    float h = bounds.getHeight();
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(bounds, h / 2.0f);

    // a tiny keyboard
    float keyW = h * 0.12f, keyH = h * 0.5f;
    float kx = bounds.getX() + h * 0.35f, ky = bounds.getCentreY() - keyH / 2;
    for (int i = 0; i < 4; i++) {
        g.setColour(Colours::cream);
        g.fillRoundedRectangle(kx + i * (keyW + 2.0f), ky, keyW, keyH, 2.0f);
    }
    g.setColour(Colours::ink);
    for (int i : {0, 1})
        g.fillRoundedRectangle(kx + keyW * 0.65f + i * (keyW + 2.0f), ky,
                               keyW * 0.7f, keyH * 0.6f, 1.5f);

    juce::String text = (octaveInt >= 0 ? "+" : "") + juce::String(octaveInt);
    g.setColour(octaveInt >= 0 ? Colours::green : Colours::red);
    g.setFont(font(h * 0.6f, true));
    float textX = kx + 4 * (keyW + 2.0f) + h * 0.15f;
    g.drawText(text,
               juce::Rectangle<float>(textX, bounds.getY(),
                                      bounds.getRight() - textX - h * 0.2f, h),
               juce::Justification::centred);
}

void OctaveDisplayComponent::resized() {}

void OctaveDisplayComponent::setOctave(int octave) {
    octaveInt = octave;
    repaint();
}
