// The words on the help cards (Control + Play), one per screen
#include "AvailablePluginsListView.h"
#include "HelpOverlay.h"
#include "PlayfulParamScreen.h"
#include "PlayfulVst3Screen.h"
#include "TracksView.h"

namespace {
juce::String u(const char *s) { return juce::String::fromUTF8(s); }

HelpCard card(const char *title, const char *about,
              std::vector<std::pair<const char *, const char *>> rows,
              const char *tip = "") {
    HelpCard c;
    c.title = u(title);
    c.about = u(about);
    for (auto &r : rows)
        c.rows.push_back({u(r.first), u(r.second)});
    c.tip = u(tip);
    return c;
}

//==============================================================================
// Screens found by their title
const std::map<juce::String, HelpCard> &screenCards() {
    static const std::map<juce::String, HelpCard> cards = [] {
        std::map<juce::String, HelpCard> m;

        m["Tracks"] = card(
            "Tracks",
            "Your song: a lane per track with its clips. The chick is the "
            "playhead, the green bar above is the loop.",
            {{"1", "Pick a track  ·  press: zoom in on it"},
             {"2", "Loop end  (with Control: loop start)"},
             {"3", "Move the playhead (Control: whole beats)  ·  press: solo"},
             {"4", "Track colour  ·  press: mute"},
             {"+ / −", "Add a track  /  delete the selected one"},
             {"Rec", "Record on this track (after a bar of count-in)"},
             {"Cut", "Cut the clip at the playhead (Control: quantize it)"},
             {"Paste", "Paste the last cut clip at the playhead"},
             {"Slice", "Split the clip (Control: join two clips)"}},
            "Control + Loop is Undo. Control + Save renders a WAV. Hold "
            "Control + Stop for 3 s to switch off.");

        m["Mixer"] = card(
            "Mixer",
            "A jelly jar per track, the Master (the final mix) first. Higher "
            "jelly is louder; the thin bars show the level as it plays.",
            {{"1", "Pick a track"},
             {"2", "Send A: to the shared reverb (Control: send B, delay)"},
             {"3", "Pan left or right  ·  press: solo"},
             {"4", "Volume  ·  press: mute"}},
            "Keep the Master out of the red: turn tracks down rather than "
            "the Master up.");

        m["Tempo"] = card(
            "Tempo",
            "How fast the song goes, and the grid notes snap to.",
            {{"1", "Beats per minute (BPM)"},
             {"2", "Quantize grid (what Control + Cut snaps to)"},
             {"3", "Swing"},
             {"4", "Click (metronome) volume"},
             {"Tempo", "Tap it in time on this screen to set the BPM"}},
            "One beat lasts 60 000 ÷ BPM milliseconds: handy for Delay "
            "times.");

        m["Settings"] = card(
            "Settings", "Your projects and the audio setup.",
            {{"1", "Pick a setting  ·  press: open it"}},
            "If there's no sound at all, check Output first. Crackles? Make "
            "the Audio Buffer Size bigger.");
        m["Projects"] = card(
            "Projects", "Every song is a project.",
            {{"1", "Pick a project  ·  press: open it"},
             {"+", "Start a new project"},
             {"−", "Delete the selected project (press twice)"}},
            "The open project can't be deleted: open another one first.");
        auto choose = [](const char *title, const char *about) {
            return card(title, about,
                        {{"1", "Pick one  ·  press: use it"}});
        };
        m["Output"] = choose(
            "Output", "Where the sound comes out: headphones, speakers or a "
                      "USB sound card.");
        m["Device Types"] = choose(
            "Device Types",
            "The kind of audio system to use. Only change it if there's no "
            "sound at all.");
        m["Sample Rate"] = choose(
            "Sample Rate", "The audio quality. 44.1 or 48 kHz is perfect.");
        m["Audio Buffer Size"] = choose(
            "Audio Buffer Size",
            "Smaller: keys respond faster. Bigger: fewer crackles.");
        m["Midi Input"] = choose(
            "Midi Input", "Which keyboard or controller plays the LMN-3.");

        m["Plugins"] = card(
            "Plugins",
            "The selected track's chain. Sound flows left to right: MIDI "
            "effects, then the instrument, then effects.",
            {{"1", "Pick a plugin  ·  press: open it"},
             {"3", "Move it along the chain"},
             {"4", "Press: bypass (switch it off without removing it)"},
             {"+", "Add a plugin"},
             {"−", "Remove the selected plugin"}},
            "Bypass an effect on and off while the loop plays to hear what "
            "it does.");
        m["Select Plugin"] = card(
            "Add a plugin",
            "It goes in the right place by itself: MIDI effects before the "
            "instrument, effects after it.",
            {{"1", "Instruments, Effects or MIDI FX"},
             {"2", "Pick the plugin  ·  press: add it"}},
            "One instrument per track: a new one replaces the old.");

        m["Modifiers"] = card(
            "Modifiers",
            "Helpers that turn a plugin knob for you, over and over, in time "
            "with the music.",
            {{"1", "Pick a modulator  ·  press: open it"},
             {"4", "Press: switch it off and on"},
             {"+", "Add one: modifier, then plugin, then knob"},
             {"−", "Remove the selected modulator"}},
            "A knob that a modulator is turning can't be turned by hand: "
            "switch the modulator off first.");
        m["Select Modifier"] = card(
            "Which modifier?",
            "Step 1 of 3. LFO: a smooth wave. Step: a pattern you draw. "
            "Random: dice. Envelope: follows the loudness. MIDI Tracker: "
            "follows the notes.",
            {{"1", "Pick one  ·  press: next step"}});
        m["Select Parameter"] = card(
            "Which knob should it turn?", "Step 3 of 3.",
            {{"1", "Pick the knob  ·  press: done"}},
            "Start with a small depth on the modulator and turn it up.");

        m["Sequencers"] = card(
            "Sequencers",
            "Each one writes a looping clip on this track, starting at the "
            "playhead.",
            {{"1", "Pick a sequencer  ·  press: open it"}},
            "Opening a sequencer twice at the same spot stacks two clips: "
            "Cut the old clip on Tracks first.");

        m["Step Sequencer"] = card(
            "Step",
            "A row of steps, each holding a note, a chord or nothing. Encoder "
            "4 flips the pages: NOTES, STEP (velocity, length, chance, "
            "ratchet) and GROOVE (swing, gate, direction, humanize).",
            {{"1", "NOTES: number of steps"},
             {"2", "NOTES: steps per bar (4, 8 or 16)"},
             {"3", "NOTES: pick a step"},
             {"4", "Press: next page"},
             {"+ & key", "Add or remove that note on the step"},
             {"−", "Clear the step"},
             {"Play", "Hear the loop (this track alone)"}},
            "On the STEP page, hold Control to change every step at once.");
        m["Drums"] = card(
            "Drums",
            "A grid: a row per drum sound, a column per step.",
            {{"1", "Sound row  (Control: that row's level)"},
             {"2", "Step  (Control: swing)"},
             {"3", "Length  (Control: steps per bar)"},
             {"4", "Hit strength  ·  press: clear (Control: fill)"},
             {"C4", "Press: edit the fill grid, press again to go back"},
             {"+ & key", "Add or remove that sound's hit on the step"},
             {"Key", "Jump to that sound's row"}},
            "While the loop plays, + and a key tap hits in live, on the step "
            "nearest the playhead.");
        m["Endless"] = card(
            "Endless",
            "A line of notes you type in, which plays round and round. Once "
            "it plays, every key moves the whole line.",
            {{"+", "Start typing notes / finish"},
             {"−", "While typing: a rest"},
             {"1", "Rate (1/4 to 1/32)"},
             {"2", "Gate (how long each note lasts)"},
             {"3", "Swing"},
             {"4", "Transpose  ·  press: undo the transpose"},
             {"Key", "While playing: the line starts on that key"}},
            "Play one key per bar for a riff that follows your finger.");
        m["Tombola"] = card(
            "Tombola",
            "Balls bounce in a spinning shape; each plays its note when it "
            "hits a wall.",
            {{"Key", "Drop in a ball with that note (up to 8)"},
             {"−", "Take the last ball out"},
             {"1", "Spin (left, still or right)"},
             {"2", "Gravity"},
             {"3", "Sides (3 to 8)"},
             {"4", "Bounce  ·  press: shake for a new loop"}},
            "Notes that sound nice together (C E G B D) always make a happy "
            "accident.");
        m["Pattern"] = card(
            "Pattern",
            "Four 16-step patterns, A to D, played in a chain like A A A B.",
            {{"1", "Pattern to edit  (Control: steps per bar)"},
             {"2", "Step"},
             {"3", "Move along the chain  ·  press: add this pattern"},
             {"C3", "Change the link's letter  ·  press: remove the link"},
             {"4", "Octave shown  ·  press: copy to the next letter"},
             {"+ & key", "Add or remove a note (the step moves on)"},
             {"−", "Clear the step"}},
            "Copy A to B, change the last few notes of B, and chain A A A B "
            "for a riff with an ending.");
        m["Euclidean"] = card(
            "Euclidean",
            "Spreads a number of hits as evenly as possible round a ring of "
            "steps. A ring per drum sound.",
            {{"1", "Pick the sound  ·  press: next sound"},
             {"C1", "Velocity of the hits"},
             {"2", "Hits (0 = off)"},
             {"C2", "Swing"},
             {"3", "Steps round the ring (1 to 32)"},
             {"4", "Rotate: where the pattern starts"},
             {"Key", "Pick that sound's ring"}},
            "4 hits in 16 is a four-on-the-floor kick; 3 in 8 is the "
            "tresillo; 2 in 16 rotated by 4 is a backbeat snare.");
        m["Turing"] = card(
            "Turing",
            "A short loop of notes from a scale that slowly changes itself.",
            {{"1", "Lock: 100% repeats, lower changes  ·  press: new melody"},
             {"C1", "Rate (1/8, 1/16 or 1/32)"},
             {"2", "Length: notes in the loop (2 to 16)"},
             {"C2", "Gate"},
             {"3", "Range (1 to 4 octaves)"},
             {"4", "Scale"},
             {"Key", "The lowest note (and the scale's root)"}},
            "Press encoder 1 until you hear something you love, then turn "
            "Lock up to keep it.");
        m["Chords"] = card(
            "Chords",
            "A chord progression, a chord per bar. The tracks after this one "
            "can follow it.",
            {{"1", "Key (C major to B minor)"},
             {"C1", "Preset progression"},
             {"2", "Bar to change"},
             {"C2", "Number of bars (1 to 8)"},
             {"3", "The chord in that bar"},
             {"4", "Follow: next tracks play bass, arpeggio, pad"},
             {"Key", "Sets the key's root note"}},
            "Give the following tracks instruments first: Follow can't go "
            "past the last track.");

        m["Sampler"] = card(
            "Sampler",
            "Plays one sound up and down the keys. At C4 it plays as "
            "recorded.",
            {{"1", "Press: open the sound list  ·  turn, press to load"},
             {"2", "Start of the sound"},
             {"3", "End of the sound"},
             {"C4", "Gain"}},
            "Put your own .wav files in the samples folder and restart.");
        m["Drum Sampler"] = card(
            "Drum Sampler",
            "A different sound on each of 24 keys. Play a key to select its "
            "pad.",
            {{"1", "Press: open the kit list  ·  turn, press to load"},
             {"2", "Start of the selected pad's sound"},
             {"3", "End of the selected pad's sound"},
             {"C4", "Gain of the selected pad"},
             {"Key", "Play and select that pad"}},
            "Tap every key once to learn where the kick, snare and hats "
            "are.");
        m["Step"] = card(
            "Step modulator",
            "Moves the knob through 16 values you draw, one per step.",
            {{"1", "Pick a candy jar (step)"},
             {"2", "How full it is (the value)"},
             {"3", "Rate"},
             {"4", "Depth"},
             {"C3", "Number of steps"},
             {"C4", "Sync to the song"}},
            "Try it on a filter's frequency for a rhythmic, choppy sound.");
        return m;
    }();
    return cards;
}

//==============================================================================
// Plugin and modulator screens, found by the plugin's name
const std::map<juce::String, HelpCard> &pluginCards() {
    static const std::map<juce::String, HelpCard> cards = [] {
        std::map<juce::String, HelpCard> m;
        // instruments
        m["4OSC"] = card(
            "4OSC",
            "Four oscillators, an envelope and a filter. + and − flip the "
            "pages: Osc 1-4, Envelope, Filter.",
            {{"1", "Osc pages: wave (off, sine, square, saw...)"},
             {"2", "Osc pages: voices"},
             {"3", "Osc pages: tune (semitones)"},
             {"4", "Osc pages: fine tune"},
             {"Ctrl", "Osc pages: detune, level, pulse width, spread"},
             {"+ / −", "Next / previous page"}},
            "A new 4OSC only has Osc 1 on: set Wave on another oscillator "
            "to add it.");
        auto engine = [](const char *title, const char *about,
                         const char *tip) {
            return card(
                title, about,
                {{"1-4", "SOUND page: the engine's own knobs (see the boxes)"},
                 {"Ctrl", "SOUND page: four more knobs"},
                 {"+ / −", "Flip to the SHAPE page and back"},
                 {"SHAPE", "Attack, decay, sustain, release; with Control: "
                           "cutoff, resonance, env amount, voices"}},
                tip);
        };
        m["String"] = engine(
            "String", "A plucked string: guitars, harps, plucks and bells.",
            "Turn up Stiffness (with Control) for a bell or kalimba.");
        m["FM"] = engine(
            "FM",
            "One wave wobbling another: electric pianos, bells, glassy "
            "tones.",
            "More Amount is brighter and more metallic.");
        m["Pulse"] = engine(
            "Pulse", "A chunky retro square wave: leads and basses.",
            "For a sliding lead, set Voices to 1 on SHAPE, then raise "
            "Glide.");
        m["Wavetable"] = engine(
            "Wavetable",
            "A stack of nine waves: Position glides through them, from soft "
            "to sharp.",
            "The Vocal table sings vowels: put an LFO on Position.");
        m["Mallets"] = engine(
            "Mallets", "Things that ring when struck: wood, glass, metal, "
                       "kalimba.",
            "Marimba: Wood, Hardness about 50%, Body up.");
        m["Granular"] = engine(
            "Granular",
            "Chops a sound into tiny grains and sprays them into a cloud. "
            "Sample is with Control on encoder 4.",
            "Freeze on, long Size, slow attack on SHAPE: an instant ambient "
            "pad.");
        m["Drum Synth"] = card(
            "Drum Synth",
            "Eight drums made from scratch on the keys from F3: kick, snare, "
            "clap, rim, hat, open hat, tom, cowbell. The knobs change the "
            "last pad you played.",
            {{"1", "Pitch"},
             {"2", "Punch: the snap at the start"},
             {"3", "Decay: how long it rings"},
             {"4", "Tone: brightness"},
             {"C1", "Noise"},
             {"C2", "Drive"},
             {"Key", "Play and select a pad"}},
            "A closed hat cuts off a ringing open hat, like a real kit.");

        // effects
        m["Reverb"] = card(
            "Reverb", "Puts the sound in a room, from a booth to a cave.",
            {{"1", "Room size"},
             {"2", "Width"},
             {"3", "Damping: darker echoes"},
             {"4", "Wet: how much reverb"},
             {"Ctrl", "Dry: how much of the original"}},
            "Rather than a reverb on every track, share one with the "
            "Mixer's send A.");
        m["Delay"] = card(
            "Delay", "Repeats the sound like an echo.",
            {{"1", "Length: time between echoes (ms)"},
             {"2", "Feedback: how many echoes"},
             {"3", "Mix: how loud they are"}},
            "In time with the song: a beat is 60 000 ÷ BPM ms (500 at "
            "120 BPM).");
        m["Chorus"] = card(
            "Chorus", "Slightly wobbly copies alongside: wider, shimmery.",
            {{"1", "Depth"}, {"2", "Speed"}, {"3", "Width"}, {"4", "Mix"}},
            "Lovely on pads, chords and electric pianos.");
        m["Phaser"] = card(
            "Phaser", "A whooshing swirl through the sound.",
            {{"1", "Depth"}, {"2", "Rate"}, {"3", "Feedback"}});
        m["Compressor"] = card(
            "Compressor", "Evens out loud and quiet moments for punch.",
            {{"1", "Threshold: where squishing starts"},
             {"2", "Ratio: how hard"},
             {"3", "Attack"},
             {"4", "Release"},
             {"Ctrl", "Output: makes up the volume"}},
            "Ratio 4:1, then lower the threshold until only the loudest hits "
            "squish.");
        m["EQ"] = card(
            "EQ", "Turns four frequency bands up and down.",
            {{"1-4", "Low, low mid, high mid, high: how loud"},
             {"Ctrl", "Each band's frequency"},
             {"+ / −", "Page 2: each band's width (Q)"}},
            "Turn Low down on everything but bass and kick to stop the mix "
            "getting muddy.");
        m["LPF/HPF"] = card(
            "LPF/HPF", "A simple filter: low-pass trims highs, high-pass trims "
                       "lows.",
            {{"1", "Frequency"}, {"2", "Mode: low-pass or high-pass"}},
            "Put an LFO modulator on Frequency for a classic sweep.");
        m["Pitch Shift"] = card(
            "Pitch Shift", "Moves the sound up or down without changing its "
                           "speed.",
            {{"1", "Semitones (+12 = an octave up, +7 = a fifth)"}});
        m["Filter"] = card(
            "Filter",
            "A resonant filter that moves by itself, with an envelope and an "
            "LFO.",
            {{"1", "Cutoff"},
             {"2", "Resonance"},
             {"3", "Envelope: follows the loudness (right opens, left closes)"},
             {"4", "LFO: sweeps in time with the song"},
             {"C1", "Type: low-pass 24/12, high-pass, band-pass, notch"},
             {"C2", "Envelope release"},
             {"C3", "LFO rate (4 bars to 1/16)"}},
            "On a drum loop: Low-pass 24, low cutoff, LFO at 2 bars: instant "
            "filter house.");
        m["Drive"] = card(
            "Drive", "Grit and fire: pushes the sound into distortion.",
            {{"1", "Drive (0 to +36 dB)"},
             {"2", "Tone"},
             {"3", "Type: soft clip, hard clip, fuzz, fold"},
             {"4", "Mix with the clean sound"},
             {"Ctrl", "Output level"}},
            "A little soft clip on bass makes it bigger without sounding "
            "distorted.");
        m["Tape"] = card(
            "Tape", "Makes anything sound like an old cassette.",
            {{"1", "Wow: slow pitch drift"},
             {"2", "Flutter: fast shimmer"},
             {"3", "Saturate: warm squash"},
             {"4", "Hiss"},
             {"C1", "Age: duller, with dropouts"},
             {"C2", "Tone"}});
        m["Lo-fi"] = card(
            "Lo-fi", "A bitcrusher: blocky and gritty, like an old sampler.",
            {{"1", "Bits (16 down to 2)"},
             {"2", "Rate (44 kHz down to 500 Hz)"},
             {"3", "Noise and crackle"},
             {"4", "Mix with the clean sound"},
             {"Ctrl", "Filter: softens the harsh top"}},
            "12 bits at 22 kHz with a little noise: that lo-fi hip-hop "
            "feel.");
        m["Ducker"] = card(
            "Ducker", "Dips the volume on every hit, then lets it breathe "
                      "back: the pumping sound.",
            {{"1", "Source: Beat, or another track (sidechain)"},
             {"2", "Depth: how far it dips"},
             {"3", "Release: how quickly it comes back"},
             {"4", "Shape: smooth, punchy or gate"}},
            "Put it on pads and bass with Source on the drum track, not on "
            "the drums.");
        m["Beat Repeat"] = card(
            "Beat Repeat",
            "The stutter button: hold encoder 1 and it repeats the last "
            "slice, starting on the next beat.",
            {{"1", "Slice length  ·  hold: repeat"},
             {"2", "Repeats (1 to 16, or Hold)"},
             {"3", "Decay: each repeat quieter"},
             {"4", "Pitch: each repeat up or down"}},
            "Tap encoder 1 at the end of a phrase for an instant fill. On a "
            "phone, keep a finger still on the box.");

        // MIDI effects
        m["Arpeggiator"] = card(
            "Arpeggiator", "Hold a chord: it plays the notes one by one, in "
                           "time.",
            {{"1", "Mode: up, down, up/down, random, as played"},
             {"C1", "Latch: keeps going after you let go"},
             {"2", "Rate"},
             {"3", "Octaves (1 to 4)"},
             {"4", "Gate: short and bouncy to smooth"}},
            "Needs an instrument after it on the same track.");
        m["Scale Lock"] = card(
            "Scale Lock",
            "Moves every key to the nearest note of a scale: no wrong notes.",
            {{"1", "Root: the home note"}, {"2", "Scale"}},
            "Minor Pentatonic is impossible to make sound bad.");
        m["Chord"] = card(
            "Chord", "Every key plays a whole chord.",
            {{"1", "Chord type"},
             {"2", "Inversion"},
             {"3", "Voicing: close or open"}},
            "Before an Arpeggiator, it makes a running chord pattern.");
        m["Note Repeat"] = card(
            "Note Repeat", "Hold a key and it repeats in time: rolls and "
                           "build-ups.",
            {{"1", "Rate"},
             {"2", "Ramp: louder (right) or fading (left)"},
             {"3", "Gate"},
             {"4", "Swing"}},
            "Change the rate while holding a key for trap hat rolls.");
        m["Strum"] = card(
            "Strum", "Spreads a chord out like a hand across guitar strings.",
            {{"1", "Time: 0 to 200 ms"},
             {"2", "Direction: up or down"},
             {"3", "Alternate: down, up, down, up"},
             {"4", "Curve"}},
            "20-40 ms sounds like a guitar.");
        m["Humanise"] = card(
            "Humanise", "Nudges each note a little so it sounds played by a "
                        "person.",
            {{"1", "Timing"}, {"2", "Velocity"}, {"3", "Length"},
             {"4", "Amount: all three together"}});
        m["MIDI Echo"] = card(
            "MIDI Echo", "Echoes made of notes, which can climb in pitch.",
            {{"1", "Time between echoes"},
             {"2", "Repeats (1 to 8)"},
             {"3", "Decay: each echo quieter"},
             {"4", "Transpose: each echo up or down"}},
            "Transpose +7 on Mallets climbs in fifths.");
        m["Finger"] = card(
            "Finger",
            "Twelve keys from KEY each play a drum groove while held. Hold "
            "two to layer them. Knobs 2-4 change the last key played.",
            {{"1", "The first pattern key"},
             {"2", "Pattern"},
             {"3", "Variation 1-4: busier, open hats, fill"},
             {"4", "Length: 1/2, 1 or 2 bars"}},
            "Record while you play keys: the clip keeps your fingers, so you "
            "can change patterns later.");

        // modulators
        m["LFO"] = card(
            "LFO", "Moves the knob in a smooth, steady wave.",
            {{"1", "Wave type"}, {"2", "Rate"}, {"3", "Depth"},
             {"4", "Phase"}, {"Ctrl", "Offset"}},
            "Start with Depth around 30% and turn up from there.");
        m["Random"] = card(
            "Random", "A new random value each step, like rolling dice.",
            {{"1", "Rate"}, {"2", "Depth"}, {"3", "Smooth: glides"},
             {"4", "Type"}, {"Ctrl", "Step depth, shape, sync, polarity"}});
        m["Envelope"] = card(
            "Envelope", "Follows how loud the track is.",
            {{"1", "Depth"}, {"2", "Attack"}, {"3", "Release"}, {"4", "Gain"},
             {"Ctrl", "Offset, hold"}});
        m["MIDI Tracker"] = card(
            "MIDI Tracker", "Turns the knob by the notes you play.",
            {{"1", "Source: pitch or velocity"}, {"2", "Mode"}, {"3", "Root"},
             {"4", "Spread"}});
        return m;
    }();
    return cards;
}

// The first visible playful screen at or inside a component
PlayfulScreen *findScreen(juce::Component *c) {
    if (c == nullptr || !c->isVisible())
        return nullptr;
    if (auto screen = dynamic_cast<PlayfulScreen *>(c))
        return screen;
    for (auto child : c->getChildren())
        if (auto screen = findScreen(child))
            return screen;
    return nullptr;
}

// A card made from the knob labels, for screens without their own words
HelpCard fromLabels(const juce::String &title,
                    const std::array<juce::String, 8> &labels) {
    HelpCard c;
    c.title = title;
    c.about = u("The four boxes at the bottom always match the four "
                "encoders, left to right.");
    for (int e = 0; e < 4; e++)
        if (labels[size_t(e)].isNotEmpty())
            c.rows.push_back({juce::String(e + 1), labels[size_t(e)]});
    for (int e = 0; e < 4; e++)
        if (labels[size_t(e + 4)].isNotEmpty())
            c.rows.push_back(
                {"C" + juce::String(e + 1), labels[size_t(e + 4)]});
    c.tip = u("+ and − flip through the pages, if there are more.");
    return c;
}
} // namespace

HelpCard helpCardFor(juce::Component *focused) {
    auto screen = findScreen(focused);
    if (screen == nullptr)
        return card("Help", "Nothing to explain on this screen.", {},
                    "Press Tracks to go back to your song.");

    if (dynamic_cast<TracksView *>(screen) != nullptr)
        return screenCards().at("Tracks");

    if (auto vst = dynamic_cast<PlayfulVst3Screen *>(screen)) {
        auto c = fromLabels(vst->getBaseTitle(), vst->getPageLabels());
        c.about = u("A VST3 plugin. Its settings come in pages of 8: 4 on the "
                    "encoders and 4 more with Control.");
        c.rows.push_back({"Press 1", u("Open the presets: turn 1 to pick "
                                       "(2 jumps 16), press 1 to load")});
        return c;
    }

    if (auto params = dynamic_cast<PlayfulParamScreen *>(screen)) {
        auto it = pluginCards().find(params->getBaseTitle());
        if (it != pluginCards().end())
            return it->second;
        return fromLabels(params->getBaseTitle(), params->getPageLabels());
    }

    auto title = screen->getScreenTitle();
    if (title.startsWith("Drums"))
        title = "Drums";
    const bool addingPlugin =
        dynamic_cast<AvailablePluginsListView *>(focused) != nullptr ||
        dynamic_cast<juce::Component *>(screen)
                ->findParentComponentOfClass<AvailablePluginsListView>() !=
            nullptr;
    if (title == "Select Plugin" && !addingPlugin)
        return card("Which plugin?",
                    "Step 2 of 3: which plugin on this track should the "
                    "modulator move?",
                    {{"1", "Pick the plugin  ·  press: next step"}});
    auto it = screenCards().find(title);
    if (it != screenCards().end())
        return it->second;

    HelpCard c;
    c.title = title;
    c.about = u("The four boxes at the bottom always match the four "
                "encoders, left to right. Hold Control for a second set "
                "(\"ctrl: more\" in the corner means there is one).");
    return c;
}
