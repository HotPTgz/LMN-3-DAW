// Android: lets the code below use JUCE's Java helpers (must come before the
// JUCE headers)
#define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#include <ImageData.h>
#include <app_configuration/app_configuration.h>
#include <app_services/app_services.h>
#include <internal_plugins/internal_plugins.h>
#include <memory>
#include <tracktion_engine/tracktion_engine.h>
// #include <SynthSampleData.h>
// #include <DrumSampleData.h>
#include "App.h"
#include "DesktopInput.h"
#include "ExtendedUIBehaviour.h"
#include "TouchShell.h"
#include "VirtualHardware.h"

#include "AppLookAndFeel.h"

class GuiAppApplication : public juce::JUCEApplication {
  public:
    GuiAppApplication()
        : splash(new juce::SplashScreen(
              "Welcome to my app!",
              juce::ImageFileFormat::loadFrom(
                  ImageData::tracktion_engine_powered_png,
                  ImageData::tracktion_engine_powered_pngSize),
              true)) {}

    const juce::String getApplicationName() override {
        return JUCE_APPLICATION_NAME_STRING;
    }
    const juce::String getApplicationVersion() override {
        return JUCE_APPLICATION_VERSION_STRING;
    }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String &commandLine) override {
        // This method is where you should put your application's initialisation
        // code..
        juce::ignoreUnused(commandLine);
#if JUCE_ANDROID
        linkDataFolderToSharedStorage();
#endif

        // Create application wide file logger
        logger = std::unique_ptr<juce::FileLogger>(
            juce::FileLogger::createDefaultAppLogger(
                getApplicationName(), "log.txt",
                getApplicationName() + " Logs"));
        juce::Logger::setCurrentLogger(logger.get());

        // we need to add the app internal plugins to the cache:
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::DrumSamplerPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ArpeggiatorPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ScaleLockPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::ChordPlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::StringEnginePlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::FmEnginePlugin>();
        engine.getPluginManager()
            .createBuiltInType<internal_plugins::PulseEnginePlugin>();

        // Tracktion can process a plugin in 128-sample sub-blocks when it has
        // automation or modulation, but MIDI that a plugin outputs from later
        // sub-blocks loses its time offset. The MIDI FX create notes, so keep
        // them on whole blocks to keep their timing exact.
        engine.getPluginManager().canUseFineGrainAutomation =
            [](tracktion::Plugin &p) {
                return !internal_plugins::MidiFxPlugin::isMidiFx(p);
            };

        auto userAppDataDirectory = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);
        projectManager = std::make_unique<app_services::ProjectManager>(
            userAppDataDirectory.getChildFile(getApplicationName()));

        // Older versions kept a single project in <app data>/edit
        auto migrated = projectManager->migrateLegacyEditIfNeeded();
        if (migrated.existsAsFile())
            juce::Logger::writeToLog("Migrated legacy edit to " +
                                     migrated.getFullPathName());

        ConfigurationHelpers::initSamples(engine);

        // VST3 plugins: scanned once and remembered (scanned again only when
        // the VST3 folders change), so projects that use them open properly.
        // The built-in knob maps go to ~/.config/LMN-3/knob_maps.
        app_view_models::PluginScanner::loadOrScan(engine);
        app_view_models::KnobMaps::installDefaults(
            app_view_models::KnobMaps::getDirectory());

        // Without the LMN-3 hardware: the on-screen touch controls (Android,
        // or LMN3_TOUCH=1) and the computer keyboard and mouse (Windows and
        // macOS, or LMN3_KEYBOARD=1 on Linux)
        auto env = [](const char *name) {
            return juce::SystemStats::getEnvironmentVariable(name, "") == "1";
        };
#if JUCE_ANDROID
        useTouch = true;
        bool useComputerKeyboard = false;
#elif JUCE_LINUX
        useTouch = env("LMN3_TOUCH");
        bool useComputerKeyboard = env("LMN3_KEYBOARD") || useTouch;
#else
        useTouch = env("LMN3_TOUCH");
        bool useComputerKeyboard = true;
#endif
        if (useTouch || useComputerKeyboard)
            VirtualHardware::createVirtualDevice(engine);

        loadEdit(projectManager->getStartupProject());

        midiCommandManager =
            std::make_unique<app_services::MidiCommandManager>(engine);
        if (useTouch || useComputerKeyboard)
            virtualHardware =
                std::make_unique<VirtualHardware>(engine, *midiCommandManager);
        if (useComputerKeyboard) {
            desktopInput = std::make_unique<DesktopInput>(*virtualHardware);
            desktopInput->setMouseEnabled(!useTouch);
        }

        if (auto uiBehavior =
                dynamic_cast<ExtendedUIBehaviour *>(&engine.getUIBehaviour())) {
            uiBehavior->setEdit(edit.get());
            uiBehavior->setMidiCommandManager(midiCommandManager.get());
        }

        projectManager->onOpenProjectRequested =
            [this](const juce::File &projectFile) { openProject(projectFile); };

        initialiseAudioDevices();
        mainWindow = std::make_unique<MainWindow>(
            getApplicationName(), engine, *edit, *midiCommandManager,
            *projectManager, useTouch ? virtualHardware.get() : nullptr);
        if (desktopInput != nullptr)
            desktopInput->attachTo(mainWindow.get());
        splash->deleteAfterDelay(juce::RelativeTime::seconds(4.25), false);
    }

    // Loads (or creates, if it doesn't exist yet) the project and makes it
    // the current edit.
    void loadEdit(const juce::File &editFile) {
        if (editFile.existsAsFile()) {
            edit = tracktion::loadEditFromFile(engine, editFile);
        } else {
            editFile.create();
            edit = tracktion::createEmptyEdit(engine, editFile);
            edit->ensureNumberOfAudioTracks(8);

            for (auto track : tracktion::getAudioTracks(*edit))
                track->setColour(appLookAndFeel.getRandomColour());

            // Write it out straight away so it shows up in the projects list
            tracktion::EditFileOperations(*edit).save(true, true, false);
        }

        // The master track does not have the default  plugins added to it by
        // default
        for (auto track : tracktion::getTopLevelTracks(*edit)) {
            if (track->isMasterTrack()) {
                if (track->pluginList
                        .getPluginsOfType<tracktion::VolumeAndPanPlugin>()
                        .getLast() == nullptr) {
                    track->pluginList.addDefaultTrackPlugins(false);
                }
            }
        }

        edit->getTransport().ensureContextAllocated();

        edit->clickTrackEnabled.setValue(true, nullptr);
        edit->setCountInMode(tracktion::Edit::CountIn::oneBar);

        projectManager->setCurrentProject(editFile);
        juce::Logger::writeToLog("Opened project " +
                                 editFile.getFullPathName());
    }

    // Saves the current project, tears down the UI and rebuilds it around the
    // newly opened project. Every view holds a reference to the edit, so
    // rebuilding the window is the simplest safe way to switch.
    void openProject(const juce::File &projectFile) {
        if (edit != nullptr) {
            edit->getTransport().stop(false, false);
            tracktion::EditFileOperations(*edit).save(true, true, false);
        }

        midiCommandManager->setFocusedComponent(nullptr);
        auto uiBehavior =
            dynamic_cast<ExtendedUIBehaviour *>(&engine.getUIBehaviour());
        if (uiBehavior != nullptr) {
            uiBehavior->setApp(nullptr);
            uiBehavior->setEdit(nullptr);
        }

        mainWindow = nullptr;
        edit = nullptr;

        loadEdit(projectFile);

        if (uiBehavior != nullptr)
            uiBehavior->setEdit(edit.get());

        mainWindow = std::make_unique<MainWindow>(
            getApplicationName(), engine, *edit, *midiCommandManager,
            *projectManager, useTouch ? virtualHardware.get() : nullptr);
        if (desktopInput != nullptr)
            desktopInput->attachTo(mainWindow.get());
    }

#if JUCE_ANDROID
    // The app's own data folder can't be reached from a computer, so the
    // LMN-3 folder (projects, samples, drum kits, renders...) lives in the
    // app's shared storage folder instead, and is linked from the data folder:
    //   Android/data/com.fundamentalfrequency.lmn3/files/LMN-3
    // (visible when the phone is plugged into a computer)
    static void linkDataFolderToSharedStorage() {
        auto link = juce::File::getSpecialLocation(
                        juce::File::userApplicationDataDirectory)
                        .getChildFile(JUCE_APPLICATION_NAME_STRING);
        if (link.isSymbolicLink() || link.isDirectory())
            return;

        auto *env = juce::getEnv();
        auto context = juce::getAppContext();
        if (env == nullptr || context.get() == nullptr)
            return;
        auto contextClass = env->GetObjectClass(context.get());
        auto getDir = env->GetMethodID(contextClass, "getExternalFilesDir",
                                       "(Ljava/lang/String;)Ljava/io/File;");
        auto dir = env->CallObjectMethod(context.get(), getDir, nullptr);
        if (dir == nullptr)
            return;
        auto fileClass = env->GetObjectClass(dir);
        auto getPath = env->GetMethodID(fileClass, "getAbsolutePath",
                                        "()Ljava/lang/String;");
        auto path = (jstring)env->CallObjectMethod(dir, getPath);
        juce::File shared(juce::juceString(path));
        auto target = shared.getChildFile(JUCE_APPLICATION_NAME_STRING);
        if (target.createDirectory().wasOk())
            target.createSymbolicLink(link, false);
    }
#endif

    void initialiseAudioDevices() {
        auto &deviceManager = engine.getDeviceManager().deviceManager;
        deviceManager.getCurrentDeviceTypeObject()->scanForDevices();
        auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
        if (result != "") {
            juce::Logger::writeToLog(
                "Attempt to initialise default devices failed!");
        }
    }

    void shutdown() override {
        // Add your application's shutdown code here..

        bool success = edit->engine.getTemporaryFileManager()
                           .getTempDirectory()
                           .deleteRecursively();
        if (!success) {
            juce::Logger::writeToLog("failed to clean up temporary directory " +
                                     edit->engine.getTemporaryFileManager()
                                         .getTempDirectory()
                                         .getFullPathName());
        }
        juce::Logger::setCurrentLogger(nullptr);
        desktopInput = nullptr;
        mainWindow = nullptr; // (deletes our window)
    }

    void systemRequestedQuit() override {
        // This is called when the app is being asked to quit: you can ignore
        // this request and let the app carry on running, or call quit() to
        // allow the app to close.
        quit();
    }

    void anotherInstanceStarted(const juce::String &commandLine) override {
        // When another instance of the app is launched while this one is
        // running, this method is invoked, and the commandLine parameter tells
        // you what the other instance's command-line arguments were.
        juce::ignoreUnused(commandLine);
    }

    class MainWindow : public juce::DocumentWindow {
      public:
        explicit MainWindow(juce::String name, tracktion::Engine &e,
                            tracktion::Edit &ed,
                            app_services::MidiCommandManager &mcm,
                            app_services::ProjectManager &pm,
                            VirtualHardware *touchHardware)
            : DocumentWindow(
                  name,
                  juce::Desktop::getInstance()
                      .getDefaultLookAndFeel()
                      .findColour(ResizableWindow::backgroundColourId),
                  DocumentWindow::allButtons),
              engine(e), edit(ed), midiCommandManager(mcm), projectManager(pm) {
            auto userAppDataDirectory = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);
            auto configFile =
                userAppDataDirectory.getChildFile(name).getChildFile(
                    "config.yaml");
            if (ConfigurationHelpers::getShowTitleBar(configFile))
                setUsingNativeTitleBar(true);
            else {
                setUsingNativeTitleBar(false);
                setTitleBarHeight(0);
            }

            auto app =
                std::make_unique<App>(edit, midiCommandManager, projectManager);
            appComponent = app.get();
            if (touchHardware != nullptr) {
                auto shell = std::make_unique<TouchShell>(std::move(app),
                                                          *touchHardware);
                shell->setSize(1280, 576); // a phone held sideways (20:9)
                setContentOwned(shell.release(), true);
            } else {
                setContentOwned(app.release(), true);
            }

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            // hide the phone's status and navigation bars
            juce::Desktop::getInstance().setKioskModeComponent(this, false);
#elif JUCE_LINUX
            setResizable(false, false);
            centreWithSize(getWidth(), getHeight());
#else
            // On a computer the window can be made bigger (the screens scale
            // with it), keeping the LMN-3 screen's shape
            setResizable(true, true);
            setResizeLimits(getWidth(), getHeight(), getWidth() * 4,
                            getHeight() * 4);
            getConstrainer()->setFixedAspectRatio(double(getWidth()) /
                                                  getHeight());
            centreWithSize(getWidth(), getHeight());
#endif
            // UIBehavior is used to show progress view
            if (auto uiBehavior = dynamic_cast<ExtendedUIBehaviour *>(
                    &engine.getUIBehaviour())) {
                uiBehavior->setApp(appComponent);
            }
            setVisible(true);
        }

        void closeButtonPressed() override {
            // This is called when the user tries to close this window. Here,
            // we'll just ask the app to quit when this happens, but you can
            // change this to do whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the
           base class uses a lot of them, so by overriding you might break its
           functionality. It's best to do all your work in your content
           component instead, but if you really have to override any
           DocumentWindow methods, make sure your subclass also calls the
           superclass's method.
        */

      private:
        tracktion::Engine &engine;
        tracktion::Edit &edit;
        app_services::MidiCommandManager &midiCommandManager;
        app_services::ProjectManager &projectManager;
        App *appComponent = nullptr;
        juce::ValueTree state;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

  private:
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<MainWindow> mainWindow;
    tracktion::Engine engine{getApplicationName(),
                             std::make_unique<ExtendedUIBehaviour>(), nullptr};
    std::unique_ptr<tracktion::Edit> edit;
    std::unique_ptr<app_services::MidiCommandManager> midiCommandManager;
    std::unique_ptr<VirtualHardware> virtualHardware;
    std::unique_ptr<DesktopInput> desktopInput;
    bool useTouch = false;
    std::unique_ptr<app_services::ProjectManager> projectManager;
    AppLookAndFeel appLookAndFeel;
    juce::SplashScreen *splash;
};

START_JUCE_APPLICATION(GuiAppApplication)
