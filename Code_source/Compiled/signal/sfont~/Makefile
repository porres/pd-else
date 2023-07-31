
# Homebrew support (pdlib-builder assumes Fink)
ifneq ($(HOMEBREW_PREFIX),)
EXTRA_INCLUDES = -I$(HOMEBREW_PREFIX)/include
EXTRA_LDFLAGS = -L$(HOMEBREW_PREFIX)/lib
endif

cflags = $(EXTRA_INCLUDES) -Ishared -DHAVE_STRUCT_TIMESPEC

lib.name = sfont~

file := ../../../shared/elsefile.c 
sfont~.class.sources := sfont~.c $(file)

ldlibs = $(EXTRA_LDFLAGS) -lfluidsynth

#datafiles = sfont~-help.pd
datadirs = sf

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

localdep_linux: install
	scripts/localdeps.linux.sh "${installpath}/sfont~.${extension}"

localdep_windows: install
	scripts/localdeps.win.sh "${installpath}/sfont~.${extension}"

localdep_macos: install
	scripts/localdeps.macos.sh "${installpath}/sfont~.${extension}"
