#!/bin/bash
# Run the compiled game (from DIST/) in DOSBox
# Usage: ./run.sh [exe_name]

DOSBOX="/Applications/dosbox.app/Contents/MacOS/DOSBox"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXE_FILE="${1:-GAME.EXE}"

"$DOSBOX" \
  -c "MOUNT C $PROJECT_DIR" \
  -c "C:" \
  -c "CALL AUTOEXEC.BAT" \
  -c "CD DIST" \
  -c "$EXE_FILE" 2>/dev/null
