# ================================================= #
# Makefile for Pure Data objects.
# Needs Makefile.pdlibbuilder as helper makefile
# for platform-dependent build settings and rules.


# ================================================= #
# library name
# include your library name here

lib.name = helloworld


# ================================================= #
# input source file (class name == source file basename)

class.sources = helloworld.c


# ================================================= #
# all extra files to be included in binary distribution of the library

datafiles = helloworld-help.pd helloworld-meta.pd README.md


# ================================================= #

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
include pd-lib-builder/Makefile.pdlibbuilder
