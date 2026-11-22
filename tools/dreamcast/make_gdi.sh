#!/bin/sh
# make_gdi.sh <dinocrisis2.elf> <data-root> [out-dir]
#
# A GD-ROM image (.gdi: GDEMU and other optical drive emulators, Flycast,
# Redream). <data-root> is the folder that holds YOUR copy of the game's
# Data/ folder (MOVIE/ and SPEECH/ optional), as for make_cdi.sh; its
# contents become the disc's root (/cd/Data/...).
#
# The layout of a GD-ROM: the low-density area (track 1, a small data track
# at sector 0; track 2, a short audio track at 450) and the high-density
# area from sector 45000 (track 3: the game). Track 3 is an ISO 9660 image
# written for sector 45000 (-C 0,45000) with IP.BIN in its first 16 sectors;
# 1ST_READ.BIN is the program as it is (a GD-ROM boots it unscrambled; a
# CD-R's is scrambled). The high-density area holds ~980 MB, so nothing has
# to be squeezed as on a CD-R (the converted speech and movies still read
# faster).
#
# Needs: xorriso (apt install xorriso) or genisoimage/mkisofs; makeip
# (KallistiOS's utils/makeip: make -C $KOS_BASE/utils/makeip, needs
# libpng-dev); sh-elf-objcopy; python3 (the image is checked at the end).
set -e
ELF=${1:?usage: make_gdi.sh <elf> <data-root> [out-dir]}
ROOT=${2:?usage: make_gdi.sh <elf> <data-root> [out-dir]}
OUT=${3:-gdi}

if command -v xorriso >/dev/null; then MKISO="xorriso -as mkisofs"
elif command -v genisoimage >/dev/null; then MKISO=genisoimage
elif command -v mkisofs >/dev/null; then MKISO=mkisofs
else echo "no xorriso, genisoimage or mkisofs (apt install xorriso)"; exit 1; fi
MAKEIP=$(command -v makeip || true)
[ -n "$MAKEIP" ] || [ -z "$KOS_BASE" ] || { [ -x "$KOS_BASE/utils/makeip/makeip" ] && MAKEIP=$KOS_BASE/utils/makeip/makeip; }
[ -n "$MAKEIP" ] || { echo "makeip not found: make -C \$KOS_BASE/utils/makeip (needs libpng-dev), or put it in PATH"; exit 1; }
OBJCOPY=$(command -v sh-elf-objcopy || echo "${KOS_CC_BASE:-/opt/toolchains/dc/sh-elf}/bin/sh-elf-objcopy")
[ -x "$OBJCOPY" ] || { echo "sh-elf-objcopy not found (source environ.sh)"; exit 1; }
if ! find "$ROOT" -mindepth 1 -maxdepth 1 -type d -iname data | grep -q .; then
    echo "$ROOT has no Data/ folder (put the game's Data folder directly inside it). It holds:"
    ls -1 "$ROOT" | sed 's/^/    /'
    exit 1
fi
[ -f "$(find "$ROOT" -maxdepth 3 -ipath '*data/bgm/index.txt' | head -1)" ] || \
    echo "WARNING: no Data/BGM/INDEX.TXT - the music will be silent (run dc2_music.py)."
if find "$ROOT" -maxdepth 2 -ipath '*movie/*.dat' | grep -q . && ! find "$ROOT" -maxdepth 2 -ipath '*movie/*.avi' | grep -q .; then
    echo "WARNING: MOVIE/ has no .AVI - the Dreamcast skips the movies (run dc2_movies.sh)."
fi
kb=$(du -sk "$ROOT" | cut -f1)
if [ "$kb" -gt $((980 * 1024)) ]; then
    echo "$ROOT is $((kb / 1024)) MB: more than a GD-ROM's high-density area holds (~980 MB)."; exit 1
fi

mkdir -p "$OUT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# the program and the boot sectors
"$OBJCOPY" -O binary -R .stack "$ELF" "$TMP/1ST_READ.BIN"
"$MAKEIP" -f -i "GD-ROM1/1" -g "DINO CRISIS 2" -c "DC2 RECOMP" -n "T0000" -e "V1.000" "$TMP/IP.BIN" >/dev/null
[ "$(wc -c < "$TMP/IP.BIN")" -eq 32768 ] || { echo "makeip made no 32 KB IP.BIN"; exit 1; }

# track 3: the game, from sector 45000
set -- -C 0,45000 -V DINO_CRISIS_2 -sysid "SEGA SEGAKATANA" -G "$TMP/IP.BIN" -l -J -joliet-long -r -o "$OUT/track03.iso" \
    -graft-points "/1ST_READ.BIN=$TMP/1ST_READ.BIN"
for entry in "$ROOT"/*; do
    name=$(basename "$entry")
    case "$name" in build|.git|gdi|*.cdi|*.gdi|*.iso|*.raw|*.elf|1ST_READ.BIN) continue ;; esac
    set -- "$@" "/$name=$entry"
done
echo "$MKISO (track 3: $((kb / 1024)) MB)"
$MKISO -quiet "$@"

# track 1: a small data track (a note) at sector 0, 300 sectors
mkdir "$TMP/t1"
printf 'Dino Crisis 2 (recompilation) for the Dreamcast.\r\nThe game is on the high-density area of this GD-ROM.\r\n' > "$TMP/t1/README.TXT"
$MKISO -quiet -V DINO_CRISIS_2 -o "$OUT/track01.iso" "$TMP/t1"
size=$(wc -c < "$OUT/track01.iso")
[ "$size" -le $((300 * 2048)) ] || { echo "track01 too big"; exit 1; }
dd if=/dev/zero bs=2048 count=$((300 - size / 2048)) 2>/dev/null >> "$OUT/track01.iso"

# track 2: audio, silence, at sector 450
dd if=/dev/zero bs=2352 count=302 2>/dev/null > "$OUT/track02.raw"

cat > "$OUT/disc.gdi" <<GDI
3
1 0 4 2048 track01.iso 0
2 450 0 2352 track02.raw 0
3 45000 4 2048 track03.iso 0
GDI

# the check: track 3's boot sectors, its volume, and 1ST_READ.BIN where the
# directory says it is (sectors counted from 45000)
python3 - "$OUT/track03.iso" "$TMP/1ST_READ.BIN" <<'PY'
import struct, sys
iso, binf = sys.argv[1], sys.argv[2]
d = open(iso, 'rb').read(); b = open(binf, 'rb').read()
assert d[:16] == b'SEGA SEGAKATANA ', 'no IP.BIN at the start of track 3'
pvd = d[16 * 2048:17 * 2048]
assert pvd[1:6] == b'CD001' and pvd[0] == 1, 'no ISO 9660 volume at sector 16'
root = pvd[156:156 + 34]
lba, size = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
assert lba >= 45000, 'the volume is not written for sector 45000 (root at %d)' % lba
dirb = d[(lba - 45000) * 2048:(lba - 45000) * 2048 + size]
i, found = 0, None
while i < len(dirb):
    n = dirb[i]
    if n == 0: i = (i // 2048 + 1) * 2048; continue
    name = dirb[i + 33:i + 33 + dirb[i + 32]].decode('latin-1').split(';')[0]
    if name.upper() == '1ST_READ.BIN': found = (struct.unpack_from('<I', dirb, i + 2)[0], struct.unpack_from('<I', dirb, i + 10)[0])
    i += n
assert found, 'no 1ST_READ.BIN in the root'
flba, fsize = found
assert fsize == len(b) and d[(flba - 45000) * 2048:(flba - 45000) * 2048 + fsize] == b, '1ST_READ.BIN is not where its record says'
print('track 3 checked: IP.BIN, volume at 45000, 1ST_READ.BIN (%d KB) at sector %d' % (fsize // 1024, flba))
PY
echo "wrote $OUT/disc.gdi, track01.iso, track02.raw, track03.iso"
