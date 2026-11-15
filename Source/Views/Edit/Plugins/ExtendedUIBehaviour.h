#pragma once
#include "App.h"
#include "FourOscView.h"
#include "InternalPluginView.h"
#include "LowPassPluginView.h"
#include "PlayfulFactory.h"
#include "PlayfulSamplerView.h"
#include "PlayfulVst3Screen.h"
#include "SamplerView.h"
#include <app_services/app_services.h>
#include <internal_plugins/internal_plugins.h>
#include <tracktion_engine/tracktion_engine.h>

//==============================================================================
class ExtendedUIBehaviour : public tracktion::UIBehaviour {
  public:
    ExtendedUIBehaviour() = default;

    std::unique_ptr<juce::Component>
    createPluginWindow(tracktion::PluginWindowState &pws) override {
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        if (auto ws = dynamic_cast<tracktion::Plugin::WindowState *>(&pws)) {
            // VST3 plugins get a playful screen on the encoders (their
            // knob map, or every parameter 8 to a page)
            if (auto vst3 =
                    createPlayfulVst3View(&ws->plugin, *midiCommandManager))
                return vst3;

            if (auto externalPlugin =
                    dynamic_cast<tracktion::ExternalPlugin *>(&(ws->plugin))) {
                if (auto pi = externalPlugin->getAudioPluginInstance()) {
                    editor.reset(pi->createEditorIfNeeded());

                    if (editor == nullptr) {
                        juce::Logger::writeToLog("using generic editor");
                        editor =
                            std::make_unique<juce::GenericAudioProcessorEditor>(
                                *pi);
                    }

                    editor->setWantsKeyboardFocus(false);
                    return editor;
                }
            }

            if (auto samplerPlugin =
                    dynamic_cast<tracktion::SamplerPlugin *>(&(ws->plugin))) {
                if (auto drumSamplerPlugin =
                        dynamic_cast<internal_plugins::DrumSamplerPlugin *>(
                            samplerPlugin)) {
                    return std::make_unique<PlayfulSamplerView>(
                        drumSamplerPlugin, *midiCommandManager);
                } else {
                    return std::make_unique<PlayfulSamplerView>(
                        samplerPlugin, *midiCommandManager);
                }
            }

            // the playful screens for the knob-based plugins
            if (auto playful =
                    createPlayfulPluginView(&ws->plugin, *midiCommandManager))
                return playful;

            if (auto reverbPlugin =
                    dynamic_cast<tracktion::ReverbPlugin *>(&(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(reverbPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto delayPlugin =
                    dynamic_cast<tracktion::DelayPlugin *>(&(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(delayPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto lowPassPlugin =
                    dynamic_cast<tracktion::LowPassPlugin *>(&(ws->plugin))) {
                std::unique_ptr<LowPassPluginView> lowPassPluginView =
                    std::make_unique<LowPassPluginView>(lowPassPlugin,
                                                        *midiCommandManager);
                return lowPassPluginView;
            }

            if (auto phaserPlugin =
                    dynamic_cast<tracktion::PhaserPlugin *>(&(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(phaserPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto chorusPlugin =
                    dynamic_cast<tracktion::ChorusPlugin *>(&(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(chorusPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto equaliserPlugin =
                    dynamic_cast<tracktion::EqualiserPlugin *>(&(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(equaliserPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto compressorPlugin =
                    dynamic_cast<tracktion::CompressorPlugin *>(
                        &(ws->plugin))) {
                std::unique_ptr<InternalPluginView> internalPluginView =
                    std::make_unique<InternalPluginView>(compressorPlugin,
                                                         *midiCommandManager);
                return internalPluginView;
            }

            if (auto midiFx = dynamic_cast<internal_plugins::MidiFxPlugin *>(
                    &(ws->plugin))) {
                using VM = app_view_models::ParameterListPluginViewModel;
                juce::Array<VM::ParameterSpec> specs;
                for (auto &control : midiFx->getControls())
                    specs.add(VM::spec(control.parameter->getParameterName(),
                                       control.parameter, control.interval));
                return std::make_unique<InternalPluginView>(
                    std::make_unique<VM>(midiFx, specs), *midiCommandManager);
            }

            if (auto pitchShiftPlugin =
                    dynamic_cast<tracktion::PitchShiftPlugin *>(
                        &(ws->plugin))) {
                using VM = app_view_models::ParameterListPluginViewModel;
                return std::make_unique<InternalPluginView>(
                    std::make_unique<VM>(
                        pitchShiftPlugin,
                        juce::Array<VM::ParameterSpec>{VM::spec(
                            "Semitones", pitchShiftPlugin->semitones, 1.0)}),
                    *midiCommandManager);
            }

            if (auto fourOscPlugin =
                    dynamic_cast<tracktion::FourOscPlugin *>(&(ws->plugin))) {
                std::unique_ptr<FourOscView> fourOscView =
                    std::make_unique<FourOscView>(fourOscPlugin,
                                                  *midiCommandManager);
                return fourOscView;
            }

            std::unique_ptr<InternalPluginView> internalPluginView =
                std::make_unique<InternalPluginView>(&ws->plugin,
                                                     *midiCommandManager);
            return internalPluginView;
        }

        juce::Logger::writeToLog("failed to create editor");
        return {};
    }

    void setEdit(tracktion::Edit *e) { edit = e; }

    void setMidiCommandManager(app_services::MidiCommandManager *mcm) {
        midiCommandManager = mcm;
    }

    void setApp(App *a) { app = a; }

    tracktion::Edit *getCurrentlyFocusedEdit() override { return edit; }

    void
    runTaskWithProgressBar(tracktion::ThreadPoolJobWithProgress &t) override {
        TaskRunner runner(t);
        if (app != nullptr) {
            app->showProgressView();
        }

        while (runner.isThreadRunning()) {
#if JUCE_ANDROID || JUCE_IOS
            // no nested message loops on phones: just wait
            juce::Thread::sleep(10);
#else
            if (!juce::MessageManager::getInstance()->runDispatchLoopUntil(
                    10)) {
                // Can update the GUI here if we need to, but since its just a
                // continuous loading animation we dont need to do anything for
                // now
            }
#endif
        }

        // Once the thread finishes, hide the progress view
        if (app != nullptr)
            app->hideProgressView();
    }

  private:
    tracktion::Edit *edit = nullptr;
    app_services::MidiCommandManager *midiCommandManager = nullptr;
    App *app = nullptr;

    struct TaskRunner : public juce::Thread {
        explicit TaskRunner(tracktion::ThreadPoolJobWithProgress &t)
            : Thread(t.getJobName()), task(t) {
            startThread();
        }

        ~TaskRunner() override {
            task.signalJobShouldExit();
            waitForThreadToExit(10000);
        }

        void run() override {
            while (!threadShouldExit())
                if (task.runJob() == juce::ThreadPoolJob::jobHasFinished)
                    break;
        }

        tracktion::ThreadPoolJobWithProgress &task;
    };
};