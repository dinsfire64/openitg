#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
image_name=${DDRMINI_DOCKER_IMAGE:-openitg-ddrmini-cross:bullseye}
output_dir=${DDRMINI_OUTPUT_DIR:-$repo_dir/Build/ddrmini-openitg}

mkdir -p "$output_dir"
docker build -t "$image_name" "$script_dir"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  --env DDRMINI_BUILD_JOBS="${DDRMINI_BUILD_JOBS:-2}" \
  --volume "$repo_dir:/src:ro" \
  --volume "$output_dir:/out" \
  "$image_name" /src /out
