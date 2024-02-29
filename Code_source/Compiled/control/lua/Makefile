# -*- mode: makefile-gmake -*-

# This needs GNU make.

# You may want to set these if you have the Pd include files in a
# non-standard location, and/or want to install the external in a
# custom directory.

#PDINCLUDEDIR = /usr/include/pd
#PDLIBDIR = /usr/lib/pd/extra

# No need to edit anything below this line, usually.

lib.name = pdlua

pdlua_version := $(shell git describe --tags 2>/dev/null)

luasrc = $(wildcard lua/onelua.c)

PKG_CONFIG ?= pkg-config

ifeq ($(luasrc),)
# compile with installed liblua
$(info ++++ NOTE: using installed lua)
luaflags = $(shell $(PKG_CONFIG) --cflags lua)
lualibs = $(shell $(PKG_CONFIG) --libs lua)
else
# compile with Lua submodule
$(info ++++ NOTE: using lua submodule)
luaflags = -DMAKE_LIB -Ilua
define forDarwin
luaflags += -DLUA_USE_MACOSX
endef
define forLinux
luaflags += -DLUA_USE_LINUX
endef
define forWindows
luaflags += -DLUA_USE_WINDOWS
endef
endif

cflags = ${luaflags} -DPDLUA_VERSION="$(pdlua_version)"

pdlua.class.sources := pdlua.c $(luasrc)
pdlua.class.ldlibs := $(lualibs)

datafiles = pd.lua $(wildcard pdlua*-help.pd)

PDLIBBUILDER_DIR=.
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

install: installplus

installplus:
	$(INSTALL_DIR) -v "$(installpath)"
	cp -r ./pdlua/ "${installpath}"/pdlua
