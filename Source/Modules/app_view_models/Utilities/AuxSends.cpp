#include "AuxSends.h"

namespace app_view_models {
namespace AuxSends {

juce::String getBusName(int bus) { return bus == 0 ? "FX A" : "FX B"; }

tracktion::AuxSendPlugin *findSend(tracktion::Track &track, int bus) {
    for (auto send : track.pluginList.getPluginsOfType<tracktion::AuxSendPlugin>())
        if (send->getBusNumber() == bus)
            return send;
    return nullptr;
}

bool isReturnTrack(tracktion::Track &track) {
    return !track.pluginList.getPluginsOfType<tracktion::AuxReturnPlugin>()
                .isEmpty();
}

tracktion::AudioTrack *findReturnTrack(tracktion::Edit &edit, int bus) {
    for (auto track : tracktion::getAudioTracks(edit))
        for (auto ret :
             track->pluginList.getPluginsOfType<tracktion::AuxReturnPlugin>())
            if (ret->busNumber.get() == bus)
                return track;
    return nullptr;
}

static tracktion::Plugin::Ptr createPlugin(tracktion::Edit &edit,
                                           const juce::String &type) {
    return edit.getPluginCache().createNewPlugin(type, {});
}

tracktion::AudioTrack *getOrCreateReturnTrack(tracktion::Edit &edit, int bus) {
    if (auto existing = findReturnTrack(edit, bus))
        return existing;

    edit.ensureNumberOfAudioTracks(tracktion::getAudioTracks(edit).size() + 1);
    auto track = tracktion::getAudioTracks(edit).getLast();
    if (track == nullptr)
        return nullptr;

    track->setName(getBusName(bus));
    track->setColour(bus == 0 ? juce::Colour(0xff458588)
                              : juce::Colour(0xffb16286));
    edit.setAuxBusName(bus, getBusName(bus));

    if (auto ret = createPlugin(edit, tracktion::AuxReturnPlugin::xmlTypeName)) {
        if (auto auxReturn = dynamic_cast<tracktion::AuxReturnPlugin *>(ret.get()))
            auxReturn->busNumber = bus;
        track->pluginList.insertPlugin(ret, 0, nullptr);
    }

    // Start with an effect so a send is audible straight away. The return
    // track should only carry the effected signal, so make it fully wet.
    if (bus == 0) {
        if (auto p = createPlugin(edit, tracktion::ReverbPlugin::xmlTypeName)) {
            if (auto reverb = dynamic_cast<tracktion::ReverbPlugin *>(p.get())) {
                reverb->setWet(1.0f);
                reverb->setDry(0.0f);
            }
            track->pluginList.insertPlugin(p, 1, nullptr);
        }
    } else {
        if (auto p = createPlugin(edit, tracktion::DelayPlugin::xmlTypeName)) {
            if (auto delay = dynamic_cast<tracktion::DelayPlugin *>(p.get())) {
                delay->mixValue = 1.0f;
                delay->lengthMs = 375;
            }
            track->pluginList.insertPlugin(p, 1, nullptr);
        }
    }

    return track;
}

float getSendLevel(tracktion::Track &track, int bus) {
    if (auto send = findSend(track, bus))
        return send->gain->getCurrentValue();
    return 0.0f;
}

static tracktion::AuxSendPlugin *createSend(tracktion::Track &track, int bus) {
    auto plugin = createPlugin(track.edit, tracktion::AuxSendPlugin::xmlTypeName);
    auto send = dynamic_cast<tracktion::AuxSendPlugin *>(plugin.get());
    if (send == nullptr)
        return nullptr;

    send->busNumber = bus;
    // start silent; the user turns it up
    send->gain->setParameter(0.0f, juce::sendNotification);

    // Post-fader: place the send right after the track's volume plugin
    int index = track.pluginList.size();
    if (auto volume = EngineHelpers::getVolumeAndPanPluginForTrack(&track))
        index = track.pluginList.indexOf(volume) + 1;

    track.pluginList.insertPlugin(plugin, index, nullptr);
    return send;
}

bool nudgeSendLevel(tracktion::Track &track, int bus, float delta) {
    if (track.isMasterTrack() || isReturnTrack(track) ||
        !juce::isPositiveAndBelow(bus, numBuses))
        return false;

    auto send = findSend(track, bus);
    if (send == nullptr) {
        if (delta <= 0.0f)
            return true; // nothing to turn down
        getOrCreateReturnTrack(track.edit, bus);
        send = createSend(track, bus);
        if (send == nullptr)
            return false;
    }

    float level =
        juce::jlimit(0.0f, 1.0f, send->gain->getCurrentValue() + delta);
    send->gain->setParameter(level, juce::sendNotification);
    return true;
}

} // namespace AuxSends
} // namespace app_view_models
