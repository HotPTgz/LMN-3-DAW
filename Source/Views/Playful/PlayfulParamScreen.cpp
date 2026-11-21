#include "PlayfulParamScreen.h"

using namespace Playful;

//==============================================================================
PlayfulScreen::PlayfulScreen()
    : startTime(juce::Time::getMillisecondCounterHiRes()),
      animate(animationsEnabled()) {
    setOpaque(true);
    setRate(animate ? 30 : 10);
}

PlayfulScreen::~PlayfulScreen() { stopTimer(); }

double PlayfulScreen::getAnimationTime() const {
    // a fixed pose when animations are off
    if (!animate)
        return 1.3;
    return (juce::Time::getMillisecondCounterHiRes() - startTime) / 1000.0;
}

void PlayfulScreen::setRate(int hz) {
    if (hz != currentRate) {
        currentRate = hz;
        startTimerHz(hz);
    }
}

void PlayfulScreen::timerCallback() {
    if (!isShowing()) {
        // check back now and then; costs nothing while hidden
        setRate(4);
        return;
    }

    if (animate || wantsSmoothFrames()) {
        setRate(30);
        repaint();
        return;
    }

    setRate(10);
    auto signature = getChangeSignature();
    if (signature != lastSignature) {
        lastSignature = signature;
        repaint();
    }
}

juce::String PlayfulScreen::getChangeSignature() {
    juce::String s = getSubtitle() + "|" + getPageLabel();
    for (auto &k : getKnobs())
        s << "|" << k.value;
    return s;
}

void PlayfulScreen::paint(juce::Graphics &g) {
    double time = getAnimationTime();
    paintFrame(
        g, getLocalBounds().toFloat(), getTitle(), getSubtitle(),
        getPageLabel(), getKnobs(), stageShowsStars(), time,
        [&](juce::Graphics &stage) { paintStage(stage, time); },
        hasFlexibleStage());
}

//==============================================================================
PlayfulParamScreen::PlayfulParamScreen(app_services::MidiCommandManager &mcm,
                                       juce::String t,
                                       std::vector<PlayfulParam> p,
                                       std::unique_ptr<PlayfulScene> s,
                                       std::shared_ptr<void> k)
    : midiCommandManager(mcm), title(std::move(t)), params(std::move(p)),
      scene(std::move(s)), keepAlive(std::move(k)) {
    midiCommandManager.addListener(this);
    if (scene != nullptr)
        scene->pageChanged(0);
}

juce::String PlayfulParamScreen::getTitle() {
    if (scene != nullptr) {
        auto sceneTitle = scene->getTitle();
        if (sceneTitle.isNotEmpty())
            return sceneTitle;
    }
    return title;
}

bool PlayfulParamScreen::hasEnabled(int from, int to) const {
    for (int i = from; i < juce::jmin(to, int(params.size())); i++)
        if (params[size_t(i)].isEnabled())
            return true;
    return false;
}

void PlayfulParamScreen::setPage(int newPage) {
    page = newPage;
    if (scene != nullptr)
        scene->pageChanged(page);
    repaint();
}

PlayfulParamScreen::~PlayfulParamScreen() {
    midiCommandManager.removeListener(this);
}

int PlayfulParamScreen::getNumPages() const {
    return juce::jmax(1, (int(params.size()) + 7) / 8);
}

int PlayfulParamScreen::indexFor(int encoder) const {
    return page * 8 + (midiCommandManager.isControlDown ? 4 : 0) + encoder;
}

void PlayfulParamScreen::nudge(int encoder, int direction) {
    int index = indexFor(encoder);
    if (index >= 0 && index < int(params.size()) &&
        params[size_t(index)].isEnabled())
        params[size_t(index)].nudge(direction);
    repaint();
}

void PlayfulParamScreen::plusButtonReleased() {
    if (isShowing() && page < getNumPages() - 1)
        setPage(page + 1);
}

void PlayfulParamScreen::minusButtonReleased() {
    if (isShowing() && page > 0)
        setPage(page - 1);
}

void PlayfulParamScreen::noteOnPressed(int noteNumber) {
    if (scene != nullptr)
        scene->notePlayed(noteNumber);
    repaint();
}

juce::String PlayfulParamScreen::getSubtitle() {
    return scene != nullptr ? scene->getSubtitle() : juce::String();
}

juce::String PlayfulParamScreen::getPageLabel() {
    juce::String label;
    if (getNumPages() > 1)
        label = juce::String(page + 1) + "/" + juce::String(getNumPages());
    bool hasControlLayer = hasEnabled(page * 8 + 4, page * 8 + 8);
    if (hasControlLayer) {
        if (midiCommandManager.isControlDown)
            label += label.isEmpty() ? "ctrl" : " ctrl";
        else
            label += label.isEmpty() ? "ctrl: more" : utf8(" · ctrl: more");
    }
    return label;
}

std::array<KnobInfo, 4> PlayfulParamScreen::getKnobs() {
    std::array<KnobInfo, 4> knobs;
    for (int i = 0; i < 4; i++) {
        int index = indexFor(i);
        auto &k = knobs[size_t(i)];
        k.colour = Colours::encoder(i);
        if (index < int(params.size()) && params[size_t(index)].isEnabled()) {
            auto &p = params[size_t(index)];
            k.label = p.label;
            k.value = p.text ? p.text() : juce::String();
            k.normalised = p.normalised ? p.normalised() : 0.0f;
            if (!p.colour.isTransparent())
                k.colour = p.colour;
        } else {
            k.enabled = false;
        }
    }
    return knobs;
}

void PlayfulParamScreen::paintStage(juce::Graphics &g, double time) {
    if (scene != nullptr)
        scene->paint(g, time);
}

bool PlayfulParamScreen::stageShowsStars() {
    return scene == nullptr || scene->showsStars();
}

std::array<juce::String, 8> PlayfulParamScreen::getPageLabels() const {
    std::array<juce::String, 8> labels;
    for (int i = 0; i < 8; i++) {
        size_t index = size_t(page * 8 + i);
        if (index < params.size() && params[index].isEnabled())
            labels[size_t(i)] = params[index].label;
    }
    return labels;
}
