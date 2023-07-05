//-----------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/utility/rttransfer.h
// Created by  : Steinberg, 04/2021
// Description : Realtime Object Transfer
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

#include <array>
#include <atomic>
#include <cassert>
#include <memory>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
/** Transfer objects from a non realtime thread to a realtime one
 *
 *	You have to use it from two threads, the realtime context thread where you are not allowed to
 *	block and a non realtime thread from where the object is coming.
 *
 *	It's guaranteed that the function you should only call in the realtime context is wait free and
 *	does not do any allocations or deallocations
 *
 */
template <typename ObjectT, typename Deleter = std::default_delete<ObjectT>>
struct RTTransferT
{
	using ObjectType = ObjectT;
	using ObjectTypePtr = std::unique_ptr<ObjectType, Deleter>;

	RTTransferT () { assert (storage[0].is_lock_free ()); }
	~RTTransferT () noexcept { clear_ui (); }

	/** Access the transfer object.
	 *
	 * 	If there's a new object, the proc is called with the new object. The object is only valid
	 *	inside the proc.
	 *
	 *	To be called from the realtime context.
	 */
	template <typename Proc>
	void accessTransferObject_rt (Proc proc) noexcept
	{
		ObjectType* newObject {nullptr};
		ObjectType* currentObject = storage[0].load ();
		if (currentObject && storage[0].compare_exchange_strong (currentObject, newObject))
		{
			proc (*currentObject);
			ObjectType* transitObj = storage[1].load ();
			if (storage[1].compare_exchange_strong (transitObj, currentObject) == false)
			{
				assert (false);
			}
			ObjectType* oldObject = storage[2].load ();
			if (storage[2].compare_exchange_strong (oldObject, transitObj) == false)
			{
				assert (false);
			}
		}
	}

	/** Transfer an object to the realtime context.
	 *
	 * 	The ownership of newObject is transfered to this object and the Deleter is used to free
	 *	the memory of it afterwards.
	 *
	 *	If there's already an object in transfer the previous object will be deallocated and
	 *	replaced with the new one without passing to the realtime context.
	 *
	 *	To be called from the non realtime context.
	 */
	void transferObject_ui (ObjectTypePtr&& newObjectPtr)
	{
		ObjectType* newObject = newObjectPtr.release ();
		clear_ui ();
		while (true)
		{
			ObjectType* currentObject = storage[0].load ();
			if (storage[0].compare_exchange_strong (currentObject, newObject))
			{
				deallocate (currentObject);
				break;
			}
		}
	}

	/** Clear the transfer.
	 *
	 *	To be called from the non realtime context.
	 */
	void clear_ui ()
	{
		clearStorage (storage[0]);
		clearStorage (storage[1]);
		clearStorage (storage[2]);
	}

private:
	using AtomicObjectPtr = std::atomic<ObjectType*>;

	void clearStorage (AtomicObjectPtr& atomObj)
	{
		ObjectType* newObject = nullptr;
		while (ObjectType* current = atomObj.load ())
		{
			if (atomObj.compare_exchange_strong (current, newObject))
			{
				deallocate (current);
				break;
			}
		}
	}

	void deallocate (ObjectType* object)
	{
		if (object)
		{
			Deleter d;
			d (object);
		}
	}

	std::array<AtomicObjectPtr, 3> storage {};
};

//------------------------------------------------------------------------
} // Vst
} // Steinberg
