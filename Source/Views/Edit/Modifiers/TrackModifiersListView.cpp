#include "TrackModifiersListView.h"
#include "AvailableModifiersListView.h"
#include "ModifierView.h"
#include "PlayfulStyle.h"
#include <app_navigation/app_navigation.h>
TrackModifiersListView::TrackModifiersListView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : track(t), midiCommandManager(mcm), viewModel(t),
      titledList(viewModel.listViewModel.getItemNames(), "Modifiers",
                 ListTitle::IconType::FONT_AWESOME,
                 juce::String::charToString(0xf83e)) {
    viewModel.listViewModel.addListener(this);
    viewModel.listViewModel.itemListState.addListener(this);
    midiCommandManager.addListener(this);

    emptyListLabel.setFont(
        juce::Font(juce::Font::getDefaultMonospacedFontName(), getHeight() * .1,
                   juce::Font::plain));
    emptyListLabel.setText("Press + to add a modifier!",
                           juce::dontSendNotification);
    emptyListLabel.setJustificationType(juce::Justification::centred);
    emptyListLabel.setAlwaysOnTop(true);
    emptyListLabel.setLookAndFeel(&labelColour1LookAndFeel);
    // the playful list shows its own "empty" message

    addAndMakeVisible(titledList);

    titledList.setLayout(TitledListView::Layout::linked);
    titledList.setHint(Playful::utf8("press 1 to edit  ·  + add  ·  − remove"));
    titledList.setEmptyMessage("Press + to add a modifier!");
    titledList.setMascot(TitledListView::Mascot::blob);
    titledList.setItemInfo([this](int index) {
        // items read "LFO>4OSC:Filter Freq", with a * when switched off
        ListItemInfo info;
        auto text = viewModel.listViewModel.getItemNames()[index];
        info.off = text.endsWith("*");
        text = text.trimCharactersAtEnd("*");
        info.name = text.upToFirstOccurrenceOf(">", false, false);
        auto target = text.fromFirstOccurrenceOf(">", false, false);
        info.target = target.upToFirstOccurrenceOf(":", false, false);
        info.targetDetail = target.fromFirstOccurrenceOf(":", false, false);
        if (info.off)
            info.detail = "off";
        return info;
    });
    titledList.setKnobs([this]() {
        std::array<Playful::KnobInfo, 4> k;
        int n = titledList.getNumItems();
        int row = titledList.getSelectedRow();
        k[0] = {Playful::utf8("MODIFIER · EDIT"),
                titledList.getSelectedItemName(),
                n > 1 ? float(row) / float(n - 1) : 1.0f,
                Playful::Colours::blue, n > 0};
        k[1].enabled = false;
        k[2].enabled = false;
        bool off =
            n > 0 && viewModel.listViewModel.getItemNames()[row].endsWith("*");
        k[3] = {Playful::utf8("— · ON/OFF"), off ? "off" : "on",
                off ? 0.0f : 1.0f, Playful::Colours::red, n > 0};
        return k;
    });
}

TrackModifiersListView::~TrackModifiersListView() {
    midiCommandManager.removeListener(this);
    viewModel.listViewModel.removeListener(this);
    viewModel.listViewModel.itemListState.removeListener(this);
    emptyListLabel.setLookAndFeel(nullptr);
}

void TrackModifiersListView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void TrackModifiersListView::resized() {
    emptyListLabel.setFont(
        juce::Font(juce::Font::getDefaultMonospacedFontName(), getHeight() * .1,
                   juce::Font::plain));
    emptyListLabel.setBounds(getLocalBounds());

    titledList.setBounds(getLocalBounds());

    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        viewModel.listViewModel.itemListState.getSelectedItemIndex());
}

void TrackModifiersListView::encoder1Increased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() +
                1);
}

void TrackModifiersListView::encoder1Decreased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() -
                1);
}

void TrackModifiersListView::encoder1ButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                if (auto modifier = dynamic_cast<tracktion::Modifier *>(
                        viewModel.listViewModel.getSelectedItem())) {
                    stackNavigationController->push(
                        createModifierView(modifier, midiCommandManager));
                    midiCommandManager.setFocusedComponent(
                        stackNavigationController->getTopComponent());
                }
            }
        }
    }
}

void TrackModifiersListView::plusButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                stackNavigationController->push(
                    new AvailableModifiersListView(track, midiCommandManager));
                midiCommandManager.setFocusedComponent(
                    stackNavigationController->getTopComponent());
            }
        }
    }
}

void TrackModifiersListView::minusButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            if (midiCommandManager.getFocusedComponent() == this)
                viewModel.deleteSelectedModifier();
}

void TrackModifiersListView::selectedIndexChanged(int newIndex) {
    titledList.getListView().getListBox().selectRow(newIndex);
    sendLookAndFeelChange();
}

void TrackModifiersListView::itemsChanged() {
    if (viewModel.listViewModel.getItemNames().size() <= 0)
        emptyListLabel.setVisible(true);
    else
        emptyListLabel.setVisible(false);

    titledList.setListItems(viewModel.listViewModel.getItemNames());
    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        titledList.getListView().getListBox().getSelectedRow());
    sendLookAndFeelChange();
    repaint();
}

void TrackModifiersListView::encoder4ButtonReleased() {
    viewModel.toggleSelectedModifierEnabled();
}
