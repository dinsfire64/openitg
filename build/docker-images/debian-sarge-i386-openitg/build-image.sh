#!/bin/bash

if [ ! "x`whoami`" = "xroot" ]; then
        echo "$0: you must be root to build the container image"
        exit 1;
fi

export rootfs=/var/tmp/rootfs

echo "Downloading debs..."
sudo debootstrap --arch=i386 sarge $rootfs http://archive.debian.org/debian/

rm -rf $rootfs/{dev,proc}
mkdir -p $rootfs/{dev,proc}

mkdir -p $rootfs/etc
cp /etc/resolv.conf $rootfs/etc/

echo "Compressing..."
tar --numeric-owner -caf $rootfs/rootfs.tar.xz -C $rootfs --transform='s,^./,,' .

mv $rootfs/rootfs.tar.xz .

rm -r $rootfs

cat > Dockerfile <<EOF
FROM scratch
ADD rootfs.tar.xz /

RUN echo "deb http://archive.debian.org/debian-backports sarge-backports main contrib non-free" >> /etc/apt/sources.list
RUN apt-get update -y
RUN apt-get install -y build-essential gettext automake1.8 gcc g++ libavcodec-dev libavformat-dev libxt-dev libogg-dev libpng-dev libjpeg-dev libvorbis-dev libusb-dev libglu1-mesa-dev libx11-dev libxrandr-dev liblua50-dev liblualib50-dev nvidia-glx-dev libmad0-dev libasound2-dev git-core automake1.7 autoconf libncurses5-dev pilrc prc-tools prc-tools-doc

CMD ["/bin/bash"]
EOF

export DOCKER_ALLOW_SCHEMA1_PUSH_DONOTUSE=1
export DOCKER_ENABLE_DEPRECATED_PULL_SCHEMA_1_IMAGE=1

docker build -t debian-sarge-i386-openitg .
