#include "InstrumentPlugins.h"

namespace internal_plugins {

namespace {
juce::String instPercent(float v) {
    return juce::String(juce::roundToInt(v * 100.0f)) + "%";
}
juce::String instSeconds(float s) {
    return s < 1.0f ? juce::String(juce::roundToInt(s * 1000.0f)) + " ms"
                    : juce::String(s, 2) + " s";
}
juce::String instHz(float hz) {
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + " kHz"
                         : juce::String(juce::roundToInt(hz)) + " Hz";
}
} // namespace

//==============================================================================
const char *WavetableEnginePlugin::xmlTypeName = "lmn3Wavetable";

float WavetableEnginePlugin::brightnessHz(float knob) {
    return 200.0f * std::pow(100.0f, juce::jlimit(0.0f, 1.0f, knob));
}

WavetableEnginePlugin::WavetableEnginePlugin(tracktion::PluginCreationInfo info)
    : EnginePlugin(info, [] {
          return std::make_unique<instruments::WavetableSound>();
      }) {
    instruments::Wavetables::get(); // build the tables now, not mid-note
    addSoundChoiceParam("table", "Table", instruments::Wavetables::getNames(),
                        0.0f);
    addSoundParam("position", "Position", {0.0f, 1.0f}, 0.01f, 0.37f,
                  instPercent);
    addSoundParam("warp", "Warp", {0.0f, 1.0f}, 0.02f, 0.1f, instPercent);
    addSoundParam("filter", "Filter", {0.0f, 1.0f}, 0.01f, 0.6f,
                  [](float v) { return instHz(brightnessHz(v)); });
    addSoundParam("posEnv", "Pos Env", {-1.0f, 1.0f}, 0.05f, 0.0f, [](float v) {
        int p = juce::roundToInt(v * 100);
        return (p > 0 ? "+" : "") + juce::String(p) + "%";
    });
    addSoundParam("envTime", "Env Time", {0.01f, 3.0f}, 0.01f, 0.4f,
                  instSeconds);
    addSoundParam("detune", "Detune", {0.0f, 50.0f}, 1.0f, 0.0f, [](float v) {
        return juce::String(juce::roundToInt(v)) + " ct";
    });
    addSoundParam("level", "Level", {0.0f, 1.0f}, 0.02f, 0.8f, instPercent);
    addShapeParams({0.1f, 0.5f, 0.8f, 0.35f, 0.85f, 0.15f, 0.15f, 6});
}

WavetableEnginePlugin::~WavetableEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

//==============================================================================
const char *MalletsEnginePlugin::xmlTypeName = "lmn3Mallets";

MalletsEnginePlugin::MalletsEnginePlugin(tracktion::PluginCreationInfo info)
    : EnginePlugin(info, [] {
          return std::make_unique<instruments::MalletSound>();
      }) {
    addSoundChoiceParam("material", "Material",
                        instruments::Mallets::getMaterialNames(), 0.0f);
    addSoundParam("hardness", "Hardness", {0.0f, 1.0f}, 0.02f, 0.6f,
                  instPercent);
    addSoundParam("decay", "Decay", {0.1f, 6.0f}, 0.05f, 1.4f, instSeconds);
    addSoundParam("tone", "Tone", {0.0f, 1.0f}, 0.02f, 0.45f, instPercent);
    addSoundParam("body", "Body", {0.0f, 1.0f}, 0.02f, 0.4f, instPercent);
    addSoundParam("strike", "Strike Pos", {0.0f, 1.0f}, 0.02f, 0.3f,
                  instPercent);
    addSoundParam("click", "Click", {0.0f, 1.0f}, 0.02f, 0.3f, instPercent);
    addSoundParam("level", "Level", {0.0f, 1.0f}, 0.02f, 0.8f, instPercent);
    // the bar rings on its own: full sustain, a long release
    addShapeParams({0.0f, 0.6f, 1.0f, 0.6f, 1.0f, 0.0f, 0.0f, 8});
}

MalletsEnginePlugin::~MalletsEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

//==============================================================================
const char *GranularEnginePlugin::xmlTypeName = "lmn3Granular";

GranularEnginePlugin::GranularEnginePlugin(tracktion::PluginCreationInfo info)
    : GranularEnginePlugin(info,
                           std::make_shared<instruments::GrainSampleSlot>()) {}

GranularEnginePlugin::GranularEnginePlugin(
    tracktion::PluginCreationInfo info,
    std::shared_ptr<instruments::GrainSampleSlot> s)
    : EnginePlugin(info,
                   [s] { return std::make_unique<instruments::GrainSound>(s); }),
      slot(s) {
    addSoundParam("position", "Position", {0.0f, 1.0f}, 0.01f, 0.42f,
                  instPercent);
    addSoundParam("size", "Size", {0.01f, 0.5f}, 0.005f, 0.08f, instSeconds);
    addSoundParam("density", "Density", {1.0f, 100.0f}, 1.0f, 24.0f,
                  [](float v) { return juce::String(juce::roundToInt(v)) + " / s"; });
    addSoundParam("spray", "Spray", {0.0f, 1.0f}, 0.02f, 0.3f, instPercent);
    addSoundParam("pitch", "Pitch", {-24.0f, 24.0f}, 1.0f, 0.0f, [](float v) {
        int st = juce::roundToInt(v);
        return (st > 0 ? "+" : "") + juce::String(st) + " st";
    });
    addSoundParam("spread", "Spread", {0.0f, 1.0f}, 0.02f, 0.1f, instPercent);
    addSoundChoiceParam("freeze", "Freeze", {"Off", "On"}, 0.0f);
    addSoundParam("level", "Level", {0.0f, 1.0f}, 0.02f, 0.8f, instPercent);
    addShapeParams({0.3f, 0.5f, 1.0f, 0.5f, 0.9f, 0.1f, 0.0f, 4});
    sampleFile.referTo(state, "sampleFile", getUndoManager());
    loadSample();
}

GranularEnginePlugin::~GranularEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

juce::File GranularEnginePlugin::getSampleFile() const {
    auto path = sampleFile.get();
    return juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File();
}

void GranularEnginePlugin::setSampleFile(const juce::File &file) {
    sampleFile = file.getFullPathName();
    loadSample();
}

void GranularEnginePlugin::restorePluginStateFromValueTree(
    const juce::ValueTree &v) {
    EnginePlugin::restorePluginStateFromValueTree(v);
    if (v.hasProperty("sampleFile"))
        sampleFile = v.getProperty("sampleFile").toString();
    loadSample();
}

void GranularEnginePlugin::loadSample() {
    auto file = getSampleFile();
    if (!file.existsAsFile()) {
        slot->set(instruments::GrainSample::builtIn());
        return;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) {
        slot->set(instruments::GrainSample::builtIn());
        return;
    }
    // at most 30 seconds, mixed down to mono
    const juce::int64 maxLength = juce::int64(reader->sampleRate * 30.0);
    int length = int(reader->lengthInSamples < maxLength
                         ? reader->lengthInSamples
                         : maxLength);
    int channels = juce::jmax(1, int(reader->numChannels));
    juce::AudioBuffer<float> read(channels, length);
    reader->read(&read, 0, length, 0, true, true);
    juce::AudioBuffer<float> mono(1, length);
    mono.clear();
    for (int c = 0; c < channels; c++)
        mono.addFrom(0, 0, read, c, 0, length, 1.0f / float(channels));
    slot->set(instruments::GrainSample::fromBuffer(
        std::move(mono), reader->sampleRate, file.getFileNameWithoutExtension()));
}

//==============================================================================
const char *DrumSynthPlugin::xmlTypeName = "lmn3DrumSynth";

DrumSynthPlugin::DrumSynthPlugin(tracktion::PluginCreationInfo info)
    : Plugin(info) {
    for (auto &t : lastHit)
        t = -1.0e9;
    auto &names = Kit::getParamNames();
    static const char *ids[] = {"pitch", "punch", "decay",
                                "tone",  "noise", "drive"};
    for (int pad = 0; pad < Kit::numPads; pad++) {
        auto defaults = Kit::defaults(pad);
        for (int k = 0; k < Kit::numParams; k++) {
            juce::String id = "pad" + juce::String(pad + 1) + ids[k];
            auto value = values.add(new juce::CachedValue<float>());
            value->referTo(state, juce::Identifier(id), getUndoManager(),
                           defaults[size_t(k)]);
            auto p = addParam(
                id,
                Kit::getPadNames()[pad] + " " + names[k].toLowerCase(),
                {0.0f, 1.0f},
                [pad, k](float v) { return Kit::describe(pad, k, v); },
                [](const juce::String &s) { return s.getFloatValue(); });
            p->attachToCurrentValue(*value);
            params[pad][k] = p;
        }
    }
}

DrumSynthPlugin::~DrumSynthPlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

tracktion::AutomatableParameter *DrumSynthPlugin::getParameter(int pad,
                                                               int param) const {
    return params[juce::jlimit(0, Kit::numPads - 1, pad)]
                 [juce::jlimit(0, Kit::numParams - 1, param)];
}

DrumSynthPlugin::Kit::PadParams DrumSynthPlugin::getPadParams(int pad) const {
    Kit::PadParams p;
    for (int k = 0; k < Kit::numParams; k++)
        p[size_t(k)] = getParameter(pad, k)->getCurrentValue();
    return p;
}

void DrumSynthPlugin::initialise(const tracktion::PluginInitialisationInfo &i) {
    sampleRate = i.sampleRate;
    kit.prepare(sampleRate);
}

void DrumSynthPlugin::restorePluginStateFromValueTree(const juce::ValueTree &v) {
    for (auto value : values)
        if (v.hasProperty(value->getPropertyID()))
            *value = float(v.getProperty(value->getPropertyID()));
    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void DrumSynthPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.destBuffer == nullptr)
        return;
    std::array<Kit::PadParams, Kit::numPads> all;
    for (int pad = 0; pad < Kit::numPads; pad++)
        all[size_t(pad)] = getPadParams(pad);

    auto &buffer = *fc.destBuffer;
    const int numSamples = fc.bufferNumSamples;
    float *left = buffer.getWritePointer(0, fc.bufferStartSample);
    float *right = buffer.getNumChannels() > 1
                       ? buffer.getWritePointer(1, fc.bufferStartSample)
                       : nullptr;
    int done = 0;
    if (auto midi = fc.bufferForMidiMessages) {
        if (midi->isAllNotesOff)
            kit.allOff();
        for (auto &m : *midi) {
            int pos = juce::jlimit(
                0, numSamples, juce::roundToInt(m.getTimeStamp() * sampleRate));
            if (pos > done) {
                kit.render(left + done, right ? right + done : nullptr,
                           pos - done, all);
                done = pos;
            }
            if (m.isNoteOn()) {
                int pad = Kit::padForNote(m.getNoteNumber());
                kit.trigger(pad, m.getFloatVelocity(), all[size_t(pad)]);
                selectedPad = pad;
                lastHit[size_t(pad)] = juce::Time::getMillisecondCounterHiRes();
            } else if (m.isAllNotesOff() || m.isAllSoundOff())
                kit.allOff();
        }
    }
    if (done < numSamples)
        kit.render(left + done, right ? right + done : nullptr,
                   numSamples - done, all);
}

} // namespace internal_plugins
