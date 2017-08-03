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
apass~.class.sources := classes/apass~.c
balance~.class.sources := classes/balance~.c
bandpass~.class.sources := classes/bandpass~.c
bcoeff.class.sources := classes/bcoeff.c
brown~.class.sources := classes/brown~.c
ceil.class.sources := classes/ceil.c
ceil~.class.sources := classes/ceil~.c
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
fbsine~.class.sources := classes/fbsine~.c
fbcomb~.class.sources := classes/fbcomb~.c
fbdelay~.class.sources := classes/fbdelay~.c
floor.class.sources := classes/floor.c
floor~.class.sources := classes/floor~.c
fold.class.sources := classes/fold.c
fold~.class.sources := classes/fold~.c
gbman~.class.sources := classes/gbman~.c
glide~.class.sources := classes/glide~.c
henon~.class.sources := classes/henon~.c
highpass~.class.sources := classes/highpass~.c
highshelf~.class.sources := classes/highshelf~.c
hz2rad~.class.sources := classes/hz2rad~.c
ikeda~.class.sources := classes/ikeda~.c
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
pluck~.class.sources := classes/pluck~.c
pimp~.class.sources := classes/pimp~.c
pmosc~.class.sources := classes/pmosc~.c
pulse~.class.sources := classes/pulse~.c
pulsecount~.class.sources := classes/pulsecount~.c
pulsediv~.class.sources := classes/pulsediv~.c
quad~.class.sources := classes/quad~.c
rad2hz~.class.sources := classes/rad2hz~.c
ramp~.class.sources := classes/ramp~.c
rampnoise~.class.sources := classes/rampnoise~.c
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
stepnoise~.class.sources := classes/stepnoise~.c
sin~.class.sources := classes/sin~.c
sine~.class.sources := classes/sine~.c
square~.class.sources := classes/square~.c
standard~.class.sources := classes/standard~.c
susloop~.class.sources := classes/susloop~.c
tgate~.class.sources := classes/tgate~.c
toggleff~.class.sources := classes/toggleff~.c
togedge~.class.sources := classes/togedge~.c
triwave~.class.sources := classes/triwave~.c
vsaw~.class.sources := classes/vsaw~.c
vu~.class.sources := classes/vu~.c
xfade~.class.sources := classes/xfade~.c
xgate~.class.sources := classes/xgate~.c
xgate2~.class.sources := classes/xgate2~.c
xmod~.class.sources := classes/xmod~.c
xselect~.class.sources := classes/xselect~.c
xselect2~.class.sources := classes/xselect2~.c
wrap2.class.sources := classes/wrap2.c
wrap2~.class.sources := classes/wrap2~.c
zerocross~.class.sources := classes/zerocross~.c

#########################################################################

# extra files

datafiles = \
$(wildcard help/*.pd) \
$(wildcard help/Abstractions/*.pd) \
*.pd \
README.md \
help/vacuous.wav

#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
