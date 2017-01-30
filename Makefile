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
cents2ratio~.class.sources := classes/cents2ratio~.c
changed~.class.sources := classes/changed~.c
downsample~.class.sources := classes/downsample~.c
hz2rad~.class.sources := classes/hz2rad~.c
pimp~.class.sources := classes/pimp~.c
imp~.class.sources := classes/imp~.c
median~.class.sources := classes/median~.c
par~.class.sources := classes/par~.c
rad2hz~.class.sources := classes/rad2hz~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
rescale~.class.sources := classes/rescale~.c
sh~.class.sources := classes/sh~.c
sin~.class.sources := classes/sin~.c
square~.class.sources := classes/square~.c
toggleff~.class.sources := classes/toggleff~.c
zerocross~.class.sources := classes/zerocross~.c

#########################################################################

# extra files

datafiles = ELSE-meta.pd README.md

#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
