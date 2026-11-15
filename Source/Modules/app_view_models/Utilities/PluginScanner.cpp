#include "PluginScanner.h"

namespace app_view_models::PluginScanner {

namespace {
juce::File getAppDataFolder() {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
#ifdef JUCE_APPLICATION_NAME_STRING
        .getChildFile(JUCE_APPLICATION_NAME_STRING);
#else
        .getChildFile("LMN-3");
#endif
}
} // namespace

juce::Array<juce::File> getSearchFolders() {
    juce::Array<juce::File> folders;
#if JUCE_LINUX
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    auto userFolder = home.getChildFile(".vst3");
    userFolder.createDirectory();
    folders.add(userFolder);
    for (auto path : {"/usr/lib/vst3", "/usr/local/lib/vst3"}) {
        juce::File folder(path);
        if (folder.isDirectory())
            folders.add(folder);
    }
#else
    // Windows: C:\Program Files\Common Files\VST3 (and the Mac equivalents)
    auto defaults = juce::VST3PluginFormat().getDefaultLocationsToSearch();
    for (int i = 0; i < defaults.getNumPaths(); i++)
        if (defaults[i].isDirectory())
            folders.add(defaults[i]);
#endif
    return folders;
}

juce::String fingerprint(const juce::Array<juce::File> &folders) {
    juce::StringArray entries;
    for (auto &folder : folders)
        for (auto &bundle : folder.findChildFiles(
                 juce::File::findFilesAndDirectories, true, "*.vst3")) {
            // a bundle is a folder: an updated plugin changes the files
            // inside it, not always the folder itself
            auto newest = bundle.getLastModificationTime().toMilliseconds();
            if (bundle.isDirectory())
                for (auto &file :
                     bundle.findChildFiles(juce::File::findFiles, true))
                    newest = juce::jmax(
                        newest,
                        file.getLastModificationTime().toMilliseconds());
            entries.add(bundle.getFullPathName() + "@" + juce::String(newest));
        }
    entries.sort(false);
    return juce::String::toHexString(entries.joinIntoString("\n").hashCode64());
}

juce::File getCacheFile() {
    return getAppDataFolder().getChildFile("plugins.xml");
}

bool loadOrScan(tracktion::Engine &engine, const juce::File &cacheFile,
                const juce::Array<juce::File> &folders) {
    auto &pluginManager = engine.getPluginManager();
    auto &list = pluginManager.knownPluginList;
    auto print = fingerprint(folders);

    if (auto xml = juce::XmlDocument::parse(cacheFile)) {
        if (xml->getStringAttribute("fingerprint") == print) {
            if (auto listXml = xml->getChildByName("KNOWNPLUGINS")) {
                list.clear();
                list.recreateFromXml(*listXml);
                return false;
            }
        }
    }

    juce::Logger::writeToLog("Scanning for VST3 plugins...");
    list.clear();
    juce::FileSearchPath searchPath;
    for (auto &folder : folders)
        searchPath.add(folder);

    for (auto format : pluginManager.pluginFormatManager.getFormats()) {
        if (format->getName() != "VST3")
            continue;
        // if a plugin crashes the scan, it's skipped next time
        juce::PluginDirectoryScanner scanner(
            list, *format, searchPath, true,
            cacheFile.getSiblingFile("plugin-scan-crashed.txt"), false);
        juce::String name;
        while (scanner.scanNextFile(true, name))
            juce::Logger::writeToLog("  scanned " + name);
    }
    juce::Logger::writeToLog("Found " + juce::String(list.getNumTypes()) +
                             " plugins");

    juce::XmlElement root("LMN3PLUGINS");
    root.setAttribute("fingerprint", print);
    if (auto listXml = list.createXml()) {
        listXml->setTagName("KNOWNPLUGINS");
        root.addChildElement(listXml.release());
    }
    cacheFile.getParentDirectory().createDirectory();
    root.writeTo(cacheFile);
    return true;
}

bool loadOrScan(tracktion::Engine &engine) {
    return loadOrScan(engine, getCacheFile(), getSearchFolders());
}

} // namespace app_view_models::PluginScanner
