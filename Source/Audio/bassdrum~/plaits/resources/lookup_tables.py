#!/usr/bin/python2.5
#
# Lookup table definitions.

import scipy.signal
import numpy
import pylab

lookup_tables = []
lookup_tables_i8 = []
lookup_tables_i16 = []
lookup_tables_i32 = []

"""----------------------------------------------------------------------------
Sine table
----------------------------------------------------------------------------"""

WAVETABLE_SIZE = 512
t = numpy.arange(0.0, WAVETABLE_SIZE + WAVETABLE_SIZE / 4 + 1) / WAVETABLE_SIZE
x = numpy.sin(2 * numpy.pi * t)
lookup_tables += [('sine', x)]

def minimum_phase_reconstruction(signal):
  fft_size = len(signal)
  Xf = numpy.fft.fft(signal, fft_size)
  real_cepstrum = numpy.fft.ifft(numpy.log(1e-50 + numpy.abs(Xf))).real
  real_cepstrum[1:fft_size / 2] *= 2
  real_cepstrum[fft_size / 2 + 1:] = 0
  min_phi = numpy.fft.ifft(numpy.exp(numpy.fft.fft(real_cepstrum))).real
  return min_phi

n = len(excitation_pulse)
target_length = 20
ratio = 32

pulse_upsampled = numpy.fft.irfft(numpy.fft.rfft(excitation_pulse), ratio * n)
pulse_upsampled = minimum_phase_reconstruction(
  pulse_upsampled * numpy.kaiser(n * ratio, 1))[:ratio * target_length]
pulse_upsampled -= pulse_upsampled[0]
pulse_upsampled[-ratio * 4:] *= numpy.linspace(1, 0, ratio * 4)
pulse_upsampled /= pulse_upsampled.max()
