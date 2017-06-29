############
# Makefile #
############

# library name

lib.name = else

#########################################################################
# sources
#########################################################################

# control:
cents2ratio.class.sources := classes/cents2ratio.c
hz2rad.class.sources := classes/hz2rad.c
rad2hz.class.sources := classes/rad2hz.c
ratio2cents.class.sources := classes/ratio2cents.c
rescale.class.sources := classes/rescale.c

# signal:
accum~.class.sources := classes/accum~.c
balance~.class.sources := classes/balance~.c
bandpass~.class.sources := classes/bandpass~.c
brown~.class.sources := classes/brown~.c
cents2ratio~.class.sources := classes/cents2ratio~.c
changed~.class.sources := classes/changed~.c
changed2~.class.sources := classes/changed2~.c
crackle~.class.sources := classes/crackle~.c
cosine~.class.sources := classes/cosine~.c
cusp~.class.sources := classes/cusp~.c
decay~.class.sources := classes/decay~.c
decay2~.class.sources := classes/decay2~.c
downsample~.class.sources := classes/downsample~.c
elapsed~.class.sources := classes/elapsed~.c
fold.class.sources := classes/fold.c
fold~.class.sources := classes/fold~.c
gbman~.class.sources := classes/gbman~.c
henon~.class.sources := classes/henon~.c
highpass~.class.sources := classes/highpass~.c
highshelf~.class.sources := classes/highshelf~.c
hz2rad~.class.sources := classes/hz2rad~.c
imp~.class.sources := classes/imp~.c
imp2~.class.sources := classes/imp2~.c
lastvalue~.class.sources := classes/lastvalue~.c
latoocarfian~.class.sources := classes/latoocarfian~.c
lfnoise~.class.sources := classes/lfnoise~.c
lincong~.class.sources := classes/lincong~.c
logistic~.class.sources := classes/logistic~.c
loop.class.sources := classes/loop.c
lowpass~.class.sources := classes/lowpass~.c
lowshelf~.class.sources := classes/lowshelf~.c
median~.class.sources := classes/median~.c
notch~.class.sources := classes/notch~.c
pan2~.class.sources := classes/pan2~.c
pan4~.class.sources := classes/pan4~.c
parabolic~.class.sources := classes/parabolic~.c
peak~.class.sources := classes/peak~.c
peakfilter~.class.sources := classes/peakfilter~.c
phaseshifter~.class.sources := classes/phaseshifter~.c
pimp~.class.sources := classes/pimp~.c
pulse~.class.sources := classes/pulse~.c
pulsecount~.class.sources := classes/pulsecount~.c
pulsediv~.class.sources := classes/pulsediv~.c
quad~.class.sources := classes/quad~.c
rad2hz~.class.sources := classes/rad2hz~.c
random~.class.sources := classes/random~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
rescale~.class.sources := classes/rescale~.c
resonant~.class.sources := classes/resonant~.c
resonant2~.class.sources := classes/resonant2~.c
rms~.class.sources := classes/rms~.c
rotate~.class.sources := classes/rotate~.c
sawtooth~.class.sources := classes/sawtooth~.c
sawtooth2~.class.sources := classes/sawtooth2~.c
sh~.class.sources := classes/sh~.c
select~.class.sources := classes/select~.c
selectx~.class.sources := classes/selectx~.c
sin~.class.sources := classes/sin~.c
sine~.class.sources := classes/sine~.c
square~.class.sources := classes/square~.c
standard~.class.sources := classes/standard~.c
toggleff~.class.sources := classes/toggleff~.c
togedge~.class.sources := classes/togedge~.c
vtriangle~.class.sources := classes/vtriangle~.c
vu~.class.sources := classes/vu~.c
xfade~.class.sources := classes/xfade~.c
wrap2.class.sources := classes/wrap2.c
wrap2~.class.sources := classes/wrap2~.c
zerocross~.class.sources := classes/zerocross~.c

#########################################################################

# extra files

datafiles = \
$(wildcard help/*.pd) \
$(wildcard help/Abstractions/*.pd) \
ELSE-meta.pd \
README.md \

#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
