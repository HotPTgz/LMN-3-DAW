#pragma once
#include "AppLookAndFeel.h"
#include <app_services/app_services.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Small chooser shown by Control + Save.
//   encoder 1 turn:  choose Mix / Stems
//   encoder 1 press: render
//   stop or Control + Save: cancel
// It takes the MIDI focus while open and gives it back when closed.
class RenderMenuComponent : public juce::Component,
                            public app_services::MidiCommandManager::Listener,
                            private juce::Timer {
  public:
    explicit RenderMenuComponent(app_services::MidiCommandManager &mcm);
    ~RenderMenuComponent() override;

    void show(bool isLoopRender);
    void close();
    bool isOpen() const { return isVisible(); }

    // Called with the chosen item index (0 = mix, 1 = stems)
    std::function<void(int)> onRender;

    void paint(juce::Graphics &) override;

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder1ButtonReleased() override;
    void stopButtonReleased() override;

  private:
    app_services::MidiCommandManager &midiCommandManager;
    juce::Component *previouslyFocused = nullptr;
    juce::StringArray items{"Mix", "Stems"};
    int selectedIndex = 0;
    bool loopRender = false;
    AppLookAndFeel appLookAndFeel;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RenderMenuComponent)
};
