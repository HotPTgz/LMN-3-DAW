// Scenes for the newer MIDI FX, effects, instruments and sequencers
#include "PlayfulScenes.h"

using namespace Playful;
using juce::AffineTransform;
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::String;

namespace PlayfulScenes {

static float fracOf(double x) { return float(x - std::floor(x)); }
static const double kSecondsPerBeat = 0.5; // the pictures run at 120 bpm

// A small two-octave keyboard from C, with some keys lit
static void litKeyboard(Graphics &g, float x, float y, float w, float h,
                        int firstNote, const std::function<Colour(int)> &lit) {
    float ww = w / 14.0f;
    keyboard(g, x, y, ww, h, ww * 0.6f, h * 0.6f, firstNote, 24, lit);
}

//==============================================================================
// Note Repeat: the held key fires a row of bars, getting louder or quieter
class NoteRepeatScene : public PlayfulScene {
  public:
    NoteRepeatScene(Getter r, TextGetter rt, Getter rp, Getter gt, Getter sw)
        : rateBeats(r), rateText(rt), ramp(rp), gate(gt), swing(sw) {}
    String getSubtitle() override { return "hold a key"; }
    void notePlayed(int note) override { lastNote = note; }

    void paint(Graphics &g, double t) override {
        double rate = juce::jmax(0.05, double(rateBeats()));
        float rp = ramp(), gt = juce::jlimit(0.1f, 1.0f, gate()),
              sw = swing();
        const int bars = 12;
        double stepSeconds = rate * kSecondsPerBeat;
        // the bars appear one by one, then the row starts again
        double loop = stepSeconds * (bars + 3);
        int shown = juce::jmin(bars, int(std::fmod(t, loop) / stepSeconds) + 1);

        const float cell = 38, left = 80, base = 180;
        for (int i = 0; i < bars; i++) {
            float v = juce::jlimit(0.08f, 1.0f, 0.55f * (1.0f + rp * 0.15f * i));
            float x = left + i * cell + ((i % 2) ? sw * cell * 0.5f : 0.0f);
            float w = juce::jmax(6.0f, (cell - 10) * gt);
            float h = v * 130;
            g.setColour(Colours::blue.withAlpha(i < shown ? 0.55f + 0.035f * i
                                                          : 0.12f));
            g.fillRoundedRectangle(x, base - h, w, h, 6);
        }
        text(g,
             "holding " + noteName(lastNote) + ": " + rateText() + " notes" +
                 (std::abs(rp) < 0.01f
                      ? String()
                      : rp > 0 ? ", getting louder" : ", getting quieter"),
             60, 40, 13, Colours::muted);

        int first = (lastNote / 12) * 12;
        litKeyboard(g, 60, 205, 480, 100, first, [&](int n) {
            return n == lastNote ? Colours::blue
                                 : juce::Colours::transparentBlack;
        });

        // the singer bounces on every repeat
        float beat = fracOf(t / stepSeconds);
        float squash = 1.0f + 0.08f * (1.0f - beat);
        g.setColour(Colours::blue);
        fillEllipseCentred(g, 660, 140, 54 * squash, 54 / squash);
        drawFace(g, 660, 140, 1.4f, Mood::sing, t);
    }

  private:
    Getter rateBeats;
    TextGetter rateText;
    Getter ramp, gate, swing;
    int lastNote = 60;
};

std::unique_ptr<PlayfulScene> noteRepeat(Getter rateBeats, TextGetter rateText,
                                         Getter ramp, Getter gate,
                                         Getter swing) {
    return std::make_unique<NoteRepeatScene>(rateBeats, rateText, ramp, gate,
                                             swing);
}

//==============================================================================
// Strum: six strings, a pick sweeping across them
class StrumScene : public PlayfulScene {
  public:
    StrumScene(Getter s, IntGetter d, std::function<bool()> a, IntGetter c,
               std::function<double(int, int, double, int)> o)
        : seconds(s), direction(d), alternate(a), curve(c), offset(o) {}
    String getSubtitle() override {
        return alternate() ? "up and down" : direction() == 1 ? "down" : "up";
    }

    void paint(Graphics &g, double t) override {
        double gap = seconds();
        static const Colour colours[] = {Colours::pink,   Colours::yellow,
                                         Colours::green,  Colours::blue,
                                         Colours::purple, Colours::orange};
        // one strum every 1.6 s; alternate flips every other one
        const double period = 1.6;
        int strum = int(t / period);
        double within = std::fmod(t, period);
        bool down = alternate() ? (strum % 2) == 1 : direction() == 1;
        // slowed down 4x so the spread is visible
        double slow = 4.0;

        for (int i = 0; i < 6; i++) {
            float y = 60 + i * 36;
            int order = down ? i : 5 - i; // up = low string (bottom) first
            double at = offset(order, 6, gap, curve()) * slow;
            float x = 120 + float(at / juce::jmax(0.001, gap * 5 * slow)) * 300;
            if (gap <= 0.0005)
                x = 120;
            bool plucked = within >= at;
            float wobble =
                plucked ? 3.0f * std::exp(-4.0f * float(within - at)) *
                              std::sin(float(within - at) * 60.0f)
                        : 0.0f;
            Path s;
            s.startNewSubPath(80, y);
            s.quadraticTo(320, y + wobble, 560, y);
            g.setColour(Colours::muted);
            strokePath(g, s, 2);
            g.setColour(colours[i].withAlpha(plucked ? 1.0f : 0.3f));
            fillEllipseCentred(g, x, y, 11, 11);
        }
        // the pick's path
        Path sweep;
        if (down) {
            sweep.startNewSubPath(110, 40);
            sweep.lineTo(440, 260);
        } else {
            sweep.startNewSubPath(110, 260);
            sweep.lineTo(440, 40);
        }
        g.setColour(Colours::cream);
        strokeDashed(g, sweep, 3, 6, 6);

        text(g,
             String("C major, strummed ") + (down ? "downwards" : "upwards") +
                 ": each note " + String(juce::roundToInt(gap * 1000)) +
                 " ms after the last",
             80, 300, 13, Colours::muted);

        // the guitar
        g.setColour(Colours::orange);
        g.fillRoundedRectangle(610, 60, 130, 180, 60);
        drawFace(g, 675, 130, 1.2f, within < 0.4 ? Mood::sing : Mood::smile, t);
        g.setColour(Colours::ink);
        fillEllipseCentred(g, 675, 200, 22, 22);
    }

  private:
    Getter seconds;
    IntGetter direction;
    std::function<bool()> alternate;
    IntGetter curve;
    std::function<double(int, int, double, int)> offset;
};

std::unique_ptr<PlayfulScene>
strum(Getter seconds, IntGetter direction, std::function<bool()> alternate,
      IntGetter curve, std::function<double(int, int, double, int)> offset) {
    return std::make_unique<StrumScene>(seconds, direction, alternate, curve,
                                        offset);
}

//==============================================================================
// Humanise: grey notes on the grid, green ones nudged around them
class HumaniseScene : public PlayfulScene {
  public:
    HumaniseScene(Getter tm, Getter v, Getter l, Getter a)
        : timing(tm), velocity(v), length(l), amount(a) {}
    String getSubtitle() override {
        float a = amount();
        return a < 0.01f ? "off" : a < 0.4f ? "gentle" : a < 0.75f ? "loose"
                                                                   : "sloppy";
    }

    void paint(Graphics &g, double t) override {
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(40, 40, 720, 210, 20);
        g.setColour(Colours::dim);
        for (int i = 0; i < 17; i++)
            g.fillRect(60.0f + i * 42 - 1, 56.0f, 2.0f, 178.0f);

        float a = amount();
        // px per second: one grid cell is a 1/16 at 120 bpm (0.125 s),
        // with the nudges drawn 4x bigger so they can be seen
        float pxPerSecond = 4 * 42 / 0.125f;
        // new random nudges every 2 seconds
        juce::Random r(int(t / 2.0) * 977 + 7);
        static const float rows[] = {150, 110, 190};
        for (int i = 0; i < 16; i++) {
            float row = rows[i % 3];
            float x = 60 + i * 42 - 6;
            float dx = r.nextFloat() * timing() * a * pxPerSecond;
            float dw = r.nextFloat() * length() * a * 34 * 4;
            float dv = (r.nextFloat() * 2 - 1) * velocity() * a / 127.0f;
            g.setColour(Colours::green.withAlpha(
                juce::jlimit(0.25f, 1.0f, 0.75f + dv * 2.5f)));
            g.fillRoundedRectangle(x + dx, row - 8, 34 + dw, 16, 6);
            // the grid note, as an outline on top
            g.setColour(Colours::muted.withAlpha(0.7f));
            g.drawRoundedRectangle(x, row - 8, 34, 16, 6, 2);
        }
        text(g,
             utf8("outline: the grid · green: what plays, nudged in time and "
                  "loudness (shown 4x bigger)"),
             40, 282, 13, Colours::muted);
    }

  private:
    Getter timing, velocity, length, amount;
};

std::unique_ptr<PlayfulScene> humanise(Getter timingSeconds, Getter velocity,
                                       Getter length, Getter amount) {
    return std::make_unique<HumaniseScene>(timingSeconds, velocity, length,
                                           amount);
}

//==============================================================================
// MIDI Echo: a singer and its fading copies
class EchoScene : public PlayfulScene {
  public:
    EchoScene(TextGetter tt, Getter tb, IntGetter r, Getter d, IntGetter tr)
        : timeText(tt), timeBeats(tb), repeats(r), decay(d), transpose(tr) {}
    String getSubtitle() override { return timeText(); }
    void notePlayed(int note) override { root = note; }

    void paint(Graphics &g, double t) override {
        int n = juce::jlimit(1, 8, repeats());
        float dc = decay();
        int tr = transpose();
        double step = juce::jmax(0.1, double(timeBeats()) * kSecondsPerBeat);
        double loop = step * (n + 3);
        double now = std::fmod(t, loop);

        int shown = juce::jmin(n, 5); // room for five copies
        float spacing = 640.0f / juce::jmax(1, shown);
        float level = 1.0f;
        for (int i = 0; i <= shown; i++) {
            if (i > 0)
                level *= 1.0f - dc;
            int note = root + i * tr;
            if (level < 0.01f || note < 0 || note > 127)
                break;
            float s = 0.45f + 0.55f * level;
            float x = 90 + i * spacing, y = 150 - i * 14.0f;
            bool lit = now >= i * step && now < i * step + step * 0.6;
            g.setColour(Colours::pink.withAlpha(
                juce::jlimit(0.15f, 1.0f, level * (lit ? 1.0f : 0.6f))));
            fillEllipseCentred(g, x, y, 44 * s, 44 * s);
            drawFace(g, x, y, 1.1f * s, lit ? Mood::sing : Mood::smile, t);
            text(g, noteName(note), x, y + 70, 15, Colours::text, true,
                 juce::Justification::horizontallyCentred);
        }
        String what = String(n) + (n == 1 ? " echo" : " echoes") +
                      ", every " + timeText() + ", each " +
                      String(juce::roundToInt(dc * 100)) + "% quieter";
        if (tr != 0)
            what << " and " << (tr > 0 ? "+" : "") << tr << " st";
        text(g, what, 60, 300, 13, Colours::muted);
    }

  private:
    TextGetter timeText;
    Getter timeBeats;
    IntGetter repeats;
    Getter decay;
    IntGetter transpose;
    int root = 60;
};

std::unique_ptr<PlayfulScene> midiEcho(TextGetter timeText, Getter timeBeats,
                                       IntGetter repeats, Getter decay,
                                       IntGetter transpose) {
    return std::make_unique<EchoScene>(timeText, timeBeats, repeats, decay,
                                       transpose);
}

//==============================================================================
// Effects

// Tape: a cassette whose reels wobble; the face blushes as it saturates
class TapeScene : public PlayfulScene {
  public:
    TapeScene(Getter w, Getter f, Getter s, Getter h, Getter wb)
        : wow(w), flutter(f), saturate(s), hiss(h), wobble(wb) {}
    String getSubtitle() override {
        float s = saturate();
        return s < 0.15f ? "clean" : s < 0.5f ? "warm" : s < 0.8f ? "hot"
                                                                  : "melting";
    }
    void paint(Graphics &g, double t) override {
        float w = wow(), f = flutter(), sat = saturate(), hs = hiss();
        // the picture's own wobble, so it moves even when nothing plays
        float wob = 0.6f * w * std::sin(float(t) * 3.4f) +
                    0.25f * f * std::sin(float(t) * 40.0f);
        Graphics::ScopedSaveState state(g);
        g.addTransform(AffineTransform::rotation(wob * 0.04f, 400, 160));

        g.setColour(Colours::panel);
        g.fillRoundedRectangle(200, 40, 400, 230, 26);
        g.setColour(Colours::dim);
        g.drawRoundedRectangle(200, 40, 400, 230, 26, 4);
        g.fillRoundedRectangle(240, 80, 320, 110, 20);
        for (int r = 0; r < 2; r++) {
            float cx = r == 0 ? 320.0f : 480.0f;
            float speed = 2.2f * (1.0f + wob * 0.6f);
            float angle = float(t) * speed;
            g.setColour(Colours::cream);
            fillEllipseCentred(g, cx, 135, 40, 40);
            g.setColour(Colours::dim);
            fillEllipseCentred(g, cx, 135, 14, 14);
            g.setColour(Colours::greyKey);
            for (int k = 0; k < 3; k++) {
                Path spoke;
                spoke.addRoundedRectangle(cx - 3, 103, 6, 14, 3);
                g.fillPath(spoke, AffineTransform::rotation(
                                      angle + k * 2.0944f, cx, 135));
            }
        }
        drawFace(g, 400, 226, 1.1f, sat > 0.6f ? Mood::sing : Mood::smile, t);
        // extra blush when saturating
        g.setColour(Colours::pink.withAlpha(0.25f + 0.6f * sat));
        fillEllipseCentred(g, 378, 232, 8 + 6 * sat, 5 + 3 * sat);
        fillEllipseCentred(g, 422, 232, 8 + 6 * sat, 5 + 3 * sat);

        // sound going in and out
        for (int side = 0; side < 2; side++) {
            float x0 = side == 0 ? 120.0f : 620.0f;
            Path wave;
            for (int i = 0; i <= 40; i++) {
                float x = x0 + i * 2.0f;
                float y = 150 - 10 * std::sin(i * 0.4f + float(t) * 6.0f) *
                                    (side == 1 ? 1.0f + wob : 1.0f);
                if (i == 0)
                    wave.startNewSubPath(x, y);
                else
                    wave.lineTo(x, y);
            }
            g.setColour(Colours::blue);
            strokePath(g, wave, 3);
        }
        // hiss: little specks
        juce::Random r(int(t * 12));
        g.setColour(Colours::muted.withAlpha(0.6f));
        for (int i = 0; i < int(hs * 60); i++)
            fillEllipseCentred(g, 200 + r.nextFloat() * 400,
                               40 + r.nextFloat() * 230, 1.2f, 1.2f);
        text(g, "the reels wobble with wow and flutter; the face blushes as it "
                "saturates",
             400, 306, 13, Colours::muted, false,
             juce::Justification::horizontallyCentred);
    }

  private:
    Getter wow, flutter, saturate, hiss, wobble;
};

std::unique_ptr<PlayfulScene> tape(Getter wow, Getter flutter, Getter saturate,
                                   Getter hiss, Getter wobble) {
    return std::make_unique<TapeScene>(wow, flutter, saturate, hiss, wobble);
}

// Lo-fi: a smooth wave and the steps that are heard
class LofiScene : public PlayfulScene {
  public:
    LofiScene(Getter b, Getter r, Getter n, Getter m)
        : bits(b), rateHz(r), noise(n), mix(m) {}
    String getSubtitle() override {
        float b = bits();
        return b >= 12 ? "subtle" : b >= 6 ? "crunchy" : "crushed";
    }
    void paint(Graphics &g, double t) override {
        float b = bits(), hz = rateHz(), nz = noise(), mx = mix();
        auto wave = [&](float i) {
            return std::sin(i * 0.045f + float(t) * 1.5f) * 70.0f;
        };
        Path smooth;
        for (int i = 0; i < 310; i++) {
            float x = 60 + i * 2.2f, y = 160 - wave(float(i));
            if (i == 0)
                smooth.startNewSubPath(x, y);
            else
                smooth.lineTo(x, y);
        }
        g.setColour(Colours::greyKey);
        strokeDashed(g, smooth, 2, 4, 5);

        // hold length in picture steps: 44.1 kHz = 1 step, lower = longer
        int hold = juce::jlimit(1, 60, juce::roundToInt(44100.0f / hz / 2.0f));
        float levels = std::pow(2.0f, juce::jlimit(1.0f, 16.0f, b) - 1.0f);
        Path steps;
        juce::Random r(int(t * 20));
        for (int i = 0; i < 310; i += hold) {
            float v = wave(float(i)) / 70.0f;
            v = std::round(v * levels) / levels;
            float y = 160 - v * 70 + (r.nextFloat() - 0.5f) * nz * 16;
            float x0 = 60 + i * 2.2f, x1 = 60 + juce::jmin(310, i + hold) * 2.2f;
            if (i == 0)
                steps.startNewSubPath(x0, y);
            else
                steps.lineTo(x0, y);
            steps.lineTo(x1, y);
        }
        g.setColour(Colours::green.withAlpha(0.35f + 0.65f * mx));
        g.strokePath(steps, juce::PathStrokeType(4));

        // the little robot
        g.setColour(Colours::green);
        g.fillRect(640.0f, 210.0f, 80.0f, 80.0f);
        g.setColour(Colours::ink);
        bool blink = std::fmod(t, 3.0) < 0.15;
        g.fillRect(648.0f, 226.0f, 12.0f, blink ? 3.0f : 12.0f);
        g.fillRect(696.0f, 226.0f, 12.0f, blink ? 3.0f : 12.0f);
        g.fillRect(660.0f, 262.0f, 40.0f, 8.0f);

        text(g,
             String(juce::roundToInt(b)) + utf8(" bits · ") +
                 (hz >= 1000 ? String(hz / 1000.0f, 1) + " kHz"
                             : String(juce::roundToInt(hz)) + " Hz") +
                 ": the green steps are what you hear",
             60, 300, 13, Colours::muted);
    }

  private:
    Getter bits, rateHz, noise, mix;
};

std::unique_ptr<PlayfulScene> lofi(Getter bits, Getter rateHz, Getter noise,
                                   Getter mix) {
    return std::make_unique<LofiScene>(bits, rateHz, noise, mix);
}

// Drive: the clipping curve and a flame that grows with the drive
class DriveScene : public PlayfulScene {
  public:
    DriveScene(Getter d, Getter tn, IntGetter ty, TextGetter tyName, Getter m,
               std::function<float(float, int)> sh)
        : driveDb(d), tone(tn), type(ty), typeName(tyName), mix(m), shape(sh) {}
    String getSubtitle() override { return typeName().toLowerCase(); }
    void paint(Graphics &g, double t) override {
        float db = driveDb();
        int ty = type();
        float gain = std::pow(10.0f, db / 20.0f);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(60, 40, 250, 250, 20);
        g.setColour(Colours::dim);
        g.fillRect(80.0f, 164.0f, 210.0f, 2.0f);
        g.fillRect(184.0f, 60.0f, 2.0f, 210.0f);
        Path curve;
        for (int i = 0; i <= 100; i++) {
            float x = (i - 50) / 50.0f;
            // the clipping curve itself (the level is evened out after)
            float y = juce::jlimit(-1.1f, 1.1f, shape(x * gain, ty));
            float px = 80 + i * 2.1f, py = 165 - 95 * y;
            if (i == 0)
                curve.startNewSubPath(px, py);
            else
                curve.lineTo(px, py);
        }
        g.setColour(Colours::red);
        strokePath(g, curve, 4);
        text(g, typeName().toLowerCase() + " curve", 80, 282, 13,
             Colours::muted);

        // the flame
        float size = 0.55f + 0.45f * juce::jlimit(0.0f, 1.0f, db / 36.0f);
        float flick = 1.0f + 0.05f * std::sin(float(t) * 13.0f) +
                      0.03f * std::sin(float(t) * 29.0f);
        Graphics::ScopedSaveState state(g);
        g.addTransform(AffineTransform::scale(size, size * flick, 540, 270));
        Path outer = svg("M520 270 C440 250 450 170 500 130 C500 170 530 170 540 "
                         "150 C540 110 560 80 600 60 C590 110 640 140 640 190 "
                         "C640 240 600 272 520 270z");
        Path inner = svg("M530 268 C490 250 500 210 520 190 C530 215 550 210 560 "
                         "195 C580 220 580 262 530 268z");
        outer.setUsingNonZeroWinding(true);
        g.setColour(Colours::orange.interpolatedWith(Colours::red,
                                                     1.0f - tone()));
        g.fillPath(outer);
        g.setColour(Colours::yellow);
        g.fillPath(inner);
        drawFace(g, 548, 232, 0.9f, db > 20 ? Mood::sing : Mood::smile, t);
    }

  private:
    Getter driveDb, tone;
    IntGetter type;
    TextGetter typeName;
    Getter mix;
    std::function<float(float, int)> shape;
};

std::unique_ptr<PlayfulScene> drive(Getter driveDb, Getter tone, IntGetter type,
                                    TextGetter typeName, Getter mix,
                                    std::function<float(float, int)> shape) {
    return std::make_unique<DriveScene>(driveDb, tone, type, typeName, mix,
                                        shape);
}

// Filter: the response curve, moving with the envelope and LFO
class ResonantFilterScene : public PlayfulScene {
  public:
    ResonantFilterScene(Getter live, Getter res, Getter env, Getter l,
                        IntGetter ty, juce::StringArray names, TextGetter rate)
        : liveCutoff(live), resonance(res), envelope(env), lfo(l), type(ty),
          typeNames(names), lfoRateText(rate) {}
    String getSubtitle() override {
        return typeNames[type()].toLowerCase();
    }
    void paint(Graphics &g, double t) override {
        float fc = liveCutoff(), res = resonance();
        int ty = type();
        float q = 0.5f + res * res * 14.0f;
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(40, 24, 560, 270, 20);

        auto xFor = [](float hz) {
            return 60 + std::log(hz / 20.0f) / std::log(1000.0f) * 520;
        };
        auto magnitude = [&](float hz) {
            float w = hz / fc;
            auto pole2 = [&](float qq, float &re, float &im) {
                re = 1 - w * w;
                im = w / qq;
            };
            float re, im;
            pole2(ty == 0 ? q : q, re, im);
            float d = std::sqrt(re * re + im * im);
            float m;
            switch (ty) {
            case 2:
                m = w * w / d;
                break;
            case 3:
                m = (w / q) / d;
                break;
            case 4:
                m = std::abs(1 - w * w) / d;
                break;
            default:
                m = 1 / d;
            }
            if (ty == 0) { // second, gentle stage
                float re2 = 1 - w * w, im2 = w / 0.54f;
                m /= std::sqrt(re2 * re2 + im2 * im2);
            }
            return juce::jlimit(1.0e-4f, 30.0f, m);
        };
        Path curve;
        for (int i = 0; i <= 130; i++) {
            float hz = 20.0f * std::pow(1000.0f, i / 130.0f);
            float db = 20 * std::log10(magnitude(hz));
            float y = juce::jlimit(36.0f, 280.0f, 140 - db * 3.2f);
            if (i == 0)
                curve.startNewSubPath(xFor(hz), y);
            else
                curve.lineTo(xFor(hz), y);
        }
        g.setColour(Colours::blue);
        strokePath(g, curve, 4);
        float cx = xFor(fc);
        g.setColour(Colours::dim);
        g.fillRect(cx - 1, 72.0f, 2.0f, 200.0f);
        String moving;
        if (std::abs(envelope()) > 0.01f && lfo() > 0.01f)
            moving = ", moving with the envelope and LFO";
        else if (std::abs(envelope()) > 0.01f)
            moving = ", moving with the envelope";
        else if (lfo() > 0.01f)
            moving = ", moving with the LFO (" + lfoRateText() + ")";
        text(g, formatFrequency(fc) + moving,
             juce::jlimit(60.0f, 380.0f, cx + 10), 66, 13, Colours::muted);

        // the type list
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(616, 24, 160, 270, 20);
        text(g, "TYPE", 632, 52, 12, Colours::muted);
        for (int j = 0; j < typeNames.size(); j++) {
            if (j == ty) {
                g.setColour(Colours::dim);
                g.fillRoundedRectangle(626, 64 + j * 34.0f, 140, 28, 10);
            }
            text(g, typeNames[j], 638, 83 + j * 34.0f, 14,
                 j == ty ? Colours::text : Colours::muted);
        }
        float bob = 3.0f * std::sin(float(t) * 2.0f);
        g.setColour(Colours::blue);
        fillEllipseCentred(g, 696, 258 + bob, 20, 20);
        drawFace(g, 696, 258 + bob, 0.65f, Mood::smile, t);
    }

  private:
    Getter liveCutoff, resonance, envelope, lfo;
    IntGetter type;
    juce::StringArray typeNames;
    TextGetter lfoRateText;
};

std::unique_ptr<PlayfulScene>
resonantFilter(Getter liveCutoff, Getter resonance, Getter envelope, Getter lfo,
               IntGetter type, juce::StringArray typeNames,
               TextGetter lfoRateText) {
    return std::make_unique<ResonantFilterScene>(
        liveCutoff, resonance, envelope, lfo, type, typeNames, lfoRateText);
}

// Ducker: the volume dips on every hit and breathes back
class DuckerScene : public PlayfulScene {
  public:
    DuckerScene(TextGetter src, std::function<bool()> beat,
                std::function<float(float)> g, Getter live)
        : source(src), followsBeat(beat), gainAt(g), liveGain(live) {}
    String getSubtitle() override {
        return followsBeat() ? "on every beat" : "following " + source();
    }
    void paint(Graphics &g, double t) override {
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(40, 40, 720, 200, 20);
        const float beatPx = 170, beatSeconds = 0.5f;
        Path curve;
        for (int b = 0; b < 4; b++) {
            float x0 = 60 + b * beatPx;
            for (int i = 0; i <= 60; i++) {
                float sec = i / 60.0f * beatSeconds;
                float y = 200 - 130 * gainAt(sec);
                if (b == 0 && i == 0)
                    curve.startNewSubPath(x0, y);
                else
                    curve.lineTo(x0 + i / 60.0f * beatPx, y);
            }
            g.setColour(Colours::pink);
            fillEllipseCentred(g, x0, 216, 9, 9);
        }
        g.setColour(Colours::purple);
        strokePath(g, curve, 4);
        // a dot riding the curve at 120 bpm
        double beats = t / beatSeconds;
        float within = fracOf(beats);
        int b = int(std::fmod(beats, 4.0));
        float px = 60 + (b + within) * beatPx;
        float py = 200 - 130 * gainAt(within * beatSeconds);
        g.setColour(Colours::yellow);
        fillEllipseCentred(g, px, py, 7, 7);

        text(g, "volume of this track", 60, 64, 13, Colours::muted);
        text(g,
             followsBeat()
                 ? "pink dots: every beat ducks the volume, then it breathes "
                   "back"
                 : "pink dots: hits on " + source() +
                       " duck the volume, then it breathes back",
             60, 262, 13, Colours::muted);
        float squish = gainAt(within * beatSeconds);
        g.setColour(Colours::purple);
        fillEllipseCentred(g, 720, 290, 24 + 4 * (1 - squish),
                           24 * (0.75f + 0.25f * squish));
        drawFace(g, 720, 290, 0.8f, Mood::sing, t);
    }

  private:
    TextGetter source;
    std::function<bool()> followsBeat;
    std::function<float(float)> gainAt;
    Getter liveGain;
};

std::unique_ptr<PlayfulScene> ducker(TextGetter sourceName,
                                     std::function<bool()> followsBeat,
                                     std::function<float(float)> gainAt,
                                     Getter liveGain) {
    return std::make_unique<DuckerScene>(sourceName, followsBeat, gainAt,
                                         liveGain);
}

// Beat Repeat: slices of sound, the repeated ones highlighted
class BeatRepeatScene : public PlayfulScene {
  public:
    BeatRepeatScene(TextGetter len, IntGetter reps, Getter dec, Getter pit,
                    IntGetter live, std::function<void(bool)> act)
        : lengthText(len), repeats(reps), decay(dec), pitch(pit),
          liveRepeat(live), setActive(act) {}
    ~BeatRepeatScene() override {
        if (held)
            setActive(false);
    }
    String getSubtitle() override { return "hold encoder 1: repeat"; }
    void encoderButton(int encoder, bool down) override {
        if (encoder == 0) {
            held = down;
            setActive(down);
        }
    }
    void paint(Graphics &g, double t) override {
        int live = liveRepeat();
        bool repeating = held && live >= 0;
        int count = repeats();
        float dc = decay();
        float pt = pitch();
        for (int i = 0; i < 8; i++) {
            float x = 40 + i * 90.0f;
            bool rep = i >= 4 && (count < 0 || i - 4 < count);
            bool now = repeating && rep && (live % 4) == i - 4;
            float level = rep ? std::pow(1.0f - dc, float(i - 4)) : 1.0f;
            g.setColour(Colours::panel);
            g.fillRoundedRectangle(x, 70, 80, 120, 16);
            if (rep) {
                g.setColour(now ? Colours::yellow
                                : Colours::yellow.withAlpha(0.5f));
                g.drawRoundedRectangle(x, 70, 80, 120, 16, now ? 5.0f : 3.0f);
            }
            // the same slice repeated (pitch squeezes or stretches it)
            int src = rep ? 3 : i;
            float squeeze = rep ? std::pow(2.0f, -(i - 3) * pt / 12.0f) : 1.0f;
            for (int j = 0; j < 6; j++) {
                float h = (8 + ((src * 3 + j * 7) % 5) * 8) * level;
                float bx = x + 10 + j * 10 * juce::jlimit(0.5f, 1.5f, squeeze);
                if (bx > x + 70)
                    break;
                g.setColour(rep ? Colours::cream : Colours::muted);
                g.fillRoundedRectangle(bx, 130 - h / 2, 5, h, 2);
            }
            if (rep)
                text(g, "repeat " + String(i - 3), x + 40, 214, 13,
                     Colours::yellow, false,
                     juce::Justification::horizontallyCentred);
        }
        String what = held ? (live == -2 ? "waiting for the next " +
                                               lengthText() + " ..."
                                         : "repeating the last " +
                                               lengthText() + " slice")
                           : "holding: the last " + lengthText() +
                                 " slice repeats, quantised to the beat";
        text(g, what, 40, 60, 13, Colours::muted);

        float pulse = held ? 0.5f + 0.5f * std::sin(float(t) * 12.0f) : 0.0f;
        g.setColour(held ? Colours::yellow.withAlpha(0.6f + 0.4f * pulse)
                         : Colours::dim);
        g.fillRoundedRectangle(300, 244, 200, 56, 18);
        text(g, held ? "repeating!" : "hold encoder 1", 400, 279, 17,
             held ? Colours::ink : Colours::text, true,
             juce::Justification::horizontallyCentred);
    }

  private:
    TextGetter lengthText;
    IntGetter repeats;
    Getter decay, pitch;
    IntGetter liveRepeat;
    std::function<void(bool)> setActive;
    bool held = false;
};

std::unique_ptr<PlayfulScene> beatRepeat(TextGetter lengthText,
                                         IntGetter repeats, Getter decay,
                                         Getter pitch, IntGetter liveRepeat,
                                         std::function<void(bool)> setActive) {
    return std::make_unique<BeatRepeatScene>(lengthText, repeats, decay, pitch,
                                             liveRepeat, setActive);
}

} // namespace PlayfulScenes
