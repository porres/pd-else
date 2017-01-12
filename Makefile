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
cents2ratio~.class.sources := classes/cents2ratio~.c
hz2rad~.class.sources := classes/hz2rad~.c
impulse~.class.sources := classes/impulse~.c
median~.class.sources := classes/median~.c
rad2hz~.class.sources := classes/rad2hz~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
rescale~.class.sources := classes/rescale~.c
sh~.class.sources := classes/sh~.c

#########################################################################

# extra files

datafiles = ELSE-meta.pd README.md

#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
