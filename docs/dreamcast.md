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
    tools/dreamcast/dc2_speech.py /srv/dc2/SPEECH              # voices: PCM -> IMA ADPCM 22050 Hz, in place

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
  A movie keeps real time (the console's timer), not the game's frames: when
  a frame runs late the next ones are decoded to catch up (up to 4 a frame),
  so the picture stays with the sound. The file is read once, in order; video
  chunks the sound reads past wait in a queue.

The Dreamcast build plays each voice from the disc as it goes
(`Snd_OpenFileVoice`, `src/pc/audio.c`) instead of reading it whole into the
game heap and copying it (the longest is 1.5 MB). The originals are PCM
waves, the lines at 18900 Hz mono and the films' sound effects
(`1000.DAT` on, roars among them) at 37800 Hz stereo: 151 KB a second, read
in the sound thread under the mixer's lock (holding up the sound and the
game's sound calls while the disc seeks) and resampled to the mix's rate.
`dc2_speech.py` makes them IMA ADPCM at 22050 Hz, channels kept, as the
music: 11 KB a second mono, 22 KB stereo, played as they are (~35 dB SNR,
the originals kept in `SPEECH_ORIG/`). Unconverted voices still play.

With all three, `Data` (~250 MB), `MOVIE` (~320 MB) and `SPEECH` (~10 MB,
43 MB unconverted) fit on an 80-minute CD-R.

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
| `DC2_DC_VERBOSE` | `1` | every 300 frames: game mode, room, game heap and surface peaks, malloc in use, free VRAM; the speed (below) |
| `DC2_DC_HEAP` | `0x300000` | the game heap (the PC build's 32 MB holds the music) |
| `DC2_UNALIGNED` | `0` | `1`: every memory access byte-safe (slow; see below) |

## What is Dreamcast-specific

| File | |
|---|---|
| `src/port/dc/main.c` | entry: video, PVR, pad, sound stream, the main loop and its pacing |
| `src/port/dc/aica_sfx.c` | the sound banks' sounds on the AICA's channels (`DC2_AICA`) |
| `src/port/dc/render_pvr.c` | the renderer (`include/dc2/render.h`) on the PowerVR2 |
| `src/port/dc/vmu_save.c` | saves on the VMU |
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
- **Unaligned access.** The game's data was laid out for the x86, which reads
  words at any address; the SH-4 faults. The game code keeps plain aligned
  accesses (fast); the places known to be unaligned use `UU16/UU32`
  (`include/dc2/types.h`; found with `-fsanitize=alignment` on Linux), and
  `src/port/dc/unaligned.c` emulates any other one in the address-error
  handler (counted; the verbose report lists their PCs, to resolve with
  `sh-elf-addr2line -e build/dc/dinocrisis2.elf`). `DC2_UNALIGNED=1` makes every
  access byte-safe instead (slow: a fallback if the handler misses a case).
- **Single precision.** The game's float maths repeats the x87's extended
  precision on the PC (`long double`, bit-identical in the harness); on the
  SH-4 the same code computes in `float` (`xreal`, `types.h`), as do the
  mixer's positions, and float-to-int conversions use the FPU directly.
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

## Speed and sound

With `DC2_DC_VERBOSE=1` every 300 frames also print a line like

    speed: 24.3 game frames/s (the game wants 30); game 31.2 ms (max 48), drawing 9.1 ms (max 15), waiting 0.0 ms; sound mixing 6% of the CPU

the real rate of game frames against the game's own (0x87DE00 sixtieths a
frame, mostly 30 a second), then the average and worst time of the game's
work (`Port_GameFrame`: logic, models, the order table), of the drawing
(`Port_DrawFrame`: the PC layer, the PVR's submission) and of the wait for
the next frame (0 when it runs late), and the share of the CPU the sound
mixer takes.

The frames are paced by the display's vertical blanks (`pace`,
`src/port/dc/main.c`): a frame ends at the blank the game's length after
the last one's (2 blanks at 30 fps), in step with the PVR, which shows a
new picture only at a blank. (Paced by a timer they began out of step and
waited for one blank more: ~37 ms a frame with the work under 33.) The
display's rate is measured at the start (`display: 60 Hz` in the log). A
frame over 100 ms prints `slow frame N: game X ms, drawing Y ms` (a few a
report), to find stalls.

The sound banks' sounds (`Snd_CreateBuffer`) are played by the AICA's own
channels (`src/port/dc/aica_sfx.c`, `DC2_AICA`): uploaded to sound RAM once
(stereo split into two channels, 8-bit made signed), then started, turned
(volume, pan in the AICA's 3 dB steps) and stopped by commands; the chip
resamples them itself. A sound's end is known by time (frames / rate); a
paused one-shot resumes from where it was, a paused loop from its start.
Sounds over 65534 frames (a channel's limit) and those that find no sound
RAM stay in the SH-4's mixer, as the music and the voices do. The log line
also counts the sounds on the AICA and its RAM left. `make aica-test`
checks the module on the host against stand-ins for KallistiOS.

The sound is mixed in the sound thread when the KOS stream asks for it
(`stream_cb`, `src/port/dc/main.c`), under the mixer's lock
(`src/pc/audio.c`), so it keeps its pace when a game frame runs long. (The
PC builds mix each game frame's share after the frame, as before.)

## Saves (VMU)

The game writes `SAVE00.DAT`..`SAVE99.DAT` (1524 bytes) with plain file calls.
On the Dreamcast they go to the RAM disk (`/ram`, `g_pc_save_root`), and
`vmu_save.c` keeps the first VMU in step with it:

- at start, every `DC2_SAVEnn` file on the VMU is unpacked into `/ram/SAVEnn.DAT`,
  so the game's load screen lists them;
- after each save the file is written to the VMU as `DC2_SAVEnn` with a VMS
  header: "DINO CRISIS 2", the slot and play time, and the save's own icon
  (the PS1 memory card icon the game puts in the file, doubled to 32x32).
  A save takes 5 blocks. The header keeps the exact length, so the game reads
  back its 1524 bytes.

Without a VMU the saves last until the console is switched off. The other
builds keep the saves next to `Data` (`dc2`: `DC2_SAVEDIR=<dir>` puts them
elsewhere).

## Memory (16 MB)

| | |
|---|---|
| program (text + bss) | 2.2 MB |
| game image without the code + scratch | 3.7 MB |
| game heap | 3 MB (peak seen: 2.1 MB) |
| PC layer surfaces (texture pages 256x256, back buffer, pictures) | 4.4-6.4 MB |
| sound: 16-bit banks play from the game heap (no copy), file blocks | ~0.1 MB |
| task stacks | up to 0.5 MB |

Surfaces get their pixels only when first used (an untouched 640x480 primary
costs nothing). That is still near the limit. The next cut is the surfaces: once a texture page is in
VRAM its main-RAM copy is only needed for partial updates.

VRAM (8 MB, ~5 MB left for textures): the PC layer's texture cache is dropped
at each room change (`PcDraw_DropTextures`: only the room's own pages come
back, from the emulated PS1 VRAM); textures unused in the last two scenes are
dropped when VRAM runs short; a texture whose power-of-two padding would waste
more than an eighth of a side is kept in pieces (`src/port/dc/pvr_tile.h`:
640 x 480 as 512 x 512 + 128 x 512, 640 KB instead of 1 MB), its triangles
cut to each piece in texture space (`make pvr-tile-test` checks the cut).

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
- the memory cut above, and speed work after profiling on the console.
