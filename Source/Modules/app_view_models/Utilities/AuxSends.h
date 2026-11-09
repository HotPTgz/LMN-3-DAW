#pragma once

namespace app_view_models {
namespace AuxSends {

// Two send buses: 0 = "FX A" (starts with a reverb), 1 = "FX B" (delay)
constexpr int numBuses = 2;
constexpr float levelStep = 0.02f;

juce::String getBusName(int bus);

// Returns the send for this bus on the track, or nullptr
tracktion::AuxSendPlugin *findSend(tracktion::Track &track, int bus);

// True if the track is one of the FX return tracks
bool isReturnTrack(tracktion::Track &track);

tracktion::AudioTrack *findReturnTrack(tracktion::Edit &edit, int bus);

// Creates "FX A"/"FX B" at the end of the track list if it doesn't exist,
// with an aux return and a 100% wet reverb (A) or delay (B) on it
tracktion::AudioTrack *getOrCreateReturnTrack(tracktion::Edit &edit, int bus);

// Send level as a 0-1 fader position (0 if there is no send)
float getSendLevel(tracktion::Track &track, int bus);

// Changes the send level, creating the send (post-fader) and the return
// track the first time. Returns false for tracks that can't have sends
// (the master track and the FX return tracks themselves).
bool nudgeSendLevel(tracktion::Track &track, int bus, float delta);

} // namespace AuxSends
} // namespace app_view_models
