#include "FxPlugins.h"

namespace internal_plugins {

namespace {
juce::String fxPercent(float v) {
    return juce::String(juce::roundToInt(v * 100.0f)) + "%";
}
juce::String fxHz(float hz) {
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz < 10000.0f ? 1 : 0) +
                               " kHz"
                         : juce::String(juce::roundToInt(hz)) + " Hz";
}
juce::String fxMs(float seconds) {
    return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
}
juce::String fxDb(float db) {
    int v = juce::roundToInt(db);
    return (v > 0 ? "+" : "") + juce::String(v) + " dB";
}
juce::NormalisableRange<float> knobRange() { return {0.0f, 1.0f}; }
} // namespace

//==============================================================================
AudioFxPlugin::AudioFxPlugin(tracktion::PluginCreationInfo info)
    : Plugin(info), tempoPosition(createPosition(edit.tempoSequence)) {}

AudioFxPlugin::~AudioFxPlugin() = default;

bool AudioFxPlugin::isAudioFx(const tracktion::Plugin &plugin) {
    return dynamic_cast<const AudioFxPlugin *>(&plugin) != nullptr;
}

tracktion::AutomatableParameter *
AudioFxPlugin::addChoice(const juce::String &paramID, const juce::String &name,
                         const juce::StringArray &choices, float defaultIndex,
                         bool knob) {
    auto value = values.add(new juce::CachedValue<float>());
    value->referTo(state, juce::Identifier(paramID), getUndoManager(),
                   defaultIndex);
    auto p = addParam(
        paramID, name,
        juce::NormalisableRange<float>(0.0f, float(choices.size() - 1), 1.0f),
        [choices](float v) {
            return choices[juce::jlimit(0, choices.size() - 1,
                                        juce::roundToInt(v))];
        },
        [choices](const juce::String &s) {
            return float(juce::jmax(0, choices.indexOf(s)));
        });
    p->attachToCurrentValue(*value);
    if (knob)
        controls.add({p, 1.0});
    byID.set(paramID, p);
    return p;
}

tracktion::AutomatableParameter *AudioFxPlugin::addNumber(
    const juce::String &paramID, const juce::String &name,
    juce::NormalisableRange<float> range, float interval, float defaultValue,
    std::function<juce::String(float)> toText, bool knob) {
    auto value = values.add(new juce::CachedValue<float>());
    value->referTo(state, juce::Identifier(paramID), getUndoManager(),
                   defaultValue);
    auto p = addParam(paramID, name, range, toText,
                      [](const juce::String &s) { return s.getFloatValue(); });
    p->attachToCurrentValue(*value);
    if (knob)
        controls.add({p, double(interval)});
    byID.set(paramID, p);
    return p;
}

float AudioFxPlugin::get(const juce::String &paramID) const {
    if (auto p = byID[paramID])
        return p->getCurrentValue();
    return 0.0f;
}

int AudioFxPlugin::getChoice(const juce::String &paramID) const {
    return juce::roundToInt(get(paramID));
}

void AudioFxPlugin::initialise(const tracktion::PluginInitialisationInfo &i) {
    sampleRate = i.sampleRate;
    freeRunningBeat = 0.0;
    prepare(sampleRate);
}

void AudioFxPlugin::restorePluginStateFromValueTree(const juce::ValueTree &v) {
    for (auto value : values)
        if (v.hasProperty(value->getPropertyID()))
            *value = float(v.getProperty(value->getPropertyID()));
    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void AudioFxPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;
    fx::BlockTime time;
    time.sampleRate = sampleRate;
    tempoPosition.set(fc.editTime);
    time.beatsPerSecond = tempoPosition.getTempo() / 60.0;
    time.startBeat =
        fc.isPlaying ? tempoPosition.getBeats().inBeats() : freeRunningBeat;
    freeRunningBeat =
        time.startBeat + fc.bufferNumSamples / sampleRate * time.beatsPerSecond;
    process(*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples, time);
}

static float *fxChannel(juce::AudioBuffer<float> &b, int c, int start) {
    return c < b.getNumChannels() ? b.getWritePointer(c, start) : nullptr;
}

//==============================================================================
const char *TapePlugin::xmlTypeName = "lmn3Tape";

TapePlugin::TapePlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addNumber("wow", "Wow", knobRange(), 0.05f, 0.2f, fxPercent);
    addNumber("flutter", "Flutter", knobRange(), 0.05f, 0.1f, fxPercent);
    addNumber("saturate", "Saturate", knobRange(), 0.05f, 0.35f, fxPercent);
    addNumber("hiss", "Hiss", knobRange(), 0.05f, 0.05f, fxPercent);
    addNumber("age", "Age", knobRange(), 0.05f, 0.0f, fxPercent);
    addNumber("tone", "Tone", knobRange(), 0.05f, 0.8f, fxPercent);
}

TapePlugin::~TapePlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void TapePlugin::prepare(double sr) { dsp.prepare(sr); }

void TapePlugin::process(juce::AudioBuffer<float> &b, int start, int n,
                         const fx::BlockTime &) {
    fx::Tape::Settings s;
    s.wow = get("wow");
    s.flutter = get("flutter");
    s.saturate = get("saturate");
    s.hiss = get("hiss");
    s.age = get("age");
    s.tone = get("tone");
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), n, s);
}

//==============================================================================
const char *LofiPlugin::xmlTypeName = "lmn3Lofi";

float LofiPlugin::rateHz(float knob) {
    return 500.0f * std::pow(44100.0f / 500.0f, juce::jlimit(0.0f, 1.0f, knob));
}

LofiPlugin::LofiPlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addNumber("bits", "Bits", {2.0f, 16.0f, 1.0f}, 1.0f, 8.0f,
              [](float v) { return juce::String(juce::roundToInt(v)); });
    // stored as a knob position: equal steps feel even across the range
    addNumber("rate", "Rate", knobRange(), 0.02f, 0.65f,
              [](float v) { return fxHz(rateHz(v)); });
    addNumber("noise", "Noise", knobRange(), 0.05f, 0.0f, fxPercent);
    addNumber("mix", "Mix", knobRange(), 0.05f, 1.0f, fxPercent);
    addNumber("filter", "Filter", knobRange(), 0.02f, 1.0f, [](float v) {
        return v >= 0.99f ? juce::String("Off")
                          : fxHz(ResonantFilterPlugin::cutoffHz(v));
    });
}

LofiPlugin::~LofiPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void LofiPlugin::prepare(double sr) { dsp.prepare(sr); }

void LofiPlugin::process(juce::AudioBuffer<float> &b, int start, int n,
                         const fx::BlockTime &) {
    fx::Lofi::Settings s;
    s.bits = get("bits");
    s.rateHz = rateHz(get("rate"));
    s.noise = get("noise");
    s.mix = get("mix");
    float f = get("filter");
    s.filterHz = f >= 0.99f ? 20000.0f : ResonantFilterPlugin::cutoffHz(f);
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), n, s);
}

//==============================================================================
const char *DrivePlugin::xmlTypeName = "lmn3Drive";

DrivePlugin::DrivePlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addNumber("drive", "Drive", {0.0f, 36.0f, 1.0f}, 1.0f, 12.0f, fxDb);
    addNumber("tone", "Tone", knobRange(), 0.05f, 0.6f, fxPercent);
    addChoice("type", "Type", fx::Drive::getTypeNames(), 0.0f);
    addNumber("mix", "Mix", knobRange(), 0.05f, 1.0f, fxPercent);
    addNumber("output", "Output", {-18.0f, 6.0f, 1.0f}, 1.0f, 0.0f, fxDb);
}

DrivePlugin::~DrivePlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void DrivePlugin::prepare(double sr) { dsp.prepare(sr); }

void DrivePlugin::process(juce::AudioBuffer<float> &b, int start, int n,
                          const fx::BlockTime &) {
    fx::Drive::Settings s;
    s.driveDb = get("drive");
    s.tone = get("tone");
    s.type = getChoice("type");
    s.mix = get("mix");
    s.outputDb = get("output");
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), n, s);
}

//==============================================================================
const char *ResonantFilterPlugin::xmlTypeName = "lmn3Filter";

float ResonantFilterPlugin::cutoffHz(float knob) {
    return 30.0f * std::pow(18000.0f / 30.0f, juce::jlimit(0.0f, 1.0f, knob));
}

const juce::StringArray &ResonantFilterPlugin::getLfoRateNames() {
    static juce::StringArray names{"4 bars", "2 bars", "1 bar", "1/2",
                                   "1/4",    "1/8",    "1/16"};
    return names;
}

double ResonantFilterPlugin::getLfoRateBeats(int index) {
    static const double beats[] = {16, 8, 4, 2, 1, 0.5, 0.25};
    return beats[juce::jlimit(0, 6, index)];
}

ResonantFilterPlugin::ResonantFilterPlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addNumber("cutoff", "Cutoff", knobRange(), 0.01f, 0.6f,
              [](float v) { return fxHz(cutoffHz(v)); });
    addNumber("resonance", "Resonance", knobRange(), 0.02f, 0.4f, fxPercent);
    addNumber("envelope", "Envelope", {-1.0f, 1.0f}, 0.05f, 0.0f, [](float v) {
        int p = juce::roundToInt(v * 100);
        return (p > 0 ? "+" : "") + juce::String(p) + "%";
    });
    addNumber("lfo", "LFO", knobRange(), 0.05f, 0.0f, fxPercent);
    addChoice("type", "Type", fx::ResonantFilter::getTypeNames(), 0.0f);
    addNumber("envSpeed", "Env Release", {0.02f, 1.0f}, 0.02f, 0.15f, fxMs);
    addChoice("lfoRate", "LFO Rate", getLfoRateNames(), 2.0f);
}

ResonantFilterPlugin::~ResonantFilterPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void ResonantFilterPlugin::prepare(double sr) { dsp.prepare(sr); }

void ResonantFilterPlugin::process(juce::AudioBuffer<float> &b, int start,
                                   int n, const fx::BlockTime &time) {
    fx::ResonantFilter::Settings s;
    s.cutoffHz = cutoffHz(get("cutoff"));
    s.resonance = get("resonance");
    s.envelope = get("envelope");
    s.lfo = get("lfo");
    s.type = getChoice("type");
    s.envRelease = get("envSpeed");
    s.lfoBeats = getLfoRateBeats(getChoice("lfoRate"));
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), n, time, s);
    liveCutoff = dsp.getCurrentCutoff();
    liveEnvelope = dsp.getEnvelope();
    liveLfo = dsp.getLfo();
}

//==============================================================================
const char *DuckerPlugin::xmlTypeName = "lmn3Ducker";

DuckerPlugin::DuckerPlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addNumber("depth", "Depth", knobRange(), 0.05f, 0.7f, fxPercent);
    addNumber("release", "Release", {0.03f, 1.0f}, 0.01f, 0.18f, fxMs);
    addChoice("shape", "Shape", fx::Ducker::getShapeNames(), 0.0f);
}

DuckerPlugin::~DuckerPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void DuckerPlugin::getChannelNames(juce::StringArray *ins,
                                   juce::StringArray *outs) {
    Plugin::getChannelNames(ins, outs);
    if (ins != nullptr)
        ins->add(TRANS("Sidechain"));
}

juce::StringArray DuckerPlugin::getSourceNames() {
    auto names = getSidechainSourceNames(false);
    names.insert(0, TRANS("Beat"));
    return names;
}

int DuckerPlugin::getSourceIndex() {
    if (!getSidechainSourceID().isValid())
        return 0;
    auto sourceName = getSidechainSourceName();
    auto names = getSourceNames();
    for (int i = 1; i < names.size(); i++)
        if (names[i].fromFirstOccurrenceOf(". ", false, false) == sourceName)
            return i;
    return 0;
}

void DuckerPlugin::setSourceIndex(int index) {
    auto names = getSourceNames();
    if (index <= 0 || index >= names.size()) {
        setSidechainSourceID({});
        return;
    }
    setSidechainSourceByName(names[index]);
}

void DuckerPlugin::prepare(double sr) { dsp.prepare(sr); }

void DuckerPlugin::process(juce::AudioBuffer<float> &b, int start, int n,
                           const fx::BlockTime &time) {
    fx::Ducker::Settings s;
    s.depth = get("depth");
    s.releaseSeconds = get("release");
    s.shape = getChoice("shape");
    const bool sidechain =
        getSidechainSourceID().isValid() && b.getNumChannels() > 2;
    const float *scL = sidechain ? b.getReadPointer(2, start) : nullptr;
    const float *scR = sidechain && b.getNumChannels() > 3
                           ? b.getReadPointer(3, start)
                           : nullptr;
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), scL, scR, n, time,
                s);
    // the sidechain channels must not be heard
    for (int c = 2; c < b.getNumChannels(); c++)
        b.clear(c, start, n);
    liveGain = dsp.getGain();
    hitCount = dsp.getHitCount();
}

//==============================================================================
const char *BeatRepeatPlugin::xmlTypeName = "lmn3BeatRepeat";

BeatRepeatPlugin::BeatRepeatPlugin(tracktion::PluginCreationInfo info)
    : AudioFxPlugin(info) {
    addChoice("length", "Length", fx::BeatRepeat::getLengthNames(), 2.0f);
    addChoice("repeats", "Repeats", fx::BeatRepeat::getRepeatNames(), 7.0f);
    addNumber("decay", "Decay", {0.0f, 0.9f}, 0.05f, 0.2f, fxPercent);
    addNumber("pitch", "Pitch", {-12.0f, 12.0f, 1.0f}, 1.0f, 0.0f,
              [](float v) {
                  int st = juce::roundToInt(v);
                  return (st > 0 ? "+" : "") + juce::String(st) + " st";
              });
    addChoice("active", "Repeat", {"Off", "On"}, 0.0f, false);
}

BeatRepeatPlugin::~BeatRepeatPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void BeatRepeatPlugin::setActive(bool shouldRepeat) {
    for (auto p : getAutomatableParameters())
        if (p->paramID == "active")
            p->setParameter(shouldRepeat ? 1.0f : 0.0f,
                            juce::sendNotification);
}

void BeatRepeatPlugin::prepare(double sr) { dsp.prepare(sr); }

void BeatRepeatPlugin::process(juce::AudioBuffer<float> &b, int start, int n,
                               const fx::BlockTime &time) {
    fx::BeatRepeat::Settings s;
    s.active = isActive();
    s.lengthBeats = fx::BeatRepeat::getLengthBeats(getChoice("length"));
    s.repeats = fx::BeatRepeat::getRepeatCount(getChoice("repeats"));
    s.decay = get("decay");
    s.pitch = get("pitch");
    dsp.process(fxChannel(b, 0, start), fxChannel(b, 1, start), n, time, s);
    liveRepeat = dsp.isRepeating() ? dsp.getRepeatIndex()
                                   : dsp.isWaiting() ? -2 : -1;
}

} // namespace internal_plugins
