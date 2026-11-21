#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace internal_plugins::fx;

namespace {
const double sr = 48000.0;

std::vector<float> sine(float hz, int n, float amp = 0.5f) {
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; i++)
        v[size_t(i)] = amp * std::sin(2.0f * juce::MathConstants<float>::pi *
                                      hz * float(i) / float(sr));
    return v;
}

float rms(const std::vector<float> &v, int from = 0) {
    double sum = 0;
    for (size_t i = size_t(from); i < v.size(); i++)
        sum += double(v[i]) * v[i];
    return float(std::sqrt(sum / double(v.size() - size_t(from))));
}

bool allFinite(const std::vector<float> &v, float limit) {
    for (auto x : v)
        if (!std::isfinite(x) || std::abs(x) > limit)
            return false;
    return true;
}
} // namespace

//==============================================================================
TEST(LofiTest, crushQuantises) {
    EXPECT_FLOAT_EQ(Lofi::crush(0.3f, 2.0f), 0.5f); // steps of 0.5
    EXPECT_FLOAT_EQ(Lofi::crush(0.2f, 2.0f), 0.0f);
    EXPECT_FLOAT_EQ(Lofi::crush(0.3f, 3.0f), 0.25f);
    EXPECT_FLOAT_EQ(Lofi::crush(2.0f, 8.0f), 1.0f);
    EXPECT_NEAR(Lofi::crush(0.3f, 16.0f), 0.3f, 1.0e-4f);
}

TEST(LofiTest, lowerRateHoldsSamples) {
    Lofi lofi;
    lofi.prepare(sr);
    Lofi::Settings s;
    s.bits = 16;
    s.rateHz = float(sr / 4);
    auto l = sine(440, 64), r = l;
    lofi.process(l.data(), r.data(), 64, s);
    int changes = 0;
    for (size_t i = 1; i < l.size(); i++)
        changes += l[i] != l[i - 1];
    EXPECT_LE(changes, 17);
    EXPECT_GE(changes, 14);
}

TEST(LofiTest, mixZeroIsDry) {
    Lofi lofi;
    lofi.prepare(sr);
    Lofi::Settings s;
    s.bits = 2;
    s.mix = 0.0f;
    auto l = sine(440, 256), r = l, dry = l;
    lofi.process(l.data(), r.data(), 256, s);
    for (size_t i = 0; i < l.size(); i++)
        EXPECT_FLOAT_EQ(l[i], dry[i]);
}

//==============================================================================
TEST(DriveTest, shapesStayBounded) {
    for (int type = 0; type < 4; type++)
        for (float x = -20.0f; x <= 20.0f; x += 0.37f)
            EXPECT_LE(std::abs(Drive::shape(x, type)), 1.0f + 1.0e-6f);
}

TEST(DriveTest, loudInputStaysSane) {
    Drive drive;
    drive.prepare(sr);
    Drive::Settings s;
    s.driveDb = 36;
    for (int type = 0; type < 4; type++) {
        s.type = type;
        auto l = sine(220, 4800, 1.0f), r = l;
        drive.process(l.data(), r.data(), 4800, s);
        EXPECT_TRUE(allFinite(l, 1.01f));
        EXPECT_GT(rms(l, 2400), 0.01f);
    }
}

//==============================================================================
TEST(ResonantFilterTest, lowPassCutsHighs) {
    ResonantFilter f;
    f.prepare(sr);
    ResonantFilter::Settings s;
    s.cutoffHz = 300;
    s.resonance = 0.0f;
    s.envelope = 0.0f;
    BlockTime time;
    time.sampleRate = sr;
    auto high = sine(8000, 9600), hr = high;
    f.process(high.data(), hr.data(), 9600, time, s);
    f.prepare(sr);
    auto low = sine(100, 9600), lr = low;
    f.process(low.data(), lr.data(), 9600, time, s);
    EXPECT_LT(rms(high, 4800), 0.01f);
    EXPECT_GT(rms(low, 4800), 0.25f);
}

// Fully open with no resonance, a loud sound comes through untouched (it
// used to be squashed and distorted on the way)
TEST(ResonantFilterTest, openFilterLeavesLoudSoundAlone) {
    ResonantFilter f;
    f.prepare(sr);
    ResonantFilter::Settings s;
    s.cutoffHz = 20000;
    s.resonance = 0.0f;
    s.envelope = 0.0f;
    s.type = ResonantFilter::lowPass12;
    BlockTime time;
    time.sampleRate = sr;
    auto in = sine(200, 9600);
    for (auto &x : in)
        x *= 0.7f / 0.5f; // a peak of 0.7
    auto l = in, r = in;
    f.process(l.data(), r.data(), 9600, time, s);
    EXPECT_NEAR(rms(l, 4800), rms(in, 4800), rms(in, 4800) * 0.03f);
}

TEST(ResonantFilterTest, highPassCutsLows) {
    ResonantFilter f;
    f.prepare(sr);
    ResonantFilter::Settings s;
    s.type = ResonantFilter::highPass;
    s.cutoffHz = 3000;
    s.resonance = 0.0f;
    s.envelope = 0.0f;
    BlockTime time;
    time.sampleRate = sr;
    auto low = sine(100, 9600), lr = low;
    f.process(low.data(), lr.data(), 9600, time, s);
    EXPECT_LT(rms(low, 4800), 0.01f);
}

TEST(ResonantFilterTest, fullResonanceIsStable) {
    ResonantFilter f;
    f.prepare(sr);
    ResonantFilter::Settings s;
    s.resonance = 1.0f;
    s.envelope = 1.0f;
    s.lfo = 1.0f;
    s.lfoBeats = 0.25;
    BlockTime time;
    time.sampleRate = sr;
    auto l = sine(1200, 48000, 1.0f), r = l;
    for (int start = 0; start < 48000; start += 480) {
        f.process(l.data() + start, r.data() + start, 480, time, s);
        time.startBeat += 480 / sr * time.beatsPerSecond;
    }
    EXPECT_TRUE(allFinite(l, 1.0f));
}

TEST(ResonantFilterTest, envelopeOpensTheFilter) {
    ResonantFilter f;
    f.prepare(sr);
    ResonantFilter::Settings s;
    s.cutoffHz = 300;
    s.envelope = 1.0f;
    BlockTime time;
    time.sampleRate = sr;
    auto l = sine(440, 4800, 0.8f), r = l;
    f.process(l.data(), r.data(), 4800, time, s);
    EXPECT_GT(f.getCurrentCutoff(), 1200.0f);
}

//==============================================================================
TEST(DuckerTest, gainCurve) {
    Ducker::Settings s;
    s.depth = 0.7f;
    s.releaseSeconds = 0.2f;
    for (int shape = 0; shape < 3; shape++) {
        s.shape = shape;
        EXPECT_NEAR(Ducker::gainAt(0.0f, s), 1.0f, 1.0e-6f);
        EXPECT_NEAR(Ducker::gainAt(0.004f, s), 0.3f, 1.0e-4f);
        EXPECT_NEAR(Ducker::gainAt(0.3f, s), 1.0f, 1.0e-6f);
    }
}

TEST(DuckerTest, ducksOnEveryBeat) {
    Ducker d;
    d.prepare(sr);
    Ducker::Settings s;
    BlockTime time;
    time.sampleRate = sr;
    time.startBeat = 0.25; // starts between beats
    std::vector<float> l(480, 0.5f), r(480, 0.5f);
    for (int b = 0; b < 100; b++) { // 1 second: beats 0.25 to 2.25
        std::fill(l.begin(), l.end(), 0.5f);
        d.process(l.data(), r.data(), nullptr, nullptr, 480, time, s);
        time.startBeat += 480 / sr * time.beatsPerSecond;
    }
    EXPECT_EQ(d.getHitCount(), 2); // beats 1 and 2
}

TEST(DuckerTest, followsSidechainHits) {
    Ducker d;
    d.prepare(sr);
    Ducker::Settings s;
    BlockTime time;
    time.sampleRate = sr;
    const int n = 48000;
    std::vector<float> l(size_t(n), 0.5f), r = l, sc(size_t(n), 0.01f);
    for (int hit : {1000, 13000, 25000})
        for (int i = 0; i < 200; i++)
            sc[size_t(hit + i)] = 0.9f;
    d.process(l.data(), r.data(), sc.data(), nullptr, n, time, s);
    EXPECT_EQ(d.getHitCount(), 3);
    EXPECT_LT(l[1000 + 480], 0.25f); // ducked 10 ms after the hit
    EXPECT_NEAR(l[900], 0.5f, 1.0e-4f);
}

//==============================================================================
namespace {
// signal: each sample is its own index (scaled), so we can tell where
// repeated samples came from
std::vector<float> ramp(int from, int n) {
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; i++)
        v[size_t(i)] = float(from + i) * 1.0e-5f;
    return v;
}
} // namespace

TEST(BeatRepeatTest, repeatsTheLastSliceFromTheNextBoundary) {
    BeatRepeat br;
    br.prepare(sr);
    BeatRepeat::Settings s;
    s.lengthBeats = 0.5; // 0.25 s = 12000 samples at 120 bpm
    s.decay = 0.0f;
    BlockTime time;
    time.sampleRate = sr;
    const int block = 480;
    std::vector<float> out;
    int pos = 0;
    for (int b = 0; b < 100; b++) {
        s.active = pos >= 18000; // pressed mid-slice (slices: 0, 12000, 24000)
        auto l = ramp(pos, block), r = l;
        br.process(l.data(), r.data(), block, time, s);
        out.insert(out.end(), l.begin(), l.end());
        pos += block;
        time.startBeat += block / sr * time.beatsPerSecond;
    }
    // before the boundary at 24000: dry
    EXPECT_NEAR(out[20000], 20000 * 1.0e-5f, 1.0e-6f);
    // well after it: the slice 12000-24000 again, then again
    EXPECT_NEAR(out[24000 + 6000], (12000 + 6000) * 1.0e-5f, 1.0e-6f);
    EXPECT_NEAR(out[36000 + 6000], (12000 + 6000) * 1.0e-5f, 1.0e-6f);
}

TEST(BeatRepeatTest, stopsAfterTheRepeatsAndOnRelease) {
    BeatRepeat br;
    br.prepare(sr);
    BeatRepeat::Settings s;
    s.lengthBeats = 0.5;
    s.repeats = 2;
    s.decay = 0.0f;
    BlockTime time;
    time.sampleRate = sr;
    const int block = 480;
    std::vector<float> out;
    int pos = 0;
    for (int b = 0; b < 200; b++) {
        s.active = pos >= 6000 && pos < 80000;
        auto l = ramp(pos, block), r = l;
        br.process(l.data(), r.data(), block, time, s);
        out.insert(out.end(), l.begin(), l.end());
        pos += block;
        time.startBeat += block / sr * time.beatsPerSecond;
    }
    // repeats at 12000-24000 and 24000-36000, then dry again
    EXPECT_NEAR(out[18000], 6000 * 1.0e-5f, 1.0e-6f);
    EXPECT_NEAR(out[30000], 6000 * 1.0e-5f, 1.0e-6f);
    EXPECT_NEAR(out[42000], 42000 * 1.0e-5f, 1.0e-6f);
    EXPECT_NEAR(out[90000], 90000 * 1.0e-5f, 1.0e-6f);
}

//==============================================================================
TEST(TapeTest, silenceStaysQuietAndLoudStaysSane) {
    Tape tape;
    tape.prepare(sr);
    Tape::Settings s;
    s.hiss = 0.0f;
    s.age = 1.0f;
    std::vector<float> l(9600, 0.0f), r = l;
    tape.process(l.data(), r.data(), 9600, s);
    EXPECT_LT(rms(l), 1.0e-6f);
    auto a = sine(440, 48000, 1.0f), b = a;
    s.saturate = 1.0f;
    s.wow = s.flutter = 1.0f;
    tape.process(a.data(), b.data(), 48000, s);
    EXPECT_TRUE(allFinite(a, 1.01f));
    EXPECT_GT(rms(a, 24000), 0.1f);
}

} // namespace AppViewModelsTests
