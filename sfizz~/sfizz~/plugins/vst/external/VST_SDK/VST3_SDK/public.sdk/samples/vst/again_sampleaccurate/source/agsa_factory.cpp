//------------------------------------------------------------------------
// Flags       : clang-format SMTGSequencer
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again_sampleaccurate/source/agsa_factory.cpp
// Created by  : Steinberg, 04/2021
// Description : AGain with Sample Accurate Parameter Changes
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

#include "agsa.h"
#include "tutorial.h"
#include "version.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/utility/testing.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#define stringPluginName "AGain Sample Accurate"

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace Steinberg::Vst::AgainSampleAccurate;

//------------------------------------------------------------------------
//  VST Plug-in Factory
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail)

// AGain sample accurate
DEF_CLASS2 (INLINE_UID_FROM_FUID (ProcessorID), PClassInfo::kManyInstances, kVstAudioEffectClass,
            stringPluginName, Vst::kDistributable, "Fx", FULL_VERSION_STR, kVstVersionString,
            createProcessorInstance)

DEF_CLASS2 (INLINE_UID_FROM_FUID (ControllerID), PClassInfo::kManyInstances,
            kVstComponentControllerClass, stringPluginName "Controller", 0, "", FULL_VERSION_STR,
            kVstVersionString, createControllerInstance)

// Test
DEF_CLASS2 (INLINE_UID_FROM_FUID (getTestFactoryUID ()), PClassInfo::kManyInstances, kTestClass,
            stringPluginName "Test Factory", 0, "", "", "", createTestFactoryInstance)

// Tutorial
DEF_CLASS2 (INLINE_UID_FROM_FUID (Tutorial::ProcessorID), PClassInfo::kManyInstances,
            kVstAudioEffectClass, "Advanced Tutorial", Vst::kDistributable, "Fx", FULL_VERSION_STR,
            kVstVersionString, Tutorial::createProcessorInstance)

DEF_CLASS2 (INLINE_UID_FROM_FUID (Tutorial::ControllerID), PClassInfo::kManyInstances,
            kVstComponentControllerClass, "Advanced Tutorial Controller", 0, "", FULL_VERSION_STR,
            kVstVersionString, Tutorial::createControllerInstance)

END_FACTORY
