#!/bin/bash
# ps2-forge headless runner: boot an ELF in Play! under Xvfb + software GL and
# grab a screenshot. The agentic test loop: build -> shot -> read the PNG.
#   tools/shot.sh <elf> [seconds] [out.png]
# Deps: Play! in PATH, Xvfb, mesa/llvmpipe, python3 + mss + Pillow.
set -u
ELF="$1"; WAIT="${2:-3}"; OUT="${3:-/tmp/ps2shot.png}"
export DISPLAY=:99
pkill -f "Xvfb :99" 2>/dev/null; pkill -f "usr/bin/Play" 2>/dev/null; sleep 1
Xvfb :99 -screen 0 1280x720x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
sleep 2
LIBGL_ALWAYS_SOFTWARE=1 Play --elf "$ELF" >/dev/null 2>&1 & PP=$!
sleep "$WAIT"
python3 - "$OUT" <<'PY'
import sys, mss
from PIL import Image
with mss.mss() as s:
    im = s.grab(s.monitors[1])
    Image.frombytes("RGB", im.size, im.bgra, "raw", "BGRX").save(sys.argv[1])
print("saved", sys.argv[1])
PY
kill "$PP" "$XP" 2>/dev/null
