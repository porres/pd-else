//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/utility/sampleaccurate.h
// Created by  : Steinberg, 04/2021
// Description :
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

#pragma once

#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <cassert>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace SampleAccurate {

//------------------------------------------------------------------------
/** Utility class to handle sample accurate parameter changes coming from IParamValueQueue
 *
 *	The normal use case is to setup the Parameter class once for a specific ParamID and then only
 *	use it in the realtime process method.
 *
 *	If there's a change for the parameter in the inputParameterChanges of the ProcessData structure
 *	the Parameters beginChange method should be called with the valueQueue of the parmID and then
 *	while processing the current audio block the parameter should be advanced as many samples as you
 *	would like to handle parameter changes. In the end the endChanges method must be called to
 *	cleanup internal data structures.
 *	For convenience the endChanges method can be called without a previous beginChanges call.
 */
struct Parameter
{
	Parameter (ParamID pid = 0, ParamValue initValue = 0.) noexcept;

	/** Set the value of the parameter
	 *
	 *	When this is called during the beginChanges() and endChanges() sequence, the changes in the
	 *	value queue are ignored
	 *
	 *	@param v the new value of the parameter
	 */
	void setValue (ParamValue v) noexcept;

	/** Set the ID of the parameter
	 *
	 *	@param pid the new ID of the parameter
	 */
	void setParamID (ParamID pid) noexcept;

	/** Get the ID of the parameter
	 *
	 *	@return ID of the parameter
	 */
	ParamID getParamID () const noexcept;

	/** Get the current value of the parameter
	 *
	 *	@return current value
	 */
	ParamValue getValue () const noexcept;
	/** Are there any pending changes
	 *
	 *	@return true when there are changes
	 */
	bool hasChanges () const noexcept;

	/** Begin change sequence
	 *
	 *	@param valueQueue the queue with the changes
	 */
	void beginChanges (IParamValueQueue* valueQueue) noexcept;

	/** Advance the changes in queue
	 *
	 *	@param numSamples how many samples to advance in the queue
	 *	@return current value
	 */
	ParamValue advance (int32 numSamples) noexcept;

	/** Flush all changes in the queue
	 *
	 *	@return value after flushing
	 */
	ParamValue flushChanges () noexcept;

	/** End change sequence
	 *
	 *	@return value after flushing all possible pending changes
	 */
	ParamValue endChanges () noexcept;

	/** Templated variant of advance
	 *
	 *	calls Proc p with the new value if the value changes
	 */
	template <typename Proc>
	void advance (int32 numSamples, Proc p) noexcept;

	/** Templated variant of flushChanges
	 *
	 *	calls Proc p with the new value if the value changes
	 */
	template <typename Proc>
	void flushChanges (Proc p) noexcept;

	/** Templated variant of endChanges
	 *
	 *	calls Proc p with the new value if the value changes
	 */
	template <typename Proc>
	void endChanges (Proc p) noexcept;

private:
	struct ValuePoint
	{
		ParamValue value {0.};
		double rampPerSample {0.};
		int32 sampleOffset {-1};
	};

	ValuePoint processNextValuePoint () noexcept;

	ParamID paramID;
	int32 pointCount {-1};
	int32 pointIndex {0};
	int32 sampleCounter {0};
	ParamValue currentValue {0.};
	ValuePoint valuePoint;

	IParamValueQueue* queue {nullptr};
};

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE Parameter::Parameter (ParamID pid, ParamValue initValue) noexcept
{
	setParamID (pid);
	setValue (initValue);
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE void Parameter::setValue (ParamValue v) noexcept
{
	currentValue = v;
	pointCount = 0;
	valuePoint = {currentValue, 0., -1};
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE void Parameter::setParamID (ParamID pid) noexcept
{
	paramID = pid;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE ParamID Parameter::getParamID () const noexcept
{
	return paramID;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE ParamValue Parameter::getValue () const noexcept
{
	return currentValue;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE bool Parameter::hasChanges () const noexcept
{
	return pointCount >= 0;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE void Parameter::beginChanges (IParamValueQueue* valueQueue) noexcept
{
	assert (queue == nullptr);
	assert (valueQueue->getParameterId () == getParamID ());
	queue = valueQueue;
	pointCount = queue->getPointCount ();
	pointIndex = 0;
	sampleCounter = 0;
	if (pointCount)
		valuePoint = processNextValuePoint ();
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE ParamValue Parameter::advance (int32 numSamples) noexcept
{
	if (pointCount < 0)
		return currentValue;
	while (valuePoint.sampleOffset >= 0 && valuePoint.sampleOffset < numSamples)
	{
		sampleCounter += valuePoint.sampleOffset;
		numSamples -= valuePoint.sampleOffset;
		currentValue = valuePoint.value;
		valuePoint = processNextValuePoint ();
	}
	currentValue += (valuePoint.rampPerSample * numSamples);
	valuePoint.sampleOffset -= numSamples;
	sampleCounter += numSamples;
	return currentValue;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE ParamValue Parameter::flushChanges () noexcept
{
	while (pointCount >= 0)
	{
		currentValue = valuePoint.value;
		valuePoint = processNextValuePoint ();
	}
	currentValue = valuePoint.value;
	return currentValue;
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE ParamValue Parameter::endChanges () noexcept
{
	flushChanges ();
	pointCount = -1;
	queue = nullptr;
	return currentValue;
}

//------------------------------------------------------------------------
template <typename Proc>
SMTG_ALWAYS_INLINE void Parameter::advance (int32 numSamples, Proc p) noexcept
{
	auto originalValue = currentValue;
	if (advance (numSamples) != originalValue)
	{
		p (currentValue);
	}
}

//------------------------------------------------------------------------
template <typename Proc>
SMTG_ALWAYS_INLINE void Parameter::flushChanges (Proc p) noexcept
{
	auto originalValue = currentValue;
	if (flushChanges () != originalValue)
		p (currentValue);
}

//------------------------------------------------------------------------
template <typename Proc>
SMTG_ALWAYS_INLINE void Parameter::endChanges (Proc p) noexcept
{
	auto originalValue = currentValue;
	if (endChanges () != originalValue)
		p (currentValue);
}

//------------------------------------------------------------------------
SMTG_ALWAYS_INLINE auto Parameter::processNextValuePoint () noexcept -> ValuePoint
{
	ValuePoint nv;
	if (pointCount == 0 || queue->getPoint (pointIndex, nv.sampleOffset, nv.value) != kResultTrue)
	{
		pointCount = -1;
		return {currentValue, 0., -1};
	}
	nv.sampleOffset -= sampleCounter;
	++pointIndex;
	--pointCount;
	if (nv.sampleOffset == 0)
		nv.rampPerSample = (nv.value - currentValue);
	else
		nv.rampPerSample = (nv.value - currentValue) / static_cast<double> (nv.sampleOffset);
	return nv;
}

//------------------------------------------------------------------------
} // SampleAccurate
} // Vst
} // Steinberg
