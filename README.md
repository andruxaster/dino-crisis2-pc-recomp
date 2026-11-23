# dino-crisis2-pc-recomp

A recompilation of **Dino Crisis 2** (PC, `Dino2.exe`, 2002) into C: the
game's code rewritten function by function with the original's behaviour,
checked against the original exe. It runs natively on Linux and Windows
(SDL2 + OpenGL) and on the **Dreamcast** (KallistiOS).

**No game data is included.** You need your own copy of the game; its
`Data` folder (and, optionally, `MOVIE` and `SPEECH` from the CD) is read at
run time.

## Status

* The game plays from the title through the opening, the rooms and their
  cutscenes, the menus, saving and loading; 2500+ functions rewritten, each
  checked against the original code (0 differences).
* Linux and Windows: the standalone game with sound, music, voices and movies.
* Dreamcast: runs in Flycast (movies as Cinepak, music
  as IMA ADPCM, saves on the VMU); speed and memory are being tuned.

What is done and what is left: [docs/progress.md](docs/progress.md).

## Quick start

Linux (SDL2 development package installed):

    cmake -S . -B build/cmake-linux -DCMAKE_BUILD_TYPE=Release
    cmake --build build/cmake-linux -j
    ./build/cmake-linux/dinocrisis2      # Data next to it, or [Assets] Path in config.ini

Windows: cross-compile with mingw-w64 or use Visual Studio, see
[docs/building.md](docs/building.md). A release is `dinocrisis2.exe`,
`SDL2.dll` and `config.ini`, with the game's `Data` folder next to them.

Dreamcast: `make -f Makefile.dreamcast` with a KallistiOS toolchain, then a
disc image from your data, see [docs/dreamcast.md](docs/dreamcast.md).

Keyboard: arrows, Z confirm, X back, C action, Enter start, Escape quits.
Game controllers (DualSense, DualShock 4, Xbox and others SDL knows, USB or
Bluetooth): d-pad or left stick, Cross/A confirm, Circle/B back, Options/Start
start, Create/Back (or the touchpad) the map; see
[docs/building.md](docs/building.md).

## Documentation

| | |
|---|---|
| [docs/building.md](docs/building.md) | all builds (make, CMake, Windows), run options and debug switches |
| [docs/recompilation.md](docs/recompilation.md) | how the recompilation works: the builds, the rules of the code, the harness, the hook DLL, the exe's tables, floating point |
| [docs/progress.md](docs/progress.md) | what is rewritten, by area; what is not yet |
| [docs/dreamcast.md](docs/dreamcast.md) | the Dreamcast port: toolchain, data conversion, memory, speed |
| [docs/gog_patches.md](docs/gog_patches.md) | the GOG release's patches |
| `docs/*.h` | the data formats and engine structures as C headers |

## Third-party code

[minimp3](https://github.com/lieff/minimp3) (CC0) and
[pl_mpeg](https://github.com/phoboslab/pl_mpeg) (MIT), in `third_party/`.

Dino Crisis 2 is © Capcom. This project is not affiliated with Capcom.
