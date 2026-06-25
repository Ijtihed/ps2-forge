#!/bin/bash
# ps2-forge headless runner + verdict. Boots an ELF in Play! under Xvfb +
# software GL, screenshots it, and prints a RENDER: PASS/FAIL the agent can read
# (so the loop is build -> one command -> verdict, no eyeballing required).
#   tools/shot.sh <elf> [seconds] [out.png]
# Deps: Play! in PATH, Xvfb, mesa/llvmpipe, python3 + mss + Pillow.
# Exit 0 if the game rendered something, 1 if it stayed blank / failed to boot.
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
    img = Image.frombytes("RGB", im.size, im.bgra, "raw", "BGRX")
img.save(sys.argv[1])
# verdict: look at the central game area; did it draw varied, non-black pixels?
W, H = img.size
crop = img.crop((int(W*0.22), int(H*0.10), int(W*0.70), int(H*0.80))).convert("RGB")
px = list(crop.getdata())
nonblack = sum(1 for r,g,b in px if r+g+b > 36)
colors = len(set((r>>4, g>>4, b>>4) for r,g,b in px))
frac = nonblack/len(px)
ok = colors >= 5 and frac > 0.005   # rendered something vs a blank/black screen
print("RENDER:", "PASS" if ok else "FAIL",
      "| distinct=%d nonblack=%.1f%% -> %s" % (colors, frac*100, sys.argv[1]))
sys.exit(0 if ok else 1)
PY
RC=$?
kill "$PP" "$XP" 2>/dev/null
exit $RC
