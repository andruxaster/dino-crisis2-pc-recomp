/*
 * dc2_game_flow.h -- top-level flow, file loading and room tables of
 * Dino Crisis 2 PC (Dino2.exe, 2002-02-05). Documentation of what the
 * binary does; names are ours. Companion data: dc2_file_ids.csv, dc2_rooms.csv.
 *
 * ---- WinMain 0x470190 (main loop is inline) ---------------------------------
 *  init: window/DirectX, 0x48C080 (game init), Task_InitStackBase,
 *        Task_Start(0, 0x471EC0 if g_flag_87DE08 else RootTask 0x46D710)
 *  per frame:
 *        frame pacing (GetTickCount accumulator 0x74D740)
 *        File_Tick 0x403E40          -> 0x404280 idle / File_Process 0x404290
 *        DrawOTag 0x4B50F0 (OT 0x87F94C), debug text 0x4BD3C0
 *        PC_VSync 0x470BF0           (message pump + timer wait)
 *        Task_RunAll 0x4C2900        (4 cooperative tasks)
 *        0x450AF0 (dispatch via table 0x7224A4), 0x471AC0
 *
 * ---- RootTask 0x46D710 --------------------------------------------------------
 *  for (;;) { g_root_modes[g_root.mode](&g_root); Task_Sleep(1); }
 *  g_root @ 0x74D608 { u8 mode, u8 sub, u8 sub2, ..., s16 timer @+4 }
 *  g_root_modes @ 0x723AC8: 0 Boot 0x46D740 (loads CORE.DAT),
 *                           1 0x46D7C0, 2 0x46D880, 3 0x46D970
 *  This {mode, sub, ...} + function table pattern repeats everywhere
 *  (154 pointer tables in .data).
 *
 * ---- file loading -----------------------------------------------------------
 *  File_RequestLoad(id) 0x403E50: one request at a time (returns 0 if busy);
 *    sets g_file_id 0x87FFE8, flag bit0 in 0x87FFEE, busy 0x87FFEA = 1.
 *  File_IsBusy() 0x403A20 -> 0x87FFEA. Tasks spin with Task_Sleep(1).
 *  File_Process 0x404290 (runs inside File_Tick, synchronous):
 *    name = g_file_names[id] (0x71B230, 676 entries), path "data\\%s",
 *    PCopen, header 0x800 bytes -> .psxseg 0x6E6D00, whole file -> malloc,
 *    then for each 32-byte record (header pointer 0x87FFE0):
 *      type 0 0x4044C0 copy to dest (abs. address in .psxseg)
 *      type 1 0x4044F0 VRAM image -> LoadImage 0x4B4D20 (via buffer 0x746CA0)
 *      type 2 0x404680 CLUT -> LoadImage_IfChanged 0x4B4DB0
 *      type 3 0x4046C0 sound-effect bank -> slot 0x87FBE0[arg]: 32 x 8-byte
 *             descriptors, then 32 x {offset, size} RIFF WAVE PCM16 mono 22050 Hz
 *      type 4 0x404770 streamed sound -> slot 0x87FB84 (Strm_Open): MPEG-1
 *             layer III, 44.1 kHz (the room's music; rec +0xC 1: looping)
 *      type 5 0x4047F0 / type 6 0x404810 -> 0x4041F0 (unpack, scratch 0x5A0000)
 *      type 7 0x4048F0 buffer 0x87F944 -> PC_SetBuffer
 *    flags 0x87FFEE bits 1/2/3 seek inside archives by index*5 / *33 / *7 sectors.
 *
 * ---- game work struct G = *(u8 **)0x87DDF8 (partial) --------------------------
 *  +0x67A u16 loaded CORE file id    +0x67C u16 area (set by area fn)
 *  +0x67E u16 loaded room file id    +0x804 fade/screen-effect block
 *  +0x108F u8 stage (CORE00..12 = file 0x16 + stage)
 *  +0x14EC options (16 bytes, defaults 0x723CCC set by 0x470907): u8 stereo,
 *    s8 music volume (0..15), s8 sound volume, +0x14EF ...
 *  +0x1090 u8 room                   +0x1091 u8 area
 *
 * ---- rooms ------------------------------------------------------------------
 *  g_areas @ 0x73825C: 10 x 24 bytes { ROOM *rooms; void (*set_tables)();
 *    u32 0; u16 ...; void *p }; set_tables 0x48D170 + 0x40*area sets G+0x67C
 *    and per-area table pointers 0x87DCA0 / 0x87F948 / 0x87DC8C / 0x87F320.
 *    0x87F948: effect slot kinds (G+0x944, 0xAC each); kinds past the common
 *    ones differ by area (src/game/k2area.c: 7 a shot at the player or the one
 *    kept alive, 8 the player's shot, 12 a growing glow; area 1: 20 a swinging
 *    thing; area 4: 16 a puff, 17 a flare shot / its glow by +0x7A; area 8: 17
 *    drops, 19 a splash, 21 a burst whose looks 0x721A68 (12 bytes each) step 3
 *    rewrites, 20 a thing on the effect +0xA0 throwing pieces (looks 0x721EE8);
 *    area 9: 17 drops; area 0: 16 by +0x7A (looks 0x721B08, 8 bytes); areas 6
 *    and 7: 16 bursts (looks 0x721B68, 0x721BE8)).
 *  ROOM (24 bytes): u16 f0 (=1), u16 dat_file_id (STxyy.DAT), u16 dbs_file_id
 *    (STxyy.DBS), u16 f6, u8 s8_15[8], void *p10, void *p14.
 *  88 rooms in 10 areas (6,17,6,7,12,5,9,8,12,6); ST906 is not in the table.
 *  Room load task 0x41EBB0: request rooms[room].dat_file_id, wait, G+0x67E = id.
 *  Stage load 0x4822D0: CORE00+stage if changed, then stage weapon file 0x1FA..0x205.
 *
 * ---- other file-id tables ------------------------------------------------------
 *  0x71FEB0 u16[2 per door type]  DOOR*.DAT (up/down variants)
 *  0x703520 stride 8              enemy type -> E*.DAT (2: E00, 3: E10 ...)
 *  0x703598 stride 8              KOF_P*.DAT playable-dinosaur models (not Regina/Dylan)
 *  0x703EE8 stride 8              OPTION/SAVE/LOAD/SUBSCR/MAP_ST*
 *  0x723B34 stride 20             extra-mode models M_*.TEX
 *  0x738774 stride 4              ARMOR00/10.DAT
 *
 * ---- room files (verified on ST000) -------------------------------------------
 *  STxyy.DAT (one container): type 3 sound bank (slot 4), type 0 table -> 0x6EEF80,
 *    type 6 texture pages (LZSS, 4bpp, 32x32-halfword tiles), type 2 CLUTs
 *    (VRAM rows 505..511), type 5 room block (LZSS) -> 0x5E0000.
 *  Room block @ 0x5E0000: header of 12 absolute pointers, applied by
 *    Room_SetPointers 0x481E10 (see dc2room.py): +0x24 cameras (16-byte list
 *    header with count at +4, then 16-byte CAMERA), +0x28 DBS table
 *    {u32 sector, u32 size} per camera, +0x0C camera-switch data (G+0xA44).
 *  CAMERA: s16 eye[3]; u16 h; s16 target[3]; s16 roll. Camera_Load builds
 *    the view with SetGeomScreen(h), LookAt 0x48F620, RotMatrixZXY.
 *  STxyy.DBS = one container per camera, at sector*0x800 from the DBS table:
 *    type 0 masks -> 0x6B9500: u16 count, u16 0, count x {u8 x/16,
 *      u8 y/16 (low nibble), s16 z = OT slot 0..1023}; 16x16 screen tiles of
 *      the background (Masks_Setup 0x4036F0, Masks_Draw 0x403760 puts one
 *      0x88 marker per mask group into ot[z]; OT 0x87BC00 = 2 x 1024 entries),
 *    type 0 JPEG 320x240 -> decoded by JPEG_Decode 0x4D2990 (static IJG
 *      libjpeg, ~275 functions in 0x4CD000..0x4DD5C0) into 16-bit RGB at
 *      0x6C1500..0x6E6D00; G+0x8 points to it, G+0xC26 = re-upload flag,
 *    type 2 CLUT (row 496),
 *    record 3: type 8 = LZSS 8bpp PS1-style background 256x448 to VRAM x=320
 *      (rooms without masks, e.g. ST000), or type 0 with arg = unpacked size
 *      = LZSS mask pixels, 648 bytes per mask = 18x18 BGR555 (16x16 + 1-pixel
 *      border for bilinear filtering), kept at CAM+0x54 (rooms with masks).
 *  PC_DrawBackground 0x4BDC90 (called from WinMain when Flag_Check(3,0)):
 *    320 = 256 + 64 wide textures, screen-space quad, before DrawOTag.
 *  Camera struct CAM = G+0x778: +0x20 angles, +0x28 target, +0x38 eye,
 *    +0x40 camera list, +0x4C current index, +0x48 mask data, +0x50 mask count.
 *
 * ---- collision, floor levels, camera zones (verified on all 88 rooms) ---------
 *  Level of an object = trunc(-y / 1000), kept at obj+0x5A. No floor heights
 *  are stored: a level-k floor lies at y = -1000*k (camera zones and footstep
 *  zones carry the level; 99% of cameras sit above it, median eye 2.7 m).
 *  G+0xA4C collision: u8 count, u32 0 @+4, u32 offset[count] @+8, POLY:
 *    u8 flag (0 off, 0x40 walkable area, else obstacle), u8 n, u8 index,
 *    u8 ignore_kinds, u16 level mask, u16 0, u32 edge[n], s16 {x,z}[n+1].
 *    edge byte0: 0 no wall, bit1 passable, bit7 passable on request;
 *    byte1: kinds that pass. Kinds: 0x80 player, 0x40 others, 0x20 obj+0x50&0x1000.
 *    Col_SegmentBlocked 0x486FB0 tests a move from->to against all edges
 *    (Col_EdgeIntersect 0x487160).
 *  G+0xA44 camera zones: u32 count, 0x2C-byte {u8 flags (0x80 off), 3, u8 camera,
 *    1, u8 level, u8 extra levels, 4, s16 quad[4][2], s16 quad2[4][2]};
 *    CamZones_Check 0x4035F0: level in [level, level+extra] and PointInQuad.
 *  G+0xA64 footstep zones: u32 count, 20-byte {u8, u8 sound, s8 level,
 *    u8 material, s16 quad[4][2]}; Footstep_Sound 0x48C3B0 (event codes
 *    0x30/0x31/0x50/0x51 + 2*material; material 3 = silent).
 *  G+0xA3C lighting: bytes 0..2 ambient (Room_SetLighting 0x46B8C0).
 *  G+0xA50 waypoints (Waypoint_Nearest 0x486D00), G+0xA58 63 x 28-byte
 *  placeholder entries.
 *
 * ---- room script G+0xA54 (see dc2script.py for the full encoding) -------------
 *  Script_Init 0x48BB60, Script_Run 0x48BC50: stack VM, 10 threads x 0xA4,
 *  opcode table 0x738C40. 01/02/03 register moves and arithmetic, 05/06
 *  push/pop, 07 conditional branch, 08 call, 04 return; all other opcodes
 *  are built-ins taking their arguments from the stack in push order
 *  (popped afterwards by 03 00 FF 00 n 00), results in r0.
 *  Built-ins 0x30..0x3B fill the AOT table G+0xA5C (32 x 40 bytes) checked by
 *  AOT_Check 0x48B040. Door (0x30): aot, flags, level, quad[8], dest x, y, z,
 *  dir, room (stage<<8|room), door anim, lock message, key flag. 171 of 175
 *  constant door destinations land in a walkable polygon of the target room
 *  at level -y/1000. Other built-ins: flags 0x1C/0x1D, threads 0x15..0x19,
 *  object spawn 0x1A (slot, type, x, y, z, dir, ...), obj_select/set/get
 *  0x12/0x1E/0x1F, wait 0x10, message 0x11.
 *
 * ---- object types (see dc2objects.py) ------------------------------------------
 *  obj+0x58 type; per-area handler table *0x87DCA0 (28 entries).
 *  0/1 player characters; 2..15 dinosaurs, Enemy_LoadModel 0x482070:
 *  2 E00 Velociraptor, 3 E10 Tyrannosaurus, 4 E20 Pteranodon, 5 E30
 *  Plesiosaurus, 6 E40 Giganotosaurus (src/game/gigano.c; kind 1 the bridge scene of
 *  room 503: sections are quads at 0x720D38 - 0x10*i, event flags 7/0x12..0x18 the
 *  broken ones), 7 E90 Oviraptor, 8 E60 Allosaurus,
 *  9 E70 Triceratops (src/game/tricera.c, handler 0x43CF30),
 *  10 E80 Mosasaurus (src/game/mosa.c, handler 0x43F820), 11/12 E31/E32 (plesiosaurs in the
 *  water: at the surface / under it, src/game/plesio2.c), 13 EA0 Compsognathus
 *  (src/game/compso.c, handler 0x445690: only a few moves in the PC game), 14 E50 Inostrancevia
 *  (src/game/inostr.c; handler 0x437230 straight in the table, no type wrapper)
 *  (E<k>0 = entry k of the game's dinosaur list; model sizes agree);
 *  16 room prop (model inside the room block); 17..28 area objects
 *  (src/game/aobj.c: 17 the falling piece of areas 1, 2, 8; 18 the flying shell;
 *  19 the moving piece; 21 the bird of area 1; 22 the poison plant of area 1;
 *  23 the rock of area 8; 26 the burner of area 4; 27 the insect, every area,
 *  26 in area 6; parameters from the script at o+0x1C0..; 29/30 (28/29 in area 6) the
 *  weapon and sub-weapon in hand, by G+0x10B4)
 *  (area 0 type 24, src/game/ptera2.c: the area 0 pteranodon, the type 4
 *  pteranodon's kin without the carry; drifts along +z at o+0x202 while not
 *  attacking)
 *  (src/game/aobj2.c: area 8 type 20 keeps to o+0xD8 and turns after the
 *  player, o+0x10C its reaction; area 4 types 25 and 28 must be kept alive,
 *  health G+0x928 of G+0x92A, flag 7:0x1D when 25 reaches its point 0x739B78
 *  + 8 * o+0x10C, 28 keeps to 0x739BD8; at 0 the game is over)
 *  (area 3 type 22, src/game/roamer.c: the roamer of rooms 303..305, walks a
 *  graph at o+0x1C0 {u16 points, u16 links; points of 8 bytes: s16 x, u16
 *  mark, s16 z, u8 kind, s8 flag; links: u8 a, u8 b, s16 length}, from room
 *  to room through the exits event flags open; G+0x1274 the room it is in);
 *  0x1D.. playable dinosaurs KOF_P*.DAT (area 9 table only, 0x739964 + 4*type): 29/30
 *  raptor (src/game/kof_rap.c; its copy 2's model at 0x633000), 31/32 oviraptor (src/game/kof_ovi.c), 33/34 allosaurus (src/game/kof_allo.c), 35/36 tyrannosaurus (src/game/kof_rex.c), 37/38 triceratops
 *  (src/game/kof_tri.c), 39/40 compsognathus
 *  (src/game/kof_comp.c; health 1, its super move a pack of objects of its own type with
 *  o+0x10C 1, run by the same handler), 41/42 inostrancevia (src/game/kof_ino.c): each kind's code twice,
 *  one per player of the duel (the pads 0x87DC94/0x87DC08 and 0x87DC92/0x87DE0A, the
 *  other player object 2 or 1, own tables, variables and model at 0x662500/0x638000).
 *  Their attacks: Kof_Attack 0x47CB20 (spheres +0xA0 -> weapon record 0x1F).
 *  Placed by the room script only: obj_spawn (op 0x1A) and enemy_set (op 0x4F,
 *  random spawner EnemySet_Tick 0x472230). ENEMY SET: u8 zones, u8 type,
 *  u8 cap, u8 room limit, u8 variant, u8, u8 point choice, u8; zones of 32
 *  bytes: s16 quad[4][2] (player area), u16 points offset, u8 point count,
 *  u8, u8 max alive, u8 max spawns, u8 random, u8, u16 delay; spawn point
 *  16 bytes: s16 x, y, z, dir, u8 entry mode, u8, u8 AI mode, u8, u8 timer.
 *  All 1265 spawn points have y = -1000*k (floor levels); 91% of spawn
 *  positions lie in a walkable polygon of their level.
 *  Game text: u16 per character, 0 space, 5 '.', 0x14.. digits, 0x4000 line,
 *  0x5000 page, 0x6000 wait, 0xF000 end. Room names: ROOM.p10 (dc2_rooms.csv).
 *
 * ---- models (verified on E00.DAT, KOF_P00P.DAT; see dc2model.py) --------------
 *  Type-5 LZSS block at its load address (E*: 0x633000, KOF_P*: 0x662500):
 *  {verts*, normals*, tris*, quads*, u16 ntri, u16 nquad, u32 nparts,
 *   u16 tpage, u16 clut, PART[nparts] (20 bytes: s16 joint[3], s8 parent,
 *   u8 flag, tris*, quads*, u16 ntri, u16 nquad)}; part-local vertices,
 *  seam polygons share vertices of adjacent parts. Drawn by MeshDrawGT3
 *  0x4834F0 / MeshDrawGT4 0x4832C0. Sound banks (type 3) also in E*.DAT and KOF_P*.DAT.
 *
 * ---- animation (see dc2anim.py) ---------------------------------------------
 *  Table of u32 sequence pointers right after the quad list (WEP_P*: after
 *  0xCD padding, at block+0x3800 = 0x665D00); object+0x88 holds it.
 *  SEQUENCE: u16 frames, ptrs +4/+8/+0xC (small tables), frames at +0x14.
 *  FRAME (align4(16 + 6*parts)): s16 ?, s16 duration, s16 root[3],
 *  s16 objrot[3], s16 rot[3] per part (4096 = 360 deg). Cubic interpolation
 *  over 4 keys, part matrix RotMatrixYXZ. Human (15-part) sequences also
 *  live in E*.DAT and DOOR*.DAT.
 *
 * ---- headerless archives (loader synthesizes the records) ----------------------
 *  ITEM.DAT 128 x 0x2800: 128x64 8bpp + CLUT256   (Item_LoadIcon 0x403EE0)
 *  MAP.DAT   87 x 0x10800: 2 x 256x256 4bpp + CLUT16 (Map_LoadPage 0x403FD0)
 *  FILE.DAT  48 x 0x3800: 128x96 8bpp + CLUT256   (File_LoadPicture 0x404100)
 *
 * ---- cutscenes SC*.DAT (see dc2cutscene.py) -----------------------------------
 *  one LZSS block per scene at 0x5FE000..0x620000: 24 files = chains of
 *  animation sequences + event tables {u16 frame, u16 code}; 19 start with an
 *  actor model; 5 are packs of env-mapped static objects. No camera tracks.
 *
 * ---- other media --------------------------------------------------------------
 *  Movie/*.DAT: MPEG-1 program streams, 320x240 or 320x160, MPEG audio 44.1 kHz.
 *  Speech/NNNN.dat: RIFF WAVE PCM16, 18900 Hz mono (284) / 37800 Hz stereo (21).
 *
 * ---- the in-game menu (task 0x491950, src/game/menu.c) ------------------------
 *  M = G+0x68C: +0 mode (table 0x739C28: 0 init, 1 opening, 2 screen +1 by
 *  entries 4.. of that table, 3 leave, ...), +2 step, +8..+0x17 key repeat
 *  counters, +0x40 points shown (to G+0x11F4), +0x44 keys of the frame, +0x4A
 *  health shown, +0x4D entry, +0x59 health bar shimmer, +0x5E/+0x5F hint text
 *  letters / text, +0x60 area, +0x64 how opened (0 pause, 1 item, 3 door,
 *  4 game over, 5 flag 2:0x20), +0x65 frames before the menu button, +0x74..
 *  +0x7C backdrop bars, +0x80/+0x82 sweeps, +0x84 pulse, +0x8C/+0x8D flash,
 *  +0x8E opening frames. Weapon records 0x704268 (main) and 0x704328 (sub),
 *  12 bytes {u, v, flags: 0x20 both hands, 0x80 infinite ammunition}.
 *  Frames of sprites: 8-byte parts {u, v, w (0: 256), h, s8 dx, s8 dy, count
 *  (first part), flags (2: port code 0x80)}.
 *  Memos (screen 3): flags 9:0x50..0x7F found; M+0x30 the six listed, +0x54
 *  count, +0x55 cursor, +0x56 first shown, +0x57 memo read, +0x58 its page,
 *  +0x92 letters typed, +0x94 page out. Records 0x704628 + 8*memo {title,
 *  text}: 16-bit codes, low 12 bits a letter (tpage, widths 0x723D4C) or 6 a
 *  digit of G+0x1204..; high nibble 1 color, 2 the picture, 4 newline, 5 page
 *  end, F text end (Memo_Type 0x49B270).
 *
 * ---- flags ----------------------------------------------------------------------
 *  Flag_Set(group, bit, on) 0x46FAA0 (503 calls), Flag_Check(group, bit)
 *  0x46FB20 (465 calls); groups @ 0x723C40 = 12 x {u32 *bits, u32 count}.
 */
#ifndef DC2_GAME_FLOW_H
#define DC2_GAME_FLOW_H
typedef struct {
    unsigned short f0, dat_file_id, dbs_file_id, f6;
    unsigned char  s8_15[8];
    void *p10, *p14;
} DC2_ROOM;   /* 24 bytes on the 32-bit build */
#endif
