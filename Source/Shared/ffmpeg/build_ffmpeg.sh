#!/bin/bash

# Variables
FFMPEG_DIR="ffmpeg-7.0.1"
OS=$(uname)

# Define platform-specific configurations
if [[ "$OS" == "Darwin" ]]; then
    ffmpeg_config="--enable-securetransport --extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9"
    ffmpeg_cc="clang -arch x86_64 -arch arm64"
elif [[ "$OS" == "Linux" ]]; then
    if [[ "${CC:-}" == *"aarch64"* ]]; then
        ffmpeg_config="--enable-cross-compile --arch=arm64 --target-os=linux --cross-prefix=aarch64-linux-gnu- --pkg-config=pkg-config"
        ffmpeg_cc="aarch64-linux-gnu-gcc"
        export PKG_CONFIG_PATH="/usr/lib/aarch64-linux-gnu/pkgconfig"
    else
        ffmpeg_config="--enable-openssl --enable-pic"
        ffmpeg_cc="${CC:-gcc}"
    fi
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
    ffmpeg_cc="${CC:-gcc}"
else
    echo "Unsupported OS: $OS"
    exit 1
fi

# Clean any previous build
cd "$FFMPEG_DIR"
make clean || true
make distclean || true

# Configure and compile FFmpeg
./configure --disable-asm --disable-libxcb --disable-bzlib --disable-lzma --disable-sdl2 --disable-libdrm --disable-vaapi --enable-static --disable-shared --enable-optimizations --disable-debug \
            --disable-programs --disable-iconv --disable-avdevice --disable-postproc \
            --disable-everything --enable-avcodec --enable-avformat --enable-avutil \
            --enable-encoder=vorbis,flac,mp3,mpeg4,aac,alac,pcm_mulaw,pcm_alaw \
            --enable-decoder=mp3*,pcm*,aac*,flac,vorbis,opus,alac,mulaw,alaw,aac,h264 \
            --enable-decoder=mpeg4,mpeg1video,mpeg2video \
            --enable-decoder=mjpeg  \
            --enable-muxer=ogg,flac,mp3,mp4,avi,mov,flv,asf,caf,au \
            --enable-demuxer=mp3,wav,aiff,flac,aac,ogg,pcm*,caf,au \
            --enable-demuxer=avi,mov,mp3,flv,asf \
            --enable-filter=aresample --enable-protocol=file \
            --enable-parser=mpegaudio,aac,mpeg4video \
            --enable-network --enable-protocol=http,https \
            --enable-swresample \
#            --enable-swscale \
#            --enable-libvorbis --enable-libopus
            $ffmpeg_config


if [[ "${CC:-}" == *"aarch64"* ]]; then
    make V=1 CC="$ffmpeg_cc" AR="aarch64-linux-gnu-ar" RANLIB="aarch64-linux-gnu-ranlib" LD="aarch64-linux-gnu-ld"
else
    make CC="$2 $ffmpeg_cc"
fi
