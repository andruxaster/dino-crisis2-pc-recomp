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
| `build/harness_stub.exe` + `harness.dll` | checks every C function against the original code | 858 functions + the PC layer (GPU memory side, drawing, JPEG), ~2 169 000 checks, 0 differences |
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
    make linux      # native static library + build/smoke + build/dc2
    make dc2gl      # build/dc2gl: dc2 in a window (SDL2 + OpenGL)

`dc2gl Dino2.exe ROOT/Data [FRAMES] [presses]` takes the pad from the
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

## Reimplemented (2136)

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
| area effect kinds | the effect slot kinds of some areas (*0x87F948): the shot at the player (7: bursts on a wall or off its floor, knocks the player or the one it keeps alive down), the player's shot (8: Weapon_Hit, the other player in the duel), the growing glow (12); area 1's swinging thing on a rope (20); area 4's drifting puff (16) and flare shot (17: lights up where it hits or lands; its glow kept on a light's axis); area 8's burst (21: two crossed copies, the looks table it rewrites), splash (19), drops bouncing off the floor (17, also area 9), the thing carried on an effect that throws up pieces (20); area 0's thing thrown into the water or shot (16); the bursts of areas 6 and 7 (16) | src/game/k2area.c |
| files | File_RequestLoad/Raw, the archive entries' loads (item icon, map page, picture), File_Tick, File_Process and the record handlers (copy, VRAM image, CLUT, sound bank, stream, LZSS, texture tiles, PC buffer), Rand8 | src/game/file.c; host files and heap: src/game/pcio.c |
| loading and room set-up | Enemy_LoadModel, Stage_Load, File_IsBusy; Room_SetPointers, Room_AllocObjects, Room_Reset, CamZones_Init, Aot_Reset; Script_Init, Script_Tick and its skip/event modes | src/game/room.c |
| root task and tasks | RootTask and its boot modes (boot, logo, warning, to title), Screen_LoadImage, Save_ScanTask; Task_Kill; host coroutines for the task system in standalone builds | src/game/root.c, src/game/task.c |
| title | TitleTask, menu (new game / load / options), difficulty choice, opening movie and attract demo, animated labels and their sprites | src/game/title.c |
| game task | Game_Start, opening task, new-game state, game task modes init / area tables / room enter / play / game over ("Will you continue?"), room / door / armour / music loads, checkpoint copy, room sound tables | src/game/game.c |
| frame of play | Game_Frame (0x472FD0): the trigger areas (Aot_Tick, AOT_Check, all their kinds: door, event, message, flag, item, effects, menus), the object, prop and effect loops; the camera zones and loading a camera's view (Camera_Load); sound each frame and the voice (Snd_Tick, Voice_Play/Stop), all the sound slots at once (clear, duck and back, fade out, pause and resume); the enemy sets that bring enemies in (EnemySet_Tick, EnemySet_Small) | src/game/game.c, src/game/room.c, src/game/camera.c, src/game/sound.c, src/game/enemyset.c |
| enemies (shared) | attack spheres against the player (Enemy_AttackHit, Sphere_Hit), kill and event scores with the combo, bleeding, hit directions, death fade, flashes and props they leave, the game over start | src/game/enemy.c |
| object effects | turning, blinking (polygon shade), thrown and debris pieces, the item in the player's hand | src/game/effect.c |
| effect slots | the slots G+0x944: motion steps, sprite animation, hidden test; sprites (init, move, fade), splashes and floating bits, both emitter kinds and their record lists; falling drops, bubbles, a sprite stuck to an effect, puffs, a glow, lone sprites, screen sparkles and bursts; the effects that fill slots from a record list or with the area's own type | src/game/k2.c |
| frame helpers | camera reset and zone check, background masks, the damage gauge block, screen shake, fades (set, tick, draw), effect 0's trail, hidden flags of objects and effects, lights picked per model part (camera sets, nearest lights, room light) and marked lights run, pads read each frame; message slots and the message box (typing, pages, prompts, yes/no, choices); text drawing and centring; the HUD: the health gauge, the boss's gauge, the event timer, the weapon change boxes, the ammo pop-up and its numbers; the starting stock by difficulty and by game mode; menus started from the game, carried items merged and closed up; sound effects (table, pan and volume by distance, sound areas, slots, groups) and stream slots; movies started and their steps (cleared, faded in, skipped by a button, faded out; standalone: each ends at once) | src/game/misc.c |
| in-game menu | the menu task (Menu_Task 0x491950: keys with repeat, the modes, opening flash and torn lines), the pause screen (backdrop, header with its sweeps and scrolling hint, health bar, points, weapon and sub-weapon panels with their ammunition, entries and their running border, frame), leaving the menu (closing line, back to the game task); screen 1, weapon and sub-weapon choice (the 3D ring turned on the GTE, equipping with the stage's default sub-weapon, the weapon figures); screen 2, items and files (medicines used, the list closing up, files read or used at their zone, the PC's ASCII letters); screen 3, memos (the list, the memo typed out page by page with its scroll bar, the typed-text codes: colors, digits, picture, page breaks); screen 4, the map (each area's page with its rooms seen, doors open, shut or with their key, save rooms, the player's arrow and the cursor gliding to his room, other areas' maps by L/R, the player's room zoomed and scrolled, the torn screen where there is no map); mode 10, an item picked up ("You got the ...", a medicine taken if there is room, a memo read at once, the ammunition box filling the weapons); mode 11, "Set the destination" (the panel of the three target points with their camera views, the route traced with its marker, figures and cross-hairs, arrival); its primitives (tiles, sprites, frames of sprites, lines, triangles, quads, drawing areas), numbers, small letters, yes/no; flags 9 counted and listed, item records searched | src/game/menu.c |
| frame drawing | Frame_Draw (0x4A7250): the view matrices of objects, parts, effects and props; models (vertices projected and lit per part, then indexed triangles and quads with depth), effect meshes and their depth rules, shadows on the floor, prop sprites and quads, background mask markers, dim bars, the rope, the text layer (message slots, the sentence and its cursor), the per-frame handlers | src/game/draw.c |
| velociraptor | handler (damage, movement, collisions), init by level, the choice of the next state, normal sub-states (wait, walk, cry, back off, eat, entrance, feint), jumps and climbs, the attacks (pounce, lunge, side bite, leap, tail) and their hits on the player, the hits it takes (flinch, knock-back, blown back, thrown in the air, burnt, drowned), death | src/game/raptor.c |
| oviraptor | handler, init by level, the choice of the next state, normal sub-states (walk and turn about, look about, hide, turn in place, entrance), climbs and jumps, the attacks (leap back, peck, pounce and the bites while holding the player down, lunge, kick, jump on the back) and their hits on the player, the hits it takes (staggers, knocked back, blown about, thrown in the air), death | src/game/oviraptor.c |
| allosaurus | handler, init, deciding (standing, walking, running, steering by walls and ledges), the moves (walk, run, leap down or up a level, turn about, step back, roar, anger, bite and shake the player, lunge, snaps, pick the player up and toss, sidestep, dash), the attacks landing, the damage it takes (anger, stagger, flinch, fall), death, the anger tint and burning | src/game/allosaurus.c |
| pteranodon | handler (the player shooting keeps them off, height limits, flight height), init, deciding (circling and steering, the swoop at the player or where the running player will be, perched), the moves (fly, rise, swoop, perch, peck, claw, carry the player off and drop it, pin it down), the attacks landing, the player's states while falling and getting up, the damage it takes (flinch, knocked about), falling dead (cliffs, water); the room's water level | src/game/pteranodon.c, src/game/object.c |
| tyrannosaurus | handler (the gate kind keeps half its health and opens the gates' flags when its attacks reach them, the scripted kind's steps), init, deciding (stand, walk, the attacks by distance, angle and level, wandering to points, charging a gate), the moves (stand, walk, roar, look about, snatch the player up and shake it, recoil, the scripted break through the wall, stagger, bite, bite while walking, charge, lunge), the attacks landing (the charge catches the player in its jaws), the player's states in its jaws, the damage it takes (by kind and weapon, flinch, reel, stagger), going down and getting up, falling dead; attack spheres against a quad, a random zone point, turning from the head | src/game/trex.c |
| plesiosaurus | handler (its parts darken as the player comes near, kept 5000 above the floor, walls only when close), init, deciding (the attacks by the player's height, angle and distance, turning about when the player is far), the moves (swim, turn about, bite at the player on land, lunge from close by, bite to the side, lunge from below), swimming (circling the room's middle with its head toward the player, coming up close and going back out by the room's events, diving away), the attacks landing, the damage it takes (flinch, still swimming), sinking dead | src/game/plesio.c |
| plesiosaurs in the water (types 11/12, E31/E32) | handlers (parts above the water hidden from its effects, darker below, two glows following the head, type 12 sinking unless it swims and giving off bubbles), init (health kept from the one taken away; a new one too near another is taken away at once), deciding (bites by distance, angle and the player's turn, leaping aside when the player turns its back, diving away when another is near or the player far), the moves (wait at the surface, swim closer, back away, leap aside, dive, rise, lunge, bite, the bite to the side, drag the player under, eat the player, dive and rise again, snap), the attacks landing, the damage (flinch, knock-back, stagger or stun), sinking dead; the extra game's kill combo | src/game/plesio2.c |
| the roamer (area 3 object type 22, rooms 303-305) | handler (walls turn it along them), init, start (gone unless in its room; at a random point), the exits opened and shut by event flags, walking off away from the player, turning toward the next place at a turning point, waiting at a point, running along the shortest path point by point, leaving through an exit into the next room or for good, bumping the player over, running off when hit; the shortest paths over its graph, the nearest point in sight; the shared state-3 handler that takes an object away | src/game/roamer.c |
| area objects (types 17..28 of the area tables) | the falling piece (type 17: shakes, then falls along its facing with a quarter turn on the way), the flying shell (18: flies at a point, spinning; a flash where it lands), the moving piece (19: the falling piece's way, or flying along two points, pitched at them), the bird (21: bobs where it sits until the player fires or runs, then flies off by two points), the poison plant (22 of area 1: rises and snaps at the player in its quad, the grown kind a poison cloud; killed for good it wilts; its own scaled matrices), the rock that breaks (23: only by two weapons), the burner (26 of area 4: lit by the room, flames that burn what is ahead, shown once by the camera), the insect (27 of every area: flies about the player bobbing, lands on it, leaves, falls apart when hit); the light step, the limited turn; the object types of the weapon and sub-weapon in hand (29/30, 28/29 in area 6); the pteranodon of area 0 (24: circles drifting along +z, swoops along an arc about the player, perches and dives at it, a peck or the claws; knocked about, falls dead off the cliff or into water); the watcher of area 8 (20: keeps to its place and turns after the player, goes home or walks off by its parameter); the two of area 4 the player must keep alive (25: follows the player between two points, cowers when others are about, climbs down ledges, waits when blocked; 28: keeps to its place; their shared health G+0x928, game over at 0) | src/game/aobj.c, src/game/ptera2.c, src/game/aobj2.c |
| giganotosaurus | handler, init by kind (the bridge one of room 503, the extra mode), the choice of the next state, walking, standing, running, the turn, the roar that stuns the player, the threat, the attacks (two bites, tail swipe, lunge, charge) chained into combos, snatching and mauling the player, the bridge scene (it walks the bridge, the sections break under its attacks, the player falls with them), the hits on the player, the hits it takes (flinch, stagger, knocked down), death | src/game/gigano.c |
| inostrancevia (type 14, E50) | handler (the fall off a ledge, removed when far and out of sight), init (health by difficulty), deciding (roar, bites, seizing or pouncing on the player by distance, angle and the player running; turns), the moves (wait, walk, roar, turn, fall off a ledge, two bites, seize and eat the player who struggles free with the pad, pounce on the player), the attacks landing, the damage (scaled by weapon; flinch, stagger, knocked over, down and hit while down), dying (falling, from lying, burnt) | src/game/inostr.c |
| triceratops (type 9, E70) | handler (pushed down the street, the lead one tinted by the player's distance, dust), init (two kinds: the lead one and the herd), the phase by its health and the herd's and its values, steering across the street (at the player, weaving, to the side, along the wall knocking debris off, to the far side, back), deciding (roar, charge, gore), the moves (run, run on, roar, swerve, charge, gore), the attacks landing, the damage (strength by the attack's frames, the herd's last phases), flinch, knocked down, dying (falls, runs off the street's end) | src/game/tricera.c |
| mosasaurus (type 10, E80) | handler (under the surface, above the bottom, its pitch limited), init, deciding (its own point away from the player, turning and pitching to the target; the lunge, the leap, the seize), swim, rush, turn, pitch, dive, the lunge and its bite, the leap that drags the player in and the player thrown (callback), the seize that swims off with the player and lets it fall (callback), the damage, flinches, dives, knocked over, dying (sinks, fades) | src/game/mosa.c |
| compsognathus (type 13, EA0) | handler, init (health 1000), the PC game's few moves: turning to its target, clinging at the player's height, hurt (lets go of the player), fading out dead; the two screen flashes in its table (whole, in quarters) | src/game/compso.c |
| the extra game's playable dinosaurs (area 9 types 29..42, KOF_P*.DAT; two copies of each kind's code, one per player of the duel) | shared: a dinosaur's attack spheres this frame against the computer's dinosaurs or the other player (Kof_Attack, Kof_HitFoe), the button sequence of the super move and its start (camera, shout). The inostrancevia (types 41/42; one C version for both copies, their tables, variables, pads and model as parameters): handler (the other player as target, head on it), init (health 1200), the super move (health coming back), the pad's choice of move by state, stand, walk, turn to the other player, walk on, walk back, roar, turn about, two bites and the lunge (their landing: flash, effect), hurt (more while its own attack is out), the flinches, falling dead (the other player wins). The triceratops (types 37/38, the same way): handler (damage taken lately: over 0xA0 it staggers), init (health 2200), the super move (rears up, gores at full speed), the pad's choice of move by state, stand, walk, turn to the other player, walk on, charge, walk back, roar, gore, the side swipe (their landing: flash, effect), hurt, flinch, stagger, knocked back, falling dead. The raptor (types 29/30): handler, init (health 1200), the super move (bite, three claws, slash, leap), stand, walk, run, step back, turn, spin round, taunt, turn to the other player, the claw, the leaping claw, the kicks, the bite (each chaining into the next attack late in its swing), the light hit, knocked back, falling dead. The oviraptor (types 31/32), built like the raptor: health 1000, the four-strike claw, the leaping claw (leaps again on a press), the kick, the head butt, the hop back, the spin round to face the other way, the super move (the claw twelve times). The allosaurus (types 33/34), built like the inostrancevia: health 1500, walk on, run, walk back (faster in the rage), face the other player on guard and side step (away from the nearer wall), the bite, the charge, the tail, the sweep (cut short in the rage), the rage as its super move (armour, a red glow, 900 frames), hurt (thrown by the side when hit close, knocked back while attacking), falling dead. The tyrannosaurus (types 35/36): health 2000, footfalls that shake the ground, the run that hurts what it runs into (not again for a while), the bite, the head swing, the tail, a roar and a long roar, the super move (a roar that darkens the screen), hurt (it shrugs hits off; the damage taken lately makes it flinch, more of it the longer flinch), the stagger, falling dead. The compsognathus (types 39/40): health 1 (every hit flinches it, one that takes it below 0 kills it), walk on, run, walk back, the spin (half a turn), the leap (its attack), the call (three of them ready the super move), the super move (a pack of compsognathi for 150 frames: each comes near the other player, runs at it, bites it once and is gone), falling dead | src/game/kof.c, src/game/kof_ino.c, src/game/kof_tri.c, src/game/kof_rap.c, src/game/kof_ovi.c, src/game/kof_allo.c, src/game/kof_rex.c, src/game/kof_comp.c |
| the extra game's human (area 9 type 23) | a player with a gun and a machete in the duel, built like the dinosaurs: handler (the next target, the head on it), init (health 2000), the pad's choice of move by state, stand, walk, turn to the foe, run, dash (none again for a while), step back, taunt, the machete combo, the gun's three attacks and their sparks, the super move (by its button sequence, at half health), hurt (the damage taken lately knocks it down or away), knocked down and away (footfalls), falling dead | src/game/kof_human.c |
| the vehicle's gun (area 9 type 43, PS1 overlay WEP0C) | handler (it shakes), init (health and size by the mount, six missiles), turning (the sweep that hits what is ahead on the heavier mount), lifting the gun, the gun, the homing missiles (the first target's first sphere), the sight's two bars (copied in VRAM), hit (thrown by the hit, shaking), dying (game over), the engine sound | src/game/vgun.c |
| doors (room change) | Door_Task and its modes (set-up: the stand-in, door models, camera, light, room name; run: area switch, room and music loads; end), all door types' handlers: hinged, shutter, sliding and double doors, lifts, stairs, camera drop, waits, message, walk; the score panel after a fight | src/game/door.c |
| PC layer: GTE | all 80 libgte functions of the port (0x4C08E0..0x4C24C0): rsin/rcos/ratan2, roots, geometry state, Apply/Mul/Rot/Scale/Transpose matrix, RotTransPers 1/3/4 and the float projection, depth cue, normal colour, register loads/stores, rtps/rtpt/nclip/ncct, PC depth stores | src/game/gte.c, include/dc2/gte.h |
| PC layer: GPU, memory side | primitives linked into the ordering tables, sprite/tile/mode/area packets, texture page words, the software VRAM (1024x512 at 0x76448C: LoadImage, StoreImage, the CLUT rows load, with the texture cache invalidation and palette conversion of the drawing layer). Tested in the harness, called by the standalone builds (the DLL keeps the originals) | src/pc/gpu.c, include/dc2/gpu.h |
| PC layer: JPEG | the rooms' backgrounds (0x4D2990): a baseline decoder that gives libjpeg 6b's pixels as the port set it up (float IDCT, fancy upsampling, 15-bit output with blue high); checked on every camera picture of the DBS files, also cut short and damaged | src/pc/jpeg.c |
| PC layer: drawing | DrawOTag (0x4B50F0) and all it draws with: the packet drawers (polygons flat/gouraud/textured, lines, tiles, sprites, the font's text quads, the frame copy, background mask groups), vertex colour, fog, blending, depth and shading states, the texture cache (VRAM pages and palettes converted into 256x256 textures, found again by palette and window, reloaded when VRAM or a palette changes, released or marked when VRAM is written), the glyph cache, the pixel converters, VRAM copies, the frame grab. Copied call for call onto the Direct3D 6 calls of `include/dc2/gpudev.h` (COM in-process, a renderer standalone); the harness runs the original and the copy against a fake Direct3D that logs every call. The frame around it: the background image (0x4BDC90 and its upload), present and clear, fog colour and switch, the viewport's background colour, PutDispEnv/PutDrawEnv, the font buffer. Not yet: code 0x84 (a sprite composed from parts; not seen in the game's frames so far), the background masks module (0x4BE3C0..) | src/pc/drawot.c, src/pc/gpudev.c, include/dc2/pcdraw.h, include/dc2/gpudev.h |
| PC layer: renderers | standalone only, not compared with the original: `src/pc/gpusoft.c` stands in for Direct3D 6 / DirectDraw 4 (surfaces as pixel buffers, render and texture-stage states, Blt/Lock/Flip, the PC layer's globals as its initialisation leaves them, fog per vertex) and hands draws to a `Renderer` (`include/dc2/render.h`): OpenGL 1.4 + SDL2 on the PC (`src/port/render_gl.c`), the PowerVR2 through KOS on the Dreamcast (`src/port/dc/render_pvr.c`: one translucent list in submission order, depth on 1/w, twiddled ARGB1555 textures; no alpha test, no read-back yet). `dc2` runs the main loop's graphics part (0x47053E) and its start (0x470980) | src/pc/gpusoft.c, src/port/render_gl.c, src/port/dc/render_pvr.c, src/port/dc2.c |
| PC layer: sound | standalone only: the DirectSound buffers (0x4BF720..: WAVE PCM copied, volume v/127*30 dB - 30 dB, pan, replay rate, looping, pause) and the MP3 streams (0x4BFC10..: decoded whole at open, looping) mixed to stereo 44.1 kHz; `dc2gl` queues each frame's samples to SDL | src/pc/audio.c, third_party/minimp3 |

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

The JPEG decoder is the exception that must be exact everywhere: its float
IDCT runs with the x87 at single precision in the game (Direct3D sets it),
so `jpeg.c` uses plain `float` arithmetic, float constants as objects and a
quantization table rounded in integers. That gives the same bits with SSE,
on the SH-4 and on the x87 at single precision (the harness sets it for
that test).

The original leaves stack garbage in a few padding words (MATRIX +0x12
after MulMatrix0/RotMatrix*, OuterProduct12 out +12); the C code leaves them
alone and the harness ignores them.

## Next

The script VM, the keyframe animation, the generic object handler (type 1), object collision, the zone navigation, the player (all states and substates, all weapons and sub-weapons), the velociraptor, the oviraptor, the allosaurus, the pteranodon, the tyrannosaurus, the plesiosaurs, the giganotosaurus, the inostrancevia, the triceratops, the mosasaurus, the compsognathus and the duel's inostrancevia, triceratops, raptor, oviraptor, allosaurus, tyrannosaurus, compsognathus and human, and the vehicle's gun are complete; the standalone build plays the opening of room 101 and, with buttons held from the command line,
walks the player through its door into room 102 (`dc2 Dino2.exe ROOT/Data 3450 400:820 440:820
2900-2932:2000 2940-2955:1000 2960-3010:20`; ROOT also holds `Speech`). The DirectSound buffers and the music streams
are mixed in `src/pc/audio.c` (the streams are MP3, decoded with minimp3).
Next: the open ends: the oviraptor's walk, Weapon_Hit's untested branches. The PC layer's drawing is copied onto
Direct3D-shaped calls and drawn by a renderer (OpenGL, the Dreamcast's PVR);
still missing there: the background masks module (0x4BE3C0..), code 0x84,
the PVR's read-back. In the standalone build each movie "plays" for no time (none are
decoded), the menu works blind. Then the file loader (File_Process 0x404290 and record
handlers), rooms and cameras, objects and their per-type handlers; the
PC layer (0x4B4000.., PsyQ on DirectX) is rewritten for SDL/GL rather than
decompiled.
