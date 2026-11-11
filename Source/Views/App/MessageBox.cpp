#include "MessageBox.h"
#include "PlayfulApp.h"

using namespace Playful;

MessageBox::MessageBox() {
    // the text is painted; the label just holds it
    addChildComponent(messageLabel);
}

MessageBox::~MessageBox() {}

void MessageBox::paint(juce::Graphics &g) {
    auto bounds = getLocalBounds().toFloat();
    float h = bounds.getHeight();
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(bounds, h / 2.0f);
    g.setColour(Colours::yellow);
    fillEllipseCentred(g, bounds.getX() + h * 0.42f, bounds.getCentreY(),
                       h * 0.1f, h * 0.1f);
    g.setColour(Colours::text);
    g.setFont(getFont());
    g.drawText(getMessage(),
               bounds.withTrimmedLeft(h * 0.65f).withTrimmedRight(h * 0.3f),
               juce::Justification::centred, true);
}

void MessageBox::resized() {}

juce::String MessageBox::getMessage() { return messageLabel.getText(); }

void MessageBox::setMessage(const juce::String &message) {
    messageLabel.setText(message, juce::sendNotification);
    repaint();
}

juce::Font MessageBox::getFont() {
    // sized for the box the edit view makes: a sixth of the screen height
    float h = getHeight() > 0 ? getHeight() * 0.36f : 26.0f;
    return font(h, true);
}
