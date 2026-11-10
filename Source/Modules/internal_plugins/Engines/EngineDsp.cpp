#include "EngineDsp.h"

namespace internal_plugins {
namespace engines {

float midiNoteToHz(float note) {
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

// one-pole smoothing coefficient that gets ~99% of the way in `seconds`
static float approachCoeff(float seconds, double sampleRate) {
    if (seconds <= 0.0005f)
        return 1.0f;
    return 1.0f - std::exp(-4.6f / float(seconds * sampleRate));
}

//==============================================================================
void Envelope::noteOn() { stage = attack; }

void Envelope::noteOff() {
    if (stage != idle)
        stage = release;
}

float Envelope::next(const ShapeSettings &shape, double sampleRate) {
    switch (stage) {
    case idle:
        break;
    case attack:
        if (shape.attack <= 0.0005f)
            level = 1.0f;
        else
            level += 1.0f / float(shape.attack * sampleRate);
        if (level >= 1.0f) {
            level = 1.0f;
            stage = decay;
        }
        break;
    case decay:
        level +=
            (shape.sustain - level) * approachCoeff(shape.decay, sampleRate);
        if (std::abs(level - shape.sustain) < 1.0e-4f)
            stage = sustain;
        break;
    case sustain:
        // follow the sustain knob smoothly
        level += (shape.sustain - level) * 0.002f;
        break;
    case release:
        level -= level * approachCoeff(shape.release, sampleRate);
        if (level < 1.0e-4f) {
            level = 0.0f;
            stage = idle;
        }
        break;
    }
    return level;
}

//==============================================================================
void Filter::setParams(float cutoff, float resonance, double sampleRate) {
    cutoff = juce::jlimit(20.0f, float(sampleRate * 0.45), cutoff);
    g = std::tan(juce::MathConstants<float>::pi * cutoff / float(sampleRate));
    k = 2.0f * (1.0f - 0.97f * juce::jlimit(0.0f, 1.0f, resonance));
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

float Filter::process(float x) {
    float v3 = x - ic2;
    float v1 = a1 * ic1 + a2 * v3;
    float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    return v2;
}

//==============================================================================
PolySynth::PolySynth(Factory factory) {
    voices.resize(maxVoices);
    for (size_t i = 0; i < voices.size(); i++) {
        voices[i].sound = factory();
        // spread the voices a little across the stereo field
        voices[i].pan =
            0.5f + ((i % 2) ? 0.12f : -0.12f) * float(1 + i / 2) / 4.0f;
    }
}

void PolySynth::prepare(double newSampleRate) {
    sampleRate = newSampleRate;
    allNotesOff();
    for (auto &v : voices) {
        v.amp.reset();
        v.filter.reset();
        v.note = -1;
    }
}

int PolySynth::getNumActiveVoices() const {
    int count = 0;
    for (auto &v : voices)
        if (v.amp.isActive())
            count++;
    return count;
}

PolySynth::Voice *PolySynth::findVoiceToUse(int note) {
    // the same note again: reuse its voice
    for (auto &v : voices)
        if (v.note == note && v.amp.isActive())
            return &v;

    int active = getNumActiveVoices();
    if (active < voiceLimit)
        for (auto &v : voices)
            if (!v.amp.isActive())
                return &v;

    // steal: the oldest released voice, else the oldest one
    Voice *oldest = nullptr;
    for (auto &v : voices)
        if (v.amp.isActive() && v.amp.isReleasing() &&
            (oldest == nullptr || v.age < oldest->age))
            oldest = &v;
    if (oldest != nullptr)
        return oldest;
    for (auto &v : voices)
        if (v.amp.isActive() && (oldest == nullptr || v.age < oldest->age))
            oldest = &v;
    return oldest != nullptr ? oldest : &voices[0];
}

void PolySynth::noteOn(int note, float velocity) {
    // starting sounds needs the parameters, so it happens in render()
    auto *v = findVoiceToUse(note);
    if (v->note != note || !v->amp.isActive())
        v->filter.reset();
    v->note = note;
    v->velocity = juce::jlimit(0.0f, 1.0f, velocity);
    v->age = ++counter;
    v->amp.noteOn();
    // mark as needing a start: negative velocity is never used otherwise
    v->velocity = -1.0f - v->velocity;
}

void PolySynth::noteOff(int note) {
    for (auto &v : voices)
        if (v.note == note && v.amp.isActive() && !v.amp.isReleasing()) {
            v.amp.noteOff();
            v.sound->stop();
        }
}

void PolySynth::allNotesOff() {
    for (auto &v : voices) {
        v.amp.noteOff();
        v.sound->stop();
    }
}

void PolySynth::render(float *left, float *right, int numSamples,
                       const float *p, const ShapeSettings &shape) {
    voiceLimit = juce::jlimit(1, maxVoices, shape.voices);
    for (int i = 0; i < numSamples; i++)
        left[i] = right[i] = 0.0f;

    for (auto &v : voices) {
        if (v.velocity < 0.0f) {
            v.velocity = -1.0f - v.velocity;
            float frequency = midiNoteToHz(float(v.note));
            v.sound->start(frequency, lastFrequency, v.velocity, sampleRate, p);
            lastFrequency = frequency;
        }
        if (!v.amp.isActive())
            continue;

        const float gain = 0.3f * (0.4f + 0.6f * v.velocity);
        const float leftGain = gain * (1.0f - (v.pan - 0.5f));
        const float rightGain = gain * (1.0f + (v.pan - 0.5f));

        // the filter follows the envelope; update it every 16 samples
        for (int start = 0; start < numSamples; start += 16) {
            int end = std::min(numSamples, start + 16);
            float open =
                std::pow(2.0f, shape.envAmount * 6.0f * v.amp.getLevel());
            v.filter.setParams(shape.cutoff * open, shape.resonance,
                               sampleRate);
            for (int i = start; i < end; i++) {
                float s = v.filter.process(v.sound->next(p));
                s *= v.amp.next(shape, sampleRate);
                left[i] += s * leftGain;
                right[i] += s * rightGain;
            }
            if (!v.amp.isActive()) {
                v.note = -1;
                break;
            }
        }
    }

    // remove DC, then limit gently so big chords don't clip
    const float r =
        1.0f - float(juce::MathConstants<double>::twoPi * 10.0 / sampleRate);
    for (int i = 0; i < numSamples; i++) {
        float l = left[i] - dcInL + r * dcOutL;
        dcInL = left[i];
        dcOutL = l;
        float rr = right[i] - dcInR + r * dcOutR;
        dcInR = right[i];
        dcOutR = rr;
        left[i] = std::tanh(l);
        right[i] = std::tanh(rr);
    }
}

//==============================================================================
// String
static constexpr int stringBufferSize = 8192;

float StringSound::Line::tick(float damping, float stiffness) {
    const int size = int(buffer.size());
    // the loop filter and allpass add a little delay; take it off the line
    float c = 0.6f * stiffness;
    float delay = length - (1.0f - damping) - (1.0f - c) / (1.0f + c);
    delay = juce::jlimit(2.0f, float(size - 2), delay);
    float readPos = float(write) - delay;
    while (readPos < 0)
        readPos += float(size);
    int i0 = int(readPos);
    float frac = readPos - float(i0);
    float y = buffer[size_t(i0)] +
              frac * (buffer[size_t((i0 + 1) % size)] - buffer[size_t(i0)]);

    float filtered = damping * y + (1.0f - damping) * last;
    last = y;
    float ap = c * filtered + allpassState;
    allpassState = filtered - c * ap;

    buffer[size_t(write)] = ap * feedback;
    write = (write + 1) % size;
    return y;
}

StringSound::StringSound() {
    // allocate up front: nothing is allocated while playing
    for (auto &line : lines)
        line.buffer.assign(size_t(stringBufferSize), 0.0f);
    excitation.assign(size_t(stringBufferSize), 0.0f);
}

void StringSound::start(float f, float, float velocity, double sr,
                        const float *p) {
    sampleRate = sr;
    frequency = juce::jlimit(25.0f, 4000.0f, f);
    const float decay = juce::jmax(0.05f, p[0]);
    const float brightness = juce::jlimit(0.0f, 1.0f, p[1]);
    const float pickPos = juce::jlimit(0.05f, 0.95f, p[2]);
    const float detune = juce::jlimit(0.0f, 1.0f, p[4]);
    const float pluckNoise = juce::jlimit(0.0f, 1.0f, p[6]);

    for (int s = 0; s < 2; s++) {
        auto &line = lines[s];
        std::fill(line.buffer.begin(), line.buffer.end(), 0.0f);
        float freq = frequency *
                     (s == 0 ? 1.0f : std::pow(2.0f, detune * 15.0f / 1200.0f));
        line.length =
            juce::jlimit(4.0f, float(stringBufferSize - 4), float(sr / freq));
        line.write = 0;
        line.last = 0.0f;
        line.allpassState = 0.0f;
        // lose 60 dB over the decay time
        line.feedback = std::pow(10.0f, -3.0f / (decay * freq));

        // the pluck: a burst of noise, softened by brightness, with the
        // pick position cutting out the harmonics that have a node there
        int len = int(line.length);
        float smooth = 0.0f, peak = 0.0f;
        float a = 0.05f + 0.9f * brightness;
        for (int i = 0; i < len; i++) {
            float n = random.nextFloat() * 2.0f - 1.0f;
            smooth += a * (n - smooth);
            excitation[size_t(i)] = smooth + pluckNoise * 0.5f * (n - smooth);
        }
        int pick = juce::jmax(1, int(pickPos * float(len)));
        for (int i = len - 1; i >= pick; i--)
            excitation[size_t(i)] -= excitation[size_t(i - pick)];
        for (int i = 0; i < len; i++)
            peak = juce::jmax(peak, std::abs(excitation[size_t(i)]));
        float scale = peak > 0 ? (0.5f + 0.5f * velocity) / peak : 0.0f;
        // the newest samples sit just before the write position
        for (int i = 0; i < len; i++)
            line.buffer[size_t(stringBufferSize - len + i)] =
                excitation[size_t(i)] * scale;
    }

    // body: two resonances, around 220 Hz and 900 Hz
    const float freqs[2] = {220.0f, 900.0f}, qs[2] = {2.0f, 3.0f};
    for (int i = 0; i < 2; i++) {
        float w = juce::MathConstants<float>::twoPi * freqs[i] / float(sr);
        float alpha = std::sin(w) / (2.0f * qs[i]);
        float a0 = 1.0f + alpha;
        bodyCoeffs[i][0] = alpha / a0;               // b0 (b2 = -b0)
        bodyCoeffs[i][1] = -2.0f * std::cos(w) / a0; // a1
        bodyCoeffs[i][2] = (1.0f - alpha) / a0;      // a2
        b1[i] = b2[i] = 0.0f;
    }
}

float StringSound::bodyFilter(int i, float x) {
    // transposed direct form II band-pass
    float y = bodyCoeffs[i][0] * x + b1[i];
    b1[i] = -bodyCoeffs[i][1] * y + b2[i];
    b2[i] = -bodyCoeffs[i][0] * x - bodyCoeffs[i][2] * y;
    return y;
}

float StringSound::next(const float *p) {
    const float brightness = juce::jlimit(0.0f, 1.0f, p[1]);
    const float body = juce::jlimit(0.0f, 1.0f, p[3]);
    const float detune = juce::jlimit(0.0f, 1.0f, p[4]);
    const float stiffness = juce::jlimit(0.0f, 1.0f, p[5]);
    const float level = juce::jlimit(0.0f, 1.0f, p[7]);
    const float damping = 0.5f + 0.48f * brightness;

    float s = lines[0].tick(damping, stiffness);
    float second = lines[1].tick(damping, stiffness);
    float mix = juce::jmin(1.0f, detune * 4.0f);
    s = (s + mix * second) / (1.0f + mix * 0.5f);

    float resonance = bodyFilter(0, s) + bodyFilter(1, s);
    s = s * (1.0f - 0.4f * body) + body * resonance * 1.5f;
    return s * level * 4.5f;
}

//==============================================================================
// FM
const std::vector<FmSound::Algorithm> &FmSound::getAlgorithms() {
    static const std::vector<Algorithm> algorithms = {
        {"stack", {{4, 3}, {3, 2}, {2, 1}}, {1}},
        {"two into one", {{4, 2}, {3, 2}, {2, 1}}, {1}},
        {"branch", {{4, 3}, {3, 1}, {2, 1}}, {1}},
        {"two pairs", {{4, 3}, {2, 1}}, {1, 3}},
        {"one to three", {{4, 1}, {4, 2}, {4, 3}}, {1, 2, 3}},
        {"pair + two", {{4, 3}}, {1, 2, 3}},
        {"stack + one", {{4, 3}, {3, 2}}, {1, 2}},
        {"organ", {}, {1, 2, 3, 4}},
    };
    return algorithms;
}

void FmSound::start(float f, float, float, double sr, const float *) {
    frequency = f;
    sampleRate = sr;
    for (int i = 0; i < 4; i++) {
        phases[i] = 0.0;
        outputs[i] = 0.0f;
    }
    feedbackState = 0.0f;
    modLevel = 1.0f;
}

float FmSound::next(const float *p) {
    const auto &algorithms = getAlgorithms();
    const auto &algorithm =
        algorithms[size_t(juce::jlimit(0, 7, juce::roundToInt(p[0])))];
    const float ratios[4] = {1.0f, p[1], p[4], p[5]};
    const float detuneCents[4] = {0.0f, 1.0f, -1.0f, 0.5f};
    const float amount = juce::jlimit(0.0f, 1.0f, p[2]);
    const float feedback = juce::jlimit(0.0f, 1.0f, p[3]);
    const float modDecay = juce::jmax(0.02f, p[6]);
    const float detune = juce::jlimit(0.0f, 1.0f, p[7]);

    // modulators start bright and settle down to 30%
    modLevel += (0.3f - modLevel) * approachCoeff(modDecay, sampleRate);
    const float index = amount * amount * 7.0f * modLevel;

    float newOutputs[4] = {};
    for (int op = 3; op >= 0; op--) {
        float mod = 0.0f;
        for (auto &link : algorithm.links)
            if (link.second == op + 1)
                mod += newOutputs[link.first - 1];
        mod *= index;
        if (op == 3)
            mod += feedback * feedback * 1.5f * feedbackState;

        newOutputs[op] = std::sin(
            float(phases[op]) * juce::MathConstants<float>::twoPi + mod);

        double cents = detune * 8.0 * detuneCents[op];
        double inc =
            frequency * ratios[op] * std::pow(2.0, cents / 1200.0) / sampleRate;
        phases[op] += inc;
        phases[op] -= std::floor(phases[op]);
    }
    feedbackState = 0.5f * (newOutputs[3] + outputs[3]);
    for (int i = 0; i < 4; i++)
        outputs[i] = newOutputs[i];

    float out = 0.0f;
    for (int c : algorithm.carriers)
        out += newOutputs[c - 1];
    return out / std::sqrt(float(algorithm.carriers.size()));
}

//==============================================================================
// Pulse
float PulseSound::polyBlep(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return float(t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return float(t * t + t + t + 1.0);
    }
    return 0.0f;
}

float PulseSound::pulse(double phase, double dt, float width) {
    float naive = phase < width ? 1.0f : -1.0f;
    naive += polyBlep(phase, dt);
    double falling = phase + 1.0 - width;
    falling -= std::floor(falling);
    naive -= polyBlep(falling, dt);
    // remove the offset an uneven pulse has
    return naive - (2.0f * width - 1.0f);
}

void PulseSound::start(float f, float previous, float, double sr,
                       const float *p) {
    sampleRate = sr;
    targetFrequency = f;
    frequency = (p[4] > 0.001f && previous > 0.0f) ? previous : f;
    phase1 = phase2 = subPhase = 0.0;
    lfoPhase = random.nextDouble();
}

float PulseSound::next(const float *p) {
    const float width = juce::jlimit(0.05f, 0.95f, p[0]);
    const float depth = juce::jlimit(0.0f, 1.0f, p[1]);
    const float rate = juce::jmax(0.01f, p[2]);
    const float sub = juce::jlimit(0.0f, 1.0f, p[3]);
    const float glide = juce::jmax(0.0f, p[4]);
    const float detune = juce::jlimit(0.0f, 1.0f, p[5]);
    const float noise = juce::jlimit(0.0f, 1.0f, p[6]);
    const float drive = juce::jlimit(0.0f, 1.0f, p[7]);

    if (glide > 0.001f)
        frequency +=
            (targetFrequency - frequency) * approachCoeff(glide, sampleRate);
    else
        frequency = targetFrequency;

    lfoPhase += rate / sampleRate;
    lfoPhase -= std::floor(lfoPhase);
    float w =
        juce::jlimit(0.05f, 0.95f,
                     width + depth * 0.45f *
                                 std::sin(float(lfoPhase) *
                                          juce::MathConstants<float>::twoPi));

    double dt1 = frequency / sampleRate;
    double dt2 = dt1 * std::pow(2.0, detune * 20.0 / 1200.0);
    double dtSub = dt1 * 0.5;

    float o1 = pulse(phase1, dt1, w);
    float mix2 = juce::jmin(1.0f, detune * 3.0f) * 0.8f;
    float o2 = mix2 > 0.0f ? pulse(phase2, dt2, w) : 0.0f;
    float s = pulse(subPhase, dtSub, 0.5f);

    phase1 += dt1;
    phase1 -= std::floor(phase1);
    phase2 += dt2;
    phase2 -= std::floor(phase2);
    subPhase += dtSub;
    subPhase -= std::floor(subPhase);

    float x = (o1 + mix2 * o2) * 0.5f + s * sub * 0.6f +
              (random.nextFloat() * 2.0f - 1.0f) * noise * 0.4f;
    if (drive > 0.0f)
        x = std::tanh(x * (1.0f + 4.0f * drive)) / (1.0f + 1.5f * drive);
    return x * 0.6f;
}

} // namespace engines
} // namespace internal_plugins
