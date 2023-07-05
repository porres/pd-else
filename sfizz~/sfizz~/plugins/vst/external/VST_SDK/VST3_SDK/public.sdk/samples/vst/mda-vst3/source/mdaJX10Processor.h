/*
 *  mdaJX10Processor.h
 *  mda-vst3
 *
 *  Created by Arne Scheffler on 6/14/08.
 *
 *  mda VST Plug-ins
 *
 *  Copyright (c) 2008 Paul Kellett
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include "mdaBaseProcessor.h"

namespace Steinberg {
namespace Vst {
namespace mda {

//-----------------------------------------------------------------------------
class JX10Processor : public BaseProcessor
{
public:
	using Base = BaseProcessor;

	JX10Processor ();
	~JX10Processor ();
	
	int32 getVst2UniqueId () const SMTG_OVERRIDE { return 'MDAj'; }

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;

	void doProcessing (ProcessData& data) SMTG_OVERRIDE;

	bool hasProgram () const SMTG_OVERRIDE { return true; }
	Steinberg::uint32 getCurrentProgram () const SMTG_OVERRIDE { return currentProgram; }
	Steinberg::uint32 getNumPrograms () const SMTG_OVERRIDE { return kNumPrograms; }
	void setCurrentProgram (Steinberg::uint32 val) SMTG_OVERRIDE;
	void setCurrentProgramNormalized (ParamValue val) SMTG_OVERRIDE;

	static const int32 kNumPrograms = 52;
	static float programParams[kNumPrograms][24];

//-----------------------------------------------------------------------------
	static FUnknown* createInstance (void*) { return (IAudioProcessor*)new JX10Processor; }
#ifdef SMTG_MDA_VST2_COMPATIBILITY
	inline static DECLARE_UID (uid, 0x5653544D, 0x44416A6D, 0x6461206A, 0x78313000);
#else
	inline static DECLARE_UID (uid, 0x82CD49DE, 0x13D743BA, 0xABDAC299, 0x1CE06F7C);
#endif

//-----------------------------------------------------------------------------
protected:
	struct VOICE  //voice state
	{
		float  period;
		float  p;    //sinc position
		float  pmax; //loop length
		float  dp;   //delta
		float  sin0; //sine osc
		float  sin1;
		float  sinx;
		float  dc;   //dc offset

		float  detune;
		float  p2;    //sinc position
		float  pmax2; //loop length
		float  dp2;   //delta
		float  sin02; //sine osc
		float  sin12;
		float  sinx2;
		float  dc2;   //dc offset

		float  fc;  //filter cutoff root
		float  ff;  //filter cutoff
		float  f0;  //filter buffers
		float  f1;
		float  f2;

		float  saw;
		//float  vca;  //current level  ///eliminate osc1 level when separate amp & filter envs?
		//float  env;  //envelope
		//float  att;  //attack
		//float  dec;  //decay
		float  env;
		float  envd;
		float  envl;
		float  fenv;
		float  fenvd;
		float  fenvl;

		float  lev;  //osc levels
		float  lev2;
		float  target; //period target
		int32  note; //remember what note triggered this
		int32 noteID;	// SNA addition
		float snaPitchbend;// SNA addition
		float snaVolume;// SNA addition
		float snaPanLeft;	// SNA addition
		float snaPanRight;	// SNA addition
	};

	static constexpr int32 kEventBufferSize = 64;
	static constexpr int32 kNumVoices = 8;
	using SynthDataT = SynthData<VOICE, kEventBufferSize, kNumVoices>;

	void preProcess () SMTG_OVERRIDE;
	void processEvent (const Event& event) SMTG_OVERRIDE;
	void recalculate () SMTG_OVERRIDE;
	void noteEvent (const Event& event);
	void setParameter (ParamID index, ParamValue newValue, int32 sampleOffset) SMTG_OVERRIDE;
	void clearVoice (VOICE& v);


	SynthDataT synthData;
	static const int32 KMAX = 32;

	///global internal variables
	float semi, cent;
	float tune, detune;
	float filtf, fzip, filtq, filtlfo, filtenv, filtvel, filtwhl;
	float oscmix, noisemix;
	float att, dec, sus, rel, fatt, fdec, fsus, frel;
	float lfo, dlfo, modwhl, press, pbend, ipbend, rezwhl;
	float velsens, volume, voltrim;
	float vibrato, pwmdep, lfoHz, glide, glidedisp;
	int32  K, lastnote, veloff, mode;
	Steinberg::uint32 noise;

	Steinberg::uint32 currentProgram;
};

}}} // namespaces
