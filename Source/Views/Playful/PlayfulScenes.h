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
// shows one scene per page of a paged screen, with a title for each and
// optional tabs (page names) drawn at the top right
std::unique_ptr<PlayfulScene>
paged(std::vector<std::unique_ptr<PlayfulScene>> scenes,
      juce::StringArray titles, juce::StringArray tabs = {});

// Built-in engines. Knob positions are 0-1 unless noted.
// String: decay in seconds
std::unique_ptr<PlayfulScene> stringEngine(Getter decaySeconds,
                                           Getter brightness, Getter pickPos,
                                           Getter body);
// FM: algorithm 0-7, ratios as multiples of the note
std::unique_ptr<PlayfulScene> fmEngine(IntGetter algorithm, Getter ratio2,
                                       Getter amount, Getter feedback,
                                       Getter ratio3, Getter ratio4);
// Pulse: PWM rate in Hz
std::unique_ptr<PlayfulScene> pulseEngine(Getter width, Getter pwmDepth,
                                          Getter pwmRateHz, Getter sub);
// The Shape page shared by the engines (knob positions)
std::unique_ptr<PlayfulScene> engineShape(Getter attack, Getter decay,
                                          Getter sustain, Getter release,
                                          Getter cutoff, IntGetter voices);

// Newer MIDI FX (PlayfulScenesMore.cpp)
std::unique_ptr<PlayfulScene> noteRepeat(Getter rateBeats, TextGetter rateText,
                                         Getter ramp, Getter gate,
                                         Getter swing);
// offset(i, n, gap, curve): when note i of n is played
std::unique_ptr<PlayfulScene>
strum(Getter seconds, IntGetter direction, std::function<bool()> alternate,
      IntGetter curve, std::function<double(int, int, double, int)> offset);
std::unique_ptr<PlayfulScene> humanise(Getter timingSeconds, Getter velocity,
                                       Getter length, Getter amount);
std::unique_ptr<PlayfulScene> midiEcho(TextGetter timeText, Getter timeBeats,
                                       IntGetter repeats, Getter decay,
                                       IntGetter transpose);

// Newer effects (PlayfulScenesMore.cpp)
std::unique_ptr<PlayfulScene> tape(Getter wow, Getter flutter, Getter saturate,
                                   Getter hiss, Getter wobble);
std::unique_ptr<PlayfulScene> lofi(Getter bits, Getter rateHz, Getter noise,
                                   Getter mix);
// shape(x, type): the clipping curve
std::unique_ptr<PlayfulScene> drive(Getter driveDb, Getter tone, IntGetter type,
                                    TextGetter typeName, Getter mix,
                                    std::function<float(float, int)> shape);
std::unique_ptr<PlayfulScene>
resonantFilter(Getter liveCutoff, Getter resonance, Getter envelope, Getter lfo,
               IntGetter type, juce::StringArray typeNames,
               TextGetter lfoRateText);
// gainAt(seconds since a hit)
std::unique_ptr<PlayfulScene> ducker(TextGetter sourceName,
                                     std::function<bool()> followsBeat,
                                     std::function<float(float)> gainAt,
                                     Getter liveGain);
// repeats: count, or -1 while held. liveRepeat: index, -1 off, -2 waiting
std::unique_ptr<PlayfulScene> beatRepeat(TextGetter lengthText,
                                         IntGetter repeats, Getter decay,
                                         Getter pitch, IntGetter liveRepeat,
                                         std::function<void(bool)> setActive);

// Newer instruments (PlayfulScenesMore.cpp)
// frame(table, index): a frame's samples; warpPhase(phase, warp)
std::unique_ptr<PlayfulScene>
wavetable(IntGetter table, Getter position, Getter warp, Getter filterHz,
          juce::StringArray names,
          std::function<const std::vector<float> &(int, int)> frame,
          std::function<float(double, float)> warpPhase);
std::unique_ptr<PlayfulScene> mallets(IntGetter material, Getter hardness,
                                      Getter decay, Getter tone,
                                      juce::StringArray names);
// overview: peak levels across the sample; playhead 0-1 (or -1)
std::unique_ptr<PlayfulScene>
granular(TextGetter sampleName, std::function<std::vector<float>()> overview,
         Getter durationSeconds, Getter position, Getter size, Getter density,
         Getter spray, Getter playhead, std::function<bool()> freeze);
// preview(pad): the start of the pad's sound; describe(pad): e.g. "48 Hz"
std::unique_ptr<PlayfulScene>
drumSynth(IntGetter selectedPad, std::function<void(int)> selectPad,
          std::function<double(int)> lastHitMs, juce::StringArray padNames,
          std::function<std::array<float, 6>(int)> padParams,
          std::function<std::vector<float>(int)> preview,
          std::function<String(int)> describe);

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
