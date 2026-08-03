// Copyright 2014 Emilie Gillet.
//
// Distortion/overdrive.

#ifndef PLAITS_DSP_FX_OVERDRIVE_H_
#define PLAITS_DSP_FX_OVERDRIVE_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {
  
class Overdrive {
 public:
  Overdrive() { }
  ~Overdrive() { }
  void Init() {
    pre_gain_ = 0.0f;
    post_gain_ = 0.0f;
  }
  void Process(float drive, float* in_out, size_t size) {
    const float drive_2 = drive * drive;
    const float pre_gain_a = drive * 0.5f;
    const float pre_gain_b = drive_2 * drive_2 * drive * 24.0f;
    const float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;
    const float drive_squashed = drive * (2.0f - drive);
    const float post_gain = 1.0f / stmlib::SoftClip(0.33f + drive_squashed * (pre_gain - 0.33f));
    stmlib::ParameterInterpolator pre_gain_modulation(&pre_gain_, pre_gain, size);
    stmlib::ParameterInterpolator post_gain_modulation(&post_gain_, post_gain, size);
    while(size--){
      float pre = pre_gain_modulation.Next() * *in_out;
      *in_out++ = stmlib::SoftClip(pre) * post_gain_modulation.Next();
    }
  }
 private:
  float pre_gain_;
  float post_gain_;
  DISALLOW_COPY_AND_ASSIGN(Overdrive);
};
}  // namespace plaits

#endif  // PLAITS_DSP_FX_OVERDRIVE_H_
