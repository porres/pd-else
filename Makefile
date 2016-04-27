
#########################################################################

# Makefile

#########################################################################

# library name

lib.name = Porres-ELS


#########################################################################

# sources

# control:

cents2ratio.class.sources := classes/cents2ratio.c
ratio2cents.class.sources := classes/ratio2cents.c
hz2rad.class.sources := classes/hz2rad.c
rad2hz.class.sources := classes/rad2hz.c

# rescale.class.sources := classes/rescale.c

# signal:

cents2ratio~.class.sources := classes/cents2ratio~.c
hz2rad~.class.sources := classes/hz2rad~.c
imp~.class.sources := classes/imp~.c
rad2hz~.class.sources := classes/rad2hz~.c
ratio2cents~.class.sources := classes/ratio2cents~.c
sh~.class.sources := classes/sh~.c

# median~.class.sources := classes/median_tilde.c


#########################################################################

# extra files

datafiles = Porres-ELS-meta.pd README.md


#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
