// Copyright 2016 Emilie Gillet.
//
// Base class for all engines.

#ifndef PLAITS_DSP_ENGINE_ENGINE_H_
#define PLAITS_DSP_ENGINE_ENGINE_H_

#include "plaits/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/buffer_allocator.h"

namespace plaits {

inline float NoteToFrequency(float midi_note) {
  midi_note -= 9.0f;
  CONSTRAIN(midi_note, -128.0f, 127.0f);
  return a0 * 0.25f * stmlib::SemitonesToRatio(midi_note);
}

enum TriggerState {
  TRIGGER_LOW = 0,
  TRIGGER_RISING_EDGE = 1,
  TRIGGER_UNPATCHED = 2,
  TRIGGER_HIGH = 4,
};

struct EngineParameters {
  int trigger;
  float note;
  float timbre;
  float morph;
  float harmonics;
  float accent;
};

struct PostProcessingSettings {
  // A negative value indicates that a limiter must be used.
  float out_gain;
  float aux_gain;
  // When this flag is true (eg: 808 kick), the synth bypasses the LPG.
  bool already_enveloped;
};

class Engine {
 public:
  Engine() { }
  ~Engine() { }
  virtual void Init(stmlib::BufferAllocator* allocator) = 0;
  virtual void Reset() = 0;
  virtual void LoadUserData(const uint8_t* user_data) = 0;
  virtual void Render(
      const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) = 0;
  PostProcessingSettings post_processing_settings;
};

template<int max_size>
class EngineRegistry {
 public:
  EngineRegistry() { }
  ~EngineRegistry() { }
  
  void Init() {
    num_engines_ = 0;
  }

  inline Engine* get(int index) {
    return engine_[index];
  }
  
  void RegisterInstance(
      Engine* instance,
      bool already_enveloped,
      float out_gain,
      float aux_gain) {
    if (num_engines_ >= max_size) {
      return;
    }
    engine_[num_engines_] = instance;
    PostProcessingSettings* s = &instance->post_processing_settings;
    s->already_enveloped = already_enveloped;
    s->out_gain = out_gain;
    s->aux_gain = aux_gain;
    ++num_engines_;
  }
  
  inline int size() const { return num_engines_; }

 private:
  Engine* engine_[max_size];
  int num_engines_;
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_ENGINE_H_
