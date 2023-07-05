/*
 *  mdaBaseProcessor.h
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

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/utility/rttransfer.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <vector>
#include <array>

namespace Steinberg {
namespace Vst {
namespace mda {

//-----------------------------------------------------------------------------
class Processor : public AudioEffect
{
protected:
	Processor ();
	~Processor ();

	virtual void doProcessing (ProcessData& data) = 0;
	virtual void preProcess () {}
	virtual bool bypassProcessing (ProcessData& data);
	virtual void processEvent (const Event& /*event*/) {}
	virtual void checkSilence (ProcessData& data);
	virtual void setBypass (bool state, int32 sampleOffset);
	virtual bool processParameterChanges (IParameterChanges* changes);
	virtual void setParameter (ParamID index, ParamValue newValue, int32 sampleOffset);
	virtual void allocParameters (int32 numParams);
	virtual void recalculate () {}
	virtual bool hasProgram () const { return false; }
	virtual uint32 getCurrentProgram () const { return 0; }
	virtual uint32 getNumPrograms () const { return 1; }
	virtual void setCurrentProgram (uint32 /*val*/) {}
	virtual void setCurrentProgramNormalized (ParamValue /*val*/) {}
	virtual int32 getVst2UniqueId () const = 0;

	void processEvents (IEventList* events);
	bool isBypassed () const { return bypassState; }
	double getSampleRate () const { return processSetup.sampleRate; }

	tresult PLUGIN_API process (ProcessData& data) SMTG_OVERRIDE;

	tresult PLUGIN_API setupProcessing (ProcessSetup& setup) SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;
	tresult PLUGIN_API setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
	                                       SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;

	tresult PLUGIN_API setState (IBStream* state) final;
	tresult PLUGIN_API getState (IBStream* state) final;

	bool checkStateTransfer ();

	ParamValue* params;
	uint32 numParams;
	int32 bypassRamp;

	float* bypassBuffer0;
	float* bypassBuffer1;

	bool bypassState;
	
	using StateT = std::vector<ParamValue>;
	RTTransferT<StateT> stateTransfer;
};

//------------------------------------------------------------------------
class SampleAccurateBaseProcessor : public Processor
{
public:
	SampleAccurateBaseProcessor ();
	void allocParameters (int32 numParams) final;
	tresult PLUGIN_API process (ProcessData& data) final;
	bool processParameterChanges (IParameterChanges* changes) final;
private:
	template<SymbolicSampleSizes SampleSize>
	void process (ProcessData& data);

	std::vector<std::pair<bool, SampleAccurate::Parameter>> parameterValues;
};

//------------------------------------------------------------------------
using BaseProcessor = SampleAccurateBaseProcessor;

static constexpr int32 EndNoteID = NoteIDUserRange::kNoteIDUserRangeLowerBound;
static constexpr int32 SustainNoteID = NoteIDUserRange::kNoteIDUserRangeLowerBound + 1;

//------------------------------------------------------------------------
template <typename VoiceT, int32 kEventBufferSize, int32 kNumVoices>
struct SynthData
{
	using VOICE = VoiceT;
	static constexpr int32 eventBufferSize = kEventBufferSize;
	static constexpr int32 numVoices = kNumVoices;
	static constexpr int32 eventsDoneID = 99999999;

	using EventArray = std::array<Event, kEventBufferSize>;
	using EventPos = typename EventArray::size_type;
	EventArray events;
	EventPos eventPos {0};

	using VoiceArray = std::array<VOICE, kNumVoices>;
	VoiceArray voice;
	int32 activevoices {0};
	int32 sustain {0};

	void init () noexcept { activevoices = 0; }

	bool hasEvents () const noexcept
	{
		return events[eventPos].flags & Event::EventFlags::kUserReserved1;
	}

	void clearEvents () noexcept
	{
		eventPos = 0;
		events[0].flags = 0;
		events[0].sampleOffset = eventsDoneID;
	}

	void processEvent (const Event& e) noexcept
	{
		switch (e.type)
		{
			//--- -------------------
			case Event::kNoteOnEvent:
			case Event::kNoteOffEvent:
			{
				events[eventPos] = e;
				events[eventPos].flags |= Event::EventFlags::kUserReserved1;
				++eventPos;
				if (eventPos >= events.size ())
					--eventPos;
				else
				{
					events[eventPos].flags = 0;
					events[eventPos].sampleOffset = eventsDoneID;
				}
				break;
			}
			//--- -------------------
			default: return;
		}
	}
};

//------------------------------------------------------------------------
}
}
} // namespaces
