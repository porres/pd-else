
lib.name = plaits~

common.sources =  ./udp/udp_discovery_peer.cpp \
                  ./udp/udp_discovery_protocol.cpp \
                  ./link.cpp

# input source file (class name == source file basename)
class.sources = pd.link.c

cflags = -I$(shell pwd)
CXX += --std=c++17

# all extra files to be included in binary distribution of the library
define forDarwin
  cflags += -I/usr/include/c++/v1 -mmacosx-version-min=10.9
  CXX += -stdlib=libc++
endef

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder
PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
