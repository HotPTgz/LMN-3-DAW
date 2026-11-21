#include "PlayfulFactory.h"
#include "PlayfulParamScreen.h"
#include "PlayfulScenes.h"
#include <app_configuration/app_configuration.h>
#include <app_view_models/app_view_models.h>
#include <internal_plugins/internal_plugins.h>

using juce::String;
using Param = PlayfulParam;
using APPtr = tracktion::AutomatableParameter::Ptr;

namespace {

String percent(double v) { return String(juce::roundToInt(v * 100.0)) + "%"; }

String signedText(double v, int decimals, const String &unit) {
    return (v > 0 ? "+" : "") + String(v, decimals) + unit;
}

// "Length (ms)" -> "LENGTH"
String cleanLabel(const String &name) {
    return name.upToFirstOccurrenceOf(" (", false, false).toUpperCase();
}

// Encoder values straight from a view model (keeps its ranges, steps and
// "don't fight a modifier" rules)
template <typename VM> std::vector<Param> paramsFrom(VM *vm) {
    std::vector<Param> params;
    for (int i = 0; i < vm->getNumberOfParameters(); i++) {
        Param p;
        p.label = cleanLabel(vm->getParameterName(i));
        p.normalised = [vm, i] {
            auto r = vm->getParameterRange(i);
            if (r.getLength() <= 0)
                return 0.0f;
            return float((vm->getParameterValue(i) - r.getStart()) /
                         r.getLength());
        };
        p.text = [vm, i] {
            return vm->getParameterValueText(i, vm->getParameterValue(i));
        };
        p.nudge = [vm, i](int direction) {
            auto r = vm->getParameterRange(i);
            vm->setParameterValue(
                i, r.clipValue(vm->getParameterValue(i) +
                               direction * vm->getParameterInterval(i)));
        };
        params.push_back(p);
    }
    return params;
}

// Override how a view model value is shown
template <typename VM>
void showAs(std::vector<Param> &params, VM *vm, int index,
            std::function<String(double)> format, const String &label = {}) {
    if (index >= int(params.size()))
        return;
    params[size_t(index)].text = [vm, index, format] {
        return format(vm->getParameterValue(index));
    };
    if (label.isNotEmpty())
        params[size_t(index)].label = label;
}

// An encoder value straight from an automatable parameter.
// step > 0: real-value step per detent, otherwise 1% of the range.
Param fromParameter(APPtr parameter, const String &label,
                    std::function<String(float)> format = {}, float step = 0,
                    bool reversed = false) {
    Param p;
    p.label = label;
    p.normalised = [parameter, reversed] {
        float n = parameter->getCurrentNormalisedValue();
        return reversed ? 1.0f - n : n;
    };
    p.text = [parameter, format] {
        return format ? format(parameter->getCurrentValue())
                      : parameter->getCurrentValueAsString();
    };
    p.nudge = [parameter, step, reversed](int direction) {
        // a modifier is driving it; leave it alone
        if (parameter->getModifiers().size() > 0)
            return;
        if (reversed)
            direction = -direction;
        if (step > 0)
            parameter->setParameter(
                parameter->getValueRange().clipValue(
                    parameter->getCurrentBaseValue() + direction * step),
                juce::sendNotification);
        else
            parameter->setNormalisedParameter(
                juce::jlimit(0.0f, 1.0f,
                             parameter->getCurrentNormalisedValue() +
                                 direction * 0.01f),
                juce::sendNotification);
    };
    return p;
}

int choiceOf(APPtr parameter) {
    return juce::roundToInt(parameter->getCurrentValue());
}

std::unique_ptr<juce::Component> screen(app_services::MidiCommandManager &mcm,
                                        const String &title,
                                        std::vector<Param> params,
                                        std::unique_ptr<PlayfulScene> scene,
                                        std::shared_ptr<void> keepAlive = {}) {
    return std::make_unique<PlayfulParamScreen>(
        mcm, title, std::move(params), std::move(scene), std::move(keepAlive));
}

// MIDI effects use the same parameter list as their old knob screen
std::shared_ptr<app_view_models::ParameterListPluginViewModel>
midiFxViewModel(internal_plugins::MidiFxPlugin *midiFx) {
    using VM = app_view_models::ParameterListPluginViewModel;
    juce::Array<VM::ParameterSpec> specs;
    for (auto &control : midiFx->getControls())
        specs.add(VM::spec(control.parameter->getParameterName(),
                           control.parameter, control.interval));
    return std::make_shared<VM>(midiFx, specs);
}

// The built-in audio effects: their knob list
std::shared_ptr<app_view_models::ParameterListPluginViewModel>
fxViewModel(internal_plugins::AudioFxPlugin *fx) {
    using VM = app_view_models::ParameterListPluginViewModel;
    juce::Array<VM::ParameterSpec> specs;
    for (auto &control : fx->getControls())
        specs.add(VM::spec(control.parameter->getParameterName(),
                           control.parameter, control.interval));
    return std::make_shared<VM>(fx, specs);
}

// Param from increment/decrement functions
Param stepper(const String &label, std::function<float()> normalised,
              std::function<String()> text, std::function<void()> up,
              std::function<void()> down) {
    Param p;
    p.label = label;
    p.normalised = std::move(normalised);
    p.text = std::move(text);
    p.nudge = [up, down](int direction) {
        if (direction > 0)
            up();
        else
            down();
    };
    return p;
}

String seconds(float s) {
    if (s < 1.0f)
        return String(juce::roundToInt(s * 1000.0f)) + " ms";
    return String(s, 2) + " s";
}

struct FourOscViewModels {
    explicit FourOscViewModels(tracktion::FourOscPlugin *p)
        : adsr(p), filter(p) {
        for (int i = 0; i < 4; i++)
            oscillators.push_back(
                std::make_unique<app_view_models::OscillatorViewModel>(p, i));
    }
    std::vector<std::unique_ptr<app_view_models::OscillatorViewModel>>
        oscillators;
    app_view_models::ADSRViewModel adsr;
    app_view_models::FilterViewModel filter;
};

// 4OSC: pages Osc 1-4, Envelope and Filter, 8 values each (4 with Control)
std::unique_ptr<juce::Component>
createFourOscScreen(tracktion::FourOscPlugin *plugin,
                    app_services::MidiCommandManager &mcm) {
    namespace S = PlayfulScenes;
    auto vms = std::make_shared<FourOscViewModels>(plugin);
    std::vector<Param> params;
    std::vector<std::unique_ptr<PlayfulScene>> scenes;
    juce::StringArray titles;

    static const char *waveNames[] = {"OFF", "SINE",     "SQUARE",
                                      "SAW", "TRIANGLE", "NOISE"};
    for (int i = 0; i < 4; i++) {
        auto *o = vms->oscillators[size_t(i)].get();
        auto *op = plugin->oscParams[i];
        params.push_back(stepper(
            "WAVE", [o] { return o->getWaveShape() / 5.0f; },
            [o] {
                return String(waveNames[juce::jlimit(0, 5, o->getWaveShape())]);
            },
            [o] { o->incrementWaveShape(); },
            [o] { o->decrementWaveShape(); }));
        params.push_back(stepper(
            "VOICES", [o] { return (o->getVoices() - 1) / 7.0f; },
            [o] { return String(o->getVoices()); },
            [o] { o->incrementVoices(); }, [o] { o->decrementVoices(); }));
        params.push_back(stepper(
            "TUNE", [o] { return o->getTune(); },
            [op] {
                int st = juce::roundToInt(op->tune->getCurrentValue());
                return (st > 0 ? "+" : "") + String(st) + " st";
            },
            [o] { o->incrementTune(); }, [o] { o->decrementTune(); }));
        params.push_back(stepper(
            "FINE", [o] { return o->getFineTune(); },
            [op] {
                return signedText(
                    juce::roundToInt(op->fineTune->getCurrentValue()), 0,
                    " ct");
            },
            [o] { o->incrementFineTune(); }, [o] { o->decrementFineTune(); }));
        params.push_back(stepper(
            "DETUNE", [o] { return o->getDetune(); },
            [o] { return percent(o->getDetune()); },
            [o] { o->incrementDetune(); }, [o] { o->decrementDetune(); }));
        params.push_back(stepper(
            "LEVEL", [o] { return o->getLevel(); },
            [op] { return String(op->level->getCurrentValue(), 1) + " dB"; },
            [o] { o->incrementLevel(); }, [o] { o->decrementLevel(); }));
        params.push_back(stepper(
            "PULSE WIDTH", [o] { return o->getPulseWidth(); },
            [op] { return percent(op->pulseWidth->getCurrentValue()); },
            [o] { o->incrementPulseWidth(); },
            [o] { o->decrementPulseWidth(); }));
        params.push_back(stepper(
            "SPREAD", [o] { return o->getSpread(); },
            [op] {
                return signedText(
                    juce::roundToInt(op->spread->getCurrentValue()), 0, "%");
            },
            [o] { o->incrementSpread(); }, [o] { o->decrementSpread(); }));

        scenes.push_back(S::oscillator(
            [o] { return o->getWaveShape(); }, [o] { return o->getVoices(); },
            [op] {
                int st = juce::roundToInt(op->tune->getCurrentValue());
                int ct = juce::roundToInt(op->fineTune->getCurrentValue());
                return (st > 0 ? "+" : "") + String(st) + " st   " +
                       (ct > 0 ? "+" : "") + String(ct) + " ct";
            }));
        titles.add(Playful::utf8("4OSC · Osc ") + String(i + 1));
    }

    auto *a = &vms->adsr;
    params.push_back(stepper(
        "ATTACK", [a] { return a->getAttack(); },
        [plugin] { return seconds(plugin->ampAttack->getCurrentValue()); },
        [a] { a->incrementAttack(); }, [a] { a->decrementAttack(); }));
    params.push_back(stepper(
        "DECAY", [a] { return a->getDecay(); },
        [plugin] { return seconds(plugin->ampDecay->getCurrentValue()); },
        [a] { a->incrementDecay(); }, [a] { a->decrementDecay(); }));
    params.push_back(stepper(
        "SUSTAIN", [a] { return a->getSustain(); },
        [plugin] {
            return String(juce::roundToInt(
                       plugin->ampSustain->getCurrentValue())) +
                   "%";
        },
        [a] { a->incrementSustain(); }, [a] { a->decrementSustain(); }));
    params.push_back(stepper(
        "RELEASE", [a] { return a->getRelease(); },
        [plugin] { return seconds(plugin->ampRelease->getCurrentValue()); },
        [a] { a->incrementRelease(); }, [a] { a->decrementRelease(); }));
    for (int i = 0; i < 4; i++)
        params.push_back(Param::none());
    scenes.push_back(S::ampEnvelope(
        [a] { return a->getAttack(); }, [a] { return a->getDecay(); },
        [a] { return a->getSustain(); }, [a] { return a->getRelease(); }));
    titles.add(Playful::utf8("4OSC · Envelope"));

    auto *f = &vms->filter;
    static const char *typeNames[] = {"OFF", "LOW-PASS", "HIGH-PASS",
                                      "BAND-PASS", "NOTCH"};
    params.push_back(stepper(
        "FREQUENCY",
        [f] {
            auto r = f->getFrequencyRange();
            return float(std::log(f->getFrequency() / r.getStart()) /
                         std::log(r.getEnd() / r.getStart()));
        },
        [f] { return PlayfulScenes::formatFrequency(f->getFrequency()); },
        [f] { f->incrementFrequency(); }, [f] { f->decrementFrequency(); }));
    params.push_back(stepper(
        "RESONANCE", [f] { return f->getResonance(); },
        [f] { return percent(f->getResonance()); },
        [f] { f->incrementResonance(); }, [f] { f->decrementResonance(); }));
    params.push_back(stepper(
        "ENV AMOUNT", [f] { return f->getEnvelopeAmount(); },
        [f] { return percent(f->getEnvelopeAmount()); },
        [f] { f->incrementEnvelopeAmount(); },
        [f] { f->decrementEnvelopeAmount(); }));
    params.push_back(stepper(
        "TYPE", [f] { return f->getFilterType() / 4.0f; },
        [f] {
            return String(typeNames[juce::jlimit(0, 4, f->getFilterType())]);
        },
        [f] { f->incrementFilterType(); }, [f] { f->decrementFilterType(); }));
    params.push_back(stepper(
        "ENV ATTACK", [f] { return f->getAttack(); },
        [plugin] { return seconds(plugin->filterAttack->getCurrentValue()); },
        [f] { f->incrementAttack(); }, [f] { f->decrementAttack(); }));
    params.push_back(stepper(
        "ENV DECAY", [f] { return f->getDecay(); },
        [plugin] { return seconds(plugin->filterDecay->getCurrentValue()); },
        [f] { f->incrementDecay(); }, [f] { f->decrementDecay(); }));
    params.push_back(stepper(
        "ENV SUSTAIN", [f] { return f->getSustain(); },
        [plugin] {
            return String(juce::roundToInt(
                       plugin->filterSustain->getCurrentValue())) +
                   "%";
        },
        [f] { f->incrementSustain(); }, [f] { f->decrementSustain(); }));
    params.push_back(stepper(
        "ENV RELEASE", [f] { return f->getRelease(); },
        [plugin] { return seconds(plugin->filterRelease->getCurrentValue()); },
        [f] { f->incrementRelease(); }, [f] { f->decrementRelease(); }));
    scenes.push_back(S::oscFilter([f] { return f->getFrequency(); },
                                  [f] { return f->getResonance(); },
                                  [f] { return f->getEnvelopeAmount(); },
                                  [f] { return f->getFilterType(); }));
    titles.add(Playful::utf8("4OSC · Filter"));

    return screen(mcm, "4OSC", std::move(params),
                  S::paged(std::move(scenes), titles), vms);
}

// Built-in engines: SOUND page (engine's own 8 knobs) and SHAPE page
std::unique_ptr<juce::Component>
createEngineScreen(internal_plugins::EnginePlugin *engine,
                   app_services::MidiCommandManager &mcm) {
    namespace S = PlayfulScenes;
    using VM = app_view_models::ParameterListPluginViewModel;
    juce::Array<VM::ParameterSpec> specs;
    for (auto &control : engine->getControls())
        specs.add(VM::spec(control.parameter->getParameterName(),
                           control.parameter, control.interval));
    auto vm = std::make_shared<VM>(engine, specs);
    auto params = paramsFrom(vm.get());

    auto sound = [engine](int i) {
        return [engine, i] { return engine->getSoundValue(i); };
    };
    auto knob = [engine](int i) {
        return [engine, i] {
            return engine->getControls()[8 + i].parameter->getCurrentValue();
        };
    };

    std::unique_ptr<PlayfulScene> soundScene;
    namespace inst = internal_plugins::instruments;
    if (dynamic_cast<internal_plugins::WavetableEnginePlugin *>(engine))
        soundScene = S::wavetable(
            [engine] { return juce::roundToInt(engine->getSoundValue(0)); },
            sound(1), sound(2),
            [engine] {
                return internal_plugins::WavetableEnginePlugin::brightnessHz(
                    engine->getSoundValue(3));
            },
            inst::Wavetables::getNames(),
            [](int table, int frame) -> const std::vector<float> & {
                return inst::Wavetables::get().frame(table, frame);
            },
            [](double phase, float warp) {
                return inst::WavetableSound::warpPhase(phase, warp);
            });
    else if (dynamic_cast<internal_plugins::MalletsEnginePlugin *>(engine))
        soundScene = S::mallets(
            [engine] { return juce::roundToInt(engine->getSoundValue(0)); },
            sound(1), sound(2), sound(3), inst::Mallets::getMaterialNames());
    else if (auto grains =
                 dynamic_cast<internal_plugins::GranularEnginePlugin *>(engine)) {
        soundScene = S::granular(
            [grains] { return grains->getSample()->name; },
            [grains] { return grains->getSample()->overview; },
            [grains] {
                auto sample = grains->getSample();
                return float(sample->audio.getNumSamples() / sample->sampleRate);
            },
            sound(0), sound(1), sound(2), sound(3),
            [grains] { return grains->getPlayhead(); },
            [engine] { return engine->getSoundValue(6) > 0.5f; });
        // the 8th sound knob picks the sample (the level stays as it is)
        auto files = std::make_shared<juce::Array<juce::File>>();
        auto rescan = [grains, files] {
            files->clear();
            files->add(juce::File()); // the built-in voice
            auto dir = ConfigurationHelpers::getTempSamplesDirectory(
                grains->edit.engine);
            auto found = dir.findChildFiles(juce::File::findFiles, true,
                                            "*.wav;*.aif;*.aiff;*.flac");
            found.sort();
            files->addArray(found);
        };
        rescan();
        Param sample;
        sample.label = "SAMPLE";
        sample.text = [grains] { return grains->getSample()->name; };
        sample.normalised = [grains, files] {
            int index = juce::jmax(0, files->indexOf(grains->getSampleFile()));
            return files->size() > 1 ? index / float(files->size() - 1) : 0.0f;
        };
        sample.nudge = [grains, files, rescan](int direction) {
            rescan();
            int index = juce::jmax(0, files->indexOf(grains->getSampleFile()));
            index = juce::jlimit(0, files->size() - 1, index + direction);
            grains->setSampleFile((*files)[index]);
        };
        if (params.size() > 7)
            params[7] = sample;
    } else if (dynamic_cast<internal_plugins::StringEnginePlugin *>(engine))
        soundScene = S::stringEngine(sound(0), sound(1), sound(2), sound(3));
    else if (dynamic_cast<internal_plugins::FmEnginePlugin *>(engine))
        soundScene = S::fmEngine(
            [engine] { return juce::roundToInt(engine->getSoundValue(0)); },
            sound(1), sound(2), sound(3), sound(4), sound(5));
    else
        soundScene = S::pulseEngine(sound(0), sound(1), sound(2), sound(3));

    std::vector<std::unique_ptr<PlayfulScene>> scenes;
    scenes.push_back(std::move(soundScene));
    scenes.push_back(
        S::engineShape(knob(0), knob(1), knob(2), knob(3), knob(4), [engine] {
            return juce::roundToInt(
                engine->getControls()[15].parameter->getCurrentValue());
        }));

    auto name = engine->getName();
    return screen(mcm, name, std::move(params),
                  S::paged(std::move(scenes), {name, name}, {"SOUND", "SHAPE"}),
                  vm);
}

} // namespace

//==============================================================================
std::unique_ptr<juce::Component>
createPlayfulPluginView(tracktion::Plugin *plugin,
                        app_services::MidiCommandManager &mcm) {
    namespace S = PlayfulScenes;
    auto title = plugin->getName();

    if (auto reverb = dynamic_cast<tracktion::ReverbPlugin *>(plugin)) {
        auto vm =
            std::make_shared<app_view_models::ReverbPluginViewModel>(reverb);
        auto params = paramsFrom(vm.get());
        const char *labels[] = {"ROOM SIZE", "WIDTH", "DAMPING", "WET", "DRY"};
        for (int i = 0; i < 5; i++)
            showAs(params, vm.get(), i, percent, labels[i]);
        return screen(mcm, "Reverb", std::move(params),
                      S::reverb([reverb] { return reverb->getRoomSize(); },
                                [reverb] { return reverb->getWidth(); },
                                [reverb] { return reverb->getDamp(); },
                                [reverb] { return reverb->getWet(); }),
                      vm);
    }

    if (auto delay = dynamic_cast<tracktion::DelayPlugin *>(plugin)) {
        auto vm =
            std::make_shared<app_view_models::DelayPluginViewModel>(delay);
        auto params = paramsFrom(vm.get());
        showAs(params, vm.get(), 0,
               [](double v) { return String(juce::roundToInt(v)) + " ms"; });
        showAs(params, vm.get(), 1,
               [](double v) { return String(juce::roundToInt(v)) + " dB"; });
        showAs(params, vm.get(), 2, percent);
        return screen(
            mcm, "Delay", std::move(params),
            S::delay(
                [delay] { return float(delay->lengthMs.get()); },
                [delay] { return delay->feedbackDb->getCurrentValue(); },
                [delay] { return delay->mixProportion->getCurrentValue(); }),
            vm);
    }

    if (auto chorus = dynamic_cast<tracktion::ChorusPlugin *>(plugin)) {
        auto vm =
            std::make_shared<app_view_models::ChorusPluginViewModel>(chorus);
        auto params = paramsFrom(vm.get());
        showAs(params, vm.get(), 0,
               [](double v) { return String(v, 1) + " ms"; });
        showAs(params, vm.get(), 1,
               [](double v) { return String(v, 1) + " Hz"; });
        showAs(params, vm.get(), 2, percent);
        showAs(params, vm.get(), 3, percent);
        return screen(
            mcm, "Chorus", std::move(params),
            S::chorus([chorus] { return chorus->depthMs.get(); },
                      [chorus] { return chorus->speedHz.get(); },
                      [chorus] { return chorus->width.get(); },
                      [chorus] { return chorus->mixProportion.get(); }),
            vm);
    }

    if (auto phaser = dynamic_cast<tracktion::PhaserPlugin *>(plugin)) {
        auto vm =
            std::make_shared<app_view_models::PhaserPluginViewModel>(phaser);
        auto params = paramsFrom(vm.get());
        showAs(params, vm.get(), 0, [](double v) { return String(v, 1); });
        showAs(params, vm.get(), 1, percent);
        showAs(params, vm.get(), 2, percent);
        // the scene reads the knob positions (0-1)
        auto norm = [vm](int i) {
            return [vm, i] {
                auto r = vm->getParameterRange(i);
                return float((vm->getParameterValue(i) - r.getStart()) /
                             r.getLength());
            };
        };
        return screen(mcm, "Phaser", std::move(params),
                      S::phaser(norm(0), norm(1), norm(2)), vm);
    }

    if (auto comp = dynamic_cast<tracktion::CompressorPlugin *>(plugin)) {
        comp->useSidechainTrigger.setValue(false, nullptr);
        auto ratioText = [](float r) {
            return r <= 0.001f ? String("INF:1") : String(1.0f / r, 1) + ":1";
        };
        std::vector<Param> params = {
            fromParameter(comp->thresholdGain, "THRESHOLD",
                          [](float g) {
                              return String(juce::roundToInt(
                                         juce::Decibels::gainToDecibels(g))) +
                                     " dB";
                          }),
            // turning right means more squash, i.e. a higher ratio
            fromParameter(comp->ratio, "RATIO", ratioText, 0, true),
            fromParameter(
                comp->attackMs, "ATTACK",
                [](float v) { return String(juce::roundToInt(v)) + " ms"; }),
            fromParameter(
                comp->releaseMs, "RELEASE",
                [](float v) { return String(juce::roundToInt(v)) + " ms"; }),
            fromParameter(
                comp->outputDb, "OUTPUT",
                [](float v) { return signedText(v, 1, " dB"); }, 0.5f),
        };
        return screen(
            mcm, "Compressor", std::move(params),
            S::compressor(
                [comp] {
                    return juce::Decibels::gainToDecibels(
                        comp->thresholdGain->getCurrentValue());
                },
                [comp] {
                    return juce::jmin(
                        20.0f,
                        1.0f /
                            juce::jmax(0.05f, comp->ratio->getCurrentValue()));
                },
                [comp] { return comp->attackMs->getCurrentValue(); },
                [comp] { return comp->releaseMs->getCurrentValue(); }));
    }

    if (auto eq = dynamic_cast<tracktion::EqualiserPlugin *>(plugin)) {
        auto gain = [](float v) { return signedText(v, 1, " dB"); };
        auto freq = [](float v) { return PlayfulScenes::formatFrequency(v); };
        auto q = [](float v) { return "Q " + String(v, 2); };
        std::vector<Param> params = {
            fromParameter(eq->loGain, "LOW", gain, 0.5f),
            fromParameter(eq->midGain1, "LOW MID", gain, 0.5f),
            fromParameter(eq->midGain2, "HIGH MID", gain, 0.5f),
            fromParameter(eq->hiGain, "HIGH", gain, 0.5f),
            fromParameter(eq->loFreq, "LOW FREQ", freq),
            fromParameter(eq->midFreq1, "LOW MID FREQ", freq),
            fromParameter(eq->midFreq2, "HIGH MID FREQ", freq),
            fromParameter(eq->hiFreq, "HIGH FREQ", freq),
            fromParameter(eq->loQ, "LOW Q", q),
            fromParameter(eq->midQ1, "LOW MID Q", q),
            fromParameter(eq->midQ2, "HIGH MID Q", q),
            fromParameter(eq->hiQ, "HIGH Q", q),
        };
        auto value = [](APPtr p) {
            return [p] { return p->getCurrentValue(); };
        };
        return screen(mcm, "EQ", std::move(params),
                      S::equaliser({value(eq->loFreq), value(eq->midFreq1),
                                    value(eq->midFreq2), value(eq->hiFreq)},
                                   {value(eq->loGain), value(eq->midGain1),
                                    value(eq->midGain2), value(eq->hiGain)},
                                   [eq](float f) {
                                       return eq->getDBGainAtFrequency(f);
                                   }));
    }

    if (auto lowPass = dynamic_cast<tracktion::LowPassPlugin *>(plugin)) {
        auto vm =
            std::make_shared<app_view_models::LowPassPluginViewModel>(lowPass);
        auto *v = vm.get();
        Param frequency;
        frequency.label = "FREQUENCY";
        frequency.normalised = [v] {
            auto r = v->getFrequencyRange();
            return float(std::log(v->getFrequency() / r.getStart()) /
                         std::log(r.getEnd() / r.getStart()));
        };
        frequency.text = [v] {
            return PlayfulScenes::formatFrequency(float(v->getFrequency()));
        };
        frequency.nudge = [v](int direction) {
            if (direction > 0)
                v->incrementFrequency();
            else
                v->decrementFrequency();
        };
        Param mode;
        mode.label = "MODE";
        mode.normalised = [v] { return v->getIsLowPass() ? 0.25f : 0.75f; };
        mode.text = [v] {
            return String(v->getIsLowPass() ? "LOW-PASS" : "HIGH-PASS");
        };
        mode.nudge = [v](int direction) { v->setIsLowPass(direction < 0); };
        return screen(mcm, "LPF/HPF", {frequency, mode},
                      S::filter([v] { return float(v->getFrequency()); },
                                [v] { return v->getIsLowPass(); }),
                      vm);
    }

    if (auto pitch = dynamic_cast<tracktion::PitchShiftPlugin *>(plugin)) {
        std::vector<Param> params = {fromParameter(
            pitch->semitones, "SEMITONES",
            [](float v) {
                int st = juce::roundToInt(v);
                return (st > 0 ? "+" : "") + String(st) + " st";
            },
            1.0f)};
        return screen(mcm, "Pitch Shift", std::move(params),
                      S::pitchShift([pitch] {
                          return pitch->semitones->getCurrentValue();
                      }));
    }

    if (auto drums = dynamic_cast<internal_plugins::DrumSynthPlugin *>(plugin)) {
        using Kit = internal_plugins::instruments::DrumKit;
        // the knobs edit whichever pad was played last
        std::vector<Param> params;
        static const char *labels[] = {"PITCH", "PUNCH", "DECAY",
                                       "TONE",  "NOISE", "DRIVE"};
        for (int k = 0; k < Kit::numParams; k++) {
            Param p;
            p.label = labels[k];
            p.normalised = [drums, k] {
                return drums->getParameter(drums->selectedPad, k)
                    ->getCurrentNormalisedValue();
            };
            p.text = [drums, k] {
                return drums->getParameter(drums->selectedPad, k)
                    ->getCurrentValueAsString();
            };
            p.nudge = [drums, k](int direction) {
                auto param = drums->getParameter(drums->selectedPad, k);
                if (param->getModifiers().size() > 0)
                    return;
                param->setParameter(
                    juce::jlimit(0.0f, 1.0f,
                                 param->getCurrentBaseValue() + direction * 0.02f),
                    juce::sendNotification);
            };
            params.push_back(p);
        }
        return screen(
            mcm, "Drum Synth", std::move(params),
            S::drumSynth(
                [drums] { return drums->selectedPad.load(); },
                [drums](int pad) { drums->selectedPad = pad; },
                [drums](int pad) { return drums->lastHit[size_t(pad)].load(); },
                Kit::getPadNames(),
                [drums](int pad) {
                    auto p = drums->getPadParams(pad);
                    std::array<float, 6> a;
                    for (size_t k = 0; k < 6; k++)
                        a[k] = p[k];
                    return a;
                },
                [drums](int pad) {
                    return Kit::preview(pad, drums->getPadParams(pad), 22050.0,
                                        8820);
                },
                [drums](int pad) {
                    auto p = drums->getPadParams(pad);
                    String punch = p[1] > 0.6f ? ", punchy" : p[1] < 0.25f ? ", soft" : "";
                    return Kit::describe(pad, 0, p[0]) + punch;
                }));
    }

    if (auto fourOsc = dynamic_cast<tracktion::FourOscPlugin *>(plugin))
        return createFourOscScreen(fourOsc, mcm);

    if (auto engine = dynamic_cast<internal_plugins::EnginePlugin *>(plugin))
        return createEngineScreen(engine, mcm);

    using namespace internal_plugins;
    if (auto arp = dynamic_cast<ArpeggiatorPlugin *>(plugin)) {
        auto vm = midiFxViewModel(arp);
        return screen(
            mcm, "Arpeggiator", paramsFrom(vm.get()),
            S::arpeggiator(
                [arp] { return choiceOf(arp->mode); },
                [arp] {
                    return float(
                        midifx::Arp::getRateBeats(choiceOf(arp->rate)));
                },
                [arp] { return choiceOf(arp->octaves); },
                [arp] { return arp->gate->getCurrentValue(); },
                [arp] { return arp->latch->getCurrentValue() > 0.5f; }),
            vm);
    }

    if (auto scale = dynamic_cast<ScaleLockPlugin *>(plugin)) {
        auto vm = midiFxViewModel(scale);
        return screen(
            mcm, "Scale Lock", paramsFrom(vm.get()),
            S::scaleLock(
                [scale] { return choiceOf(scale->root); },
                [scale] {
                    return midifx::Scales::getNames()[choiceOf(scale->scale)];
                },
                [scale](int note) {
                    return midifx::Scales::snapToScale(
                        note, choiceOf(scale->root), choiceOf(scale->scale));
                }),
            vm);
    }

    if (auto chord = dynamic_cast<ChordPlugin *>(plugin)) {
        auto vm = midiFxViewModel(chord);
        return screen(
            mcm, "Chord", paramsFrom(vm.get()),
            S::chord(
                [chord](int root) {
                    return midifx::Chords::build(root, choiceOf(chord->chord),
                                                 choiceOf(chord->inversion),
                                                 choiceOf(chord->spread));
                },
                [chord] {
                    return midifx::Chords::getNames()[choiceOf(chord->chord)];
                },
                [chord] {
                    return midifx::Chords::getInversionNames()[choiceOf(
                        chord->inversion)];
                },
                [chord] {
                    return midifx::Chords::getSpreadNames()[choiceOf(
                        chord->spread)];
                }),
            vm);
    }

    if (auto tape = dynamic_cast<TapePlugin *>(plugin)) {
        auto vm = fxViewModel(tape);
        auto get = [tape](const char *id) {
            return [tape, id] { return tape->get(id); };
        };
        return screen(mcm, "Tape", paramsFrom(vm.get()),
                      S::tape(get("wow"), get("flutter"), get("saturate"),
                              get("hiss"), [tape] { return tape->getWobble(); }),
                      vm);
    }

    if (auto lofi = dynamic_cast<LofiPlugin *>(plugin)) {
        auto vm = fxViewModel(lofi);
        return screen(
            mcm, "Lo-fi", paramsFrom(vm.get()),
            S::lofi([lofi] { return lofi->get("bits"); },
                    [lofi] { return LofiPlugin::rateHz(lofi->get("rate")); },
                    [lofi] { return lofi->get("noise"); },
                    [lofi] { return lofi->get("mix"); }),
            vm);
    }

    if (auto drive = dynamic_cast<DrivePlugin *>(plugin)) {
        auto vm = fxViewModel(drive);
        return screen(
            mcm, "Drive", paramsFrom(vm.get()),
            S::drive([drive] { return drive->get("drive"); },
                     [drive] { return drive->get("tone"); },
                     [drive] { return drive->getChoice("driveType"); },
                     [drive] {
                         return fx::Drive::getTypeNames()[drive->getChoice(
                             "driveType")];
                     },
                     [drive] { return drive->get("mix"); },
                     [](float x, int type) { return fx::Drive::shape(x, type); }),
            vm);
    }

    if (auto filt = dynamic_cast<ResonantFilterPlugin *>(plugin)) {
        auto vm = fxViewModel(filt);
        return screen(
            mcm, "Filter", paramsFrom(vm.get()),
            S::resonantFilter(
                [filt] {
                    // the live cutoff while sound plays, else the knob's
                    float moving = filt->liveCutoff.load();
                    bool modulated = std::abs(filt->get("envelope")) > 0.01f ||
                                     filt->get("lfo") > 0.01f;
                    return modulated ? moving
                                     : ResonantFilterPlugin::cutoffHz(
                                           filt->get("cutoff"));
                },
                [filt] { return filt->get("resonance"); },
                [filt] { return filt->get("envelope"); },
                [filt] { return filt->get("lfo"); },
                [filt] { return filt->getChoice("filterType"); },
                fx::ResonantFilter::getTypeNames(),
                [filt] {
                    return ResonantFilterPlugin::getLfoRateNames()
                        [filt->getChoice("lfoRate")];
                }),
            vm);
    }

    if (auto duck = dynamic_cast<DuckerPlugin *>(plugin)) {
        auto vm = fxViewModel(duck);
        auto params = paramsFrom(vm.get());
        // SOURCE: the beat or another track (sidechain)
        auto sourceText = [duck] {
            int index = duck->getSourceIndex();
            if (index == 0)
                return String("Beat");
            auto name = duck->getSourceNames()[index];
            auto number = name.upToFirstOccurrenceOf(".", false, false);
            auto trackName = name.fromFirstOccurrenceOf(". ", false, false);
            return trackName.isEmpty() || trackName == number
                       ? "Track " + number
                       : trackName;
        };
        Param source;
        source.label = "SOURCE";
        source.text = sourceText;
        source.normalised = [duck] {
            int count = duck->getSourceNames().size();
            return count > 1 ? duck->getSourceIndex() / float(count - 1) : 0.0f;
        };
        source.nudge = [duck](int direction) {
            int count = duck->getSourceNames().size();
            duck->setSourceIndex(
                juce::jlimit(0, count - 1, duck->getSourceIndex() + direction));
        };
        params.insert(params.begin(), source);
        return screen(
            mcm, "Ducker", std::move(params),
            S::ducker(sourceText, [duck] { return duck->getSourceIndex() == 0; },
                      [duck](float seconds) {
                          fx::Ducker::Settings s;
                          s.depth = duck->get("depth");
                          s.releaseSeconds = duck->get("release");
                          s.shape = duck->getChoice("shape");
                          return fx::Ducker::gainAt(seconds, s);
                      },
                      [duck] { return duck->liveGain.load(); }),
            vm);
    }

    if (auto rep = dynamic_cast<BeatRepeatPlugin *>(plugin)) {
        auto vm = fxViewModel(rep);
        return screen(
            mcm, "Beat Repeat", paramsFrom(vm.get()),
            S::beatRepeat(
                [rep] {
                    return fx::BeatRepeat::getLengthNames()[rep->getChoice(
                        "length")];
                },
                [rep] {
                    return fx::BeatRepeat::getRepeatCount(
                        rep->getChoice("repeats"));
                },
                [rep] { return rep->get("decay"); },
                [rep] { return rep->get("pitch"); },
                [rep] { return rep->liveRepeat.load(); },
                [rep](bool down) { rep->setActive(down); }),
            vm);
    }

    if (auto fing = dynamic_cast<FingerPlugin *>(plugin)) {
        fing->updateKitNotes();
        std::vector<Param> params;
        params.push_back(fromParameter(fing->baseKey, "KEY", {}, 1.0f));
        static const char *labels[] = {"PATTERN", "VARIATION", "LENGTH"};
        for (int which = 0; which < 3; which++) {
            Param p;
            p.label = labels[which];
            p.normalised = [fing, which] {
                return fing->getSlotParameter(fing->selectedSlot, which)
                    ->getCurrentNormalisedValue();
            };
            p.text = [fing, which] {
                return fing->getSlotParameter(fing->selectedSlot, which)
                    ->getCurrentValueAsString();
            };
            p.nudge = [fing, which](int direction) {
                auto param = fing->getSlotParameter(fing->selectedSlot, which);
                param->setParameter(param->getValueRange().clipValue(
                                        param->getCurrentBaseValue() + direction),
                                    juce::sendNotification);
            };
            params.push_back(p);
        }
        return screen(
            mcm, "Finger", std::move(params),
            S::finger(
                [fing] { return juce::roundToInt(fing->baseKey->getCurrentValue()); },
                [fing] { return fing->selectedSlot.load(); },
                [fing](int slot) { fing->selectedSlot = slot; },
                [fing] { return fing->heldMask.load(); },
                [fing](int slot) {
                    std::array<int, 3> a;
                    for (int which = 0; which < 3; which++)
                        a[size_t(which)] = juce::roundToInt(
                            fing->getSlotParameter(slot, which)->getCurrentValue());
                    return a;
                },
                midifx::Finger::getPatternNames(),
                [](int pattern, int variation, int role, int step) {
                    return midifx::Finger::hit(pattern, variation, role, step);
                }));
    }

    if (auto rep = dynamic_cast<NoteRepeatPlugin *>(plugin)) {
        auto vm = midiFxViewModel(rep);
        return screen(
            mcm, "Note Repeat", paramsFrom(vm.get()),
            S::noteRepeat(
                [rep] {
                    return float(
                        midifx::NoteRepeat::getRateBeats(choiceOf(rep->rate)));
                },
                [rep] {
                    return midifx::NoteRepeat::getRateNames()[choiceOf(
                        rep->rate)];
                },
                [rep] { return rep->ramp->getCurrentValue(); },
                [rep] { return rep->gate->getCurrentValue(); },
                [rep] { return rep->swing->getCurrentValue(); }),
            vm);
    }

    if (auto st = dynamic_cast<StrumPlugin *>(plugin)) {
        auto vm = midiFxViewModel(st);
        return screen(
            mcm, "Strum", paramsFrom(vm.get()),
            S::strum([st] { return st->time->getCurrentValue(); },
                     [st] { return choiceOf(st->direction); },
                     [st] { return choiceOf(st->alternate) == 1; },
                     [st] { return choiceOf(st->curve); },
                     [](int i, int n, double gap, int curve) {
                         return midifx::Strum::offset(i, n, gap, curve);
                     }),
            vm);
    }

    if (auto hu = dynamic_cast<HumanisePlugin *>(plugin)) {
        auto vm = midiFxViewModel(hu);
        return screen(mcm, "Humanise", paramsFrom(vm.get()),
                      S::humanise([hu] { return hu->timing->getCurrentValue(); },
                                  [hu] { return hu->velocity->getCurrentValue(); },
                                  [hu] { return hu->length->getCurrentValue(); },
                                  [hu] { return hu->amount->getCurrentValue(); }),
                      vm);
    }

    if (auto echo = dynamic_cast<MidiEchoPlugin *>(plugin)) {
        auto vm = midiFxViewModel(echo);
        return screen(
            mcm, "MIDI Echo", paramsFrom(vm.get()),
            S::midiEcho(
                [echo] {
                    return midifx::Echo::getTimeNames()[choiceOf(echo->time)];
                },
                [echo] {
                    return float(
                        midifx::Echo::getTimeBeats(choiceOf(echo->time)));
                },
                [echo] { return choiceOf(echo->repeats); },
                [echo] { return echo->decay->getCurrentValue(); },
                [echo] { return choiceOf(echo->transpose); }),
            vm);
    }

    return nullptr;
}

//==============================================================================
std::unique_ptr<juce::Component>
createPlayfulModifierView(tracktion::Modifier *modifier,
                          app_services::MidiCommandManager &mcm) {
    namespace S = PlayfulScenes;

    // the step modifier has its own editor
    if (dynamic_cast<tracktion::StepModifier *>(modifier) != nullptr)
        return nullptr;

    std::shared_ptr<app_view_models::ModifierViewModel> vm =
        app_view_models::createModifierViewModel(modifier);
    auto params = paramsFrom(vm.get());
    auto title = modifier->getName();

    auto norm = [&params](int i) -> PlayfulScenes::Getter {
        if (i >= int(params.size()))
            return [] { return 0.0f; };
        return params[size_t(i)].normalised;
    };
    auto textOf = [&params](int i) -> PlayfulScenes::TextGetter {
        if (i >= int(params.size()))
            return [] { return String(); };
        return params[size_t(i)].text;
    };

    auto *v = vm.get();
    auto ms = [](double x) { return String(juce::roundToInt(x)) + " ms"; };

    std::unique_ptr<PlayfulScene> scene;
    if (auto lfo = dynamic_cast<tracktion::LFOModifier *>(modifier)) {
        showAs(params, v, 1,
               [](double x) { return String(x, x < 10 ? 2 : 1) + " Hz"; });
        showAs(params, v, 2, percent);
        showAs(params, v, 3, [](double x) {
            return String(juce::roundToInt(x * 360)) +
                   juce::String::fromUTF8("\u00b0");
        });
        showAs(params, v, 4, percent);
        scene = S::lfo([lfo] { return juce::roundToInt(lfo->wave.get()); },
                       norm(1), norm(2), norm(3));
        title = "LFO";
    } else if (dynamic_cast<tracktion::RandomModifier *>(modifier)) {
        showAs(params, v, 1, percent);
        showAs(params, v, 2, percent);
        showAs(params, v, 4, percent);
        scene = S::random(textOf(0), norm(1), norm(2));
        title = "Random";
    } else if (auto env = dynamic_cast<tracktion::EnvelopeFollowerModifier *>(
                   modifier)) {
        showAs(params, v, 0, percent);
        showAs(params, v, 1, ms);
        showAs(params, v, 2, ms);
        showAs(params, v, 3, [](double x) { return signedText(x, 1, " dB"); });
        showAs(params, v, 4, percent);
        showAs(params, v, 5, ms);
        scene = S::envelopeFollower(
            norm(0), [env] { return env->attackParam->getCurrentValue(); },
            [env] { return env->releaseParam->getCurrentValue(); },
            [env] { return env->gainDbParam->getCurrentValue(); });
        title = "Envelope";
    } else if (auto tracker =
                   dynamic_cast<tracktion::MIDITrackerModifier *>(modifier)) {
        showAs(params, v, 2, [](double x) {
            return PlayfulScenes::noteName(juce::roundToInt(x));
        });
        showAs(params, v, 3,
               [](double x) { return String(juce::roundToInt(x)) + " st"; });
        scene = S::midiTracker(
            [tracker] { return choiceOf(tracker->typeParam); }, textOf(1),
            [tracker] { return choiceOf(tracker->relativeRootParam); },
            [tracker] { return choiceOf(tracker->relativeSpreadParam); });
        title = "MIDI Tracker";
    } else {
        scene = S::generic({});
    }

    return screen(mcm, title, std::move(params), std::move(scene), vm);
}
