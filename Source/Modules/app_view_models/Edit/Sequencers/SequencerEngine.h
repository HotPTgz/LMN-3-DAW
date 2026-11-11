#pragma once

// The note generators behind the sequencers. Everything here is plain data
// in, notes out (times in beats from the start of the clip), so it can be
// tested without an edit.
namespace app_view_models::Sequencers {

struct NoteEvent {
    int pitch = 60;
    double start = 0.0;   // beats
    double length = 0.25; // beats
    int velocity = 100;
};

// Beats taken by one step with this many steps per 4/4 bar
inline double stepBeats(int stepsPerBar) {
    return 4.0 / double(juce::jmax(1, stepsPerBar));
}

// Swing pushes every other step later by up to half a step
double swingOffset(int slot, double swing, double stepLength);

//==============================================================================
// Step: a grid of up to 24 notes x 16 steps, with per-step tricks
enum class Direction { forward = 0, backward, pingPong, random };
juce::String directionName(Direction d);

struct StepTricks {
    int velocity = 100; // 1-127
    int length = 1;     // in steps (ties into the next steps)
    float chance = 1.0f;
    int ratchet = 1; // 1-4 hits inside the step
};

struct StepGroove {
    double swing = 0.0; // 0-1
    double gate = 0.9;  // 0.1-1, how much of the step a note fills
    Direction direction = Direction::forward;
    double humanize = 0.0; // 0-1
};

struct StepInput {
    // notes[step] = pitches on that step
    std::array<juce::Array<int>, 16> notes;
    std::array<StepTricks, 16> tricks;
    int length = 16;
    int stepsPerBar = 4;
    StepGroove groove;
};

// How many passes of the pattern the clip holds: more than one when chance,
// random order or humanize make every pass different, and two for ping-pong
int stepPasses(const StepInput &input);
// The step played in a given slot of a given pass
int stepForSlot(const StepInput &input, int pass, int slot, juce::Random &r);
std::vector<NoteEvent> generateStep(const StepInput &input,
                                    juce::int64 seed = 1);

//==============================================================================
// Drums: a row per kit sound, up to 64 steps, hits at 3 strengths
constexpr int drumRows = 24;
constexpr int drumMaxSteps = 64;
constexpr int firstDrumNote = 53; // the first key of a drum kit

struct DrumGrid {
    // cell values: 0 off, 1 ghost, 2 normal, 3 accent
    std::array<std::array<juce::uint8, drumMaxSteps>, drumRows> cells{};

    juce::String toString() const;
    static DrumGrid fromString(const juce::String &s);
    bool isEmpty() const;
};

struct DrumInput {
    DrumGrid main, fill;
    int length = 16;      // steps: 16, 32, 48 or 64
    int stepsPerBar = 16; // 4, 8, 16 or 32
    double swing = 0.0;
    int fillEvery = 0; // 0 = no fill, else the fill replaces every Nth pass
    std::array<float, drumRows> levels;
    DrumInput() { levels.fill(1.0f); }
};

int drumVelocity(int cell, float level);
int drumPasses(const DrumInput &input);
std::vector<NoteEvent> generateDrums(const DrumInput &input);

//==============================================================================
// Endless: a loop of notes and rests played one after another
struct EndlessInput {
    juce::Array<int> notes;   // -1 = rest
    double stepLength = 0.25; // beats
    double gate = 0.6;
    double swing = 0.0;
    int transpose = 0;
};

std::vector<NoteEvent> generateEndless(const EndlessInput &input);
double endlessLength(const EndlessInput &input);

//==============================================================================
// Tombola: balls bouncing inside a spinning many-sided shape; every hit on a
// wall plays the ball's note. The simulation is the same every time for the
// same settings, so the clip and the picture always agree.
struct TombolaInput {
    juce::Array<int> balls; // the note of each ball, up to 8
    double spin = 0.4;      // -1 to 1 (turns per bar, either way)
    double gravity = 0.35;  // 0-1
    int sides = 6;          // 3-8
    double bounce = 0.7;    // 0-1
    double lengthBeats = 16.0;
    int seed = 0; // a shake: different starting throws
};

struct TombolaFrame {
    double angle = 0.0;                    // the shape's rotation, radians
    std::vector<juce::Point<float>> balls; // in shape units (radius 1)
};

class TombolaSimulation {
  public:
    explicit TombolaSimulation(const TombolaInput &input);

    const std::vector<NoteEvent> &getNotes() const { return notes; }
    // where everything is at a time in beats (wraps around the loop)
    TombolaFrame frameAt(double beat) const;
    // hits per ball, for the screen
    const std::vector<int> &getHitCounts() const { return hitCounts; }

    static constexpr double stepsPerBeat = 96.0;
    static constexpr float ballRadius = 0.09f;

  private:
    TombolaInput input;
    std::vector<NoteEvent> notes;
    std::vector<std::vector<juce::Point<float>>> positions; // [frame][ball]
    std::vector<int> hitCounts;
};

//==============================================================================
// Pattern: four 16-step patterns, A to D, played in a chain
constexpr int patternSteps = 16;
constexpr int patternCount = 4;

struct PatternData {
    // steps[step] = pitches
    std::array<juce::Array<int>, patternSteps> steps;
    juce::String toString() const;
    static PatternData fromString(const juce::String &s);
    bool isEmpty() const;
};

struct PatternInput {
    std::array<PatternData, patternCount> patterns;
    juce::Array<int> chain; // pattern indices, at least one
    int stepsPerBar = 16;
};

std::vector<NoteEvent> generatePattern(const PatternInput &input);
double patternLength(const PatternInput &input);
juce::String chainToString(const juce::Array<int> &chain);
juce::Array<int> chainFromString(const juce::String &s);

} // namespace app_view_models::Sequencers
