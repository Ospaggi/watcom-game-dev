#!/bin/bash
# Engine asset pipeline. Generates game-ready files in SRC/.
# Base assets only: palette, font, one SFX, and the VGM music tracks.
# (Game-specific sprite conversion is added back per-game.)

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

set -e

echo "Generating palette..."
bun tools/mkpalette.ts

echo "Compiling fonts..."
bun tools/mkfont.ts

echo "Converting SFX..."
bun tools/mksfx.ts "/Users/gcjjyy/Documents/게임개발/fx_sounds/ui-sound4.ogg" SRC/FIRE.SFX

echo "Copying VGM..."
cp "/Users/gcjjyy/lab/oscc/imsplay/public/18 Tyrian, The Level.vgm" SRC/TYRIAN.VGM
cp ASSETS/raw/audio_vgm/ST00.vgm SRC/ST00.VGM
cp ASSETS/raw/audio_vgm/ST01.vgm SRC/ST01.VGM
cp ASSETS/raw/audio_vgm/ST02.vgm SRC/ST02.VGM
cp ASSETS/raw/audio_vgm/ST03.vgm SRC/ST03.VGM
cp ASSETS/raw/audio_vgm/ST06.vgm SRC/ST06.VGM
cp ASSETS/raw/audio_vgm/ST07.vgm SRC/ST07.VGM

echo "Done."
