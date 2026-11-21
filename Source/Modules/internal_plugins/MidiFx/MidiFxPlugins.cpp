#include "MidiFxPlugins.h"

namespace internal_plugins {

MidiFxPlugin::MidiFxPlugin(tracktion::PluginCreationInfo info) : Plugin(info) {}

MidiFxPlugin::~MidiFxPlugin() = default;

bool MidiFxPlugin::isMidiFx(const tracktion::Plugin &plugin) {
    return dynamic_cast<const MidiFxPlugin *>(&plugin) != nullptr;
}

tracktion::AutomatableParameter *MidiFxPlugin::addChoiceParam(
    const juce::String &paramID, const juce::String &name,
    const juce::StringArray &choices, juce::CachedValue<float> &value,
    float defaultIndex) {
    value.referTo(state, juce::Identifier(paramID), getUndoManager(),
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
    p->attachToCurrentValue(value);
    controls.add({p, 1.0});
    cachedValues.add(&value);
    return p;
}

tracktion::AutomatableParameter *MidiFxPlugin::addNumberParam(
    const juce::String &paramID, const juce::String &name,
    juce::Range<float> range, float interval, juce::CachedValue<float> &value,
    float defaultValue, std::function<juce::String(float)> toText) {
    value.referTo(state, juce::Identifier(paramID), getUndoManager(),
                  defaultValue);
    auto p = addParam(paramID, name,
                      juce::NormalisableRange<float>(range.getStart(),
                                                     range.getEnd(), interval),
                      toText,
                      [](const juce::String &s) { return s.getFloatValue(); });
    p->attachToCurrentValue(value);
    controls.add({p, double(interval)});
    cachedValues.add(&value);
    return p;
}

void MidiFxPlugin::restorePluginStateFromValueTree(const juce::ValueTree &v) {
    for (auto value : cachedValues)
        if (v.hasProperty(value->getPropertyID()))
            *value = float(v.getProperty(value->getPropertyID()));

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

//==============================================================================
const char *ArpeggiatorPlugin::xmlTypeName = "lmn3Arpeggiator";

ArpeggiatorPlugin::ArpeggiatorPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info), tempoPosition(createPosition(edit.tempoSequence)) {
    mode = addChoiceParam("mode", TRANS("Mode"), midifx::Arp::getModeNames(),
                          modeValue, 0.0f);
    rate = addChoiceParam("rate", TRANS("Rate"), midifx::Arp::getRateNames(),
                          rateValue, 4.0f); // 1/16
    octaves = addNumberParam(
        "octaves", TRANS("Octaves"), {1.0f, 4.0f}, 1.0f, octavesValue, 1.0f,
        [](float v) { return juce::String(juce::roundToInt(v)); });
    gate = addNumberParam(
        "gate", TRANS("Gate"), {0.1f, 1.0f}, 0.05f, gateValue, 0.5f,
        [](float v) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
    latch = addChoiceParam("latch", TRANS("Latch"), {"Off", "On"}, latchValue,
                           0.0f);
}

ArpeggiatorPlugin::~ArpeggiatorPlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

void ArpeggiatorPlugin::initialise(
    const tracktion::PluginInitialisationInfo &) {
    freeRunningBeat = 0.0;
}

void ArpeggiatorPlugin::applyToBuffer(
    const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;

    tempoPosition.set(fc.editTime);
    const double beatsPerSecond = tempoPosition.getTempo() / 60.0;
    const double blockSeconds = fc.bufferNumSamples / sampleRate;

    // Follow the song position while playing so the arpeggio lines up with
    // the beat; keep counting on our own while stopped so it still plays.
    double blockStartBeat;
    if (fc.isPlaying) {
        blockStartBeat = tempoPosition.getBeats().inBeats();
        freeRunningBeat = blockStartBeat;
    } else {
        blockStartBeat = freeRunningBeat;
    }
    freeRunningBeat = blockStartBeat + blockSeconds * beatsPerSecond;

    midifx::ArpEngine::Settings settings;
    settings.mode = choice(mode.get());
    settings.rateBeats = midifx::Arp::getRateBeats(choice(rate.get()));
    settings.octaves = juce::roundToInt(octaves->getCurrentValue());
    settings.gate = gate->getCurrentValue();
    settings.latch = choice(latch.get()) == 1;

    engine.process(*fc.bufferForMidiMessages, blockStartBeat, beatsPerSecond,
                   blockSeconds, settings);
}

//==============================================================================
const char *ScaleLockPlugin::xmlTypeName = "lmn3ScaleLock";

ScaleLockPlugin::ScaleLockPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info) {
    root = addChoiceParam("root", TRANS("Root"), midifx::Scales::getRootNames(),
                          rootValue, 0.0f);
    scale = addChoiceParam("scale", TRANS("Scale"), midifx::Scales::getNames(),
                           scaleValue, 0.0f);
    for (auto &channel : mappedNote)
        for (auto &n : channel)
            n = -1;
}

ScaleLockPlugin::~ScaleLockPlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

void ScaleLockPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;

    const int rootNote = choice(root.get());
    const int scaleIndex = choice(scale.get());

    for (auto &m : *fc.bufferForMidiMessages) {
        if (!(m.isNoteOn() || m.isNoteOff()))
            continue;

        int channel = juce::jlimit(1, 16, m.getChannel()) - 1;
        int note = m.getNoteNumber();

        if (m.isNoteOn()) {
            int snapped =
                midifx::Scales::snapToScale(note, rootNote, scaleIndex);
            mappedNote[channel][note] = snapped;
            m.setNoteNumber(snapped);
        } else {
            // send the note off to the note this key actually played, even if
            // the scale changed while it was held
            int mapped = mappedNote[channel][note];
            if (mapped >= 0)
                m.setNoteNumber(mapped);
            mappedNote[channel][note] = -1;
        }
    }
}

//==============================================================================
const char *ChordPlugin::xmlTypeName = "lmn3Chord";

ChordPlugin::ChordPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info) {
    chord = addChoiceParam("chord", TRANS("Chord"), midifx::Chords::getNames(),
                           chordValue, 0.0f);
    inversion = addChoiceParam("inversion", TRANS("Inversion"),
                               midifx::Chords::getInversionNames(),
                               inversionValue, 0.0f);
    spread =
        addChoiceParam("spread", TRANS("Voicing"),
                       midifx::Chords::getSpreadNames(), spreadValue, 0.0f);
}

ChordPlugin::~ChordPlugin() {
    // detach first: telling listeners about the deletion empties the
    // parameter list, and parameters still attached would later touch
    // values that are already gone
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
    notifyListenersOfDeletion();
}

void ChordPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;

    auto &midi = *fc.bufferForMidiMessages;
    tracktion::MidiMessageArray output;
    output.isAllNotesOff = midi.isAllNotesOff;

    for (auto &m : midi) {
        if (m.isNoteOn()) {
            int channel = juce::jlimit(1, 16, m.getChannel()) - 1;
            auto notes = midifx::Chords::build(
                m.getNoteNumber(), choice(chord.get()), choice(inversion.get()),
                choice(spread.get()));
            sounding[size_t(channel)][size_t(m.getNoteNumber())] = notes;
            for (int n : notes) {
                auto on = m;
                on.setNoteNumber(n);
                output.add(on);
            }
        } else if (m.isNoteOff()) {
            int channel = juce::jlimit(1, 16, m.getChannel()) - 1;
            auto &notes = sounding[size_t(channel)][size_t(m.getNoteNumber())];
            if (notes.isEmpty()) {
                output.add(m);
            } else {
                for (int n : notes) {
                    auto off = m;
                    off.setNoteNumber(n);
                    output.add(off);
                }
                notes.clear();
            }
        } else {
            output.add(m);
        }
    }

    midi.clear();
    midi.isAllNotesOff = output.isAllNotesOff;
    midi.mergeFrom(output);
}

} // namespace internal_plugins
