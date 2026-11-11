#include "AvailableSequencersListView.h"
#include "PlayfulSequencerViews.h"
#include "PlayfulStyle.h"
#include "StepSequencerView.h"
#include <app_navigation/app_navigation.h>

AvailableSequencersListView::AvailableSequencersListView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : track(t), midiCommandManager(mcm), viewModel(t),
      titledList(viewModel.getItemNames(), "Sequencers",
                 ListTitle::IconType::FONT_AWESOME,
                 juce::String::charToString(0xf00a)) {
    viewModel.itemListState.addListener(this);
    midiCommandManager.addListener(this);

    addAndMakeVisible(titledList);

    titledList.setSubtitle(track->getName().toLowerCase());
    titledList.setHint("press 1 to open");
    titledList.setPrimaryLabel(Playful::utf8("SEQUENCER · OPEN"));
    titledList.setMascot(
        TitledListView::Mascot::bee,
        [this]() { return titledList.getSelectedItemName(); },
        [this]() {
            static const juce::StringArray blurbs{
                "notes on a grid, with per-step tricks",
                "one row per kit sound, with fills",
                "play notes in, it loops them", "bouncing balls play the notes",
                "four patterns, chained into a song"};
            return blurbs[titledList.getSelectedRow()];
        });
    titledList.setMascotChooser([this]() {
        using M = TitledListView::Mascot;
        static const M mascots[] = {M::bee, M::drum, M::caterpillar, M::tombola,
                                    M::train};
        int row = juce::jlimit(0, 4, titledList.getSelectedRow());
        return mascots[row];
    });
    titledList.setItemInfo([](int index) {
        static const juce::StringArray details{"grid", "kit", "loop", "balls",
                                               "A B C D"};
        ListItemInfo info;
        info.detail = details[index];
        return info;
    });
}

AvailableSequencersListView::~AvailableSequencersListView() {
    midiCommandManager.removeListener(this);
    viewModel.itemListState.removeListener(this);
}

void AvailableSequencersListView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void AvailableSequencersListView::resized() {
    titledList.setBounds(getLocalBounds());
    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        viewModel.itemListState.getSelectedItemIndex());
}

void AvailableSequencersListView::encoder1Increased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.itemListState.setSelectedItemIndex(
                viewModel.itemListState.getSelectedItemIndex() + 1);
}

void AvailableSequencersListView::encoder1Decreased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.itemListState.setSelectedItemIndex(
                viewModel.itemListState.getSelectedItemIndex() - 1);
}

void AvailableSequencersListView::encoder1ButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                auto name = viewModel.getSelectedItem();
                juce::Component *view = nullptr;
                if (name == "Drums")
                    view = new DrumSequencerView(track, midiCommandManager);
                else if (name == "Endless")
                    view = new EndlessSequencerView(track, midiCommandManager);
                else if (name == "Tombola")
                    view = new TombolaSequencerView(track, midiCommandManager);
                else if (name == "Pattern")
                    view = new PatternSequencerView(track, midiCommandManager);
                else
                    view = new StepSequencerView(track, midiCommandManager);
                stackNavigationController->push(view);
                midiCommandManager.setFocusedComponent(
                    stackNavigationController->getTopComponent());
            }
        }
    }
}

void AvailableSequencersListView::selectedIndexChanged(int newIndex) {
    titledList.getListView().getListBox().selectRow(newIndex);
    sendLookAndFeelChange();
}
