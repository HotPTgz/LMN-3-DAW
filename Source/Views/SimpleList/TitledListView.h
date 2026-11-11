#pragma once
#include "ListTitle.h"
#include "PlayfulParamScreen.h"
#include "SimpleListView.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Extra things a list screen can show for each item
struct ListItemInfo {
    juce::String name;         // shown instead of the list item text
    juce::String detail;       // right-aligned text on the card
    juce::String kind;         // chain layout: the label above the card
    juce::String target;       // linked layout: what the item is wired to
    juce::String targetDetail; // linked layout: second line of the target
    bool off = false;          // disabled / bypassed
    bool marked = false;       // grid layout: the open project
};

/**
 * A playful list screen: header, a list of candy cards, an optional little
 * character on the right and the knob strip. The (invisible) ListBox inside
 * still holds the selection, so the views that own one keep driving it the
 * same way as before.
 */
class TitledListView : public PlayfulScreen {
  public:
    enum class Layout {
        list,   // cards, one per row
        linked, // cards with a dashed line to what each one is wired to
        chain,  // cards in a row, left to right (the plugin chain)
        grid    // big cards, three to a row (projects)
    };

    enum class Mascot { none, cat, bee, blob, ball, robot };

    TitledListView(const juce::StringArray &listItems,
                   const juce::String &titleString, ListTitle::IconType type,
                   const juce::String &iconString);

    void resized() override;

    void setTitleString(juce::String newTitle);
    SimpleListView &getListView();
    void setListItems(const juce::StringArray &listItems);

    // playful extras
    void setSubtitle(const juce::String &s) { subtitle = s; }
    void setHint(const juce::String &h) { hint = h; }
    void setLayout(Layout l) { layout = l; }
    void setItemInfo(std::function<ListItemInfo(int)> f) {
        itemInfo = std::move(f);
    }
    void setMascot(Mascot m, std::function<juce::String()> caption = {},
                   std::function<juce::String()> subCaption = {});
    // the "1 modifier · 2 plugin · 3 knob" steps; -1 hides them
    void setStep(int s) { step = s; }
    void setEmptyMessage(const juce::String &m) { emptyMessage = m; }
    // what encoder 1 does, e.g. "SETTING · OPEN"
    void setPrimaryLabel(const juce::String &l) { primaryLabel = l; }
    void setKnobs(std::function<std::array<Playful::KnobInfo, 4>()> f) {
        knobs = std::move(f);
    }

    int getSelectedRow();
    int getNumItems();
    juce::String getSelectedItemName();

  protected:
    juce::String getTitle() override { return title; }
    juce::String getSubtitle() override { return subtitle; }
    juce::String getPageLabel() override { return hint; }
    std::array<Playful::KnobInfo, 4> getKnobs() override;
    void paintStage(juce::Graphics &g, double time) override;
    juce::String getChangeSignature() override;

  private:
    SimpleListView listView;
    juce::String title, subtitle, hint, emptyMessage, primaryLabel{"PICK"};
    Layout layout = Layout::list;
    Mascot mascot = Mascot::none;
    std::function<juce::String()> mascotCaption, mascotSubCaption;
    std::function<ListItemInfo(int)> itemInfo;
    std::function<std::array<Playful::KnobInfo, 4>()> knobs;
    int step = -1;

    ListItemInfo infoFor(int index);
    juce::String nameFor(int index, const ListItemInfo &info);
    void paintList(juce::Graphics &g, double time, float top, float width);
    void paintLinked(juce::Graphics &g, double time);
    void paintChain(juce::Graphics &g, double time);
    void paintGrid(juce::Graphics &g, double time);
    void paintSteps(juce::Graphics &g);
    void paintMascot(juce::Graphics &g, double time, float cx, float cy);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitledListView)
};

// Paints a playful character: shared by the list screens
void paintListMascot(juce::Graphics &g, TitledListView::Mascot mascot, float cx,
                     float cy, double time, juce::Colour colour);
