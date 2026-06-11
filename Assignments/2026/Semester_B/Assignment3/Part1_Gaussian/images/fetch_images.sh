#!/usr/bin/env bash
# Re-runnable: downloads a handful of real test images from picsum.photos
# (CC0). Seeds are fixed so the same URLs always return the same image.
# Skips any file that already exists.

set -euo pipefail

cd "$(dirname "$0")"

fetch() {
    local out="$1"
    local url="$2"
    if [[ -f "$out" ]]; then
        echo "[skip] $out already present"
    else
        echo "[get]  $out  <-  $url"
        curl -sSL -o "$out" "$url"
    fi
}

fetch small.jpg  "https://picsum.photos/seed/parallel-small/800/600"
fetch medium.jpg "https://picsum.photos/seed/parallel-medium/1600/1200"
fetch large.jpg  "https://picsum.photos/seed/parallel-large/2400/1800"
fetch xlarge.jpg "https://picsum.photos/seed/parallel-xlarge/3200/2400"

mkdir -p output
echo "Done. Images in $(pwd)"
ls -lh *.jpg
