#include "MoreSequencerViewModels.h"

namespace app_view_models {

namespace {
const juce::Identifier euclidState("EUCLID_SEQUENCER"), euclidRingsId("rings"),
    euclidVelocityId("velocity"), euclidSwingId("swing");
const juce::Identifier turingState("TURING_SEQUENCER"), turingLockId("lock"),
    turingLengthId("length"), turingRangeId("range"), turingScaleId("scale"),
    turingRootId("root"), turingRateId("rate"), turingGateId("gate"),
    turingSeedId("seed");
const juce::Identifier chordsState("CHORDS_SEQUENCER"), chordsKeyId("key"),
    chordsDegreesId("degrees"), chordsFollowId("follow"),
    chordsPresetId("preset");
const double turingRateBeats[] = {0.5, 0.25, 0.125};
} // namespace

//==============================================================================
EuclideanSequencerViewModel::EuclideanSequencerViewModel(
    tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(euclidState,
                                                            nullptr)),
      clip(t), rowNames(Sequencers::kitRowNames(*t)) {
    auto saved = state[euclidRingsId].toString();
    if (saved.isNotEmpty()) {
        auto rings = juce::StringArray::fromTokens(saved, ",", "");
        for (int row = 0; row < juce::jmin(rings.size(), Sequencers::drumRows);
             row++) {
            auto parts = juce::StringArray::fromTokens(rings[row], ":", "");
            auto &r = input.rings[size_t(row)];
            r.hits = juce::jlimit(0, 32, parts[0].getIntValue());
            r.steps = juce::jlimit(1, 32, parts[1].getIntValue());
            r.rotate = parts[2].getIntValue();
        }
    } else {
        // a groove to start: kick, snare and hat, wherever the kit has them
        auto find = [this](const juce::String &word, int fallback) {
            for (int row = 0; row < Sequencers::drumRows; row++)
                if (rowNames[row].containsIgnoreCase(word))
                    return row;
            return fallback;
        };
        input.rings[size_t(find("KICK", 0))] = {4, 16, 0};
        input.rings[size_t(find("SNARE", 1))] = {2, 16, 4};
        input.rings[size_t(find("HAT", 4))] = {7, 16, 1};
    }
    input.velocity = juce::jlimit(1, 127, int(state.getProperty(euclidVelocityId, 100)));
    input.swing = double(state.getProperty(euclidSwingId, 0.0));
    for (int row = 0; row < Sequencers::drumRows; row++)
        if (input.rings[size_t(row)].hits > 0) {
            selected = row;
            break;
        }
    changed();
}

void EuclideanSequencerViewModel::changed() {
    juce::StringArray rings;
    for (auto &r : input.rings)
        rings.add(juce::String(r.hits) + ":" + juce::String(r.steps) + ":" +
                  juce::String(r.rotate));
    state.setProperty(euclidRingsId, rings.joinIntoString(","), nullptr);
    state.setProperty(euclidVelocityId, input.velocity, nullptr);
    state.setProperty(euclidSwingId, input.swing, nullptr);
    clip.write(Sequencers::generateEuclid(input),
               Sequencers::euclidLengthSteps(input) * 0.25);
}

void EuclideanSequencerViewModel::nudgeSound(int delta) {
    selected = juce::jlimit(0, Sequencers::drumRows - 1, selected + delta);
}

void EuclideanSequencerViewModel::nextSound() {
    selected = (selected + 1) % Sequencers::drumRows;
}

void EuclideanSequencerViewModel::selectForNote(int noteNumber) {
    int row = noteNumber - Sequencers::firstDrumNote;
    if (juce::isPositiveAndBelow(row, Sequencers::drumRows))
        selected = row;
}

void EuclideanSequencerViewModel::nudgeHits(int delta) {
    auto &r = input.rings[size_t(selected)];
    r.hits = juce::jlimit(0, r.steps, r.hits + delta);
    changed();
}

void EuclideanSequencerViewModel::nudgeSteps(int delta) {
    auto &r = input.rings[size_t(selected)];
    r.steps = juce::jlimit(1, 32, r.steps + delta);
    r.hits = juce::jmin(r.hits, r.steps);
    changed();
}

void EuclideanSequencerViewModel::nudgeRotate(int delta) {
    auto &r = input.rings[size_t(selected)];
    r.rotate = ((r.rotate + delta) % r.steps + r.steps) % r.steps;
    changed();
}

void EuclideanSequencerViewModel::nudgeVelocity(int delta) {
    input.velocity = juce::jlimit(1, 127, input.velocity + delta * 5);
    changed();
}

void EuclideanSequencerViewModel::nudgeSwing(int delta) {
    input.swing = juce::jlimit(0.0, 1.0, input.swing + delta * 0.05);
    changed();
}

int EuclideanSequencerViewModel::getPlayingStep() const {
    if (!isPlaying())
        return -1;
    return int(clip.getPositionBeats() / 0.25) % getLengthSteps();
}

//==============================================================================
const juce::StringArray &TuringSequencerViewModel::rateNames() {
    static juce::StringArray names{"1/8", "1/16", "1/32"};
    return names;
}

TuringSequencerViewModel::TuringSequencerViewModel(tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(turingState,
                                                            nullptr)),
      clip(t) {
    input.lock = double(state.getProperty(turingLockId, 0.85));
    input.length = int(state.getProperty(turingLengthId, 16));
    input.range = int(state.getProperty(turingRangeId, 2));
    // C minor to start
    int minor = internal_plugins::midifx::Scales::getNames().indexOf("Minor");
    input.scale = int(state.getProperty(turingScaleId, juce::jmax(0, minor)));
    input.root = int(state.getProperty(turingRootId, 48));
    rateIndex = juce::jlimit(0, 2, int(state.getProperty(turingRateId, 1)));
    input.gate = double(state.getProperty(turingGateId, 0.5));
    input.seed = int(state.getProperty(turingSeedId, 1));
    changed();
}

void TuringSequencerViewModel::changed() {
    state.setProperty(turingLockId, input.lock, nullptr);
    state.setProperty(turingLengthId, input.length, nullptr);
    state.setProperty(turingRangeId, input.range, nullptr);
    state.setProperty(turingScaleId, input.scale, nullptr);
    state.setProperty(turingRootId, input.root, nullptr);
    state.setProperty(turingRateId, rateIndex, nullptr);
    state.setProperty(turingGateId, input.gate, nullptr);
    state.setProperty(turingSeedId, input.seed, nullptr);
    input.stepLength = turingRateBeats[rateIndex];
    notes = Sequencers::turingNotes(input);
    clip.write(Sequencers::generateTuring(input), Sequencers::turingLength(input));
}

void TuringSequencerViewModel::nudgeLock(int delta) {
    input.lock = juce::jlimit(0.0, 1.0, input.lock + delta * 0.05);
    changed();
}
void TuringSequencerViewModel::nudgeLength(int delta) {
    input.length = juce::jlimit(2, 16, input.length + delta);
    changed();
}
void TuringSequencerViewModel::nudgeRange(int delta) {
    input.range = juce::jlimit(1, 4, input.range + delta);
    changed();
}
void TuringSequencerViewModel::nudgeScale(int delta) {
    int count = internal_plugins::midifx::Scales::getNames().size();
    input.scale = juce::jlimit(0, count - 1, input.scale + delta);
    changed();
}
void TuringSequencerViewModel::nudgeRate(int delta) {
    rateIndex = juce::jlimit(0, 2, rateIndex + delta);
    changed();
}
void TuringSequencerViewModel::nudgeGate(int delta) {
    input.gate = juce::jlimit(0.1, 1.0, input.gate + delta * 0.05);
    changed();
}
void TuringSequencerViewModel::notePressed(int noteNumber) {
    input.root = juce::jlimit(24, 84, noteNumber);
    changed();
}
void TuringSequencerViewModel::reroll() {
    input.seed++;
    changed();
}

int TuringSequencerViewModel::getPlayingStep() const {
    if (!isPlaying() || notes.empty())
        return -1;
    return int(clip.getPositionBeats() / input.stepLength) % int(notes.size());
}

//==============================================================================
ChordsSequencerViewModel::ChordsSequencerViewModel(tracktion::AudioTrack::Ptr t)
    : track(t), state(track->state.getOrCreateChildWithName(chordsState,
                                                            nullptr)),
      clip(t) {
    input.key = juce::jlimit(0, 23, int(state.getProperty(chordsKeyId, 0)));
    for (auto d : juce::StringArray::fromTokens(
             state[chordsDegreesId].toString(), ",", ""))
        if (d.trim().isNotEmpty())
            input.degrees.add(juce::jlimit(0, 6, d.getIntValue()));
    presetIndex = int(state.getProperty(chordsPresetId, 0));
    if (input.degrees.isEmpty())
        input.degrees = Sequencers::Chords::preset(presetIndex);
    changed();
    setFollowers(int(state.getProperty(chordsFollowId, 0)));
}

ChordsSequencerViewModel::~ChordsSequencerViewModel() {
    stop();
    followers.clear();
}

juce::Array<tracktion::AudioTrack *> ChordsSequencerViewModel::nextTracks() const {
    juce::Array<tracktion::AudioTrack *> next;
    auto all = tracktion::getAudioTracks(track->edit);
    int index = all.indexOf(track.get());
    for (int i = index + 1; i < all.size() && next.size() < 3; i++)
        next.add(all[i]);
    return next;
}

int ChordsSequencerViewModel::getMaxFollowers() const {
    return nextTracks().size();
}

juce::StringArray ChordsSequencerViewModel::getFollowerNames() const {
    juce::StringArray names;
    auto all = tracktion::getAudioTracks(track->edit);
    for (auto &f : followers)
        names.add("Track " + juce::String(all.indexOf(f.track.get()) + 1) +
                  juce::String::fromUTF8(" \xc2\xb7 ") +
                  Sequencers::roleName(f.role));
    return names;
}

void ChordsSequencerViewModel::setFollowers(int count) {
    auto next = nextTracks();
    count = juce::jlimit(0, next.size(), count);
    while (int(followers.size()) > count) {
        auto &f = followers.back();
        if (soloedFollowers)
            f.track->setSolo(false);
        f.clip->write({}, 4.0); // an empty clip is removed with it
        followers.pop_back();
    }
    static const Sequencers::ChordRole roles[] = {
        Sequencers::ChordRole::bassRoots, Sequencers::ChordRole::arpeggio,
        Sequencers::ChordRole::pad};
    while (int(followers.size()) < count) {
        // start the new clip where this one starts
        if (auto c = clip.getClip())
            track->edit.getTransport().setPosition(c->getPosition().getStart());
        Follower f;
        f.track = next[int(followers.size())];
        f.clip = std::make_unique<SequencerClip>(f.track);
        f.role = roles[followers.size()];
        followers.push_back(std::move(f));
    }
    state.setProperty(chordsFollowId, count, nullptr);
    changed();
}

void ChordsSequencerViewModel::changed() {
    juce::StringArray degrees;
    for (auto d : input.degrees)
        degrees.add(juce::String(d));
    state.setProperty(chordsKeyId, input.key, nullptr);
    state.setProperty(chordsDegreesId, degrees.joinIntoString(","), nullptr);
    state.setProperty(chordsPresetId, presetIndex, nullptr);
    selectedBar = juce::jlimit(0, input.degrees.size() - 1, selectedBar);
    double length = Sequencers::chordsLength(input);
    clip.write(Sequencers::generateChords(input, Sequencers::ChordRole::chords),
               length);
    for (auto &f : followers)
        f.clip->write(Sequencers::generateChords(input, f.role), length);
}

void ChordsSequencerViewModel::nudgeKey(int delta) {
    input.key = juce::jlimit(0, 23, input.key + delta);
    changed();
}
void ChordsSequencerViewModel::nudgeBar(int delta) {
    selectedBar = juce::jlimit(0, input.degrees.size() - 1, selectedBar + delta);
}
void ChordsSequencerViewModel::nudgeChord(int delta) {
    auto &d = input.degrees.getReference(selectedBar);
    d = juce::jlimit(0, 6, d + delta);
    changed();
}
void ChordsSequencerViewModel::nudgeFollow(int delta) {
    setFollowers(getFollowers() + delta);
}
void ChordsSequencerViewModel::nudgePreset(int delta) {
    presetIndex = juce::jlimit(
        0, Sequencers::Chords::getPresetNames().size() - 1, presetIndex + delta);
    input.degrees = Sequencers::Chords::preset(presetIndex);
    changed();
}
void ChordsSequencerViewModel::nudgeBars(int delta) {
    int bars = juce::jlimit(1, 8, input.degrees.size() + delta);
    while (input.degrees.size() < bars)
        input.degrees.add(input.degrees.getLast());
    while (input.degrees.size() > bars)
        input.degrees.removeLast();
    changed();
}
void ChordsSequencerViewModel::notePressed(int noteNumber) {
    input.key = (input.key >= 12 ? 12 : 0) + noteNumber % 12;
    changed();
}

void ChordsSequencerViewModel::play() {
    clip.play();
    if (clip.isPlaying())
        for (auto &f : followers)
            f.track->setSolo(true);
    soloedFollowers = clip.isPlaying();
}

void ChordsSequencerViewModel::stop() {
    clip.stop();
    if (soloedFollowers)
        for (auto &f : followers)
            f.track->setSolo(false);
    soloedFollowers = false;
}

double ChordsSequencerViewModel::getPlayingBeat() const {
    return isPlaying() ? clip.getPositionBeats() : -1.0;
}

int ChordsSequencerViewModel::getPlayingBar() const {
    return isPlaying() ? int(clip.getPositionBeats() / 4.0) : -1;
}

} // namespace app_view_models
