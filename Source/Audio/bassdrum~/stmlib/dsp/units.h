// Copyright 2014 Emilie Gillet.
//
// Conversion from semitones to frequency ratio.

#ifndef STMLIB_DSP_UNITS_H_
#define STMLIB_DSP_UNITS_H_

#include "stmlib/stmlib.h"
#include <math.h>

namespace stmlib {

inline float SemitonesToRatio(float semitones){
  return pow(2, semitones / 12.f);
}

}  // namespace stmlib

#endif  // STMLIB_DSP_UNITS_H_
