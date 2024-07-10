FFMPEG_DIR := ffmpeg-7.0.1
FFMPEG_TAR := $(FFMPEG_DIR).tar.bz2
FFMPEG_LIBS := libavcodec.a libavformat.a libavutil.a libswresample.a

LIBSAMPLERATE=../../../shared/libsamplerate

cflags = -I$(LIBSAMPLERATE) -I$(FFMPEG_DIR) -DHAVE_STRUCT_TIMESPEC

lib.name = play.file~

define forDarwin
    cflags += -mmacosx-version-min=10.9
endef

LIBSAMPLERATE=../../../shared/libsamplerate

play.file~.class.sources := play.file~.c
sfload.class.sources := sfload.c

ldlibs = -L$(FFMPEG_DIR)/libavutil -L$(FFMPEG_DIR)/libavcodec -L$(FFMPEG_DIR)/libavformat -L$(FFMPEG_DIR)/libswresample -lavformat -lavcodec -lswresample -lavutil -lz

# Change the arch to arm64 if the extension is d_arm64

ifeq ($(extension),d_arm64)
  override arch := arm64
endif

define forLinux
# ASM causes static linker issues on Linux
ffmpeg_config = --enable-pic --disable-asm
endef

define forWindows
ffmpeg_config = --disable-x86asm
ldlibs += -lbcrypt
endef

define forDarwin
  ffmpeg_config = --extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9
endef

# Makefile based on pd-lib-builder by Katja Vetter, see: https://github.com/pure-data/pd-lib-builder
PDLIBBUILDER_DIR=../../../../pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

.PHONY: clean

all_obj: ffmpeg play.file~ sfload

ffmpeg: $(FFMPEG_DIR)

$(FFMPEG_DIR):
	tar xjf $(FFMPEG_TAR)
	cd $(FFMPEG_DIR) && ./configure --enable-static --disable-shared --enable-optimizations --disable-debug --disable-doc --disable-programs --disable-iconv --disable-avdevice --disable-postproc --disable-network --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample --enable-decoder=mp3*,pcm*,aac*,flac,vorbis,opus --enable-parser=mpegaudio,aac --enable-demuxer=mp3,wav,aiff,flac,aac,ogg,pcm* --enable-filter=aresample --enable-protocol=file $(ffmpeg_config)
	cd $(FFMPEG_DIR) && make

clean:
	rm -rf $(FFMPEG_DIR)
	rm -f *.d_*
	rm -f *.pd_*
