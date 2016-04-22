
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

# signal:
cents2ratio~.class.sources := classes/cents2ratio_tilde.c
ratio2cents~.class.sources := classes/ratio2cents_tilde.c
sh~.class.sources := classes/sh_tilde.c


#########################################################################

# extra files

datafiles = Porres-ELS-meta.pd README.md


#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
