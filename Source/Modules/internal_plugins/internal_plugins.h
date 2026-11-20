// clang-format off
/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION
  ID:               internal_plugins
  vendor:           Stone Preston
  version:          1.0.0
  name:             Internal Plugins
  description:      Internal plugins for app
  website:          http://github.com/stonepreston
  license:          GPL-3.0
  dependencies:     juce_data_structures tracktion_engine juce_events juce_core juce_graphics
 END_JUCE_MODULE_DECLARATION
*******************************************************************************/
#pragma once

namespace internal_plugins {

    class DrumSamplerPlugin;
    class MidiFxPlugin;
    class ArpeggiatorPlugin;
    class ScaleLockPlugin;
    class ChordPlugin;
    class NoteRepeatPlugin;
    class StrumPlugin;
    class HumanisePlugin;
    class MidiEchoPlugin;
    class AudioFxPlugin;
    class TapePlugin;
    class LofiPlugin;
    class DrivePlugin;
    class ResonantFilterPlugin;
    class DuckerPlugin;
    class BeatRepeatPlugin;
    class EnginePlugin;
    class StringEnginePlugin;
    class FmEnginePlugin;
    class PulseEnginePlugin;

}

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <tracktion_engine/tracktion_engine.h>
#include <functional>
#include <array>
#include <atomic>
#include <vector>
#include <algorithm>

#include "DrumSamplerPlugin/DrumSamplerPlugin.h"
#include "MidiFx/MidiFxLogic.h"
#include "MidiFx/MidiFxPlugins.h"
#include "MidiFx/MoreMidiFx.h"
#include "Engines/EngineDsp.h"
#include "Engines/EnginePlugins.h"
#include "Effects/FxDsp.h"
#include "Effects/FxPlugins.h"



