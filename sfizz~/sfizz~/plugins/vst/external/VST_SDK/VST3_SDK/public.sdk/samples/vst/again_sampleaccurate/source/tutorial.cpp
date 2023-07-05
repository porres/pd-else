//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again_sampleaccurate/source/tutorial.cpp
// Created by  : Steinberg, 04/2021
// Description : Tutorial
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2023, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "tutorial.h"
#include "public.sdk/source/vst/utility/audiobuffers.h"
#include "public.sdk/source/vst/utility/processdataslicer.h"
#include "public.sdk/source/vst/utility/rttransfer.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "base/source/fstreamer.h"
#include <array>
#include <cassert>
#include <limits>
#include <vector>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace Tutorial {

//------------------------------------------------------------------------
enum ParameterID
{
	Gain = 1,
};

//------------------------------------------------------------------------
struct StateModel
{
	double gain;
};

//------------------------------------------------------------------------
struct MyEffect : public AudioEffect
{
	using RTTransfer = RTTransferT<StateModel>;

	MyEffect ();
	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	tresult PLUGIN_API setState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API getState (IBStream* state) SMTG_OVERRIDE;
	tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
	                                       SpeakerArrangement* outputs,
	                                       int32 numOuts) SMTG_OVERRIDE;
	tresult PLUGIN_API canProcessSampleSize (int32 symbolicSampleSize) SMTG_OVERRIDE;
	tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;

	void handleParameterChanges (IParameterChanges* changes);

	template <SymbolicSampleSizes SampleSize>
	void process (ProcessData& data);

	SampleAccurate::Parameter gainParameter {ParameterID::Gain, 1.};
	RTTransfer stateTransfer;
};

//------------------------------------------------------------------------
MyEffect::MyEffect ()
{
	setControllerClass (ControllerID);
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::initialize (FUnknown* context)
{
	auto result = AudioEffect::initialize (context);
	if (result == kResultTrue)
	{
		addAudioInput (STR ("Input"), SpeakerArr::kStereo);
		addAudioOutput (STR ("Output"), SpeakerArr::kStereo);
	}
	return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::terminate ()
{
	stateTransfer.clear_ui ();
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::setState (IBStream* state)
{
	if (!state)
		return kInvalidArgument;

	IBStreamer streamer (state, kLittleEndian);

	uint32 numParams;
	if (streamer.readInt32u (numParams) == false)
		return kResultFalse;

	auto model = std::make_unique<StateModel> ();

	ParamValue value;
	if (!streamer.readDouble (value))
		return kResultFalse;

	model->gain = value;

	stateTransfer.transferObject_ui (std::move (model));
	return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::getState (IBStream* state)
{
	if (!state)
		return kInvalidArgument;

	IBStreamer streamer (state, kLittleEndian);
	streamer.writeDouble (gainParameter.getValue ());
	return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                 SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns != 1 || numOuts != 1)
		return kResultFalse;
	if (SpeakerArr::getChannelCount (inputs[0]) == SpeakerArr::getChannelCount (outputs[0]))
	{
		getAudioInput (0)->setArrangement (inputs[0]);
		getAudioOutput (0)->setArrangement (outputs[0]);
		return kResultTrue;
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::canProcessSampleSize (int32 symbolicSampleSize)
{
	return (symbolicSampleSize == SymbolicSampleSizes::kSample32 ||
	        symbolicSampleSize == SymbolicSampleSizes::kSample64) ?
	           kResultTrue :
	           kResultFalse;
}

//------------------------------------------------------------------------
template <SymbolicSampleSizes SampleSize>
void MyEffect::process (ProcessData& data)
{
	ProcessDataSlicer slicer (8);

	auto doProcessing = [this] (ProcessData& data) {
		// get the gain value for this block
		ParamValue gain = gainParameter.advance (data.numSamples);

		// process audio
		AudioBusBuffers* inputs = data.inputs;
		AudioBusBuffers* outputs = data.outputs;
		for (auto channelIndex = 0; channelIndex < inputs[0].numChannels; ++channelIndex)
		{
			auto inputBuffers = getChannelBuffers<SampleSize> (inputs[0])[channelIndex];
			auto outputBuffers = getChannelBuffers<SampleSize> (outputs[0])[channelIndex];
			for (auto sampleIndex = 0; sampleIndex < data.numSamples; ++sampleIndex)
			{
				auto sample = inputBuffers[sampleIndex];
				outputBuffers[sampleIndex] = sample * gain;
			}
		}
	};

	slicer.process<SampleSize> (data, doProcessing);
}

//------------------------------------------------------------------------
void MyEffect::handleParameterChanges (IParameterChanges* changes)
{
	if (!changes)
		return;
	int32 changeCount = changes->getParameterCount ();
	for (auto i = 0; i < changeCount; ++i)
	{
		if (auto queue = changes->getParameterData (i))
		{
			auto paramID = queue->getParameterId ();
			if (paramID == ParameterID::Gain)
			{
				gainParameter.beginChanges (queue);
			}
		}
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API MyEffect::process (ProcessData& data)
{
	stateTransfer.accessTransferObject_rt (
	    [this] (const auto& stateModel) { gainParameter.setValue (stateModel.gain); });

	handleParameterChanges (data.inputParameterChanges);

	if (processSetup.symbolicSampleSize == SymbolicSampleSizes::kSample32)
		process<SymbolicSampleSizes::kSample32> (data);
	else
		process<SymbolicSampleSizes::kSample64> (data);

	gainParameter.endChanges ();
	return kResultTrue;
}

//------------------------------------------------------------------------
FUnknown* createProcessorInstance (void*)
{
	return static_cast<IAudioProcessor*> (new MyEffect);
}

//------------------------------------------------------------------------
class Controller : public EditController
{
	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API setComponentState (IBStream* state) SMTG_OVERRIDE;
};

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
	tresult result = EditController::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}
	parameters.addParameter (STR ("Gain"), STR ("%"), 0, 1., ParameterInfo::kCanAutomate,
	                         ParameterID::Gain);
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
	if (!state)
		return kInvalidArgument;

	IBStreamer streamer (state, kLittleEndian);

	ParamValue value;
	if (!streamer.readDouble (value))
		return kResultFalse;

	if (auto param = parameters.getParameter (ParameterID::Gain))
		param->setNormalized (value);
	return kResultTrue;
}

//------------------------------------------------------------------------
FUnknown* createControllerInstance (void*)
{
	return static_cast<IEditController*> (new Controller);
}

//------------------------------------------------------------------------
} // Tutorial
} // Vst
} // Steinberg
