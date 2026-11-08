#include "ProjectsListView.h"

const juce::String ProjectsListView::defaultTitle = "Projects";

ProjectsListView::ProjectsListView(app_services::ProjectManager &pm,
                                   app_services::MidiCommandManager &mcm)
    : projectManager(pm), midiCommandManager(mcm),
      titledList({}, defaultTitle, ListTitle::IconType::FONT_AWESOME,
                 juce::String::charToString(0xf07b)) {
    midiCommandManager.addListener(this);
    addAndMakeVisible(titledList);
    refresh();

    // Start with the open project selected
    auto current = projects.indexOf(projectManager.getCurrentProject());
    setSelectedIndex(current >= 0 ? current : 0);
}

ProjectsListView::~ProjectsListView() {
    midiCommandManager.removeListener(this);
}

void ProjectsListView::paint(juce::Graphics &g) {
    g.fillAll(
        getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ProjectsListView::resized() {
    titledList.setBounds(getLocalBounds());
    titledList.getListView().getListBox().scrollToEnsureRowIsOnscreen(
        selectedIndex);
}

bool ProjectsListView::hasFocus() {
    return isShowing() && midiCommandManager.getFocusedComponent() == this;
}

juce::StringArray ProjectsListView::getDisplayNames() const {
    juce::StringArray names;
    for (const auto &project : projects) {
        auto name = app_services::ProjectManager::getProjectName(project);
        if (project == projectManager.getCurrentProject())
            name += "  (open)";
        names.add(name);
    }
    return names;
}

void ProjectsListView::refresh() {
    projects = projectManager.getProjects();
    titledList.setListItems(getDisplayNames());
    setSelectedIndex(selectedIndex);
}

void ProjectsListView::setSelectedIndex(int newIndex) {
    if (projects.isEmpty()) {
        selectedIndex = 0;
        return;
    }

    selectedIndex = juce::jlimit(0, projects.size() - 1, newIndex);
    auto &listBox = titledList.getListView().getListBox();
    listBox.selectRow(selectedIndex);
    listBox.scrollToEnsureRowIsOnscreen(selectedIndex);
    sendLookAndFeelChange();
}

void ProjectsListView::showTemporaryTitle(const juce::String &message) {
    titledList.setTitleString(message);
    startTimer(2500);
}

void ProjectsListView::timerCallback() {
    stopTimer();
    pendingDeleteIndex = -1;
    titledList.setTitleString(defaultTitle);
}

void ProjectsListView::encoder1Increased() {
    if (hasFocus()) {
        pendingDeleteIndex = -1;
        setSelectedIndex(selectedIndex + 1);
    }
}

void ProjectsListView::encoder1Decreased() {
    if (hasFocus()) {
        pendingDeleteIndex = -1;
        setSelectedIndex(selectedIndex - 1);
    }
}

void ProjectsListView::encoder1ButtonReleased() {
    if (!hasFocus() ||
        !juce::isPositiveAndBelow(selectedIndex, projects.size()))
        return;

    auto selected = projects[selectedIndex];
    if (selected == projectManager.getCurrentProject()) {
        showTemporaryTitle("Already open");
        return;
    }

    titledList.setTitleString("Opening...");
    projectManager.requestOpenProject(selected);
}

void ProjectsListView::plusButtonReleased() {
    if (!hasFocus())
        return;

    titledList.setTitleString("Creating...");
    projectManager.requestOpenProject(projectManager.getNextNewProjectFile());
}

void ProjectsListView::minusButtonReleased() {
    if (!hasFocus() ||
        !juce::isPositiveAndBelow(selectedIndex, projects.size()))
        return;

    auto selected = projects[selectedIndex];
    if (selected == projectManager.getCurrentProject()) {
        showTemporaryTitle("Can't delete open project");
        return;
    }

    if (pendingDeleteIndex != selectedIndex) {
        pendingDeleteIndex = selectedIndex;
        showTemporaryTitle("Press - again to delete");
        return;
    }

    pendingDeleteIndex = -1;
    bool deleted = projectManager.deleteProject(selected);
    refresh();
    showTemporaryTitle(deleted ? "Deleted" : "Delete failed");
}
