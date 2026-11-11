#include "TrackPluginsListView.h"
#include "AvailablePluginsListView.h"
#include "EditTabBarView.h"
#include "FourOscView.h"
#include "PlayfulStyle.h"
#include "PluginView.h"
#include <app_navigation/app_navigation.h>
TrackPluginsListView::TrackPluginsListView(
    tracktion::AudioTrack::Ptr t, app_services::MidiCommandManager &mcm)
    : track(t), midiCommandManager(mcm), viewModel(t),
      titledList(viewModel.listViewModel.getItemNames(), "Plugins",
                 ListTitle::IconType::FONT_AWESOME,
                 juce::String::charToString(0xf1e6)) {
    viewModel.listViewModel.addListener(this);
    viewModel.listViewModel.itemListState.addListener(this);
    midiCommandManager.addListener(this);

    emptyListLabel.setFont(
        juce::Font(juce::Font::getDefaultMonospacedFontName(), getHeight() * .1,
                   juce::Font::plain));
    emptyListLabel.setText("Press + to add a plugin!",
                           juce::dontSendNotification);
    emptyListLabel.setJustificationType(juce::Justification::centred);
    emptyListLabel.setAlwaysOnTop(true);
    emptyListLabel.setLookAndFeel(&labelColour1LookAndFeel);
    // the playful list shows its own "empty" message

    addAndMakeVisible(titledList);

    titledList.setLayout(TitledListView::Layout::chain);
    titledList.setSubtitle(track->getName().toLowerCase());
    titledList.setHint(Playful::utf8("+ add  ·  − remove"));
    titledList.setEmptyMessage("Press + to add a plugin!");
    titledList.setItemInfo([this](int index) {
        ListItemInfo info;
        auto plugin = dynamic_cast<tracktion::Plugin *>(
            viewModel.listViewModel.getAdapter()->getItemAtIndex(index));
        if (plugin == nullptr)
            return info;
        auto type = plugin->getPluginType();
        if (plugin->isSynth())
            info.kind = "INSTRUMENT";
        else if (type == "lmn3Arpeggiator" || type == "lmn3Chord" ||
                 type == "lmn3ScaleLock")
            info.kind = "MIDI FX";
        else if (type == tracktion::AuxSendPlugin::xmlTypeName)
            info.kind = "SEND";
        else
            info.kind = "EFFECT";
        info.name = plugin->getName();
        if (auto send = dynamic_cast<tracktion::AuxSendPlugin *>(plugin))
            info.name = send->getBusNumber() == 0 ? "Send A" : "Send B";
        info.off = !plugin->isEnabled();
        return info;
    });
    titledList.setKnobs([this]() {
        std::array<Playful::KnobInfo, 4> k;
        int n = titledList.getNumItems();
        int row = titledList.getSelectedRow();
        bool any = n > 0;
        auto plugin = dynamic_cast<tracktion::Plugin *>(
            viewModel.listViewModel.getSelectedItem());
        k[0] = {Playful::utf8("PLUGIN · OPEN"),
                titledList.getSelectedItemName(),
                n > 1 ? float(row) / float(n - 1) : 1.0f,
                Playful::Colours::blue, any};
        k[1].enabled = false;
        k[2] = {"MOVE", juce::String(row + 1) + " of " + juce::String(n),
                n > 1 ? float(row) / float(n - 1) : 1.0f,
                Playful::Colours::cream, any};
        bool on = plugin != nullptr && plugin->isEnabled();
        k[3] = {Playful::utf8("— · BYPASS"), on ? "on" : "bypassed",
                on ? 1.0f : 0.0f, Playful::Colours::red, any};
        return k;
    });
}

TrackPluginsListView::~TrackPluginsListView() {
    midiCommandManager.removeListener(this);
    viewModel.listViewModel.removeListener(this);
    viewModel.listViewModel.itemListState.removeListener(this);
    emptyListLabel.setLookAndFeel(nullptr);
}

void TrackPluginsListView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void TrackPluginsListView::resized() {
    emptyListLabel.setFont(
        juce::Font(juce::Font::getDefaultMonospacedFontName(), getHeight() * .1,
                   juce::Font::plain));
    emptyListLabel.setBounds(getLocalBounds());

    titledList.setBounds(getLocalBounds());

    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        viewModel.listViewModel.itemListState.getSelectedItemIndex());
}

void TrackPluginsListView::encoder1Increased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() +
                1);
}

void TrackPluginsListView::encoder1Decreased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            viewModel.listViewModel.itemListState.setSelectedItemIndex(
                viewModel.listViewModel.itemListState.getSelectedItemIndex() -
                1);
}

void TrackPluginsListView::encoder1ButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                if (auto plugin = dynamic_cast<tracktion::Plugin *>(
                        viewModel.listViewModel.getSelectedItem())) {
                    // this creates the plugin "window" component (not really a
                    // window, just a component) in the window state object
                    plugin->showWindowExplicitly();
                    stackNavigationController->push(new PluginView(
                        midiCommandManager, plugin,
                        plugin->windowState->pluginWindow.get()));

                    // the classic four osc view has a tab component; the
                    // playful screens take the focus themselves
                    auto *window = plugin->windowState->pluginWindow.get();
                    if (auto fourOscView = dynamic_cast<FourOscView *>(window))
                        midiCommandManager.setFocusedComponent(
                            fourOscView->getCurrentContentComponent());
                    else
                        midiCommandManager.setFocusedComponent(window);
                }
            }
        }
    }
}

void TrackPluginsListView::plusButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                stackNavigationController->push(
                    new AvailablePluginsListView(track, midiCommandManager));
                midiCommandManager.setFocusedComponent(
                    stackNavigationController->getTopComponent());
            }
        }
    }
}

void TrackPluginsListView::minusButtonReleased() {
    if (isShowing())
        if (midiCommandManager.getFocusedComponent() == this)
            if (midiCommandManager.getFocusedComponent() == this) {
                viewModel.deleteSelectedPlugin();
                if (auto editTabBarView =
                        findParentComponentOfClass<EditTabBarView>()) {
                    // reset the modifiers tab since we deleted a plugin
                    editTabBarView->resetModifiersTab();
                }
            }
}

void TrackPluginsListView::selectedIndexChanged(int newIndex) {
    titledList.getListView().getListBox().selectRow(newIndex);
    sendLookAndFeelChange();
}

void TrackPluginsListView::itemsChanged() {
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

void TrackPluginsListView::encoder3Increased() {
    viewModel.moveSelectedPluginDown();
}

void TrackPluginsListView::encoder3Decreased() {
    viewModel.moveSelectedPluginUp();
}

void TrackPluginsListView::encoder4ButtonReleased() {
    viewModel.toggleSelectedPluginEnabled();
}
