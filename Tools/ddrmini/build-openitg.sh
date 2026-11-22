#!/bin/sh
set -eu

source_dir=${1:-/src}
output_dir=${2:-/out}
jobs=${DDRMINI_BUILD_JOBS:-2}
work_dir=/tmp/openitg-ddrmini

test -f "$source_dir/configure.ac" || {
  echo "OpenITG source tree not found at $source_dir" >&2
  exit 1
}

mkdir -p "$work_dir" "$output_dir"
cp -a "$source_dir/." "$work_dir/"
cd "$work_dir"

# A copied host tree may contain stale native objects.  Remove generated build
# products without touching the bind-mounted source directory.
find . -name '*.o' -o -name '*.a' -o -name '*.Po' | xargs -r rm -f
rm -rf autom4te.cache src/.deps

export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
export AR=aarch64-linux-gnu-ar
export RANLIB=aarch64-linux-gnu-ranlib
export STRIP=aarch64-linux-gnu-strip
export PKG_CONFIG_LIBDIR=/opt/ddrmini-ffmpeg/lib/pkgconfig:/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig

./autogen.sh

# OpenITG's checked-in canonicalization scripts predate AArch64.  Automake
# preserves those existing files, so explicitly install the current copies
# supplied by the container before configuring the cross build.
config_aux_dir=/usr/share/misc
if test ! -f "$config_aux_dir/config.sub"; then
  config_aux_dir=/usr/share/automake-1.16
fi
cp "$config_aux_dir/config.sub" autoconf/config.sub
cp "$config_aux_dir/config.guess" autoconf/config.guess

./configure \
  --build="$(./autoconf/config.guess)" \
  --host=aarch64-linux-gnu \
  --enable-ddrmini \
  --disable-gtk2 \
  --without-mp3 \
  --without-sse2

make -j"$jobs"

# Retain the full-symbol binary beside the deployable artifact.  It is used
# with addr2line/gdb on the build host and does not need to be copied to USB.
cp src/openitg "$output_dir/openitg.debug"
cp src/openitg "$output_dir/openitg"
cp "$source_dir/Tools/ddrmini/runtime/DDRmenu" "$output_dir/DDRmenu"
chmod +x "$output_dir/DDRmenu"
"$STRIP" --strip-unneeded "$output_dir/openitg"

# The firmware is not expected to contain OpenITG's old third-party runtime
# dependencies.  Bundle the Bullseye ARM64 libraries while continuing to use
# the firmware's glibc, dynamic loader, pthread, libm, librt, and libdl.
mkdir -p "$output_dir/lib"
# Older bootstrap builds bundled these, but the firmware's GLES stack requires
# its own newer C++ runtime.  Remove stale copies from a reused output folder.
rm -f "$output_dir/lib/libstdc++.so.6" "$output_dir/lib/libgcc_s.so.1"
# FFmpeg is linked statically.  Remove libraries left by an older shared build
# so they cannot accidentally be deployed or loaded on the appliance.
rm -f "$output_dir/lib/libavcodec.so."* \
      "$output_dir/lib/libavformat.so."* \
      "$output_dir/lib/libavutil.so."* \
      "$output_dir/lib/libswscale.so."* \
      "$output_dir/lib/libswresample.so."*
copy_runtime_library()
{
  library_pattern=$1
  found=no
  rm -f "$output_dir/.runtime-library-found"
  find /usr/lib /lib -maxdepth 4 -name "$library_pattern" -print | while IFS= read -r path; do
    if file -L "$path" | grep -q 'ARM aarch64'; then
      cp -L "$path" "$output_dir/lib/$(basename "$path")"
      printf '%s\n' yes > "$output_dir/.runtime-library-found"
    fi
  done
  if test -f "$output_dir/.runtime-library-found"; then
    found=yes
    rm -f "$output_dir/.runtime-library-found"
  fi
  test "$found" = yes || {
    echo "Required ARM64 runtime library $library_pattern was not found" >&2
    exit 1
  }
}

for library in \
  libjpeg.so.62 \
  liblua5.1.so.0 \
  libogg.so.0 \
  libpng16.so.16 \
  'libresample.so.*' \
  libusb-0.1.so.4 \
  libvorbis.so.0 \
  libvorbisfile.so.3 \
  libz.so.1
do
  copy_runtime_library "$library"
done

file "$output_dir/openitg"
aarch64-linux-gnu-readelf -d "$output_dir/openitg"
aarch64-linux-gnu-readelf --version-info "$output_dir/openitg" > "$output_dir/version-info.txt"
echo "DDR Mini OpenITG build written to $output_dir/openitg"
