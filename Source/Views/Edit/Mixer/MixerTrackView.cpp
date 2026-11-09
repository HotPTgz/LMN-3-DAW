#include "MixerTrackView.h"
MixerTrackView::MixerTrackView(tracktion::Track::Ptr t)
    : track(t), viewModel(track),
      levelMeter0(
          (track->isMasterTrack())
              ? std::make_unique<LevelMeterComponent>(
                    t->edit.getCurrentPlaybackContext()->masterLevels, 0)
              : std::make_unique<LevelMeterComponent>(
                    track->pluginList
                        .getPluginsOfType<tracktion::LevelMeterPlugin>()
                        .getLast()
                        ->measurer,
                    0)),
      levelMeter1(
          (track->isMasterTrack())
              ? std::make_unique<LevelMeterComponent>(
                    t->edit.getCurrentPlaybackContext()->masterLevels, 1)
              : std::make_unique<LevelMeterComponent>(
                    track->pluginList
                        .getPluginsOfType<tracktion::LevelMeterPlugin>()
                        .getLast()
                        ->measurer,
                    1)) {
    addAndMakeVisible(levelMeter0.get());
    addAndMakeVisible(levelMeter1.get());
    if (track->getName().contains("Track")) {
        panKnob.getLabel().setText(
            track->getName().trimCharactersAtStart("Track "),
            juce::dontSendNotification);
    }

    else if (!track->isMasterTrack()) {
        // e.g. the "FX A" / "FX B" return tracks
        panKnob.getLabel().setText(track->getName(),
                                   juce::dontSendNotification);
    }

    if (track->isMasterTrack()) {
        panKnob.getLabel().setText("M", juce::dontSendNotification);
    }

    sendsLabel.setJustificationType(juce::Justification::centred);
    sendsLabel.setColour(juce::Label::textColourId, appLookAndFeel.colour1);
    sendsLabel.setAlwaysOnTop(true);
    addChildComponent(sendsLabel);

    panKnob.getSlider().setRange(
        viewModel.getVolumeAndPanPlugin()->panParam->getValueRange().getStart(),
        viewModel.getVolumeAndPanPlugin()->panParam->getValueRange().getEnd(),
        0);
    panKnob.getSlider().setColour(juce::Slider::rotarySliderFillColourId,
                                  appLookAndFeel.colour3);
    panKnob.getSlider().setColour(juce::Slider::thumbColourId,
                                  appLookAndFeel.colour3);
    panKnob.getSlider().setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(panKnob);

    volumeSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    volumeSlider.setRange(0, 1, .01);
    volumeSlider.setColour(juce::Slider::textBoxOutlineColourId,
                           appLookAndFeel.redColour);
    volumeSlider.setColour(juce::Slider::backgroundColourId,
                           appLookAndFeel.redColour);
    volumeSlider.setColour(juce::Slider::trackColourId,
                           appLookAndFeel.redColour);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(volumeSlider);

    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.setGap(juce::Grid::Px(2));
    grid.templateRows = {Track(Fr(1))};
    grid.templateColumns = {Track(Fr(2)), Track(Fr(2)), Track(Fr(10)),
                            Track(Fr(1))};

    grid.items.add(levelMeter0.get());
    grid.items.add(levelMeter1.get());
    grid.items.add(panKnob);
    grid.items.add(volumeSlider);

    soloLabel.setFont(fontAwesomeFont);
    soloLabel.setText(juce::String::charToString(0x53),
                      juce::dontSendNotification);
    soloLabel.setJustificationType(juce::Justification::centred);
    soloLabel.setColour(juce::Label::textColourId, appLookAndFeel.colour3);
    soloLabel.setAlwaysOnTop(true);
    addAndMakeVisible(soloLabel);

    muteLabel.setFont(fontAwesomeFont);
    muteLabel.setText(juce::String::charToString(0xf6a9),
                      juce::dontSendNotification);
    muteLabel.setJustificationType(juce::Justification::centred);
    muteLabel.setColour(juce::Label::textColourId, appLookAndFeel.colour4);
    muteLabel.setAlwaysOnTop(true);
    addAndMakeVisible(muteLabel);

    viewModel.addListener(this);
}

MixerTrackView::~MixerTrackView() { viewModel.removeListener(this); }

void MixerTrackView::paint(juce::Graphics &g) {
    if (isSelected) {
        g.setColour(appLookAndFeel.colour1);
        g.drawRect(getLocalBounds(), 2);
    }
}

void MixerTrackView::resized() {
    grid.performLayout(getLocalBounds().reduced(getWidth() * .05f));

    int iconHeight = getHeight() / 4;
    int iconWidth = iconHeight;
    int soloX = (panKnob.getX() + (panKnob.getWidth() / 2)) - iconWidth - 10;
    int muteX = (panKnob.getX() + (panKnob.getWidth() / 2)) + 10 + 2;
    int iconY = panKnob.getLabel().getY() + 5;

    soloLabel.setFont(fontAwesomeFont);
    muteLabel.setFont(fontAwesomeFont);
    soloLabel.setBounds(soloX, iconY, iconWidth, iconHeight);
    muteLabel.setBounds(muteX, iconY, iconWidth, iconHeight);

    int sendsHeight = getHeight() / 10;
    sendsLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                  sendsHeight * .8f, juce::Font::plain));
    sendsLabel.setBounds(panKnob.getX(), getHeight() - sendsHeight - 2,
                         panKnob.getWidth(), sendsHeight);
}

void MixerTrackView::sendLevelsChanged(double sendA, double sendB) {
    // shown only once a send is in use, e.g. "A40 B0"
    bool anySend = sendA > 0.0 || sendB > 0.0;
    sendsLabel.setText("A" + juce::String(juce::roundToInt(sendA * 100)) +
                           " B" + juce::String(juce::roundToInt(sendB * 100)),
                       juce::dontSendNotification);
    sendsLabel.setVisible(anySend);
}

void MixerTrackView::setSelected(bool selected) {
    isSelected = selected;
    if (isSelected)
        panKnob.getLabel().setColour(juce::Label::textColourId,
                                     appLookAndFeel.colour1);
    else
        panKnob.getLabel().setColour(juce::Label::textColourId,
                                     appLookAndFeel.colour3);
}

void MixerTrackView::panChanged(double pan) {
    panKnob.getSlider().setValue(pan, juce::dontSendNotification);
}

void MixerTrackView::volumeChanged(double volume) {
    volumeSlider.setValue(volume, juce::dontSendNotification);
}

void MixerTrackView::soloStateChanged(bool solo) {
    soloLabel.setVisible(solo);
    resized();
}

void MixerTrackView::muteStateChanged(bool mute) {
    muteLabel.setVisible(mute);
    resized();
}
