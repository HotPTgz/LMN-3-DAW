#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace internal_plugins;
using namespace internal_plugins::engines;

namespace {
constexpr double sampleRate = 48000.0;

struct Rendered {
    std::vector<float> left, right;
};

// The engines' default sound settings
const float stringDefaults[8] = {2.4f, 0.7f, 0.2f, 0.5f,
                                 0.0f, 0.0f, 0.5f, 0.8f};
const float fmDefaults[8] = {0.0f, 2.0f, 0.6f, 0.2f, 3.0f, 1.0f, 0.8f, 0.0f};
const float pulseDefaults[8] = {0.3f, 0.4f, 0.8f, 0.5f, 0.0f, 0.0f, 0.0f, 0.2f};

ShapeSettings openShape() {
    ShapeSettings s;
    s.attack = 0.005f;
    s.decay = 0.2f;
    s.sustain = 1.0f;
    s.release = 0.2f;
    s.cutoff = 20000.0f;
    s.resonance = 0.0f;
    s.envAmount = 0.0f;
    s.voices = 6;
    return s;
}

void render(PolySynth &synth, Rendered &out, double seconds, const float *p,
            const ShapeSettings &shape) {
    int n = int(seconds * sampleRate);
    size_t start = out.left.size();
    out.left.resize(start + size_t(n));
    out.right.resize(start + size_t(n));
    // in blocks, like the audio thread
    for (int pos = 0; pos < n; pos += 256) {
        int len = std::min(256, n - pos);
        synth.render(out.left.data() + start + size_t(pos),
                     out.right.data() + start + size_t(pos), len, p, shape);
    }
}

float rms(const std::vector<float> &x, size_t from, size_t to) {
    double sum = 0.0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; i++)
        sum += double(x[i]) * x[i];
    return to > from ? float(std::sqrt(sum / double(to - from))) : 0.0f;
}

// Fundamental frequency by autocorrelation, searching 60 Hz - 2 kHz
float pitchOf(const std::vector<float> &x, size_t from, size_t length) {
    int minLag = int(sampleRate / 2000.0), maxLag = int(sampleRate / 60.0);
    std::vector<double> r(size_t(maxLag + 2), 0.0);
    for (int lag = minLag; lag <= maxLag + 1; lag++) {
        double sum = 0.0;
        for (size_t i = from; i < from + length; i++)
            sum += double(x[i]) * x[i + size_t(lag)];
        r[size_t(lag)] = sum;
    }
    double best = 0.0;
    for (int lag = minLag; lag <= maxLag; lag++)
        best = std::max(best, r[size_t(lag)]);
    // the first lag that gets close to the best (avoids picking an octave
    // below)
    for (int lag = minLag + 1; lag <= maxLag; lag++)
        if (r[size_t(lag)] > 0.9 * best &&
            r[size_t(lag)] >= r[size_t(lag - 1)] &&
            r[size_t(lag)] >= r[size_t(lag + 1)]) {
            // parabolic interpolation
            double a = r[size_t(lag - 1)], b = r[size_t(lag)],
                   c = r[size_t(lag + 1)];
            double offset = 0.5 * (a - c) / (a - 2 * b + c);
            return float(sampleRate / (lag + offset));
        }
    return 0.0f;
}

bool allFinite(const Rendered &r) {
    for (size_t i = 0; i < r.left.size(); i++)
        if (!std::isfinite(r.left[i]) || !std::isfinite(r.right[i]) ||
            std::abs(r.left[i]) > 1.0f || std::abs(r.right[i]) > 1.0f)
            return false;
    return true;
}

std::unique_ptr<PolySynth> makeSynth(int engine) {
    auto synth =
        std::make_unique<PolySynth>([engine]() -> std::unique_ptr<VoiceSound> {
            if (engine == 0)
                return std::make_unique<StringSound>();
            if (engine == 1)
                return std::make_unique<FmSound>();
            return std::make_unique<PulseSound>();
        });
    synth->prepare(sampleRate);
    return synth;
}

const float *defaultsFor(int engine) {
    return engine == 0 ? stringDefaults
                       : (engine == 1 ? fmDefaults : pulseDefaults);
}
} // namespace

//==============================================================================
class EngineSoundTest : public ::testing::TestWithParam<int> {};

TEST_P(EngineSoundTest, playsInTune) {
    float p[8];
    std::copy(defaultsFor(GetParam()), defaultsFor(GetParam()) + 8, p);
    if (GetParam() == 2)
        p[3] = 0.0f; // the sub (an octave down) would halve the pitch
    for (int note : {45, 57, 69}) {
        auto synth = makeSynth(GetParam());
        Rendered out;
        synth->noteOn(note, 0.8f);
        render(*synth, out, 0.6, p, openShape());
        float expected = midiNoteToHz(float(note));
        float measured = pitchOf(out.left, size_t(0.1 * sampleRate), 8192);
        EXPECT_NEAR(measured, expected, expected * 0.01f)
            << "engine " << GetParam() << " note " << note;
    }
}

TEST_P(EngineSoundTest, staysInRangeAndStopsAfterRelease) {
    auto synth = makeSynth(GetParam());
    Rendered out;
    for (int note : {48, 52, 55, 60, 64, 67, 72, 76})
        synth->noteOn(note, 1.0f);
    render(*synth, out, 0.5, defaultsFor(GetParam()), openShape());
    EXPECT_GT(rms(out.left, 0, out.left.size()), 0.01f);
    for (int note : {48, 52, 55, 60, 64, 67, 72, 76})
        synth->noteOff(note);
    render(*synth, out, 1.0, defaultsFor(GetParam()), openShape());
    EXPECT_TRUE(allFinite(out));
    EXPECT_EQ(synth->getNumActiveVoices(), 0);
    size_t end = out.left.size();
    EXPECT_LT(rms(out.left, end - 4800, end), 1.0e-4f);
}

TEST_P(EngineSoundTest, voiceLimitIsRespected) {
    auto synth = makeSynth(GetParam());
    auto shape = openShape();
    shape.voices = 2;
    Rendered out;
    for (int note : {60, 64, 67, 71}) {
        synth->noteOn(note, 0.8f);
        render(*synth, out, 0.05, defaultsFor(GetParam()), shape);
    }
    EXPECT_LE(synth->getNumActiveVoices(), 2);
}

TEST_P(EngineSoundTest, sustainPedalHoldsReleasedKeys) {
    auto synth = makeSynth(GetParam());
    Rendered out;
    synth->noteOn(60, 0.9f);
    render(*synth, out, 0.2, defaultsFor(GetParam()), openShape());
    synth->setSustain(true);
    synth->noteOff(60);
    render(*synth, out, 1.0, defaultsFor(GetParam()), openShape());
    EXPECT_GT(synth->getNumActiveVoices(), 0) << "engine " << GetParam();
    synth->setSustain(false);
    render(*synth, out, 1.0, defaultsFor(GetParam()), openShape());
    EXPECT_EQ(synth->getNumActiveVoices(), 0) << "engine " << GetParam();
}

INSTANTIATE_TEST_SUITE_P(Engines, EngineSoundTest, ::testing::Values(0, 1, 2));

TEST(EngineSoundTest, everyFmAlgorithmMakesSound) {
    for (int algorithm = 0; algorithm < 8; algorithm++) {
        float p[8];
        std::copy(fmDefaults, fmDefaults + 8, p);
        p[0] = float(algorithm);
        auto synth = makeSynth(1);
        Rendered out;
        synth->noteOn(60, 0.8f);
        render(*synth, out, 0.3, p, openShape());
        EXPECT_GT(rms(out.left, 0, out.left.size()), 0.02f) << algorithm;
        EXPECT_TRUE(allFinite(out));
    }
}

TEST(EngineSoundTest, longerStringDecayRingsLonger) {
    auto energyAfter = [](float decay) {
        float p[8];
        std::copy(stringDefaults, stringDefaults + 8, p);
        p[0] = decay;
        auto synth = makeSynth(0);
        Rendered out;
        synth->noteOn(57, 0.8f);
        render(*synth, out, 2.0, p, openShape());
        return rms(out.left, size_t(1.5 * sampleRate),
                   size_t(2.0 * sampleRate));
    };
    EXPECT_GT(energyAfter(4.0f), energyAfter(1.0f) * 5.0f);
}

TEST(EngineSoundTest, envelopeAttackAndSustain) {
    Envelope env;
    ShapeSettings s = openShape();
    s.attack = 0.1f;
    s.sustain = 0.5f;
    env.noteOn();
    float level = 0.0f;
    for (int i = 0; i < int(0.05 * sampleRate); i++)
        level = env.next(s, sampleRate);
    EXPECT_NEAR(level, 0.5f, 0.02f); // halfway up a 100 ms attack
    for (int i = 0; i < int(1.0 * sampleRate); i++)
        level = env.next(s, sampleRate);
    EXPECT_NEAR(level, 0.5f, 0.01f); // settled at the sustain level
    env.noteOff();
    for (int i = 0; i < int(0.5 * sampleRate); i++)
        level = env.next(s, sampleRate);
    EXPECT_FALSE(env.isActive());
}

TEST(EngineSoundTest, pulseGlidesBetweenNotes) {
    float p[8];
    std::copy(pulseDefaults, pulseDefaults + 8, p);
    p[1] = 0.0f; // steady width
    p[3] = 0.0f; // no sub
    p[4] = 0.4f; // glide
    auto synth = makeSynth(2);
    auto shape = openShape();
    shape.voices = 1;
    Rendered out;
    synth->noteOn(45, 0.8f); // 110 Hz
    render(*synth, out, 0.5, p, shape);
    synth->noteOn(57, 0.8f); // 220 Hz
    render(*synth, out, 1.0, p, shape);
    float early = pitchOf(out.left, size_t(0.52 * sampleRate), 2048);
    float late = pitchOf(out.left, size_t(1.3 * sampleRate), 4096);
    EXPECT_GT(early, 112.0f);
    EXPECT_LT(early, 200.0f);
    EXPECT_NEAR(late, 220.0f, 3.0f);
}

//==============================================================================
// The plugins themselves: parameters and saving
class EnginePluginTest : public ::testing::Test {
  protected:
    EnginePluginTest() {
        engine.getPluginManager().createBuiltInType<StringEnginePlugin>();
        engine.getPluginManager().createBuiltInType<FmEnginePlugin>();
        engine.getPluginManager().createBuiltInType<PulseEnginePlugin>();
        edit = tracktion::Edit::createSingleTrackEdit(engine);
    }

    tracktion::Engine engine{"ENGINE"};
    std::unique_ptr<tracktion::Edit> edit;
};

TEST_F(EnginePluginTest, eachEngineHasTwoPagesOfControls) {
    for (auto type :
         {StringEnginePlugin::xmlTypeName, FmEnginePlugin::xmlTypeName,
          PulseEnginePlugin::xmlTypeName}) {
        auto plugin = edit->getPluginCache().createNewPlugin(type, {});
        auto enginePlugin = dynamic_cast<EnginePlugin *>(plugin.get());
        ASSERT_NE(enginePlugin, nullptr) << type;
        EXPECT_TRUE(enginePlugin->isSynth());
        EXPECT_EQ(enginePlugin->getControls().size(), 16) << type;
    }
}

TEST_F(EnginePluginTest, settingsAreSavedWithTheProject) {
    auto plugin =
        edit->getPluginCache().createNewPlugin(FmEnginePlugin::xmlTypeName, {});
    auto fm = dynamic_cast<EnginePlugin *>(plugin.get());
    ASSERT_NE(fm, nullptr);
    fm->getControls()[0].parameter->setParameter(4.0f, juce::sendNotification);
    fm->getControls()[1].parameter->setParameter(7.5f, juce::sendNotification);

    // a fresh FM loading the saved settings
    auto other =
        edit->getPluginCache().createNewPlugin(FmEnginePlugin::xmlTypeName, {});
    auto copy = dynamic_cast<EnginePlugin *>(other.get());
    ASSERT_NE(copy, nullptr);
    copy->restorePluginStateFromValueTree(plugin->state);
    EXPECT_FLOAT_EQ(copy->getSoundValue(0), 4.0f);
    EXPECT_FLOAT_EQ(copy->getSoundValue(1), 7.5f);
}

} // namespace AppViewModelsTests
