
lib.name = pd.link

opus_flags = -DUSE_ALLOCA=1 -DOPUS_BUILD=1 -DFLOAT_APPROX=1 -Iopus/src -Iopus/include -Iopus/celt -Iopus/silk -Iopus/silk/float
opus_sources =    ./opus/src/analysis.c \
                  ./opus/src/extensions.c \
                  ./opus/src/mapping_matrix.c \
                  ./opus/src/mlp.c \
                  ./opus/src/mlp_data.c \
                  ./opus/src/opus.c \
                  ./opus/src/opus_compare.c \
                  ./opus/src/opus_decoder.c \
                  ./opus/src/opus_encoder.c \
                  ./opus/src/opus_multistream.c \
                  ./opus/src/opus_multistream_decoder.c \
                  ./opus/src/opus_multistream_encoder.c \
                  ./opus/src/opus_projection_decoder.c \
                  ./opus/src/opus_projection_encoder.c \
                  ./opus/src/repacketizer.c \
                  ./opus/celt/bands.c \
                  ./opus/celt/celt.c \
                  ./opus/celt/celt_decoder.c \
                  ./opus/celt/celt_encoder.c \
                  ./opus/celt/celt_lpc.c \
                  ./opus/celt/cwrs.c \
                  ./opus/celt/entcode.c \
                  ./opus/celt/entdec.c \
                  ./opus/celt/entenc.c \
                  ./opus/celt/kiss_fft.c \
                  ./opus/celt/laplace.c \
                  ./opus/celt/mathops.c \
                  ./opus/celt/mdct.c \
                  ./opus/celt/modes.c \
                  ./opus/celt/pitch.c \
                  ./opus/celt/quant_bands.c \
                  ./opus/celt/rate.c \
                  ./opus/celt/vq.c \
                  $(wildcard ./opus/silk/*.c) \
                  $(wildcard ./opus/silk/float/*.c)

common.sources =  ./udp/udp_discovery_peer.cpp \
                  ./udp/udp_discovery_protocol.cpp \
                  ./link.cpp


# input source file (class name == source file basename)
class.sources = pd.link.c pd.link~.c

pd.link~.class.sources += $(opus_sources) ./../../../shared/magic.c

cflags = $(opus_flags) -I$(shell pwd) -I../../../shared
CXX += --std=c++17

# all extra files to be included in binary distribution of the library
define forDarwin
  cflags += -I/usr/include/c++/v1 -mmacosx-version-min=10.9
  CXX += -stdlib=libc++
endef

define forWindows
ldlibs += -lws2_32 -liphlpapi -lstdc++
ldflags += -static-libgcc -static-libstdc++ -static
endef

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder
PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
