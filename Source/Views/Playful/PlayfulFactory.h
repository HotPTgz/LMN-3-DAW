#pragma once
#include <app_services/app_services.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

// Playful screens for the knob-based plugins and modifiers.
// Both return nullptr when there is no playful screen for the given type, so
// the caller can fall back to its usual view.
std::unique_ptr<juce::Component>
createPlayfulPluginView(tracktion::Plugin *plugin,
                        app_services::MidiCommandManager &mcm);

std::unique_ptr<juce::Component>
createPlayfulModifierView(tracktion::Modifier *modifier,
                          app_services::MidiCommandManager &mcm);
