// Copyright 2019 Emilie Gillet.
//
// Threshold type filter that filters out slow jumps of a value.

#ifndef STMLIB_DSP_HYSTERESIS_FILTER_H_
#define STMLIB_DSP_HYSTERESIS_FILTER_H_

#include "stmlib/stmlib.h"

namespace stmlib {

class HysteresisFilter {
 public:
  HysteresisFilter() { }
  ~HysteresisFilter() { }

  void Init(float threshold) {
    value_ = 0.0f;
    threshold_ = threshold;
  }
  
  inline float Process(float value) {
    return Process(value, threshold_);
  }

  inline float Process(float value, float threshold) {
    if (threshold == 0.0f) {
      value_ = value;
    } else {
      float error = value - value_;
      if (error > threshold) {
        value_ = value - threshold;
      } else if (error < -threshold) {
        value_ = value + threshold;
      }
    }
    
    return value_;
  }

  inline float value() const { return value_; }

 private:
  float value_;
  float threshold_;
  
  DISALLOW_COPY_AND_ASSIGN(HysteresisFilter);
};


}  // namespace stmlib

#endif  // STMLIB_DSP_HYSTERESIS_FILTER_H_

