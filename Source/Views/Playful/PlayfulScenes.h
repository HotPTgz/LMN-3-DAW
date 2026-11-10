#pragma once
#include "PlayfulParamScreen.h"

// The animated pictures for each playful plugin/modifier screen, ported from
// the playful design mockups. Each scene reads the values it shows through
// getters, so the scenes know nothing about Tracktion.
namespace PlayfulScenes {

using Getter = std::function<float()>;
using IntGetter = std::function<int()>;
using TextGetter = std::function<juce::String()>;
using juce::String;

std::unique_ptr<PlayfulScene> reverb(Getter roomSize, Getter width,
                                     Getter damping, Getter wet);
std::unique_ptr<PlayfulScene> delay(Getter lengthMs, Getter feedbackDb,
                                    Getter mix);
std::unique_ptr<PlayfulScene> chorus(Getter depthMs, Getter speedHz,
                                     Getter width, Getter mix);
std::unique_ptr<PlayfulScene> phaser(Getter depth01, Getter rate01,
                                     Getter feedback01);
std::unique_ptr<PlayfulScene> compressor(Getter thresholdDb, Getter ratio,
                                         Getter attackMs, Getter releaseMs);
// bandFreqs/bandGains: low, mid 1, mid 2, high. response: dB at a frequency
std::unique_ptr<PlayfulScene> equaliser(std::array<Getter, 4> bandFreqs,
                                        std::array<Getter, 4> bandGains,
                                        std::function<float(float)> response);
std::unique_ptr<PlayfulScene> filter(Getter frequency,
                                     std::function<bool()> isLowPass);
std::unique_ptr<PlayfulScene> pitchShift(Getter semitones);

std::unique_ptr<PlayfulScene> arpeggiator(IntGetter mode, Getter rateBeats,
                                          IntGetter octaves, Getter gate,
                                          std::function<bool()> latch);
// notesFor(root): the chord notes built on a root note
std::unique_ptr<PlayfulScene>
chord(std::function<juce::Array<int>(int)> notesFor, TextGetter chordName,
      TextGetter inversionName, TextGetter voicingName);
// snap(note): the note a key plays once snapped to the scale
std::unique_ptr<PlayfulScene> scaleLock(IntGetter root, TextGetter scaleName,
                                        std::function<int(int)> snap);

std::unique_ptr<PlayfulScene> lfo(IntGetter wave, Getter rate01, Getter depth01,
                                  Getter phase01);
std::unique_ptr<PlayfulScene> random(TextGetter rateText, Getter depth01,
                                     Getter smooth01);
std::unique_ptr<PlayfulScene> envelopeFollower(Getter depth01, Getter attackMs,
                                               Getter releaseMs, Getter gainDb);
// source 0 = pitch, 1 = velocity
std::unique_ptr<PlayfulScene> midiTracker(IntGetter source, TextGetter mode,
                                          IntGetter rootNote, IntGetter spread);

// 4OSC pages. wave: 0 off, 1 sine, 2 square, 3 saw, 4 triangle, 5 noise
std::unique_ptr<PlayfulScene> oscillator(IntGetter wave, IntGetter voices,
                                         TextGetter tuneText);
// attack/decay/sustain/release as knob positions (0-1)
std::unique_ptr<PlayfulScene> ampEnvelope(Getter attack, Getter decay,
                                          Getter sustain, Getter release);
// type: 0 off, 1 low-pass, 2 high-pass, 3 band-pass, 4 notch
std::unique_ptr<PlayfulScene> oscFilter(Getter frequencyHz, Getter resonance01,
                                        Getter envAmount01, IntGetter type);
// shows one scene per page of a paged screen, with a title for each
std::unique_ptr<PlayfulScene>
paged(std::vector<std::unique_ptr<PlayfulScene>> scenes,
      juce::StringArray titles);

// any other plugin or modifier
std::unique_ptr<PlayfulScene> generic(TextGetter subtitle);

// Shared drawing bits, also used by the custom playful screens
void strokePath(juce::Graphics &g, const juce::Path &p, float width);
void strokeDashed(juce::Graphics &g, const juce::Path &p, float width,
                  float dash, float gap);
juce::Path svg(const juce::String &d);
void text(juce::Graphics &g, const juce::String &s, float x, float baseline,
          float size, juce::Colour colour, bool bold = false,
          juce::Justification j = juce::Justification::left);
// a keyboard: notes [firstNote, firstNote + numNotes); keyColour decides the
// fill of each key (return transparent for the default)
void keyboard(juce::Graphics &g, float x, float y, float whiteWidth,
              float whiteHeight, float blackWidth, float blackHeight,
              int firstNote, int numNotes,
              const std::function<juce::Colour(int)> &keyColour);
juce::String noteName(int note); // e.g. "C4"
juce::String formatFrequency(float hz, bool shortForm = false);

} // namespace PlayfulScenes
