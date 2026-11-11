#include "TitledSplitListView.h"
#include "PlayfulApp.h"

using namespace Playful;

TitledSplitListView::TitledSplitListView(
    const juce::StringArray &leftListItems,
    const juce::StringArray &rightListItems, const juce::String &titleString,
    ListTitle::IconType, const juce::String &)
    : splitListView(leftListItems, rightListItems), title(titleString) {
    // only keeps the selection; everything is painted here
    addChildComponent(splitListView);
}

void TitledSplitListView::resized() {
    splitListView.setBounds(getLocalBounds());
    int rowHeight = juce::jmax(1, getHeight() / 6);
    splitListView.getLeftListView().getListBox().setRowHeight(rowHeight);
    splitListView.getRightListView().getListBox().setRowHeight(rowHeight);
}

SplitListView &TitledSplitListView::getSplitListView() { return splitListView; }

int TitledSplitListView::selectedRow(SimpleListView &list) {
    int n = list.getListItems().size();
    if (n == 0)
        return -1;
    return juce::jlimit(0, n - 1, list.getListBox().getSelectedRow());
}

juce::String TitledSplitListView::getSubtitle() {
    if (subtitle.isNotEmpty())
        return subtitle;
    auto &left = splitListView.getLeftListView();
    int row = selectedRow(left);
    return row >= 0 ? left.getListItems()[row].toLowerCase() : juce::String();
}

std::array<KnobInfo, 4> TitledSplitListView::getKnobs() {
    auto &left = splitListView.getLeftListView();
    auto &right = splitListView.getRightListView();
    int l = selectedRow(left), r = selectedRow(right);
    int nl = left.getListItems().size(), nr = right.getListItems().size();
    std::array<KnobInfo, 4> k;
    k[0] = {leftLabel, l >= 0 ? left.getListItems()[l] : juce::String(),
            nl > 1 ? float(l) / float(nl - 1) : 1.0f, Colours::blue, nl > 0};
    k[1] = {rightLabel, r >= 0 ? right.getListItems()[r] : juce::String(),
            nr > 1 ? float(r) / float(nr - 1) : 1.0f, Colours::green, nr > 0};
    k[2].enabled = false;
    k[3].enabled = false;
    return k;
}

juce::String TitledSplitListView::getChangeSignature() {
    auto &left = splitListView.getLeftListView();
    auto &right = splitListView.getRightListView();
    return PlayfulScreen::getChangeSignature() + "|" +
           juce::String(selectedRow(left)) + "|" +
           juce::String(selectedRow(right)) + "|" +
           right.getListItems().joinIntoString(",");
}

void TitledSplitListView::paintStage(juce::Graphics &g, double time) {
    auto &left = splitListView.getLeftListView();
    auto &right = splitListView.getRightListView();
    int l = selectedRow(left), r = selectedRow(right);
    const auto &leftItems = left.getListItems();
    const auto &rightItems = right.getListItems();

    // categories
    for (int i = 0; i < juce::jmin(6, leftItems.size()); i++) {
        float y = 14.0f + i * 46.0f;
        bool selected = i == l;
        g.setColour(selected ? Colours::yellow : Colours::panel);
        g.fillRoundedRectangle(24.0f, y, 150.0f, 40.0f, 14.0f);
        drawText(g, leftItems[i], 40.0f, y, 120.0f, 40.0f, 15.0f,
                 selected ? Colours::ink : Colours::text, true,
                 juce::Justification::centredLeft);
    }

    // items
    int n = rightItems.size();
    const int visible = 6;
    int first = juce::jlimit(0, juce::jmax(0, n - visible), r - visible / 2);
    for (int i = first; i < juce::jmin(n, first + visible); i++) {
        float y = 14.0f + (i - first) * 46.0f;
        drawCard(g, {194.0f, y, 300.0f, 40.0f}, rightItems[i], {},
                 Colours::candy(i), i == r, false, time);
    }
    if (n > visible)
        drawText(g, juce::String(r + 1) + " / " + juce::String(n), 194.0f,
                 296.0f, 300.0f, 20.0f, 12.0f, Colours::muted);

    // the chosen one, waving
    if (r >= 0) {
        drawBallFriend(g, 650.0f, 130.0f, 46.0f, Colours::candy(r), time);
        drawText(g, rightItems[r], 540.0f, 204.0f, 220.0f, 24.0f, 17.0f,
                 Colours::text, true);
        if (caption)
            drawText(g, caption(), 540.0f, 228.0f, 220.0f, 18.0f, 12.0f,
                     Colours::muted);
    }
}
