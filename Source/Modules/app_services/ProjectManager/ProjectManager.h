#pragma once

namespace app_services {

/**
 * Keeps track of the projects (edit files) stored in
 * <app data dir>/projects and which one is currently open.
 *
 * Opening a different project requires tearing down and rebuilding the whole
 * UI (every view holds a reference to the edit), so the actual switch is done
 * by the application via the onOpenProjectRequested callback.
 */
class ProjectManager {
  public:
    explicit ProjectManager(juce::File appDataDirectory);

    static constexpr const char *projectFileExtension = ".tracktionedit";
    static constexpr const char *defaultProjectBaseName = "Project";

    juce::File getProjectsDirectory() const;

    // All project files, sorted naturally by name ("Project 2" < "Project 10")
    juce::Array<juce::File> getProjects() const;
    juce::StringArray getProjectNames() const;

    static juce::String getProjectName(const juce::File &projectFile);

    // Returns a file for a new project named "Project N" where N is one more
    // than the highest existing number. The file itself is not created.
    juce::File getNextNewProjectFile() const;

    // Deletes a project. The currently open project cannot be deleted.
    bool deleteProject(const juce::File &projectFile);

    // Moves the edit file used by older versions (<app data dir>/edit) into
    // the projects directory as "Project 1". Returns the new file, or an
    // invalid file if there was nothing to migrate.
    juce::File migrateLegacyEditIfNeeded() const;

    // The project to open on startup: the last opened project if it still
    // exists, otherwise the first project, otherwise a new project file.
    juce::File getStartupProject() const;

    juce::File getCurrentProject() const { return currentProject; }
    void setCurrentProject(const juce::File &projectFile);

    // Asks the application to save the current project and open another one.
    // Must be set by the application.
    std::function<void(const juce::File &)> onOpenProjectRequested;
    void requestOpenProject(const juce::File &projectFile);

  private:
    juce::File appDataDirectory;
    juce::File currentProject;

    juce::File getLastProjectRecordFile() const;
    juce::File getLegacyEditFile() const;
};

} // namespace app_services
