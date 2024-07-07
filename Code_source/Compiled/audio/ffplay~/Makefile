FFMPEG_VERSION := 7.0.1
FFMPEG_DIR := ffmpeg-$(FFMPEG_VERSION)
FFMPEG_TAR := $(FFMPEG_DIR).tar.bz2
FFMPEG_URL := https://ffmpeg.org/releases/$(FFMPEG_TAR)
FFMPEG_LIBS := libavcodec.a libavformat.a libavutil.a libswscale.a libswresample.a

# Homebrew support (pdlib-builder assumes Fink)
ifneq ($(HOMEBREW_PREFIX),)
EXTRA_INCLUDES = -I$(HOMEBREW_PREFIX)/include
EXTRA_LDFLAGS = -L$(HOMEBREW_PREFIX)/lib
endif

cflags = $(EXTRA_INCLUDES) -Ishared -DHAVE_STRUCT_TIMESPEC

lib.name = ffplay~

LIBSAMPLERATE=../../../shared/libsamplerate

ffplay~.class.sources := ffplay~.c $(LIBSAMPLERATE)/samplerate.c $(LIBSAMPLERATE)/src_linear.c $(LIBSAMPLERATE)/src_sinc.c $(LIBSAMPLERATE)/src_zoh.c

ldlibs = $(EXTRA_LDFLAGS) -lavutil -lavcodec -lavformat -lswresample

# Change the arch to arm64 if the extension is d_arm64
ifeq ($(extension),d_arm64)
  override arch := arm64
endif

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

.PHONY: all ffmpeg

all: ffmpeg

ffmpeg: $(FFMPEG_DIR)

$(FFMPEG_DIR): $(FFMPEG_TAR)
	tar xjf $(FFMPEG_TAR)
	cd $(FFMPEG_DIR) && ./configure --enable-static --disable-shared --disable-debug --disable-doc --disable-programs --disable-avdevice --disable-postproc --disable-network --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample
	cd $(FFMPEG_DIR) && make
	rm -f $(FFMPEG_TAR)

$(FFMPEG_TAR):
	wget $(FFMPEG_URL)
