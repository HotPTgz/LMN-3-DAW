#include "AvailableSequencersListView.h"
#include "MoreSequencerViews.h"
#include "PlayfulSequencerViews.h"
#include "PluginView.h"
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
                "four patterns, chained into a song",
                "rings of evenly spread drum hits",
                "hold a key and it plays a drum pattern",
                "a chord per bar; other tracks follow",
                "a random melody that slowly changes"};
            return blurbs[titledList.getSelectedRow()];
        });
    titledList.setMascotChooser([this]() {
        using M = TitledListView::Mascot;
        static const M mascots[] = {M::bee,   M::drum,  M::caterpillar,
                                    M::tombola, M::train, M::drum,
                                    M::ball,  M::blob,  M::robot};
        int row = juce::jlimit(0, 8, titledList.getSelectedRow());
        return mascots[row];
    });
    titledList.setItemInfo([](int index) {
        static const juce::StringArray details{
            "grid",  "kit",       "loop",   "balls", "A B C D",
            "rings", "hold keys", "I V vi IV", "random"};
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
                else if (name == "Euclidean")
                    view = new EuclideanSequencerView(track, midiCommandManager);
                else if (name == "Turing")
                    view = new TuringSequencerView(track, midiCommandManager);
                else if (name == "Chords")
                    view = new ChordsSequencerView(track, midiCommandManager);
                else if (name == "Finger") {
                    // Finger plays live, so it is a MIDI effect at the front
                    // of the track: reuse it, or add one
                    tracktion::Plugin::Ptr finger;
                    for (auto p : track->pluginList.getPluginsOfType<
                                  internal_plugins::FingerPlugin>())
                        finger = p;
                    if (finger == nullptr) {
                        finger = track->edit.getPluginCache().createNewPlugin(
                            internal_plugins::FingerPlugin::xmlTypeName, {});
                        if (finger != nullptr)
                            track->pluginList.insertPlugin(finger, 0, nullptr);
                    }
                    if (finger == nullptr)
                        return;
                    finger->showWindowExplicitly();
                    auto *window = finger->windowState->pluginWindow.get();
                    stackNavigationController->push(
                        new PluginView(midiCommandManager, finger.get(), window));
                    midiCommandManager.setFocusedComponent(window);
                    return;
                }
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
