#!/bin/bash
set -e  # stop on errors

# Variables
FFMPEG_DIR="ffmpeg-7.0.1"
OS=$(uname)

echo "Detected OS: $OS"

# Define platform-specific configurations
if [[ "$OS" == "Darwin" ]]; then
    export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
    ffmpeg_config="--enable-securetransport --extra-cflags=-mmacosx-version-min=15.0 --extra-ldflags=-mmacosx-version-min=15.0"
    ffmpeg_cc="clang -arch arm64"
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

echo "Changing directory to $FFMPEG_DIR"
cd "$FFMPEG_DIR"

echo "Cleaning previous build"
make clean || true
make distclean || true

echo "Running configure..."
export MACOSX_DEPLOYMENT_TARGET=15.0
./configure --disable-asm --disable-libxcb --disable-bzlib --disable-lzma --disable-sdl2 --disable-libdrm --disable-vaapi --enable-static --disable-shared --enable-optimizations --disable-debug \
            --disable-programs --disable-iconv --disable-avdevice --disable-postproc \
            --disable-everything --enable-avcodec --enable-avformat --enable-avutil \
            --enable-encoder=vorbis,flac,mp3,mpeg4,aac,alac,pcm_mulaw,pcm_alaw \
            --enable-encoder=libvorbis \
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
            --enable-libvorbis \
            $ffmpeg_config > ../configure_output.log 2>&1 || { echo "Configure failed. See configure_output.log for details."; exit 1; }

echo "Starting compilation..."
if [[ "${CC:-}" == *"aarch64"* ]]; then
    make V=1 CC="$ffmpeg_cc" AR="aarch64-linux-gnu-ar" RANLIB="aarch64-linux-gnu-ranlib" LD="aarch64-linux-gnu-ld" || { echo "Make failed"; exit 1; }
else
    make CC="$2 $ffmpeg_cc" || { echo "Make failed"; exit 1; }
fi

echo "Build finished successfully."
