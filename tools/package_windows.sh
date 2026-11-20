#!/bin/sh
# Builds the Windows release with mingw-w64 and lays it out in dist/:
#   dist/dinocrisis2-windows-x64/{dinocrisis2.exe, SDL2.dll, config.ini}
# and a .zip of it. Needs SDL2's mingw development package:
#   tools/package_windows.sh /path/to/SDL2-2.x.y   (the unpacked SDL2-devel-*-mingw)
# The game's data is not included: put its Data folder next to the executable
# (or set [Assets] Path in config.ini).
set -e
SDL=${1:?usage: $0 SDL2_MINGW_DIR}
cd "$(dirname "$0")/.."
B=build/cmake-win
OUTDIR=dist/dinocrisis2-windows-x64
cmake -S . -B $B -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DSDL2_DIR="$SDL/x86_64-w64-mingw32/lib/cmake/SDL2" -DCMAKE_BUILD_TYPE=Release
cmake --build $B -j
rm -rf $OUTDIR
cmake --install $B --prefix $OUTDIR
x86_64-w64-mingw32-strip $OUTDIR/dinocrisis2.exe
(cd dist && rm -f dinocrisis2-windows-x64.zip && zip -qr dinocrisis2-windows-x64.zip dinocrisis2-windows-x64)
echo "dist/dinocrisis2-windows-x64.zip"
