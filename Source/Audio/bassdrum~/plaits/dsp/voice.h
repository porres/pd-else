// Copyright 2016 Emilie Gillet.
//
// Main synthesis voice.

#ifndef PLAITS_DSP_VOICE_H_
#define PLAITS_DSP_VOICE_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/limiter.h"
#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/utils/buffer_allocator.h"
#include "plaits/dsp/engine/bass_drum.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/engine/hi_hat_engine.h"
#include "plaits/dsp/engine/snare_drum_engine.h"
#include "plaits/dsp/envelope.h"

namespace plaits {
    
    const int kMaxEngines = 4;
    
    class ChannelPostProcessor {
    public:
        ChannelPostProcessor() { }
        ~ChannelPostProcessor() { }
        
        void Init() {
            Reset();
        }
        
        void Reset() {
            limiter_.Init();
        }
        
        void Process(
                     float gain,
                     bool bypass_lpg,
                     float low_pass_gate_gain,
                     float low_pass_gate_frequency,
                     float low_pass_gate_hf_bleed,
                     float* in,
                     short* out,
                     size_t size,
                     size_t stride) {
            if (gain < 0.0f) {
                limiter_.Process(-gain, in, size);
            }
            const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;
            
            while (size--) {
                *out = stmlib::Clip16(1 + static_cast<int32_t>(*in++ * post_gain));
                out += stride;
            }
            
        }
        
    private:
        stmlib::Limiter limiter_;
        
        DISALLOW_COPY_AND_ASSIGN(ChannelPostProcessor);
    };
    
    struct Patch {
        float note;
        float harmonics;
        float timbre;
        float morph;
        float frequency_modulation_amount;
        float timbre_modulation_amount;
        float morph_modulation_amount;
        int engine;
        float decay;
        float lpg_colour;
    };
    
    struct Modulations {
        float engine;
        float note;
        float frequency;
        float harmonics;
        float timbre;
        float morph;
        float trigger;
        float level;
        bool frequency_patched;
        bool timbre_patched;
        bool morph_patched;
        bool trigger_patched;
        bool level_patched;
    };
    
// char (*__foo)[sizeof(HiHatEngine)] = 1;


    class Voice {
    public:
        Voice() {}
        ~Voice() {}
        
        struct Frame {
            short out;
            short aux;
        };
        
        void Init(stmlib::BufferAllocator* allocator);
        void FreeEngines();
        void Render(
                    const Patch& patch,
                    const Modulations& modulations,
                    Frame* frames,
                    size_t size);
        inline int active_engine() const { return previous_engine_index_; }
        
    private:
        void ComputeDecayParameters(const Patch& settings);
        
        inline float ApplyModulations(float base_value,
                                      float modulation_amount,
                                      bool use_external_modulation,
                                      float external_modulation,
                                      bool use_internal_envelope,
                                      float envelope,
                                      float default_internal_modulation,
                                      float minimum_value,
                                      float maximum_value) {
            float value = base_value;
            modulation_amount *= std::max(fabsf(modulation_amount) - 0.05f, 0.05f);
            modulation_amount *= 1.05f;
            
            float modulation = use_external_modulation
            ? external_modulation : (use_internal_envelope ? envelope : default_internal_modulation);
            value += modulation_amount * modulation;
            CONSTRAIN(value, minimum_value, maximum_value);
            return value;
        }
        
        BassDrumEngine* bass_drum_engine_;
        HiHatEngine* hi_hat_engine_;
        SnareDrumEngine* snare_drum_engine_;

        stmlib::HysteresisQuantizer2 engine_quantizer_;
        
        int previous_engine_index_;
        float engine_cv_;
        
        float previous_note_;
        bool trigger_state_;
        
        DecayEnvelope decay_envelope_;
        
        ChannelPostProcessor out_post_processor_;
        ChannelPostProcessor aux_post_processor_;
        
        EngineRegistry<kMaxEngines> engines_;
        
        float out_buffer_[kMaxBlockSize];
        float aux_buffer_[kMaxBlockSize];
        
        DISALLOW_COPY_AND_ASSIGN(Voice);
    };
    
}  // namespace plaits

#endif  // PLAITS_DSP_VOICE_H_
