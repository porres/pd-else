//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/hosting/test/connectionproxytest.cpp
// Created by  : Steinberg, 08/2021
// Description : Test connection proxy
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
#include "public.sdk/source/vst/hosting/connectionproxy.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/utility/testing.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {

//------------------------------------------------------------------------
class ConnectionPoint : public IConnectionPoint
{
public:
	tresult PLUGIN_API connect (IConnectionPoint* inOther) override
	{
		other = inOther;
		return kResultTrue;
	}

	tresult PLUGIN_API disconnect (IConnectionPoint* inOther) override
	{
		if (inOther != other)
			return kResultFalse;
		return kResultTrue;
	}

	tresult PLUGIN_API notify (IMessage*) override
	{
		messageReceived = true;
		return kResultTrue;
	}

	tresult PLUGIN_API queryInterface (const TUID, void**) override { return kNotImplemented; }
	uint32 PLUGIN_API addRef () override { return 100; }
	uint32 PLUGIN_API release () override { return 100; }

	IConnectionPoint* other {nullptr};
	bool messageReceived {false};
};

//------------------------------------------------------------------------
ModuleInitializer ConnectionProxyTests ([] () {
	constexpr auto TestSuiteName = "ConnectionProxy";
	registerTest (TestSuiteName, STR ("Connect and disconnect"), [] (ITestResult* testResult) {
		ConnectionPoint cp1;
		ConnectionPoint cp2;
		ConnectionProxy proxy (&cp1);
		EXPECT_EQ (proxy.connect (&cp2), kResultTrue);
		EXPECT_EQ (proxy.disconnect (&cp2), kResultTrue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Disconnect wrong object"), [] (ITestResult* testResult) {
		ConnectionPoint cp1;
		ConnectionPoint cp2;
		ConnectionPoint cp3;
		ConnectionProxy proxy (&cp1);
		EXPECT_EQ (proxy.connect (&cp2), kResultTrue);
		EXPECT_NE (proxy.disconnect (&cp3), kResultTrue);
		return true;
	});
	registerTest (TestSuiteName, STR ("Send message on UI thread"), [] (ITestResult* testResult) {
		ConnectionPoint cp1;
		ConnectionPoint cp2;
		ConnectionProxy proxy (&cp1);
		EXPECT_EQ (proxy.connect (&cp2), kResultTrue);
		EXPECT_FALSE (cp2.messageReceived);
		HostMessage msg;
		EXPECT_EQ (proxy.notify (&msg), kResultTrue);
		EXPECT_TRUE (cp2.messageReceived);
		return true;
	});
	registerTest (TestSuiteName, STR ("Send message on 2nd thread"), [] (ITestResult* testResult) {
		ConnectionPoint cp1;
		ConnectionPoint cp2;
		ConnectionProxy proxy (&cp1);
		EXPECT_EQ (proxy.connect (&cp2), kResultTrue);
		EXPECT_FALSE (cp2.messageReceived);

		std::condition_variable c1;
		std::mutex m1;
		std::mutex m2;
		std::atomic<tresult> notifyResult {0};

		std::unique_lock<std::mutex> lm1 (m1);
		m2.lock ();
		std::thread thread ([&] () {
			m2.lock ();
			HostMessage msg;
			notifyResult = proxy.notify (&msg);
			c1.notify_all ();
			m2.unlock ();
		});
		m2.unlock ();
		c1.wait (lm1);
		EXPECT_NE (notifyResult, kResultTrue);
		EXPECT_FALSE (cp2.messageReceived);
		thread.join ();
		return true;
	});
});

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
} // Vst
} // Steinberg
