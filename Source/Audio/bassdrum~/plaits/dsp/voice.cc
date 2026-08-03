// Copyright 2016 Emilie Gillet.
//
// Main synthesis voice.

#include "plaits/dsp/voice.h"

namespace plaits {
    
    using namespace std;
    using namespace stmlib;
    
    void Voice::FreeEngines() {
        delete bass_drum_engine_;
        delete snare_drum_engine_;
        delete hi_hat_engine_;
    }
    
    void Voice::Init(BufferAllocator* allocator) {
        bass_drum_engine_ = new BassDrumEngine();
        snare_drum_engine_ = new SnareDrumEngine();
        hi_hat_engine_ = new HiHatEngine();
        
        engines_.Init();

        engines_.RegisterInstance(bass_drum_engine_, true, 0.8f, 0.8f);
        engines_.RegisterInstance(snare_drum_engine_, true, 0.8f, 0.8f);
        engines_.RegisterInstance(hi_hat_engine_, true, 0.8f, 0.8f);

        for (int i = 0; i < engines_.size(); ++i) {
            // All engines will share the same RAM space.
            allocator->Free();
            engines_.get(i)->Init(allocator);
        }

        engine_quantizer_.Init(engines_.size(), 0.05f, true);
        previous_engine_index_ = -1;
        engine_cv_ = 0.0f;
        
        out_post_processor_.Init();
        aux_post_processor_.Init();
        
        decay_envelope_.Init();
        
        trigger_state_ = false;
        previous_note_ = 0.0f;
        
    }
    
    void Voice::Render(const Patch& patch,
                       const Modulations& modulations,
                       Frame* frames,
                       size_t size) {
        // Trigger, internal envelope.
        
        float trigger_value = modulations.trigger;
        
        bool previous_trigger_state = trigger_state_;
        if (!previous_trigger_state) {
            if (trigger_value > 0.0f) {
                trigger_state_ = true;
                decay_envelope_.Trigger();
                engine_cv_ = modulations.engine;
            }
        } else {
            if (trigger_value <= 0.0f) {
                trigger_state_ = false;
            }
        }
        
        // Engine selection.
        int engine_index = engine_quantizer_.Process(patch.engine, engine_cv_);
        
        Engine* e = engines_.get(engine_index);
        
        if (engine_index != previous_engine_index_) {
            e->Reset();
            out_post_processor_.Reset();
            previous_engine_index_ = engine_index;
        }
        EngineParameters p;
        
        bool rising_edge = trigger_state_ && !previous_trigger_state;
        float note = (modulations.note + previous_note_) * 0.5f;
        previous_note_ = modulations.note;
        const PostProcessingSettings& pp_s = e->post_processing_settings;
        
        p.trigger = (rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW) | \
            (trigger_state_ ? TRIGGER_HIGH : TRIGGER_LOW);
        
        const float short_decay = (200.0f * kBlockSize) / kSampleRate *
        SemitonesToRatio(-96.0f * patch.decay);
        
        decay_envelope_.Process(short_decay * 2.0f);
        
        float compressed_level = 1.3f * modulations.level / (0.3f + fabsf(modulations.level));
        CONSTRAIN(compressed_level, 0.0f, 1.0f);
        p.accent = modulations.level_patched ? compressed_level : 0.8f;
        
        bool use_internal_envelope = 1;
        
        // Actual synthesis parameters.
        
        p.harmonics = patch.harmonics + modulations.harmonics;
        CONSTRAIN(p.harmonics, 0.0f, 1.0f);
        
        float internal_envelope_amplitude = 1.0f;
        float internal_envelope_amplitude_timbre = 1.0f;
        
        p.note = ApplyModulations(patch.note + note,
                                  patch.frequency_modulation_amount,
                                  modulations.frequency_patched,
                                  modulations.frequency,
                                  use_internal_envelope,
                                  internal_envelope_amplitude * \
                                  decay_envelope_.value() * decay_envelope_.value() * 48.0f,
                                  1.0f,
                                  -119.0f,
                                  120.0f);
        
        p.timbre = ApplyModulations(patch.timbre,
                                    patch.timbre_modulation_amount,
                                    modulations.timbre_patched,
                                    modulations.timbre,
                                    use_internal_envelope,
                                    internal_envelope_amplitude_timbre * decay_envelope_.value(),
                                    0.0f,
                                    0.0f,
                                    1.0f);
        
        p.morph = ApplyModulations(patch.morph,
                                   patch.morph_modulation_amount,
                                   modulations.morph_patched,
                                   modulations.morph,
                                   use_internal_envelope,
                                   internal_envelope_amplitude * decay_envelope_.value(),
                                   0.0f,
                                   0.0f,
                                   1.0f);
        
        bool already_enveloped = pp_s.already_enveloped;
        e->Render(p, out_buffer_, aux_buffer_, size, &already_enveloped);
        
        out_post_processor_.Process(pp_s.out_gain,
                                    1,
                                    0,
                                    0,
                                    0,
                                    out_buffer_,
                                    &frames->out,
                                    size,
                                    2);
        
        aux_post_processor_.Process(pp_s.aux_gain,
                                    1,
                                    0,
                                    0,
                                    0,
                                    aux_buffer_,
                                    &frames->aux,
                                    size,
                                    2);
    }
    
}  // namespace plaits
