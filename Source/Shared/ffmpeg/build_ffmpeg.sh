#!/bin/bash

# Variables
FFMPEG_DIR="ffmpeg-7.0.1"
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

# Configure and compile FFmpeg
cd "$FFMPEG_DIR"
./configure --disable-asm --disable-libxcb --disable-bzlib --disable-lzma --disable-sdl2 --disable-libdrm --disable-vaapi --enable-static --disable-shared --enable-optimizations --disable-debug --disable-doc \
            --disable-programs --disable-iconv --disable-avdevice --disable-postproc --disable-network \
            --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swscale \
            --enable-swresample --enable-decoder=mp3*,pcm*,aac*,flac,vorbis,opus --enable-parser=mpegaudio,aac \
            --enable-demuxer=mp3,wav,aiff,flac,aac,ogg,pcm* --enable-filter=aresample --enable-protocol=file \
            --enable-demuxer=avi --enable-demuxer=mov --enable-demuxer=mp3 \
            --enable-demuxer=flv --enable-demuxer=asf --enable-muxer=avi --enable-muxer=mov --enable-muxer=mp4 \
            --enable-muxer=flv --enable-muxer=asf --enable-decoder=mp3 --enable-decoder=aac --enable-decoder=h264 \
            --enable-decoder=mpeg4 --enable-decoder=mpeg1video --enable-decoder=mpeg2video --enable-decoder=mjpeg --enable-encoder=aac --enable-encoder=mpeg4 \
            --enable-encoder=mpeg1video --enable-parser=mpeg4video --enable-network --enable-protocol=http --enable-protocol=https --enable-protocol=rtmp --enable-protocol=rtmpt --enable-protocol=rtmps --enable-protocol=hls --enable-protocol=tcp --enable-protocol=udp \
            $ffmpeg_config

make CC="$2 $ffmpeg_cc"
