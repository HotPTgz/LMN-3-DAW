#include "InstrumentDsp.h"

namespace internal_plugins {
namespace instruments {

static constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
static constexpr float kPi = juce::MathConstants<float>::pi;

// How loud each harmonic is for a vowel (formants), with harmonics of refHz
static float vowelGain(float hz, float vowel) {
    // a, e, i, o, u: three formants each
    static const float formants[5][3] = {{730, 1090, 2440},
                                         {530, 1840, 2480},
                                         {270, 2290, 3010},
                                         {570, 840, 2410},
                                         {300, 870, 2240}};
    static const float widths[3] = {90, 110, 170};
    static const float levels[3] = {1.0f, 0.6f, 0.3f};
    vowel = juce::jlimit(0.0f, 3.999f, vowel);
    int v0 = int(vowel);
    float mix = vowel - float(v0);
    float g = 0.02f;
    for (int k = 0; k < 3; k++) {
        float f = formants[v0][k] + (formants[v0 + 1][k] - formants[v0][k]) * mix;
        float d = (hz - f) / widths[k];
        g += levels[k] * std::exp(-d * d);
    }
    return g;
}

//==============================================================================
const juce::StringArray &Wavetables::getNames() {
    static juce::StringArray names{"Glass", "Sine to saw", "Vocal", "Metal",
                                   "PWM"};
    return names;
}

const Wavetables &Wavetables::get() {
    static const Wavetables tables;
    return tables;
}

static std::vector<float> harmonicLevels(int table, int frame, int count) {
    std::vector<float> a(size_t(count) + 1, 0.0f);
    float x = frame / float(Wavetables::numFrames - 1);
    for (int n = 1; n <= count; n++) {
        float level = 0.0f;
        switch (table) {
        case 0: { // glass: a sine growing bell-like partials
            int root = juce::roundToInt(std::sqrt(float(n)));
            bool square = root * root == n;
            level = (n == 1 ? 1.0f - 0.4f * x : 0.0f) +
                    x * (square ? 1.0f / float(n) : 0.0f) +
                    0.12f * x / std::pow(float(n), 1.5f);
            break;
        }
        case 1: // sine to saw
            level = n == 1 ? 1.0f : x / float(n);
            break;
        case 2: // vocal: a saw through vowels a-e-i-o-u
            level = vowelGain(130.0f * n, x * 4.0f) / std::pow(float(n), 0.7f);
            break;
        case 3: { // metal
            float s = 0.5f + 0.5f * std::sin(n * (1.0f + 3.0f * x) * 1.7f);
            level = s * s / float(n) * ((n % 2) ? 1.0f : 0.3f + 0.7f * x);
            break;
        }
        default: { // PWM: pulse width 50% down to 5%
            float w = 0.5f - 0.45f * x;
            level = std::sin(n * kPi * w) / float(n);
        }
        }
        a[size_t(n)] = level;
    }
    return a;
}

Wavetables::Wavetables() {
    data.resize(size_t(numTables * numFrames * numLevels));
    for (int t = 0; t < numTables; t++)
        for (int f = 0; f < numFrames; f++) {
            auto levels = harmonicLevels(t, f, 64);
            float gain = 1.0f;
            for (int l = 0; l < numLevels; l++) {
                int count = 64 >> l;
                std::vector<float> wave(size_t(size), 0.0f);
                for (int n = 1; n <= count; n++) {
                    float a = levels[size_t(n)];
                    if (std::abs(a) < 1.0e-5f)
                        continue;
                    for (int i = 0; i < size; i++)
                        wave[size_t(i)] +=
                            a * std::sin(kTwoPi * float(n) * i / float(size));
                }
                if (l == 0) { // even out the frames' loudness
                    float peak = 1.0e-6f;
                    for (auto v : wave)
                        peak = juce::jmax(peak, std::abs(v));
                    gain = 0.8f / peak;
                }
                for (auto &v : wave)
                    v *= gain;
                data[size_t((t * numFrames + f) * numLevels + l)] =
                    std::move(wave);
            }
        }
}

const std::vector<float> &Wavetables::at(int t, int f, int level) const {
    return data[size_t((t * numFrames + f) * numLevels + level)];
}

const std::vector<float> &Wavetables::frame(int table, int frameIndex) const {
    return at(juce::jlimit(0, numTables - 1, table),
              juce::jlimit(0, numFrames - 1, frameIndex), 0);
}

float Wavetables::sample(int table, float position, double phase, float hz,
                         double sampleRate) const {
    table = juce::jlimit(0, numTables - 1, table);
    // fewer harmonics for higher notes, so nothing folds back
    float allowed = float(sampleRate * 0.45) / juce::jmax(1.0f, hz);
    int level = 0;
    while (level < numLevels - 1 && float(64 >> level) > allowed)
        level++;
    float fp = juce::jlimit(0.0f, 1.0f, position) * (numFrames - 1);
    int f0 = juce::jmin(int(fp), numFrames - 2);
    float mix = fp - float(f0);
    double ph = (phase - std::floor(phase)) * size;
    int i0 = int(ph) % size, i1 = (i0 + 1) % size;
    float part = float(ph - std::floor(ph));
    auto &a = at(table, f0, level), &b = at(table, f0 + 1, level);
    float va = a[size_t(i0)] + (a[size_t(i1)] - a[size_t(i0)]) * part;
    float vb = b[size_t(i0)] + (b[size_t(i1)] - b[size_t(i0)]) * part;
    return va + (vb - va) * mix;
}

float WavetableSound::warpPhase(double phase, float warp) {
    // bends the cycle: the first half squeezes, the second stretches
    float p = float(phase - std::floor(phase));
    float w = 0.5f - 0.45f * juce::jlimit(0.0f, 1.0f, warp);
    return p < w ? p * 0.5f / w : 0.5f + (p - w) * 0.5f / (1.0f - w);
}

void WavetableSound::start(float f, float, float, double sr, const float *) {
    frequency = f;
    sampleRate = sr;
    phase1 = 0.0;
    phase2 = 0.37;
    envTime = 0.0f;
    lowPass = 0.0f;
}

float WavetableSound::next(const float *p) {
    auto &tables = Wavetables::get();
    int table = juce::roundToInt(p[0]);
    float envelope = std::exp(-envTime / juce::jmax(0.01f, p[5]));
    float position = juce::jlimit(0.0f, 1.0f, p[1] + p[4] * envelope);
    envTime += float(1.0 / sampleRate);
    float warp = p[2];
    float out = tables.sample(table, position, warpPhase(phase1, warp),
                              frequency, sampleRate);
    phase1 += frequency / sampleRate;
    if (phase1 >= 1.0)
        phase1 -= 1.0;
    if (p[6] > 0.5f) {
        float f2 = frequency * std::pow(2.0f, p[6] / 1200.0f);
        out = 0.5f * (out + tables.sample(table, position,
                                          warpPhase(phase2, warp), f2,
                                          sampleRate));
        phase2 += f2 / sampleRate;
        if (phase2 >= 1.0)
            phase2 -= 1.0;
    }
    float cutoff = 200.0f * std::pow(100.0f, juce::jlimit(0.0f, 1.0f, p[3]));
    float coef = 1.0f - std::exp(-kTwoPi * cutoff / float(sampleRate));
    lowPass += (out - lowPass) * coef;
    return lowPass * p[7];
}

//==============================================================================
namespace Mallets {
const juce::StringArray &getMaterialNames() {
    static juce::StringArray names{"Wood", "Glass", "Metal", "Kalimba"};
    return names;
}
const Material &getMaterial(int index) {
    static const Material materials[] = {
        // marimba bar: tuned 1 : 4 : 10
        {{1.0f, 3.99f, 10.65f, 16.2f},
         {1.0f, 0.35f, 0.15f, 0.08f},
         {1.0f, 0.5f, 0.25f, 0.12f}},
        // glass
        {{1.0f, 2.76f, 5.40f, 8.93f, 13.3f},
         {1.0f, 0.7f, 0.5f, 0.35f, 0.25f},
         {1.0f, 0.6f, 0.45f, 0.3f, 0.2f}},
        // bell: hum, fundamental, minor third, fifth, octave...
        {{0.5f, 1.0f, 1.19f, 1.5f, 2.0f, 2.5f, 2.67f, 3.0f},
         {1.4f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.35f},
         {0.4f, 1.0f, 0.6f, 0.5f, 0.45f, 0.3f, 0.25f, 0.2f}},
        // kalimba tine
        {{1.0f, 5.92f, 16.7f}, {1.0f, 0.25f, 0.08f}, {1.0f, 0.35f, 0.15f}},
    };
    return materials[juce::jlimit(0, 3, index)];
}
} // namespace Mallets

void MalletSound::start(float frequency, float, float velocity, double sr,
                        const float *p) {
    auto &material = Mallets::getMaterial(juce::roundToInt(p[0]));
    float hardness = juce::jlimit(0.0f, 1.0f, p[1]) * (0.6f + 0.4f * velocity);
    float decay = juce::jmax(0.05f, p[2]);
    float tone = juce::jlimit(0.0f, 1.0f, p[3]);
    float body = juce::jlimit(0.0f, 1.0f, p[4]);
    float strike = juce::jlimit(0.0f, 1.0f, p[5]);
    numModes = 0;
    for (size_t m = 0; m < material.ratios.size() && numModes < maxModes; m++) {
        float ratio = material.ratios[m];
        float hz = frequency * ratio;
        if (hz > float(sr) * 0.45f)
            break;
        float octaves = std::log2(juce::jmax(1.0f, ratio));
        float level = material.levels[m] *
                      std::pow(0.08f + 0.92f * hardness, octaves) *
                      std::pow(0.4f + 1.2f * tone, octaves * 0.5f);
        // where the mallet hits: some modes are quieter near their nodes
        float node = std::abs(std::sin(kPi * float(m + 1) *
                                       (0.08f + 0.42f * strike)));
        level *= 0.35f + 0.65f * node;
        float tau = decay * material.decays[m];
        if (ratio == 1.0f) { // the body (resonator tube) helps the fundamental
            level *= 1.0f + body;
            tau *= 1.0f + 2.0f * body;
        }
        auto &mode = modes[numModes++];
        float w = kTwoPi * hz / float(sr);
        mode.cosw = std::cos(w);
        mode.sinw = std::sin(w);
        mode.decay = std::exp(-1.0f / (juce::jmax(0.005f, tau) * float(sr)));
        mode.re = level;
        mode.im = 0.0f;
        mode.level = level;
    }
    clickLevel = juce::jlimit(0.0f, 1.0f, p[6]) * (0.3f + 0.7f * hardness);
    clickDecay = std::exp(-1.0f / ((0.006f - 0.004f * hardness) * float(sr)));
    clickState = 0.0f;
}

float MalletSound::next(const float *p) {
    float out = 0.0f;
    for (int m = 0; m < numModes; m++) {
        auto &mode = modes[m];
        float re = mode.re * mode.cosw - mode.im * mode.sinw;
        float im = mode.re * mode.sinw + mode.im * mode.cosw;
        mode.re = re * mode.decay;
        mode.im = im * mode.decay;
        out += mode.im;
    }
    if (clickLevel > 1.0e-4f) {
        float n = random.nextFloat() * 2.0f - 1.0f;
        float bright = n - clickState; // a crude high-pass
        clickState = n;
        out += bright * clickLevel * 0.5f;
        clickLevel *= clickDecay;
    }
    return out * 0.5f * p[7];
}

//==============================================================================
std::shared_ptr<GrainSample> GrainSample::fromBuffer(juce::AudioBuffer<float> b,
                                                     double sr,
                                                     const juce::String &name) {
    auto s = std::make_shared<GrainSample>();
    s->audio = std::move(b);
    s->sampleRate = sr;
    s->name = name;
    int n = s->audio.getNumSamples();
    s->overview.assign(96, 0.0f);
    float peak = 1.0e-6f;
    for (int i = 0; i < 96 && n > 0; i++) {
        int from = i * n / 96, to = juce::jmax(from + 1, (i + 1) * n / 96);
        float level = 0.0f;
        for (int k = from; k < juce::jmin(to, n); k++)
            level = juce::jmax(level, std::abs(s->audio.getSample(0, k)));
        s->overview[size_t(i)] = level;
        peak = juce::jmax(peak, level);
    }
    for (auto &v : s->overview)
        v /= peak;
    return s;
}

std::shared_ptr<GrainSample> GrainSample::builtIn() {
    static std::shared_ptr<GrainSample> voice = [] {
        const double sr = 44100.0;
        const int n = int(sr * 2.4);
        juce::AudioBuffer<float> b(1, n);
        juce::Random random(21);
        double phase = 0.0;
        float peak = 1.0e-6f;
        for (int i = 0; i < n; i++) {
            float t = float(i / sr);
            float hz = 196.0f * (1.0f + 0.012f * std::sin(kTwoPi * 5.0f * t));
            phase += hz / sr;
            float vowel = juce::jlimit(0.0f, 3.99f,
                                       t < 0.8f   ? 0.0f
                                       : t < 1.6f ? 3.0f * (t - 0.8f) / 0.8f
                                                  : 3.0f - (t - 1.6f) / 0.8f);
            float x = 0.0f;
            for (int h = 1; h <= 40; h++)
                x += vowelGain(hz * h, vowel) / std::pow(float(h), 0.8f) *
                     std::sin(kTwoPi * float(h) * float(phase));
            x += (random.nextFloat() - 0.5f) * 0.03f; // breath
            float edge = juce::jmin(1.0f, juce::jmin(t, 2.4f - t) / 0.05f);
            b.setSample(0, i, x * edge);
            peak = juce::jmax(peak, std::abs(x));
        }
        b.applyGain(0.8f / peak);
        return fromBuffer(std::move(b), sr, "Built-in voice");
    }();
    return voice;
}

void GrainSampleSlot::set(std::shared_ptr<const GrainSample> s) {
    if (s == nullptr)
        return;
    const juce::SpinLock::ScopedLockType l(lock);
    previous = current;
    current = std::move(s);
}

std::shared_ptr<const GrainSample> GrainSampleSlot::get() const {
    const juce::SpinLock::ScopedLockType l(lock);
    return current;
}

void GrainSound::start(float frequency, float, float, double sr,
                       const float *) {
    sample = slot->get();
    sampleRate = sr;
    noteRatio = frequency / engines::midiNoteToHz(60.0f);
    untilNext = 0;
    scan = 0;
    for (auto &g : grains)
        g.active = false;
}

int GrainSound::getNumActiveGrains() const {
    int count = 0;
    for (auto &g : grains)
        count += g.active;
    return count;
}

float GrainSound::next(const float *p) {
    if (sample == nullptr)
        return 0.0f;
    const int len = sample->audio.getNumSamples();
    if (len < 16)
        return 0.0f;
    const float *data = sample->audio.getReadPointer(0);
    const double fileRate = sample->sampleRate / sampleRate;
    const bool freeze = p[6] > 0.5f;
    const float size = juce::jlimit(0.005f, 1.0f, p[1]);
    const float density = juce::jlimit(1.0f, 200.0f, p[2]);

    // not frozen: the grain window travels through the sample
    if (!freeze) {
        scan += fileRate / len;
        if (scan >= 1.0)
            scan -= 1.0;
    }
    untilNext -= 1.0;
    if (untilNext <= 0.0) {
        for (auto &g : grains)
            if (!g.active) {
                double centre = p[0] + scan +
                                (random.nextDouble() * 2.0 - 1.0) * p[3] * 0.25;
                centre -= std::floor(centre);
                float semis = p[4] + (random.nextFloat() * 2.0f - 1.0f) * p[5];
                g.pos = centre * len;
                g.rate = noteRatio * std::pow(2.0f, semis / 12.0f) * fileRate;
                g.length = juce::jmax(16, int(size * sampleRate));
                g.age = 0;
                g.active = true;
                break;
            }
        untilNext = sampleRate / density * (0.75 + 0.5 * random.nextDouble());
    }

    float out = 0.0f;
    for (auto &g : grains) {
        if (!g.active)
            continue;
        float window =
            0.5f - 0.5f * std::cos(kTwoPi * float(g.age) / float(g.length));
        double wrapped = g.pos - std::floor(g.pos / len) * len;
        int i0 = int(wrapped) % len, i1 = (i0 + 1) % len;
        float part = float(wrapped - std::floor(wrapped));
        out += (data[i0] + (data[i1] - data[i0]) * part) * window;
        g.pos += g.rate;
        if (++g.age >= g.length)
            g.active = false;
    }
    // even out the level as grains overlap more
    float overlap = juce::jmax(1.0f, density * size);
    if ((++counter & 255) == 0)
        slot->playhead = float(std::fmod(p[0] + scan, 1.0));
    return out / std::sqrt(overlap) * p[7];
}

//==============================================================================
const juce::StringArray &DrumKit::getPadNames() {
    static juce::StringArray names{"kick", "snare", "clap",     "rim",
                                   "hat",  "open hat", "tom", "cowbell"};
    return names;
}

const juce::StringArray &DrumKit::getParamNames() {
    static juce::StringArray names{"Pitch", "Punch", "Decay",
                                   "Tone",  "Noise", "Drive"};
    return names;
}

DrumKit::PadParams DrumKit::defaults(int pad) {
    static const PadParams d[numPads] = {
        {0.25f, 0.7f, 0.4f, 0.3f, 0.1f, 0.15f},  // kick
        {0.4f, 0.4f, 0.35f, 0.6f, 0.6f, 0.1f},   // snare
        {0.5f, 0.5f, 0.35f, 0.5f, 0.6f, 0.1f},   // clap
        {0.5f, 0.5f, 0.3f, 0.6f, 0.3f, 0.1f},    // rim
        {0.5f, 0.3f, 0.3f, 0.7f, 0.5f, 0.0f},    // hat
        {0.5f, 0.3f, 0.45f, 0.7f, 0.5f, 0.0f},   // open hat
        {0.35f, 0.6f, 0.45f, 0.4f, 0.15f, 0.1f}, // tom
        {0.5f, 0.3f, 0.35f, 0.5f, 0.0f, 0.1f},   // cowbell
    };
    return d[juce::jlimit(0, numPads - 1, pad)];
}

static float expMap(float lo, float hi, float knob) {
    return lo * std::pow(hi / lo, juce::jlimit(0.0f, 1.0f, knob));
}

float DrumKit::pitchHz(int pad, float knob) {
    static const float ranges[numPads][2] = {
        {30, 120},  {120, 400},  {600, 3000}, {300, 1800},
        {20, 80},   {20, 80},    {60, 400},   {300, 900}};
    auto &r = ranges[juce::jlimit(0, numPads - 1, pad)];
    return expMap(r[0], r[1], knob);
}

float DrumKit::decaySeconds(int pad, float knob) {
    static const float ranges[numPads][2] = {
        {0.08f, 1.5f}, {0.05f, 0.6f}, {0.05f, 0.8f}, {0.01f, 0.15f},
        {0.01f, 0.25f}, {0.15f, 1.5f}, {0.08f, 1.2f}, {0.05f, 0.8f}};
    auto &r = ranges[juce::jlimit(0, numPads - 1, pad)];
    return expMap(r[0], r[1], knob);
}

juce::String DrumKit::describe(int pad, int param, float knob) {
    if (param == 0) {
        float hz = pitchHz(pad, knob);
        if (pad == hat || pad == openHat) // the metal's base pitch
            return juce::String(juce::roundToInt(hz)) + " Hz";
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + " kHz"
                             : juce::String(juce::roundToInt(hz)) + " Hz";
    }
    if (param == 2) {
        float s = decaySeconds(pad, knob);
        return s < 1.0f ? juce::String(juce::roundToInt(s * 1000.0f)) + " ms"
                        : juce::String(s, 2) + " s";
    }
    return juce::String(juce::roundToInt(knob * 100.0f)) + "%";
}

void DrumKit::prepare(double sr) {
    sampleRate = sr;
    allOff();
}

void DrumKit::allOff() {
    for (auto &v : voices)
        v.active = false;
}

void DrumKit::trigger(int pad, float velocity, const PadParams &p) {
    pad = juce::jlimit(0, numPads - 1, pad);
    auto &v = voices[pad];
    v.active = true;
    v.choking = false;
    v.t = 0;
    v.velocity = velocity;
    v.level = 1.0f;
    for (auto &ph : v.phase)
        ph = 0;
    v.noiseFilter.reset();
    v.bodyFilter.reset();
    float hz = pitchHz(pad, p[0]);
    float tone = p[3];
    switch (pad) {
    case snare:
        v.noiseFilter.setup(sampleRate, 1000.0f + tone * 7000.0f, 0.7f);
        break;
    case clap:
        v.noiseFilter.setup(sampleRate, hz, 1.5f + tone * 2.0f);
        break;
    case rim:
        v.noiseFilter.setup(sampleRate, 3000.0f + tone * 5000.0f, 0.7f);
        break;
    case hat:
    case openHat:
        v.noiseFilter.setup(sampleRate, 4000.0f + tone * 8000.0f, 0.7f);
        break;
    case cowbell:
        v.bodyFilter.setup(sampleRate, hz * 1.5f, 1.5f + tone * 3.0f);
        break;
    default:
        v.noiseFilter.setup(sampleRate, 2000.0f + tone * 6000.0f, 0.7f);
    }
    // a closed hat stops the open one
    if (pad == hat && voices[openHat].active)
        voices[openHat].choking = true;
}

static float squareWave(double phase) {
    return phase - std::floor(phase) < 0.5 ? 1.0f : -1.0f;
}

float DrumKit::renderVoice(int pad, Voice &v, const PadParams &p) {
    const float t = float(v.t);
    const double dt = 1.0 / sampleRate;
    const float hz = pitchHz(pad, p[0]);
    const float punch = p[1], tau = decaySeconds(pad, p[2]);
    const float tone = p[3], noiseAmount = p[4];
    const float noise = random.nextFloat() * 2.0f - 1.0f;
    float out = 0.0f;
    using Out = fx::Svf::Output;

    switch (pad) {
    case kick: {
        float f = hz * (1.0f + punch * 5.0f * std::exp(-t / 0.025f));
        v.phase[0] += f * dt;
        float env = std::exp(-t / tau);
        out = std::sin(kTwoPi * float(v.phase[0])) * env;
        out += tone * 0.3f * std::sin(2.0f * kTwoPi * float(v.phase[0])) *
               std::exp(-t / (tau * 0.3f));
        out += noise * std::exp(-t / 0.004f) * noiseAmount * 0.6f;
        break;
    }
    case snare: {
        float f = hz * (1.0f + punch * 1.5f * std::exp(-t / 0.01f));
        v.phase[0] += f * dt;
        v.phase[1] += f * 1.47f * dt;
        float body = (std::sin(kTwoPi * float(v.phase[0])) +
                      0.5f * std::sin(kTwoPi * float(v.phase[1]))) *
                     std::exp(-t / (tau * 0.5f)) * 0.6f;
        float rattle =
            v.noiseFilter.process(noise, Out::highPass) * std::exp(-t / tau);
        out = body * (1.0f - noiseAmount * 0.7f) + rattle * noiseAmount;
        break;
    }
    case clap: {
        float spacing = 0.016f - 0.01f * punch;
        float env = 0.0f;
        for (int k = 0; k < 3; k++) {
            float tk = t - k * spacing;
            if (tk >= 0)
                env = juce::jmax(env, std::exp(-tk / 0.006f));
        }
        float tailStart = 3 * spacing;
        if (t > tailStart)
            env = juce::jmax(env, (0.4f + 0.4f * noiseAmount) *
                                      std::exp(-(t - tailStart) / tau));
        out = v.noiseFilter.process(noise, Out::bandPass) * env * 2.5f;
        break;
    }
    case rim: {
        v.phase[0] += hz * dt;
        v.phase[1] += hz * 2.3f * dt;
        out = (std::sin(kTwoPi * float(v.phase[0])) +
               0.6f * std::sin(kTwoPi * float(v.phase[1]))) *
              std::exp(-t / tau) * 0.6f;
        out += v.noiseFilter.process(noise, Out::highPass) *
               std::exp(-t / 0.002f) * noiseAmount * (0.5f + punch);
        break;
    }
    case hat:
    case openHat: {
        // six detuned squares (the 808's metal)
        static const float ratios[6] = {2.0f, 3.0f, 4.16f, 5.43f, 6.79f, 8.21f};
        float metal = 0.0f;
        for (int k = 0; k < 6; k++) {
            v.phase[k] += hz * ratios[k] * dt;
            metal += squareWave(v.phase[k]);
        }
        metal /= 6.0f;
        float mixed = metal * (1.0f - noiseAmount * 0.6f) + noise * noiseAmount * 0.6f;
        float env = std::exp(-t / tau) + punch * std::exp(-t / 0.002f) * 0.5f;
        out = v.noiseFilter.process(mixed, Out::highPass) * env;
        break;
    }
    case tom: {
        float f = hz * (1.0f + punch * 0.8f * std::exp(-t / 0.04f));
        v.phase[0] += f * dt;
        out = std::sin(kTwoPi * float(v.phase[0])) * std::exp(-t / tau);
        out += v.noiseFilter.process(noise, Out::bandPass) *
               std::exp(-t / 0.01f) * noiseAmount * 0.5f;
        break;
    }
    default: { // cowbell
        v.phase[0] += hz * dt;
        v.phase[1] += hz * 1.48f * dt;
        float x = 0.5f * (squareWave(v.phase[0]) + squareWave(v.phase[1]));
        float env = 0.5f * std::exp(-t / 0.015f) * (0.5f + punch) +
                    0.5f * std::exp(-t / tau);
        out = v.bodyFilter.process(x, Out::bandPass) * env * 1.5f;
        out += noise * noiseAmount * 0.2f * std::exp(-t / 0.01f);
    }
    }

    float drive = 1.0f + p[5] * 8.0f;
    out = std::tanh(out * drive) / (1.0f + p[5] * 1.5f);
    v.t += dt;
    if (v.choking) {
        v.level *= 0.995f;
        if (v.level < 0.001f)
            v.active = false;
    }
    if (v.t > tau * 7.0 + 0.06)
        v.active = false;
    return out * v.velocity * v.level;
}

void DrumKit::render(float *left, float *right, int n,
                     const std::array<PadParams, numPads> &params) {
    static const float pans[numPads] = {0.0f,  0.05f, -0.1f, 0.15f,
                                        0.25f, 0.25f, -0.2f, 0.1f};
    for (int pad = 0; pad < numPads; pad++) {
        auto &v = voices[pad];
        if (!v.active)
            continue;
        float gl = 0.7f * (1.0f - pans[pad]), gr = 0.7f * (1.0f + pans[pad]);
        for (int i = 0; i < n && v.active; i++) {
            float x = renderVoice(pad, v, params[size_t(pad)]);
            if (left)
                left[i] += x * gl;
            if (right)
                right[i] += x * gr;
        }
    }
}

std::vector<float> DrumKit::preview(int pad, const PadParams &params,
                                    double sr, int numSamples) {
    DrumKit kit;
    kit.prepare(sr);
    std::array<PadParams, numPads> all;
    all[size_t(juce::jlimit(0, numPads - 1, pad))] = params;
    kit.trigger(pad, 1.0f, params);
    std::vector<float> out(static_cast<size_t>(numSamples), 0.0f);
    kit.render(out.data(), nullptr, numSamples, all);
    return out;
}

} // namespace instruments
} // namespace internal_plugins
