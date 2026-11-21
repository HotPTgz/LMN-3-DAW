#include "EnginePlugins.h"

namespace internal_plugins {

namespace {
juce::String secondsText(float s) {
    if (s < 1.0f)
        return juce::String(juce::roundToInt(s * 1000.0f)) + " ms";
    return juce::String(s, 2) + " s";
}
juce::String percentText(float v) {
    return juce::String(juce::roundToInt(v * 100.0f)) + "%";
}
juce::String ratioText(float v) {
    return "x" + juce::String(v, v == std::floor(v) ? 0 : 1);
}
float parseNumber(const juce::String &s) { return s.getFloatValue(); }
} // namespace

EnginePlugin::EnginePlugin(tracktion::PluginCreationInfo info,
                           engines::PolySynth::Factory factory)
    : Plugin(info), synth(std::move(factory)) {}

EnginePlugin::~EnginePlugin() = default;

bool EnginePlugin::isEngine(const tracktion::Plugin &plugin) {
    return dynamic_cast<const EnginePlugin *>(&plugin) != nullptr;
}

tracktion::AutomatableParameter *EnginePlugin::addValueParam(
    const juce::String &paramID, const juce::String &name,
    juce::NormalisableRange<float> range, float interval, float defaultValue,
    std::function<juce::String(float)> toText,
    std::function<float(const juce::String &)> fromText) {
    auto value = values.add(new juce::CachedValue<float>());
    value->referTo(state, juce::Identifier(paramID), getUndoManager(),
                   defaultValue);
    auto p = addParam(paramID, name, range, toText, fromText);
    p->attachToCurrentValue(*value);
    controls.add({p, double(interval)});
    return p;
}

void EnginePlugin::addSoundParam(const juce::String &paramID,
                                 const juce::String &name,
                                 juce::Range<float> range, float interval,
                                 float defaultValue,
                                 std::function<juce::String(float)> toText) {
    soundParams.add(addValueParam(
        paramID, name,
        juce::NormalisableRange<float>(range.getStart(), range.getEnd()),
        interval, defaultValue, toText, parseNumber));
}

void EnginePlugin::addSoundChoiceParam(const juce::String &paramID,
                                       const juce::String &name,
                                       const juce::StringArray &choices,
                                       float defaultIndex) {
    soundParams.add(addValueParam(
        paramID, name,
        juce::NormalisableRange<float>(0.0f, float(choices.size() - 1), 1.0f),
        1.0f, defaultIndex,
        [choices](float v) {
            return choices[juce::jlimit(0, choices.size() - 1,
                                        juce::roundToInt(v))];
        },
        [choices](const juce::String &s) {
            return float(juce::jmax(0, choices.indexOf(s)));
        }));
}

void EnginePlugin::addShapeParams(const ShapeDefaults &d) {
    jassert(soundParams.size() == engines::numSoundParams);
    auto knob = juce::NormalisableRange<float>(0.0f, 1.0f);
    auto time = [](float v) {
        return secondsText(engines::envelopeSeconds(v));
    };
    shapeParams.add(addValueParam("ampAttack", "Attack", knob, 0.02f, d.attack,
                                  time, parseNumber));
    shapeParams.add(addValueParam("ampDecay", "Decay", knob, 0.02f, d.decay,
                                  time, parseNumber));
    shapeParams.add(addValueParam("ampSustain", "Sustain", knob, 0.02f,
                                  d.sustain, percentText, parseNumber));
    shapeParams.add(addValueParam("ampRelease", "Release", knob, 0.02f,
                                  d.release, time, parseNumber));
    shapeParams.add(addValueParam(
        "cutoff", "Cutoff", knob, 0.01f, d.cutoff,
        [](float v) {
            float hz = engines::cutoffHz(v);
            return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + " kHz"
                                 : juce::String(juce::roundToInt(hz)) + " Hz";
        },
        parseNumber));
    shapeParams.add(addValueParam("resonance", "Resonance", knob, 0.02f,
                                  d.resonance, percentText, parseNumber));
    shapeParams.add(addValueParam("envAmount", "Env Amount", knob, 0.02f,
                                  d.envAmount, percentText, parseNumber));
    shapeParams.add(addValueParam(
        "voices", "Voices",
        juce::NormalisableRange<float>(1.0f, float(engines::maxVoices), 1.0f),
        1.0f, float(d.voices),
        [](float v) { return juce::String(juce::roundToInt(v)); },
        parseNumber));
}

float EnginePlugin::getSoundValue(int index) const {
    if (auto p = soundParams[index])
        return p->getCurrentValue();
    return 0.0f;
}

engines::ShapeSettings EnginePlugin::getShape() const {
    engines::ShapeSettings s;
    if (shapeParams.size() < 8)
        return s;
    s.attack = engines::envelopeSeconds(shapeParams[0]->getCurrentValue());
    s.decay = engines::envelopeSeconds(shapeParams[1]->getCurrentValue());
    s.sustain = shapeParams[2]->getCurrentValue();
    s.release = engines::envelopeSeconds(shapeParams[3]->getCurrentValue());
    s.cutoff = engines::cutoffHz(shapeParams[4]->getCurrentValue());
    s.resonance = shapeParams[5]->getCurrentValue();
    s.envAmount = shapeParams[6]->getCurrentValue();
    s.voices = juce::roundToInt(shapeParams[7]->getCurrentValue());
    return s;
}

double EnginePlugin::getTailLength() const { return getShape().release + 0.1; }

void EnginePlugin::initialise(const tracktion::PluginInitialisationInfo &info) {
    sampleRate = info.sampleRate;
    synth.prepare(sampleRate);
    scratch.assign(size_t(juce::jmax(512, info.blockSizeSamples * 2)), 0.0f);
}

void EnginePlugin::reset() { synth.allNotesOff(); }

void EnginePlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.destBuffer == nullptr)
        return;

    float p[engines::numSoundParams];
    for (int i = 0; i < engines::numSoundParams; i++)
        p[i] = getSoundValue(i);
    const auto shape = getShape();

    auto &buffer = *fc.destBuffer;
    const int numSamples = fc.bufferNumSamples;
    float *left = buffer.getWritePointer(0, fc.bufferStartSample);
    const bool stereo = buffer.getNumChannels() > 1;
    float *right =
        stereo ? buffer.getWritePointer(1, fc.bufferStartSample) : nullptr;

    auto renderRange = [&](int from, int to) {
        while (from < to) {
            int n = to - from;
            if (stereo) {
                synth.render(left + from, right + from, n, p, shape);
            } else {
                n = juce::jmin(n, int(scratch.size()));
                synth.render(left + from, scratch.data(), n, p, shape);
                for (int i = 0; i < n; i++)
                    left[from + i] =
                        0.5f * (left[from + i] + scratch[size_t(i)]);
            }
            from += n;
        }
    };

    int done = 0;
    if (auto midi = fc.bufferForMidiMessages) {
        if (midi->isAllNotesOff)
            synth.allNotesOff();

        for (auto &m : *midi) {
            int pos = juce::jlimit(
                0, numSamples, juce::roundToInt(m.getTimeStamp() * sampleRate));
            if (pos > done) {
                renderRange(done, pos);
                done = pos;
            }
            if (m.isNoteOn())
                synth.noteOn(m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())
                synth.noteOff(m.getNoteNumber());
            else if (m.isPitchWheel())
                synth.setPitchBend(float(m.getPitchWheelValue() - 8192) /
                                   (m.getPitchWheelValue() >= 8192 ? 8191.0f
                                                                    : 8192.0f));
            else if (m.isControllerOfType(1)) // mod wheel
                synth.setModWheel(float(m.getControllerValue()) / 127.0f);
            else if (m.isSustainPedalOn())
                synth.setSustain(true);
            else if (m.isSustainPedalOff())
                synth.setSustain(false);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                synth.allNotesOff();
        }
    }
    renderRange(done, numSamples);
}

void EnginePlugin::restorePluginStateFromValueTree(const juce::ValueTree &v) {
    for (auto value : values)
        if (v.hasProperty(value->getPropertyID()))
            *value = float(v.getProperty(value->getPropertyID()));

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

//==============================================================================
const char *StringEnginePlugin::xmlTypeName = "lmn3String";

StringEnginePlugin::StringEnginePlugin(tracktion::PluginCreationInfo info)
    : EnginePlugin(info,
                   [] { return std::make_unique<engines::StringSound>(); }) {
    addSoundParam("decay", "Decay", {0.1f, 8.0f}, 0.1f, 2.4f,
                  [](float v) { return juce::String(v, 1) + " s"; });
    addSoundParam("brightness", "Brightness", {0.0f, 1.0f}, 0.02f, 0.7f,
                  percentText);
    addSoundParam("pickPos", "Pick Pos", {0.05f, 0.95f}, 0.01f, 0.2f,
                  percentText);
    addSoundParam("body", "Body", {0.0f, 1.0f}, 0.02f, 0.5f, percentText);
    addSoundParam("detune", "Detune", {0.0f, 1.0f}, 0.02f, 0.2f, percentText);
    addSoundParam("stiffness", "Stiffness", {0.0f, 1.0f}, 0.02f, 0.0f,
                  percentText);
    addSoundParam("pluckNoise", "Pluck Noise", {0.0f, 1.0f}, 0.02f, 0.5f,
                  percentText);
    addSoundParam("level", "Level", {0.0f, 1.0f}, 0.02f, 0.8f, percentText);
    addShapeParams({0.0f, 0.7f, 1.0f, 0.3f, 0.85f, 0.1f, 0.15f, 6});
}

StringEnginePlugin::~StringEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

//==============================================================================
const char *FmEnginePlugin::xmlTypeName = "lmn3Fm";

FmEnginePlugin::FmEnginePlugin(tracktion::PluginCreationInfo info)
    : EnginePlugin(info, [] { return std::make_unique<engines::FmSound>(); }) {
    juce::StringArray algorithms;
    int i = 1;
    for (auto &a : engines::FmSound::getAlgorithms())
        algorithms.add(juce::String(i++) + " " + a.name);
    addSoundChoiceParam("algorithm", "Algorithm", algorithms, 0.0f);
    addSoundParam("ratio", "Ratio", {0.5f, 16.0f}, 0.5f, 2.0f, ratioText);
    addSoundParam("amount", "Amount", {0.0f, 1.0f}, 0.02f, 0.6f, percentText);
    addSoundParam("feedback", "Feedback", {0.0f, 1.0f}, 0.02f, 0.2f,
                  percentText);
    addSoundParam("ratio3", "Op 3 Ratio", {0.5f, 16.0f}, 0.5f, 3.0f, ratioText);
    addSoundParam("ratio4", "Op 4 Ratio", {0.5f, 16.0f}, 0.5f, 1.0f, ratioText);
    addSoundParam("modDecay", "Mod Decay", {0.05f, 5.0f}, 0.05f, 0.8f,
                  secondsText);
    addSoundParam("detune", "Detune", {0.0f, 1.0f}, 0.02f, 0.1f, percentText);
    addShapeParams({0.06f, 0.5f, 0.6f, 0.35f, 0.9f, 0.1f, 0.1f, 6});
}

FmEnginePlugin::~FmEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

//==============================================================================
const char *PulseEnginePlugin::xmlTypeName = "lmn3Pulse";

PulseEnginePlugin::PulseEnginePlugin(tracktion::PluginCreationInfo info)
    : EnginePlugin(info,
                   [] { return std::make_unique<engines::PulseSound>(); }) {
    addSoundParam("width", "Width", {0.05f, 0.95f}, 0.01f, 0.3f, percentText);
    addSoundParam("pwmDepth", "PWM Depth", {0.0f, 1.0f}, 0.02f, 0.4f,
                  percentText);
    addSoundParam("pwmRate", "PWM Rate", {0.1f, 10.0f}, 0.1f, 0.8f,
                  [](float v) { return juce::String(v, 1) + " Hz"; });
    addSoundParam("sub", "Sub", {0.0f, 1.0f}, 0.02f, 0.5f, percentText);
    addSoundParam("glide", "Glide", {0.0f, 1.0f}, 0.02f, 0.0f, secondsText);
    addSoundParam("detune", "Detune", {0.0f, 1.0f}, 0.02f, 0.2f, percentText);
    addSoundParam("noise", "Noise", {0.0f, 1.0f}, 0.02f, 0.0f, percentText);
    addSoundParam("drive", "Drive", {0.0f, 1.0f}, 0.02f, 0.2f, percentText);
    addShapeParams({0.06f, 0.4f, 0.7f, 0.3f, 0.6f, 0.25f, 0.4f, 6});
}

PulseEnginePlugin::~PulseEnginePlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

} // namespace internal_plugins
