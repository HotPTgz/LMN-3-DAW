#include "PlayfulSamplerView.h"
#include "PlayfulScenes.h"

using namespace Playful;
using juce::AffineTransform;
using juce::Graphics;
using juce::String;

PlayfulSamplerView::PlayfulSamplerView(tracktion::SamplerPlugin *sampler,
                                       app_services::MidiCommandManager &mcm)
    : samplerPlugin(sampler), midiCommandManager(mcm),
      viewModel(
          std::make_unique<app_view_models::SynthSamplerViewModel>(sampler)),
      isDrum(false) {
    viewModel->addListener(this);
    midiCommandManager.addListener(this);
}

PlayfulSamplerView::PlayfulSamplerView(
    internal_plugins::DrumSamplerPlugin *drumSampler,
    app_services::MidiCommandManager &mcm)
    : samplerPlugin(drumSampler), midiCommandManager(mcm),
      viewModel(
          std::make_unique<app_view_models::DrumSamplerViewModel>(drumSampler)),
      isDrum(true) {
    viewModel->addListener(this);
    midiCommandManager.addListener(this);
}

PlayfulSamplerView::~PlayfulSamplerView() {
    viewModel->removeListener(this);
    midiCommandManager.removeListener(this);
}

bool PlayfulSamplerView::isActive() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

//==============================================================================
void PlayfulSamplerView::encoder1Increased() {
    if (isActive() && browsing && !midiCommandManager.isControlDown) {
        viewModel->increaseSelectedIndex();
        repaint();
    }
}

void PlayfulSamplerView::encoder1Decreased() {
    if (isActive() && browsing && !midiCommandManager.isControlDown) {
        viewModel->decreaseSelectedIndex();
        repaint();
    }
}

void PlayfulSamplerView::encoder1ButtonReleased() {
    if (!isActive() || viewModel->getItemNames().size() <= 0)
        return;
    if (browsing) {
        if (viewModel->isDir())
            viewModel->enterDir();
        else
            browsing = false;
    } else {
        browsing = true;
    }
    repaint();
}

void PlayfulSamplerView::encoder2Increased() {
    if (isActive())
        viewModel->increaseStartTime();
}

void PlayfulSamplerView::encoder2Decreased() {
    if (isActive())
        viewModel->decreaseStartTime();
}

void PlayfulSamplerView::encoder3Increased() {
    if (isActive())
        viewModel->increaseEndTime();
}

void PlayfulSamplerView::encoder3Decreased() {
    if (isActive())
        viewModel->decreaseEndTime();
}

void PlayfulSamplerView::encoder4Increased() {
    if (isActive() && midiCommandManager.isControlDown)
        viewModel->increaseGain();
}

void PlayfulSamplerView::encoder4Decreased() {
    if (isActive() && midiCommandManager.isControlDown)
        viewModel->decreaseGain();
}

void PlayfulSamplerView::noteOnPressed(int noteNumber) {
    if (isActive()) {
        viewModel->setSelectedSoundIndex(noteNumber);
        repaint();
    }
}

//==============================================================================
double PlayfulSamplerView::getLength() {
    return viewModel->getFullSampleThumbnail().getTotalLength();
}

juce::Array<float> PlayfulSamplerView::getBars(int numBars) {
    juce::Array<float> bars;
    auto &thumb = viewModel->getFullSampleThumbnail();
    double length = getLength();
    float loudest = 0.0f;
    for (int i = 0; i < numBars; i++) {
        float peak = 0.0f;
        if (length > 0 && thumb.getNumChannels() > 0) {
            float low = 0, high = 0;
            thumb.getApproximateMinMax(
                length * i / numBars, length * (i + 1) / numBars, 0, low, high);
            peak = juce::jmax(std::abs(low), std::abs(high));
        }
        loudest = juce::jmax(loudest, peak);
        bars.add(peak);
    }
    if (loudest > 0)
        for (auto &b : bars)
            b /= loudest;
    return bars;
}

String PlayfulSamplerView::getGainText() {
    int gain = int(std::floor(viewModel->getGain()));
    return (gain > 0 ? "+" : "") + String(gain) + " dB";
}

String PlayfulSamplerView::getTitle() {
    return isDrum ? "Drum Sampler" : "Sampler";
}

String PlayfulSamplerView::getSubtitle() {
    if (isDrum)
        return "pad " + String(viewModel->getSelectedSoundIndex() + 1) +
               " of 24";
    return viewModel->getSelectedItemName();
}

String PlayfulSamplerView::getPageLabel() {
    if (viewModel->getItemNames().size() <= 0)
        return {};
    return browsing ? "press 1 to load" : "press 1 to browse";
}

std::array<KnobInfo, 4> PlayfulSamplerView::getKnobs() {
    std::array<KnobInfo, 4> knobs;
    for (int i = 0; i < 4; i++)
        knobs[size_t(i)].colour = Colours::encoder(i);

    auto names = viewModel->getItemNames();
    int selected = viewModel->itemListState.getSelectedItemIndex();
    knobs[0].label = isDrum ? "KIT" : "SAMPLE";
    knobs[0].value = names[selected];
    knobs[0].normalised =
        names.size() > 1 ? float(selected) / (names.size() - 1) : 0.0f;
    knobs[0].enabled = names.size() > 0;

    double length = getLength();
    knobs[1].label = "START";
    knobs[1].value = String(viewModel->getStartTime(), 2) + " s";
    knobs[1].normalised =
        length > 0 ? float(viewModel->getStartTime() / length) : 0.0f;
    knobs[2].label = "END";
    knobs[2].value = String(viewModel->getEndTime(), 2) + " s";
    knobs[2].normalised =
        length > 0 ? float(viewModel->getEndTime() / length) : 0.0f;
    knobs[3].label =
        midiCommandManager.isControlDown ? String("GAIN") : utf8("GAIN · CTRL");
    knobs[3].value = getGainText();
    knobs[3].normalised =
        juce::jlimit(0.0f, 1.0f, float(viewModel->getGain() + 24) / 48.0f);
    return knobs;
}

//==============================================================================
void PlayfulSamplerView::paintStage(Graphics &g, double time) {
    if (viewModel->getItemNames().size() <= 0) {
        g.setColour(Colours::cream);
        fillEllipseCentred(g, 400, 120, 70, 56 - 2 * pingPong(time, 3.0));
        drawFace(g, 400, 124, 1.6f, Mood::sleep, time);
        PlayfulScenes::text(g, "z", 470 + 6 * pingPong(time, 2.0), 70, 22,
                            Colours::muted);
        PlayfulScenes::text(g, "No samples yet", 400, 210, 26, Colours::text,
                            true, juce::Justification::horizontallyCentred);
        PlayfulScenes::text(
            g, "See the README for adding samples and drum kits", 400, 245, 16,
            Colours::muted, false, juce::Justification::horizontallyCentred);
        return;
    }

    if (isDrum)
        paintDrum(g, time);
    else
        paintSynth(g, time);
}

void PlayfulSamplerView::paintList(Graphics &g, float x, float y, float width,
                                   int rows) {
    auto names = viewModel->getItemNames();
    int selected = viewModel->itemListState.getSelectedItemIndex();
    int first = juce::jlimit(0, juce::jmax(0, names.size() - rows),
                             selected - rows / 2);
    for (int row = 0; row < rows && first + row < names.size(); row++) {
        int index = first + row;
        bool isSelected = index == selected;
        float ry = y + row * 46;
        g.setColour(isSelected
                        ? Colours::blue
                        : Colours::panel.withAlpha(browsing ? 1.0f : 0.6f));
        g.fillRoundedRectangle(x, ry, width, 40, 14);
        g.setColour(isSelected
                        ? Colours::ink
                        : Colours::text.withAlpha(browsing ? 1.0f : 0.5f));
        g.setFont(font(15, isSelected));
        g.drawText(names[index],
                   juce::Rectangle<float>(x + 16, ry, width - 24, 40),
                   juce::Justification::centredLeft, true);
    }
}

void PlayfulSamplerView::paintSynth(Graphics &g, double time) {
    using PlayfulScenes::strokePath;
    using PlayfulScenes::svg;

    paintList(g, 24, 20, 210, 6);

    double length = getLength();
    float start = length > 0 ? float(viewModel->getStartTime() / length) : 0;
    float end = length > 0 ? float(viewModel->getEndTime() / length) : 1;

    Graphics::ScopedSaveState s(g);
    g.addTransform(AffineTransform::translation(260, 30));
    g.setColour(Colours::panel);
    g.fillRoundedRectangle(0, 0, 510, 230, 28);

    float selX = 8 + start * 492, selX2 = 8 + end * 492;
    g.setColour(Colours::green.withAlpha(0.14f));
    g.fillRoundedRectangle(selX, 10, juce::jmax(0.0f, selX2 - selX), 210, 18);

    auto bars = getBars(60);
    for (int j = 0; j < bars.size(); j++) {
        float u = j / 60.0f;
        float h = juce::jmax(6.0f, bars[j] * 200);
        bool inSelection = u >= start && u <= end;
        g.setColour(inSelection ? Colours::cream : Colours::greyKey);
        g.fillRoundedRectangle(8 + j * 8.2f, 115 - h / 2, 5, h, 2.5f);
    }

    // crab claws pinch the start and end
    float pinch = 2.0f * pingPong(time, 1.2);
    g.setColour(Colours::green);
    g.fillRect(selX - 2, 10.0f, 4.0f, 210.0f);
    g.setColour(Colours::cream);
    g.fillRect(selX2 - 2, 10.0f, 4.0f, 210.0f);
    g.setColour(Colours::red);
    g.fillPath(svg("M-18 0 Q-24 -16 -8 -18 L-2 -8 Q-10 -6 -8 2 Z"),
               AffineTransform::rotation(-0.1f * pinch).translated(selX, 236));
    g.fillPath(svg("M18 0 Q24 -16 8 -18 L2 -8 Q10 -6 8 2 Z"),
               AffineTransform::rotation(0.1f * pinch).translated(selX2, 236));

    // the crab
    float crabX = 255 + 12.0f * std::sin(float(time) * 1.3f);
    g.addTransform(AffineTransform::translation(crabX, 262));
    g.setColour(Colours::red);
    fillEllipseCentred(g, 0, 0, 40, 20);
    strokePath(
        g, svg("M-36 6 L-46 18 M36 6 L46 18 M-26 12 L-32 24 M26 12 L32 24"), 4);
    g.strokePath(svg("M-4 -18 L-8 -32 M4 -18 L8 -32"), juce::PathStrokeType(3));
    drawFace(g, 0, -36, 0.5f, Mood::smile, time);
}

void PlayfulSamplerView::paintDrum(Graphics &g, double time) {
    int selected = viewModel->getSelectedSoundIndex();
    static const juce::Colour padColours[] = {Colours::pink,   Colours::yellow,
                                              Colours::green,  Colours::blue,
                                              Colours::purple, Colours::orange};

    for (int i = 0; i < 24; i++) {
        int r = i / 6, c = i % 6;
        bool isSelected = i == selected;
        float scale = isSelected ? 1.0f + 0.08f * pingPong(time, 0.9) : 1.0f;
        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::scale(scale).translated(
            60.0f + c * 82, 58.0f + r * 72));
        auto pad = juce::Rectangle<float>(-26, -26, 52, 52);
        bool hasSound = i < samplerPlugin->getNumSounds();
        g.setColour(hasSound ? padColours[(r + c) % 6] : Colours::dim);
        g.fillRoundedRectangle(pad, 18);
        g.setColour(isSelected ? Colours::cream : Colours::ink);
        g.drawRoundedRectangle(pad, 18, isSelected ? 5.0f : 2.0f);
        g.setColour(Colours::ink);
        fillEllipseCentred(g, -8, -2, 3, 3);
        fillEllipseCentred(g, 8, -2, 3, 3);
        juce::Path mouth;
        mouth.startNewSubPath(-5, 8);
        mouth.quadraticTo(0, 12, 5, 8);
        PlayfulScenes::strokePath(g, mouth, 2);
    }

    {
        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(560, 30));
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(0, 0, 210, 270, 28);
        auto names = viewModel->getItemNames();
        g.setColour(Colours::muted);
        g.setFont(font(13));
        g.drawText(names[viewModel->itemListState.getSelectedItemIndex()],
                   juce::Rectangle<float>(10, 24, 190, 22),
                   juce::Justification::centred, true);
        {
            Graphics::ScopedSaveState p(g);
            g.addTransform(AffineTransform::translation(
                105, 108 - 8 * pingPong(time, 2.6)));
            g.setColour(padColours[(selected / 6 + selected % 6) % 6]);
            g.fillRoundedRectangle(-46, -46, 92, 92, 30);
            drawFace(g, 0, -4, 1.1f, Mood::smile, time);
        }
        g.setColour(Colours::text);
        g.setFont(font(20, true));
        g.drawFittedText(viewModel->getSelectedItemName(), 10, 172, 190, 28,
                         juce::Justification::centred, 1, 0.7f);

        double length = getLength();
        float start =
            length > 0 ? float(viewModel->getStartTime() / length) : 0;
        float end = length > 0 ? float(viewModel->getEndTime() / length) : 1;
        auto bars = getBars(30);
        for (int j = 0; j < bars.size(); j++) {
            float u = j / 30.0f;
            float h = juce::jmax(4.0f, bars[j] * 50);
            g.setColour(u >= start && u <= end ? Colours::cream
                                               : Colours::greyKey);
            g.fillRoundedRectangle(30 + j * 5.2f, 232 - h / 2, 4, h, 2);
        }
    }

    if (browsing) {
        g.setColour(Colours::background.withAlpha(0.85f));
        g.fillRoundedRectangle(14, 10, 516, 300, 24);
        PlayfulScenes::text(g, viewModel->getTitle(), 34, 40, 18, Colours::text,
                            true);
        paintList(g, 34, 56, 476, 5);
    }
}
