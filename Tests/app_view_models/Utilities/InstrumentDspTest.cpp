#include <app_view_models/app_view_models.h>
#include <gtest/gtest.h>

namespace AppViewModelsTests {

using namespace internal_plugins::instruments;

namespace {
const double sr = 48000.0;

template <typename Sound>
std::vector<float> play(Sound &sound, float hz, const float *p, int n) {
    sound.start(hz, hz, 1.0f, sr, p);
    std::vector<float> out(static_cast<size_t>(n));
    for (auto &x : out)
        x = sound.next(p);
    return out;
}

float rms(const std::vector<float> &v, int from, int to) {
    double sum = 0;
    for (int i = from; i < to; i++)
        sum += double(v[size_t(i)]) * v[size_t(i)];
    return float(std::sqrt(sum / juce::jmax(1, to - from)));
}

int upwardCrossings(const std::vector<float> &v) {
    int count = 0;
    for (size_t i = 1; i < v.size(); i++)
        count += v[i - 1] < 0.0f && v[i] >= 0.0f;
    return count;
}

bool sane(const std::vector<float> &v, float limit = 2.0f) {
    for (auto x : v)
        if (!std::isfinite(x) || std::abs(x) > limit)
            return false;
    return true;
}
} // namespace

//==============================================================================
TEST(WavetableTest, tablesArePeriodicAndBounded) {
    auto &tables = Wavetables::get();
    EXPECT_EQ(Wavetables::getNames().size(), Wavetables::numTables);
    for (int t = 0; t < Wavetables::numTables; t++)
        for (float pos : {0.0f, 0.37f, 1.0f})
            for (double ph : {0.1, 0.5, 0.9}) {
                float a = tables.sample(t, pos, ph, 220.0f, sr);
                float b = tables.sample(t, pos, ph + 1.0, 220.0f, sr);
                EXPECT_NEAR(a, b, 1.0e-5f);
                EXPECT_LE(std::abs(a), 1.0f);
            }
}

TEST(WavetableTest, playsTheNotesPitch) {
    WavetableSound sound;
    // sine to saw at position 0 is a sine; filter wide open, no detune
    float p[8] = {1, 0, 0, 1, 0, 0.4f, 0, 1};
    auto out = play(sound, 440.0f, p, int(sr));
    EXPECT_NEAR(upwardCrossings(out), 440, 3);
    EXPECT_TRUE(sane(out));
}

TEST(WavetableTest, warpKeepsTheEnds) {
    for (float warp : {0.0f, 0.5f, 1.0f}) {
        EXPECT_NEAR(WavetableSound::warpPhase(0.0, warp), 0.0f, 1.0e-6f);
        EXPECT_NEAR(WavetableSound::warpPhase(0.999999, warp), 1.0f, 1.0e-4f);
    }
    EXPECT_NEAR(WavetableSound::warpPhase(0.3, 0.0f), 0.3f, 1.0e-6f);
    EXPECT_GT(WavetableSound::warpPhase(0.1, 1.0f), 0.1f); // squeezed
}

//==============================================================================
TEST(MalletsTest, everyMaterialRingsAndFades) {
    for (int m = 0; m < 4; m++) {
        MalletSound sound;
        float p[8] = {float(m), 0.6f, 0.5f, 0.5f, 0.4f, 0.3f, 0.3f, 0.8f};
        auto out = play(sound, 330.0f, p, int(sr * 3));
        EXPECT_TRUE(sane(out)) << m;
        float start = rms(out, 0, int(sr * 0.1));
        float end = rms(out, int(sr * 2.9), int(sr * 3));
        EXPECT_GT(start, 0.01f) << m;
        EXPECT_LT(end, start * 0.1f) << m;
    }
}

TEST(MalletsTest, harderMalletIsBrighter) {
    // a harder mallet: more of the upper modes, so more zero crossings
    MalletSound soft, hard;
    float ps[8] = {0, 0.0f, 1.0f, 0.5f, 0.0f, 0.3f, 0.0f, 1.0f};
    float ph[8] = {0, 1.0f, 1.0f, 0.5f, 0.0f, 0.3f, 0.0f, 1.0f};
    auto a = play(soft, 220.0f, ps, 4800);
    auto b = play(hard, 220.0f, ph, 4800);
    EXPECT_GT(upwardCrossings(b), upwardCrossings(a));
}

//==============================================================================
TEST(GranularTest, builtInVoiceMakesSound) {
    auto slot = std::make_shared<GrainSampleSlot>();
    EXPECT_GT(slot->get()->audio.getNumSamples(), 1000);
    EXPECT_EQ(slot->get()->overview.size(), 96u);
    GrainSound sound(slot);
    float p[8] = {0.4f, 0.08f, 24.0f, 0.3f, 0.0f, 0.1f, 0.0f, 0.8f};
    auto out = play(sound, 261.63f, p, int(sr));
    EXPECT_TRUE(sane(out, 4.0f));
    EXPECT_GT(rms(out, int(sr * 0.2), int(sr)), 0.01f);
}

TEST(GranularTest, grainsOverlapAsExpected) {
    auto slot = std::make_shared<GrainSampleSlot>();
    GrainSound sound(slot);
    // 10 grains a second of 50 ms: rarely more than one at a time
    float p[8] = {0.5f, 0.05f, 10.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f};
    sound.start(261.63f, 261.63f, 1.0f, sr, p);
    int most = 0;
    for (int i = 0; i < int(sr); i++) {
        sound.next(p);
        most = juce::jmax(most, sound.getNumActiveGrains());
    }
    EXPECT_GE(most, 1);
    EXPECT_LE(most, 2);
}

TEST(GranularTest, slotSwapsSamples) {
    GrainSampleSlot slot;
    juce::AudioBuffer<float> b(1, 4800);
    b.clear();
    b.setSample(0, 100, 1.0f);
    slot.set(GrainSample::fromBuffer(std::move(b), 48000.0, "click"));
    EXPECT_EQ(slot.get()->name, "click");
    EXPECT_FLOAT_EQ(*std::max_element(slot.get()->overview.begin(),
                                      slot.get()->overview.end()),
                    1.0f);
}

//==============================================================================
TEST(DrumKitTest, everyPadSoundsAndStops) {
    for (int pad = 0; pad < DrumKit::numPads; pad++) {
        auto wave = DrumKit::preview(pad, DrumKit::defaults(pad), sr, int(sr * 4));
        EXPECT_TRUE(sane(wave)) << DrumKit::getPadNames()[pad];
        EXPECT_GT(rms(wave, 0, int(sr * 0.05)), 0.01f)
            << DrumKit::getPadNames()[pad];
        // long after the hit: silence
        EXPECT_LT(rms(wave, int(sr * 3.9), int(sr * 4)), 1.0e-4f)
            << DrumKit::getPadNames()[pad];
    }
}

TEST(DrumKitTest, closedHatChokesOpenHat) {
    auto openOnly = [](bool choke) {
        DrumKit kit;
        kit.prepare(sr);
        std::array<DrumKit::PadParams, DrumKit::numPads> all;
        for (int i = 0; i < DrumKit::numPads; i++)
            all[size_t(i)] = DrumKit::defaults(i);
        all[DrumKit::hat][2] = 0.0f; // a very short closed hat
        kit.trigger(DrumKit::openHat, 1.0f, all[DrumKit::openHat]);
        std::vector<float> out(static_cast<size_t>(sr * 0.5), 0.0f);
        kit.render(out.data(), nullptr, 2400, all);
        if (choke)
            kit.trigger(DrumKit::hat, 1.0f, all[DrumKit::hat]);
        kit.render(out.data() + 2400, nullptr, int(out.size()) - 2400, all);
        return rms(out, int(sr * 0.2), int(sr * 0.5));
    };
    EXPECT_LT(openOnly(true), openOnly(false) * 0.1f);
}

TEST(DrumKitTest, keysAndText) {
    EXPECT_EQ(DrumKit::padForNote(53), 0);
    EXPECT_EQ(DrumKit::padForNote(60), 7);
    EXPECT_EQ(DrumKit::padForNote(61), 0);
    EXPECT_EQ(DrumKit::padForNote(52), 7);
    EXPECT_EQ(DrumKit::describe(DrumKit::kick, 0, 0.0f), "30 Hz");
    EXPECT_EQ(DrumKit::describe(DrumKit::kick, 1, 0.7f), "70%");
}

} // namespace AppViewModelsTests
