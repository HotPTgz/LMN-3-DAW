#pragma once
#include "TitledListView.h"
#include <app_services/app_services.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Settings -> Projects
//   encoder 1 turn:  select project
//   encoder 1 press: open selected project (current one is saved first)
//   +:               create a new project and open it
//   -:               delete selected project (press twice to confirm)
class ProjectsListView : public juce::Component,
                         public app_services::MidiCommandManager::Listener,
                         private juce::Timer {
  public:
    ProjectsListView(app_services::ProjectManager &pm,
                     app_services::MidiCommandManager &mcm);
    ~ProjectsListView() override;

    void paint(juce::Graphics &) override;
    void resized() override;

    void encoder1Increased() override;
    void encoder1Decreased() override;
    void encoder1ButtonReleased() override;
    void plusButtonReleased() override;
    void minusButtonReleased() override;

  private:
    app_services::ProjectManager &projectManager;
    app_services::MidiCommandManager &midiCommandManager;
    juce::Array<juce::File> projects;
    int selectedIndex = 0;
    int pendingDeleteIndex = -1;
    TitledListView titledList;

    static const juce::String defaultTitle;

    bool hasFocus();
    void refresh();
    void setSelectedIndex(int newIndex);
    void showTemporaryTitle(const juce::String &message);
    juce::StringArray getDisplayNames() const;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectsListView)
};
