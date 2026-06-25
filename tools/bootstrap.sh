#!/bin/bash
# One-time toolchain setup for ps2-forge: downloads the prebuilt ps2dev
# toolchain (ee-gcc + ps2sdk + gsKit). No sudo. Linux x86_64 (Ubuntu build;
# also runs on Debian). For macOS/Windows, grab the matching asset from
# https://github.com/ps2dev/ps2dev/releases and set the same env vars.
#   tools/bootstrap.sh [install-dir]   (default: ~/ps2dev)
set -e
DEST="${1:-$HOME/ps2dev}"
URL="https://github.com/ps2dev/ps2dev/releases/download/v2.0.0/ps2dev-ubuntu-latest.tar.gz"
mkdir -p "$DEST"
echo "downloading ps2dev toolchain -> $DEST ..."
curl -sSL -o /tmp/ps2dev.tar.gz "$URL"
tar -xzf /tmp/ps2dev.tar.gz -C "$DEST" --strip-components=1
rm -f /tmp/ps2dev.tar.gz
echo
echo "done. add this to your shell, then 'make' any example:"
echo "  export PS2DEV=$DEST PS2SDK=\$PS2DEV/ps2sdk GSKIT=\$PS2DEV/gsKit"
echo "  export PATH=\$PS2DEV/ee/bin:\$PS2DEV/bin:\$PS2SDK/bin:\$PATH"
