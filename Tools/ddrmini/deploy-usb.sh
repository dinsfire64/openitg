#!/bin/sh
set -eu

usage()
{
	echo "Usage: $0 USB_ROOT" >&2
	exit 2
}

[ "$#" -eq 1 ] || usage

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
usb_root=$1
app_dir="$usb_root/openitg"

[ -d "$usb_root" ] || {
	echo "USB root is not a directory: $usb_root" >&2
	exit 1
}

# Refuse broad or obviously accidental deployment targets.  The caller must
# provide the mounted USB directory itself, not the filesystem root.
case "$usb_root" in
	/|/home|/root|/usr|/var|/tmp)
		echo "Refusing unsafe USB root: $usb_root" >&2
		exit 1
		;;
esac

for required in \
	"$repo_dir/assets/d4" \
	"$repo_dir/assets/patch-data" \
	"$repo_dir/assets/game-data"
do
	[ -e "$required" ] || {
		echo "Required deployment input is missing: $required" >&2
		exit 1
	}
done

mkdir -p "$app_dir"

echo "Installing base d4 assets..."
cp -a "$repo_dir/assets/d4/." "$app_dir/"

echo "Applying patch-data assets..."
cp -a "$repo_dir/assets/patch-data/." "$app_dir/"

echo "Applying game-data assets..."
cp -a "$repo_dir/assets/game-data/." "$app_dir/"

sync

echo "Asset deployment complete: $app_dir"
echo "Writable OpenITG data was preserved at: $app_dir/.openitg"
