/*
 *  mdaBaseProcessor.cpp
 *  mda-vst3
 *
 *  Created by Arne Scheffler on 6/14/08.
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

#include "mdaBaseProcessor.h"
#include "mdaBaseController.h"
#include "helpers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include "public.sdk/source/vst/utility/vst2persistence.h"
#include "public.sdk/source/vst/utility/processdataslicer.h"
#include "base/source/fstreamer.h"

#include <cmath>
#include <cstdlib>

namespace Steinberg {
namespace Vst {
namespace mda {

//-----------------------------------------------------------------------------
Processor::Processor ()
: params (nullptr)
, numParams (0)
, bypassRamp (0)
, bypassBuffer0 (nullptr)
, bypassBuffer1 (nullptr)
, bypassState (false)
{
}

//-----------------------------------------------------------------------------
Processor::~Processor ()
{
	if (bypassBuffer0)
		std::free (bypassBuffer0);
	if (bypassBuffer1)
		std::free (bypassBuffer1);
	if (params)
		delete [] params;
}

//------------------------------------------------------------------------
bool Processor::checkStateTransfer ()
{
	bool result = false;
	stateTransfer.accessTransferObject_rt ([this, &result] (const std::vector<ParamValue>& p) {
		for (auto index = 0u; index < p.size (); ++index)
		{
			params[index] = p[index];
		}
		result = true;
	});
	return result;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::process (ProcessData& data)
{
	preProcess ();
	bool doRecalculate = checkStateTransfer ();
	doRecalculate |= processParameterChanges (data.inputParameterChanges);
	if (doRecalculate)
		recalculate ();

	processEvents (data.inputEvents);

	if (data.numSamples > 0 && !bypassProcessing (data))
	{
		doProcessing (data);
		checkSilence (data);
	}
	return kResultTrue;
}

//-----------------------------------------------------------------------------
void Processor::processEvents (IEventList* events)
{
	if (events)
	{
		Event e;
		int32 count = events->getEventCount ();
		for (int32 i = 0; i < count; i++)
		{
			if (events->getEvent (i, e) == kResultTrue)
				processEvent (e);
		}
	}
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& setup)
{
	if (bypassBuffer0)
		std::free (bypassBuffer0);
	bypassBuffer0 = (float*)std::malloc (setup.maxSamplesPerBlock * sizeof (float));

	if (bypassBuffer1)
		std::free (bypassBuffer1);
	bypassBuffer1 = (float*)std::malloc (setup.maxSamplesPerBlock * sizeof (float));
	
	return AudioEffect::setupProcessing (setup);
}

//-----------------------------------------------------------------------------
bool Processor::bypassProcessing (ProcessData& data)
{
	if (data.numSamples == 0)
		return true;

	if (isBypassed () || bypassRamp != 0)
	{
		if (bypassRamp)
		{
			float* origOut0 = data.outputs[0].channelBuffers32[0];
			float* origOut1 = data.outputs[0].channelBuffers32[1];

			data.outputs[0].channelBuffers32[0] = bypassBuffer0;
			data.outputs[0].channelBuffers32[1] = bypassBuffer1;

			doProcessing (data);

			data.outputs[0].channelBuffers32[0] = origOut0;
			data.outputs[0].channelBuffers32[1] = origOut1;

			float* bpb0 = bypassBuffer0;
			float* bpb1 = bypassBuffer1;
			float origMix = 0;
			float plugMix = 0;
			float factor = 1.f / (float)data.numSamples;

			if (bypassRamp > 0)
			{
				for (int32 i = 0; i < data.numSamples; i++)
				{
					origMix = (float)i * factor;
					plugMix = (float)(data.numSamples - i) * factor;
					*origOut0 = *origOut0 * origMix + *bpb0 * plugMix;
					*origOut1 = *origOut1 * origMix + *bpb1 * plugMix;
					origOut0++;
					origOut1++;
					bpb0++;
					bpb1++;
				}
			}
			else
			{
				for (int32 i = 0; i < data.numSamples; i++)
				{
					plugMix = (float)i * factor;
					origMix = (float)(data.numSamples - i) * factor;
					*origOut0 = *origOut0 * origMix + *bpb0 * plugMix;
					*origOut1 = *origOut1 * origMix + *bpb1 * plugMix;
					origOut0++;
					origOut1++;
					bpb0++;
					bpb1++;
				}
			}
			data.outputs[0].silenceFlags = 0;
			bypassRamp = 0;
		}
		else
		{
			for (int32 bus = 0; (bus < data.numInputs) && (bus < data.numOutputs); bus++)
			{
				Bus* output = getAudioOutput (bus);
				if (!output || !output->isActive ())
					continue;
				for (int32 channel = 0; channel < data.outputs[bus].numChannels; channel++)
				{
					if (data.numInputs <= bus || data.inputs[bus].numChannels <= channel)
					{
						memset (data.outputs[bus].channelBuffers32[channel], 0, sizeof (float) * data.numSamples);
						data.outputs[bus].silenceFlags |= (uint64)1 << channel;
					}
					else
					{
						if (data.outputs[bus].channelBuffers32[channel] != data.inputs[bus].channelBuffers32[channel])
							memcpy (data.outputs[bus].channelBuffers32[channel], data.inputs[bus].channelBuffers32[channel], sizeof (float) * data.numSamples);
 						data.outputs[bus].silenceFlags |= data.inputs[bus].silenceFlags & ((uint64)1 << channel);
					}
				}
			}
		}
		return true;
	}
	return false;
}

#define kSilenceThreshold 0.000132184039

//-----------------------------------------------------------------------------
void Processor::checkSilence (ProcessData& data)
{
	for (int32 bus = 0; bus < data.numOutputs; bus ++)
	{
		data.outputs[bus].silenceFlags = 0;
		
		if (!getAudioOutput (bus)->isActive ())
			continue;

		for (int32 channel = 0; channel < data.outputs[bus].numChannels; channel++)
		{
			bool channelIsSilent = true;
			for (int32 sample = 0; sample < data.numSamples; sample += 10)
			{
				if (fabs (data.outputs[bus].channelBuffers32[channel][sample]) > kSilenceThreshold)
				{
					channelIsSilent = false;
					break;
				}
			}
			if (channelIsSilent)
				data.outputs[bus].silenceFlags |= (uint64)1 << channel;
		}
	}
}

//-----------------------------------------------------------------------------
bool Processor::processParameterChanges (IParameterChanges* changes)
{
	if (changes)
	{
		int32 count = changes->getParameterCount ();
		if (count > 0)
		{
			for (int32 i = 0; i < count; i++)
			{
				IParamValueQueue* queue = changes->getParameterData (i);
				if (!queue)
					continue;
				ParamID paramId = queue->getParameterId ();
				int32 pointCount = queue->getPointCount ();
				int32 sampleOffset;
				ParamValue value;
				queue->getPoint (pointCount - 1, sampleOffset, value);
				if (paramId == BaseController::kBypassParam)
					setBypass (value >= 0.5, sampleOffset);
				else if (paramId == BaseController::kPresetParam)
					setCurrentProgramNormalized (value);
				else
					setParameter (paramId, value, sampleOffset);
			}
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
void Processor::setBypass (bool state, int32 /*sampleOffset*/)
{
	if (state != bypassState)
	{
		bypassState = state;
		if (bypassState)
			bypassRamp = 1;
		else
			bypassRamp = -1;
	}
}

//-----------------------------------------------------------------------------
void Processor::setParameter (ParamID index, ParamValue newValue, int32 /*sampleOffset*/)
{
	if (numParams > index)
		params[index] = newValue;
}

//-----------------------------------------------------------------------------
void Processor::allocParameters (int32 _numParams)
{
	if (params)
		return;
	numParams = _numParams;
	params = new ParamValue[numParams];
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns)
	{
		if (SpeakerArr::getChannelCount (inputs[0]) != 2)
			return kResultFalse;
	}
	if (numOuts)
	{
		if (SpeakerArr::getChannelCount (outputs[0]) != 2)
			return kResultFalse;
	}
	return kResultTrue;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::setActive (TBool state)
{
	if (state)
	{
		IMessage* msg = allocateMessage ();
		if (msg)
		{
			msg->setMessageID ("activated");
			msg->getAttributes ()->setFloat ("SampleRate", processSetup.sampleRate);
			sendMessage (msg);
			msg->release ();
		}
	}
	return AudioEffect::setActive (state);
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	if (auto vst2State = VST3::tryVst2StateLoad (*state))
	{
		if ((vst2State->programs.empty ()) ||
		    (static_cast<int32_t> (vst2State->programs.size ()) <= vst2State->currentProgram))
			return kResultFalse;
		auto& currentProgram = vst2State->programs[vst2State->currentProgram];
		bypassState = vst2State->isBypassed;
		auto numStateParams = static_cast<uint32> (currentProgram.values.size ());
		auto state = std::make_unique<StateT> ();
		state->reserve (numStateParams);
		for (uint32 index = 0; index < numParams && index < numStateParams; ++index)
		{
			state->push_back (currentProgram.values[index]);
		}
		stateTransfer.transferObject_ui (std::move (state));
		return kResultTrue;
	}

	IBStreamer streamer (state, kLittleEndian);

	uint32 temp;
	streamer.readInt32u (temp); // numParams or Header

	if (temp == BaseController::kMagicNumber)
	{
		// read current Program
		streamer.readInt32u (temp);
		setCurrentProgram (temp);

		streamer.readInt32u (temp);
	}

	// read each parameter
	for (uint32 i = 0; i < temp, i < numParams; i++)
	{
		streamer.readDouble (params[i]);
	}

	// bypass
	streamer.readInt32u (temp);
	bypassState = temp > 0;

	recalculate ();

	return kResultTrue;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API Processor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	VST3::Vst2xState fxb;
	fxb.fxUniqueID = getVst2UniqueId ();
	fxb.fxVersion = 1;
	fxb.currentProgram = getCurrentProgram ();
	fxb.isBypassed = isBypassed ();
	fxb.programs.resize (getNumPrograms ());
	for (auto& program : fxb.programs)
		program.values.resize (numParams);
	auto& currentProgram = fxb.programs[fxb.currentProgram];
	currentProgram.fxUniqueID = getVst2UniqueId ();
	currentProgram.fxVersion = 1;
	currentProgram.name = "";
	for (uint32 index = 0; index < numParams; ++index)
	{
		currentProgram.values[index] = static_cast<float> (params[index]);
	}
	VST3::writeVst2State (fxb, *state);

#if 0
	IBStreamer streamer (state, kLittleEndian);

	if (hasProgram ())
	{
		// save header key
		uint32 temp = BaseController::kMagicNumber;
		streamer.writeInt32u (temp);

		// save program
		temp = getCurrentProgram ();
		streamer.writeInt32u (temp);
	}

	// save number of parameter
	streamer.writeInt32u (numParams);

	// save each parameter
	for (uint32 i = 0; i < numParams; i++)
	{
		streamer.writeDouble (params[i]);
	}

	// save bypass
	streamer.writeInt32u (bypassState ? 1 : 0);
#endif

	return kResultTrue;
}

//------------------------------------------------------------------------
SampleAccurateBaseProcessor::SampleAccurateBaseProcessor ()
{
	parameterValues.resize (1);
}

//------------------------------------------------------------------------
void SampleAccurateBaseProcessor::allocParameters (int32 numParams)
{
	Processor::allocParameters (numParams);
	parameterValues.resize (numParams+1);
}

//------------------------------------------------------------------------
template <SymbolicSampleSizes SampleSize>
void SampleAccurateBaseProcessor::process (ProcessData& data)
{
	ProcessDataSlicer slicer (16);

	Event event {};
	event.sampleOffset = -1;
	auto eventCount = data.inputEvents ? data.inputEvents->getEventCount () : 0;
	auto eventIndex = 0;
	auto sampleCounter = 0;
	if (eventCount)
		data.inputEvents->getEvent (eventIndex++, event);

	auto doProcess = [&] (ProcessData & data) noexcept
	{
		preProcess ();
		while (event.sampleOffset >= 0)
		{
			if (event.sampleOffset <= data.numSamples)
			{
				processEvent (event);
				event.sampleOffset = -1;
				if (eventCount > eventIndex)
				{
					if (data.inputEvents->getEvent (eventIndex++, event) == kResultTrue)
					{
						event.sampleOffset -= sampleCounter;
					}
					else
						event.sampleOffset = -1;
				}
			}
			else
			{
				event.sampleOffset -= data.numSamples;
				break;
			}
		}

		bool needRecalculate = false;
		for (auto& param : parameterValues)
		{
			if (!param.first)
				break;
			param.second.advance (data.numSamples, [&] (ParamValue value) {
				setParameter (param.second.getParamID (), value, 0);
				needRecalculate = true;
			});
		}
		if (needRecalculate)
			recalculate ();
		doProcessing (data);
		sampleCounter += data.numSamples;
	};
	slicer.process<SampleSize> (data, doProcess);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SampleAccurateBaseProcessor::process (ProcessData& data)
{
	if (checkStateTransfer ())
		recalculate ();
	processParameterChanges (data.inputParameterChanges);

	if (data.numSamples > 0 && !bypassProcessing (data))
	{
		if (processSetup.symbolicSampleSize == SymbolicSampleSizes::kSample32)
			process<SymbolicSampleSizes::kSample32> (data);
		else
			process<SymbolicSampleSizes::kSample64> (data);
		checkSilence (data);
	}

	for (auto& paramterValue : parameterValues)
	{
		if (!paramterValue.first)
			break;
		paramterValue.second.endChanges ([&] (ParamValue value) {
			setParameter (paramterValue.second.getParamID (), value, 0);
		});
	}

	return kResultTrue;
}

//------------------------------------------------------------------------
bool SampleAccurateBaseProcessor::processParameterChanges (IParameterChanges* changes)
{
	parameterValues[0].first = false;
	if (!changes)
		return false;
	int32 count = changes->getParameterCount ();
	if (count > 0)
	{
		uint32 usedChangedParameters = 0;
		for (int32 i = 0; i < count; i++)
		{
			IParamValueQueue* queue = changes->getParameterData (i);
			if (!queue)
				continue;
			auto paramID = queue->getParameterId ();
			if (paramID < parameterValues.size ())
			{
				parameterValues[usedChangedParameters].first = true;
				parameterValues[usedChangedParameters].second.setParamID (paramID);
				parameterValues[usedChangedParameters].second.setValue (params[paramID]);
				parameterValues[usedChangedParameters].second.beginChanges (queue);
				++usedChangedParameters;
			}
			else
			{
				int32 sampleOffset;
				ParamValue value;
				queue->getPoint (queue->getPointCount () - 1, sampleOffset, value);
				if (paramID == BaseController::kBypassParam)
					setBypass (value >= 0.5, sampleOffset);
				else if (paramID == BaseController::kPresetParam)
					setCurrentProgramNormalized (value);
				else
					setParameter (paramID, value, sampleOffset);
			}
		}
		parameterValues[usedChangedParameters].first = false;
		return true;
	}
	return false;	
}

//------------------------------------------------------------------------
}}} // namespace
