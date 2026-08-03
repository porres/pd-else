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

n = len(excitation_pulse)
target_length = 20
ratio = 32
