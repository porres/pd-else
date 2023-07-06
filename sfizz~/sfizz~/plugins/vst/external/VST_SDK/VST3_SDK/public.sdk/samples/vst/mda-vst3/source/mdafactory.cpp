/*
 *  mdafactory.cpp
 *  mda-vst3
 *
 *  Created by Arne Scheffler on 6/13/08.
 *
 *  mda VST Plug-ins
 *
 *  Copyright (c) 2008 Paul Kellett
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "mdaAmbienceController.h"
#include "mdaBandistoController.h"
#include "mdaBeatBoxController.h"
#include "mdaComboController.h"
#include "mdaDeEsserController.h"
#include "mdaDegradeController.h"
#include "mdaDelayController.h"
#include "mdaDetuneController.h"
#include "mdaDitherController.h"
#include "mdaDubDelayController.h"
#include "mdaDX10Controller.h"
#include "mdaDynamicsController.h"
#include "mdaEPianoController.h"
#include "mdaImageController.h"
#include "mdaJX10Controller.h"
#include "mdaLeslieController.h"
#include "mdaLimiterController.h"
#include "mdaLoudnessController.h"
#include "mdaMultiBandController.h"
#include "mdaOverdriveController.h"
#include "mdaPianoController.h"
#include "mdaRePsychoController.h"
#include "mdaRezFilterController.h"
#include "mdaRingModController.h"
#include "mdaRoundPanController.h"
#include "mdaShepardController.h"
#include "mdaSplitterController.h"
#include "mdaStereoController.h"
#include "mdaSubSynthController.h"
#include "mdaTalkBoxController.h"
#include "mdaTestToneController.h"
#include "mdaThruZeroController.h"
#include "mdaTrackerController.h"
#include "mdaSpecMeterController.h"
#include "helpers.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory_constexpr.h"

//-----------------------------------------------------------------------------
#define kVersionString	FULL_VERSION_STR

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail, 68)

//-----------------------------------------------------------------------------
// -- Ambience
DEF_CLASS  (mda::AmbienceProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Ambience",
			Vst::kDistributable,
			Vst::PlugType::kFxReverb,
			kVersionString,
			kVstVersionString,
			mda::AmbienceProcessor::createInstance, nullptr)

DEF_CLASS  (mda::AmbienceController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Ambience",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::AmbienceController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Bandisto
DEF_CLASS  (mda::BandistoProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Bandisto",
			Vst::kDistributable,
			Vst::PlugType::kFxDistortion,
			kVersionString,
			kVstVersionString,
			mda::BandistoProcessor::createInstance, nullptr)

DEF_CLASS  (mda::BandistoController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Bandisto",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::BandistoController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- BeatBox
DEF_CLASS  (mda::BeatBoxProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda BeatBox",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::BeatBoxProcessor::createInstance, nullptr)

DEF_CLASS  (mda::BeatBoxController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda BeatBox",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::BeatBoxController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Combo
DEF_CLASS  (mda::ComboProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Combo",
			Vst::kDistributable,
			Vst::PlugType::kFxDistortion,
			kVersionString,
			kVstVersionString,
			mda::ComboProcessor::createInstance, nullptr)

DEF_CLASS  (mda::ComboController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Combo",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::ComboController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- DeEsser
DEF_CLASS  (mda::DeEsserProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda DeEsser",
			Vst::kDistributable,
			Vst::PlugType::kFxMastering,
			kVersionString,
			kVstVersionString,
			mda::DeEsserProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DeEsserController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda DeEsser",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DeEsserController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Degrade
DEF_CLASS  (mda::DegradeProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Degrade",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::DegradeProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DegradeController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Degrade",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DegradeController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Delay
DEF_CLASS  (mda::DelayProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Delay",
			Vst::kDistributable,
			Vst::PlugType::kFxDelay,
			kVersionString,
			kVstVersionString,
			mda::DelayProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DelayController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Delay",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DelayController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Detune
DEF_CLASS  (mda::DetuneProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Detune",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::DetuneProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DetuneController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Detune",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DetuneController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Dither
DEF_CLASS  (mda::DitherProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Dither",
			Vst::kDistributable,
			Vst::PlugType::kFxMastering,
			kVersionString,
			kVstVersionString,
			mda::DitherProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DitherController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Dither",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DitherController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- DubDelay
DEF_CLASS  (mda::DubDelayProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda DubDelay",
			Vst::kDistributable,
			Vst::PlugType::kFxDelay,
			kVersionString,
			kVstVersionString,
			mda::DubDelayProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DubDelayController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda DubDelay",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DubDelayController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- DX10
DEF_CLASS  (mda::DX10Processor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda DX10",
			Vst::kDistributable,
			Vst::PlugType::kInstrumentSynth,
			kVersionString,
			kVstVersionString,
			mda::DX10Processor::createInstance, nullptr)

DEF_CLASS  (mda::DX10Controller::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda DX10",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DX10Controller::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Dynamics
DEF_CLASS  (mda::DynamicsProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Dynamics",
			Vst::kDistributable,
			Vst::PlugType::kFxDynamics,
			kVersionString,
			kVstVersionString,
			mda::DynamicsProcessor::createInstance, nullptr)

DEF_CLASS  (mda::DynamicsController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Dynamics",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::DynamicsController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- EPiano
DEF_CLASS  (mda::EPianoProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda EPiano",
			Vst::kDistributable,
			Vst::PlugType::kInstrumentSynth,
			kVersionString,
			kVstVersionString,
			mda::EPianoProcessor::createInstance, nullptr)

DEF_CLASS  (mda::EPianoController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda EPiano",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::EPianoController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Image
DEF_CLASS  (mda::ImageProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Image",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::ImageProcessor::createInstance, nullptr)

DEF_CLASS  (mda::ImageController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Image",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::ImageController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- JX10
DEF_CLASS  (mda::JX10Processor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda JX10",
			Vst::kDistributable,
			Vst::PlugType::kInstrumentSynth,
			kVersionString,
			kVstVersionString,
			mda::JX10Processor::createInstance, nullptr)

DEF_CLASS  (mda::JX10Controller::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda JX10",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::JX10Controller::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Leslie
DEF_CLASS  (mda::LeslieProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Leslie",
			Vst::kDistributable,
			Vst::PlugType::kFxModulation,
			kVersionString,
			kVstVersionString,
			mda::LeslieProcessor::createInstance, nullptr)

DEF_CLASS  (mda::LeslieController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Leslie",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::LeslieController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Limiter
DEF_CLASS  (mda::LimiterProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Limiter",
			Vst::kDistributable,
			Vst::PlugType::kFxDynamics,
			kVersionString,
			kVstVersionString,
			mda::LimiterProcessor::createInstance, nullptr)

DEF_CLASS  (mda::LimiterController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Limiter",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::LimiterController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Loudness
DEF_CLASS  (mda::LoudnessProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Loudness",
			Vst::kDistributable,
			Vst::PlugType::kFxDynamics,
			kVersionString,
			kVstVersionString,
			mda::LoudnessProcessor::createInstance, nullptr)

DEF_CLASS  (mda::LoudnessController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Loudness",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::LoudnessController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- MultiBand
DEF_CLASS  (mda::MultiBandProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda MultiBand",
			Vst::kDistributable,
			Vst::PlugType::kFxDynamics,
			kVersionString,
			kVstVersionString,
			mda::MultiBandProcessor::createInstance, nullptr)

DEF_CLASS  (mda::MultiBandController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda MultiBand",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::MultiBandController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Overdrive
DEF_CLASS  (mda::OverdriveProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Overdrive",
			Vst::kDistributable,
			Vst::PlugType::kFxDistortion,
			kVersionString,
			kVstVersionString,
			mda::OverdriveProcessor::createInstance, nullptr)

DEF_CLASS  (mda::OverdriveController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Overdrive",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::OverdriveController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Piano
DEF_CLASS  (mda::PianoProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Piano",
			Vst::kDistributable,
			Vst::PlugType::kInstrumentSynth,
			kVersionString,
			kVstVersionString,
			mda::PianoProcessor::createInstance, nullptr)

DEF_CLASS  (mda::PianoController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Piano",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::PianoController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- RePsycho
DEF_CLASS  (mda::RePsychoProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda RePsycho!",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::RePsychoProcessor::createInstance, nullptr)

DEF_CLASS  (mda::RePsychoController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda RePsycho!",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::RePsychoController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- RezFilter
DEF_CLASS  (mda::RezFilterProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda RezFilter",
			Vst::kDistributable,
			Vst::PlugType::kFxFilter,
			kVersionString,
			kVstVersionString,
			mda::RezFilterProcessor::createInstance, nullptr)

DEF_CLASS  (mda::RezFilterController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda RezFilter",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::RezFilterController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- RingMod
DEF_CLASS  (mda::RingModProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda RingMod",
			Vst::kDistributable,
			Vst::PlugType::kFxModulation,
			kVersionString,
			kVstVersionString,
			mda::RingModProcessor::createInstance, nullptr)

DEF_CLASS  (mda::RingModController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda RingMod",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::RingModController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Round Panner
DEF_CLASS  (mda::RoundPanProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Round Panner",
			Vst::kDistributable,
			Vst::PlugType::kFxModulation,
			kVersionString,
			kVstVersionString,
			mda::RoundPanProcessor::createInstance, nullptr)

DEF_CLASS  (mda::RoundPanController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Round Panner",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::RoundPanController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Shepard
DEF_CLASS  (mda::ShepardProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Shepard",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::ShepardProcessor::createInstance, nullptr)

DEF_CLASS  (mda::ShepardController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Shepard",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::ShepardController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Splitter
DEF_CLASS  (mda::SplitterProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Splitter",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::SplitterProcessor::createInstance, nullptr)

DEF_CLASS  (mda::SplitterController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Splitter",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::SplitterController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Stereo Simulator
DEF_CLASS  (mda::StereoProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Stereo Simulator",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::StereoProcessor::createInstance, nullptr)

DEF_CLASS  (mda::StereoController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Stereo Simulator",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::StereoController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Sub-Bass Synthesizer
DEF_CLASS  (mda::SubSynthProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Sub-Bass Synthesizer",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::SubSynthProcessor::createInstance, nullptr)

DEF_CLASS  (mda::SubSynthController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Sub-Bass Synthesizer",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::SubSynthController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- TalkBox
DEF_CLASS  (mda::TalkBoxProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda TalkBox",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::TalkBoxProcessor::createInstance, nullptr)

DEF_CLASS  (mda::TalkBoxController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda TalkBox",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::TalkBoxController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- TestTone
DEF_CLASS  (mda::TestToneProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda TestTone",
			Vst::kDistributable,
			Vst::PlugType::kFxGenerator,
			kVersionString,
			kVstVersionString,
			mda::TestToneProcessor::createInstance, nullptr)

DEF_CLASS  (mda::TestToneController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda TestTone",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::TestToneController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Thru-Zero Flanger
DEF_CLASS  (mda::ThruZeroProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Thru-Zero Flanger",
			Vst::kDistributable,
			Vst::PlugType::kFxModulation,
			kVersionString,
			kVstVersionString,
			mda::ThruZeroProcessor::createInstance, nullptr)

DEF_CLASS  (mda::ThruZeroController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Thru-Zero Flanger",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::ThruZeroController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- Tracker
DEF_CLASS  (mda::TrackerProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda Tracker",
			Vst::kDistributable,
			Vst::PlugType::kFx,
			kVersionString,
			kVstVersionString,
			mda::TrackerProcessor::createInstance, nullptr)

DEF_CLASS  (mda::TrackerController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda Tracker",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::TrackerController::createInstance, nullptr)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// -- SpecMeter
DEF_CLASS  (mda::SpecMeterProcessor::uid,
			PClassInfo::kManyInstances,
			kVstAudioEffectClass,
			"mda SpecMeter",
			Vst::kDistributable,
			Vst::PlugType::kFxAnalyzer,
			kVersionString,
			kVstVersionString,
			mda::SpecMeterProcessor::createInstance, nullptr)

DEF_CLASS  (mda::SpecMeterController::uid,
			PClassInfo::kManyInstances,
			kVstComponentControllerClass,
			"mda SpecMeter",
			Vst::kDistributable,
			"",
			kVersionString,
			kVstVersionString,
			mda::SpecMeterController::createInstance, nullptr)
//-----------------------------------------------------------------------------
END_FACTORY

