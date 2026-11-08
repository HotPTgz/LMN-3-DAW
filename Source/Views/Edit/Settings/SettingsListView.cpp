#include "SettingsListView.h"
#include "AudioBufferSizeListView.h"
#include "DeviceTypeListView.h"
#include "MidiInputListView.h"
#include "OutputListView.h"
#include "ProjectsListView.h"
#include "SampleRateListView.h"
#include <app_navigation/app_navigation.h>

SettingsListView::SettingsListView(tracktion::Edit &e,
                                   juce::AudioDeviceManager &dm,
                                   app_services::MidiCommandManager &mcm,
                                   app_services::ProjectManager &pm)
    : edit(e), deviceManager(dm), midiCommandManager(mcm), projectManager(pm),
      viewModel(e, deviceManager),
      titledList(viewModel.getItemNames(), "Settings",
                 ListTitle::IconType::FONT_AWESOME,
                 juce::String::charToString(0xf013)) {
    viewModel.itemListState.addListener(this);
    midiCommandManager.addListener(this);
    addAndMakeVisible(titledList);
}

SettingsListView::~SettingsListView() {
    midiCommandManager.removeListener(this);
    viewModel.itemListState.removeListener(this);
}

void SettingsListView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SettingsListView::resized() {
    titledList.setBounds(getLocalBounds());
    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        viewModel.itemListState.getSelectedItemIndex());
}

void SettingsListView::encoder1Increased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            viewModel.itemListState.setSelectedItemIndex(
                viewModel.itemListState.getSelectedItemIndex() + 1);
        }
    }
}

void SettingsListView::encoder1Decreased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            viewModel.itemListState.setSelectedItemIndex(
                viewModel.itemListState.getSelectedItemIndex() - 1);
        }
    }
}

void SettingsListView::encoder1ButtonReleased() {
    if (isShowing()) {
        if (midiCommandManager.getFocusedComponent() == this) {
            if (auto stackNavigationController = findParentComponentOfClass<
                    app_navigation::StackNavigationController>()) {
                juce::String selectedItem = viewModel.getSelectedItem();
                if (selectedItem == viewModel.projectsSettingName) {
                    stackNavigationController->push(
                        new ProjectsListView(projectManager, midiCommandManager));
                } else if (selectedItem == viewModel.deviceTypeSettingName) {
                    stackNavigationController->push(new DeviceTypeListView(
                        edit, deviceManager, midiCommandManager));
                } else if (selectedItem == viewModel.outputSettingName) {
                    stackNavigationController->push(new OutputListView(
                        edit, deviceManager, midiCommandManager));
                } else if (selectedItem == viewModel.sampleRateSettingName) {
                    stackNavigationController->push(new SampleRateListView(
                        edit, deviceManager, midiCommandManager));
                } else if (selectedItem == viewModel.midiInputSettingName) {
                    stackNavigationController->push(new MidiInputListView(
                        edit, deviceManager, midiCommandManager));
                } else {
                    stackNavigationController->push(new AudioBufferSizeListView(
                        edit, deviceManager, midiCommandManager));
                }
                midiCommandManager.setFocusedComponent(
                    stackNavigationController->getTopComponent());
            }
        }
    }
}

void SettingsListView::selectedIndexChanged(int newIndex) {
    titledList.getListView().getListBox().selectRow(newIndex);
    sendLookAndFeelChange();
}
