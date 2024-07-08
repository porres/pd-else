FFMPEG_VERSION := 7.0.1
FFMPEG_DIR := ffmpeg-$(FFMPEG_VERSION)
FFMPEG_TAR := $(FFMPEG_DIR).tar.bz2
FFMPEG_URL := https://ffmpeg.org/releases/$(FFMPEG_TAR)
FFMPEG_LIBS := libavcodec.a libavformat.a libavutil.a libswresample.a

LIBSAMPLERATE=../../../shared/libsamplerate

cflags = -I$(LIBSAMPLERATE) -I$(FFMPEG_DIR) -DHAVE_STRUCT_TIMESPEC

lib.name = ffplay~

define forDarwin
    cflags += -mmacosx-version-min=10.9
endef

LIBSAMPLERATE=../../../shared/libsamplerate

ffplay~.class.sources := ffplay~.c $(LIBSAMPLERATE)/samplerate.c $(LIBSAMPLERATE)/src_linear.c $(LIBSAMPLERATE)/src_sinc.c $(LIBSAMPLERATE)/src_zoh.c

ldlibs = -L$(FFMPEG_DIR)/libavutil -L$(FFMPEG_DIR)/libavcodec -L$(FFMPEG_DIR)/libavformat -L$(FFMPEG_DIR)/libswresample -lavutil -lavcodec -lavformat -lswresample

# Change the arch to arm64 if the extension is d_arm64

#ffmpeg_cflags = -I./libs/include
#ffmpeg_ldflags = -L./libs/lib

ifeq ($(extension),d_arm64)
  override arch := arm64
endif

define forLinux
# ASM causes static linker issues on Linux
ffmpeg_config = --enable-pic --disable-asm
endef

define forDarwin
  ffmpeg_config = --extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9
endef

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))

.PHONY: all ffmpeg

all: ffmpeg

ffmpeg: $(FFMPEG_DIR)

$(FFMPEG_DIR): $(FFMPEG_TAR)
	tar xjf $(FFMPEG_TAR)
#	cd libs && ./compile.sh
	cd $(FFMPEG_DIR) && ./configure --enable-static --disable-shared --disable-debug --disable-doc --disable-programs --disable-avdevice --disable-postproc --disable-network --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample --enable-decoder=mp3*,pcm*,flac,aac,vorbis,opus --enable-parser=mpegaudio,aac --enable-demuxer=mp3,wav,aiff,flac,aac,ogg --enable-filter=aresample --enable-protocol=file $(ffmpeg_config)
	cd $(FFMPEG_DIR) && make
	rm -f $(FFMPEG_TAR)

$(FFMPEG_TAR):
	curl -O $(FFMPEG_URL)
