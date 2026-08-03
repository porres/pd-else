// Copyright 2016 Emilie Gillet.
//
// Internal Envelope.

#ifndef PLAITS_DSP_ENVELOPE_H_
#define PLAITS_DSP_ENVELOPE_H_

#include "stmlib/stmlib.h"

namespace plaits {

class DecayEnvelope {
 public:
  DecayEnvelope() { }
  ~DecayEnvelope() { }
  
  inline void Init() {
    value_ = 0.0f;
  }
  
  inline void Trigger() {
    value_ = 1.0f;
  }
  
  inline void Process(float decay) {
    value_ *= (1.0f - decay);
  }
  
  inline float value() const { return value_; }
  
 private:
  float value_;
  
  DISALLOW_COPY_AND_ASSIGN(DecayEnvelope);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENVELOPE_H_
