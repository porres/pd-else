//-----------------------------------------------------------------------------
// Project     : SDK Core
//
// Category    : Common Base Classes
// Filename    : public.sdk/source/main/moduleinit.cpp
// Created by  : Steinberg, 11/2020
// Description : Module Initializers/Terminators
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
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "moduleinit.h"
#include <atomic>
#include <vector>

extern void* moduleHandle; // from dllmain.cpp, linuxmain.cpp or macmain.cpp

//------------------------------------------------------------------------
namespace Steinberg {
namespace {

//------------------------------------------------------------------------
using FunctionVector = std::vector<std::pair<ModuleInitPriority, ModuleInitFunction>>;

//------------------------------------------------------------------------
FunctionVector& getInitFunctions ()
{
	static FunctionVector gInitVector;
	return gInitVector;
}

//------------------------------------------------------------------------
FunctionVector& getTermFunctions ()
{
	static FunctionVector gTermVector;
	return gTermVector;
}

//------------------------------------------------------------------------
void addInitFunction (ModuleInitFunction&& func, ModuleInitPriority prio)
{
	getInitFunctions ().emplace_back (prio, std::move (func));
}

//------------------------------------------------------------------------
void addTerminateFunction (ModuleInitFunction&& func, ModuleInitPriority prio)
{
	getTermFunctions ().emplace_back (prio, std::move (func));
}

//------------------------------------------------------------------------
void sortAndRunFunctions (FunctionVector& array)
{
	std::sort (array.begin (), array.end (),
	           [] (const FunctionVector::value_type& v1, const FunctionVector::value_type& v2) {
		           return v1.first < v2.first;
	           });
	for (auto& entry : array)
		entry.second ();
}

//------------------------------------------------------------------------
} // anonymous

//------------------------------------------------------------------------
ModuleInitializer::ModuleInitializer (ModuleInitFunction&& func, ModuleInitPriority prio)
{
	addInitFunction (std::move (func), prio);
}

//------------------------------------------------------------------------
ModuleTerminator::ModuleTerminator (ModuleInitFunction&& func, ModuleInitPriority prio)
{
	addTerminateFunction (std::move (func), prio);
}

//------------------------------------------------------------------------
PlatformModuleHandle getPlatformModuleHandle ()
{
	return reinterpret_cast<PlatformModuleHandle> (moduleHandle);
}

//------------------------------------------------------------------------
} // Steinberg

//------------------------------------------------------------------------
bool InitModule ()
{
	Steinberg::sortAndRunFunctions (Steinberg::getInitFunctions ());
	return true;
}

//------------------------------------------------------------------------
bool DeinitModule ()
{
	Steinberg::sortAndRunFunctions (Steinberg::getTermFunctions ());
	return true;
}
