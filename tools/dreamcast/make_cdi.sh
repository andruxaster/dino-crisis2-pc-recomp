#!/bin/sh
# make_cdi.sh <dinocrisis2.elf> <data-root> [out.cdi]
#
# A bootable CDI (Flycast, CD-R) made with mkdcdisc
# (https://gitlab.com/simulant/mkdcdisc). <data-root> is a folder that holds
# YOUR copy of the game's Data/ folder (MOVIE/ and SPEECH/ optional); its
# contents become the disc's root, so the game reads /cd/Data/... (the ELF
# built with the default DC2_DC_ROOT=/cd).
set -e
ELF=${1:?usage: make_cdi.sh <elf> <data-root> [out.cdi]}
ROOT=${2:?usage: make_cdi.sh <elf> <data-root> [out.cdi]}
OUT=${3:-dinocrisis2.cdi}

command -v mkdcdisc >/dev/null || { echo "mkdcdisc not found in PATH"; exit 1; }
if ! find "$ROOT" -mindepth 1 -maxdepth 1 -type d -iname data | grep -q .; then
    echo "$ROOT has no Data/ folder (put the game's Data folder directly inside it). It holds:"
    ls -1 "$ROOT" | sed 's/^/    /'
    exit 1
fi
kb=$(du -sk "$ROOT" | cut -f1)
if [ "$kb" -gt $((680 * 1024)) ]; then
    echo "WARNING: $ROOT is $((kb / 1024)) MB - more than an 80-minute CD-R holds (emulators may still load it)."
fi

set -- -e "$ELF" -n "DINO CRISIS 2" -o "$OUT"
for entry in "$ROOT"/*; do
    name=$(basename "$entry")
    case "$name" in build|.git|*.cdi|*.elf) continue ;; esac
    if [ -d "$entry" ]; then set -- "$@" -d "$entry"
    elif [ -f "$entry" ]; then set -- "$@" -f "$entry"
    fi
done
echo "mkdcdisc $*"
mkdcdisc "$@"
echo "wrote $OUT"
