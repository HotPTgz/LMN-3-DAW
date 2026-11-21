#pragma once
#include "PlayfulStyle.h"
#include <app_services/app_services.h>
#include <juce_gui_basics/juce_gui_basics.h>

// The animated picture in the middle of a playful screen. Scenes paint in
// stage coordinates: 800 x 328, origin at the top left of the stage.
class PlayfulScene {
  public:
    virtual ~PlayfulScene() = default;
    virtual juce::String getSubtitle() { return {}; }
    virtual void paint(juce::Graphics &g, double time) = 0;
    // a key was played while the screen is focused
    virtual void notePlayed(int /*noteNumber*/) {}
    virtual bool showsStars() { return true; }
    // for screens with pages: called with the page shown
    virtual void pageChanged(int /*page*/) {}
    // a title for the current page (empty: the screen's own title)
    virtual juce::String getTitle() { return {}; }
    // an encoder (0-3) was pushed down or let go
    virtual void encoderButton(int /*encoder*/, bool /*down*/) {}
};

// One encoder-controlled value on a playful screen
struct PlayfulParam {
    juce::String label;
    std::function<float()> normalised;
    std::function<juce::String()> text;
    std::function<void(int)> nudge; // +1 / -1 per encoder detent
    juce::Colour colour = juce::Colours::transparentBlack; // 0 = encoder

    // a gap in the knob strip
    static PlayfulParam none() { return {}; }
    bool isEnabled() const { return nudge != nullptr; }
};

/**
 * Base for a full playful screen: header, stage and knob strip, drawn in the
 * 800x480 design space and scaled to fit. Runs a timer that animates at
 * ~30 fps while visible (or just watches for value changes when animations
 * are turned off in config.yaml).
 */
class PlayfulScreen : public juce::Component, private juce::Timer {
  public:
    PlayfulScreen();
    ~PlayfulScreen() override;

    void paint(juce::Graphics &g) override;

    // the title shown at the top (used by the help card)
    juce::String getScreenTitle() { return getTitle(); }

  protected:
    virtual juce::String getTitle() = 0;
    virtual juce::String getSubtitle() { return {}; }
    virtual juce::String getPageLabel() { return {}; }
    virtual std::array<Playful::KnobInfo, 4> getKnobs() = 0;
    virtual void paintStage(juce::Graphics &g, double time) = 0;
    virtual bool stageShowsStars() { return true; }
    // true while the screen must redraw every frame even with animations
    // turned off (e.g. a moving playhead)
    virtual bool wantsSmoothFrames() { return false; }

    // something that changes whenever the screen should be redrawn (used
    // when not animating). Defaults to the knob values and subtitle.
    virtual juce::String getChangeSignature();

    double getAnimationTime() const;

  private:
    void timerCallback() override;
    juce::String lastSignature;
    double startTime;
    bool animate;
    int currentRate = 0;

    void setRate(int hz);
};

/**
 * A playful screen for any knob-based plugin or modifier: up to 4 values per
 * encoder layer, 4 more while Control is held, and further pages on +/-.
 */
class PlayfulParamScreen : public PlayfulScreen,
                           public app_services::MidiCommandManager::Listener {
  public:
    PlayfulParamScreen(app_services::MidiCommandManager &mcm,
                       juce::String title, std::vector<PlayfulParam> params,
                       std::unique_ptr<PlayfulScene> scene,
                       std::shared_ptr<void> keepAlive = {});
    ~PlayfulParamScreen() override;

    void encoder1Increased() override { nudge(0, 1); }
    void encoder1Decreased() override { nudge(0, -1); }
    void encoder2Increased() override { nudge(1, 1); }
    void encoder2Decreased() override { nudge(1, -1); }
    void encoder3Increased() override { nudge(2, 1); }
    void encoder3Decreased() override { nudge(2, -1); }
    void encoder4Increased() override { nudge(3, 1); }
    void encoder4Decreased() override { nudge(3, -1); }
    void encoder1ButtonPressed() override { encoderButton(0, true); }
    void encoder1ButtonReleased() override { encoderButton(0, false); }
    void encoder2ButtonPressed() override { encoderButton(1, true); }
    void encoder2ButtonReleased() override { encoderButton(1, false); }
    void encoder3ButtonPressed() override { encoderButton(2, true); }
    void encoder3ButtonReleased() override { encoderButton(2, false); }
    void encoder4ButtonPressed() override { encoderButton(3, true); }
    void encoder4ButtonReleased() override { encoderButton(3, false); }

    void controlButtonPressed() override { repaint(); }
    void controlButtonReleased() override { repaint(); }
    void plusButtonReleased() override;
    void minusButtonReleased() override;
    void noteOnPressed(int noteNumber) override;

    int getPage() const { return page; }
    void setPage(int newPage);
    int getNumPages() const;

    // the plugin or modifier name the screen was made with
    const juce::String &getBaseTitle() const { return title; }
    // the knob labels on the current page: encoder 1-4, then with Control
    std::array<juce::String, 8> getPageLabels() const;

  protected:
    juce::String getTitle() override;
    juce::String getSubtitle() override;
    juce::String getPageLabel() override;
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    bool stageShowsStars() override;

  private:
    app_services::MidiCommandManager &midiCommandManager;
    juce::String title;
    std::vector<PlayfulParam> params;
    std::unique_ptr<PlayfulScene> scene;
    std::shared_ptr<void> keepAlive; // e.g. the view model the params use
    int page = 0;

    int indexFor(int encoder) const;
    bool hasEnabled(int from, int to) const;
    void nudge(int encoder, int direction);
    void encoderButton(int encoder, bool down) {
        if (scene != nullptr)
            scene->encoderButton(encoder, down);
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayfulParamScreen)
};
