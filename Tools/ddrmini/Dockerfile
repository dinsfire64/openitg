FROM debian:bullseye-20260824-slim

ENV DEBIAN_FRONTEND=noninteractive

# Bullseye's final packages are archived.  Building against its older glibc
# keeps the resulting AArch64 binary compatible with the DDR Mini's glibc 2.35.
RUN dpkg --add-architecture arm64 \
 && printf '%s\n' \
      'deb http://archive.debian.org/debian bullseye main' \
      'deb http://archive.debian.org/debian bullseye-updates main' \
      'deb http://archive.debian.org/debian-security bullseye-security main' \
      'deb-src http://archive.debian.org/debian bullseye main' \
      'deb-src http://archive.debian.org/debian-security bullseye-security main' \
      > /etc/apt/sources.list \
 && printf '%s\n' \
      'Acquire::Check-Valid-Until "false";' \
      'Acquire::Check-Date "false";' \
      > /etc/apt/apt.conf.d/99debian-archive \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      autoconf \
      automake \
      binutils-aarch64-linux-gnu \
      build-essential \
      ca-certificates \
      file \
      g++-aarch64-linux-gnu \
      gcc-aarch64-linux-gnu \
      gettext \
      git \
      libc6-dev-arm64-cross \
      libasound2-dev:arm64 \
      libjpeg-dev:arm64 \
      libegl-dev:arm64 \
      libgles-dev:arm64 \
      liblua5.1-0-dev:arm64 \
      libpng-dev:arm64 \
      libresample1-dev:arm64 \
      libsdl2-dev:arm64 \
      libusb-dev:arm64 \
      libvorbis-dev:arm64 \
      make \
      pkg-config \
      zlib1g-dev:arm64 \
 && cd /tmp \
 && apt-get source ffmpeg \
 && cd ffmpeg-* \
 && ./configure \
      --prefix=/opt/ddrmini-ffmpeg \
      --arch=aarch64 \
      --target-os=linux \
      --cross-prefix=aarch64-linux-gnu- \
      --enable-cross-compile \
      --disable-shared \
      --enable-static \
      --disable-programs \
      --disable-doc \
      --disable-debug \
      --disable-network \
      --disable-autodetect \
      --disable-everything \
      --enable-avcodec \
      --enable-avformat \
      --enable-avutil \
      --enable-swscale \
      --enable-decoder=mpeg1video,mpeg2video,mjpeg,mpeg4 \
      --enable-demuxer=mpegps,mpegvideo,mjpeg,avi \
      --enable-parser=mpegvideo,mjpeg,mpeg4video \
 && make -j2 \
 && make install \
 && aarch64-linux-gnu-nm -g --defined-only /opt/ddrmini-ffmpeg/lib/libavcodec.a \
      | grep -q ' ff_mpeg4_decoder$' \
 && cd / \
 && rm -rf /tmp/ffmpeg-* /var/lib/apt/lists/*

COPY build-openitg.sh /usr/local/bin/build-openitg-ddrmini

ENTRYPOINT ["/usr/local/bin/build-openitg-ddrmini"]
