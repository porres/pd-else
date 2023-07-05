/*
 *  mdaPianoProcessor.h
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
class PianoProcessor : public BaseProcessor
{
public:
	using Base = BaseProcessor;

	PianoProcessor ();
	~PianoProcessor ();
	
	int32 getVst2UniqueId () const SMTG_OVERRIDE { return 'MDAp'; }

	tresult PLUGIN_API initialize (FUnknown* context) SMTG_OVERRIDE;
	tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	tresult PLUGIN_API setActive (TBool state) SMTG_OVERRIDE;

	void doProcessing (ProcessData& data) SMTG_OVERRIDE;

	bool hasProgram () const SMTG_OVERRIDE { return true; }
	Steinberg::uint32 getCurrentProgram () const SMTG_OVERRIDE { return currentProgram; }
	Steinberg::uint32 getNumPrograms () const SMTG_OVERRIDE { return kNumPrograms; }
	void setCurrentProgram (Steinberg::uint32 val) SMTG_OVERRIDE;
	void setCurrentProgramNormalized (ParamValue val) SMTG_OVERRIDE;

//-----------------------------------------------------------------------------
	static FUnknown* createInstance (void*) { return (IAudioProcessor*)new PianoProcessor; }
#ifdef SMTG_MDA_VST2_COMPATIBILITY
	inline static DECLARE_UID (uid, 0x5653544D, 0x4441706D, 0x64612070, 0x69616E6F);
#else
	inline static DECLARE_UID (uid, 0xA77EA27C, 0x1F184322, 0xBBD2AD93, 0x73A2B7A0);
#endif

//-----------------------------------------------------------------------------

	enum {
		NPARAMS=12,
		WAVELEN=586348
	};
	static const int32 kNumPrograms = 8;

	static float programParams[][NPARAMS];

protected:
	void setParameter (ParamID index, ParamValue newValue, int32 sampleOffset) SMTG_OVERRIDE;
	void preProcess () SMTG_OVERRIDE;
	void processEvent (const Event& event) SMTG_OVERRIDE;
	void noteEvent (const Event& event);
	void recalculate () SMTG_OVERRIDE;
	void allNotesOff ();

	struct VOICE  //voice state
	{
		int32 delta;  //sample playback
		int32 frac;
		int32 pos;
		int32 end;
		int32 loop;

		float env;  //envelope
		float dec;

		float f0;   //first-order LPF
		float f1;
		float ff;

		float outl;
		float outr;
		int32 note; //remember what note triggered this
		int32 noteID;
	};


	struct KGRP  //keygroup
	{
		int32  root;  //MIDI root note
		int32  high;  //highest note
		int32  pos;
		int32  end;
		int32  loop;
	};

	float Fs, iFs;

	static constexpr int32 kNumVoices = 32;
	static constexpr int32 kEventBufferSize = 64;
	using SynthDataT = SynthData<VOICE, kEventBufferSize, kNumVoices>;
	SynthDataT synthData;

	///global internal variables
	KGRP  kgrp[16];
	int32 poly, cpos;
	short *waves;
	int32 cmax;
	float *comb, cdep, width, trim;
	int32 size;
	float tune, fine, random, stretch;
	float muff, muffvel, sizevel, velsens, volume;

	Steinberg::uint32 currentProgram;
};

}}} // namespaces
