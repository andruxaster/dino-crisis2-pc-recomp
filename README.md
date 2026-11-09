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
| `tools/` | Python tools for the data formats: containers/LZSS (`dc2dat.py`), rooms, collision, cameras (`dc2room.py`), room scripts (`dc2script.py`), objects and enemy sets (`dc2objects.py`), models, animations, cutscenes; `dc2_labels.csv` (function/data names, `ghidra_apply_labels.py`), room and file tables; `dc2_psx_map.py` → `dc2_psx_map.csv`: PC functions paired with the PS1 version's (SLUS_012.79 and its code overlays: areas ST0..9, enemies E00..EA0, weapons, menus; put in `game/PSX/`) |
| `docs/` | formats and engine notes as C headers: `dc2_game_flow.h` (files, rooms, collision, scripts, objects, models), `dc2_pc_prims.h`, `dc2_texcache.h`, `dc2_render_states.h` |
| `viewer/` | `dc2view`: SDL2/GL room viewer (backgrounds, masks, models, collision, doors, script-placed dinosaurs) |

## The recompilation (dc2re)

Game code of `Dino2.exe` (2002 PC release) rewritten in C, function by
function, with the same addresses, arguments and behaviour as the original.
One source tree, three builds:

| build | what | status |
|---|---|---|
| `build/dinput.dll` | hook DLL: put next to `Dino2.exe`, redirects the original functions to the C ones | runs inside the game |
| `build/harness_stub.exe` + `harness.dll` | checks every C function against the original code | 568 functions, ~1 440 000 checks, 0 differences |
| `build/dc2` (Linux) | the game started like WinMain: task system, root task, per-frame work; headless, prints a trace | boots: CORE.DAT, warning and Capcom screens, save scan, title (menu, opening movie / attract demo in turn); buttons can be given on the command line; New Game runs the opening and the game task into room ST101 (file loads, room set-up, room script start); sets up the player; its first state loads the weapon set and stops at the model set-up (Model_Setup 0x48D8B0, not rewritten yet) |
| `build/smoke` (Linux) | the C game code without any exe code, on an emulated memory image | rooms loaded and set up by the game's own code (File_Process, Room_SetPointers, Script_Init), script VM, animation: 80 of 89 room scripts run 600 frames; 9 stop on cutscene data (SCxx.DAT) that is not loaded yet |

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

## Reimplemented (568)

The authoritative list is `src/hook/hooks.c`. By area:

| area | functions | file |
|---|---|---|
| files | Lzss_Decode | src/game/lzss.c |
| flags | Flag_Set/Check, Bits_Set/Check | src/game/flag.c |
| geometry | PointInQuad, Room_GlobalIndex | src/game/geom.c |
| script VM | GetValue/SetValue/BinOp/SetCond, Run, Start/KillThread; opcodes 00-08, 10, 15-19, 1C, 1D | src/game/script_vm.c |
| script objects | opcodes 12 obj_select, 13 effect_init, 14, 1E obj_set, 1F obj_get (all 9 object kinds), 2A wait_frame, 44, 45 obj_enable, 4A cutscene camera; Obj_SavePos, Obj_SyncLinks, Obj_UpdateMatrix, Obj_Disable, Obj_Free, Camera_Switch, Camera_LookAt | src/game/script_obj.c |
| script built-ins | all remaining opcodes: 11 message, 1A obj_spawn, 1B, 20 wait_event, 21/46 animation, 22-28 (models, timer, fade, wait flag), 29 character switch, 2B-2F, 40-43 (camera move, cutscenes, sound), 47-59 (part links, random, sound slots, enemy_set, texture upload, ...); Obj_Init, Obj_Part, Script_SelObj, Room_SoundSlot | src/game/script_ops.c |
| AOT (trigger areas) | opcodes 30-33, 35-3B define entries of G+0xA5C (doors, items, ...), 3E runs an entry's kind handler | src/game/script_aot.c |
| animation | Anim_Set, Anim_Start, Anim_Step, Anim_ApplyFrame (cubic over four keys, blending into a new animation), Anim_Events, Anim_BlendLinear/BlendMatrix, Matrix_ToAngles; the player's two animation layers (Anim_Save/LoadLayer, Anim_StartLayers, Anim_StepLayers), Anim_FrameFlag | src/game/anim.c |
| objects | generic object handler (type 1: Generic_Handler, its modes), head tracking (Head_Update and modes 1-4, Angle_Approach/ApproachLim), foot anchoring (Obj_SetFeet, Obj_AnchorFoot), Obj_MoveLocal, distances and angles (Dist2_XZ, Camera_Distance, Angles_LookAt, Angle_XZ, Angle_Seg); collision: objects against each other (cylinders) and against the room's wall polygons (sliding along walls, corners), 2D segment tests | src/game/object.c |
| player | Player_Setup, Player_Handler, Player_Breath; states: init, normal (target search with MSVC-order qsort, two-gun arm aiming), hits (light, knocked down), dying, scripted; normal substates: standing, walking, turning, walking backwards, quick turn, climbing steps (ledge and step probes Wall_Ahead/Step_Ahead), pushing; damage scaling, inventory search, ammo; floor heights; effect slots; model set-up (part chains), effect removal by kind; weapon and sub-weapon set-ups; aiming (upper-body pitch toward the target) and the weapon/sub-weapon substates; shots (Weapon_Hit: targets in the weapon's cone nearest first, walls between, damage and reaction), sub-weapons in flight (homing, walls, blasts, noise); firearms: all twelve weapons - state handlers, firing (handgun, shotgun, grenades, spray, machine gun bursts, twin guns, homing missiles, shells), their aiming substates (ready, hold, lower, next target, turn back, walk, walk back, turn; the heavy weapons crouch instead of dodging), ammunition, muzzle flashes; the sub-weapons in use (blades, kick, flare, grenade, one mine or a row of them) and put back.; substate 9 (crouched aiming with the heavy weapons); standing and wading in water. All player states and substates are native | src/game/gun.c, src/game/player.c, src/game/weapon.c, src/game/object.c, src/game/crt.c |
| zones (navigation) | the room's zone graph G+0xA50: nearest zone in sight, shortest paths to the player (Dijkstra, 0x87FCE0), each object's goal point, sight lines against the wall polygons | src/game/zone.c |
| props | room prop type (a model playing its animation): handler, init, play modes; room change: Eff_KillRoom, Room_SoundInit | src/game/object.c, src/game/game.c |
| files | File_RequestLoad/Raw, File_Tick, File_Process and the record handlers (copy, VRAM image, CLUT, sound bank, stream, LZSS, texture tiles, PC buffer), Rand8 | src/game/file.c; host files and heap: src/game/pcio.c |
| loading and room set-up | Enemy_LoadModel, Stage_Load, File_IsBusy; Room_SetPointers, Room_AllocObjects, Room_Reset, CamZones_Init, Aot_Reset; Script_Init, Script_Tick and its skip/event modes | src/game/room.c |
| root task and tasks | RootTask and its boot modes (boot, logo, warning, to title), Screen_LoadImage, Save_ScanTask; Task_Kill; host coroutines for the task system in standalone builds | src/game/root.c, src/game/task.c |
| title | TitleTask, menu (new game / load / options), difficulty choice, opening movie and attract demo, animated labels and their sprites | src/game/title.c |
| game task | Game_Start, opening task, new-game state, game task modes init / area tables / room enter / play / game over ("Will you continue?"), room / door / armour / music loads, checkpoint copy, room sound tables | src/game/game.c |
| frame of play | Game_Frame (0x472FD0): the trigger areas (Aot_Tick, AOT_Check, AOT_Door), the object, prop and effect loops; the camera zones and loading a camera's view (Camera_Load); sound each frame and the voice (Snd_Tick, Voice_Play/Stop); the enemy sets that bring enemies in (EnemySet_Tick, EnemySet_Small) | src/game/game.c, src/game/room.c, src/game/camera.c, src/game/sound.c, src/game/enemyset.c |
| doors (room change) | Door_Task and its modes (set-up: the stand-in, door models, camera, light, room name; run: area switch, room and music loads; end), all door types' handlers: hinged, shutter, sliding and double doors, lifts, stairs, camera drop, waits, message, walk; the score panel after a fight | src/game/door.c |
| PC layer: GTE | all 80 libgte functions of the port (0x4C08E0..0x4C24C0): rsin/rcos/ratan2, roots, geometry state, Apply/Mul/Rot/Scale/Transpose matrix, RotTransPers 1/3/4 and the float projection, depth cue, normal colour, register loads/stores, rtps/rtpt/nclip/ncct, PC depth stores | src/game/gte.c, include/dc2/gte.h |

The game runs as cooperative tasks (4 slots, `Task_Sleep(n)` gives up the
CPU for n frames). In-process the original scheduler switches stacks; the
standalone builds run each task as a host coroutine (ucontext) and start
the C version of the task function found by its address in the hook table
(`Hook_Find`, also used for the game's function tables). Outside a task
(test drivers) `Task_Sleep` only runs the file loader.

The standalone builds read files from the host (`g_pc_root`, names matched
without case) and allocate from a heap inside the emulated image, so data
the game keeps 32-bit pointers to stays addressable (`include/dc2/pcio.h`).

Functions still called in the original (from C) are listed once in
`include/dc2/orig.h` (`ORIG_LIST`): in the DLL and the harness they run the
original code, in the standalone build they are reported once and skipped.
Calls through the game's function tables (object type handlers, AOT kind
handlers) go through `Orig_Call1`.

In the harness, the tests of the built-ins replace each of those callees by
a recorder (a JMP at its address) that logs the arguments into compared
memory, so a wrong argument shows up as a memory difference; pointer
arguments to temporaries are logged by content.

### Floating point

The PC port projects to float screen coordinates on the x87. `gte.c` repeats
the original operation order in `long double` and rounds to `float` where
the original stores a float; with `-fexcess-precision=standard` on the
32-bit build the results are bit-identical (same process, same FPU control
word in the harness and in the DLL). Other compilers/CPUs (the Dreamcast)
get the same formulas with their own rounding: screen positions may differ
in the last bit, which only matters for drawing.

The original leaves stack garbage in a few padding words (MATRIX +0x12
after MulMatrix0/RotMatrix*, OuterProduct12 out +12); the C code leaves them
alone and the harness ignores them.

## Next

The script VM, the keyframe animation, the generic object handler (type 1), object collision, the zone navigation and the player (all states and substates, all weapons and sub-weapons) are complete; the standalone build plays the opening of room 101 and, with buttons held from the command line,
walks the player through its door into room 102 (`dc2 Dino2.exe ROOT/Data 3300 400:820 440:820
2900-2932:2000 2940-2955:1000 2960-3010:20`; ROOT also holds `Speech`). The DirectSound buffers have a
silent stand-in there that only keeps time. Next: the enemies' type handlers (Enemy_LoadModel 0x482070), the object
type handlers (*0x87DCA0), sound/BGM (0x48C9C0..0x48CE60), messages
(0x4718F0), fades and menus. Then the libgpu side
of the PC layer (primitives, DrawOTag, texture cache) — rewritten for
SDL/GL rather than copied, since it talks to Direct3D. Then the file loader (File_Process 0x404290 and record
handlers), rooms and cameras, objects and their per-type handlers; the
PC layer (0x4B4000.., PsyQ on DirectX) is rewritten for SDL/GL rather than
decompiled.
