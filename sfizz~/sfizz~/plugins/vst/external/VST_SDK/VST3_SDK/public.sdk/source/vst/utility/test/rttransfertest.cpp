//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/utility/test/rtstatetransfertest.cpp
// Created by  : Steinberg, 04/2021
// Description : Realtime State Transfer
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
#include "public.sdk/source/vst/utility/rttransfer.h"
#include "public.sdk/source/vst/utility/testing.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {
namespace {
//------------------------------------------------------------------------
using ParameterVector = std::vector<std::pair<ParamID, ParamValue>>;
using RTTransfer = RTTransferT<ParameterVector>;

//------------------------------------------------------------------------
struct RaceConditionTestObject
{
	static std::atomic<uint32> numDeletes;
//------------------------------------------------------------------------
	struct MyDeleter
	{
		void operator () (double* v) const noexcept
		{
			delete v;
			++numDeletes;
		}
	};

	RTTransferT<double, MyDeleter> transfer;
	std::thread thread;
	std::mutex m1;
	std::mutex m2;
	std::condition_variable c1;

	bool test (ITestResult* result)
	{
		numDeletes = 0;
		{
			auto obj1 = std::unique_ptr<double, MyDeleter> (new double (0.5));
			auto obj2 = std::unique_ptr<double, MyDeleter> (new double (1.));
			transfer.transferObject_ui (std::move (obj1));
			m2.lock ();
			thread = std::thread ([&] () {
				transfer.accessTransferObject_rt ([&] (const double&) {
					c1.notify_all ();
					m2.lock ();
					m2.unlock ();
				});
				transfer.accessTransferObject_rt ([&] (const double&) {});
			});
			std::unique_lock<std::mutex> lm1 (m1);
			c1.wait (lm1);
			transfer.transferObject_ui (std::move (obj2));
			m2.unlock ();

			thread.join ();
			transfer.clear_ui ();
		}
		return numDeletes == 2;
	}
};
std::atomic<uint32> RaceConditionTestObject::numDeletes {0};

//------------------------------------------------------------------------
static std::atomic<uint32> CustomDeleterCallCount;
struct CustomDeleter
{
	template <typename T>
	void operator () (T* v) const noexcept
	{
		delete v;
		++CustomDeleterCallCount;
	}
};

//------------------------------------------------------------------------
ModuleInitializer InitStateTransferTests ([] () {
	registerTest ("RTTransfer", STR ("Simple Transfer"), [] (ITestResult*) {
		RTTransfer helper;
		auto list = std::make_unique<ParameterVector> ();
		list->emplace_back (std::make_pair (0, 1.));
		helper.transferObject_ui (std::move (list));
		bool success = false;
		constexpr double one = 1.;
		helper.accessTransferObject_rt ([&success, one = one] (const auto& list) {
			if (list.size () == 1)
			{
				if (list[0].first == 0)
				{
					if (Test::equal (one, list[0].second))
					{
						success = true;
					}
				}
			}
		});

		list = std::make_unique<ParameterVector> ();
		list->emplace_back (std::make_pair (0, 1.));
		helper.transferObject_ui (std::move (list));
		helper.accessTransferObject_rt ([] (auto&) {});
		list = std::make_unique<ParameterVector> ();
		list->emplace_back (std::make_pair (0, 1.));
		helper.transferObject_ui (std::move (list));
		helper.accessTransferObject_rt ([] (auto&) {});
		return success;
	});
	registerTest ("RTTransfer", STR ("CheckRaceCondition"), [] (ITestResult* r) {
		RaceConditionTestObject obj;
		return obj.test (r);
	});
	registerTest ("RTTransfer", STR ("Custom Deleter"), [] (ITestResult* result) {
		CustomDeleterCallCount = 0;
		RTTransferT<double, CustomDeleter> transfer;
		auto obj1 = std::unique_ptr<double, CustomDeleter> (new double (1.));
		transfer.transferObject_ui (std::move (obj1));
		if (CustomDeleterCallCount != 0)
		{
			return false;
		}
		transfer.clear_ui ();
		return CustomDeleterCallCount == 1;
	});
});

//------------------------------------------------------------------------
} // Anonymous
} // Vst
} // Steinberg
