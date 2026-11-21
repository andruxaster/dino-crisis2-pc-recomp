# Dreamcast port (KallistiOS)

Status: **first build, not run yet**. The game code is the same as the PC
builds'; `Makefile.dreamcast` builds it with KOS into `build/dc/dinocrisis2.elf`.
The code paths it uses were checked on Linux in the same configuration (see
*Checked on Linux*); on the console itself nothing has run yet.

Toolchain: KallistiOS (master, 2026-10), GCC 15.2 / binutils 2.45.1 / newlib
(`kos-chain` "stable" profile).

## Preparing the data (once, on a copy of your game)

The Dreamcast build reads the music and the movies in other formats than the
PC builds (which keep using the originals):

    tools/dreamcast/dc2_music.py  /srv/dc2/Data --strip        # music: MP3 -> Data/BGM/*.WAV (IMA ADPCM)
    tools/dreamcast/dc2_movies.sh /srv/dc2/MOVIE --remove-dat  # movies: MPEG-1 -> MOVIE/*.AVI (Cinepak)

- **Music.** Each area's `MS_*.DAT` holds its music as MP3 streams that the PC
  game loads whole (8 MB and more). `dc2_music.py` writes each distinct stream
  once as a WAV with IMA ADPCM at 22050 Hz stereo (39 streams, ~59 MB) and
  `Data/BGM/INDEX.TXT` (which file and record uses which WAV); `--strip` cuts
  the `MS_*.DAT` down to their 2 KB header (the originals go to `MS_ORIG/`
  next to `Data`). The game plays the WAV from the disc as it goes, one
  ADPCM block in memory (`Strm_OpenFile`, `src/pc/audio.c`).
- **Movies.** `dc2_movies.sh` makes `NAME.AVI` (Cinepak video, same size and
  frame rate, IMA ADPCM sound at 22050 Hz) next to each `NAME.DAT`; the
  Dreamcast build plays only the `.AVI` (`src/pc/movie_avi.c`) and skips a
  movie that has none. ffmpeg's Cinepak encoder is slow (here ~10 s a second
  of movie); the 12 movies come to ~320 MB.

With both, `Data` (~250 MB), `MOVIE` (~320 MB) and `SPEECH` (43 MB) fit on an
80-minute CD-R.

## Building

    source /opt/toolchains/dc/kos/environ.sh
    make -f Makefile.dreamcast                         # build/dc/dinocrisis2.elf
    make -f Makefile.dreamcast cdi DATA=/srv/dc2       # CD-R image (needs mkdcdisc)
    make -f Makefile.dreamcast DC2_DC_ROOT=/pc/srv/dc2 run DC_IP=192.168.1.50   # dcload

`DATA` (or the dcload folder) holds the game's `Data/` folder; `MOVIE/` and
`SPEECH/` may sit next to it. Its contents become the disc's root, so the game
reads `/cd/Data/...`.

| Switch | Default | |
|---|---|---|
| `DC2_DC_ROOT` | `/cd` | the folder that holds `Data/` (`/pc/<dir>` under dcload) |
| `DC2_DC_AUDIO` | `1` | `0`: no sound at all |
| `DC2_DC_MOVIES` | `1` | `0`: skip the movies |
| `DC2_DC_VERBOSE` | `1` | every 300 frames: game mode, room, game heap and surface peaks, malloc in use, free VRAM |
| `DC2_DC_HEAP` | `0x300000` | the game heap (the PC build's 32 MB holds the music) |

## What is Dreamcast-specific

| File | |
|---|---|
| `src/port/dc/main.c` | entry: video, PVR, pad, sound stream, the main loop and its pacing |
| `src/port/dc/render_pvr.c` | the renderer (`include/dc2/render.h`) on the PowerVR2 |
| `src/port/dc/ctx_sh4.S`, `ctx_sh4.h` | the task system's context switch (KOS has no ucontext) |
| `src/game/task.c` | the task backend for KOS (`__DREAMCAST__`) |
| `src/port/frame.c` | the main loop's game work, shared with `dc2.c` |
| `tools/dreamcast/make_cdi.sh` | the CDI image |

And in the shared code, behind switches that the PC builds leave off:

- **`DC2_LOWMEM`** (`src/game/file.c`): containers are read record by record
  instead of whole, and the room music's streams (record type 4, the MP3 of
  `MS_*.DAT`) are not read: they play from `Data/BGM/` (above). The game heap
  peaks at ~2.5 MB instead of ~19 MB.
- **`DC2_MOVIE_AVI`** (`src/pc/movie.c`, `movie_avi.c`): the movies as Cinepak
  AVI, decoded straight to ARGB1555 (checked against ffmpeg's decoder: the
  same frames, differences only in the last bit of 5).
- **Unaligned access** (`include/dc2/types.h`): on the SH-4 every `U16/U32/
  S16/S32/F32/D64` goes through a packed member, because the game's data was
  laid out for the x86 and may be unaligned anywhere. Correct, but slower;
  the hot spots can be given aligned fast paths later, by profile.
- **The image without the code**: the original's `.text` (0x400000..0x4E8000,
  ~930 KB) is not allocated; `g_membase` points before the buffer.
- **Paths** (`src/game/pcio.c`): on KOS names are used as they are (no folder
  scan per component, ISO9660 is case-insensitive).

## The pad

| Dreamcast | the game's pad bit | |
|---|---|---|
| D-pad, analog stick | 0x1000 / 0x4000 / 0x8000 / 0x2000 | up, down, left, right |
| A | 0x20 | action / confirm |
| B | 0x40 | cancel |
| X | 0x10 | (PC: V) |
| Y | 0x100 | status screen |
| L / R triggers | 0x04 / 0x08 | (PC: G / Shift) |
| Start | 0x800 | pause |
| L + R + Start | 0x01 | map |
| A+B+X+Y+Start | | back to the loader |

## Memory (16 MB)

| | |
|---|---|
| program (text + bss) | 2.2 MB |
| game image without the code + scratch | 3.7 MB |
| game heap | 3 MB (peak seen: 2.1 MB) |
| PC layer surfaces (texture pages 256x256, back buffer, pictures) | 4.4-6.4 MB |
| sound buffers (PCM copies of the banks), task stacks | ~1 MB |

That is at the limit. The next cut is the surfaces: once a texture page is in
VRAM its main-RAM copy is only needed for partial updates.

## Checked on Linux

`dc2` built with `DC2_LOWMEM DC2_UNALIGNED HEAP_SIZE=0x300000` and run with
`DC2_GUARD_TEXT=1` (the code region made inaccessible) plays the new game
through rooms 101 and 102, the pause screen, the options, the status screen
and the map, and gives the same trace as the normal build. Debug switches of
`dc2`: `DC2_HEAPSTAT=1` (game heap and surface peaks), `DC2_HEAPLOG=1` (heap
blocks of 64 KB and more), `DC2_SURFLOG=1` (the live surfaces at each new peak).

On Linux, `dc2` built with `DC2_LOWMEM DC2_MOVIE_AVI` and the converted data
plays the room music with the same loudness, 10 s window by window, as the PC
build's MP3, and the title's movie and attract loop with the same timing
(one frame longer per movie: pl_mpeg, in the PC builds, does not give the
MPEG files' last picture).

## Not done yet

- the renderer's alpha test (punch-through list), read-back (frame copies see black);
- saves on the VMU (the game writes `SAVExx.DAT` with plain file calls);
- the memory cut above, and speed work after profiling on the console.
