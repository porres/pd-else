// Copyright 2015 Emilie Gillet.
//
// Quantize a float in [0, 1] to an integer in [0, num_steps[. Apply hysteresis
// to prevent jumps near the decision boundary.

#ifndef STMLIB_DSP_HYSTERESIS_QUANTIZER_H_
#define STMLIB_DSP_HYSTERESIS_QUANTIZER_H_

#include "stmlib/stmlib.h"

namespace stmlib {

class HysteresisQuantizer {
 public:
  HysteresisQuantizer() { }
  ~HysteresisQuantizer() { }

  void Init() {
    quantized_value_ = 0;
  }

  inline int Process(float value, int num_steps) {
    return Process(value, num_steps, 0.25f);
  }

  inline int Process(float value, int num_steps, float hysteresis) {
    return Process(0, value, num_steps, hysteresis);
  }

  inline int Process(int base, float value, int num_steps, float hysteresis) {
    value *= static_cast<float>(num_steps - 1);
    value += static_cast<float>(base);
    float hysteresis_feedback = value > static_cast<float>(quantized_value_)
        ? -hysteresis
        : hysteresis;
    int q = static_cast<int>(value + hysteresis_feedback + 0.5f);
    CONSTRAIN(q, 0, num_steps - 1);
    quantized_value_ = q;
    return q;
  }

  template<typename T>
  const T& Lookup(const T* array, float value, int num_steps) {
    return array[Process(value, num_steps)];
  }

 private:
  int quantized_value_;
  
  DISALLOW_COPY_AND_ASSIGN(HysteresisQuantizer);
};


// Note: currently refactoring this aspect of all Mutable Instruments modules.
// The codebase will progressively use only this class, at which point the other
// version will be deprecated

class HysteresisQuantizer2 {
 public:
  HysteresisQuantizer2() { }
  ~HysteresisQuantizer2() { }

  void Init(int num_steps, float hysteresis, bool symmetric) {
    num_steps_ = num_steps;
    hysteresis_ = hysteresis;

    scale_ = static_cast<float>(symmetric ? num_steps - 1 : num_steps);
    offset_ = symmetric ? 0.0f : -0.5f;

    quantized_value_ = 0;
  }

  inline int Process(float value) {
    return Process(0, value);
  }

  inline int Process(int base, float value) {
    value *= scale_;
    value += offset_;
    value += static_cast<float>(base);

    float hysteresis_sign = value > static_cast<float>(quantized_value_)
        ? -1.0f
        : +1.0f;
    int q = static_cast<int>(value + hysteresis_sign * hysteresis_ + 0.5f);
    CONSTRAIN(q, 0, num_steps_ - 1);
    quantized_value_ = q;
    return q;
  }

  template<typename T>
  const T& Lookup(const T* array, float value) {
    return array[Process(value)];
  }
  
  inline int num_steps() const {
    return num_steps_;
  }
  
  inline int quantized_value() const {
    return quantized_value_;
  }

 private:
  int num_steps_;
  float hysteresis_;
  
  float scale_;
  float offset_;

  int quantized_value_;
  
  DISALLOW_COPY_AND_ASSIGN(HysteresisQuantizer2);
};

}  // namespace stmlib

#endif  // STMLIB_DSP_HYSTERESIS_QUANTIZER_H_
