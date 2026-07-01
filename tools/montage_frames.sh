#!/usr/bin/env bash
# montage_frames.sh - Concatenate N same-size PNGs into a single horizontal strip.
#
# All inputs must have identical w x h. Output PNG is N*w wide and h tall.
# Legacy helper for packing separate PNG frames into a single horizontal sheet.
# for `mksprite --bin --grid Nx1`.
#
# Usage: tools/montage_frames.sh <out.png> <in1.png> <in2.png> [<in3.png>...]

set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <out.png> <in1.png> <in2.png> [<inN.png>...]" >&2
  exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "error: ImageMagick 'magick' not found in PATH (try: brew install imagemagick)" >&2
  exit 1
fi

OUT="$1"
shift

# Concatenate horizontally; force 8-bit per channel to avoid reduction to 4-bit
magick "$@" -define png:color-type=6 -depth 8 +append "$OUT"
