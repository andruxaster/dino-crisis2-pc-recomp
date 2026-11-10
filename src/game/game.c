/* From the title into the game: Game_Start (menu screens and the game by
 * mode), the opening, the new-game state, and the game task (0x4510F0)
 * with its modes: init, area tables, room enter, play. Plus the file loads
 * of rooms, doors and armour, and the checkpoint copy of the game state. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/task.h"
#include "dc2/gte.h"

#define G16(o)  U16(G() + (o))
#define G8(o)   U8(G() + (o))
#define A_AREAS        0x73825Cu   /* 10 x 24: rooms, set_tables, ... */
#define A_MENU_FILES   0x703EE8u   /* 8 per mode: u16 file, u32 task */
#define A_OPENING      0x74DB90u

static void wait_busy(void) { while (File_IsBusy()) Task_Sleep(1); }
static void load_wait(u32 id) { wait_busy(); File_RequestLoad(id); wait_busy(); }
static gptr room_entry(void) { return U32(A_AREAS + G8(0x1091) * 24) + G8(0x1090) * 24; }

static void screen_tint(u8 c, u8 b) { G8(0xB58) = c; G8(0xB59) = c; G8(0xB5A) = c; G8(0xB5B) = b; G8(0xB5C) = 0; }

/* load the file of a menu screen / the game, then switch to its task */
void DC2CALL Game_Start(u32 mode)                                    /* 0x4826F0 */
{
    screen_tint(0x80, 1);
    Flag_Set(3, 0, 0);
    while (File_IsBusy()) { DrawSync(0); Task_Sleep(1); }
    File_RequestLoad(U16(A_MENU_FILES + mode * 8));
    while (File_IsBusy()) { DrawSync(0); Task_Sleep(1); }
    screen_tint(0xA0, 0);
    gptr t = U32(A_MENU_FILES + 4 + mode * 8);
    if (t) Task_Chain(t);
}

void DC2CALL Pause_LoadScreen(void)                                  /* 0x4827F0 */
{
    screen_tint(0x80, 1);
    while (File_IsBusy()) { DrawSync(0); Task_Sleep(1); }
    File_RequestRaw(0x89, 0x6C1500);
    while (File_IsBusy()) { DrawSync(0); Task_Sleep(1); }
    screen_tint(0xA0, 0);
}

/* ---- loads of the room set ------------------------------------------------------------ */
void DC2CALL Room_LoadDat(void)                                      /* 0x481C10 */
{
    gptr e = room_entry();
    if (G16(0x67E) == U16(e + 2)) return;
    load_wait(U16(e + 2));
    G16(0x67E) = U16(e + 2);
}

void DC2CALL Room_LoadExtra(u32 v)                                   /* 0x481CA0: by 0x73842C */
{
    v &= 0xFFFF;
    u16 id = U16(U32(0x73842C + (v >> 8) * 4) + (v & 0xFF) * 2);
    if (id == G16(0x678)) return;
    load_wait(id);
    G16(0x678) = id;
}

void DC2CALL Door_Load(u32 n)                                        /* 0x481D20 */
{
    gptr e = 0x738498 + n * 12;
    u32 end = U32(e + 8);
    if (end != U32(e + 4) && G16(0x680) != 0xFFFF && U32(0x703524 + G16(0x680) * 8) < end)
        G16(0x680) = 0xFFFF;                     /* the door overwrites the enemy model */
    load_wait(U16(e));
}

void DC2CALL Armor_Load(u32 n) { load_wait(U16(0x738774 + n * 4)); } /* 0x481DC0 */

/* music of the room: the stream id from its sound slot; when only the low
   byte (track) changes, bit 3 marks it as a continuation */
u32 DC2CALL Snd_RoomMusic(void)                                      /* 0x48CBD0 */
{
    u16 v = U16(Room_SoundSlot(G8(0x1091), G8(0x1090)));
    if (v != 0xFFFF && v != U16(0x87FBD4)) {
        u16 old = U16(0x87FBD4);
        if (!((old ^ v) & 0xFF00)) v |= 8;
        U16(0x87FBD2) = old;
        U16(0x87FBD4) = v;
    }
    return v;
}

/* ---- checkpoint: position and game flags kept for a retry ------------------------------ */
void DC2CALL Checkpoint_Save(void)                                   /* 0x48F2C0 */
{
    G16(0x10AC) = G16(0x840); G16(0x10AE) = G16(0x842); G16(0x10B0) = G16(0x844); G16(0x10B2) = G16(0x846);
    memcpy(MEM(G() + 0xC2C), MEM(G() + 0x108C), 0x118 * 4);
}

void DC2CALL Checkpoint_Load(void)                                   /* 0x48F330 */
{
    memcpy(MEM(G() + 0x108C), MEM(G() + 0xC2C), 0x118 * 4);
    G16(0x840) = G16(0x10AC); G16(0x842) = G16(0x10AE); G16(0x844) = G16(0x10B0); G16(0x846) = G16(0x10B2);
}

/* ---- area tables (object type handlers etc. of each area) ------------------------------- */
static const u32 g_area_tabs[10][4] = {
    {0x738DDC, 0x738E4C, 0x738E94, 0x738EE8}, {0x738F08, 0x738F78, 0x738FCC, 0x739020},
    {0x739064, 0x7390D4, 0x73911C, 0x739174}, {0x7391B8, 0x739228, 0x739274, 0x7392C8},
    {0x73930C, 0x739380, 0x7393CC, 0x739424}, {0x739468, 0x7394D8, 0x73951C, 0x739568},
    {0x7395B0, 0x73961C, 0x739660, 0x7396AC}, {0x7396CC, 0x73973C, 0x739784, 0x7397DC},
    {0x739800, 0x739870, 0x7398C8, 0x739924}, {0x739964, 0x739A14, 0x739A60, 0x739ABC},
};
static void area_tables(u32 a)
{
    G16(0x67C) = (u16)a;
    U32(0x87DCA0) = g_area_tabs[a][0]; U32(0x87F948) = g_area_tabs[a][1];
    U32(0x87DC8C) = g_area_tabs[a][2]; U32(0x87F320) = g_area_tabs[a][3];
}
#define AREA_FN(n, addr) void DC2CALL Area##n##_SetTables(void) { area_tables(n); }
AREA_FN(0, 0x48D170) AREA_FN(1, 0x48D1B0) AREA_FN(2, 0x48D1F0) AREA_FN(3, 0x48D230) AREA_FN(4, 0x48D270)
AREA_FN(5, 0x48D2B0) AREA_FN(6, 0x48D2F0) AREA_FN(7, 0x48D330) AREA_FN(8, 0x48D370) AREA_FN(9, 0x48D3B0)

/* ---- sound settings of all rooms from the tables in .data -------------------------------- */
void DC2CALL Snd_InitRoomTables(void)                                /* 0x48BF00 */
{
    static const u16 dst[10] = {0x138C, 0x13A4, 0x13E8, 0x1400, 0x141C, 0x144C, 0x1460, 0x1484, 0x14A4, 0x14D4};
    static const u32 src[10] = {0x704014, 0x70402C, 0x704070, 0x704088, 0x7040A4, 0x7040D4, 0x7040E8, 0x70410C, 0x70412C, 0x70415C};
    static const u8 n[10] = {6, 0x11, 6, 7, 0xC, 5, 9, 8, 0xC, 6};
    for (int i = 0; i < 10; i++) memcpy(MEM(G() + dst[i]), MEM(src[i]), n[i] * 4u);
}

/* ---- new game ------------------------------------------------------------------------------ */
void DC2CALL Game_ClearState(void) { memset(MEM(G() + 0x664), 0, 0x42 * 4); }  /* 0x4510D0 */

void DC2CALL Game_NewState(void)                                     /* 0x450E00 */
{
    static const u32 parts[8] = {0x50302, 0x3050100, 0x2050401, 0x10205, 0x20405, 0x5010004, 0x1050003, 0x1020504};
    G16(0x1090) = 0x101;                          /* area 1, room 1 */
    U32(G() + 0x1098) = 0;
    gptr e = room_entry();
    G16(0x840) = U16(e + 8);   G16(0x10AC) = G16(0x840);   /* start position */
    G16(0x842) = U16(e + 0xA); G16(0x10AE) = G16(0x842);
    G16(0x844) = U16(e + 0xC); G16(0x10B0) = G16(0x844);
    G16(0x846) = U16(e + 0xE); G16(0x10B2) = G16(0x846);
    G8(0x108F) = 1;
    G16(0x10B4) = 0x101; G16(0x10B6) = 0x202; G16(0x10B8) = 0x101; G16(0x10BA) = 0x50A;
    G16(0x11F8) = 0x4B0; G16(0x11FC) = 0x4B0;
    G8(0x10AA) = 1;
    U32(G() + 0x1204) = parts[Rand8() & 7] & 0x0F0F0F0F;
    Game_InitStock();
    switch ((s8)G8(0x108E)) {                     /* difficulty: starting items */
    case 1: {
        static const u8 f[10] = {0x55, 0x6D, 0x4B, 0x59, 0x4F, 0x6E, 0x65, 0x66, 0x67, 0x68};
        for (int i = 0; i < 10; i++) Flag_Set(5, f[i], 1);
        break; }
    case 2: {
        static const u8 f[6] = {0x4A, 0x48, 0x64, 0x5C, 0x69, 0x6F};
        for (u32 i = 0x40; i < 0x80; i++) Flag_Set(5, i, 1);
        for (int i = 0; i < 6; i++) Flag_Set(5, f[i], 0);
        break; }
    default: break;
    }
    Flag_Set(8, 0x19, 1);
}

/* ---- the opening (task 0x475630) ------------------------------------------------------------ */
void DC2CALL Opening_Init(gptr r)                                    /* 0x475660 */
{
    Snd_InitRoomTables();
    Flag_Set(3, 0, 0);
    U8(r)++;
}

void DC2CALL Opening_Movie(gptr r)                                   /* 0x4756A0 */
{
    switch (U8(r + 2)) {
    case 0: U8(r + 2)++; Movie_Start(3); break;
    case 1: Movie_Tick(); if (!Flag_Check(2, 0x17)) U8(r + 2)++; break;
    case 2: U8(r + 2) = 0; U8(r + 1) = 0; U8(r)++; break;
    default: break;
    }
}

void DC2CALL Opening_Run(gptr r)                                     /* 0x475680 */
{
    if (U8(r + 1) == 0) Opening_Movie(r);
    else Orig_Call1(U32(0x73791C + U8(r + 1) * 4), r);
}

void DC2CALL Opening_ToGame(gptr r)                                  /* 0x475700 */
{
    (void)r;
    Game_ClearState();
    Game_NewState();
    Task_Chain(0x4510F0);
}

void DC2CALL OpeningTask(void)                                       /* 0x475630 */
{
    U32(A_OPENING) = 0;
    for (;;) {
        switch (U8(A_OPENING)) {
        case 0: Opening_Init(A_OPENING); break;
        case 1: Opening_Run(A_OPENING); break;
        case 2: Opening_ToGame(A_OPENING); break;
        case 3: Opening_Movie(A_OPENING); break;
        default: Orig_Call1(U32(0x737910 + U8(A_OPENING) * 4), A_OPENING); break;
        }
        Task_Sleep(1);
    }
}

/* ---- the game task (0x4510F0): G+0x664 mode ------------------------------------------------- */
void DC2CALL Game_Init(void)                                         /* 0x451120  mode 0 */
{
    G16(0x664)++;
    Camera_Reset();
    Msg_Reset();
    Snd_ReleaseAll();
    G16(0x682) = 0xFFFF; G16(0x680) = 0xFFFF; G16(0x67E) = 0xFFFF; G16(0x67C) = 0xFFFF;
    G16(0x67A) = 0xFFFF; G16(0x678) = 0xFFFF; G16(0x676) = 0xFFFF; G16(0x674) = 0xFFFF;
    U16(0x87FBD4) = 0xFFFF; U16(0x87FBD2) = 0xFFFF;   /* nothing loaded */
    if (G8(0x662)) {                              /* continue from the checkpoint position */
        G16(0x840) = G16(0x10AC); G16(0x842) = G16(0x10AE); G16(0x844) = G16(0x10B0); G16(0x846) = G16(0x10B2);
    } else {
        Game_NewState();
    }
    if (G8(0x927)) {                              /* extra game */
        G16(0x1090) = 0x905;
        G16(0x846) = 0; G16(0x844) = 0; G16(0x842) = 0; G16(0x840) = 0;
        G8(0x108E) = 1;
        G8(0x108F) = G8(0x850);
        u32 i = Room_GlobalIndex(G8(0x1091), G8(0x1090)) & 0xFF;
        U8(G() + 0x12D0 + i) = 0;
        Game_ModeStock();
        for (u32 f = 0x50; f < 0x80; f++) Flag_Set(9, f, 0);
    } else if (G8(0x1094) & 1) {
        Flag_Set(9, 0x35, 1);
    }
}

void DC2CALL Game_AreaLoad(void)                                     /* 0x4512F0  mode 1 */
{
    G16(0x664)++;
    if (G16(0x67C) == G8(0x1091)) return;
    DrawSync(0);
    Orig_Call1(U32(A_AREAS + 4 + G8(0x1091) * 24), 0);   /* the area's set_tables */
}

void DC2CALL Game_RoomEnter(void)                                    /* 0x451330  mode 2 */
{
    G16(0x664)++;
    if (!G8(0x926)) Checkpoint_Save();
    Light_SetFar(0);
    memset(MEM(G() + 0x938), 0, 0xBC * 4);
    U32(0x87DE00) = 2;
    if (!G8(0x108E)) Flag_Set(2, 0x1A, 1);
    if (G8(0x927) == 1) {
        u8 s = G8(0x850);
        if (s > 6 || (s >= 2 && s <= 4)) Flag_Set(2, 0x1A, 1);
    }
    Room_LoadDat();
    Shake_Set(0xFFFFFFFF, 0, 0);
    Room_SetPointers(0x5E0000);
    G8(0xA89) = 0xFF; G8(0xAD1) = 0xFF;
    if (!G8(0x927)) {
        u32 v = Snd_RoomMusic();
        if (v != 0xFFFF) Room_LoadExtra(v);
    }
    G8(0xB4B) = 4;
    SetDepthScale((float)(s32)(0x400u << G8(0xB4B)));
    Flag_Set(3, 1, 1); Flag_Set(3, 2, 1); Flag_Set(3, 3, 1); Flag_Set(3, 4, 1); Flag_Set(3, 5, 1);
    Flag_Set(3, 8, 1);
    gptr pl = U32(G() + 0x940);
    switch (G8(0x927)) {
    case 0: Player_Setup(pl); break;
    case 1: if (G8(0x108F) == 3) Flag_Set(7, 0x11, 1); Player_Setup(pl); break;
    case 2: Flag_Set(2, 0x21, 1); Fn_4BF130(1); break;
    default: break;
    }
    if (G8(0x662)) { G8(0x662) = 0; Flag_Set(2, 0x23, 1); }
    Room_SoundInit();
    CamZones_Init();
    Msg_Reset();
    Script_Init();
    Camera_ZoneTick();
    Room_SetLighting();
    G8(0x852) = 0xFF;
    Fade_Set(0, 0x14, 1);
    G8(0x926) = 0;
    Flag_Set(2, 4, (u32)(s32)(s8)G8(0x14F0));
    Flag_Set(2, 0xE, (u32)(s32)(s8)G8(0x14F1));
    Flag_Set(2, 0x1C, (u32)(s32)(s8)G8(0x14F2));
    Gauge_Reset();
    Flag_Set(0xB, (Area_SoundArea(G8(0x1091), G8(0x1090)) & 0xFF) + 0x75, 1);
    Flag_Set(0xB, Room_GlobalIndex(G8(0x1091), G8(0x1090)) & 0xFF, 1);
    if (G16(0x1090) == 4 || G16(0x1090) == 5) {
        Flag_Set(0xB, 0, 1); Flag_Set(0xB, 4, 0); Flag_Set(0xB, 5, 0);
    }
}

void DC2CALL Game_Play(void)                                         /* 0x451790  mode 3 */
{
    if (Flag_Check(2, 0x1B)) {                    /* end of the game */
        Snd_FadeAll();
        Room_Reset();
        Task_Sleep(0x1E);
        Game_Start(0x11);
    }
    if (!Flag_Check(2, 6) && !Flag_Check(2, 0xF)) {
        gptr pl = U32(G() + 0x940);
        int free = !Flag_Check(1, 9) && !Flag_Check(1, 0x10) && !U16(pl + 0x174) &&
                   !Flag_Check(1, 0xA) && !Flag_Check(2, 0x25) && U8(pl + 0x54) == 1;
        u32 pad = U32(0x87DC94);
        if ((pad & 0x100) && free) Flag_Set(2, 8, 1);           /* inventory */
        else if ((pad & 1) && !G8(0x927) && free)
            Menu_Open(6, (u32)G8(0x1091) << 8 | G8(0x1090), 0, 0, 0);
        else if (pad & 0x800) Flag_Set(2, 0x10, 1);             /* map */
    }
    if (!Flag_Check(2, 6)) {
        if (Flag_Check(2, 8)) {
            Flag_Set(2, 0x25, 1);
            Frame_Draw();
            Task_Chain(0x491950);
        } else if (Flag_Check(2, 0x11)) {
            Frame_Draw();
            G16(0x76C) = 0;
            Task_Chain(0x451660);
            return;
        } else if (Flag_Check(2, 0x10) && !Flag_Check(2, 0xF)) {
            Frame_Draw();
            Task_Chain(0x478F40);
        }
    }
    if (Flag_Check(3, 9)) Timer_Draw();
    Game_Frame();
    Frame_Draw();
    Flag_Set(2, 0x25, 0);
    Shake_Tick();
}

void DC2CALL GameTask(void)                                          /* 0x4510F0 */
{
    Game_SetMode723CC0(0);
    for (;;) {
        switch ((s16)G16(0x664)) {
        case 0: Game_Init(); break;
        case 1: Game_AreaLoad(); break;
        case 2: Game_RoomEnter(); break;
        case 3: Game_Play(); break;
        default: Orig_Call1(U32(0x7224B4 + (s16)G16(0x664) * 4), 0); break;
        }
        Task_Sleep(1);
    }
}

/* the room's sound set-up from its sound table entry */
void DC2CALL Room_SoundInit(void)                                  /* 0x48CA10 */
{
    Strm_StartMask(U16(Room_SoundSlot(U8(G() + 0x1091), U8(G() + 0x1090)) + 2));
}

/* ---- the per-frame game work ------------------------------------------------------- */

/* the player (unless flag 1/3) and the other objects (unless 1/2) by their
 * type handlers (*0x87DCA0), their yaw kept to 12 bits */
void DC2CALL Obj_RunAll(void)                                       /* 0x471EF0 */
{
    gptr o;
    if (!Flag_Check(1, 3)) {
        o = U32(G() + 0x940);
        if (U8(o + 0x50) & 1) {
            Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), o);
            U16(o + 0x4A) &= 0xFFF;
        }
    }
    if (Flag_Check(1, 2)) return;
    o = U32(G() + 0x940) + 0x260;
    for (u32 n = U32(0x87F940) - 1; n; n--, o += 0x260) {
        if (!(U8(o + 0x50) & 1)) continue;
        Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), o);
        U16(o + 0x4A) &= 0xFFF;
    }
}

/* the room's props (G+0x944, 0xAC each) by their handlers (*0x87F948) */
void DC2CALL Prop_RunAll(void)                                      /* 0x471F90 */
{
    gptr o = U32(G() + 0x944);
    for (u32 n = U32(0x87DC88); n; n--, o += 0xAC)
        if (U8(o + 0x50) & 1) Orig_Call1(U32(U32(0x87F948) + U8(o + 0x58) * 4), o);
}

/* the effects (G+0x948, 0x34 each) by their handlers (*0x87DC8C) */
void DC2CALL Eff_RunAll(void)                                       /* 0x471FE0 */
{
    for (gptr e = U32(G() + 0x948); e < U32(G() + 0x948) + U32(0x87DE14) * 0x34; e += 0x34)
        if (U8(e) & 1) Orig_Call1(U32(U32(0x87DC8C) + U8(e + 1) * 4), e);
}

/* the object effects (G+0x93C, 0x70 each) by their handlers (*0x87F320),
 * then their matrices */
void DC2CALL ObjEff_RunAll(void)                                    /* 0x472040 */
{
    for (gptr o = U32(G() + 0x93C); o < U32(G() + 0x93C) + U32(0x87DE04) * 0x70; o += 0x70) {
        if (!(U8(o + 0x50) & 1)) continue;
        Orig_Call1(U32(U32(0x87F320) + S8(o + 0x4E) * 4), o);
        RotMatrixYXZ(o + 0x48, o + 0x20);
    }
}

/* one frame of play: each part unless its flag of group 1 is set */
void DC2CALL Game_Frame(void)                                       /* 0x472FD0 */
{
    gptr g = G();
    if (!Flag_Check(2, 0x1D)) {
        Flag_Set(2, 0x19, 0);
        if (!Flag_Check(1, 1)) Camera_Tick();
        if (!Flag_Check(1, 8)) Aot_Tick();
        if (!Flag_Check(1, 6)) Script_Tick();
        if (!Flag_Check(1, 0xB)) { EnemySet_Tick(); EnemySet_Small(); }
        Dim_Tick();
        if (!Flag_Check(1, 7)) Eff_RunAll();
        Obj_RunAll();
        if (!Flag_Check(1, 0xD)) ObjEff_RunAll();
        if (!Flag_Check(1, 4)) Prop_RunAll();
        Eff0_Trail();
        Msg_Tick();
    }
    if (U8(g + 0x84A) != U8(g + 0x84B)) {
        Camera_Load();
        U8(G() + 0x84B) = U8(G() + 0x84A);
    }
    if (!Flag_Check(1, 0xC)) Hud_Weapon();
    Gauge_Draw(0xC0, 0x10);
    if (Flag_Check(3, 0xB)) Boss_Gauge(0xC8, 0xD0);
    Hidden_UpdateAll();
    if (!Flag_Check(1, 5)) Obj_PickLights(G() + 0x778);
}

/* ---- game mode 4: game over, "Will you continue?" ----------------------------------- */
/* a label's animation {first frame, frame, delay, typing}; frames of 8
 * bytes: u, v, w, h, n | flags (bit 5 last, bit 6 loop), delay, dx, dy */
#define CONT_YESNO  0x74B178u      /* the "yes / no" choice */
#define CONT_TITLE  0x74B184u      /* "GAME OVER" */
#define CONT_TEXT   0x74B190u      /* the question typed out: length, shown, delay, typing */
#define CONT_LOGO   0x74B19Cu

void DC2CALL Continue_SetAnim(gptr a, gptr f)                       /* 0x4522C0 */
{
    U8(a + 9) = 0;
    U32(a) = f; U32(a + 4) = f;
    U8(a + 8) = U8(f + 5);
}

/* 0 running, 1 at the end, 2 looped back */
u32 DC2CALL Continue_AnimStep(gptr a)                               /* 0x4522E0 */
{
    if (U8(a + 8)) { U8(a + 8)--; return 0; }
    if (U8(a + 9)) {                                /* typing: one character every 3 frames */
        U16(a + 2)++;
        U8(a + 8) = 2;
        return U16(a + 2) > U16(a);
    }
    gptr f = U32(a + 4);
    s32 bits = (s8)U8(f + 4) >> 5;
    if (bits & 1) return 1;
    if (bits & 2) { U32(a + 4) = U32(a); U8(a + 8) = U8(U32(a) + 5); return 2; }
    f += (U8(f + 4) & 0x1F) * 8;
    U32(a + 4) = f;
    U8(a + 8) = U8(f + 5);
    return 0;
}

/* a label's sprites (CLUT 0x7C00) and its draw mode */
void DC2CALL Continue_Draw(s32 x, s32 y, u32 color, gptr f)        /* 0x452170 */
{
    s32 h = (s8)U8(f + 4);
    u32 n = h & 0x1F;
    u32 code = ((((u32)(h >> 5) & 4) >> 1) | 0x64) << 24 | color;
    for (; n; n--, f += 8) {
        gptr p = U32(0x87DC98);
        Gpu_SetSprt(p);
        U32(p + 4) = code;
        F32(p + 8) = (float)(u32)(x + (s8)U8(f + 6));
        F32(p + 0xC) = (float)(u32)(y + (s8)U8(f + 7));
        U8(p + 0x14) = U8(f); U8(p + 0x15) = U8(f + 1);
        U16(p + 0x18) = U8(f + 2); U16(p + 0x1A) = U8(f + 3);
        U16(p + 0x16) = 0x7C00;
        Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
        U32(0x87DC98) += 0x1C;
    }
    gptr p = U32(0x87DC98);
    Gpu_SetDrawMode(p, 1, 0, 0x85, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
    U32(0x87DC98) += 0xC;
}

/* characters of a message up to its end code (0xFxxx) */
u32 DC2CALL Text_Length(gptr s)                                     /* 0x471A90 */
{
    u32 n = 0;
    while ((U16(s) & 0xF000) != 0xF000) { s += 2; n++; }
    return n;
}

static void cont_wait(void)
{
    while (File_IsBusy()) Task_Sleep(1);
}

static void cont_flush(void)
{
    u32 k = U32(0x87DE0C) << 12;
    Gpu_DrawOT(U32(0x87F94C) + 0x88, 0x87CBFC + k, 0x87BC00 + k);
}

/* G+0x666: 0 set-up, 1 the question appears, 2 yes / no, 3 the game over
 * picture, 4 back to the title; G+0x668 the choice, G+0x66A frames */
void DC2CALL Game_Continue(void)                                    /* 0x451990  mode 4 */
{
    gptr g = G();
    gptr text = U8(g + 0x108E) ? 0x7275F6u : 0x7275D0u;   /* the question (the extra game's) */
    switch ((s16)G16(0x666)) {
    case 0:
        G16(0x668) += 4;
        U8(G() + 0xB58) = 0x80; U8(G() + 0xB59) = 0x80; U8(G() + 0xB5A) = 0x80;
        U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
        DrawSync(0);
        if (Flag_Check(2, 6)) { Game_Frame(); Frame_Draw(); return; }
        Gauge_Reset();
        Room_Reset();
        U32(0x87DE00) = 2;
        Shake_Set(0xFFFFFFFF, 0, 0);
        Flag_Set(2, 4, 1);
        Lights_Unmark();
        G16(0x668) = 0;
        Flag_Set(3, 0, 0);
        cont_wait();
        if (U8(G() + 0x927) == 1 || (U8(G() + 0x108E) && (s8)Item_Find(G() + 0x11AC, 6, 0x1E) < 0)) {
            File_RequestLoad(0x78);                  /* no continue: the game over picture */
            cont_wait();
            U32(G() + 8) = 0x6C1500;
            G16(0x666) = 3;
            G16(0x66A) = 0x64;
        } else {
            File_RequestLoad(0x14);
            cont_wait();
            U32(G() + 8) = 0x6C1500;
            G16(0x666) = 1;
            G16(0x66A) = 0;
            Continue_SetAnim(CONT_TITLE, 0x6601D0);
            text = U8(G() + 0x108E) ? 0x7275F6u : 0x7275D0u;
            U16(CONT_TEXT) = (u16)Text_Length(text);
            U32(CONT_TEXT + 4) = (u16)Text_Left(text);
            U16(CONT_TEXT + 2) = 0;
            U8(CONT_TEXT + 8) = 0; U8(CONT_TEXT + 9) = 1;
            Continue_SetAnim(CONT_LOGO, 0x65F500);
        }
        Flag_Set(3, 0, 1);
        Fade_Set(0, 0xA, 1);
        return;
    case 1:
        Flag_Set(3, 0, 1);
        if (Flag_Check(2, 6)) return;
        Continue_AnimStep(CONT_TITLE);
        if (Continue_AnimStep(CONT_TEXT) || (U32(0x87DC00) & 0x70)) {
            Continue_SetAnim(CONT_YESNO, 0x660220);
            G16(0x666) = 2;
        }
        Text_Draw(text, U16(CONT_TEXT + 4), 0x8A, U16(CONT_TEXT + 2), 0x808080, 1, 1);
        cont_flush();
        Continue_AnimStep(CONT_LOGO);
        Continue_Draw(0x9F, 0x80, 0x808080, U32(CONT_TITLE + 4));
        Continue_Draw(0x9F, 0x80, 0x808080, U32(CONT_LOGO + 4));
        return;
    case 2: {
        Text_Draw(text, U16(CONT_TEXT + 4), 0x8A, 0x64, 0x808080, 1, 1);
        cont_flush();
        Continue_AnimStep(CONT_YESNO);
        Continue_AnimStep(CONT_TITLE);
        Continue_AnimStep(CONT_LOGO);
        Continue_Draw(0x9F, 0x80, 0x808080, U32(CONT_YESNO + 4));
        Continue_Draw(0x9F, 0x80, 0x808080, U32(CONT_TITLE + 4));
        Continue_Draw(0x9F, 0x80, 0x808080, U32(CONT_LOGO + 4));
        G16(0x66A)++;
        if ((s16)G16(0x66A) > 0x1E) G16(0x66A) = 0x1F;
        if ((s16)G16(0x66A) <= 5) return;
        u32 pad = U32(0x87DC00);
        if (pad & 0xA000) {                          /* left / right */
            Se_Play(0, 0);
            G16(0x668) ^= 1;
            G16(0x66A) = 0;
            Continue_SetAnim(CONT_YESNO, G16(0x668) ? 0x660250u : 0x660220u);
            return;
        }
        if (!(pad & 0x20)) return;
        if (U8(G() + 0x108C) < 0x63) U8(G() + 0x108C)++;   /* continues used */
        Se_Play(0, 2);
        if (!G16(0x668)) {                           /* yes: from the checkpoint */
            u16 rm = U16(G() + 0x1090);
            u32 t = U32(G() + 0x1098);
            if (rm == 1 || rm == 0x80A) G16(0x67E) = 0xFFFF;
            Checkpoint_Load();
            if (U8(G() + 0x108E)) {                  /* the extra game: one continue item used */
                u32 i = Item_Find(G() + 0x11AC, 6, 0x1E) & 0xFF;
                gptr e = G() + (i + 0x179) * 12;
                U16(e + 8)--;
                if (S16(e + 8) <= 0) {
                    U32(e) = 0; U32(e + 4) = 0; U32(e + 8) = 0;
                    Item_Compact(G() + 0x11AC, 6);
                }
            }
            U32(G() + 0x1098) = t;
            Item_Merge();
            U16(G() + 0x11F8) = 0x4B0; U16(G() + 0x11FC) = 0x4B0; U16(G() + 0x1200) = 0;
            G16(0x664) = 1; G16(0x66A) = 0; G16(0x668) = 0; G16(0x666) = 0;
            Fade_Set(1, 0xA, 1);
            Task_Sleep(0xA);
            return;
        }
        Fade_Set(1, 0xA, 1);                         /* no: the game over picture */
        Task_Sleep(0xA);
        cont_wait();
        Flag_Set(3, 0, 0);
        File_RequestLoad(0x78);
        cont_wait();
        U32(G() + 8) = 0x6C1500;
        Flag_Set(3, 0, 1);
        Fade_Set(0, 0xA, 1);
        G16(0x666) = 3;
        G16(0x66A) = 0x64;
        return; }
    case 3:
        Flag_Set(3, 0, 1);
        if (--G16(0x66A)) return;
        G16(0x666) = 4; G16(0x668) = 0; G16(0x66A) = 0;
        Fade_Set(1, 0x14, 1);
        return;
    case 4:
        if (Flag_Check(2, 6)) return;
        Snd_ReleaseAll();
        Game_ClearState();
        G16(0x772) = 0; G16(0x770) = 0; G16(0x76E) = 0; G16(0x76C) = 0;
        if (U8(G() + 0x927) == 1) Game_Start(0x19);
        Task_Chain(0x471EC0);
        return;
    }
}

/* the byte at 0x723CC0 (1 once the game is left) */
void DC2CALL Game_SetMode723CC0(u32 v)                             /* 0x46FC50 */
{
    U8(0x723CC0) = (u8)v;
}

/* the game left (game over without continuing): the ending sequence when
   G+0x108D is set, else the save scan and the title */
void DC2CALL Game_Leave(void)                                       /* 0x471EC0 */
{
    Game_SetMode723CC0(1);
    if (U8(G() + 0x108D)) { Game_Start(0x16); return; }
    Task_Chain(0x403920);
}
