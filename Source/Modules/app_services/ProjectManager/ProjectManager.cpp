#include "ProjectManager.h"

namespace app_services {

ProjectManager::ProjectManager(juce::File dir)
    : appDataDirectory(std::move(dir)) {}

juce::File ProjectManager::getProjectsDirectory() const {
    auto projectsDirectory = appDataDirectory.getChildFile("projects");
    if (!projectsDirectory.isDirectory())
        projectsDirectory.createDirectory();
    return projectsDirectory;
}

juce::Array<juce::File> ProjectManager::getProjects() const {
    auto projects = getProjectsDirectory().findChildFiles(
        juce::File::findFiles, false,
        juce::String("*") + projectFileExtension);

    std::sort(projects.begin(), projects.end(),
              [](const juce::File &a, const juce::File &b) {
                  return getProjectName(a).compareNatural(
                             getProjectName(b)) < 0;
              });
    return projects;
}

juce::StringArray ProjectManager::getProjectNames() const {
    juce::StringArray names;
    for (const auto &project : getProjects())
        names.add(getProjectName(project));
    return names;
}

juce::String ProjectManager::getProjectName(const juce::File &projectFile) {
    return projectFile.getFileNameWithoutExtension();
}

juce::File ProjectManager::getNextNewProjectFile() const {
    const juce::String prefix = juce::String(defaultProjectBaseName) + " ";
    int highest = 0;
    for (const auto &project : getProjects()) {
        auto name = getProjectName(project);
        if (name.startsWith(prefix)) {
            auto suffix = name.substring(prefix.length());
            if (suffix.containsOnly("0123456789"))
                highest = juce::jmax(highest, suffix.getIntValue());
        }
    }

    return getProjectsDirectory().getChildFile(prefix +
                                               juce::String(highest + 1) +
                                               projectFileExtension);
}

bool ProjectManager::deleteProject(const juce::File &projectFile) {
    if (projectFile == currentProject)
        return false;
    if (!projectFile.isAChildOf(getProjectsDirectory()))
        return false;
    return projectFile.deleteFile();
}

juce::File ProjectManager::migrateLegacyEditIfNeeded() const {
    auto legacyEdit = getLegacyEditFile();
    if (!legacyEdit.existsAsFile() || getProjects().size() > 0)
        return {};

    auto target = getProjectsDirectory().getChildFile(
        juce::String(defaultProjectBaseName) + " 1" + projectFileExtension);

    // Copy rather than move, so the old file stays around as a backup and
    // older builds of the app keep working.
    if (legacyEdit.copyFileTo(target))
        return target;

    return {};
}

juce::File ProjectManager::getStartupProject() const {
    auto record = getLastProjectRecordFile();
    if (record.existsAsFile()) {
        auto last = getProjectsDirectory().getChildFile(
            record.loadFileAsString().trim());
        if (last.existsAsFile() && last.hasFileExtension(projectFileExtension))
            return last;
    }

    auto projects = getProjects();
    if (projects.size() > 0)
        return projects.getFirst();

    return getNextNewProjectFile();
}

void ProjectManager::setCurrentProject(const juce::File &projectFile) {
    currentProject = projectFile;
    getLastProjectRecordFile().replaceWithText(projectFile.getFileName());
}

void ProjectManager::requestOpenProject(const juce::File &projectFile) {
    if (onOpenProjectRequested == nullptr || projectFile == currentProject)
        return;

    // The UI that requested the switch is destroyed as part of the switch, so
    // perform it asynchronously, after the current callback has returned.
    auto callback = onOpenProjectRequested;
    juce::MessageManager::callAsync(
        [callback, projectFile]() { callback(projectFile); });
}

juce::File ProjectManager::getLastProjectRecordFile() const {
    return appDataDirectory.getChildFile("last_project");
}

juce::File ProjectManager::getLegacyEditFile() const {
    return appDataDirectory.getChildFile("edit");
}

} // namespace app_services
