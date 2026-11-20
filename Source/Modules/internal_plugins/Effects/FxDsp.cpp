#include "FxDsp.h"

namespace internal_plugins {
namespace fx {

static constexpr float twoPi = juce::MathConstants<float>::twoPi;

//==============================================================================
void Smoothed::prepare(double sampleRate, float seconds) {
    coefficient = float(1.0 - std::exp(-1.0 / (juce::jmax(1.0e-4f, seconds) *
                                                sampleRate)));
}

void DelayLine::prepare(int maxSamples) {
    buffer.assign(size_t(juce::jmax(4, maxSamples + 2)), 0.0f);
    write = 0;
}
void DelayLine::clear() { std::fill(buffer.begin(), buffer.end(), 0.0f); }
void DelayLine::push(float x) {
    write = (write + 1) % int(buffer.size());
    buffer[size_t(write)] = x;
}
float DelayLine::read(float delaySamples) const {
    int size = int(buffer.size());
    float d = juce::jlimit(0.0f, float(size - 2), delaySamples);
    int whole = int(d);
    float part = d - float(whole);
    int i0 = (write - whole + size) % size;
    int i1 = (i0 - 1 + size) % size;
    return buffer[size_t(i0)] + (buffer[size_t(i1)] - buffer[size_t(i0)]) * part;
}

void Svf::setup(double sampleRate, float hz, float q) {
    hz = juce::jlimit(16.0f, float(sampleRate * 0.45), hz);
    float g = std::tan(juce::MathConstants<float>::pi * hz / float(sampleRate));
    k = 1.0f / juce::jmax(0.1f, q);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

float Svf::process(float x, Output out) {
    float v3 = x - ic2;
    float v1 = a1 * ic1 + a2 * v3;
    float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    switch (out) {
    case Output::lowPass:
        return v2;
    case Output::bandPass:
        return v1;
    case Output::highPass:
        return x - k * v1 - v2;
    case Output::notch:
        return x - k * v1;
    }
    return v2;
}

//==============================================================================
void Tape::prepare(double sr) {
    sampleRate = sr;
    for (auto &d : delay)
        d.prepare(int(sr * 0.03));
    drive.prepare(sr);
    hissLevel.prepare(sr);
    drive.reset(1.0f);
    hissLevel.reset(0.0f);
}

void Tape::process(float *left, float *right, int n, const Settings &s) {
    float *ch[2] = {left, right};
    const float base = float(sampleRate * 0.006); // 6 ms
    const float wowDepth = float(sampleRate * 0.003) * s.wow;
    const float flutterDepth = float(sampleRate * 0.0004) * s.flutter;
    const double wowInc = 0.55 / sampleRate, flutterInc = 6.5 / sampleRate;
    drive.set(1.0f + s.saturate * 7.0f);
    hissLevel.set(s.hiss * 0.025f);
    // brightness: 3 kHz (old, dull) to 20 kHz
    float bright = juce::jlimit(0.0f, 1.0f, s.tone * (1.0f - 0.6f * s.age));
    float cutoff = 3000.0f * std::pow(20000.0f / 3000.0f, bright);
    float lpCoef = 1.0f - std::exp(-twoPi * cutoff / float(sampleRate));

    for (int i = 0; i < n; i++) {
        wowPhase += wowInc;
        flutterPhase += flutterInc;
        if (wowPhase > 1)
            wowPhase -= 1;
        if (flutterPhase > 1)
            flutterPhase -= 1;
        float w = std::sin(twoPi * float(wowPhase)) +
                  0.3f * std::sin(twoPi * float(wowPhase) * 2.7f);
        float f = std::sin(twoPi * float(flutterPhase));
        float mod = base + wowDepth * w + flutterDepth * f;
        wobble = juce::jlimit(-1.0f, 1.0f, (wowDepth * w + flutterDepth * f) /
                                               float(sampleRate * 0.0034));

        // worn tape: now and then the level dips for a moment
        if (s.age > 0.0f && random.nextFloat() < s.age * 0.00004f)
            dropoutTarget = 0.35f + 0.5f * random.nextFloat();
        dropout += (dropoutTarget - dropout) * 0.002f;
        if (std::abs(dropout - dropoutTarget) < 0.01f)
            dropoutTarget = 1.0f;

        float g = drive.next();
        float norm = 1.0f / std::sqrt(g);
        float hiss = hissLevel.next();
        for (int c = 0; c < 2; c++) {
            if (ch[c] == nullptr)
                continue;
            delay[c].push(ch[c][i]);
            float x = delay[c].read(mod);
            x = std::tanh(g * x) * norm;
            lowPass[c] += (x - lowPass[c]) * lpCoef;
            float noise = (random.nextFloat() * 2.0f - 1.0f) * hiss;
            ch[c][i] = lowPass[c] * dropout + noise;
        }
    }
}

//==============================================================================
void Lofi::prepare(double sr) {
    sampleRate = sr;
    phase = 1.0;
    held[0] = held[1] = 0.0f;
    for (auto &f : filter)
        f.reset();
}

float Lofi::crush(float x, float bits) {
    float steps = std::pow(2.0f, juce::jlimit(1.0f, 16.0f, bits) - 1.0f);
    return std::round(juce::jlimit(-1.0f, 1.0f, x) * steps) / steps;
}

void Lofi::process(float *left, float *right, int n, const Settings &s) {
    float *ch[2] = {left, right};
    double inc = juce::jlimit(1.0e-3, 1.0, s.rateHz / sampleRate);
    for (auto &f : filter)
        f.setup(sampleRate, s.filterHz, 0.707f);
    for (int i = 0; i < n; i++) {
        phase += inc;
        bool take = phase >= 1.0;
        if (take)
            phase -= std::floor(phase);
        float noise = 0.0f;
        if (s.noise > 0.0f) {
            noise = (random.nextFloat() * 2.0f - 1.0f) * 0.02f * s.noise;
            if (random.nextFloat() < 0.0005f * s.noise) // vinyl crackle
                noise += (random.nextFloat() - 0.5f) * 0.5f * s.noise;
        }
        for (int c = 0; c < 2; c++) {
            if (ch[c] == nullptr)
                continue;
            float dry = ch[c][i];
            if (take)
                held[c] = crush(dry, s.bits);
            float wet = held[c] + noise;
            if (s.filterHz < 19000.0f)
                wet = filter[c].process(wet, Svf::Output::lowPass);
            ch[c][i] = dry + (wet - dry) * s.mix;
        }
    }
}

//==============================================================================
const juce::StringArray &Drive::getTypeNames() {
    static juce::StringArray names{"Soft clip", "Hard clip", "Fuzz", "Fold"};
    return names;
}

float Drive::shape(float x, int type) {
    switch (type) {
    case hard:
        return juce::jlimit(-1.0f, 1.0f, x);
    case fuzz: {
        // lopsided: the top squashes harder than the bottom
        float y = x > 0 ? std::tanh(x * 1.8f) : std::tanh(x * 0.7f) * 0.85f;
        return y;
    }
    case fold:
        return std::sin(x * juce::MathConstants<float>::halfPi);
    default:
        return std::tanh(x);
    }
}

void Drive::prepare(double sr) {
    sampleRate = sr;
    gain.prepare(sr);
    gain.reset(1.0f);
    toneState[0] = toneState[1] = 0.0f;
}

void Drive::process(float *left, float *right, int n, const Settings &s) {
    float *ch[2] = {left, right};
    gain.set(dbToGain(s.driveDb));
    float cutoff = 800.0f * std::pow(18000.0f / 800.0f, s.tone);
    float coef = 1.0f - std::exp(-twoPi * cutoff / float(sampleRate));
    float out = dbToGain(s.outputDb);
    for (int i = 0; i < n; i++) {
        float g = gain.next();
        // roughly level: louder drive doesn't mean much louder output
        float makeUp = 1.0f / std::sqrt(g);
        for (int c = 0; c < 2; c++) {
            if (ch[c] == nullptr)
                continue;
            float dry = ch[c][i];
            float wet = shape(dry * g, s.type) * makeUp;
            toneState[c] += (wet - toneState[c]) * coef;
            ch[c][i] = (dry + (toneState[c] - dry) * s.mix) * out;
        }
    }
}

//==============================================================================
const juce::StringArray &ResonantFilter::getTypeNames() {
    static juce::StringArray names{"Low-pass 24", "Low-pass 12", "High-pass",
                                   "Band-pass", "Notch"};
    return names;
}

void ResonantFilter::prepare(double sr) {
    sampleRate = sr;
    for (int c = 0; c < 2; c++) {
        stage1[c].reset();
        stage2[c].reset();
    }
    envelope = 0.0f;
}

void ResonantFilter::process(float *left, float *right, int n,
                             const BlockTime &time, const Settings &s) {
    float *ch[2] = {left, right};
    const float attack = 1.0f - std::exp(-1.0f / (0.004f * float(sampleRate)));
    const float release =
        1.0f - std::exp(-1.0f / (juce::jmax(0.01f, s.envRelease) *
                                 float(sampleRate)));
    const float q = 0.5f + s.resonance * s.resonance * 14.0f;
    Svf::Output out = Svf::Output::lowPass;
    if (s.type == highPass)
        out = Svf::Output::highPass;
    else if (s.type == bandPass)
        out = Svf::Output::bandPass;
    else if (s.type == notch)
        out = Svf::Output::notch;

    const int chunk = 16; // coefficients follow the modulation every 16
    for (int start = 0; start < n; start += chunk) {
        int len = juce::jmin(chunk, n - start);
        // input level for the envelope
        for (int i = start; i < start + len; i++) {
            float level = 0.0f;
            for (auto c : ch)
                if (c != nullptr)
                    level = juce::jmax(level, std::abs(c[i]));
            envelope += (level - envelope) * (level > envelope ? attack : release);
        }
        double beat =
            time.startBeat + (start / time.sampleRate) * time.beatsPerSecond;
        double cycle = beat / juce::jmax(0.0625, s.lfoBeats);
        lfoValue = std::sin(twoPi * float(cycle - std::floor(cycle)));
        float octaves = s.envelope * juce::jmin(1.0f, envelope * 2.0f) * 5.0f +
                        s.lfo * lfoValue * 3.0f;
        currentCutoff = juce::jlimit(20.0f, 20000.0f,
                                     s.cutoffHz * std::pow(2.0f, octaves));
        for (int c = 0; c < 2; c++) {
            stage1[c].setup(sampleRate, currentCutoff,
                            s.type == lowPass24 ? 0.54f : q);
            stage2[c].setup(sampleRate, currentCutoff, q);
        }
        for (int c = 0; c < 2; c++) {
            if (ch[c] == nullptr)
                continue;
            for (int i = start; i < start + len; i++) {
                float y = stage1[c].process(ch[c][i], out);
                if (s.type == lowPass24)
                    y = stage2[c].process(y, out);
                // keep big resonance from blowing up
                ch[c][i] = std::tanh(y);
            }
        }
    }
}

//==============================================================================
const juce::StringArray &Ducker::getShapeNames() {
    static juce::StringArray names{"Smooth", "Punchy", "Gate"};
    return names;
}

void Ducker::prepare(double sr) {
    sampleRate = sr;
    sinceHit = 1.0e9;
    detector = 0.0f;
    armed = true;
}

float Ducker::gainAt(float t, const Settings &s) {
    const float attack = 0.004f;
    float release = juce::jmax(0.01f, s.releaseSeconds);
    float duck;
    if (t < attack)
        duck = t / attack; // dips quickly, without a click
    else {
        float x = juce::jlimit(0.0f, 1.0f, (t - attack) / release);
        float back;
        if (s.shape == 1)
            back = x * x * x; // stays down, then snaps back
        else if (s.shape == 2)
            back = x < 0.85f ? 0.0f : 1.0f;
        else
            back = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * x);
        duck = 1.0f - back;
    }
    return 1.0f - juce::jlimit(0.0f, 1.0f, s.depth) * duck;
}

void Ducker::process(float *left, float *right, const float *scLeft,
                     const float *scRight, int n, const BlockTime &time,
                     const Settings &s) {
    const double dt = 1.0 / sampleRate;
    const float fall = std::exp(-1.0f / (0.05f * float(sampleRate)));
    const double beatsPerSample = time.beatsPerSecond / time.sampleRate;
    const bool useSidechain = scLeft != nullptr || scRight != nullptr;
    for (int i = 0; i < n; i++) {
        bool hit = false;
        if (useSidechain) {
            float level = 0.0f;
            if (scLeft)
                level = std::abs(scLeft[i]);
            if (scRight)
                level = juce::jmax(level, std::abs(scRight[i]));
            detector = juce::jmax(level, detector * fall);
            if (armed && detector > 0.2f && sinceHit > 0.05) {
                hit = true;
                armed = false;
            } else if (detector < 0.08f)
                armed = true;
        } else {
            // this sample is the first of a new beat
            double b0 = time.startBeat + i * beatsPerSample;
            hit = std::floor(b0) != std::floor(b0 - beatsPerSample);
        }
        if (hit) {
            sinceHit = 0.0;
            hits++;
        }
        float g = gainAt(float(sinceHit), s);
        sinceHit += dt;
        if (left)
            left[i] *= g;
        if (right)
            right[i] *= g;
        lastGain = g;
    }
}

//==============================================================================
const juce::StringArray &BeatRepeat::getLengthNames() {
    static juce::StringArray names{"1/2", "1/4", "1/8", "1/16", "1/32"};
    return names;
}
double BeatRepeat::getLengthBeats(int index) {
    static const double beats[] = {2.0, 1.0, 0.5, 0.25, 0.125};
    return beats[juce::jlimit(0, 4, index)];
}
const juce::StringArray &BeatRepeat::getRepeatNames() {
    static juce::StringArray names{"1", "2", "3", "4", "6", "8", "16", "Hold"};
    return names;
}
int BeatRepeat::getRepeatCount(int index) {
    static const int counts[] = {1, 2, 3, 4, 6, 8, 16, -1};
    return counts[juce::jlimit(0, 7, index)];
}

void BeatRepeat::prepare(double sr) {
    sampleRate = sr;
    for (int c = 0; c < 2; c++) {
        history[c].assign(size_t(sr * 4.5), 0.0f);
        slice[c].assign(size_t(sr * 4.5), 0.0f);
    }
    write = 0;
    repeating = wasActive = pending = false;
    wet = 0.0f;
}

void BeatRepeat::startSlice(int length) {
    int size = int(history[0].size());
    sliceLength = juce::jlimit(16, size - 1, length);
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < sliceLength; i++)
            slice[c][size_t(i)] =
                history[c][size_t((write - sliceLength + i + size) % size)];
    readPos = 0;
    rate = 1;
    gain = 1.0f;
    repeatIndex = 0;
    repeating = true;
}

void BeatRepeat::process(float *left, float *right, int n,
                         const BlockTime &time, const Settings &s) {
    if (history[0].empty())
        return;
    float *ch[2] = {left, right};
    const int size = int(history[0].size());
    const double lengthBeats = juce::jmax(0.0625, s.lengthBeats);
    const int length = juce::roundToInt(lengthBeats / time.beatsPerSecond *
                                        time.sampleRate);
    const double beatsPerSample = time.beatsPerSecond / time.sampleRate;
    const float fadeStep = float(1.0 / (0.003 * sampleRate));
    const int edge = juce::jmax(1, int(0.002 * sampleRate));

    for (int i = 0; i < n; i++) {
        // keep recording
        write = (write + 1) % size;
        for (int c = 0; c < 2; c++)
            history[c][size_t(write)] = ch[c] ? ch[c][i] : 0.0f;

        if (!s.active)
            repeating = pending = false;
        else if (!wasActive)
            pending = true;
        wasActive = s.active;
        // start on the next slice boundary after the button goes down,
        // repeating the slice that just ended
        if (pending) {
            // this sample is the first of a new slice
            double b0 = time.startBeat + i * beatsPerSample;
            double before = b0 - beatsPerSample;
            if (std::floor(b0 / lengthBeats) !=
                std::floor(before / lengthBeats)) {
                startSlice(length);
                pending = false;
            }
        }

        float target = repeating ? 1.0f : 0.0f;
        wet += juce::jlimit(-fadeStep, fadeStep, target - wet);
        if (wet <= 0.0f && !repeating)
            continue;

        float outL = 0, outR = 0;
        if (repeating) {
            int p = int(readPos);
            float part = float(readPos - p);
            int p1 = juce::jmin(p + 1, sliceLength - 1);
            // short fades at the slice edges so the loop doesn't click
            float env = juce::jmin(1.0f, juce::jmin(float(readPos) / edge,
                                                    float(sliceLength - readPos) /
                                                        edge));
            env = juce::jmax(0.0f, env) * gain;
            outL = (slice[0][size_t(p)] +
                    (slice[0][size_t(p1)] - slice[0][size_t(p)]) * part) *
                   env;
            outR = (slice[1][size_t(p)] +
                    (slice[1][size_t(p1)] - slice[1][size_t(p)]) * part) *
                   env;
            readPos += rate;
            if (readPos >= sliceLength) {
                repeatIndex++;
                readPos = 0;
                gain *= 1.0f - juce::jlimit(0.0f, 0.95f, s.decay);
                rate = std::pow(2.0, repeatIndex * s.pitch / 12.0);
                if (s.repeats > 0 && repeatIndex >= s.repeats)
                    repeating = false;
            }
        }
        for (int c = 0; c < 2; c++)
            if (ch[c])
                ch[c][i] = ch[c][i] * (1.0f - wet) + (c == 0 ? outL : outR) * wet;
    }
}

} // namespace fx
} // namespace internal_plugins
