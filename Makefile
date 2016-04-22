
#########################################################################

# Makefile

#########################################################################

# library name

lib.name = Porres-ELS


#########################################################################

# sources

cents2ratio.class.sources := classes/cents2ratio.c
ratio2cents.class.sources := classes/ratio2cents.c


#########################################################################

# extra files

datafiles = Porres-ELS-meta.pd README.md


#########################################################################

# include Makefile.pdlibbuilder

include pd-lib-builder/Makefile.pdlibbuilder
