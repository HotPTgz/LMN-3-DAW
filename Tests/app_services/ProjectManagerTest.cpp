#include <app_services/app_services.h>
#include <gtest/gtest.h>

namespace AppServicesTests {

class ProjectManagerTest : public ::testing::Test {
  protected:
    ProjectManagerTest() { appDataDirectory.createDirectory(); }
    ~ProjectManagerTest() override { appDataDirectory.deleteRecursively(); }

    juce::File addProject(const juce::String &name) {
        auto file = manager.getProjectsDirectory().getChildFile(
            name + app_services::ProjectManager::projectFileExtension);
        file.replaceWithText("<EDIT/>");
        return file;
    }

    juce::File appDataDirectory = juce::File::createTempFile("lmn3-pm-test");
    app_services::ProjectManager manager{appDataDirectory};
};

TEST_F(ProjectManagerTest, firstNewProjectIsProject1) {
    EXPECT_EQ(manager.getNextNewProjectFile().getFileName(),
              "Project 1.tracktionedit");
    EXPECT_EQ(manager.getStartupProject().getFileName(),
              "Project 1.tracktionedit");
}

TEST_F(ProjectManagerTest, newProjectNumberFollowsHighestExisting) {
    addProject("Project 2");
    addProject("Project 10");
    addProject("Beat ideas");
    EXPECT_EQ(manager.getNextNewProjectFile().getFileName(),
              "Project 11.tracktionedit");
}

TEST_F(ProjectManagerTest, projectsAreSortedNaturally) {
    addProject("Project 10");
    addProject("Project 2");
    addProject("Project 1");
    EXPECT_EQ(manager.getProjectNames(),
              juce::StringArray({"Project 1", "Project 2", "Project 10"}));
}

TEST_F(ProjectManagerTest, migratesLegacyEditOnce) {
    appDataDirectory.getChildFile("edit").replaceWithText("<EDIT legacy/>");

    auto migrated = manager.migrateLegacyEditIfNeeded();
    ASSERT_TRUE(migrated.existsAsFile());
    EXPECT_EQ(migrated.getFileName(), "Project 1.tracktionedit");
    EXPECT_EQ(migrated.loadFileAsString(), "<EDIT legacy/>");
    // the original is kept as a backup
    EXPECT_TRUE(appDataDirectory.getChildFile("edit").existsAsFile());

    // second launch: nothing more to migrate
    EXPECT_FALSE(manager.migrateLegacyEditIfNeeded().exists());
    EXPECT_EQ(manager.getProjects().size(), 1);
}

TEST_F(ProjectManagerTest, startupProjectIsLastOpened) {
    addProject("Project 1");
    auto second = addProject("Project 2");
    manager.setCurrentProject(second);

    app_services::ProjectManager relaunched{appDataDirectory};
    EXPECT_EQ(relaunched.getStartupProject(), second);
}

TEST_F(ProjectManagerTest, startupFallsBackWhenLastOpenedWasDeleted) {
    auto first = addProject("Project 1");
    auto second = addProject("Project 2");
    manager.setCurrentProject(second);
    second.deleteFile();

    app_services::ProjectManager relaunched{appDataDirectory};
    EXPECT_EQ(relaunched.getStartupProject(), first);
}

TEST_F(ProjectManagerTest, cannotDeleteCurrentProject) {
    auto first = addProject("Project 1");
    auto second = addProject("Project 2");
    manager.setCurrentProject(first);

    EXPECT_FALSE(manager.deleteProject(first));
    EXPECT_TRUE(first.existsAsFile());

    EXPECT_TRUE(manager.deleteProject(second));
    EXPECT_FALSE(second.existsAsFile());
}

TEST_F(ProjectManagerTest, cannotDeleteFilesOutsideProjectsDirectory) {
    auto outside = appDataDirectory.getChildFile("config.yaml");
    outside.replaceWithText("x");
    EXPECT_FALSE(manager.deleteProject(outside));
    EXPECT_TRUE(outside.existsAsFile());
}

} // namespace AppServicesTests
