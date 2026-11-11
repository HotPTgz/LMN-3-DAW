#pragma once
#include "ListTitle.h"
#include "PlayfulParamScreen.h"
#include "SplitListView.h"
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Playful two-column chooser (categories on the left, items on the right)
 * with a little character showing the chosen item. The invisible split list
 * inside keeps the selection, as before.
 */
class TitledSplitListView : public PlayfulScreen {
  public:
    TitledSplitListView(const juce::StringArray &leftListItems,
                        const juce::StringArray &rightListItems,
                        const juce::String &titleString,
                        ListTitle::IconType type,
                        const juce::String &iconString);
    void resized() override;

    SplitListView &getSplitListView();

    void setSubtitle(const juce::String &s) { subtitle = s; }
    void setHint(const juce::String &h) { hint = h; }
    // labels for encoders 1 and 2
    void setLabels(const juce::String &left, const juce::String &right) {
        leftLabel = left;
        rightLabel = right;
    }
    // shown under the character
    void setCaption(std::function<juce::String()> f) { caption = std::move(f); }

  protected:
    juce::String getTitle() override { return title; }
    juce::String getSubtitle() override;
    juce::String getPageLabel() override { return hint; }
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    juce::String getChangeSignature() override;

  private:
    SplitListView splitListView;
    juce::String title, subtitle, hint, leftLabel{"CATEGORY"},
        rightLabel{"PICK"};
    std::function<juce::String()> caption;

    static int selectedRow(SimpleListView &list);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitledSplitListView)
};
