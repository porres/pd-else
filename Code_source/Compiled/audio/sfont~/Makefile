FLUID_SOURCES = \
    FluidLite/src/fluid_init.c \
    FluidLite/src/fluid_chan.c \
    FluidLite/src/fluid_chorus.c \
    FluidLite/src/fluid_conv.c \
    FluidLite/src/fluid_defsfont.c \
    FluidLite/src/fluid_dsp_float.c \
    FluidLite/src/fluid_gen.c \
    FluidLite/src/fluid_hash.c \
    FluidLite/src/fluid_list.c \
    FluidLite/src/fluid_mod.c \
    FluidLite/src/fluid_ramsfont.c \
    FluidLite/src/fluid_rev.c \
    FluidLite/src/fluid_settings.c \
    FluidLite/src/fluid_synth.c \
    FluidLite/src/fluid_sys.c \
    FluidLite/src/fluid_tuning.c \
    FluidLite/src/fluid_voice.c

VORBIS_SOURCES = \
    FluidLite/libvorbis-1.3.5/lib/vorbisenc.c \
    FluidLite/libvorbis-1.3.5/lib/info.c \
    FluidLite/libvorbis-1.3.5/lib/analysis.c \
    FluidLite/libvorbis-1.3.5/lib/bitrate.c \
    FluidLite/libvorbis-1.3.5/lib/block.c \
    FluidLite/libvorbis-1.3.5/lib/codebook.c \
    FluidLite/libvorbis-1.3.5/lib/envelope.c \
    FluidLite/libvorbis-1.3.5/lib/floor0.c \
    FluidLite/libvorbis-1.3.5/lib/floor1.c \
    FluidLite/libvorbis-1.3.5/lib/lookup.c \
    FluidLite/libvorbis-1.3.5/lib/lpc.c \
    FluidLite/libvorbis-1.3.5/lib/lsp.c \
    FluidLite/libvorbis-1.3.5/lib/mapping0.c \
    FluidLite/libvorbis-1.3.5/lib/mdct.c \
    FluidLite/libvorbis-1.3.5/lib/psy.c \
    FluidLite/libvorbis-1.3.5/lib/registry.c \
    FluidLite/libvorbis-1.3.5/lib/res0.c \
    FluidLite/libvorbis-1.3.5/lib/sharedbook.c \
    FluidLite/libvorbis-1.3.5/lib/smallft.c \
    FluidLite/libvorbis-1.3.5/lib/vorbisfile.c \
    FluidLite/libvorbis-1.3.5/lib/window.c \
    FluidLite/libvorbis-1.3.5/lib/synthesis.c

OGG_SOURCES = \
    FluidLite/libogg-1.3.2/src/bitwise.c \
    FluidLite/libogg-1.3.2/src/framing.c

cflags = $(EXTRA_INCLUDES) -I../../ -IFluidLite/libvorbis-1.3.5/include -IFluidLite/libvorbis-1.3.5/lib -IFluidLite/libogg-1.3.2/include -IFluidLite/include -DHAVE_STRUCT_TIMESPEC

lib.name = sfont~

file := ../../../shared/elsefile.c
sfont~.class.sources := sfont~.c $(file) $(FLUID_SOURCES) $(VORBIS_SOURCES) $(OGG_SOURCES)

ldlibs = $(EXTRA_LDFLAGS)

#datafiles = sfont~-help.pd
datadirs = sf

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))
