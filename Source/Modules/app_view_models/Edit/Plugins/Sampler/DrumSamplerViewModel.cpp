
namespace app_view_models {

DrumSamplerViewModel::DrumSamplerViewModel(
    internal_plugins::DrumSamplerPlugin *sampler)
    : SamplerViewModel(sampler, IDs::DRUM_SAMPLER_VIEW_STATE) {
    updateDrumKits();
    itemListState.listSize = drumKitNames.size();

    if (drumKitNames.size() > 0) {
        DBG("current kit index: " +
            std::to_string(itemListState.getSelectedItemIndex()));
        if (samplerPlugin->getNumSounds() > 0) {
            // The plugin already has a kit loaded (e.g. the plugin view was
            // reopened or the project was reloaded). Don't reload the mapping
            // file, since that would reset the gain and start/end times of
            // every sound (#44). Just rebuild the list of sample files from
            // the plugin.
            rebuildSampleFilesFromPlugin();
        } else {
            juce::File currentMap =
                mapFiles[itemListState.getSelectedItemIndex()];
            readMappingFileIntoSampler(currentMap, true);
        }
        DBG("updating thumb");
        updateThumb();
    }
}

void DrumSamplerViewModel::rebuildSampleFilesFromPlugin() {
    // Read the SOUND entries from the plugin state rather than using
    // getSoundFile(), which relies on a sound list that the plugin rebuilds
    // asynchronously.
    drumSampleFiles.clear();
    for (auto child : samplerPlugin->state)
        if (child.hasType(tracktion::IDs::SOUND))
            drumSampleFiles.add(
                tracktion::SourceFileReference::findFileFromString(
                    samplerPlugin->edit,
                    child[tracktion::IDs::source].toString()));
}

juce::StringArray DrumSamplerViewModel::getItemNames() { return drumKitNames; }

juce::String DrumSamplerViewModel::getSelectedItemName() {
    return drumSampleFiles[selectedSoundIndex].getFileNameWithoutExtension();
}

void DrumSamplerViewModel::setSelectedSoundIndex(int noteNumber) {
    if (noteNumber >= 53 && noteNumber <= 76)
        selectedSoundIndex.setValue(noteNumber - 53, nullptr);
}

void DrumSamplerViewModel::selectedIndexChanged(int newIndex) {
    // we just changed kits
    juce::File newMapFile = mapFiles[newIndex];
    readMappingFileIntoSampler(newMapFile, true);
    selectedSoundIndex.setValue(0, nullptr);
    markAndUpdate(shouldUpdateGain);
    updateThumb();
}

void DrumSamplerViewModel::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
    SamplerViewModel::valueTreePropertyChanged(treeWhosePropertyHasChanged,
                                               property);
    if (treeWhosePropertyHasChanged == state) {
        if (property == IDs::selectedSoundIndex) {
            // a pad's sound may have been swapped (e.g. a mic sample)
            rebuildSampleFilesFromPlugin();
            markAndUpdate(shouldUpdateGain);
            updateThumb();
        }
    }
}

void DrumSamplerViewModel::readMappingFileIntoSampler(
    const juce::File &mappingFile, bool shouldUpdateSounds) {
    drumSampleFiles.clear();
    // Remove every SOUND child directly. SamplerPlugin::removeSound(i)
    // removes the i-th child of the plugin state rather than the i-th sound,
    // so it would also delete this view's state (which is stored as a child
    // of the plugin state), and the old ascending loop only removed every
    // other sound.
    for (int i = samplerPlugin->state.getNumChildren() - 1; i >= 0; i--)
        if (samplerPlugin->state.getChild(i).hasType(tracktion::IDs::SOUND))
            samplerPlugin->state.removeChild(i, nullptr);

    YAML::Node rootNode =
        YAML::LoadFile(mappingFile.getFullPathName().toStdString());
    const auto kitDir =
        samplerPlugin->edit.engine.getTemporaryFileManager().getTempFile(
            ConfigurationHelpers::DRUM_KITS_DIRECTORY_NAME);
    YAML::Node mappings = rootNode["mappings"];
    assert(mappings.IsSequence());
    auto foundDirs =
        kitDir.findChildFiles(juce::File::TypesOfFileToFind::findDirectories,
                              false, mappingFile.getFileNameWithoutExtension());
    bool isUserMap = foundDirs.size() > 0;
    for (std::size_t i = 0; i < mappings.size(); i++) {
        YAML::Node mapping = mappings[i];
        int noteNumber = mapping["note_number"].as<int>();
        juce::String fileName = mapping["file_name"].as<std::string>();

        // First check if a directory with the same name as the mapping file
        // exists in the drum_kits dir
        juce::File file;
        if (isUserMap) {
            // We need to look for the sample file in a folder with the same
            // name as the map file There's surely a better way to handle this,
            // but this way is pretty easy The only downside is that it requires
            // users to name the yaml file the same as the parent folder it
            // resides in
            file =
                kitDir.getChildFile(mappingFile.getFileNameWithoutExtension())
                    .getChildFile(fileName);
        } else {
            file = kitDir.getChildFile(fileName);
        }
        drumSampleFiles.add(file);

        // check if the drum sampler has enough sounds to cover this index:
        // if it doesnt have enough sounds we need to add one
        // otherwise just change the sound media
        int index = drumSampleFiles.size() - 1;
        if (samplerPlugin->getNumSounds() <= index) {
            juce::String error = samplerPlugin->addSound(
                file.getFullPathName(), file.getFileNameWithoutExtension(), 0.0,
                0.0, 1.0);
            jassert(error.isEmpty());
        }
        samplerPlugin->setSoundMedia(index, file.getFullPathName());
        if (shouldUpdateSounds) {
            samplerPlugin->setSoundParams(index, noteNumber, noteNumber,
                                          noteNumber);
            samplerPlugin->setSoundGains(index, 1, 0);
            samplerPlugin->setSoundExcerpt(
                index, 0,
                tracktion::AudioFile(samplerPlugin->engine, file).getLength());
            samplerPlugin->setSoundOpenEnded(index, true);
        }
    }
}

void DrumSamplerViewModel::updateDrumKits() {
    mapFiles.clear();
    drumKitNames.clear();

    const auto sampleDir = ConfigurationHelpers::getTempDrumKitsDirectory(
        samplerPlugin->edit.engine);
    for (juce::DirectoryEntry entry : juce::RangedDirectoryIterator(
             sampleDir, true, "*.yaml",
             juce::File::TypesOfFileToFind::findFiles)) {
        YAML::Node node =
            YAML::LoadFile(entry.getFile().getFullPathName().toStdString());

        if (!node.IsNull()) {
            drumKitNames.add(node["name"].as<std::string>());
            mapFiles.add(entry.getFile());
        } else {
            juce::Logger::writeToLog(
                "unable to read YAML file: " +
                entry.getFile().getFullPathName().toStdString());
        }
    }
}

void DrumSamplerViewModel::updateThumb() {
    auto file = drumSampleFiles[selectedSoundIndex];
    auto *reader = formatManager.createReaderFor(file);
    if (reader != nullptr) {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(
            new juce::AudioFormatReaderSource(reader, true));
        fullSampleThumbnail.clear();
        fullSampleThumbnailCache.clear();
        fullSampleThumbnail.setSource(new juce::FileInputSource(file));
        readerSource.reset(newSource.release());
    }

    markAndUpdate(shouldUpdateSample);
}
} // namespace app_view_models
