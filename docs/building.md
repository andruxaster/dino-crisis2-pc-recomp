# Building and running

    make            # needs i686-w64-mingw32-gcc
    make check      # harness under wine (DINO2=.../Dino2.exe DATA=.../Data)
    make linux      # native static library + build/smoke + build/dc2
    make dc2gl      # build/dc2gl: dc2 in a window (SDL2 + OpenGL)

## The game on Linux and Windows (CMake)

The standalone game, `dinocrisis2` (SDL2 + OpenGL; `dinocrisis2.exe` on
Windows), and the headless `dc2` build with CMake on both systems; game logic,
renderer and sound are the same code, only the task switching differs
(ucontext / Windows fibers):

    # Linux (SDL2 development package installed)
    cmake -S . -B build/cmake-linux -DCMAKE_BUILD_TYPE=Release
    cmake --build build/cmake-linux -j

    # Windows, cross-compiled with mingw-w64 (x86_64-w64-mingw32-gcc) and
    # SDL2's mingw development package (SDL2-devel-2.x-mingw from libsdl.org)
    cmake -S . -B build/cmake-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
          -DSDL2_DIR=<SDL2-2.x>/x86_64-w64-mingw32/lib/cmake/SDL2 -DCMAKE_BUILD_TYPE=Release
    cmake --build build/cmake-win -j
    tools/package_windows.sh <SDL2-2.x>       # dist/dinocrisis2-windows-x64.zip

    # Windows with Visual Studio (SDL2-devel-2.x-VC): not tried yet
    cmake -S . -B build/vs -DSDL2_DIR=<SDL2-2.x>/cmake && cmake --build build/vs --config Release

A release is `dinocrisis2.exe`, `SDL2.dll` and `config.ini` (from
`config.ini.template`); no game data. The game looks for its `Data` folder
next to the executable, or under `[Assets] Path` of `config.ini` (relative to
the executable's folder), so it starts the same from a shortcut:

    DinoCrisis2/
      dinocrisis2.exe   (or dinocrisis2 on Linux)
      SDL2.dll
      config.ini        ([Display] Scale, FullScreen; [Assets] Path)
      Data/             (the installed game's DATA folder)
      MOVIE/, SPEECH/   (from the CD, optional)

Fullscreen uses the desktop's resolution and keeps the picture 4:3. A
`DATA_DIR` given on the command line overrides the search; the game runs until
the window is closed (or for `FRAMES`). The Makefile's `dc2gl` is the same
program.

`dc2gl ROOT/Data [FRAMES] [presses]` takes the pad from the
keyboard as well (arrows, Z confirm, X back, C action, Enter start; Escape
quits). `DC2_STATS=1` prints each frame in numbers (draws, vertices, texture
uploads, mean colour, share of lit pixels), `DC2_DRAWLOG=F` (and
`DC2_DRAWLOG_N`) every draw of frames F.., `DC2_FAST=1` runs without frame
pacing, `DC2_SCALE=n` enlarges the window. It plays the sound effects, the voice
and the rooms' music (the MP3 streams of the room files, decoded with
[minimp3](https://github.com/lieff/minimp3), CC0, in `third_party/`);
`DC2_MUTE=1` silences it, `DC2_WAV=file.wav` writes the mixed sound to a
file (also with the headless `dc2`), `DC2_SNDLOG=1` prints the sound calls. `src/port/dc/render_pvr.c` needs
KallistiOS; without a KOS toolchain it can be syntax-checked with
`sh4-linux-gnu-gcc -fsyntax-only` against KOS's headers.
