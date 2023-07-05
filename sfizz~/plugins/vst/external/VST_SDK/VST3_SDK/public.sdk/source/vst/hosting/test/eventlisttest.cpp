//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/eventlisttest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test event list
// Flags       : clang-format SMTGSequencer
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2022, Steinberg Media Technologies GmbH, All Rights Reserved
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
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/utility/testing.h"

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
ModuleInitializer EventListTests ([] () {
	constexpr auto TestSuiteName = "EventList";
	registerTest (TestSuiteName, STR ("Set and get single event"), [] (ITestResult* testResult) {
		EventList eventList;
		Event event1 = {};
		event1.type = Event::kNoteOnEvent;
		event1.noteOn.noteId = 10;
		EXPECT_EQ (eventList.addEvent (event1), kResultTrue);
		Event event2;
		EXPECT_EQ (eventList.getEvent (0, event2), kResultTrue);
		EXPECT_EQ (memcmp (&event1, &event2, sizeof (Event)), 0);
		return true;
	});
	registerTest (TestSuiteName, STR ("Count events"), [] (ITestResult* testResult) {
		EventList eventList;
		Event event = {};
		for (auto i = 0; i < 20; ++i)
		{
			EXPECT_EQ (eventList.addEvent (event), kResultTrue);
		}
		EXPECT_EQ (eventList.getEventCount (), 20);
		return true;
	});
	registerTest (TestSuiteName, STR ("Overflow"), [] (ITestResult* testResult) {
		EventList eventList (20);
		Event event = {};
		for (auto i = 0; i < 20; ++i)
		{
			EXPECT_EQ (eventList.addEvent (event), kResultTrue);
		}
		EXPECT_EQ (eventList.getEventCount (), 20);
		EXPECT_NE (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.getEventCount (), 20);
		return true;
	});
	registerTest (TestSuiteName, STR ("Get unknown event"), [] (ITestResult* testResult) {
		EventList eventList;
		Event event;
		EXPECT_NE (eventList.getEvent (0, event), kResultTrue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Resize"), [] (ITestResult* testResult) {
		EventList eventList (1);
		Event event;
		EXPECT_NE (eventList.getEvent (0, event), kResultTrue);
		event = {};
		EXPECT_EQ (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.getEventCount (), 1);
		EXPECT_NE (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.getEventCount (), 1);
		eventList.setMaxSize (2);
		EXPECT_EQ (eventList.getEventCount (), 0);
		EXPECT_EQ (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.getEventCount (), 2);
		EXPECT_NE (eventList.addEvent (event), kResultTrue);
		EXPECT_EQ (eventList.getEventCount (), 2);
		return true;
	});
});

//------------------------------------------------------------------------
} // anonymous
} // Vst
} // Steinberg
