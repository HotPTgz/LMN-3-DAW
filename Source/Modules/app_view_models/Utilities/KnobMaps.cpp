#include "KnobMaps.h"
#include <yaml-cpp/yaml.h>

namespace app_view_models::KnobMaps {

namespace {
const char *obxdMap = R"(# Knob map for OB-Xd (OB-X style analog synth).
# Each page holds up to 8 knobs: the first 4 on the encoders, the next 4
# while holding Control. A knob is "LABEL = parameter name", and other
# spellings of the name can follow, separated by |.
plugin: OB-Xd
match: [obxd, ob-xd]
pages:
  - name: OSC
    knobs:
      - OSC 1 PITCH = Osc1Pitch
      - OSC 2 PITCH = Osc2Pitch
      - OSC 1 MIX = Osc1Mix
      - OSC 2 MIX = Osc2Mix
      - DETUNE = Oscillator2detune
      - PULSE WIDTH = PulseWidth
      - SYNC = Osc2HardSync
      - NOISE = NoiseMix
  - name: WAVE
    knobs:
      - OSC 1 SAW = Osc1Saw
      - OSC 1 PULSE = Osc1Pulse
      - OSC 2 SAW = Osc2Saw
      - OSC 2 PULSE = Osc2Pulse
      - CROSS MOD = Xmod
      - BRIGHTNESS = Brightness
      - OCTAVE = Octave
      - TUNE = Tune
  - name: FILTER
    knobs:
      - CUTOFF = Cutoff
      - RESONANCE = Resonance
      - ENV AMOUNT = FilterEnvAmount
      - KEY FOLLOW = FilterKeyFollow
      - MODE = Multimode
      - 4 POLE = FourPole
      - BANDPASS = BandpassBlend
      - WARM = Filter_Warm
  - name: AMP
    knobs:
      - ATTACK = Attack
      - DECAY = Decay
      - SUSTAIN = Sustain
      - RELEASE = Release
      - VOLUME = Volume
      - VOICES = VoiceCount
      - UNISON = Unison
      - SPREAD = VoiceDetune
  - name: FILTER ENV
    knobs:
      - ATTACK = FilterAttack
      - DECAY = FilterDecay
      - SUSTAIN = FilterSustain
      - RELEASE = FilterRelease
      - ENV > PITCH = EnvelopeToPitch
      - ENV > PW = PwEnv
      - VELO > AMP = VAmpFactor
      - VELO > FILT = VFltFactor
  - name: LFO
    knobs:
      - RATE = LfoFrequency
      - AMOUNT 1 = LfoAmount1
      - AMOUNT 2 = LfoAmount2
      - SINE = LfoSineWave
      - SQUARE = LfoSquareWave
      - S&H = LfoSampleHoldWave
      - TO OSC 1 = LfoOsc1
      - TO FILTER = LfoFilter
  - name: PLAY
    knobs:
      - GLIDE = Portamento
      - LEGATO = LegatoMode
      - BEND RANGE = BendRange
      - VIBRATO = VibratoRate
      - SLOP GLIDE = PortamentoDetune
      - SLOP FILTER = FilterDetune
      - SLOP ENV = EnvelopeDetune
      - LFO SYNC = LfoSync
)";

const char *dexedMap = R"(# Knob map for Dexed (DX7 style FM synth).
# Each page holds up to 8 knobs: the first 4 on the encoders, the next 4
# while holding Control. A knob is "LABEL = parameter name", and other
# spellings of the name can follow, separated by | (Dexed 0.9 and 1.x name
# some parameters differently; case and spaces don't matter).
plugin: Dexed
match: [dexed]
pages:
  - name: VOICE
    knobs:
      - ALGORITHM = ALGORITHM
      - FEEDBACK = FEEDBACK
      - TRANSPOSE = TRANSPOSE|MIDDLE C
      - VOLUME = Output
      - CUTOFF = Cutoff
      - RESONANCE = Resonance
      - TUNE = MASTER TUNE ADJ
      - MONO = MonoMode
  - name: LEVELS
    knobs:
      - OP 1 = OP1 OUTPUT LEVEL
      - OP 2 = OP2 OUTPUT LEVEL
      - OP 3 = OP3 OUTPUT LEVEL
      - OP 4 = OP4 OUTPUT LEVEL
      - OP 5 = OP5 OUTPUT LEVEL
      - OP 6 = OP6 OUTPUT LEVEL
  - name: RATIOS
    knobs:
      - OP 1 = OP1 F COARSE|OP1 Frequency Coarse
      - OP 2 = OP2 F COARSE|OP2 Frequency Coarse
      - OP 3 = OP3 F COARSE|OP3 Frequency Coarse
      - OP 4 = OP4 F COARSE|OP4 Frequency Coarse
      - OP 5 = OP5 F COARSE|OP5 Frequency Coarse
      - OP 6 = OP6 F COARSE|OP6 Frequency Coarse
  - name: FINE
    knobs:
      - OP 1 = OP1 F FINE|OP1 Frequency Fine
      - OP 2 = OP2 F FINE|OP2 Frequency Fine
      - OP 3 = OP3 F FINE|OP3 Frequency Fine
      - OP 4 = OP4 F FINE|OP4 Frequency Fine
      - OP 5 = OP5 F FINE|OP5 Frequency Fine
      - OP 6 = OP6 F FINE|OP6 Frequency Fine
  - name: DETUNE
    knobs:
      - OP 1 = OP1 OSC DETUNE|OP1 Detune
      - OP 2 = OP2 OSC DETUNE|OP2 Detune
      - OP 3 = OP3 OSC DETUNE|OP3 Detune
      - OP 4 = OP4 OSC DETUNE|OP4 Detune
      - OP 5 = OP5 OSC DETUNE|OP5 Detune
      - OP 6 = OP6 OSC DETUNE|OP6 Detune
  - name: LFO
    knobs:
      - SPEED = LFO SPEED|Lfo Rate
      - DELAY = LFO DELAY
      - PITCH DEPTH = LFO PM DEPTH
      - AMP DEPTH = LFO AM DEPTH|Lfo Amp Depth
      - WAVE = LFO WAVE|Lfo Waveform
      - KEY SYNC = LFO KEY SYNC
      - PITCH SENS = P MODE SENS.|Pitch Mod Sens
      - OSC SYNC = OSC KEY SYNC
  - name: OP 1 ENV
    knobs:
      - RATE 1 = OP1 EG RATE 1
      - RATE 2 = OP1 EG RATE 2
      - RATE 3 = OP1 EG RATE 3
      - RATE 4 = OP1 EG RATE 4
      - LEVEL 1 = OP1 EG LEVEL 1
      - LEVEL 2 = OP1 EG LEVEL 2
      - LEVEL 3 = OP1 EG LEVEL 3
      - LEVEL 4 = OP1 EG LEVEL 4
)";

Knob parseKnob(const juce::String &text) {
    Knob knob;
    auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return knob;
    auto label = trimmed.upToFirstOccurrenceOf("=", false, false).trim();
    auto names = trimmed.fromFirstOccurrenceOf("=", false, false).trim();
    if (!trimmed.contains("=")) {
        // just a parameter name: use it as the label too
        names = trimmed;
        label = trimmed.upToFirstOccurrenceOf("|", false, false).trim();
    }
    knob.label = label.toUpperCase();
    for (auto name : juce::StringArray::fromTokens(names, "|", ""))
        if (name.trim().isNotEmpty())
            knob.names.add(name.trim());
    return knob;
}
} // namespace

bool Map::matches(const juce::String &pluginName) const {
    auto name = pluginName.toLowerCase();
    for (auto &m : match)
        if (m.isNotEmpty() && name.contains(m.toLowerCase()))
            return true;
    return plugin.isNotEmpty() && name == plugin.toLowerCase();
}

Map parse(const juce::String &yamlText) {
    Map map;
    try {
        auto root = YAML::Load(yamlText.toStdString());
        if (!root.IsMap())
            return {};
        if (root["plugin"])
            map.plugin = juce::String(root["plugin"].as<std::string>());
        if (auto match = root["match"]) {
            if (match.IsSequence()) {
                for (auto item : match)
                    map.match.add(juce::String(item.as<std::string>()));
            } else {
                map.match.add(juce::String(match.as<std::string>()));
            }
        }
        if (auto pages = root["pages"]) {
            for (auto pageNode : pages) {
                Page page;
                if (pageNode["name"])
                    page.name =
                        juce::String(pageNode["name"].as<std::string>());
                if (auto knobs = pageNode["knobs"])
                    for (auto knobNode : knobs) {
                        if (page.knobs.size() >= 8)
                            break;
                        page.knobs.push_back(parseKnob(
                            juce::String(knobNode.as<std::string>(""))));
                    }
                bool any = false;
                for (auto &k : page.knobs)
                    any = any || !k.isEmpty();
                if (any)
                    map.pages.push_back(page);
            }
        }
    } catch (const std::exception &e) {
        juce::Logger::writeToLog("Knob map could not be read: " +
                                 juce::String(e.what()));
        return {};
    }
    return map;
}

juce::String normalise(const juce::String &name) {
    juce::String out;
    for (auto c : name.toLowerCase())
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            out += c;
    return out;
}

int findParameter(const juce::StringArray &parameterNames, const Knob &knob) {
    for (auto &wanted : knob.names) {
        auto key = normalise(wanted);
        for (int i = 0; i < parameterNames.size(); i++)
            if (normalise(parameterNames[i]) == key)
                return i;
    }
    return -1;
}

const std::vector<std::pair<juce::String, juce::String>> &getDefaultMaps() {
    static const std::vector<std::pair<juce::String, juce::String>> maps{
        {"OB-Xd.yaml", juce::String::fromUTF8(obxdMap)},
        {"Dexed.yaml", juce::String::fromUTF8(dexedMap)}};
    return maps;
}

juce::File getDirectory() {
    return juce::File::getSpecialLocation(
               juce::File::userApplicationDataDirectory)
#ifdef JUCE_APPLICATION_NAME_STRING
        .getChildFile(JUCE_APPLICATION_NAME_STRING)
#else
        .getChildFile("LMN-3")
#endif
        .getChildFile("knob_maps");
}

void installDefaults(const juce::File &directory) {
    directory.createDirectory();
    for (auto &[fileName, text] : getDefaultMaps()) {
        auto file = directory.getChildFile(fileName);
        if (!file.existsAsFile())
            file.replaceWithText(text);
    }
}

Map findMapFor(const juce::String &pluginName, const juce::File &directory) {
    for (auto &file : directory.findChildFiles(juce::File::findFiles, false,
                                               "*.yaml;*.yml")) {
        auto map = parse(file.loadFileAsString());
        if (map.isValid() && map.matches(pluginName))
            return map;
    }
    return {};
}

} // namespace app_view_models::KnobMaps
