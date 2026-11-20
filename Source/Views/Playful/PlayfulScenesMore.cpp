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

} // namespace PlayfulScenes
