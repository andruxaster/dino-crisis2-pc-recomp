/* The player object (type 0 of every area; Regina, Dylan, the others by
 * G+0x108F) and its per-frame handler. The states (+0x54, table 0x7380FC)
 * are not rewritten yet and run through Orig_Call1.
 * *0x87FD60 = player + 0x1C0, the player's extra block: +0x26 timer,
 * +0x36 frame counter, +0x3A breath (0x20..0xE0), +0x79 idle counter. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/pcio.h"

#define A_PLAYER_EXT    0x87FD60u
#define A_PLAYER_STATES 0x7380FCu
#define A_PLAYER_FILES  0x7034F0u   /* u16 file ids of the characters' models */
#define PX()            U32(A_PLAYER_EXT)

/* object type and animation set by character, the character's file, then
 * the type handler's set-up */
void DC2CALL Player_Setup(gptr o)                                  /* 0x47FE10 */
{
    gptr g = G();
    u8 c = U8(g + 0x108F);
    u32 file;
    if (c == 3) { U8(o + 0x58) = 0x2B; U16(G() + 0x10B4) = 0xC; file = 0; }
    else if (c == 4) { U8(o + 0x58) = 0x14; U16(G() + 0x10B4) = 0xD; file = 1; }
    else if (c >= 7) { U8(o + 0x58) = (u8)(c + 0xE); file = c - 5u; U16(G() + 0x10B4) = (u16)(c + 7); }
    else {
        if (U8(g + 0x927)) switch (c) {
            case 0: U16(g + 0x10B4) = 6; break;
            case 1: U16(g + 0x10B4) = 0x101; break;
            case 5: U16(g + 0x10B4) = 0x604; break;
            case 6: U16(g + 0x10B4) = 5; break;
        }
        Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), o);
        return;
    }
    gptr id = A_PLAYER_FILES + (file & 0xFFFF) * 2;
    if (U16(G() + 0x674) != U16(id)) {
        while (File_IsBusy()) Task_Sleep(1);
        File_RequestLoad(U16(id));
        while (File_IsBusy()) Task_Sleep(1);
        U16(G() + 0x674) = U16(id);
    }
    Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), o);
}

/* breath (+0x3A of the extra block) by state: up while running, down at rest */
void DC2CALL Player_Breath(gptr o)                                 /* 0x47A7E0 */
{
    u8 c = U8(G() + 0x108F);
    if (c == 4 || c >= 7) { U16(PX() + 0x3A) = 0x70; return; }
    if (U8(o + 0x54) == 1) {
        if (U8(o + 0x55) == 2) U16(PX() + 0x3A) += 2;
        if (U8(o + 0x55) == 0) U16(PX() + 0x3A)--;
        if (U8(o + 0x55) == 5) U16(PX() + 0x3A) += 0xFFFE;
        if (U8(o + 0x55) == 6) U16(PX() + 0x3A) = 0x20;
    }
    if (U16(PX() + 0x3A) > 0xE0) U16(PX() + 0x3A) = 0xE0;
    if (U16(PX() + 0x3A) < 0x20) U16(PX() + 0x3A) = 0x20;
}

/* floor below the player: layer +0x5A (1000 units each) */
static s16 floor_layer(gptr o)
{
    gptr q = Tmp_Push(8);
    U16(q) = U16(o + 0x40); U16(q + 2) = 10000; U16(q + 4) = U16(o + 0x44);
    s16 h = (s16)Floor_Height(q, o + 0x40, 0x80, 0);
    Tmp_Pop(8);
    s16 layer = (s16)(h / -1000);
    U16(o + 0x5A) = (u16)layer;
    return layer;
}

void DC2CALL Player_Handler(gptr o)                                /* 0x47CE40 */
{
    Obj_SavePos(o);
    U32(A_PLAYER_EXT) = o + 0x1C0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x4B0;
    U8(o + 0x15D) = 0xFF;
    U32(o + 0x158) = 1;
    Orig_Call1(U32(A_PLAYER_STATES + U8(o + 0x54) * 4), o);
    U16(PX() + 0x36)++;
    if (U16(PX() + 0x26)) U16(PX() + 0x26)--;
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    if (U8(G() + 0x108F) == 2) {                    /* in water (character 2) */
        if (U8(o + 0x55) != 9 && !(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
        if (U8(o + 0x54) <= 2) {                    /* float up to the surface */
            s32 lim = -1000 * floor_layer(o);
            if (lim > S16(o + 0x42)) U16(o + 0x42) += 0x32;
            if (lim < S16(o + 0x42)) U16(o + 0x42) = (u16)lim;
            U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
        }
        if (!Flag_Check(2, 0xF)) {
            u8 n = ++U8(PX() + 0x79);
            if (n == 0x50) { Fn_48C0B0(o, 0x66); Effect_Attach(o, 0x6825D8); }
            else if (n == 0xBE) { Fn_48C0B0(o, 0x67); Effect_Attach(o, 0x6825D8); U8(PX() + 0x79) = 0; }
        }
    }
    if (U16(o + 0x17E) && !Flag_Check(2, 0xF)) {   /* bleeding */
        u32 t = --U16(o + 0x17E);
        if (t % 12 == 1) {
            s32 n = (s32)t / 50;
            if ((s16)n > 10) n = 10;
            else if ((s16)n < 3) n = 3;
            if (U8(G() + 0x108F) == 2) Effect_Attach(o, 0x682A44);
            else Effect_Drops(o, (u32)n, 0);
        }
        if (U16(o + 0x17E) % 6 == 1) {
            U16(o + 0x108) -= 4;
            if (S16(o + 0x108) < 0) U16(o + 0x108) = 0;
        }
    }
    Head_Update(o);
    Obj_UpdateMatrix(o);
    Obj_AnchorFoot(o);
    if (U8(G() + 0x108F) == 2 && U8(o + 0x55) == 9) {   /* walls one and two layers up too */
        u16 y = U16(o + 0x42), py = U16(o + 0xE2), layer = U16(o + 0x5A);
        U16(o + 0x42) = (u16)(y - 1000); U16(o + 0xE2) = (u16)(py - 1000);
        U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
        if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
        U16(o + 0x42) += 0xFC40; U16(o + 0xE2) += 0xFC40;
        U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
        if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
        U16(o + 0xE2) = py; U16(o + 0x5A) = layer; U16(o + 0x42) = y;
    }
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(G() + 0x108F) == 2 && U8(o + 0x54) == 1 && U8(o + 0x55) != 9 && !Flag_Check(2, 0xF)) {
        if (-1000 * floor_layer(o) > S16(o + 0x42)) U32(o + 0x54) = 0x40901;   /* surfaced: swim */
    }
    Fn_47AFA0(o);
    if (!(U8(o + 0x50) & 0x80)) Fn_486870(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
}
