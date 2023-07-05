//-----------------------------------------------------------------------------
// Project     : SDK Core
//
// Category    : Common Base Classes
// Filename    : public.sdk/source/main/moduleinit.h
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

#pragma once

#include "pluginterfaces/base/ftypes.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>

//------------------------------------------------------------------------
/** A replacement for InitModule and DeinitModule
 *
 *	If you link this file the InitModule and DeinitModule functions are 
 *	implemented and you can use this to register functions that will be 
 *	called when the module is loaded and before the module is unloaded.
 *
 *	Use this for one time initializers or cleanup functions.
 *	For example: if you depend on a 3rd party library that needs
 *	initialization before you can use it you can write an initializer like this:
 *
 *	static ModuleInitializer InitMyExternalLib ([] () { MyExternalLib::init (); });
 *
 *	Or you have a lazy create wavetable you need to free the allocated memory later:
 *
 *	static ModuleTerminator FreeWaveTableMemory ([] () { MyWaveTable::free (); });
 */

//------------------------------------------------------------------------
#if SMTG_OS_WINDOWS
using HINSTANCE = struct HINSTANCE__*;
namespace Steinberg { using PlatformModuleHandle = HINSTANCE; }
//------------------------------------------------------------------------
#elif SMTG_OS_OSX || SMTG_OS_IOS
typedef struct __CFBundle* CFBundleRef;
namespace Steinberg { using PlatformModuleHandle = CFBundleRef; }
//------------------------------------------------------------------------
#elif SMTG_OS_LINUX
namespace Steinberg { using PlatformModuleHandle = void*; }
#endif

//------------------------------------------------------------------------
namespace Steinberg {

using ModuleInitFunction = std::function<void ()>;
using ModuleInitPriority = uint32;
static constexpr ModuleInitPriority DefaultModulePriority =
    std::numeric_limits<ModuleInitPriority>::max () / 2;

//------------------------------------------------------------------------
struct ModuleInitializer
{
	/**
	 *	Register a function which is called when the module is loaded
	 *	@param func function to call
	 *	@param prio priority
	 */
	ModuleInitializer (ModuleInitFunction&& func,
	                   ModuleInitPriority prio = DefaultModulePriority);
};

//------------------------------------------------------------------------
struct ModuleTerminator
{
	/**
	 *	Register a function which is called when the module is unloaded
	 *	@param func function to call
	 *	@param prio priority
	 */
	ModuleTerminator (ModuleInitFunction&& func,
	                  ModuleInitPriority prio = DefaultModulePriority);
};

//------------------------------------------------------------------------
PlatformModuleHandle getPlatformModuleHandle ();

//------------------------------------------------------------------------
} // Steinberg

