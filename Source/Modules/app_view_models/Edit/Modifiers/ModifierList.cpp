#include "ModifierList.h"

namespace app_view_models {

ModifierList::ModifierList(tracktion::Edit &e) : edit(e) {}

juce::Array<juce::Identifier> ModifierList::getModifierIdentifiers() {
    juce::Array<juce::Identifier> identifiers;
    for (auto modifierListItem : getModifierListItems()) {
        identifiers.add(modifierListItem.identifier);
    }

    return identifiers;
}

juce::Array<ModifierList::ModifierListItem>
ModifierList::getModifierListItems() {
    juce::Array<ModifierList::ModifierListItem> modifiers;

    // Display names are fixed rather than read from temporary modifier
    // instances: some modifiers start timers when constructed, and a
    // temporary LFO that was never initialised dereferences a null timer
    // in its destructor
    modifiers.add(ModifierListItem(tracktion::IDs::LFO, "LFO Modifier"));
    modifiers.add(ModifierListItem(tracktion::IDs::STEP, "Step"));
    modifiers.add(ModifierListItem(tracktion::IDs::RANDOM, "Random"));
    modifiers.add(
        ModifierListItem(tracktion::IDs::ENVELOPEFOLLOWER, "Envelope"));
    modifiers.add(
        ModifierListItem(tracktion::IDs::MIDITRACKER, "MIDI Tracker"));

    return modifiers;
}
} // namespace app_view_models