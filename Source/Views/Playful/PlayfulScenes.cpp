#include "PlayfulScenes.h"

using namespace Playful;
using juce::AffineTransform;
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::String;

namespace PlayfulScenes {

//==============================================================================
// Shared helpers

void strokePath(Graphics &g, const Path &p, float width) {
    g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));
}

void strokeDashed(Graphics &g, const Path &p, float width, float dash,
                  float gap) {
    Path dashed;
    float lengths[] = {juce::jmax(0.01f, dash), gap};
    juce::PathStrokeType(width, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createDashedStroke(dashed, p, lengths, 2);
    g.fillPath(dashed);
}

Path svg(const String &d) { return juce::Drawable::parseSVGPath(d); }

void text(Graphics &g, const String &s, float x, float baseline, float size,
          Colour colour, bool bold, juce::Justification j) {
    g.setColour(colour);
    g.setFont(font(size, bold));
    g.drawSingleLineText(s, juce::roundToInt(x), juce::roundToInt(baseline), j);
}

static bool isBlackKey(int note) {
    int pc = ((note % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void keyboard(Graphics &g, float x, float y, float ww, float wh, float bw,
              float bh, int firstNote, int numNotes,
              const std::function<Colour(int)> &keyColour) {
    // white keys first
    int whiteIndex = 0;
    juce::Array<std::pair<int, float>> blacks;
    for (int n = firstNote; n < firstNote + numNotes; n++) {
        if (isBlackKey(n)) {
            blacks.add({n, x + whiteIndex * ww});
            continue;
        }
        auto c = keyColour(n);
        auto r = juce::Rectangle<float>(x + whiteIndex * ww, y, ww, wh);
        g.setColour(c.isTransparent() ? Colours::text : c);
        g.fillRoundedRectangle(r, 8);
        g.setColour(Colours::ink);
        g.drawRoundedRectangle(r, 8, 3);
        whiteIndex++;
    }
    for (auto &b : blacks) {
        auto c = keyColour(b.first);
        g.setColour(c.isTransparent() ? Colours::ink : c);
        g.fillRoundedRectangle(b.second - bw / 2, y, bw, bh, 6);
    }
}

String noteName(int note) {
    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
    return String(names[((note % 12) + 12) % 12]) +
           String(int(std::floor(note / 12.0)) - 1);
}

String formatFrequency(float hz, bool shortForm) {
    if (hz >= 1000.0f)
        return String(hz / 1000.0f, 1) + (shortForm ? "k" : " kHz");
    return String(juce::roundToInt(hz)) + (shortForm ? "" : " Hz");
}

static String percent(float v) {
    return String(juce::roundToInt(v * 100.0f)) + "%";
}

// fades whatever is drawn inside area while it exists
struct Fade {
    Fade(Graphics &gr, juce::Rectangle<float> area, float alpha)
        : g(gr), active(alpha < 0.99f) {
        g.saveState();
        if (active) {
            g.reduceClipRegion(area.getSmallestIntegerContainer());
            g.beginTransparencyLayer(juce::jlimit(0.0f, 1.0f, alpha));
        }
    }
    ~Fade() {
        if (active)
            g.endTransparencyLayer();
        g.restoreState();
    }
    Graphics &g;
    bool active;
};

static float frac(double x) { return float(x - std::floor(x)); }

// CSS "float": up and down by 8px over 2.6s
static float floatY(double t, double delay = 0.0) {
    return -8.0f * pingPong(t + delay, 2.6);
}

// the "pBounce" hop: returns y offset and squash
struct Squash {
    float dy, sx, sy;
};
static Squash bounce(float p) {
    if (p < 0.45f) {
        float e = easeInOutSine(p / 0.45f);
        return {-40 * e, 1 - 0.05f * e, 1 + 0.05f * e};
    }
    if (p < 0.9f) {
        float e = easeInOutSine((p - 0.45f) / 0.45f);
        return {-40 * (1 - e), 0.95f + 0.17f * e, 1.05f - 0.17f * e};
    }
    float e = easeInOutSine((p - 0.9f) / 0.1f);
    return {0, 1.12f - 0.12f * e, 0.88f + 0.12f * e};
}

// CSS "sway": -6..6 degrees
static float swayAngle(double t, double duration, double delay = 0.0) {
    return juce::degreesToRadians(-6.0f +
                                  12.0f * pingPong(t + delay, duration));
}

static const Colour candyColours[] = {Colours::pink,   Colours::yellow,
                                      Colours::green,  Colours::blue,
                                      Colours::purple, Colours::orange};

//==============================================================================
// 1 Reverb: a ghost sings in an echo cave
class ReverbScene : public PlayfulScene {
  public:
    ReverbScene(Getter r, Getter w, Getter d, Getter wt)
        : room(r), width(w), damp(d), wet(wt) {}
    String getSubtitle() override { return "echo cave"; }

    void paint(Graphics &g, double t) override {
        float rs = juce::jlimit(0.0f, 1.0f, room());
        float wd = juce::jlimit(0.0f, 1.0f, width());
        float dp = juce::jlimit(0.0f, 1.0f, damp());
        float wt = juce::jlimit(0.0f, 1.0f, wet());

        float w = 300 + rs * 360, h = 170 + rs * 130;
        juce::Rectangle<float> cave(400 - w / 2, 176 - h / 2, w, h);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(cave, 70);
        g.setColour(Colours::blue);
        g.drawRoundedRectangle(cave, 70, 6);

        // fluffy walls: more poms = more damping
        int n = juce::roundToInt(4 + dp * 18);
        for (int i = 0; i < n; i++) {
            float a = float(i) / n * juce::MathConstants<float>::twoPi;
            float x = 400 + (w / 2 - 4) * std::cos(a);
            float y = 176 + (h / 2 - 4) * std::sin(a);
            g.setColour(Colours::cream);
            fillEllipseCentred(g, x, y, 9 + dp * 5, 9 + dp * 5);
            g.setColour(Colours::pink);
            fillEllipseCentred(g, x, y, 3 + dp * 2, 3 + dp * 2);
        }

        // echo bubbles drifting out to the walls
        double duration = 1.8 + rs * 1.8 - dp * 0.8;
        for (int j = 0; j < 8; j++) {
            float ang = j / 8.0f * juce::MathConstants<float>::twoPi + 0.3f;
            float dx = std::cos(ang) * (w / 2 - 30) * (0.55f + wd * 0.45f);
            float dy = std::sin(ang) * (h / 2 - 30) * (1 - wd * 0.35f);
            float r = 9.0f + ((j * 7) % 5) * 2.0f;
            float p = frac((t + j * 0.35) / duration);
            float e = 1 - (1 - p) * (1 - p);
            float scale = 0.3f + 0.7f * e;
            float alpha = p < 0.15f ? p / 0.15f : 1 - (p - 0.15f) / 0.85f;
            float bx = 400 + dx * e, by = 176 + dy * e;
            g.setColour(Colours::green.withAlpha(0.9f * alpha));
            fillEllipseCentred(g, bx, by, r * scale, r * scale);
            g.setColour(Colours::white.withAlpha(0.8f * alpha));
            fillEllipseCentred(g, bx - r * 0.35f * scale,
                               by - r * 0.35f * scale, r * 0.25f * scale,
                               r * 0.25f * scale);
        }

        // the ghost
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(0, floatY(t)));
            static const Path ghost =
                svg("M362 210 L362 168 A38 38 0 0 1 438 168 L438 210 L425 200 "
                    "L412 212 L400 200 L388 212 L375 200 Z");
            g.setColour(Colours::text);
            g.fillPath(ghost);
            drawFace(g, 400, 170, 1, Mood::sing, t);
        }

        // wet drop
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(730, 60));
            static const Path drop =
                svg("M20 0 C20 0 0 26 0 42 A20 20 0 0 0 40 "
                    "42 C40 26 20 0 20 0 Z");
            g.setColour(Colours::dim);
            g.fillPath(drop);
            {
                Graphics::ScopedSaveState c(g);
                g.reduceClipRegion(drop);
                g.setColour(Colours::red);
                g.fillRect(0.0f, 62 - wt * 62, 40.0f, 70.0f);
            }
            text(g, "wet", 20, 84, 12, Colours::muted, false,
                 juce::Justification::horizontallyCentred);
        }
    }

  private:
    Getter room, width, damp, wet;
};

std::unique_ptr<PlayfulScene> reverb(Getter roomSize, Getter width,
                                     Getter damping, Getter wet) {
    return std::make_unique<ReverbScene>(roomSize, width, damping, wet);
}

//==============================================================================
// 2 Delay: a jelly hops and echo jellies copy it
class DelayScene : public PlayfulScene {
  public:
    DelayScene(Getter l, Getter f, Getter m) : length(l), feedback(f), mix(m) {}
    String getSubtitle() override {
        int n = countJellies();
        return String(n) + (n == 1 ? " jelly" : " jellies");
    }

    int countJellies() {
        float len = juce::jmax(1.0f, length());
        float spacing = 70 + (len / 1000.0f) * 230;
        float decay = std::pow(10.0f, feedback() / 20.0f);
        float x = 80, o = 1;
        int n = 0;
        for (int i = 0; i < 9 && x < 770 && o > 0.07f; i++) {
            n++;
            x += spacing;
            o *= decay;
        }
        return n;
    }

    void paint(Graphics &g, double t) override {
        float len = juce::jmax(1.0f, length());
        float fb = feedback(), mx = juce::jlimit(0.0f, 1.0f, mix());

        static const Path ground =
            svg("M0 282 Q100 262 200 282 T400 282 T600 282 T800 282 L800 328 "
                "L0 328 Z");
        g.setColour(Colours::panel);
        g.fillPath(ground);

        struct Jelly {
            float x, s, o;
            Colour fill;
            double delay;
        };
        juce::Array<Jelly> jellies;
        float spacing = 70 + (len / 1000.0f) * 230;
        float decay = std::pow(10.0f, fb / 20.0f);
        float x = 80, s = 1, o = 1;
        static const Colour echoColours[] = {Colours::green, Colours::pink,
                                             Colours::purple, Colours::blue,
                                             Colours::orange};
        g.setColour(Colours::blue);
        for (int i = 0; i < 9 && x < 770 && o > 0.07f; i++) {
            jellies.add(
                {x, s,
                 i == 0 ? 1.0f
                        : (0.35f + mx * 0.65f) * juce::jmin(1.0f, o + 0.25f),
                 i == 0 ? Colours::yellow : echoColours[(i - 1) % 5],
                 i * len / 1000.0});
            float nx = x + spacing;
            if (nx < 770) {
                Path arc;
                arc.startNewSubPath(x, 240);
                arc.quadraticTo((x + nx) / 2, 150 - 40 * s, nx, 240);
                strokeDashed(g, arc, 3, 2, 9);
            }
            x = nx;
            s = juce::jmax(0.35f, s * std::sqrt(decay));
            o *= decay;
        }

        double cycle = len / 1000.0 * juce::jmax(2, jellies.size());
        static const Path body =
            svg("M-34 0 L-34 -30 A34 34 0 0 1 34 -30 L34 0 Q17 8 0 0 Q-17 8 "
                "-34 0 Z");
        for (auto &j : jellies) {
            auto sq = bounce(frac((t - j.delay) / cycle));
            Graphics::ScopedSaveState state(g);
            g.addTransform(AffineTransform::scale(j.s)
                               .scaled(sq.sx, sq.sy)
                               .translated(0, sq.dy)
                               .translated(j.x, 280));
            Fade fade(g, {-50, -80, 100, 95}, j.o);
            g.setColour(j.fill);
            g.fillPath(body);
            g.setColour(Colours::white.withAlpha(0.55f));
            fillEllipseCentred(g, -14, -50, 8, 4);
            drawFace(g, 0, -28, 0.9f, Mood::smile, t + j.delay);
        }

        text(g, String(juce::roundToInt(len)) + " ms between hops", 24, 316, 13,
             Colours::muted);
    }

  private:
    Getter length, feedback, mix;
};

std::unique_ptr<PlayfulScene> delay(Getter lengthMs, Getter feedbackDb,
                                    Getter mix) {
    return std::make_unique<DelayScene>(lengthMs, feedbackDb, mix);
}

//==============================================================================
// 3 Chorus: three birds sing on a swaying wire
class ChorusScene : public PlayfulScene {
  public:
    ChorusScene(Getter d, Getter s, Getter w, Getter m)
        : depth(d), speed(s), width(w), mix(m) {}
    String getSubtitle() override { return "3 singers"; }

    void paint(Graphics &g, double t) override {
        float dp = depth(), sp = juce::jmax(0.2f, speed());
        float wd = juce::jlimit(0.0f, 1.0f, width());
        float mx = juce::jlimit(0.0f, 1.0f, mix());

        float gap = 110 + wd * 130, wireY = 210, sag = wireY + 20 + dp * 8;
        auto yOn = [&](float x) {
            float u = (x + 20) / 840;
            return (1 - u) * (1 - u) * wireY + 2 * (1 - u) * u * sag +
                   u * u * wireY;
        };
        float xs[] = {400 - gap, 400, 400 + gap};
        Colour fills[] = {Colours::pink, Colours::yellow, Colours::purple};

        {
            Graphics::ScopedSaveState state(g);
            g.addTransform(
                AffineTransform::translation(0, -8.0f * pingPong(t, 3.0 / sp)));
            Path wire;
            wire.startNewSubPath(-20, wireY);
            wire.quadraticTo(400, sag, 820, wireY);
            g.setColour(Colours::blue);
            strokePath(g, wire, 5);

            static const Path wing = svg("M-8 6 Q-30 -4 -36 18 Q-18 16 -8 6 Z");
            static const Path beak = svg("M30 -2 L46 4 L30 10 Z");
            static const Path legs = svg("M-6 30 L-6 40 M6 30 L6 40");
            for (int i = 0; i < 3; i++) {
                bool side = i != 1;
                float sc = side ? 0.75f + mx * 0.25f : 1.0f;
                float alpha = side ? 0.35f + mx * 0.65f : 1.0f;
                Graphics::ScopedSaveState b(g);
                // sway about the feet (40 below the centre)
                auto transform =
                    AffineTransform::translation(0, -40)
                        .rotated(swayAngle(t, (1.4 + i * 0.3) / sp))
                        .translated(0, 40)
                        .scaled(sc)
                        .translated(xs[i], yOn(xs[i]) - 40);
                g.addTransform(transform);
                Fade fade(g, {-50, -45, 105, 95}, alpha);
                g.setColour(fills[i]);
                fillEllipseCentred(g, 0, 0, 34, 30);
                g.setColour(Colours::white.withAlpha(0.35f));
                g.fillPath(wing);
                g.setColour(Colours::orange);
                g.fillPath(beak);
                strokePath(g, legs, 3);
                drawFace(g, 6, -6, 0.8f, Mood::sing, t + i);
            }
        }

        int nn = juce::roundToInt(2 + mx * 4);
        static const Path stem = svg("M0 0 L0 -26 L16 -30 L16 -6");
        for (int k = 0; k < nn; k++) {
            float bx = xs[k % 3];
            float p = frac((t + k * 0.55) / 2.8);
            float alpha = p < 0.2f ? p / 0.2f : 1 - (p - 0.2f) / 0.8f;
            float e = 1 - (1 - p) * (1 - p);
            Graphics::ScopedSaveState n(g);
            g.addTransform(
                AffineTransform::translation(bx + 20, yOn(bx) - 90 - 70 * e));
            g.setColour(Colours::red.withAlpha(alpha));
            strokePath(g, stem, 4);
            fillEllipseCentred(g, -4, 2, 6, 4.5f);
            fillEllipseCentred(g, 12, -2, 6, 4.5f);
        }
    }

  private:
    Getter depth, speed, width, mix;
};

std::unique_ptr<PlayfulScene> chorus(Getter depthMs, Getter speedHz,
                                     Getter width, Getter mix) {
    return std::make_unique<ChorusScene>(depthMs, speedHz, width, mix);
}

//==============================================================================
// 4 Compressor: a cloud squishes a marshmallow
class CompressorScene : public PlayfulScene {
  public:
    CompressorScene(Getter th, Getter r, Getter a, Getter rl)
        : threshold(th), ratio(r), attack(a), release(rl) {}
    String getSubtitle() override {
        return squishAmount() > 0.05f ? "squish!" : "relaxed";
    }

    // how far the cloud squashes the marshmallow (0 - 0.6)
    float squishAmount() {
        float th = juce::jlimit(-60.0f, 0.0f, threshold());
        float rt = juce::jmax(1.0f, ratio());
        float cloudY = 40 + ((th + 60) / 60) * 150;
        const float top = 290 - 150;
        if (cloudY + 20 <= top)
            return 0.0f;
        float over = (cloudY + 20 - top) / 150;
        return juce::jlimit(0.0f, 0.6f, over * (1 - 1 / rt) * 1.6f);
    }

    void paint(Graphics &g, double t) override {
        float th = juce::jlimit(-60.0f, 0.0f, threshold());
        float rt = juce::jmax(1.0f, ratio());
        float atk = attack(), rel = release();

        g.setColour(Colours::panel);
        g.fillRect(0, 290, 800, 38);

        float cloudY = 40 + ((th + 60) / 60) * 150;
        const float top = 290 - 150;
        float squish = squishAmount();
        float sy = 1 - squish, sx = 1 + squish * 0.7f, h = 150 * sy;
        cloudY = juce::jmin(cloudY, top + 40);

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(
                0, -8.0f * pingPong(t, 0.8 + rel / 500.0)));
            g.setColour(Colours::blue);
            for (float x : {330.0f, 470.0f}) {
                Path line;
                line.startNewSubPath(x, 0);
                line.lineTo(x, cloudY);
                strokeDashed(g, line, 5, 4, 8);
            }
            static const Path cloud = svg(
                "M-120 20 Q-130 -14 -96 -18 Q-90 -48 -52 -40 Q-36 -66 0 -54 "
                "Q30 -72 54 -42 Q94 -50 98 -16 Q132 -10 120 20 Z");
            Graphics::ScopedSaveState c(g);
            g.addTransform(AffineTransform::translation(400, cloudY));
            g.fillPath(cloud);
            drawFace(g, 0, -14, 0.8f, Mood::smile, t);
        }

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(400, 290));
            {
                Graphics::ScopedSaveState m(g);
                g.addTransform(AffineTransform::scale(sx, sy));
                g.setColour(Colours::text);
                g.fillRoundedRectangle(-70, -150, 140, 150, 40);
                g.setColour(Colours::white);
                fillEllipseCentred(g, -30, -128, 16, 7);
            }
            drawFace(g, 0, -h * 0.55f, 1.1f + squish * 0.4f,
                     squish > 0.15f ? Mood::squish : Mood::smile, t);
            if (squish > 0.15f) {
                static const Path lines = svg(
                    "M-110 -20 L-130 -34 M110 -20 L130 -34 M-112 -6 L-136 -6 "
                    "M112 -6 L136 -6");
                g.setColour(Colours::green);
                strokePath(g, lines, 4);
            }
        }

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(600, 70));
            g.setColour(Colours::green);
            g.fillRoundedRectangle(0, 0, 150, 62, 20);
            g.fillPath(svg("M30 62 L22 82 L48 62 Z"));
            text(g, String(rt, 1) + ":1", 75, 42, 28, Colours::ink, true,
                 juce::Justification::horizontallyCentred);
        }

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(60, 80));
            g.setColour(Colours::panel);
            g.fillEllipse(0, 0, 80, 80);
            g.setColour(Colours::cream);
            g.drawEllipse(0, 0, 80, 80, 4);
            float af = juce::jlimit(0.03f, 0.45f, atk / 100.0f);
            float rf = juce::jlimit(0.05f, 0.5f, rel / 1000.0f);
            auto arc = [&](float fraction, float startDegrees) {
                Path p;
                float a0 = juce::degreesToRadians(startDegrees);
                p.addCentredArc(
                    40, 40, 30, 30, 0, a0,
                    a0 + fraction * juce::MathConstants<float>::twoPi, true);
                return p;
            };
            g.setColour(Colours::cream);
            strokePath(g, arc(af, 0), 8);
            g.setColour(Colours::red);
            strokePath(g, arc(rf, af * 360 + 8), 8);
            text(g, utf8("attack · release"), 40, 100, 12, Colours::muted,
                 false, juce::Justification::horizontallyCentred);
        }
    }

  private:
    Getter threshold, ratio, attack, release;
};

std::unique_ptr<PlayfulScene> compressor(Getter thresholdDb, Getter ratio,
                                         Getter attackMs, Getter releaseMs) {
    return std::make_unique<CompressorScene>(thresholdDb, ratio, attackMs,
                                             releaseMs);
}

//==============================================================================
// 5 EQ: a balloon party
static float logX(float f) {
    f = juce::jlimit(20.0f, 20000.0f, f);
    return 40 + (std::log10(f) - std::log10(20.0f)) /
                    (std::log10(20000.0f) - std::log10(20.0f)) * 720;
}

class EqualiserScene : public PlayfulScene {
  public:
    EqualiserScene(std::array<Getter, 4> f, std::array<Getter, 4> gn,
                   std::function<float(float)> r)
        : freqs(f), gains(gn), response(r) {}
    String getSubtitle() override { return "balloon party"; }

    void paint(Graphics &g, double t) override {
        auto gy = [](float db) {
            return juce::jlimit(10.0f, 300.0f, 200 - db * 8);
        };

        struct Cloud {
            float x, y, x2, y2, d;
        };
        static const Cloud clouds[] = {{130, 60, 160, 50, 0.4f},
                                       {520, 40, 548, 30, 1.2f},
                                       {700, 90, 724, 80, 2.0f}};
        g.setColour(Colours::panel);
        for (auto &c : clouds) {
            float dy = floatY(t, c.d);
            fillEllipseCentred(g, c.x, c.y + dy, 46, 16);
            fillEllipseCentred(g, c.x2, c.y2 + dy, 28, 16);
        }

        Path zero;
        zero.startNewSubPath(30, 200);
        zero.lineTo(770, 200);
        g.setColour(Colours::dim);
        strokeDashed(g, zero, 3, 6, 8);

        Path curve;
        for (int i = 0; i <= 100; i++) {
            float f = 20.0f * std::pow(1000.0f, i / 100.0f);
            float x = logX(f), y = gy(response(f));
            if (i == 0)
                curve.startNewSubPath(x, y);
            else
                curve.lineTo(x, y);
        }
        g.setColour(Colours::yellow);
        strokePath(g, curve, 6);

        static const Colour cols[] = {Colours::blue, Colours::green,
                                      Colours::cream, Colours::red};
        for (int i = 0; i < 4; i++) {
            float f = freqs[size_t(i)]();
            float x = logX(f);
            float cy = gy(response(f));
            float by = juce::jmax(40.0f, cy - 90);
            float str = cy - by;
            {
                Graphics::ScopedSaveState s(g);
                g.addTransform(
                    AffineTransform::translation(x, by + floatY(t, i * 0.6)));
                Path string;
                string.startNewSubPath(0, 34);
                string.quadraticTo(-6, 50, 0, juce::jmax(36.0f, str));
                g.setColour(Colours::cream);
                g.strokePath(string, juce::PathStrokeType(2));
                g.setColour(cols[i]);
                fillEllipseCentred(g, 0, -6, 30, 36);
                g.fillPath(svg("M-5 28 L5 28 L0 36 Z"));
                g.setColour(Colours::white.withAlpha(0.45f));
                fillEllipseCentred(g, -12, -22, 7, 11);
                drawFace(g, 0, -2, 0.7f, Mood::smile, t + i * 1.1);
            }
            text(g, formatFrequency(f, true), x, 320, 13, Colours::muted, false,
                 juce::Justification::horizontallyCentred);
        }
    }

  private:
    std::array<Getter, 4> freqs, gains;
    std::function<float(float)> response;
};

std::unique_ptr<PlayfulScene> equaliser(std::array<Getter, 4> bandFreqs,
                                        std::array<Getter, 4> bandGains,
                                        std::function<float(float)> response) {
    return std::make_unique<EqualiserScene>(bandFreqs, bandGains, response);
}

//==============================================================================
// 6 Filter: a flower garden with a hedge at the cutoff
class FilterScene : public PlayfulScene {
  public:
    FilterScene(Getter f, std::function<bool()> lp)
        : frequency(f), isLowPass(lp) {}
    String getSubtitle() override {
        return isLowPass() ? "low-pass" : "high-pass";
    }

    void paint(Graphics &g, double t) override {
        float cut = logX(frequency());
        bool lp = isLowPass();

        g.setColour(Colours::panel);
        g.fillRect(0, 290, 800, 38);

        static const Colour petals[] = {Colours::pink,   Colours::red,
                                        Colours::purple, Colours::orange,
                                        Colours::blue,   Colours::cream};
        for (int i = 0; i < 18; i++) {
            float x = 40.0f + i * 42;
            float top = -(90.0f + float((i * 53) % 90));
            bool awake = lp ? (x < cut - 16) : (x > cut + 16);
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::rotation(
                               swayAngle(t, 1.8 + (i % 5) * 0.3, (i % 4) * 0.4))
                               .translated(x, 290));
            Path stem;
            stem.startNewSubPath(0, 0);
            stem.lineTo(0, top);
            g.setColour(Colours::green);
            strokePath(g, stem, 4);
            Path leaf;
            leaf.addEllipse(-1, top * 0.45f - 4, 20, 8);
            g.fillPath(leaf,
                       AffineTransform::rotation(juce::degreesToRadians(-30.0f),
                                                 9, top * 0.45f));
            g.addTransform(AffineTransform::translation(0, top));
            if (awake) {
                g.setColour(petals[i % 6]);
                fillEllipseCentred(g, 0, -12, 9, 9);
                fillEllipseCentred(g, 12, 0, 9, 9);
                fillEllipseCentred(g, 0, 12, 9, 9);
                fillEllipseCentred(g, -12, 0, 9, 9);
                g.setColour(Colours::yellow);
                fillEllipseCentred(g, 0, 0, 8, 8);
            } else {
                g.setColour(Colours::dim);
                fillEllipseCentred(g, 0, -2, 9, 13);
                float zy = -18 - 4 * pingPong(t + i * 0.3, 2.0);
                text(g, "z", 10, zy, 13, Colours::muted);
            }
        }

        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(cut, 0));
        g.setColour(Colours::blue);
        g.fillRoundedRectangle(-14, 150, 28, 140, 14);
        fillEllipseCentred(g, -8, 160, 16, 16);
        fillEllipseCentred(g, 10, 152, 18, 18);
        {
            Graphics::ScopedSaveState snail(g);
            g.addTransform(AffineTransform::translation(0, 132));
            g.setColour(Colours::orange);
            fillEllipseCentred(g, 0, 0, 22, 14);
            g.setColour(Colours::ink);
            g.drawEllipse(-13, -13, 18, 18, 2.5f);
            g.setColour(Colours::orange);
            strokePath(g, svg("M18 -2 Q30 -18 26 -26 M22 -4 Q36 -14 36 -22"),
                       3);
            drawFace(g, 14, 0, 0.4f, Mood::smile, t);
        }
        text(g, formatFrequency(frequency()), 0, 118, 15, Colours::text, true,
             juce::Justification::horizontallyCentred);
    }

  private:
    Getter frequency;
    std::function<bool()> isLowPass;
};

std::unique_ptr<PlayfulScene> filter(Getter frequency,
                                     std::function<bool()> isLowPass) {
    return std::make_unique<FilterScene>(frequency, isLowPass);
}

//==============================================================================
// 7 Pitch shift: a rocket climbs a candy ladder
class PitchShiftScene : public PlayfulScene {
  public:
    explicit PitchShiftScene(Getter s) : semitones(s) {}
    String getSubtitle() override {
        int st = juce::roundToInt(semitones());
        return st > 0 ? "going up" : (st < 0 ? "going down" : "staying put");
    }

    void paint(Graphics &g, double t) override {
        int st = juce::roundToInt(semitones());

        g.setColour(Colours::pink);
        g.fillRoundedRectangle(330, 10, 14, 308, 7);
        g.fillRoundedRectangle(456, 10, 14, 308, 7);
        static const Colour rungColours[] = {Colours::yellow, Colours::green,
                                             Colours::blue,   Colours::purple,
                                             Colours::orange, Colours::red};
        for (int i = -12; i <= 12; i++) {
            float y = 164 - i * 12.4f;
            bool octave = i % 12 == 0;
            g.setColour(octave ? Colours::cream : rungColours[(i + 24) % 6]);
            g.fillRoundedRectangle(344, y - 3, 112, octave ? 7.0f : 5.0f, 3);
        }

        g.setColour(Colours::dim);
        fillEllipseCentred(g, 384, 164, 12, 12);
        fillEllipseCentred(g, 402, 158, 15, 15);
        fillEllipseCentred(g, 418, 166, 11, 11);

        {
            float newY = 164 - juce::jlimit(-12, 12, st) * 12.4f;
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(400, newY + floatY(t)));
            g.setColour(Colours::red);
            g.fillPath(svg("M-24 22 L-38 40 L-20 34 Z M24 22 L38 40 L20 34 Z"));
            g.setColour(Colours::blue);
            g.fillPath(svg("M-24 30 L-24 -12 Q0 -58 24 -12 L24 30 Z"));
            g.setColour(Colours::cream);
            fillEllipseCentred(g, 0, -8, 13, 13);
            float flame =
                (st >= 0 ? 52.0f : 44.0f) + 4.0f * std::sin(float(t) * 25.0f);
            Path f;
            f.startNewSubPath(-12, 32);
            f.quadraticTo(0, flame, 12, 32);
            f.closeSubPath();
            g.setColour(Colours::yellow);
            g.fillPath(f);
            drawFace(g, 0, -6, 0.42f, Mood::smile, t);
        }

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(560, 90));
            g.setColour(Colours::cream);
            g.fillRoundedRectangle(0, 0, 190, 110, 34);
            g.fillPath(svg("M0 110 L-16 130 L26 110 Z"));
            text(g, (st > 0 ? "+" : "") + String(st), 95, 68, 58, Colours::ink,
                 true, juce::Justification::horizontallyCentred);
            text(g, intervalName(st), 95, 96, 14, Colours::ink, false,
                 juce::Justification::horizontallyCentred);
        }
    }

  private:
    Getter semitones;

    static String intervalName(int st) {
        static const char *names[] = {"original",  "minor 2nd", "major 2nd",
                                      "minor 3rd", "major 3rd", "4th",
                                      "tritone",   "5th",       "minor 6th",
                                      "major 6th", "minor 7th", "major 7th"};
        int a = std::abs(st) % 12, octaves = std::abs(st) / 12;
        if (a == 0)
            return octaves == 0 ? "original pitch"
                                : (octaves > 1 ? String(octaves) + " octaves"
                                               : String("octave"));
        return String(names[a]) +
               (octaves > 0 ? " + " + String(octaves) + " oct" : String());
    }
};

std::unique_ptr<PlayfulScene> pitchShift(Getter semitones) {
    return std::make_unique<PitchShiftScene>(semitones);
}

//==============================================================================
// 8 Arpeggiator: a bunny hops across note blocks
class ArpeggiatorScene : public PlayfulScene {
  public:
    ArpeggiatorScene(IntGetter m, Getter r, IntGetter o, Getter gt,
                     std::function<bool()> l)
        : mode(m), rateBeats(r), octaves(o), gate(gt), latch(l) {}
    String getSubtitle() override { return latch() ? "latch on" : "latch off"; }

    void paint(Graphics &g, double t) override {
        int md = mode(), oct = juce::jlimit(1, 4, octaves());
        float gt = juce::jlimit(0.05f, 1.0f, gate());

        juce::Array<int> seq;
        for (int o = 0; o < oct; o++)
            for (int n : {0, 4, 7})
                seq.add(n + 12 * o);
        int n = seq.size();
        static const int randomOrder[] = {0, 5, 2, 7, 1, 4, 8, 3,
                                          6, 2, 0, 5, 7, 1, 3, 4};
        int order[16];
        for (int i = 0; i < 16; i++) {
            int idx = 0;
            switch (md) {
            case 1: // down
                idx = n - 1 - (i % n);
                break;
            case 2: { // up/down
                int per = juce::jmax(1, 2 * (n - 1));
                int pos = i % per;
                idx = n == 1 ? 0 : (pos < n ? pos : per - pos);
                break;
            }
            case 3: // random
                idx = randomOrder[i] % n;
                break;
            default: // up, as played
                idx = i % n;
            }
            order[i] = seq[idx];
        }

        static const Colour octaveColours[] = {Colours::blue, Colours::green,
                                               Colours::yellow, Colours::red};
        const float cell = 40, base = 300, maxH = 170;
        float heights[16];
        for (int i = 0; i < 16; i++) {
            float h = 26 + (order[i] / (12.0f * oct)) * maxH;
            heights[i] = h;
            float w = juce::jmax(10.0f, (cell - 6) * gt);
            float x = 64 + i * cell - w / 2;
            g.setColour(octaveColours[(order[i] / 12) % 4]);
            g.fillRoundedRectangle(x, base - h, w, h, 10);
            g.setColour(Colours::white.withAlpha(0.6f));
            fillEllipseCentred(g, x + w / 2, base - h + 10, 3, 3);
        }

        // the bunny hops along on top of the blocks (120 bpm)
        double stepSeconds = juce::jmax(0.05, rateBeats() * 0.5);
        double steps = t / stepSeconds;
        int step = int(std::fmod(steps, 16.0));
        float within = frac(steps);
        float hop = -22.0f * std::sin(within * juce::MathConstants<float>::pi);
        float bx = 64 + (step + within) * cell;
        float by = base - heights[step] - 28 + hop;
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(bx, by));
            g.setColour(Colours::text);
            fillEllipseCentred(g, -10, -44, 8, 22);
            fillEllipseCentred(g, 10, -44, 8, 22);
            g.setColour(Colours::pink);
            fillEllipseCentred(g, -10, -44, 4, 15);
            fillEllipseCentred(g, 10, -44, 4, 15);
            g.setColour(Colours::text);
            fillEllipseCentred(g, 0, 0, 30, 26);
            fillEllipseCentred(g, -26, 10, 8, 8);
            drawFace(g, 0, -2, 0.75f, Mood::smile, t);
        }
        text(g, utf8("example chord: C · E · G"), 776, 320, 13, Colours::muted,
             false, juce::Justification::right);
    }

  private:
    IntGetter mode;
    Getter rateBeats;
    IntGetter octaves;
    Getter gate;
    std::function<bool()> latch;
};

std::unique_ptr<PlayfulScene> arpeggiator(IntGetter mode, Getter rateBeats,
                                          IntGetter octaves, Getter gate,
                                          std::function<bool()> latch) {
    return std::make_unique<ArpeggiatorScene>(mode, rateBeats, octaves, gate,
                                              latch);
}

//==============================================================================
// 9 Chord: a layer cake, one layer per note
class ChordScene : public PlayfulScene {
  public:
    ChordScene(std::function<juce::Array<int>(int)> n, TextGetter c,
               TextGetter i, TextGetter v)
        : notesFor(n), chordName(c), inversionName(i), voicingName(v) {}
    String getSubtitle() override {
        return played ? "root " + noteName(root) : "play a key";
    }
    void notePlayed(int note) override {
        root = note;
        played = true;
    }

    void paint(Graphics &g, double t) override {
        auto notes = notesFor(root);
        notes.sort();
        if (notes.isEmpty())
            notes.add(root);

        String rootName = noteName(root).trimCharactersAtEnd("-0123456789");
        text(g, rootName + " " + chordName().toLowerCase(), 40, 60, 46,
             Colours::text, true);
        text(g,
             inversionName().toLowerCase() + utf8(" · ") +
                 voicingName().toLowerCase() + " voicing",
             42, 88, 15, Colours::muted);

        g.setColour(Colours::cream);
        g.fillRoundedRectangle(520, 196, 220, 12, 6);
        static const Colour cake[] = {Colours::pink, Colours::yellow,
                                      Colours::green, Colours::blue,
                                      Colours::purple};
        float lh = juce::jmin(36.0f, 150.0f / notes.size());
        for (int i = 0; i < notes.size(); i++) {
            float y = 196 - (i + 1) * lh;
            g.setColour(cake[i % 5]);
            g.fillRoundedRectangle(540, y, 180, lh - 3,
                                   juce::jmin(16.0f, lh / 2));
            Path drip;
            drip.startNewSubPath(540, y + 8);
            drip.quadraticTo(560, y + 20, 580, y + 8);
            drip.quadraticTo(600, y + 24, 620, y + 8);
            drip.lineTo(620, y + 4);
            drip.lineTo(540, y + 4);
            drip.closeSubPath();
            g.setColour(Colours::text);
            g.fillPath(drip);
            text(g, noteName(notes[i]), 630, y + lh / 2 + 5, 15, Colours::ink,
                 true, juce::Justification::horizontallyCentred);
        }
        {
            float cy = 196 - notes.size() * lh - 12 + -3.0f * pingPong(t, 1.6);
            g.setColour(Colours::red);
            fillEllipseCentred(g, 630, cy, 13, 13);
            g.setColour(Colours::green);
            Path stalk;
            stalk.startNewSubPath(630, cy - 12);
            stalk.quadraticTo(636, cy - 28, 648, cy - 30);
            strokePath(g, stalk, 3);
        }

        int first = (notes.getFirst() / 12) * 12;
        keyboard(g, 40, 226, 30, 96, 20, 58, first, 36, [&](int n) {
            int index = notes.indexOf(n);
            return index >= 0 ? cake[index % 5]
                              : juce::Colours::transparentBlack;
        });
    }

  private:
    std::function<juce::Array<int>(int)> notesFor;
    TextGetter chordName, inversionName, voicingName;
    int root = 57; // A3
    bool played = false;
};

std::unique_ptr<PlayfulScene>
chord(std::function<juce::Array<int>(int)> notesFor, TextGetter chordName,
      TextGetter inversionName, TextGetter voicingName) {
    return std::make_unique<ChordScene>(notesFor, chordName, inversionName,
                                        voicingName);
}

//==============================================================================
// 10 Scale lock: a flower whose petals are the notes of the scale
class ScaleLockScene : public PlayfulScene {
  public:
    ScaleLockScene(IntGetter r, TextGetter n, std::function<int(int)> s)
        : root(r), scaleName(n), snap(s) {}
    String getSubtitle() override {
        int count = 0;
        for (int i = 0; i < 12; i++)
            count += snap(60 + i) == 60 + i ? 1 : 0;
        return String(count) + " petals";
    }

    void paint(Graphics &g, double t) override {
        static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                      "F#", "G",  "G#", "A",  "A#", "B"};
        int rt = ((root() % 12) + 12) % 12;
        bool inScale[12];
        for (int i = 0; i < 12; i++)
            inScale[i] = snap(60 + i) == 60 + i;

        static const Colour petalColours[] = {Colours::pink,   Colours::green,
                                              Colours::purple, Colours::orange,
                                              Colours::red,    Colours::blue};
        {
            Graphics::ScopedSaveState s(g);
            float spin = float(std::fmod(t / 40.0, 1.0)) *
                         juce::MathConstants<float>::twoPi;
            int k = 0;
            for (int i = 0; i < 12; i++) {
                bool on = inScale[i];
                float rot = juce::degreesToRadians(i * 30.0f) + spin;
                float cy = on ? -84.0f : -66.0f;
                Colour fill =
                    i == rt ? Colours::blue
                            : (on ? petalColours[(k++) % 6] : Colours::dim);
                auto tr = AffineTransform::rotation(rot).translated(190, 168);
                Path petal;
                petal.addEllipse(on ? -24.0f : -14.0f, cy - (on ? 44 : 30),
                                 on ? 48.0f : 28.0f, on ? 88.0f : 60.0f);
                g.setColour(fill);
                g.fillPath(petal, tr);
                // upright label near the petal tip
                float ty = cy + (on ? -22.0f : -14.0f);
                juce::Point<float> p(0, ty);
                p.applyTransform(tr);
                g.setColour(on ? Colours::ink : Colours::muted);
                g.setFont(font(12, true));
                g.drawText(names[i],
                           juce::Rectangle<float>(p.x - 15, p.y - 8, 30, 16),
                           juce::Justification::centred);
            }
            g.setColour(Colours::yellow);
            fillEllipseCentred(g, 190, 168, 38, 38);
            drawFace(g, 190, 168, 1, Mood::smile, t);
        }

        text(g, String(names[rt]) + " " + scaleName().toLowerCase(), 400, 66,
             38, Colours::text, true);
        text(g, "wrong keys hop to the nearest petal", 402, 94, 14,
             Colours::muted);

        const float kx = 400, ky = 170, ww = 50;
        float keyX[12];
        int wi = 0;
        for (int n = 0; n < 12; n++) {
            if (!isBlackKey(n)) {
                keyX[n] = kx + wi * ww + ww / 2;
                wi++;
            } else {
                keyX[n] = kx + wi * ww;
            }
        }
        keyboard(g, kx, ky, ww, 136, 28, 82, 0, 12, [&](int n) {
            if (!inScale[n])
                return isBlackKey(n) ? Colours::ink : Colours::greyKey;
            if (n == rt)
                return Colours::blue;
            return isBlackKey(n) ? Colours::green : Colours::text;
        });

        g.setColour(Colours::red);
        for (int m = 0; m < 12; m++) {
            if (inScale[m])
                continue;
            int target = snap(60 + m) - 60;
            float x1 = keyX[m];
            float x2 = target >= 12 ? keyX[target - 12] + 7 * ww
                       : target < 0 ? keyX[target + 12] - 7 * ww
                                    : keyX[target];
            Path arc;
            arc.startNewSubPath(x1, ky - 6);
            arc.quadraticTo((x1 + x2) / 2, ky - 46, x2, ky - 6);
            strokeDashed(g, arc, 3, 1, 6);
            fillEllipseCentred(g, x2, ky - 8, 5, 5);
        }
    }

  private:
    IntGetter root;
    TextGetter scaleName;
    std::function<int(int)> snap;
};

std::unique_ptr<PlayfulScene> scaleLock(IntGetter root, TextGetter scaleName,
                                        std::function<int(int)> snap) {
    return std::make_unique<ScaleLockScene>(root, scaleName, snap);
}

//==============================================================================
// Phaser: a swirly jellyfish
class PhaserScene : public PlayfulScene {
  public:
    PhaserScene(Getter d, Getter r, Getter f)
        : depth(d), rate(r), feedback(f) {}
    String getSubtitle() override { return "swirly"; }

    void paint(Graphics &g, double t) override {
        float dp = juce::jlimit(0.0f, 1.0f, depth());
        float rt = juce::jlimit(0.0f, 1.0f, rate());
        float fb = juce::jlimit(0.0f, 1.0f, feedback());

        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(400, 150 + floatY(t)));

        static const Colour tentacleColours[] = {
            Colours::pink, Colours::green, Colours::yellow, Colours::orange,
            Colours::blue};
        for (int i = 0; i < 5; i++) {
            float x = -80.0f + i * 40 + 6.0f * pingPong(t + i * 0.3, 1.8);
            float len = 60 + dp * 60;
            Path p;
            p.startNewSubPath(x, 24);
            p.quadraticTo(x - 16, 24 + len / 3, x, 24 + len * 2 / 3);
            p.quadraticTo(x + 16, 24 + len, x, 24 + len);
            g.setColour(tentacleColours[i]);
            strokePath(g, p, 6);
        }

        static const Path bell =
            svg("M-110 20 Q-110 -100 0 -100 Q110 -100 110 20 Q80 34 55 20 Q28 "
                "34 0 20 Q-28 34 -55 20 Q-80 34 -110 20 Z");
        g.setColour(Colours::purple);
        g.fillPath(bell);

        float spin = float(t) * juce::MathConstants<float>::twoPi /
                     float(4.0 / juce::jmax(0.1, rt * 3.0));
        int rings = 1 + juce::roundToInt(fb * 3);
        for (int i = 0; i < rings; i++) {
            float r = 14 + i * 14 + dp * 14;
            Path ring;
            ring.addCentredArc(0, -40, r, r, spin, 0,
                               juce::MathConstants<float>::twoPi, true);
            g.setColour(Colours::blue.withAlpha(1.0f - i * 0.18f));
            Path dashed;
            float lengths[] = {r * 0.9f, r * 0.5f};
            juce::PathStrokeType(5).createDashedStroke(dashed, ring, lengths,
                                                       2);
            g.fillPath(dashed);
        }
        drawFace(g, 0, -6, 1, Mood::smile, t);
    }

  private:
    Getter depth, rate, feedback;
};

std::unique_ptr<PlayfulScene> phaser(Getter depth01, Getter rate01,
                                     Getter feedback01) {
    return std::make_unique<PhaserScene>(depth01, rate01, feedback01);
}

//==============================================================================
// LFO: a caterpillar crawls along the wave
class LfoScene : public PlayfulScene {
  public:
    LfoScene(IntGetter w, Getter r, Getter d, Getter p)
        : wave(w), rate(r), depth(d), phase(p) {}
    String getSubtitle() override { return "wiggly"; }

    void paint(Graphics &g, double t) override {
        int wv = wave();
        float rt = juce::jlimit(0.0f, 1.0f, rate());
        float dp = juce::jlimit(0.0f, 1.0f, depth());
        float ph = juce::jlimit(0.0f, 1.0f, phase());

        auto w = [&](float u) {
            float x = frac(u + ph);
            switch (wv) {
            case 0:
                return std::sin(x * juce::MathConstants<float>::twoPi);
            case 1:
                return 1 - 4 * std::abs(x - 0.5f);
            case 2:
                return 2 * x - 1;
            case 3:
                return 1 - 2 * x;
            case 4:
                return x < 0.5f ? 1.0f : -1.0f;
            default:
                return std::sin(x * 37.1f) * std::cos(x * 13.7f);
            }
        };
        float amp = 20 + dp * 90;
        const float cycles = 2;
        auto yAt = [&](float x) {
            return 170 - w((x - 40) / 720 * cycles) * amp;
        };

        Path path;
        for (int i = 0; i <= 160; i++) {
            float x = 40 + i / 160.0f * 720;
            if (i == 0)
                path.startNewSubPath(x, yAt(x));
            else
                path.lineTo(x, yAt(x));
        }
        g.setColour(Colours::dim);
        strokePath(g, path, 10);

        // crawl back and forth, faster with a higher rate
        double duration = 4.0 / juce::jmax(0.1, double(rt) * 5.0);
        float hx = 420 - 30 + 60 * (1 - std::abs(1 - 2 * loop(t, duration)));
        static const Colour segColours[] = {Colours::yellow, Colours::green,
                                            Colours::pink, Colours::green,
                                            Colours::yellow};
        for (int s = 5; s >= 1; s--) {
            float x = hx - s * 26;
            g.setColour(segColours[s - 1]);
            fillEllipseCentred(g, x, yAt(x), 16, 16);
        }
        Graphics::ScopedSaveState st(g);
        g.addTransform(AffineTransform::translation(hx, yAt(hx)));
        g.setColour(Colours::green);
        fillEllipseCentred(g, 0, 0, 24, 24);
        strokePath(g, svg("M-8 -22 Q-14 -40 -20 -42 M8 -22 Q14 -40 20 -42"), 3);
        g.setColour(Colours::pink);
        fillEllipseCentred(g, -20, -42, 4, 4);
        fillEllipseCentred(g, 20, -42, 4, 4);
        drawFace(g, 0, 0, 0.7f, Mood::smile, t);
    }

  private:
    IntGetter wave;
    Getter rate, depth, phase;
};

std::unique_ptr<PlayfulScene> lfo(IntGetter wave, Getter rate01, Getter depth01,
                                  Getter phase01) {
    return std::make_unique<LfoScene>(wave, rate01, depth01, phase01);
}

//==============================================================================
// Random: a raccoon rolls dice
static float hash01(int a, int b) {
    uint32_t h = uint32_t(a) * 374761393u + uint32_t(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

class RandomScene : public PlayfulScene {
  public:
    RandomScene(TextGetter r, Getter d, Getter s)
        : rateText(r), depth(d), smooth(s) {}
    String getSubtitle() override { return "rolling " + rateText(); }

    void paint(Graphics &g, double t) override {
        float dp = juce::jlimit(0.0f, 1.0f, depth());
        float sm = juce::jlimit(0.0f, 1.0f, smooth());

        // a new die is rolled every 0.6s and the row scrolls along
        int roll = int(t / 0.6);
        float vals[8];
        for (int i = 0; i < 8; i++)
            vals[i] = hash01(roll + i, 7);

        juce::Point<float> pts[8];
        for (int i = 0; i < 8; i++)
            pts[i] = {60.0f + i * 78, 230 - vals[i] * 170 * dp};
        Path trail;
        trail.startNewSubPath(pts[0]);
        for (int j = 1; j < 8; j++) {
            auto a = pts[j - 1], b = pts[j];
            if (sm < 0.15f) {
                trail.lineTo(b.x, a.y);
                trail.lineTo(b);
            } else {
                float c = 39 * sm * 2;
                trail.cubicTo(a.x + c, a.y, b.x - c, b.y, b.x, b.y);
            }
        }
        g.setColour(Colours::green);
        strokePath(g, trail, 6);

        static const Colour diceColours[] = {
            Colours::pink,   Colours::yellow, Colours::blue, Colours::orange,
            Colours::purple, Colours::green,  Colours::red,  Colours::cream};
        static const int pipSets[6][6][2] = {
            {{0, 0}},
            {{-7, -7}, {7, 7}},
            {{-7, -7}, {0, 0}, {7, 7}},
            {{-7, -7}, {7, -7}, {-7, 7}, {7, 7}},
            {{-7, -7}, {7, -7}, {0, 0}, {-7, 7}, {7, 7}},
            {{-7, -7}, {7, -7}, {-7, 0}, {7, 0}, {-7, 7}, {7, 7}}};
        for (int i = 0; i < 8; i++) {
            float p = pingPong(t + i * 0.3, 2.4);
            Graphics::ScopedSaveState s(g);
            g.addTransform(
                AffineTransform::rotation(juce::degreesToRadians(-8 + 16 * p))
                    .translated(pts[i].x, pts[i].y - 26 - 6 * p));
            g.setColour(diceColours[(roll + i) % 8]);
            g.fillRoundedRectangle(-16, -16, 32, 32, 8);
            g.setColour(Colours::ink);
            int count = 1 + juce::roundToInt(vals[i] * 5);
            for (int k = 0; k < count; k++)
                fillEllipseCentred(g, float(pipSets[count - 1][k][0]),
                                   float(pipSets[count - 1][k][1]), 3.2f, 3.2f);
        }

        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(690, 250 + floatY(t)));
        g.setColour(Colours::muted);
        fillEllipseCentred(g, 0, 10, 40, 34);
        g.fillPath(
            svg("M-34 -16 L-28 -44 L-12 -26 Z M34 -16 L28 -44 L12 -26 Z"));
        g.fillPath(svg("M36 30 Q70 24 70 0 Q76 30 44 40 Z"));
        g.setColour(Colours::ink.withAlpha(0.85f));
        g.fillPath(svg("M-36 -2 Q0 -14 36 -2 L36 10 Q0 0 -36 10 Z"));
        drawFace(g, 0, 4, 0.8f, Mood::smile, t);
        g.setColour(Colours::ink);
        g.strokePath(svg("M54 30 L64 26 M60 18 L68 14"),
                     juce::PathStrokeType(3));
    }

  private:
    TextGetter rateText;
    Getter depth, smooth;
};

std::unique_ptr<PlayfulScene> random(TextGetter rateText, Getter depth01,
                                     Getter smooth01) {
    return std::make_unique<RandomScene>(rateText, depth01, smooth01);
}

//==============================================================================
// Envelope follower: a goat climbs hills made of loudness
class EnvelopeScene : public PlayfulScene {
  public:
    EnvelopeScene(Getter d, Getter a, Getter r, Getter gn)
        : depth(d), attack(a), release(r), gain(gn) {}
    String getSubtitle() override { return "following loudness"; }

    void paint(Graphics &g, double t) override {
        float dp = juce::jlimit(0.0f, 1.0f, depth());
        float atk = juce::jmax(0.0f, attack()),
              rel = juce::jmax(0.0f, release());
        float gn = gain();

        static const float raw[] = {0.1f,  0.8f, 0.3f,  0.15f, 0.9f, 0.6f,
                                    0.2f,  0.1f, 0.7f,  0.95f, 0.4f, 0.2f,
                                    0.05f, 0.5f, 0.85f, 0.3f};
        float env[16];
        float e = 0;
        float a = 1 - std::exp(-1 / (1 + atk / 40)),
              r = 1 - std::exp(-1 / (1 + rel / 80));
        for (int i = 0; i < 16; i++) {
            float x = raw[i] * std::pow(10.0f, gn / 20);
            e += (x > e ? a : r) * (x - e);
            env[i] = e;
        }
        float scale = 200 * dp + 20;
        auto makePath = [](const float *arr, float sc, bool close) {
            Path p;
            p.startNewSubPath(40, 300);
            for (int i = 0; i < 16; i++)
                p.lineTo(40.0f + i * 48, 300 - juce::jmin(1.0f, arr[i]) * sc);
            if (close) {
                p.lineTo(760, 300);
                p.closeSubPath();
            }
            return p;
        };
        g.setColour(Colours::panel);
        g.fillPath(makePath(raw, 200, true));
        g.setColour(Colours::green.withAlpha(0.9f));
        g.fillPath(makePath(env, scale, true));
        g.setColour(Colours::cream);
        g.strokePath(makePath(env, scale, false),
                     juce::PathStrokeType(4, juce::PathStrokeType::curved));

        // the goat walks along the hill
        float pos = loop(t, 12.0) * 15.0f;
        int i0 = int(pos);
        float u = pos - i0;
        float y0 = 300 - juce::jmin(1.0f, env[i0]) * scale;
        float y1 = 300 - juce::jmin(1.0f, env[juce::jmin(15, i0 + 1)]) * scale;
        float gx = 40 + pos * 48, gy = y0 + (y1 - y0) * u;
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(gx, gy - 8));
            g.setColour(Colours::text);
            fillEllipseCentred(g, 0, -18, 26, 18);
            strokePath(g, svg("M-14 -2 L-14 10 M14 -2 L14 10"), 5);
            g.setColour(Colours::orange);
            strokePath(
                g, svg("M-14 -34 Q-24 -52 -10 -54 M14 -34 Q24 -52 10 -54"), 4);
            drawFace(g, 0, -20, 0.6f, Mood::smile, t);
        }

        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::translation(610, 40));
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(0, 0, 160, 56, 18);
        text(g, "attack", 16, 24, 12, Colours::muted);
        text(g, String(juce::roundToInt(atk)) + " ms", 16, 44, 16,
             Colours::cream);
        text(g, "release", 88, 24, 12, Colours::muted);
        text(g, String(juce::roundToInt(rel)) + " ms", 88, 44, 16,
             Colours::red);
    }

  private:
    Getter depth, attack, release, gain;
};

std::unique_ptr<PlayfulScene> envelopeFollower(Getter depth01, Getter attackMs,
                                               Getter releaseMs,
                                               Getter gainDb) {
    return std::make_unique<EnvelopeScene>(depth01, attackMs, releaseMs,
                                           gainDb);
}

//==============================================================================
// MIDI tracker: a frog sits on the key being played
class MidiTrackerScene : public PlayfulScene {
  public:
    MidiTrackerScene(IntGetter s, TextGetter m, IntGetter r, IntGetter sp)
        : source(s), mode(m), root(r), spread(sp) {}
    String getSubtitle() override {
        return String(source() == 0 ? "pitch" : "velocity") + utf8(" · ") +
               mode().toLowerCase();
    }
    void notePlayed(int n) override { note = n; }

    void paint(Graphics &g, double t) override {
        bool pitch = source() == 0;
        int rt = root(), sp = juce::jmax(1, spread());
        int first = 48;
        // keep the played key on screen
        while (note < first)
            first -= 12;
        while (note >= first + 36)
            first += 12;

        float frogX = 40;
        int wi = 0;
        for (int n = first; n < first + 36; n++) {
            if (!isBlackKey(n)) {
                if (n == note)
                    frogX = 40 + wi * 30 + 15.0f;
                wi++;
            }
        }
        keyboard(g, 40, 210, 30, 96, 20, 58, first, 36, [&](int n) {
            if (n == note)
                return Colours::green;
            if (n == rt && !isBlackKey(n))
                return Colours::yellow;
            return juce::Colours::transparentBlack;
        });

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(
                AffineTransform::translation(frogX, 196 + floatY(t)));
            g.setColour(Colours::green);
            fillEllipseCentred(g, 0, -10, 30, 22);
            fillEllipseCentred(g, -14, -30, 11, 11);
            fillEllipseCentred(g, 14, -30, 11, 11);
            drawFace(g, 0, -28, 0.7f, Mood::smile, t);
        }

        float v = pitch ? juce::jlimit(0.0f, 1.0f,
                                       0.5f + float(note - rt) / (2.0f * sp))
                        : 96.0f / 127.0f;
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(700, 30));
            g.setColour(Colours::panel);
            g.fillRoundedRectangle(0, 0, 44, 160, 22);
            g.setColour(Colours::blue);
            float h = v * 160;
            g.fillRoundedRectangle(0, 160 - h, 44, h, juce::jmin(22.0f, h / 2));
            text(g, "knob", 22, 182, 12, Colours::muted, false,
                 juce::Justification::horizontallyCentred);
        }

        String name = noteName(note);
        text(g, name, 40, 60, 34, Colours::text, true);
        float ax = 40 + font(34, true).getStringWidthFloat(name) + 14;
        Path arrow;
        arrow.startNewSubPath(ax, 48);
        arrow.lineTo(ax + 28, 48);
        arrow.startNewSubPath(ax + 18, 38);
        arrow.lineTo(ax + 28, 48);
        arrow.lineTo(ax + 18, 58);
        g.setColour(Colours::muted);
        strokePath(g, arrow, 4);
        text(g, percent(v), ax + 42, 60, 34, Colours::text, true);
        text(g,
             pitch ? "higher notes turn the knob further"
                   : "harder hits turn the knob further",
             42, 86, 14, Colours::muted);
    }

  private:
    IntGetter source;
    TextGetter mode;
    IntGetter root, spread;
    int note = 67;
};

std::unique_ptr<PlayfulScene> midiTracker(IntGetter source, TextGetter mode,
                                          IntGetter rootNote,
                                          IntGetter spread) {
    return std::make_unique<MidiTrackerScene>(source, mode, rootNote, spread);
}

//==============================================================================
// Anything else: a friendly blob
class GenericScene : public PlayfulScene {
  public:
    explicit GenericScene(TextGetter s) : subtitle(s) {}
    String getSubtitle() override { return subtitle ? subtitle() : String(); }

    void paint(Graphics &g, double t) override {
        auto sq = bounce(loop(t, 1.6));
        g.setColour(Colours::panel);
        fillEllipseCentred(g, 400, 284, 90 * sq.sx, 12);
        Graphics::ScopedSaveState s(g);
        g.addTransform(AffineTransform::scale(sq.sx, sq.sy)
                           .translated(0, sq.dy * 0.6f)
                           .translated(400, 280));
        static const Path blob =
            svg("M-80 0 Q-96 -60 -60 -110 Q0 -150 60 -110 Q96 -60 80 0 Q0 16 "
                "-80 0 Z");
        g.setColour(Colours::pink);
        g.fillPath(blob);
        g.setColour(Colours::white.withAlpha(0.4f));
        fillEllipseCentred(g, -40, -100, 14, 8);
        drawFace(g, 0, -60, 1.4f, Mood::smile, t);
    }

  private:
    TextGetter subtitle;
};

std::unique_ptr<PlayfulScene> generic(TextGetter subtitle) {
    return std::make_unique<GenericScene>(subtitle);
}

} // namespace PlayfulScenes

namespace PlayfulScenes {

//==============================================================================
// 4OSC oscillator: a blob whose outline is the waveform, with clones for the
// extra voices
class OscillatorScene : public PlayfulScene {
  public:
    OscillatorScene(IntGetter w, IntGetter v, TextGetter t)
        : wave(w), voices(v), tuneText(t) {}
    String getSubtitle() override {
        int v = voices();
        return String(v) + (v == 1 ? " voice" : " voices");
    }

    void paint(Graphics &g, double t) override {
        int wv = juce::jlimit(0, 5, wave());
        int vc = juce::jlimit(1, 8, voices());
        float flow = float(t) * 0.25f;

        auto edge = [&](float u) {
            u = frac(u + flow);
            switch (wv) {
            case 1:
                return std::sin(u * juce::MathConstants<float>::twoPi);
            case 2:
                return frac(u * 2) < 0.5f ? 1.0f : -1.0f;
            case 3:
                return 2 * frac(u * 2) - 1;
            case 4:
                return 1 - 4 * std::abs(frac(u + 0.25f) - 0.5f);
            case 5:
                return std::sin(u * 91.3f + float(t) * 7) *
                       std::cos(u * 47.9f - float(t) * 5);
            default:
                return 0.0f;
            }
        };
        Path body;
        body.startNewSubPath(-110, 60);
        for (int i = 0; i <= 60; i++) {
            float u = i / 60.0f;
            body.lineTo(-110 + u * 220, -60 + edge(u) * 14);
        }
        body.lineTo(110, 60);
        body.quadraticTo(0, 86, -110, 60);
        body.closeSubPath();

        static const Colour cloneColours[] = {
            Colours::pink,   Colours::purple, Colours::green, Colours::orange,
            Colours::yellow, Colours::red,    Colours::cream};
        int clones = juce::jmin(6, vc - 1);
        for (int c = 0; c < clones; c++) {
            int side = c % 2 ? 1 : -1;
            int k = c / 2 + 1;
            float sc = 0.55f - k * 0.08f;
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::scale(sc).translated(
                400.0f + side * k * 120, 200.0f + k * 10 + floatY(t, c * 0.4)));
            g.setColour(cloneColours[c % 7].withAlpha(0.9f - k * 0.15f));
            g.fillPath(body);
        }

        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(400, 170 + floatY(t)));
            g.setColour(wv == 0 ? Colours::dim : Colours::blue);
            g.fillPath(body);
            drawFace(g, 0, 10, 1.1f, wv == 0 ? Mood::sleep : Mood::sing, t);
        }

        static const char *names[] = {"off",           "sine wave",
                                      "square wave",   "saw wave",
                                      "triangle wave", "noise"};
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(40, 40, 190, 48, 16);
        text(g, names[wv], 56, 71, 18, Colours::text, true);
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(580, 40, 180, 48, 16);
        text(g, tuneText(), 596, 71, 18, Colours::text, true);
    }

  private:
    IntGetter wave, voices;
    TextGetter tuneText;
};

std::unique_ptr<PlayfulScene> oscillator(IntGetter wave, IntGetter voices,
                                         TextGetter tuneText) {
    return std::make_unique<OscillatorScene>(wave, voices, tuneText);
}

//==============================================================================
// 4OSC envelope: a penguin sleds down the envelope
class AmpEnvelopeScene : public PlayfulScene {
  public:
    AmpEnvelopeScene(Getter a, Getter d, Getter s, Getter r)
        : attack(a), decay(d), sustain(s), release(r) {}
    String getSubtitle() override { return "amp envelope"; }

    void paint(Graphics &g, double t) override {
        float at = juce::jlimit(0.0f, 1.0f, attack());
        float de = juce::jlimit(0.0f, 1.0f, decay());
        float su = juce::jlimit(0.0f, 1.0f, sustain());
        float re = juce::jlimit(0.0f, 1.0f, release());

        const float base = 290, top = 60, x0 = 40;
        float x1 = x0 + 40 + at * 150, x2 = x1 + 40 + de * 150, x3 = x2 + 140;
        float x4 = juce::jmin(760.0f, x3 + 40 + re * 200);
        float ys = base - (base - top) * su;

        Path route;
        route.startNewSubPath(x0, base);
        route.lineTo(x1, top);
        route.lineTo(x2, ys);
        route.lineTo(x3, ys);
        route.lineTo(x4, base);
        Path hill(route);
        hill.closeSubPath();
        g.setColour(Colours::panel);
        g.fillPath(hill);

        auto segment = [&](float ax, float ay, float bx, float by, Colour c) {
            Path p;
            p.startNewSubPath(ax, ay);
            p.lineTo(bx, by);
            g.setColour(c);
            strokePath(g, p, 8);
        };
        segment(x0, base, x1, top, Colours::blue);
        segment(x1, top, x2, ys, Colours::green);
        segment(x2, ys, x3, ys, Colours::cream);
        segment(x3, ys, x4, base, Colours::red);

        double duration = 2 + at * 2 + de * 2 + re * 2;
        auto pos =
            route.getPointAlongPath(route.getLength() * loop(t, duration));
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(AffineTransform::translation(pos.x, pos.y - 26));
            g.setColour(Colours::ink);
            fillEllipseCentred(g, 0, 0, 20, 24);
            g.setColour(Colours::text);
            g.drawEllipse(-20, -24, 40, 48, 2);
            fillEllipseCentred(g, 0, 6, 13, 16);
            g.setColour(Colours::orange);
            g.fillPath(svg("M-5 -2 L5 -2 L0 5 Z"));
            g.setColour(Colours::text);
            float blinkScale =
                frac(t / 4.2) > 0.92 && frac(t / 4.2) < 0.98 ? 0.15f : 1.0f;
            fillEllipseCentred(g, -5.4f, -10, 2, 2.7f * blinkScale);
            fillEllipseCentred(g, 5.4f, -10, 2, 2.7f * blinkScale);
            g.setColour(Colours::pink);
            strokePath(g, svg("M-22 24 L22 24"), 5);
        }

        const char *letters[] = {"A", "D", "S", "R"};
        float centres[] = {(x0 + x1) / 2, (x1 + x2) / 2, (x2 + x3) / 2,
                           (x3 + x4) / 2};
        Colour cols[] = {Colours::blue, Colours::green, Colours::cream,
                         Colours::red};
        for (int i = 0; i < 4; i++)
            text(g, letters[i], centres[i], 316, 14, cols[i], true,
                 juce::Justification::horizontallyCentred);
    }

  private:
    Getter attack, decay, sustain, release;
};

std::unique_ptr<PlayfulScene> ampEnvelope(Getter attack, Getter decay,
                                          Getter sustain, Getter release) {
    return std::make_unique<AmpEnvelopeScene>(attack, decay, sustain, release);
}

//==============================================================================
// 4OSC filter: an owl perches on the resonance peak
class OscFilterScene : public PlayfulScene {
  public:
    OscFilterScene(Getter f, Getter r, Getter e, IntGetter ty)
        : frequency(f), resonance(r), envAmount(e), type(ty) {}
    String getSubtitle() override {
        return "env " + percent(juce::jlimit(0.0f, 1.0f, envAmount()));
    }

    void paint(Graphics &g, double t) override {
        float f = frequency();
        float res = juce::jlimit(0.0f, 1.0f, resonance());
        int ty = juce::jlimit(0, 4, type());
        float cx = logX(f), peak = ty == 0 ? 0 : res * 120;

        auto yAt = [&](float x) {
            float u = (x - cx) / 80;
            float gain = 0;
            switch (ty) {
            case 1:
                gain = x < cx ? 0 : -juce::jmin(1.0f, u) * 2;
                break;
            case 2:
                gain = x > cx ? 0 : -juce::jmin(1.0f, -u) * 2;
                break;
            case 3:
                gain = -juce::jmin(1.0f, std::abs(u));
                break;
            case 4:
                gain = -(1 - juce::jmin(1.0f, std::abs(u))) * 2;
                break;
            default:
                break;
            }
            float bump = ty == 4 ? 0 : peak * std::exp(-u * u * 3);
            return juce::jlimit(40.0f, 300.0f, 150 - gain * 80 - bump);
        };

        Path curve;
        for (int i = 0; i <= 120; i++) {
            float x = 40.0f + i * 6;
            if (i == 0)
                curve.startNewSubPath(x, yAt(x));
            else
                curve.lineTo(x, yAt(x));
        }
        Path fill(curve);
        fill.lineTo(760, 300);
        fill.lineTo(40, 300);
        fill.closeSubPath();
        g.setColour(Colours::panel);
        g.fillPath(fill);
        g.setColour(Colours::blue);
        strokePath(g, curve, 7);

        float perch = yAt(cx);
        {
            Graphics::ScopedSaveState s(g);
            g.addTransform(
                AffineTransform::translation(cx, perch + 6 + floatY(t)));
            g.setColour(Colours::purple);
            fillEllipseCentred(g, 0, -26, 26, 28);
            g.fillPath(
                svg("M-22 -48 L-14 -62 L-8 -48 Z M22 -48 L14 -62 L8 -48 Z"));
            g.setColour(Colours::cream);
            fillEllipseCentred(g, -10, -32, 10, 10);
            fillEllipseCentred(g, 10, -32, 10, 10);
            g.setColour(Colours::ink);
            float open =
                frac(t / 4.2) > 0.92 && frac(t / 4.2) < 0.98 ? 0.15f : 1.0f;
            fillEllipseCentred(g, -10, -32, 5, 5 * open);
            fillEllipseCentred(g, 10, -32, 5, 5 * open);
            g.setColour(Colours::orange);
            g.fillPath(svg("M-4 -22 L4 -22 L0 -16 Z"));
            strokePath(g, svg("M-8 0 L-8 6 M8 0 L8 6"), 3);
        }

        static const char *names[] = {"filter off", "low-pass", "high-pass",
                                      "band-pass", "notch"};
        g.setColour(Colours::panel);
        g.fillRoundedRectangle(40, 20, 220, 48, 16);
        text(g, names[ty], 56, 51, 18, Colours::text, true);
        text(g, formatFrequency(f), cx, juce::jmax(16.0f, perch - 72), 14,
             Colours::text, false, juce::Justification::horizontallyCentred);
    }

  private:
    Getter frequency, resonance, envAmount;
    IntGetter type;
};

std::unique_ptr<PlayfulScene> oscFilter(Getter frequencyHz, Getter resonance01,
                                        Getter envAmount01, IntGetter type) {
    return std::make_unique<OscFilterScene>(frequencyHz, resonance01,
                                            envAmount01, type);
}

//==============================================================================
class PagedScene : public PlayfulScene {
  public:
    PagedScene(std::vector<std::unique_ptr<PlayfulScene>> s,
               juce::StringArray t)
        : scenes(std::move(s)), titles(t) {}

    PlayfulScene *current() {
        if (scenes.empty())
            return nullptr;
        return scenes[size_t(juce::jlimit(0, int(scenes.size()) - 1, page))]
            .get();
    }
    void pageChanged(int p) override { page = p; }
    String getTitle() override { return titles[page]; }
    String getSubtitle() override {
        return current() != nullptr ? current()->getSubtitle() : String();
    }
    void notePlayed(int note) override {
        if (current() != nullptr)
            current()->notePlayed(note);
    }
    void paint(Graphics &g, double t) override {
        if (current() != nullptr)
            current()->paint(g, t);
    }

  private:
    std::vector<std::unique_ptr<PlayfulScene>> scenes;
    juce::StringArray titles;
    int page = 0;
};

std::unique_ptr<PlayfulScene>
paged(std::vector<std::unique_ptr<PlayfulScene>> scenes,
      juce::StringArray titles) {
    return std::make_unique<PagedScene>(std::move(scenes), titles);
}

} // namespace PlayfulScenes
