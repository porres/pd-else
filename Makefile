############
# Makefile #
############

# library name

lib.name = ELSE

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
brown~.class.sources := classes/brown~.c
cents2ratio~.class.sources := classes/cents2ratio~.c
changed~.class.sources := classes/changed~.c
crackle~.class.sources := classes/crackle~.c
downsample~.class.sources := classes/downsample~.c
elapsed~.class.sources := classes/elapsed~.c
gbman~.class.sources := classes/gbman~.c
henon~.class.sources := classes/henon~.c
hz2rad~.class.sources := classes/hz2rad~.c
imp~.class.sources := classes/imp~.c
imp2~.class.sources := classes/imp2~.c
lastvalue~.class.sources := classes/lastvalue~.c
latoocarfian~.class.sources := classes/latoocarfian~.c
lfnoise~.class.sources := classes/lfnoise~.c
lincong~.class.sources := classes/lincong~.c
logistic~.class.sources := classes/logistic~.c
median~.class.sources := classes/median~.c
pan2~.class.sources := classes/pan2~.c
pan4~.class.sources := classes/pan4~.c
parabolic~.class.sources := classes/parabolic~.c
peak~.class.sources := classes/peak~.c
pimp~.class.sources := classes/pimp~.c
pulse~.class.sources := classes/pulse~.c
pulsecount~.class.sources := classes/pulsecount~.c
pulsediv~.class.sources := classes/pulsediv~.c
quad~.class.sources := classes/quad~.c
rad2hz~.class.sources := classes/rad2hz~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
rescale~.class.sources := classes/rescale~.c
rms~.class.sources := classes/rms~.c
sawtooth~.class.sources := classes/sawtooth~.c
sh~.class.sources := classes/sh~.c
select~.class.sources := classes/select~.c
sin~.class.sources := classes/sin~.c
square~.class.sources := classes/square~.c
standard~.class.sources := classes/standard~.c
toggleff~.class.sources := classes/toggleff~.c
togedge~.class.sources := classes/togedge~.c
vtriangle~.class.sources := classes/vtriangle~.c
vu~.class.sources := classes/vu~.c
xfade~.class.sources := classes/xfade~.c
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
