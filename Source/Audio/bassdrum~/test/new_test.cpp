// Renders one hit through the trimmed Voice, exactly as plaits~ would (12-sample blocks).
// usage: new_test hz punch tone decay penv level(-1 = unpatched) aux(0/1) out.raw
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/voice.h"

int main(int argc, char** argv) {
  float hz = 50, punch = .5f, tone = .5f, decay = .5f, penv = 0, level = -1;
  int aux = 0;
  const char* path = "new.raw";
  if (argc > 1) hz = atof(argv[1]);
  if (argc > 2) punch = atof(argv[2]);
  if (argc > 3) tone = atof(argv[3]);
  if (argc > 4) decay = atof(argv[4]);
  if (argc > 5) penv = atof(argv[5]);
  if (argc > 6) level = atof(argv[6]);
  if (argc > 7) aux = atoi(argv[7]);
  if (argc > 8) path = argv[8];

  plaits::kSampleRate = 48000.f;
  plaits::a0 = 55.f / plaits::kSampleRate;
  static plaits::Voice v;
  v.Init();

  plaits::Patch p = {};
  plaits::Modulations m = {};
  float pitch = log2f(fabsf(hz) / 440) + 0.75;
  p.note = 60.0f + pitch * 12.f;
  p.harmonics = punch;
  p.timbre = tone;
  p.morph = decay;
  p.decay = p.frequency_modulation_amount = penv;
  m.level = level < 0 ? 0 : level;
  m.level_patched = level >= 0;

  float peak_pos = 0, peak_neg = 0;
  FILE* f = fopen(path, "wb");
  for (int b = 0; b < 48000 / 12; ++b) {
    m.trigger = (b == 10) ? 1.f : 0.f;
    plaits::Voice::Frame fr[12];
    v.Render(p, m, fr, 12);
    for (int i = 0; i < 12; ++i) {
      float s = (aux ? fr[i].aux : fr[i].out) / 32768.0f;
      if (s > peak_pos) peak_pos = s;
      if (s < peak_neg) peak_neg = s;
      fwrite(&s, 4, 1, f);
    }
  }
  fclose(f);
  printf("NEW  peak+ %.4f  peak- %.4f\n", peak_pos, peak_neg);
  return 0;
}
