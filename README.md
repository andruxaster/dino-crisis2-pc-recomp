# dino-crisis2-recomp — Dino Crisis 2 PC recompilation

Reverse engineering and recompilation of Dino Crisis 2 for PC (`Dino2.exe`,
2002), aiming at native Linux/Windows builds and a Dreamcast port
(KallistiOS). **No game data is included**: everything here works on your
own copy of the game. Put it in `game/` (ignored by git):

    game/Dino2.exe
    game/Data/        (the DATA folder of the installed game)
    game/MOVIE/, game/SPEECH/   (from the CD, optional)

| folder | contents |
|---|---|
| `include/`, `src/`, `tests/` | the recompilation (below) |
| `tools/` | Python tools for the data formats: containers/LZSS (`dc2dat.py`), rooms, collision, cameras (`dc2room.py`), room scripts (`dc2script.py`), objects and enemy sets (`dc2objects.py`), models, animations, cutscenes; `dc2_labels.csv` (function/data names, `ghidra_apply_labels.py`), room and file tables |
| `docs/` | formats and engine notes as C headers: `dc2_game_flow.h` (files, rooms, collision, scripts, objects, models), `dc2_pc_prims.h`, `dc2_texcache.h`, `dc2_render_states.h` |
| `viewer/` | `dc2view`: SDL2/GL room viewer (backgrounds, masks, models, collision, doors, script-placed dinosaurs) |

## The recompilation (dc2re)

Game code of `Dino2.exe` (2002 PC release) rewritten in C, function by
function, with the same addresses, arguments and behaviour as the original.
One source tree, three builds:

| build | what | status |
|---|---|---|
| `build/dinput.dll` | hook DLL: put next to `Dino2.exe`, redirects the original functions to the C ones | runs inside the game |
| `build/harness_stub.exe` + `harness.dll` | checks every C function against the original code | 26 functions, ~55 000 checks, 0 differences |
| `build/smoke` (Linux) | the C game code without any exe code, on an emulated memory image | room load + script VM |

The Dreamcast (KallistiOS) build will reuse the standalone path once the
whole game logic is in C.

## Rules of the code

* Game memory keeps the original layout. A pointer stored in game data is a
  32-bit address of the original process (`gptr`); code reads and writes
  it only through `MEM()/U8()/U16()/U32()` (`include/dc2/types.h`). In the
  DLL `g_membase` is 0x400000 (identity), elsewhere it points to a buffer,
  so the same C works in 64-bit and on the Dreamcast.
* Each function carries its original address in a comment and is listed in
  `src/hook/hooks.c`. A function goes into the table only after the harness
  shows no difference.
* Calls between functions go to the C version; anything not rewritten yet
  is reached through the original address (DLL / harness only).

## Building

    make            # needs i686-w64-mingw32-gcc
    make check      # harness under wine (DINO2=.../Dino2.exe DATA=.../Data)
    make linux      # native static library + build/smoke

The harness exe is based at 0x400000 with a large .bss, so that the range of
the game image is free; `harness.dll` copies the game sections there, calls
original and rewritten function with the same inputs (random and edge
cases, real LZSS records of all `.DAT` files, random script programs run by
`Script_Run`) and compares return values and both writable sections byte
by byte. An injected bug (`>` turned into `>=`) is caught.

## Running the game with the hook DLL

Copy `build/dinput.dll` next to `Dino2.exe`. Optional `dc2re.ini`:

    [hooks]
    off=Script_Run,PointInQuad   ; or off=* to disable all
    trace=1                      ; dc2re.log: call counts every 5 s

Under wine: `WINEDLLOVERRIDES=dinput=n,b wine Dino2.exe`. The game looks for
a CD drive (C:..L:) holding `MOVIE\OPEN_30.DAT`; it streams the movies and
`Speech\NNNN.DAT` from there. Under wine a folder with `MOVIE` and `SPEECH`
works as that drive when it is mapped as a CD-ROM:

    ln -s /path/to/cdfolder ~/.wine/dosdevices/d:
    wine reg add 'HKLM\Software\Wine\Drives' /v d: /t REG_SZ /d cdrom /f

Checked under wine 9 (32-bit prefix): all 26 hooks installed; after one
minute Lzss_Decode 23 calls, Flag_Check 62 000, Script_Run every frame,
PointInQuad 5 000.

## Reimplemented (26)

| address | function | file |
|---|---|---|
| 0x4041F0 | Lzss_Decode | src/game/lzss.c |
| 0x46FAA0 / 0x46FB20 | Flag_Set / Flag_Check | src/game/flag.c |
| 0x46FB50 / 0x46FB90 | Bits_Set / Bits_Check | src/game/flag.c |
| 0x472210 | Room_GlobalIndex | src/game/geom.c |
| 0x48F790 | PointInQuad | src/game/geom.c |
| 0x48B8E0 / 0x48B930 / 0x48B980 / 0x48BB20 | Script_GetValue / SetValue / BinOp / SetCond | src/game/script_vm.c |
| 0x48BC50 / 0x48BDB0 / 0x48BE20 | Script_Run / StartThread / KillThread | src/game/script_vm.c |
| 0x489990, 0x4873C0..0x4875D0, 0x487620, 0x487B70, 0x487BF0 | script opcodes 00..08, 10, 1C, 1D | src/game/script_vm.c |

## Next

Script opcodes by use in the 88 room scripts: 1E obj_set (5422),
12 obj_select (2506), 1F obj_get (1235), 45 obj_enable (1188),
2A wait_frame (1004), 21 anim_start (705), 4A (655), 44 (445),
1A obj_spawn (442) — `build/smoke` stops at the first one missing
(ST100: 44). Then the file loader (File_Process 0x404290 and record
handlers), rooms and cameras, objects and their per-type handlers; the
PC layer (0x4B4000.., PsyQ on DirectX) is rewritten for SDL/GL rather than
decompiled.
