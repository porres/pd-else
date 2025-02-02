#!/bin/bash

# Variables
FFMPEG_DIR=${FFMPEG_DIR:-"ffmpeg-7.0.1"}
OS=$(uname)

# Define platform-specific configurations
if [[ "$OS" == "Darwin" ]]; then
    ffmpeg_config="--enable-securetransport --extra-cflags=-mmacosx-version-min=10.9 --extra-ldflags=-mmacosx-version-min=10.9"
    ffmpeg_cc="clang -arch x86_64 -arch arm64"
elif [[ "$OS" == "Linux" ]]; then
    ffmpeg_config="--enable-openssl --enable-pic"
    ffmpeg_cc="${CC:-gcc}"
elif [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
    ffmpeg_cc="${CC:-gcc}"
else
    echo "Unsupported OS: $OS"
    exit 1
fi

# Optional: Clean build directory
if [[ "$1" == "clean" ]]; then
    make clean
    exit 0
fi

# Validate compiler argument
if [[ -z "$2" ]]; then
    echo "Usage: $0 <compiler>"
    exit 1
fi

# Change to FFmpeg directory
cd "$FFMPEG_DIR" || { echo "Error: Directory $FFMPEG_DIR not found"; exit 1; }

# Configure and compile FFmpeg
./configure \
    --disable-asm \
    --disable-libxcb \
    --disable-bzlib \
    --disable-lzma \
    --disable-sdl2 \
    --disable-libdrm \
    --disable-vaapi \
    --enable-static \
    --disable-shared \
    --enable-optimizations \
    --disable-debug \
    --disable-doc \
    --disable-programs \
    --disable-iconv \
    --disable-avdevice \
    --disable-postproc \
    --disable-network \
    --disable-everything \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-decoder=mp3*,pcm*,aac*,flac,vorbis,opus,alac,mulaw,alaw \
    --enable-parser=mpegaudio,aac \
    --enable-demuxer=mp3,wav,aiff,flac,aac,ogg,pcm*,caf,au \
    --enable-muxer=avi,mov,mp4,flv,asf,caf,au \
    --enable-filter=aresample \
    --enable-protocol=file \
    --enable-decoder=h264,mpeg4,mpeg1video,mpeg2video,mjpeg \
    --enable-encoder=aac,mpeg4,mpeg1video,alac,mulaw,alaw \
    --enable-parser=mpeg4video \
    --enable-network \
    --enable-protocol=http,https,rtmp,rtmpt,rtmps,hls,tcp,udp \
    $ffmpeg_config

make CC="$2 $ffmpeg_cc"
