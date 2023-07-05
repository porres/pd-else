//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/source/vst/utility/test/sampleaccuratetest.cpp
// Created by  : Steinberg, 04/2021
// Description : Tests for Sample Accurate Parameter Changes
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

#include "public.sdk/source/main/moduleinit.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/utility/sampleaccurate.h"
#include "public.sdk/source/vst/utility/testing.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
static ModuleInitializer InitTests ([] () {
	registerTest ("SampleAccurate::Parameter", STR ("Single Change"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 0.);
		ParameterValueQueue queue (pid);
		int32 index = 0;
		queue.addPoint (0, 0., index);
		queue.addPoint (100, 1., index);

		param.beginChanges (&queue);
		param.advance (50);
		if (Test::notEqual (param.getValue (), 0.5))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.advance (50);
		if (Test::notEqual (param.getValue (), 1.))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.endChanges ();

		return true;
	});
	registerTest ("SampleAccurate::Parameter", STR ("Multi Change"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 0.);
		ParameterValueQueue queue (pid);
		int32 index = 0;
		queue.addPoint (0, 0., index);
		queue.addPoint (100, 1., index);
		queue.addPoint (120, 0., index);

		param.beginChanges (&queue);
		param.advance (50);
		if (Test::notEqual (param.getValue (), 0.5))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.advance (50);
		if (Test::notEqual (param.getValue (), 1.))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.advance (20);
		if (Test::notEqual (param.getValue (), 0.))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.endChanges ();

		return true;
	});
	registerTest ("SampleAccurate::Parameter", STR ("Edge"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 0.);
		ParameterValueQueue queue (pid);
		int32 index = 0;
		queue.addPoint (0, 0., index);
		queue.addPoint (1, 1., index);
		queue.addPoint (2, 0., index);

		param.beginChanges (&queue);
		param.advance (2);
		if (Test::notEqual (param.getValue (), 0.))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.endChanges ();

		return true;
	});
	registerTest ("SampleAccurate::Parameter", STR ("Flush"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 0.);
		ParameterValueQueue queue (pid);
		int32 index = 0;
		queue.addPoint (0, 0., index);
		queue.addPoint (256, 1., index);
		queue.addPoint (258, 0.5, index);

		param.beginChanges (&queue);
		param.flushChanges ();
		if (Test::notEqual (param.getValue (), 0.5))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}
		param.endChanges ();

		return true;
	});
	registerTest ("SampleAccurate::Parameter", STR ("Callback"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 0.);
		ParameterValueQueue queue (pid);
		int32 index = 0;
		queue.addPoint (0, 0., index);
		queue.addPoint (128, 0., index);
		queue.addPoint (256, 1., index);
		queue.addPoint (258, 0.5, index);

		param.beginChanges (&queue);
		bool failure = false;
		param.advance (128, [&result, &failure] (auto) {
			result->addErrorMessage (STR ("Unexpected Value"));
			failure = true;
		});
		if (failure)
			return false;
		constexpr auto half = 0.5;
		param.advance (514, [&result, &failure, half = half] (auto value) {
			if (Test::notEqual (value, half))
			{
				result->addErrorMessage (STR ("Unexpected Value"));
				failure = true;
			}
			else
				failure = false;
		});
		if (failure)
			return false;

		param.endChanges ();

		return true;
	});
	registerTest ("SampleAccurate::Parameter", STR ("NoChanges"), [] (ITestResult* result) {
		ParamID pid = 1;
		SampleAccurate::Parameter param (pid, 1.);
		param.endChanges ();

		if (Test::notEqual (param.getValue (), 1.))
		{
			result->addErrorMessage (STR ("Unexpected Value"));
			return false;
		}

		return true;
	});
});

//------------------------------------------------------------------------
} // Vst
} // Steinberg
