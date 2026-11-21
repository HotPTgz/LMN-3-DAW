#include "SequencerEngine.h"

namespace app_view_models::Sequencers {

double swingOffset(int slot, double swing, double stepLength) {
    return (slot % 2 == 1) ? juce::jlimit(0.0, 1.0, swing) * 0.5 * stepLength
                           : 0.0;
}

juce::String directionName(Direction d) {
    switch (d) {
    case Direction::backward:
        return "backward";
    case Direction::pingPong:
        return "ping-pong";
    case Direction::random:
        return "random";
    case Direction::forward:
    default:
        return "forward";
    }
}

//==============================================================================
int stepPasses(const StepInput &input) {
    int passes = input.groove.direction == Direction::pingPong ? 2 : 1;
    bool varies = input.groove.direction == Direction::random ||
                  input.groove.humanize > 0.0;
    for (int s = 0; s < juce::jlimit(1, 16, input.length) && !varies; s++)
        if (!input.notes[size_t(s)].isEmpty() &&
            input.tricks[size_t(s)].chance < 1.0f)
            varies = true;
    return varies ? 4 : passes;
}

int stepForSlot(const StepInput &input, int pass, int slot, juce::Random &r) {
    int length = juce::jlimit(1, 16, input.length);
    switch (input.groove.direction) {
    case Direction::backward:
        return length - 1 - slot;
    case Direction::pingPong:
        return pass % 2 == 0 ? slot : length - 1 - slot;
    case Direction::random:
        return r.nextInt(length);
    case Direction::forward:
    default:
        return slot;
    }
}

std::vector<NoteEvent> generateStep(const StepInput &input, juce::int64 seed) {
    std::vector<NoteEvent> events;
    juce::Random random(seed);
    int length = juce::jlimit(1, 16, input.length);
    double stepLength = stepBeats(input.stepsPerBar);
    double gate = juce::jlimit(0.1, 1.0, input.groove.gate);
    double humanize = juce::jlimit(0.0, 1.0, input.groove.humanize);
    int passes = stepPasses(input);
    const double clipLength = passes * length * stepLength;

    for (int pass = 0; pass < passes; pass++) {
        for (int slot = 0; slot < length; slot++) {
            int step = stepForSlot(input, pass, slot, random);
            const auto &pitches = input.notes[size_t(step)];
            if (pitches.isEmpty())
                continue;
            const auto &tricks = input.tricks[size_t(step)];
            if (tricks.chance < 1.0f &&
                random.nextFloat() >= juce::jmax(0.0f, tricks.chance))
                continue;

            double start = (pass * length + slot) * stepLength +
                           swingOffset(slot, input.groove.swing, stepLength);
            if (humanize > 0.0)
                start =
                    juce::jmax(0.0, start + (random.nextDouble() - 0.5) *
                                                humanize * 0.2 * stepLength);
            int ratchet = juce::jlimit(1, 4, tricks.ratchet);
            int tie = juce::jlimit(1, 16, tricks.length);

            for (int hit = 0; hit < ratchet; hit++) {
                double hitStart = start + hit * stepLength / ratchet;
                // swing and ratchets can push the last hits of the last step
                // past the end of the clip, where they'd never play: they
                // come round at the start of the loop instead
                if (hitStart >= clipLength)
                    hitStart -= clipLength;
                double hitLength =
                    ratchet == 1 ? (tie - 1) * stepLength + stepLength * gate
                                 : stepLength / ratchet * gate;
                int velocity = tricks.velocity;
                if (humanize > 0.0)
                    velocity += juce::roundToInt((random.nextFloat() - 0.5f) *
                                                 humanize * 40.0);
                for (auto pitch : pitches)
                    events.push_back({juce::jlimit(0, 127, pitch), hitStart,
                                      hitLength,
                                      juce::jlimit(1, 127, velocity)});
            }
        }
    }
    return events;
}

//==============================================================================
juce::String DrumGrid::toString() const {
    juce::StringArray rows;
    for (auto &row : cells) {
        juce::String text;
        for (auto cell : row)
            text << juce::String(int(juce::jlimit<juce::uint8>(0, 3, cell)));
        rows.add(text);
    }
    return rows.joinIntoString("/");
}

DrumGrid DrumGrid::fromString(const juce::String &s) {
    DrumGrid grid;
    auto rows = juce::StringArray::fromTokens(s, "/", "");
    for (int r = 0; r < juce::jmin(drumRows, rows.size()); r++)
        for (int c = 0; c < juce::jmin(drumMaxSteps, rows[r].length()); c++) {
            int value = rows[r][c] - '0';
            grid.cells[size_t(r)][size_t(c)] =
                juce::uint8(juce::jlimit(0, 3, value));
        }
    return grid;
}

bool DrumGrid::isEmpty() const {
    for (auto &row : cells)
        for (auto cell : row)
            if (cell > 0)
                return false;
    return true;
}

int drumVelocity(int cell, float level) {
    static const int velocities[] = {0, 40, 90, 127};
    if (cell <= 0)
        return 0;
    return juce::jlimit(1, 127,
                        juce::roundToInt(velocities[juce::jlimit(1, 3, cell)] *
                                         juce::jlimit(0.0f, 1.0f, level)));
}

int drumPasses(const DrumInput &input) {
    // (the screen offers 2, 4 or 8; a damaged project could hold anything)
    return input.fillEvery > 1 && !input.fill.isEmpty()
               ? juce::jmin(8, input.fillEvery)
               : 1;
}

std::vector<NoteEvent> generateDrums(const DrumInput &input) {
    std::vector<NoteEvent> events;
    int length = juce::jlimit(1, drumMaxSteps, input.length);
    double stepLength = stepBeats(input.stepsPerBar);
    int passes = drumPasses(input);
    for (int pass = 0; pass < passes; pass++) {
        const auto &grid =
            (passes > 1 && pass == passes - 1) ? input.fill : input.main;
        for (int row = 0; row < drumRows; row++)
            for (int step = 0; step < length; step++) {
                int cell = grid.cells[size_t(row)][size_t(step)];
                int velocity = drumVelocity(cell, input.levels[size_t(row)]);
                if (velocity <= 0)
                    continue;
                double start = (pass * length + step) * stepLength +
                               swingOffset(step, input.swing, stepLength);
                events.push_back(
                    {firstDrumNote + row, start, stepLength * 0.9, velocity});
            }
    }
    return events;
}

//==============================================================================
static double endlessStep(const EndlessInput &input) {
    return std::isfinite(input.stepLength)
               ? juce::jlimit(0.0625, 1.0, input.stepLength)
               : 0.25;
}

double endlessLength(const EndlessInput &input) {
    return juce::jmax(1, input.notes.size()) * endlessStep(input);
}

std::vector<NoteEvent> generateEndless(const EndlessInput &unchecked) {
    auto input = unchecked;
    input.stepLength = endlessStep(unchecked);
    std::vector<NoteEvent> events;
    for (int i = 0; i < input.notes.size(); i++) {
        int note = input.notes[i];
        if (note < 0)
            continue;
        events.push_back({juce::jlimit(0, 127, note + input.transpose),
                          i * input.stepLength +
                              swingOffset(i, input.swing, input.stepLength),
                          input.stepLength * juce::jlimit(0.1, 1.0, input.gate),
                          100});
    }
    return events;
}

//==============================================================================
TombolaSimulation::TombolaSimulation(const TombolaInput &in) : input(in) {
    const int sides = juce::jlimit(3, 8, input.sides);
    const int numBalls = juce::jmin(8, input.balls.size());
    const double dt = 1.0 / stepsPerBeat;
    const int frames = int(input.lengthBeats * stepsPerBeat);
    const double turnsPerBeat = juce::jlimit(-1.0, 1.0, input.spin) / 4.0;
    const double omega = turnsPerBeat * juce::MathConstants<double>::twoPi;
    const double gravity = juce::jlimit(0.0, 1.0, input.gravity) * 6.0;
    const double bounce = 0.35 + 0.6 * juce::jlimit(0.0, 1.0, input.bounce);
    const double apothem =
        std::cos(juce::MathConstants<double>::pi / sides) - ballRadius;
    const double cooldown = 0.2;

    struct Ball {
        double x, y, vx, vy, lastHit = -1.0;
    };
    std::vector<Ball> balls;
    for (int i = 0; i < numBalls; i++) {
        double a = i * 2.4 + input.seed * 0.9,
               b = i * 1.7 + 0.5 + input.seed * 1.3;
        balls.push_back({0.3 * std::cos(a), 0.3 * std::sin(a) * 0.6 - 0.1,
                         std::cos(b) * 0.9, std::sin(b) * 0.9});
    }
    hitCounts.assign(size_t(numBalls), 0);
    positions.reserve(size_t(frames));

    for (int frame = 0; frame < frames; frame++) {
        double t = frame * dt;
        double angle = omega * t;
        std::vector<juce::Point<float>> framePositions;
        for (int i = 0; i < numBalls; i++) {
            auto &ball = balls[size_t(i)];
            ball.vy += gravity * dt;
            ball.x += ball.vx * dt;
            ball.y += ball.vy * dt;

            for (int k = 0; k < sides; k++) {
                double normalAngle =
                    angle - juce::MathConstants<double>::halfPi +
                    (k + 0.5) * juce::MathConstants<double>::twoPi / sides;
                double nx = std::cos(normalAngle), ny = std::sin(normalAngle);
                double distance = ball.x * nx + ball.y * ny;
                if (distance <= apothem)
                    continue;
                // the wall moves with the spin
                double wallX = -omega * ball.y, wallY = omega * ball.x;
                double relX = ball.vx - wallX, relY = ball.vy - wallY;
                double normalSpeed = relX * nx + relY * ny;
                if (normalSpeed > 0.0) {
                    relX -= (1.0 + bounce) * normalSpeed * nx;
                    relY -= (1.0 + bounce) * normalSpeed * ny;
                    ball.vx = relX + wallX;
                    ball.vy = relY + wallY;
                    if (normalSpeed > 0.8 && t - ball.lastHit > cooldown) {
                        ball.lastHit = t;
                        // land hits on the 1/16 grid so they groove, and
                        // only once per ball per sixteenth
                        double start = std::round(t * 4.0) / 4.0;
                        bool duplicate = false;
                        for (auto &n : notes)
                            duplicate = duplicate ||
                                        (n.pitch == input.balls[i] &&
                                         std::abs(n.start - start) < 1.0e-6);
                        if (!duplicate && start < input.lengthBeats) {
                            hitCounts[size_t(i)]++;
                            int velocity = juce::jlimit(
                                30, 127,
                                juce::roundToInt(40.0 + normalSpeed * 30.0));
                            notes.push_back(
                                {juce::jlimit(0, 127, input.balls[i]), start,
                                 0.25, velocity});
                        }
                    }
                }
                ball.x -= (distance - apothem) * nx;
                ball.y -= (distance - apothem) * ny;
            }

            // keep things lively but sane
            double speed = std::hypot(ball.vx, ball.vy);
            if (speed > 6.0) {
                ball.vx *= 6.0 / speed;
                ball.vy *= 6.0 / speed;
            }
            framePositions.push_back({float(ball.x), float(ball.y)});
        }
        positions.push_back(std::move(framePositions));
    }
}

TombolaFrame TombolaSimulation::frameAt(double beat) const {
    TombolaFrame frame;
    if (positions.empty())
        return frame;
    double length = input.lengthBeats;
    double t = std::fmod(beat, length);
    if (t < 0.0)
        t += length;
    int index =
        juce::jlimit(0, int(positions.size()) - 1, int(t * stepsPerBeat));
    double turnsPerBeat = juce::jlimit(-1.0, 1.0, input.spin) / 4.0;
    frame.angle = turnsPerBeat * juce::MathConstants<double>::twoPi * index /
                  stepsPerBeat;
    frame.balls = positions[size_t(index)];
    return frame;
}

//==============================================================================
juce::String PatternData::toString() const {
    juce::StringArray stepTexts;
    for (auto &pitches : steps) {
        juce::StringArray notes;
        for (auto p : pitches)
            notes.add(juce::String(p));
        stepTexts.add(notes.joinIntoString("."));
    }
    return stepTexts.joinIntoString("|");
}

PatternData PatternData::fromString(const juce::String &s) {
    PatternData data;
    auto stepTexts = juce::StringArray::fromTokens(s, "|", "");
    for (int i = 0; i < juce::jmin(patternSteps, stepTexts.size()); i++)
        for (auto n : juce::StringArray::fromTokens(stepTexts[i], ".", ""))
            if (n.isNotEmpty())
                data.steps[size_t(i)].addIfNotAlreadyThere(
                    juce::jlimit(0, 127, n.getIntValue()));
    return data;
}

bool PatternData::isEmpty() const {
    for (auto &pitches : steps)
        if (!pitches.isEmpty())
            return false;
    return true;
}

double patternLength(const PatternInput &input) {
    return juce::jmax(1, input.chain.size()) * patternSteps *
           stepBeats(input.stepsPerBar);
}

std::vector<NoteEvent> generatePattern(const PatternInput &input) {
    std::vector<NoteEvent> events;
    double stepLength = stepBeats(input.stepsPerBar);
    for (int link = 0; link < input.chain.size(); link++) {
        int index = juce::jlimit(0, patternCount - 1, input.chain[link]);
        const auto &pattern = input.patterns[size_t(index)];
        for (int step = 0; step < patternSteps; step++)
            for (auto pitch : pattern.steps[size_t(step)])
                events.push_back({pitch,
                                  (link * patternSteps + step) * stepLength,
                                  stepLength * 0.9, 100});
    }
    return events;
}

juce::String chainToString(const juce::Array<int> &chain) {
    juce::StringArray parts;
    for (auto c : chain)
        parts.add(juce::String(c));
    return parts.joinIntoString(",");
}

juce::Array<int> chainFromString(const juce::String &s) {
    juce::Array<int> chain;
    for (auto part : juce::StringArray::fromTokens(s, ",", ""))
        if (part.trim().isNotEmpty())
            chain.add(juce::jlimit(0, patternCount - 1, part.getIntValue()));
    if (chain.isEmpty())
        chain.add(0);
    return chain;
}

} // namespace app_view_models::Sequencers
