#include "EditTabBarView.h"
#include "AvailableSequencersListView.h"
#include "FourOscView.h"
#include "MixerView.h"
#include "PlayfulApp.h"
#include "PluginView.h"
#include "SettingsListView.h"
#include "TempoSettingsView.h"
#include "TrackModifiersListView.h"
#include "TrackPluginsListView.h"
#include "TracksView.h"
EditTabBarView::EditTabBarView(tracktion::Edit &e,
                               app_services::MidiCommandManager &mcm,
                               app_services::ProjectManager &pm)
    : TabbedComponent(juce::TabbedButtonBar::Orientation::TabsAtTop), edit(e),
      midiCommandManager(mcm), projectManager(pm), viewModel(edit),
      renderMenu(mcm) {
    // Note: Some tabs are on a per-track basis and are added in
    // selectedIndexChanged, not here this is possible since this view is a
    // listener of the tracks item list state
    addTab(tracksTabName, juce::Colours::transparentBlack,
           new TracksView(edit, midiCommandManager), true);
    addTab(tempoSettingsTabName, juce::Colours::transparentBlack,
           new TempoSettingsView(edit, midiCommandManager), true);
    addTab(mixerTabName, juce::Colours::transparentBlack,
           new MixerView(edit, midiCommandManager), true);
    addTab(settingsTabName, juce::Colours::transparentBlack,
           new app_navigation::StackNavigationController(new SettingsListView(
               edit, edit.engine.getDeviceManager().deviceManager,
               midiCommandManager, projectManager)),
           true);

    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        // this is so we can be notified when selected track changes
        tracksView->getViewModel().listViewModel.itemListState.addListener(
            this);
    }

    // hide tab bar
    setTabBarDepth(0);

    addChildComponent(octaveDisplayComponent);
    octaveDisplayComponent.setAlwaysOnTop(true);

    addChildComponent(messageBox);
    messageBox.setAlwaysOnTop(true);

    addChildComponent(renderMenu);
    renderMenu.setAlwaysOnTop(true);
    renderMenu.onRender = [this](int choice) {
        // Let the oven show "baking" before the (blocking) render starts
        juce::Component::SafePointer<EditTabBarView> safeThis(this);
        juce::Timer::callAfterDelay(80, [safeThis, choice]() {
            if (safeThis == nullptr)
                return;
            safeThis->render(
                choice == 0
                    ? app_view_models::RenderPlanner::RenderType::mix
                    : app_view_models::RenderPlanner::RenderType::stems);
            safeThis->renderMenu.close();
        });
    };

    addChildComponent(powerOffOverlay);
    powerOffOverlay.setAlwaysOnTop(true);

    addChildComponent(helpOverlay);
    helpOverlay.setAlwaysOnTop(true);

    midiCommandManager.addListener(this);
    viewModel.addListener(this);

    // Set tracks as initial view
    setCurrentTabIndex(tracksIndex);
}

EditTabBarView::~EditTabBarView() {
    midiCommandManager.removeListener(this);
    viewModel.removeListener(this);
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);

    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex)))
        tracksView->getViewModel().listViewModel.itemListState.removeListener(
            this);
}

void EditTabBarView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void EditTabBarView::resized() {
    juce::TabbedComponent::resized();

    int octaveDisplayWidth = getWidth() / 6;
    int octaveDisplayHeight = getHeight() / 8;
    octaveDisplayComponent.setBounds((getWidth() - octaveDisplayWidth) / 2,
                                     (getHeight() - octaveDisplayHeight) / 2,
                                     octaveDisplayWidth, octaveDisplayHeight);

    int messageBoxHeight = getHeight() / 6;
    auto font = Playful::font(messageBoxHeight * 0.36f, true);
    int width = font.getStringWidth(messageBox.getMessage());
    int messageBoxWidth =
        juce::jmin(getWidth() - 20, width + messageBoxHeight + 10);

    messageBox.setBounds((getWidth() - messageBoxWidth) / 2,
                         (getHeight() - messageBoxHeight) / 2, messageBoxWidth,
                         messageBoxHeight);

    renderMenu.setBounds(getLocalBounds());
    powerOffOverlay.setBounds(getLocalBounds());
    helpOverlay.setBounds(getLocalBounds());
}

void EditTabBarView::helpButtonPressed() {
    helpOverlay.show(helpCardFor(midiCommandManager.getFocusedComponent()));
    midiCommandManager.isHelpOpen = true;
}

void EditTabBarView::helpClosed() { helpOverlay.hide(); }

void EditTabBarView::showMessage(const juce::String &message, int durationMs) {
    messageBox.setMessage(message);
    // must call resized so message box width is updated to fit text
    resized();
    messageBox.setVisible(true);
    startTimer(durationMs);
}

void EditTabBarView::tracksButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(tracksTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            // Clear the history so that only changes that happen on the tracks
            // tab can be undone ie dont let plugins that were added in the
            // plugins tab get removed or whatever
            edit.getUndoManager().clearUndoHistory();
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

void EditTabBarView::tempoSettingsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(tempoSettingsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

void EditTabBarView::saveButtonReleased() {
    if (isShowing()) {
        juce::Logger::writeToLog("Saving edit ...");
        tracktion::EditFileOperations fileOperations(edit);
        fileOperations.save(true, true, false);
        juce::Logger::writeToLog("Save complete!");

        messageBox.setMessage("Save Complete!");
        // must call resized so message box width is updated to fit text
        resized();
        messageBox.setVisible(true);
        startTimer(1000);
    }
}

void EditTabBarView::powerOffHoldStarted() {
    powerOffSecondsLeft = powerOffHoldSeconds;
    messageBox.setVisible(false);
    powerOffOverlay.start(powerOffHoldSeconds);

    powerOffCountdown.onTick = [this]() {
        powerOffSecondsLeft--;
        if (powerOffSecondsLeft <= 0) {
            powerOffCountdown.stopTimer();
            saveAndPowerOff();
        }
    };
    powerOffCountdown.startTimer(1000);
}

void EditTabBarView::powerOffHoldCancelled() {
    if (powerOffCountdown.isTimerRunning()) {
        powerOffCountdown.stopTimer();
        powerOffOverlay.hide();
        showMessage("Power off cancelled");
    }
}

void EditTabBarView::saveAndPowerOff() {
    juce::Logger::writeToLog("Saving before power off ...");
    edit.getTransport().stop(false, false);
    tracktion::EditFileOperations(edit).save(true, true, false);

    stopTimer();
    powerOffOverlay.showSaved("Saved. Goodnight!");

    // let the message appear before the shutdown starts
    juce::Component::SafePointer<EditTabBarView> safeThis(this);
    juce::Timer::callAfterDelay(500, [safeThis]() {
        if (!app_services::PowerControl::powerOff() && safeThis != nullptr) {
            safeThis->powerOffOverlay.hide();
            safeThis->showMessage("Power off failed", 2000);
        }
    });
}

void EditTabBarView::renderButtonReleased() {
    if (isShowing()) {
        // Control + Save toggles the render menu
        if (renderMenu.isOpen())
            renderMenu.close();
        else
            renderMenu.show(app_view_models::RenderPlanner::isLoopRender(edit),
                            app_services::ProjectManager::getProjectName(
                                projectManager.getCurrentProject()));
    }
}

void EditTabBarView::render(app_view_models::RenderPlanner::RenderType type) {
    using namespace app_view_models;

    edit.getTransport().stop(false, false);

    auto rendersDirectory =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile(applicationName)
            .getChildFile("renders");

    auto jobs = RenderPlanner::planRender(edit, type, rendersDirectory,
                                          juce::Time::getCurrentTime());
    if (jobs.isEmpty()) {
        showMessage("Nothing to render", 1500);
        return;
    }

    auto timeRange = RenderPlanner::getRenderRange(edit);
    if (timeRange.getLength().inSeconds() <= 0.0) {
        showMessage("Nothing to render", 1500);
        return;
    }

    // A track that makes no sound (e.g. MIDI clips but no instrument) produces
    // no file, so count the files that were actually written.
    int rendered = 0;
    for (const auto &job : jobs) {
        juce::Logger::writeToLog("Rendering " + job.file.getFullPathName());
        job.file.getParentDirectory().createDirectory();
        bool ok = tracktion::Renderer::renderToFile("Render", job.file, edit,
                                                    timeRange, job.tracksToDo,
                                                    true, {}, true);
        if (ok && job.file.existsAsFile())
            rendered++;
        else
            juce::Logger::writeToLog("Nothing rendered for " +
                                     job.file.getFileName());
    }

    // Don't leave an empty stems folder behind
    if (type == RenderPlanner::RenderType::stems) {
        auto stemsDirectory = jobs.getFirst().file.getParentDirectory();
        if (stemsDirectory.isDirectory() &&
            stemsDirectory.getNumberOfChildFiles(juce::File::findFiles) == 0)
            stemsDirectory.deleteRecursively();
    }

    juce::Logger::writeToLog("Render complete! " + juce::String(rendered) +
                             " of " + juce::String(jobs.size()) + " files");
    if (rendered == 0)
        showMessage("No audio to render", 1500);
    else if (type == RenderPlanner::RenderType::stems)
        showMessage(juce::String(rendered) + " stems rendered", 1500);
    else
        showMessage("Render Complete!");
}

void EditTabBarView::mixerButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(mixerTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            midiCommandManager.setFocusedComponent(
                getCurrentContentComponent());
        }
    }
}

void EditTabBarView::settingsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(settingsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::pluginsButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(pluginsTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                if (auto pluginView = dynamic_cast<PluginView *>(
                        navigationController->getTopComponent())) {
                    // Four osc has a tab bar component, have to treat it
                    // special
                    if (auto fourOscView = dynamic_cast<FourOscView *>(
                            pluginView->getPlugin()
                                ->windowState->pluginWindow.get()))
                        midiCommandManager.setFocusedComponent(
                            fourOscView->getCurrentContentComponent());
                    else
                        midiCommandManager.setFocusedComponent(
                            pluginView->getPlugin()
                                ->windowState->pluginWindow.get());
                } else {
                    midiCommandManager.setFocusedComponent(
                        navigationController->getTopComponent());
                }
            }

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::modifiersButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(modifiersTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent()))
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::sequencersButtonReleased() {
    if (isShowing()) {
        juce::StringArray tabNames = getTabNames();
        int index = tabNames.indexOf(sequencersTabName);
        if (index != getCurrentTabIndex()) {
            setCurrentTabIndex(index);
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent()))
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());

        } else {
            if (auto navigationController =
                    dynamic_cast<app_navigation::StackNavigationController *>(
                        getCurrentContentComponent())) {
                navigationController->popToRoot();
                midiCommandManager.setFocusedComponent(
                    navigationController->getTopComponent());
            }
        }
    }
}

void EditTabBarView::selectedIndexChanged(int newIndex) {
    resetTrackRelatedTabs();
}

void EditTabBarView::resetModifiersTab() {
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                tracksView->getViewModel().listViewModel.getSelectedItem())) {
            tabNames = getTabNames();
            int modifiersIndex = tabNames.indexOf(modifiersTabName);
            removeTab(modifiersIndex);

            addTab(modifiersTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackModifiersListView(track, midiCommandManager)),
                   true);
        }
    }
}

void EditTabBarView::octaveChanged(int newOctave) {
    viewModel.setCurrentOctave(newOctave);
    octaveDisplayComponent.setOctave(newOctave);
    octaveDisplayComponent.setVisible(true);
    startTimer(1000);
}

void EditTabBarView::timerCallback() {
    octaveDisplayComponent.setVisible(false);
    messageBox.setVisible(false);
}

void EditTabBarView::currentTabChanged(int newCurrentTabIndex,
                                       const juce::String &newCurrentTabName) {
    renderMenu.close();

    // A bunch of stuff happens in the sequencer view constructor
    // that needs to happen everytime it comes on screen
    // easiest way is just to remove and add the sequencers tab anytime a tab is
    // changed A better way would be to do this only when the SEQUENCER tab is
    // navigated away from, but this works for now The best way would be to
    // detect when the sequencer tab is shown and run some init method or
    // something
    if (newCurrentTabName != sequencersTabName) {
        juce::StringArray tabNames = getTabNames();
        int tracksIndex = tabNames.indexOf(tracksTabName);
        if (auto tracksView = dynamic_cast<TracksView *>(
                getTabContentComponent(tracksIndex))) {
            if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                    tracksView->getViewModel()
                        .listViewModel.getSelectedItem())) {
                tabNames = getTabNames();
                int sequencersIndex = tabNames.indexOf(sequencersTabName);
                removeTab(sequencersIndex);

                addTab(sequencersTabName, juce::Colours::transparentBlack,
                       new app_navigation::StackNavigationController(
                           new AvailableSequencersListView(track,
                                                           midiCommandManager)),
                       true);
            }
        }
    }
}

void EditTabBarView::trackDeleted() { resetTrackRelatedTabs(); }

void EditTabBarView::resetTrackRelatedTabs() {
    juce::StringArray tabNames = getTabNames();
    int tracksIndex = tabNames.indexOf(tracksTabName);
    if (auto tracksView =
            dynamic_cast<TracksView *>(getTabContentComponent(tracksIndex))) {
        if (auto track = dynamic_cast<tracktion::AudioTrack *>(
                tracksView->getViewModel().listViewModel.getSelectedItem())) {
            tabNames = getTabNames();
            int sequencersIndex = tabNames.indexOf(sequencersTabName);
            removeTab(sequencersIndex);

            tabNames = getTabNames();
            int modifiersIndex = tabNames.indexOf(modifiersTabName);
            removeTab(modifiersIndex);

            tabNames = getTabNames();
            int pluginsIndex = tabNames.indexOf(pluginsTabName);
            removeTab(pluginsIndex);

            addTab(pluginsTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackPluginsListView(track, midiCommandManager)),
                   true);
            addTab(modifiersTabName, juce::Colours::transparentBlack,
                   new app_navigation::StackNavigationController(
                       new TrackModifiersListView(track, midiCommandManager)),
                   true);
            addTab(
                sequencersTabName, juce::Colours::transparentBlack,
                new app_navigation::StackNavigationController(
                    new AvailableSequencersListView(track, midiCommandManager)),
                true);
        }
    }
}
