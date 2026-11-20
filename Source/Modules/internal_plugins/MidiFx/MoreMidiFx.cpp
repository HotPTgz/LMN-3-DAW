#include "MoreMidiFx.h"

namespace internal_plugins {
namespace midifx {

static void addAt(tracktion::MidiMessageArray &midi,
                  const juce::MidiMessage &m, double time) {
    midi.addMidiMessage(m, juce::jmax(0.0, time),
                        tracktion::MidiMessageArray::notMPE);
}

// Takes the note messages out of a buffer (in time order), keeping the rest
static std::vector<juce::MidiMessage>
takeNotes(tracktion::MidiMessageArray &midi, bool &allNotesOff) {
    std::vector<juce::MidiMessage> notes;
    tracktion::MidiMessageArray rest;
    allNotesOff = midi.isAllNotesOff;
    midi.sortByTimestamp();
    for (auto &m : midi) {
        if (m.isNoteOn() || m.isNoteOff())
            notes.push_back(m);
        else {
            if (m.isAllNotesOff() || m.isAllSoundOff())
                allNotesOff = true;
            rest.add(m);
        }
    }
    midi.clear();
    midi.isAllNotesOff = allNotesOff;
    midi.mergeFrom(rest);
    return notes;
}

//==============================================================================
void MidiScheduler::add(double time, const juce::MidiMessage &message) {
    pending.push_back({time, message});
}

void MidiScheduler::flush(tracktion::MidiMessageArray &midi,
                          double blockStart, double blockSeconds) {
    double end = blockStart + blockSeconds;
    std::stable_sort(pending.begin(), pending.end(),
                     [](const Item &a, const Item &b) {
                         return a.time < b.time;
                     });
    size_t i = 0;
    for (; i < pending.size() && pending[i].time < end; i++)
        addAt(midi, pending[i].message, pending[i].time - blockStart);
    pending.erase(pending.begin(), pending.begin() + long(i));
}

double MidiScheduler::lastNoteOnTime(int note) const {
    double latest = -1.0;
    for (auto &item : pending)
        if (item.message.isNoteOn() && item.message.getNoteNumber() == note)
            latest = juce::jmax(latest, item.time);
    return latest;
}

//==============================================================================
namespace NoteRepeat {
const juce::StringArray &getRateNames() {
    static juce::StringArray names{"1/4",  "1/8",   "1/8T",
                                   "1/16", "1/16T", "1/32"};
    return names;
}
double getRateBeats(int index) {
    static const double beats[] = {1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125};
    return beats[juce::jlimit(0, 5, index)];
}
} // namespace NoteRepeat

void NoteRepeatEngine::noteOffNow(tracktion::MidiMessageArray &midi, int note,
                                  double time) {
    for (size_t i = sounding.size(); i-- > 0;)
        if (sounding[i].note == note) {
            addAt(midi, juce::MidiMessage::noteOff(channel, note), time);
            sounding.erase(sounding.begin() + long(i));
        }
}

void NoteRepeatEngine::process(tracktion::MidiMessageArray &midi,
                               double blockStartBeat, double beatsPerSecond,
                               double blockSeconds,
                               const Settings &settings) {
    bool allOff = false;
    auto notes = takeNotes(midi, allOff);
    if (allOff) {
        held.clear();
        for (auto &s : sounding)
            addAt(midi, juce::MidiMessage::noteOff(channel, s.note), 0.0);
        sounding.clear();
        return;
    }
    if (beatsPerSecond <= 0.0 || blockSeconds <= 0.0)
        return;

    const double rate = juce::jmax(1.0e-3, settings.rateBeats);
    // the song jumped backwards (loop, restart): start the grid again
    if (blockStartBeat < lastStepBeat - rate * 2.0) {
        lastStepBeat = -1.0e9;
        for (auto &h : held)
            h.pressBeat = blockStartBeat - rate;
    }
    const double gate = juce::jlimit(0.05, 1.0, settings.gate);
    const double blockEndBeat = blockStartBeat + blockSeconds * beatsPerSecond;
    auto toTime = [&](double beat) {
        return (beat - blockStartBeat) / beatsPerSecond;
    };
    auto toBeat = [&](double time) {
        return blockStartBeat + time * beatsPerSecond;
    };

    // The grid steps in this block (every second one late by the swing).
    // Start one step early: a swung step can land in the next block.
    std::vector<double> steps;
    double first = std::ceil(blockStartBeat / rate - 1.0e-9) * rate - rate;
    for (double b = first; b < blockEndBeat - 1.0e-9; b += rate) {
        long k = std::lround(b / rate);
        double swung = b + ((k % 2) != 0 ? settings.swing * rate * 0.5 : 0.0);
        if (swung >= blockStartBeat - 1.0e-9 && swung < blockEndBeat - 1.0e-9 &&
            swung > lastStepBeat + 1.0e-9) {
            steps.push_back(swung);
            lastStepBeat = swung;
        }
    }

    size_t ni = 0, si = 0;
    while (ni < notes.size() || si < steps.size()) {
        double noteBeat = ni < notes.size()
                              ? toBeat(notes[ni].getTimeStamp())
                              : 1.0e300;
        double stepBeat = si < steps.size() ? steps[si] : 1.0e300;

        // note offs due before the next event
        double next = juce::jmin(noteBeat, stepBeat);
        for (size_t i = sounding.size(); i-- > 0;)
            if (sounding[i].offBeat <= next) {
                addAt(midi,
                      juce::MidiMessage::noteOff(channel, sounding[i].note),
                      toTime(sounding[i].offBeat));
                sounding.erase(sounding.begin() + long(i));
            }

        if (noteBeat <= stepBeat) {
            auto &m = notes[ni++];
            int note = m.getNoteNumber();
            double time = m.getTimeStamp();
            if (m.isNoteOn()) {
                channel = m.getChannel();
                noteOffNow(midi, note, time);
                held.push_back({note, m.getVelocity(), noteBeat, 0});
                addAt(midi, m, time);
                sounding.push_back({note, noteBeat + rate * gate});
            } else {
                for (size_t i = held.size(); i-- > 0;)
                    if (held[i].note == note)
                        held.erase(held.begin() + long(i));
                noteOffNow(midi, note, time);
            }
        } else {
            si++;
            for (auto &h : held) {
                // the key was only just pressed: that was this step's hit
                if (stepBeat - h.pressBeat < rate * 0.5)
                    continue;
                noteOffNow(midi, h.note, toTime(stepBeat));
                h.count++;
                double v = h.velocity *
                           (1.0 + juce::jlimit(-1.0, 1.0, settings.ramp) *
                                      0.15 * h.count);
                int velocity = juce::jlimit(1, 127, juce::roundToInt(v));
                addAt(midi,
                      juce::MidiMessage::noteOn(channel, h.note,
                                                juce::uint8(velocity)),
                      toTime(stepBeat));
                sounding.push_back({h.note, stepBeat + rate * gate});
            }
        }
    }

    // note offs later in this block
    for (size_t i = sounding.size(); i-- > 0;)
        if (sounding[i].offBeat < blockEndBeat) {
            addAt(midi, juce::MidiMessage::noteOff(channel, sounding[i].note),
                  toTime(sounding[i].offBeat));
            sounding.erase(sounding.begin() + long(i));
        }
    midi.sortByTimestamp();
}

//==============================================================================
namespace Strum {
const juce::StringArray &getDirectionNames() {
    static juce::StringArray names{"Up", "Down"};
    return names;
}
const juce::StringArray &getCurveNames() {
    static juce::StringArray names{"Even", "Speed up", "Slow down"};
    return names;
}
double offset(int i, int n, double seconds, int curve) {
    if (n <= 1 || i <= 0)
        return 0.0;
    double span = seconds * (n - 1);
    double x = double(i) / double(n - 1);
    if (curve == 1)
        x = std::pow(x, 0.6); // gaps get shorter
    else if (curve == 2)
        x = std::pow(x, 1.6); // gaps get longer
    return span * x;
}
} // namespace Strum

void StrumEngine::process(tracktion::MidiMessageArray &midi,
                          double blockSeconds, const Settings &settings) {
    bool allOff = false;
    auto notes = takeNotes(midi, allOff);
    if (allOff) {
        scheduler.clear();
        now += blockSeconds;
        return;
    }

    // note ons less than 5 ms apart are one chord
    for (size_t i = 0; i < notes.size();) {
        auto &m = notes[i];
        double time = now + m.getTimeStamp();
        if (m.isNoteOff()) {
            // never before its (strummed) note on
            double on = scheduler.lastNoteOnTime(m.getNoteNumber());
            scheduler.add(juce::jmax(time, on + 0.002), m);
            i++;
            continue;
        }
        std::vector<juce::MidiMessage> chord;
        size_t j = i;
        while (j < notes.size() && notes[j].isNoteOn() &&
               notes[j].getTimeStamp() - m.getTimeStamp() < 0.005)
            chord.push_back(notes[j++]);
        bool down = settings.direction == 1;
        if (settings.alternate) {
            down = nextDown;
            nextDown = !nextDown;
        }
        std::sort(chord.begin(), chord.end(),
                  [down](const juce::MidiMessage &a,
                         const juce::MidiMessage &b) {
                      return down ? a.getNoteNumber() > b.getNoteNumber()
                                  : a.getNoteNumber() < b.getNoteNumber();
                  });
        int n = int(chord.size());
        for (int k = 0; k < n; k++)
            scheduler.add(time + Strum::offset(k, n, settings.seconds,
                                               settings.curve),
                          chord[size_t(k)]);
        i = j;
    }

    scheduler.flush(midi, now, blockSeconds);
    now += blockSeconds;
    midi.sortByTimestamp();
}

//==============================================================================
void HumaniseEngine::process(tracktion::MidiMessageArray &midi,
                             double blockSeconds, const Settings &settings) {
    bool allOff = false;
    auto notes = takeNotes(midi, allOff);
    if (allOff) {
        scheduler.clear();
        now += blockSeconds;
        return;
    }
    const double amount = juce::jlimit(0.0, 1.0, settings.amount);
    for (auto &m : notes) {
        int note = m.getNoteNumber();
        double time = now + m.getTimeStamp();
        if (m.isNoteOn()) {
            double delay = random.nextDouble() * settings.timingSeconds * amount;
            int change = juce::roundToInt((random.nextDouble() * 2.0 - 1.0) *
                                          settings.velocity * amount);
            auto on = m;
            on.setVelocity(
                juce::jlimit(1, 127, m.getVelocity() + change) / 127.0f);
            pressTime[size_t(note)] = time;
            delayOf[size_t(note)] = delay;
            scheduler.add(time + delay, on);
        } else {
            double held = juce::jmax(0.0, time - pressTime[size_t(note)]);
            double longer =
                random.nextDouble() * settings.length * amount * held;
            scheduler.add(time + delayOf[size_t(note)] + longer + 1.0e-4, m);
        }
    }
    scheduler.flush(midi, now, blockSeconds);
    now += blockSeconds;
    midi.sortByTimestamp();
}

//==============================================================================
namespace Echo {
const juce::StringArray &getTimeNames() {
    static juce::StringArray names{"1/16", "1/8T", "1/8", "1/8.",
                                   "1/4",  "1/4.", "1/2"};
    return names;
}
double getTimeBeats(int index) {
    static const double beats[] = {0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0};
    return beats[juce::jlimit(0, 6, index)];
}
} // namespace Echo

void EchoEngine::process(tracktion::MidiMessageArray &midi,
                         double beatsPerSecond, double blockSeconds,
                         const Settings &settings) {
    bool allOff = false;
    auto notes = takeNotes(midi, allOff);
    if (allOff) {
        scheduler.clear();
        now += blockSeconds;
        return;
    }
    const double step =
        beatsPerSecond > 0 ? settings.timeBeats / beatsPerSecond : 0.25;
    for (auto &m : notes) {
        double time = now + m.getTimeStamp();
        scheduler.add(time, m);
        if (!m.isNoteOn())
            continue;
        double velocity = m.getVelocity();
        for (int k = 1; k <= juce::jlimit(1, 8, settings.repeats); k++) {
            velocity *= 1.0 - juce::jlimit(0.0, 0.95, settings.decay);
            int v = juce::roundToInt(velocity);
            int note = m.getNoteNumber() + k * settings.transpose;
            if (v < 1 || note < 0 || note > 127)
                break;
            double on = time + k * step;
            scheduler.add(on, juce::MidiMessage::noteOn(m.getChannel(), note,
                                                        juce::uint8(v)));
            scheduler.add(on + juce::jmax(0.03, step * 0.5),
                          juce::MidiMessage::noteOff(m.getChannel(), note));
        }
    }
    scheduler.flush(midi, now, blockSeconds);
    now += blockSeconds;
    midi.sortByTimestamp();
}


//==============================================================================
namespace Finger {
const juce::StringArray &getRoleNames() {
    static juce::StringArray names{"kick", "snare", "clap",     "rim",
                                   "hat",  "open hat", "tom", "perc"};
    return names;
}
const juce::StringArray &getPatternNames() {
    static juce::StringArray names{"Four floor", "Boom bap",  "Half time",
                                   "Shuffle",    "Breakbeat", "Dembow",
                                   "Two step",   "Trap",      "Disco",
                                   "Bossa",      "Fill A",    "Fill B"};
    return names;
}
const juce::StringArray &getLengthNames() {
    static juce::StringArray names{"1/2 bar", "1 bar", "2 bars"};
    return names;
}
int lengthSteps(int index) {
    static const int steps[] = {8, 16, 32};
    return steps[juce::jlimit(0, 2, index)];
}

// 16 steps per role: x accent, o normal, - rest.
// Roles: kick, snare, clap, rim, hat, open hat, tom, perc
static const char *const patterns[12][numRoles] = {
    // Four floor
    {"x---x---x---x---", "----------------", "----x-------x---", "----------------",
     "--x---x---x---x-", "----------------", "----------------", "----------------"},
    // Boom bap
    {"x------o--x-----", "----x-------x---", "----------------", "----------------",
     "x-o-x-o-x-o-x-o-", "----------------", "----------------", "----------------"},
    // Half time
    {"x-----o---------", "--------x-------", "----------------", "------------o---",
     "x-o-x-o-x-o-x-o-", "----------------", "----------------", "----------------"},
    // Shuffle
    {"x-----o-x-------", "----x-------x---", "----------------", "----------------",
     "x--ox--ox--ox--o", "----------------", "----------------", "----------------"},
    // Breakbeat
    {"x-x-------x-----", "----x--o-o--x--o", "----------------", "----------------",
     "x-x-x-x-x-x-x-x-", "----------------", "----------------", "----------------"},
    // Dembow
    {"x---x---x---x---", "---x--x----x--x-", "----------------", "----------------",
     "o-o-o-o-o-o-o-o-", "----------------", "----------------", "----------------"},
    // Two step
    {"x------x--o-----", "----x-------x---", "----------------", "-o-------o------",
     "--x---xo--x---x-", "----------------", "----------------", "----------------"},
    // Trap
    {"x------x--x-----", "--------x-------", "----------------", "----------------",
     "x-o-x-oox-o-xooo", "----------------", "----------------", "----------------"},
    // Disco
    {"x---x---x---x---", "----x-------x---", "----------------", "----------------",
     "x---x---x---x---", "--x---x---x---x-", "----------------", "----------------"},
    // Bossa
    {"x--o--x-x--o--x-", "----------------", "----------------", "x--x--x---x--x--",
     "x-o-x-o-x-o-x-o-", "----------------", "----------------", "----------------"},
    // Fill A
    {"x-------x-------", "----x-o-xoxox-xx", "------------x---", "----------------",
     "x-x-x-x---------", "----------------", "----------o-x-x-", "----------------"},
    // Fill B
    {"x---------------", "xoxo----xoxo----", "----------------", "----------------",
     "----------------", "----------------", "----xoxo----xoxo", "x---------------"},
};

static int baseHit(int pattern, int role, int step) {
    char c = patterns[juce::jlimit(0, 11, pattern)][role][step & 15];
    return c == 'x' ? 115 : c == 'o' ? 80 : 0;
}

int hit(int pattern, int variation, int role, int step) {
    if (role < 0 || role >= numRoles)
        return 0;
    step &= 15;
    int v = baseHit(pattern, role, step);
    switch (variation) {
    case 1: // busier: 16th hats and ghost snares
        if (role == 4 && v == 0 && (step % 2) == 1)
            return 55;
        if (role == 1 && v == 0 && (step == 7 || step == 15))
            return 45;
        return v;
    case 2: // open hats on the off-beats, an extra kick
        if (role == 5 && (step % 4) == 2)
            return 90;
        if (role == 4 && (step % 4) == 2)
            return 0;
        if (role == 0 && v == 0 && step == 14)
            return 85;
        return v;
    case 3: // a fill over the last beat
        if (step >= 12) {
            if (role == 1)
                return 70 + 12 * (step - 12);
            if (role == 6 && (step % 2) == 1)
                return 90;
            if (role == 4 || role == 5)
                return 0;
        }
        return v;
    default:
        return v;
    }
}
} // namespace Finger

void FingerEngine::process(tracktion::MidiMessageArray &midi,
                           double blockStartBeat, double beatsPerSecond,
                           double blockSeconds, const Settings &settings) {
    bool allOff = false;
    auto notes = takeNotes(midi, allOff);
    if (allOff) {
        heldMask = 0;
        for (auto &o : offs)
            addAt(midi, juce::MidiMessage::noteOff(channel, o.note), 0.0);
        offs.clear();
        return;
    }
    if (beatsPerSecond <= 0.0 || blockSeconds <= 0.0)
        return;
    const double step = 0.25;
    const double blockEnd = blockStartBeat + blockSeconds * beatsPerSecond;
    auto toTime = [&](double beat) {
        return (beat - blockStartBeat) / beatsPerSecond;
    };
    if (blockStartBeat < lastStepBeat - 1.0) // the song jumped back
        lastStepBeat = -1.0e9;

    // the keys: pattern keys are taken, the rest pass through
    for (auto &m : notes) {
        int slot = m.getNoteNumber() - settings.baseKey;
        if (!juce::isPositiveAndBelow(slot, Finger::numSlots)) {
            addAt(midi, m, m.getTimeStamp());
            continue;
        }
        channel = m.getChannel();
        if (m.isNoteOn()) {
            heldMask |= 1u << slot;
            lastSlot = slot;
        } else
            heldMask &= ~(1u << slot);
    }

    // note offs due in this block
    for (size_t i = offs.size(); i-- > 0;)
        if (offs[i].beat < blockEnd) {
            addAt(midi, juce::MidiMessage::noteOff(channel, offs[i].note),
                  toTime(offs[i].beat));
            offs.erase(offs.begin() + long(i));
        }

    // the 16th steps starting in this block
    double first = std::ceil(blockStartBeat / step - 1.0e-9) * step;
    for (double b = first; b < blockEnd - 1.0e-9; b += step) {
        if (b <= lastStepBeat + 1.0e-9)
            continue;
        lastStepBeat = b;
        if (heldMask == 0)
            continue;
        long globalStep = std::lround(b / step);
        std::array<int, Finger::numRoles> velocity{};
        for (int slot = 0; slot < Finger::numSlots; slot++) {
            if ((heldMask & (1u << slot)) == 0)
                continue;
            auto &s = settings.slots[size_t(slot)];
            int length = Finger::lengthSteps(s.length);
            int at = int(globalStep % length);
            int variation = s.variation;
            if (length == 32 && at >= 16) // the second bar ends in a fill
                variation = 3;
            for (int r = 0; r < Finger::numRoles; r++)
                velocity[size_t(r)] = juce::jmax(
                    velocity[size_t(r)], Finger::hit(s.pattern, variation, r, at));
        }
        for (int r = 0; r < Finger::numRoles; r++) {
            if (velocity[size_t(r)] <= 0)
                continue;
            int note = settings.notes[size_t(r)];
            addAt(midi,
                  juce::MidiMessage::noteOn(channel, note,
                                            juce::uint8(velocity[size_t(r)])),
                  toTime(b));
            if (b + 0.1 < blockEnd)
                addAt(midi, juce::MidiMessage::noteOff(channel, note),
                      toTime(b + 0.1));
            else
                offs.push_back({note, b + 0.1});
        }
    }
    midi.sortByTimestamp();
}

} // namespace midifx

//==============================================================================
static juce::String moreFxPercent(float v) {
    return juce::String(juce::roundToInt(v * 100)) + "%";
}
static juce::String moreFxSignedPercent(float v) {
    int p = juce::roundToInt(v * 100);
    return (p > 0 ? "+" : "") + juce::String(p) + "%";
}

// beat position for tempo-synced effects: follows the song while playing,
// keeps counting on its own while stopped
static double moreFxBlockBeat(const tracktion::PluginRenderContext &fc,
                        tracktion::tempo::Sequence::Position &position,
                        double &freeRunning, double sampleRate,
                        double &beatsPerSecond) {
    position.set(fc.editTime);
    beatsPerSecond = position.getTempo() / 60.0;
    double start = fc.isPlaying ? position.getBeats().inBeats() : freeRunning;
    freeRunning = start + fc.bufferNumSamples / sampleRate * beatsPerSecond;
    return start;
}

const char *NoteRepeatPlugin::xmlTypeName = "lmn3NoteRepeat";

NoteRepeatPlugin::NoteRepeatPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info), tempoPosition(createPosition(edit.tempoSequence)) {
    rate = addChoiceParam("rate", TRANS("Rate"),
                          midifx::NoteRepeat::getRateNames(), rateValue, 3.0f);
    ramp = addNumberParam("ramp", TRANS("Ramp"), {-1.0f, 1.0f}, 0.05f,
                          rampValue, 0.0f, moreFxSignedPercent);
    gate = addNumberParam("gate", TRANS("Gate"), {0.1f, 1.0f}, 0.05f, gateValue,
                          0.5f, moreFxPercent);
    swing = addNumberParam("swing", TRANS("Swing"), {0.0f, 0.75f}, 0.05f,
                           swingValue, 0.0f, moreFxPercent);
}

NoteRepeatPlugin::~NoteRepeatPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void NoteRepeatPlugin::initialise(const tracktion::PluginInitialisationInfo &) {
    freeRunningBeat = 0.0;
}

void NoteRepeatPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;
    double bps = 2.0;
    double start = moreFxBlockBeat(fc, tempoPosition, freeRunningBeat, sampleRate, bps);
    midifx::NoteRepeatEngine::Settings s;
    s.rateBeats = midifx::NoteRepeat::getRateBeats(choice(rate.get()));
    s.ramp = ramp->getCurrentValue();
    s.gate = gate->getCurrentValue();
    s.swing = swing->getCurrentValue();
    engine.process(*fc.bufferForMidiMessages, start, bps,
                   fc.bufferNumSamples / sampleRate, s);
    heldNotes = engine.getNumHeldNotes();
}

//==============================================================================
const char *StrumPlugin::xmlTypeName = "lmn3Strum";

StrumPlugin::StrumPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info) {
    time = addNumberParam(
        "time", TRANS("Time"), {0.0f, 0.2f}, 0.005f, timeValue, 0.03f,
        [](float v) { return juce::String(juce::roundToInt(v * 1000)) + " ms"; });
    direction = addChoiceParam("direction", TRANS("Direction"),
                               midifx::Strum::getDirectionNames(),
                               directionValue, 0.0f);
    alternate = addChoiceParam("alternate", TRANS("Alternate"), {"Off", "On"},
                               alternateValue, 0.0f);
    curve = addChoiceParam("curve", TRANS("Curve"),
                           midifx::Strum::getCurveNames(), curveValue, 0.0f);
}

StrumPlugin::~StrumPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void StrumPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;
    midifx::StrumEngine::Settings s;
    s.seconds = time->getCurrentValue();
    s.direction = choice(direction.get());
    s.alternate = choice(alternate.get()) == 1;
    s.curve = choice(curve.get());
    engine.process(*fc.bufferForMidiMessages, fc.bufferNumSamples / sampleRate,
                   s);
}

//==============================================================================
const char *HumanisePlugin::xmlTypeName = "lmn3Humanise";

HumanisePlugin::HumanisePlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info) {
    auto plusMinus = juce::String::fromUTF8("\xc2\xb1");
    timing = addNumberParam("timing", TRANS("Timing"), {0.0f, 0.03f}, 0.001f,
                            timingValue, 0.01f, [plusMinus](float v) {
                                return plusMinus +
                                       juce::String(juce::roundToInt(v * 1000)) +
                                       " ms";
                            });
    velocity = addNumberParam(
        "velocity", TRANS("Velocity"), {0.0f, 40.0f}, 1.0f, velocityValue,
        12.0f, [plusMinus](float v) {
            return plusMinus + juce::String(juce::roundToInt(v));
        });
    length = addNumberParam("length", TRANS("Length"), {0.0f, 0.3f}, 0.01f,
                            lengthValue, 0.05f, [plusMinus](float v) {
                                return plusMinus +
                                       juce::String(juce::roundToInt(v * 100)) +
                                       "%";
                            });
    amount = addNumberParam("amount", TRANS("Amount"), {0.0f, 1.0f}, 0.05f,
                            amountValue, 0.5f, moreFxPercent);
}

HumanisePlugin::~HumanisePlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void HumanisePlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;
    midifx::HumaniseEngine::Settings s;
    s.timingSeconds = timing->getCurrentValue();
    s.velocity = juce::roundToInt(velocity->getCurrentValue());
    s.length = length->getCurrentValue();
    s.amount = amount->getCurrentValue();
    engine.process(*fc.bufferForMidiMessages, fc.bufferNumSamples / sampleRate,
                   s);
}

//==============================================================================
const char *MidiEchoPlugin::xmlTypeName = "lmn3MidiEcho";

MidiEchoPlugin::MidiEchoPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info), tempoPosition(createPosition(edit.tempoSequence)) {
    time = addChoiceParam("time", TRANS("Time"), midifx::Echo::getTimeNames(),
                          timeValue, 2.0f);
    repeats = addNumberParam(
        "repeats", TRANS("Repeats"), {1.0f, 8.0f}, 1.0f, repeatsValue, 4.0f,
        [](float v) { return juce::String(juce::roundToInt(v)); });
    decay = addNumberParam("decay", TRANS("Decay"), {0.0f, 0.9f}, 0.05f,
                           decayValue, 0.3f, moreFxPercent);
    transpose = addNumberParam(
        "transpose", TRANS("Transpose"), {-12.0f, 12.0f}, 1.0f, transposeValue,
        0.0f, [](float v) {
            int st = juce::roundToInt(v);
            return (st > 0 ? "+" : "") + juce::String(st) + " st";
        });
}

MidiEchoPlugin::~MidiEchoPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

void MidiEchoPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;
    tempoPosition.set(fc.editTime);
    midifx::EchoEngine::Settings s;
    s.timeBeats = midifx::Echo::getTimeBeats(choice(time.get()));
    s.repeats = juce::roundToInt(repeats->getCurrentValue());
    s.decay = decay->getCurrentValue();
    s.transpose = juce::roundToInt(transpose->getCurrentValue());
    engine.process(*fc.bufferForMidiMessages, tempoPosition.getTempo() / 60.0,
                   fc.bufferNumSamples / sampleRate, s);
}


//==============================================================================
const char *FingerPlugin::xmlTypeName = "lmn3Finger";

FingerPlugin::FingerPlugin(tracktion::PluginCreationInfo info)
    : MidiFxPlugin(info), tempoPosition(createPosition(edit.tempoSequence)) {
    baseKey = addNumberParam(
        "baseKey", TRANS("Key"), {36.0f, 84.0f}, 1.0f, baseKeyValue, 54.0f,
        [](float v) {
            return juce::MidiMessage::getMidiNoteName(juce::roundToInt(v), true,
                                                      true, 4);
        });
    juce::StringArray variations{"1 of 4", "2 of 4", "3 of 4", "4 of 4"};
    for (int slot = 0; slot < midifx::Finger::numSlots; slot++) {
        auto n = juce::String(slot + 1);
        slotParams[slot][0] = addChoiceParam(
            "slot" + n + "pattern", "Pattern " + n,
            midifx::Finger::getPatternNames(), slotValues[slot][0], float(slot));
        slotParams[slot][1] = addChoiceParam("slot" + n + "variation",
                                             "Variation " + n, variations,
                                             slotValues[slot][1], 0.0f);
        slotParams[slot][2] = addChoiceParam(
            "slot" + n + "length", "Length " + n,
            midifx::Finger::getLengthNames(), slotValues[slot][2], 1.0f);
    }
}

FingerPlugin::~FingerPlugin() {
    notifyListenersOfDeletion();
    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();
}

tracktion::AutomatableParameter *FingerPlugin::getSlotParameter(int slot,
                                                                int which) const {
    return slotParams[juce::jlimit(0, midifx::Finger::numSlots - 1, slot)]
                     [juce::jlimit(0, 2, which)];
}

void FingerPlugin::initialise(const tracktion::PluginInitialisationInfo &) {
    freeRunningBeat = 0.0;
    updateKitNotes();
}

void FingerPlugin::updateKitNotes() {
    std::array<int, midifx::Finger::numRoles> notes{53, 54, 55, 56,
                                                    57, 58, 59, 60};
    if (auto track = dynamic_cast<tracktion::AudioTrack *>(getOwnerTrack())) {
        // a sample kit: find each role by its sound's name
        static const char *const words[midifx::Finger::numRoles][4] = {
            {"kick", "bd", nullptr, nullptr},
            {"snare", "sd", nullptr, nullptr},
            {"clap", "snare", nullptr, nullptr},
            {"rim", "clave", "snare", nullptr},
            {"hat", "hh", nullptr, nullptr},
            {"open", "hat", nullptr, nullptr},
            {"tom", nullptr, nullptr, nullptr},
            {"cowbell", "perc", "shaker", "clave"}};
        for (auto sampler :
             track->pluginList.getPluginsOfType<tracktion::SamplerPlugin>()) {
            for (int role = 0; role < midifx::Finger::numRoles; role++) {
                bool found = false;
                for (auto word : words[role]) {
                    if (word == nullptr || found)
                        break;
                    for (int i = 0; i < sampler->getNumSounds() && !found; i++) {
                        auto name = sampler->getSoundName(i).toLowerCase();
                        // a closed hat is a hat that isn't open
                        if (role == 4 && name.contains("open"))
                            continue;
                        if (name.contains(word)) {
                            notes[size_t(role)] = sampler->getKeyNote(i);
                            found = true;
                        }
                    }
                }
            }
            break;
        }
    }
    const juce::SpinLock::ScopedLockType l(kitLock);
    kitNotes = notes;
}

midifx::FingerEngine::Settings FingerPlugin::getSettings() const {
    midifx::FingerEngine::Settings s;
    s.baseKey = juce::roundToInt(baseKey->getCurrentValue());
    for (int slot = 0; slot < midifx::Finger::numSlots; slot++) {
        auto &sl = s.slots[size_t(slot)];
        sl.pattern = choice(slotParams[slot][0]);
        sl.variation = choice(slotParams[slot][1]);
        sl.length = choice(slotParams[slot][2]);
    }
    return s;
}

void FingerPlugin::applyToBuffer(const tracktion::PluginRenderContext &fc) {
    if (fc.bufferForMidiMessages == nullptr)
        return;
    double bps = 2.0;
    double start =
        moreFxBlockBeat(fc, tempoPosition, freeRunningBeat, sampleRate, bps);
    auto s = getSettings();
    {
        const juce::SpinLock::ScopedTryLockType l(kitLock);
        if (l.isLocked())
            s.notes = kitNotes;
    }
    engine.process(*fc.bufferForMidiMessages, start, bps,
                   fc.bufferNumSamples / sampleRate, s);
    heldMask = engine.getHeldMask();
    if (engine.getLastSlot() != lastReported) // the knobs follow the last key
        selectedSlot = lastReported = engine.getLastSlot();
}

} // namespace internal_plugins
