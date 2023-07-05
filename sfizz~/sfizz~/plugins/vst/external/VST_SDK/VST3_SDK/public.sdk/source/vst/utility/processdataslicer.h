//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/utility/processdataslicer.h
// Created by  : Steinberg, 04/2021
// Description : Process the process data in slices
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

#include "pluginterfaces/vst/ivstaudioprocessor.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
/** Process Data Slicer
 *
 *	Cuts the VST process data into slices to process
 *
 *	Example:
 *	\code{.cpp}
 *	tresult PLUGIN_API Processor::process (ProcessData& data)
 *	{
 *		ProcessDataSlicer slicer (32);
 *		slicer.process<SymbolicSampleSizes::kSample32> (data, [&] (ProcessData& data) {
 *			doSlicedProcessing (data); // data.numSamples <= 32
 *		});
 *	}
 *	\endcode
 */
class ProcessDataSlicer
{
public:
	/** Constructor
	 *
	 *	@param inSliceSice slice size in samples
	 */
	ProcessDataSlicer (int32 inSliceSize = 8) : sliceSize (inSliceSize) {}

//------------------------------------------------------------------------
	/** Process the data
	 *
	 *	@tparam SampleSize sample size 32 or 64 bit processing
	 *	@tparam DoProcessCallback the callback proc
	 *	@param data Process data
	 *	@param doProcessing process callback
	 */
	template <SymbolicSampleSizes SampleSize, typename DoProcessCallback>
	void process (ProcessData& data, DoProcessCallback doProcessing) noexcept
	{
		stopIt = false;
		auto numSamples = data.numSamples;
		auto samplesLeft = data.numSamples;
		while (samplesLeft > 0 && !stopIt)
		{
			auto currentSliceSize = samplesLeft > sliceSize ? sliceSize : samplesLeft;

			data.numSamples = currentSliceSize;
			doProcessing (data);

			advanceBuffers<SampleSize> (data.inputs, data.numInputs, currentSliceSize);
			advanceBuffers<SampleSize> (data.outputs, data.numOutputs, currentSliceSize);
			samplesLeft -= currentSliceSize;
		}
		// revert buffer pointers (otherwise some hosts may use these wrong pointers)
		advanceBuffers<SampleSize> (data.inputs, data.numInputs, -(numSamples - samplesLeft));
		advanceBuffers<SampleSize> (data.outputs, data.numOutputs, -(numSamples - samplesLeft));
		data.numSamples = numSamples;
	}

	/** Stop the slice process
	 *
	 *	If you want to break the slice processing early, you have to capture the slicer in the
	 *	DoProcessCallback and call the stop method.
	 */
	void stop () noexcept { stopIt = true; }

private:
	template <SymbolicSampleSizes SampleSize>
	void advanceBuffers (AudioBusBuffers* buffers, int32 numBuffers, int32 numSamples) const
	    noexcept
	{
		for (auto index = 0; index < numBuffers; ++index)
		{
			for (auto channelIndex = 0; channelIndex < buffers[index].numChannels; ++channelIndex)
			{
				if (SampleSize == SymbolicSampleSizes::kSample32)
					buffers[index].channelBuffers32[channelIndex] += numSamples;
				else
					buffers[index].channelBuffers64[channelIndex] += numSamples;
			}
		}
	}
	int32 sliceSize;
	bool stopIt {false};
};

//------------------------------------------------------------------------
} // Vst
} // Steinberg
