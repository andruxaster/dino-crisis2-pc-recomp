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
    Fn_496410();
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
    case 0: U8(r + 2)++; Fn_473130(3); break;
    case 1: Fn_4731F0(); if (!Flag_Check(2, 0x17)) U8(r + 2)++; break;
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
    Fn_403480();
    Fn_470CB0();
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
        Fn_452450();
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
    Fn_48FB50(0);
    memset(MEM(G() + 0x938), 0, 0xBC * 4);
    U32(0x87DE00) = 2;
    if (!G8(0x108E)) Flag_Set(2, 0x1A, 1);
    if (G8(0x927) == 1) {
        u8 s = G8(0x850);
        if (s > 6 || (s >= 2 && s <= 4)) Flag_Set(2, 0x1A, 1);
    }
    Room_LoadDat();
    Fn_46FBC0(0xFFFFFFFF, 0, 0);
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
    Fn_48CA10();
    CamZones_Init();
    Fn_470CB0();
    Script_Init();
    Fn_403520();
    Fn_46B8C0();
    G8(0x852) = 0xFF;
    Fade_Set(0, 0x14, 1);
    G8(0x926) = 0;
    Flag_Set(2, 4, (u32)(s32)(s8)G8(0x14F0));
    Flag_Set(2, 0xE, (u32)(s32)(s8)G8(0x14F1));
    Flag_Set(2, 0x1C, (u32)(s32)(s8)G8(0x14F2));
    Fn_407170();
    Flag_Set(0xB, (Fn_491B10(G8(0x1091), G8(0x1090)) & 0xFF) + 0x75, 1);
    Flag_Set(0xB, Room_GlobalIndex(G8(0x1091), G8(0x1090)) & 0xFF, 1);
    if (G16(0x1090) == 4 || G16(0x1090) == 5) {
        Flag_Set(0xB, 0, 1); Flag_Set(0xB, 4, 0); Flag_Set(0xB, 5, 0);
    }
}

void DC2CALL Game_Play(void)                                         /* 0x451790  mode 3 */
{
    if (Flag_Check(2, 0x1B)) {                    /* end of the game */
        Fn_48CB40();
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
            Fn_4A7250();
            Task_Chain(0x491950);
        } else if (Flag_Check(2, 0x11)) {
            Fn_4A7250();
            G16(0x76C) = 0;
            Task_Chain(0x451660);
            return;
        } else if (Flag_Check(2, 0x10) && !Flag_Check(2, 0xF)) {
            Fn_4A7250();
            Task_Chain(0x478F40);
        }
    }
    if (Flag_Check(3, 9)) Fn_405E40();
    Fn_472FD0();
    Fn_4A7250();
    Flag_Set(2, 0x25, 0);
    Fn_452370();
}

void DC2CALL GameTask(void)                                          /* 0x4510F0 */
{
    Fn_46FC50(0);
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
