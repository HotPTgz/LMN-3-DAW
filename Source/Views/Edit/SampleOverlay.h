#pragma once
#include "LiveControls.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Full screen shown while Control + MIC samples a sound, and while the
// sound waits for a drum pad: a singing microphone friend whose rings grow
// with the level, and a countdown to the 20 s limit.
class SampleOverlay : public juce::Component {
  public:
    explicit SampleOverlay(LiveControls &controls);

    // called when the live controls change: shows, hides and redraws
    void update();

    void paint(juce::Graphics &g) override;

  private:
    LiveControls &controls;
    double shownAt = 0.0;
};
