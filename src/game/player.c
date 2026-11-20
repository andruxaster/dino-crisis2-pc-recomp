/* The player object (type 0 of every area; Regina, Dylan, the others by
 * G+0x108F) and its per-frame handler. The states (+0x54, table 0x7380FC)
 * are not rewritten yet and run through Orig_Call1.
 * *0x87FD60 = player + 0x1C0, the player's extra block: +0x26 timer,
 * +0x36 frame counter, +0x3A breath (0x20..0xE0), +0x79 idle counter. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/pcinput.h"
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
            if (n == 0x50) { Se_Play(o, 0x66); Effect_Attach(o, 0x6825D8); }
            else if (n == 0xBE) { Se_Play(o, 0x67); Effect_Attach(o, 0x6825D8); U8(PX() + 0x79) = 0; }
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
    Thrown_Update(o);
    if (!(U8(o + 0x50) & 0x80)) Zone_Update(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
}

/* ---- states ------------------------------------------------------------------- */
#define A_AIM_TABLE 0x6F0000u       /* scratch: {u32 dist2, u16 object, u16 priority} */

void DC2CALL Obj_SaveFeet(gptr o)                                  /* 0x48FCE0 */
{
    Obj_UpdateMatrix(o);
    for (int k = 0; k < 2; k++) {
        gptr p = U32(o + 0x120 + 4 * k);
        U16(p + 0x6C) = U16(p + 0x40); U16(p + 0x6E) = U16(p + 0x42); U16(p + 0x70) = U16(p + 0x44);
    }
}

/* weapon handler of the current set: 0x7380A0[G+0x10B4 >> 8] */
void DC2CALL Player_Weapon(gptr o)                                 /* 0x47AF40 */
{
    Orig_Call1(U32(0x7380A0 + (U16(G() + 0x10B4) >> 8) * 4), o);
}

/* free the 12 object-effect slots G+0x93C */
void DC2CALL Eff_FreeAll(void)                                     /* 0x47AF60 */
{
    for (gptr e = U32(G() + 0x93C); e < U32(G() + 0x93C) + 0x540; e += 0x70) Eff_Free(e);
}

void DC2CALL Eff_Free(gptr e) { U32(e + 0x50) = 0; }               /* 0x48D730 */

/* first free one of the 12 object-effect slots (+0x46 = its index) */
gptr DC2CALL Eff_Alloc(void)                                       /* 0x47C150 */
{
    gptr e = U32(G() + 0x93C);
    u8 i = 0;
    while (U8(e + 0x50) & 1) {
        e += 0x70; i++;
        if (e >= U32(G() + 0x93C) + 0x540) return 0;
    }
    memset(MEM(e), 0, 0x70);
    U8(e + 0x46) = i;
    U32(e + 0x50) = 1;
    return e;
}

/* point q inside the cone below p: half widths a (lo, hi halves) over the
 * heights b (lo, hi); p (x, -, z), q (x, y, -, r) */
u32 DC2CALL Cone_HitX(u32 a, u32 b, gptr p, gptr q)                /* 0x47C1A0 */
{
    s32 a0 = (s16)a, a1 = (s16)(a >> 16) + 1, b0 = (s16)b, b1 = (s16)(b >> 16);
    s32 px = S16(p), pz = S16(p + 4), q1 = S16(q + 2), q3 = S16(q + 6);
    if (a0 + pz > q1 + q3) return 0;
    if (q3 + pz + a1 < q1 - q3) return 0;
    s32 w = (q1 - pz) / ((a1 - a0) / (b1 - b0 + 1) + 1) + q3 + b0;
    s32 q0 = S16(q);
    if (q0 > w + px) return 0;
    return q0 >= px - w;
}

u32 DC2CALL Cone_HitZ(u32 a, u32 b, gptr p, gptr q)                /* 0x47C250 */
{
    s32 a0 = (s16)a, a1 = (s16)(a >> 16), b0 = (s16)b, b1 = (s16)(b >> 16);
    s32 w = (S16(q + 2) - S16(p + 4)) / ((a1 - a0 + 1) / (b1 - b0 + 1) + 1) + b0;
    s32 qz = S16(q + 4), py = S16(p + 2);
    if (qz > w + py) return 0;
    return qz >= py - w;
}

static s32 DC2CALL target_compare(gptr a, gptr b)                   /* 0x47BF30 */
{
    if (U16(a + 6)) { if (!U16(b + 6)) return 1; }
    else if (U16(b + 6)) return -1;
    if (U32(a) < U32(b)) return -1;
    return U32(a) != U32(b);
}
s32 DC2CALL Target_Compare(gptr a, gptr b) { return target_compare(a, b); }

/* objects with flag 8 within +-range of o's heading, nearest first (those
 * with +0x184 bit 0 last) -> extra block: +0 nearest dist2 (-1: none),
 * +4 count, +8 object indices, +0x14 nearest object */
void DC2CALL Target_Find(gptr o, u32 range)                        /* 0x47BDE0 */
{
    u32 n = 0;
    gptr rec = A_AIM_TABLE;
    gptr ob = U32(G() + 0x940) + 0x260;
    for (u32 i = 1; i < U32(0x87F940); i++, ob += 0x260) {
        u32 fl = U32(ob + 0x50);
        if (!(fl & 8) || (fl & 0x10000)) continue;
        u32 d = (U16(o + 0x4A) - (s32)(s16)Angle_XZ(o + 0x40, ob + 0x40) + range) & 0xFFF;
        if ((s32)d > (s32)(range * 2)) continue;
        U32(rec) = Dist2_XZ(o + 0x40, ob + 0x40);
        U16(rec + 4) = (u16)i;
        U16(rec + 6) = U8(ob + 0x184) & 1;
        n++; rec += 8;
    }
    Crt_qsort(A_AIM_TABLE, n, 8, 0x47BF30);
    for (u32 k = 0; k < n; k++) U8(PX() + 8 + k) = U8(A_AIM_TABLE + 4 + 8 * k);
    U32(PX()) = n ? U32(A_AIM_TABLE) : 0xFFFFFFFFu;
    U32(PX() + 4) = n;
    U32(PX() + 0x14) = n ? U32(G() + 0x940) + U16(A_AIM_TABLE + 4) * 0x260 : 0;
}

/* arm part `arm` (1 right, 4 left): its aim angles (extra +0x60 / +0x58)
 * turn back to rest */
void DC2CALL Arm_Relax(gptr o, u32 arm)                            /* 0x47C4C0 */
{
    gptr body = Obj_Part(U32(o + 0x64), 0), p = Obj_Part(U32(o + 0x64), arm & 0xFFFF);
    gptr a = (u16)arm == 1 ? PX() + 0x60 : PX() + 0x58;
    U16(a) += (u16)Angle_Approach(U16(a), 0x60, 0);
    U16(a + 2) += (u16)Angle_Approach(U16(a + 2), 0x60, 0);
    U16(p + 0x48) = (u16)(U16(a) - U16(body + 0x48));
    U16(p + 0x4C) = 0;
    U16(p + 0x4A) = (u16)(U16(a + 2) - U16(body + 0x4A));
}

/* arm `arm` aims at the nearest target in front of it (o is turned to the
 * arm's side for the search, its +0x40/+0x44/+0x4A restored by the caller) */
void DC2CALL Arm_Aim(gptr o, u32 arm, u32 yaw)                     /* 0x47C2C0 */
{
    gptr body = Obj_Part(U32(o + 0x64), 0), p = Obj_Part(U32(o + 0x64), arm & 0xFFFF);
    gptr a, slot;
    u32 ry;
    if ((u16)arm == 1) {
        a = PX() + 0x60; U16(o + 0x4A) = (u16)((yaw + 0x140) & 0xFFF); ry = yaw + 0x60;
        slot = PX() + 0x6C; U32(PX() + 0x74) = 0;
    } else {
        a = PX() + 0x58; U16(o + 0x4A) = (u16)((yaw - 0x140) & 0xFFF); ry = yaw - 0x60;
        slot = PX() + 0x68; U32(PX() + 0x70) = 0;
    }
    U32(o + 0x40) = U32(p + 0x40); U16(o + 0x44) = U16(p + 0x44);
    Target_Find(o, 0x180);
    gptr t = U32(PX() + 0x14);
    if (t) {
        if ((u16)arm == 1) U32(PX() + 0x74) = t; else U32(PX() + 0x70) = t;
        gptr ang = Tmp_Push(8);
        Angles_LookAt(Obj_Part(U32(t + 0x64), U8(U32(t + 0x134) + 0xA)) + 0x40, p + 0x40, ang);
        u16 ax = U16(ang);
        if ((s16)ax > 0x200) ax = 0x200;
        else if ((s16)ax < (s16)0xFE00) ax = 0xFE00;
        U16(a) += (u16)Angle_Approach(U16(a), 0x50, (u32)ax + ((u16)arm == 1 ? 0x33E : 0x37A));
        U16(a + 2) += (u16)Angle_Approach(U16(a + 2), 0x50, U32(ang + 2) - ry);
        Tmp_Pop(8);
    } else {
        U16(a) += (u16)Angle_Approach(U16(a), 0x60, (u16)arm == 1 ? 0x33E : 0x37A);
        U16(a + 2) += (u16)Angle_Approach(U16(a + 2), 0x60, (u16)arm == 1 ? 0xFFF6 : 0xA);
    }
    U32(slot) = U32(PX());
    U16(p + 0x48) = U16(a);
    U16(p + 0x4C) = 0;
    U16(p + 0x4A) = (u16)(U16(a + 2) - U16(body + 0x4A));
}

/* state 0: put the player at the room's entry point */
void DC2CALL Player_Init(gptr o)                                   /* 0x47D2F0 */
{
    U32(o + 0x50) = 3;
    gptr g = G();
    U16(o + 0x108) = (U8(g + 0x108F) & 1) ? U16(g + 0x11FC) : U16(g + 0x11F8);
    U16(o + 0x10A) = 0x4B0;
    U16(o + 0x40) = U16(o + 0xE0) = U16(G() + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    U8(o + 0x17B) = 0;
    U16(o + 0x17E) = U16(G() + 0x1200);
    U32(o + 0x12C) = 0x7380E0;
    U32(o + 0x134) = 0x7380F0;
    U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
    u16 set = U16(G() + 0x10B4);
    Stage_Load(set & 0xFF, set >> 8);
    Head_Reset(o);
    U8(o + 0x15F) = 7; U8(o + 0x160) = 0;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U16(o + 0x16A) = 0x100; U16(o + 0x16E) = 0x280;
    U8(o + 0x15D) = 0xFF; U8(o + 0x15E) = 0;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x300;
    U8(o + 0x15C) = 0;
    if (U8(G() + 0x108F) == 2) {
        U32(o + 0x128) = 0x7380D4;
        Obj_SetFeet(o, 0xA, 0xD);
        U8(o + 0x15F) = 0;
        Obj_Set118(o, 7, 0x1000, 0x1000);
        U8(o + 0x118) |= 0x20;
    } else {
        U32(o + 0x128) = 0x7380C8;
        Obj_SetFeet(o, 0xB, 0xE);
        Obj_Set118(o, 8, 0x1000, 0x1000);
    }
    U32(o + 0x188) = 0;
    U16(PX() + 0x3A) = 0x80;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

static void aim_target(gptr o, gptr t)
{
    U8(o + 0x15D) = U8(t + 0xFF);
    U32(o + 0x158) = 2;
    U8(o + 0x15E) = U8(U32(t + 0x134) + 0xA);
}

/* state 1: normal (substates +0x55, table 0x738118; in water 0x738148) */
void DC2CALL Player_Normal(gptr o)                                 /* 0x47D500 */
{
    Obj_SaveFeet(o);
    if (U8(G() + 0x108F) == 2) { Orig_Call1(U32(0x738148 + U8(o + 0x55) * 4), o); return; }
    if (U8(o + 0x55) <= 4 && !Flag_Check(2, 0xF)) {       /* look at the nearest target */
        Target_Find(o, 0x400);
        gptr t = U32(PX() + 0x14);
        if (t && U32(PX()) <= 0xF42400) aim_target(o, t);
    }
    Orig_Call1(U32(0x738118 + U8(o + 0x55) * 4), o);
    if ((U8(G() + 0x10B4) & 0xF) != 6) return;               /* two guns */
    u8 s = U8(o + 0x55);
    if (s != 5 && s != 6) {
        U16(PX() + 0x60) = 0; U16(PX() + 0x62) = 0; U16(PX() + 0x58) = 0; U16(PX() + 0x5A) = 0;
        return;
    }
    if (s == 5 && U8(o + 0x56) == 2) { Arm_Relax(o, 1); Arm_Relax(o, 4); return; }
    u16 yaw = U16(o + 0x4A), z = U16(o + 0x44);
    u32 xy = U32(o + 0x40);
    Arm_Aim(o, 1, yaw);
    Arm_Aim(o, 4, yaw);
    U32(o + 0x40) = xy; U16(o + 0x44) = z; U16(o + 0x4A) = yaw;
    gptr t = U32(PX() + 0x68) < U32(PX() + 0x6C) ? U32(PX() + 0x70) : U32(PX() + 0x74);
    if (t) aim_target(o, t);
}

/* state 2 (substates 0x738178), 5, 6: hand over to the room's code */
void DC2CALL Player_State2(gptr o)                                 /* 0x47DA60 */
{
    if (!U8(o + 0x56)) Player_Weapon(o);
    if (U8(G() + 0x108F) == 2) Flag_Set(1, 9, 0);
    Orig_Call1(U32(0x738178 + U8(o + 0x55) * 4), o);
}

void DC2CALL Player_State5(gptr o)                                 /* 0x47DBD0 */
{
    if ((U8(G() + 0x10B4) & 0xF) == 6) {
        U16(PX() + 0x60) = 0; U16(PX() + 0x62) = 0; U16(PX() + 0x58) = 0; U16(PX() + 0x5A) = 0;
    }
    Player_Weapon(o);
    if (U8(G() + 0x108F) == 2) Flag_Set(1, 9, 0);
    Orig_Call1(U32(G() + 0x92C), o);
}

void DC2CALL Player_State6(gptr o) { Orig_Call1(U32(G() + 0x930), o); }   /* 0x47DC50 */

/* ---- damage --------------------------------------------------------------------- */
/* damage d scaled by difficulty G+0x108E and the protection flags */
u32 DC2CALL Damage_Scale(u32 d)                                    /* 0x479700 */
{
    u32 v = d;
    if (U8(G() + 0x108E) == 2) {
        if (Flag_Check(2, 0x26)) v = d * 2;
        else if (Flag_Check(2, 0x27)) v = (u32)((s32)((d & 0xFFFF) * 5) / 2);
        else v = d * 3;
    }
    if (!U8(G() + 0x108E)) v = (u32)((s32)((v & 0xFFFF) * 2) / 3);
    u16 r = (u16)v;
    if (Flag_Check(9, 0x33)) r >>= 1;
    if (U8(G() + 0x927) == 1) {
        u8 c = U8(G() + 0x108F);
        if (c >= 7 || c == 4) r >>= 1;
    }
    return r;
}

/* 10 per button bit held in *0x87DC12 (shaking off a grab) */
u32 DC2CALL Pad_Shake(void)                                        /* 0x479D70 */
{
    u32 b = U16(0x87DC12), n = 0;
    for (int i = 0; i < 16; i++, b >>= 1) n += b & 1;
    return n * 10;
}

static void arms_rest(void)
{
    U16(PX() + 0x60) = 0; U16(PX() + 0x62) = 0; U16(PX() + 0x58) = 0; U16(PX() + 0x5A) = 0;
}

static void hit_recover(gptr o)
{
    Player_Weapon(o);
    U32(o + 0x50) &= 0xFFFFF6CF;
}

/* state 2, substates 0/1: light hit from direction +0x178 (+0x55 1: back) */
void DC2CALL Player_HitLight(gptr o)                               /* 0x47D6A0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        s32 dir = S16(o + 0x178);
        u8 back = U8(o + 0x55);
        u16 push = (u16)(s16)S8((back ? 0x7034C0 : 0x7034BC) + dir);
        if (dir & 1) { U16(o + 0xF0) = push; U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; }
        else { U32(o + 0xF0) = 0; U16(o + 0xF4) = push; }
        if (back) Anim_Start(o, U8(0x7034C4 + dir), 0, 0, 4);
        else Anim_Start(o, U8(0x7034C4 + dir) + 4u, 0, 0x40, 4);
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U8(o + 0x11B) = 0;
        U16(o + 0x174) = 1;
        U8(o + 0x56)++;
        U8(PX() + 0x2E) = 0;
        Se_Play(o, 3);
        arms_rest();
        if (S16(o + 0x108) < 0) U32(o + 0x54) = 3;
        break; }
    case 1:
        if (Anim_Step(o)) {
            hit_recover(o);
            U16(o + 0x174) = 0;
            U32(o + 0x1DC) = 8;
            U32(o + 0x54) = 1;
        } else if (U16(o + 0x92) > 7) { U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; }
        break;
    }
}

/* state 2, substates 2/3: knocked down (3: forward), get up by shaking */
void DC2CALL Player_HitDown(gptr o)                                /* 0x47D820 */
{
    u8 fwd = (u8)(U8(o + 0x55) - 2);
    switch (U8(o + 0x56)) {
    case 0:
        U32(o + 0xF0) = 0;
        if (fwd) { U16(o + 0xF4) = 0xFF80; Anim_Start(o, 0xB, 0, 0, 4); }
        else { U16(o + 0xF4) = 0x80; Anim_Start(o, 0xA, 0, 0, 4); }
        Se_Play(o, 3);
        U8(o + 0x11B) = 0;
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U32(o + 0x50) &= ~0x20u;
        U16(o + 0x174) = 1;
        U8(o + 0x56)++;
        U16(PX() + 0x28) = (u16)(U8(o + 0x57) * 4);
        U16(PX() + 0x28) += 0x32;
        arms_rest();
        if (S16(o + 0x108) < 0) U32(o + 0x54) = 3;
        break;
    case 1:
        if (Anim_Step(o)) {
            Anim_Start(o, fwd ? 0x11 : 0x12, 0, 1, 4);
            Player_Weapon(o);
            U8(o + 0x174) |= 2;
            U16(o + 0xF4) = 0;
            U8(o + 0x56)++;
        }
        break;
    case 2:
        if (U8(G() + 0x108F) == 2 && -1000 * S16(o + 0x5A) != S16(o + 0x42)) break;
        U16(PX() + 0x28) -= (u16)Pad_Shake();
        U16(PX() + 0x28) -= 2;
        if (S16(PX() + 0x28) > 0) { Anim_Step(o); break; }
        U16(o + 0x174) &= 0xFFFD;
        Player_Weapon(o);
        Anim_Start(o, fwd ? 0xD : 0xC, 0, 0, 4);
        U8(o + 0x56)++;
        break;
    case 3:
        if (Anim_Step(o)) {
            hit_recover(o);
            U16(o + 0x174) = 0;
            U8(o + 0x17B) = 0xA;
            U32(o + 0x1DC) = 8;
            U32(o + 0x54) = 1;
        }
        break;
    }
}

/* state 3: dying */
void DC2CALL Player_Die(gptr o)                                    /* 0x47DAB0 */
{
    switch (U8(o + 0x55)) {
    case 0: {
        Anim_Start(o, 8, 0, 0, 0);
        u8 s = (u8)(U8(o + 0x55) + 1);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x55) = s;
        GameOver_Start(0);
        break; }
    case 1:
        Anim_Step(o);
        if (U8(G() + 0x108F) == 2) {                /* sinks to the floor */
            s32 lim = -1000 * floor_layer(o);
            if (lim > S16(o + 0x42)) U16(o + 0x42) += 0x64;
            if (lim < S16(o + 0x42)) U16(o + 0x42) = (u16)lim;
            U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
        }
        break;
    }
}

/* state 4: driven by the room (table 0x737A5C) */
void DC2CALL Player_Scripted(gptr o) { Orig_Call1(U32(0x737A5C + U8(o + 0x55) * 4), o); }  /* 0x4796C0 */

void DC2CALL Obj_Stop(gptr o)                                      /* 0x4796E0 */
{
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U8(o + 0x55) = 0;
}

/* ---- inventory, walls ahead ---------------------------------------------------- */
/* index of the entry (12 bytes, +0 used, +1 item) for item id, 0xFF if none */
u32 DC2CALL Item_Find(gptr list, u32 count, u32 id)                /* 0x495CB0 */
{
    for (u8 i = 0; i < (u8)count; i++, list += 12)
        if (U8(list) && U8(list + 1) == (u8)id) return i;
    return 0xFF;
}

/* n if the current sub-weapon has n rounds left (or unlimited), else 0 */
u32 DC2CALL Ammo_Have(u32 n)                                       /* 0x496240 */
{
    if (U8(G() + 0x1094) & 1) return n & 0xFFFF;
    u8 i = (u8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B5) | 0x10);
    s32 have = S16(G() + i * 12 + 0x10C4);
    return have < (s32)(n & 0xFFFF) ? 0 : n & 0xFFFF;
}

/* a held throw ends (effects in flight drop) or the sub-weapon is empty */
u32 DC2CALL Player_SubDone(void)                                   /* 0x47C860 */
{
    u8 sub = U8(G() + 0x10B5);
    if (U8(PX() + 0x78)) {
        for (gptr e = U32(G() + 0x93C); e < U32(G() + 0x93C) + 0x540; e += 0x70)
            if (U8(e + 0x4F) == 3) U8(e + 0x4F) = 0x10;
        U8(PX() + 0x78) = 0;
        return 1;
    }
    if ((sub == 3 || sub == 4 || sub == 6) && !Ammo_Have(1)) return 1;
    return 0;
}

/* a wall edge (flags bits 0 and 7) dist ahead of o that o faces;
 * edge flags into o+0x11A */
u32 DC2CALL Wall_Ahead(gptr o, u32 dist)                           /* 0x479DA0 */
{
    u8 mask = o != U32(G() + 0x940) ? 0x40 : 0x80;
    if (U32(o + 0x50) & 0x1000) mask = 0x20;
    gptr t = Tmp_Push(0x30), m = t, v = t + 0x20, s = t + 0x28;
    U32(v) = 0; U16(v + 4) = (u16)dist;
    RotMatrix(o + 0x48, m);
    ApplyMatrixSV(m, v, v);
    U16(s) = U16(o + 0x40); U16(s + 2) = U16(o + 0x44);
    U16(s + 4) = (u16)(U16(o + 0x40) + U16(v)); U16(s + 6) = (u16)(U16(o + 0x44) + U16(v + 4));
    u32 ret = 0;
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)) && !ret; k++) {
        gptr coll = U32(G() + 0xA4C), poly = coll + U32(coll + 8 + 4 * k);
        if (!(((u32)U16(poly + 4) >> (U8(o + 0x5A) & 31)) & 1)) continue;
        if (!(U8(poly) & 1) || (U8(poly + 3) & mask)) continue;
        gptr ef = poly + 8, vtx = poly + 8 + U8(poly + 1) * 4;
        for (u32 j = 0; j < U8(poly + 1); j++, ef += 4, vtx += 4) {
            u8 fl = U8(ef);
            if (!(fl & 1) || !(fl & 0x80)) continue;
            if ((fl & 8) && mask == 0x20) continue;
            if (!Seg_Intersect(vtx, s)) continue;
            u32 ux = (u32)(S16(s) - S16(vtx)), uz = (u32)(S16(s + 2) - S16(vtx + 2));
            u32 wx = (u32)(S16(vtx + 4) - S16(vtx)), wz = (u32)(S16(vtx + 6) - S16(vtx + 2));
            if ((s32)(wx * uz - ux * wz) < 0) continue;
            if (mask == 0x80 && !(U8(ef) & 0x44)) continue;
            U8(o + 0x11A) = U8(ef);
            ret = 1;
            break;
        }
    }
    Tmp_Pop(0x30);
    return ret;
}

/* Two probes (left/right of o by w, `len` ahead along heading yaw) cross a
 * ledge edge (flags bits 0, 7) and no plain wall of the floor two layers up
 * or down (edge bit 6: down); then both probes find the same floor there,
 * other than o's height: a step to climb or drop. Ledge flags -> o+0x11A.
 * The original keeps the probes in its stack frame; F*() mirror it. */
#define FB(x)  U8(fr + (x))
#define FW(x)  U16(fr + (x))
#define FD(x)  U32(fr + (x))

static s32 cross_y(gptr t, gptr e)          /* (t0 - e0) x (e1 - e0), y */
{
    u32 ux = (u32)(S16(t) - S16(e)), uz = (u32)(S16(t + 2) - S16(e + 2));
    u32 wx = (u32)(S16(e + 4) - S16(e)), wz = (u32)(S16(e + 6) - S16(e + 2));
    return (s32)(wx * uz - ux * wz);
}

u32 DC2CALL Step_Ahead(gptr o, u32 len, u32 w, u32 yaw)           /* 0x479FC0 */
{
    gptr fr = Tmp_Push(0xB0);
    U8(o + 0x11A) = 0;
    FD(0x18) = 0;
    u8 mask = o != U32(G() + 0x940) ? 0x40 : 0x80;
    if (U32(o + 0x50) & 0x1000) mask = 0x20;
    FD(0x3C) = mask;
    /* forward vector (len + w) and the two side offsets */
    FW(0x10) = U16(o + 0x48); FW(0x12) = (u16)yaw; FW(0x14) = U16(o + 0x4C);
    RotMatrix(fr + 0x10, fr + 0x8C);
    FW(0x10) = 0; FW(0x12) = 0; FW(0x14) = (u16)(len + w);
    ApplyMatrixSV(fr + 0x8C, fr + 0x10, fr + 0x5C);
    FW(0x10) = U16(o + 0x48); FW(0x12) = (u16)(yaw + 0x400); FW(0x14) = U16(o + 0x4C);
    RotMatrix(fr + 0x10, fr + 0x8C);
    FW(0x10) = 0; FW(0x12) = 0; FW(0x14) = (u16)w;
    ApplyMatrixSV(fr + 0x8C, fr + 0x10, fr + 0x10);
    FW(0x74) = (u16)(U16(o + 0x40) + FW(0x10)); FW(0x76) = (u16)(U16(o + 0x44) + FW(0x14));
    FW(0x78) = (u16)(U16(o + 0x40) + FW(0x5C) + FW(0x10)); FW(0x7A) = (u16)(U16(o + 0x44) + FW(0x60) + FW(0x14));
    FW(0x10) = U16(o + 0x48); FW(0x12) = (u16)(yaw - 0x400); FW(0x14) = U16(o + 0x4C);
    RotMatrix(fr + 0x10, fr + 0x8C);
    FW(0x10) = 0; FW(0x12) = 0; FW(0x14) = (u16)w;
    ApplyMatrixSV(fr + 0x8C, fr + 0x10, fr + 0x10);
    FW(0x7C) = (u16)(U16(o + 0x40) + FW(0x10)); FW(0x7E) = (u16)(U16(o + 0x44) + FW(0x14));
    FW(0x80) = (u16)(U16(o + 0x40) + FW(0x5C) + FW(0x10)); FW(0x82) = (u16)(U16(o + 0x44) + FW(0x60) + FW(0x14));
    u32 ret = 0;
    /* a ledge both probes cross from its outer side */
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)) && !U8(o + 0x11A); k++) {
        gptr coll = U32(G() + 0xA4C), poly = coll + U32(coll + 8 + 4 * k);
        if (!(U8(poly) & 1)) continue;
        if (!(((u32)U16(poly + 4) >> (U8(o + 0x5A) & 31)) & 1)) continue;
        if (mask & U8(poly + 3)) continue;
        FD(0x18) = poly + 8;
        gptr vtx = poly + 8 + U8(poly + 1) * 4;
        for (u32 j = 0; j < U8(poly + 1); j++, FD(0x18) += 4, vtx += 4) {
            FD(0x2C) = U32(vtx); FD(0x30) = U32(vtx + 4);
            u8 fl = U8(FD(0x18));
            if (!(fl & 1) || !(fl & 0x80)) continue;
            for (u32 s = 0; s < 2; s++) {
                FD(0x24) = FD(0x74 + 8 * s); FD(0x28) = FD(0x78 + 8 * s);
                if (!Seg_Intersect(fr + 0x2C, fr + 0x24)) break;
                if (cross_y(fr + 0x24, fr + 0x2C) < 0) break;
                if (mask == 0x80 && !(U8(FD(0x18)) & 0x44)) break;
                if (s == 1) { U8(o + 0x11A) = U8(FD(0x18)); break; }
            }
            if (U8(o + 0x11A)) break;
        }
    }
    if (!U8(o + 0x11A)) goto out;
    /* nothing in the way on the other floor */
    s32 layer = (U8(FD(0x18)) & 0x40) ? S16(o + 0x5A) - 2 : S16(o + 0x5A) + 2;
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)); k++) {
        gptr coll = U32(G() + 0xA4C), poly = coll + U32(coll + 8 + 4 * k);
        if (!(U8(poly) & 1)) continue;
        if (!(((u32)U16(poly + 4) >> (layer & 31)) & 1)) continue;
        if (mask & U8(poly + 3)) continue;
        gptr ef = poly + 8, vtx = poly + 8 + U8(poly + 1) * 4;
        for (u32 j = 0; j < U8(poly + 1); j++, ef += 4, vtx += 4) {
            FD(0x2C) = U32(vtx); FD(0x30) = U32(vtx + 4);
            u8 fl = U8(ef);
            if (!(fl & 1) || (fl & 0x80)) continue;
            for (u32 s = 0; s < 2; s++) {
                FD(0x24) = FD(0x74 + 8 * s); FD(0x28) = FD(0x78 + 8 * s);
                if (Seg_Intersect(fr + 0x2C, fr + 0x24) && cross_y(fr + 0x24, fr + 0x2C) >= 0) goto out;
            }
        }
    }
    /* the same floor under both probes' ends, not o's height */
    for (u32 s = 0; s < 2; s++) {
        FW(0x10) = FW(0x78 + 8 * s); FW(0x14) = FW(0x7A + 8 * s);
        FW(0x12) = (u16)(-1000 * layer);
        FW(0x54 + 2 * s) = (u16)Floor_Height(fr + 0x10, fr + 0x10, mask, 0);
    }
    if (FW(0x54) == FW(0x56) && FW(0x54) != U16(o + 0x42)) ret = 1;
out:
    Tmp_Pop(0xB0);
    return ret;
}

/* ---- normal substates --------------------------------------------------------- */
#define PAD_HELD  U32(0x87DC08)
#define PAD_PRESS U32(0x87DC94)

/* standing: idle animation, the occasional look around (+0x56 2..5); the
 * buttons pick the next substate (+0x54 = state | sub << 8 | ...) */
void DC2CALL Player_Stand(gptr o)                                  /* 0x47DC70 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0x16, U16(PX() + 0x1E), 9, U16(PX() + 0x1C));
        { u8 s = (u8)(U8(o + 0x56) + 1); U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U8(o + 0x56) = s; }
        U16(PX() + 0x38) = 0x200;
        break;
    case 1:
        Anim_Step(o);
        if (Flag_Check(2, 0xF)) break;
        if (S16(PX() + 0x38) < 0) break;
        if (!S16(PX() + 0x38)) U8(o + 0x56)++;
        else U16(PX() + 0x38)--;
        break;
    case 2:
        Anim_Start(o, 0x3A, 0, 0x40, 8);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(PX() + 0x38) = 0xFFFF;
        U8(o + 0x56)++;
        break;
    case 3:
        if (Anim_Step(o)) { Anim_Start(o, 0x3B, 0, 9, 8); U8(o + 0x56)++; }
        break;
    case 4:
        Anim_Step(o);
        if (PAD_HELD & 0xF07E) {
            Anim_Start(o, 0x3C, 0, 0x40, 8);
            U16(PX() + 0x38) = 0x200;
            U8(o + 0x56)++;
        }
        break;
    case 5:
        if (Anim_Step(o)) { Anim_Start(o, 0x16, 0, 9, 8); U8(o + 0x56) = 1; }
        U16(PX() + 0x38) = 0x200;
        break;
    case 0xFF:
        Anim_Start(o, 0x16, U16(PX() + 0x1E), 0x109, U16(PX() + 0x1C));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56) = 1;
        U16(PX() + 0x38) = 0x200;
        break;
    }
    if (U8(0x87DC08) & 0x20) {                      /* action: a step ahead? */
        if (Wall_Ahead(o, 0x280) && Step_Ahead(o, 0x3E8, 0xC8, (U8(o + 0x11A) & 0x30) << 6)) {
            U8(o + 0x174) |= 1;
            U8(o + 0x11B) = 0;
            U32(o + 0x1DC) = 5;
            U32(o + 0x54) = 0x801;
            return;
        }
        Flag_Set(2, 0xD, 1);
    }
    u8 pr = U8(0x87DC94);
    if (pr & 0x40) {                                /* quick turn / dodge */
        if (Flag_Check(2, 0x1C)) {
            u8 h = (u8)(PAD_HELD >> 8);
            u32 st = 0;
            if (h & 0x40) st = (h & 0x20) ? 0x30B01 : (h & 0x80) ? 0x40B01 : 0xB01;
            else if (h & 0x20) st = 0x10B01;
            else if (h & 0x80) st = 0x20B01;
            if (st) {
                U32(o + 0x54) = st;
                Flag_Set(2, 0xD, 0);
                U8(o + 0x11B) = 0;
                return;
            }
        }
        pr = U8(0x87DC94);
    }
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {   /* sub-weapon */
        if (Player_SubDone()) return;
        Flag_Set(2, 0xD, 0);
        U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 0x20) { Flag_Set(2, 1, 1); return; }
    if (pr & 2) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    u32 held = PAD_HELD;
    if ((held & 8) && (U8(G() + 0x10B4) & 0xF)) { Flag_Set(2, 0xD, 0); U32(o + 0x54) = 0x501; return; }
    if (held & 0x1000) {                            /* forward */
        Flag_Set(2, 0xD, 0);
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 4;
        U32(o + 0x54) = (U8(G() + 0x10B4) & 0xF) == 8 ? 0x201 : 0xFF0201;
        return;
    }
    if (held & 0x4000) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 8; U32(o + 0x54) = 0x401; return; }
    if (held & 0xA000) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x301; }
}

/* action button: climb/drop a step ahead (state 8); 1 if taken */
static int try_step(gptr o)
{
    if (!(U8(0x87DC08) & 0x20)) return 0;
    if (Wall_Ahead(o, 0x280) && Step_Ahead(o, 0x3E8, 0xC8, (U8(o + 0x11A) & 0x30) << 6)) {
        U8(o + 0x174) |= 1;
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 5;
        U32(o + 0x54) = 0x801;
        return 1;
    }
    Flag_Set(2, 0xD, 1);
    return 0;
}

/* quick turn / dodge (state 0xB with a direction); 1 if taken */
static int try_dodge(gptr o)
{
    if (!(U8(0x87DC94) & 0x40) || !Flag_Check(2, 0x1C)) return 0;
    u8 h = (u8)(PAD_HELD >> 8);
    u32 st = 0;
    if (h & 0x40) st = (h & 0x20) ? 0x30B01 : (h & 0x80) ? 0x40B01 : 0xB01;
    else if (h & 0x20) st = 0x10B01;
    else if (h & 0x80) st = 0x20B01;
    if (!st) return 0;
    U32(o + 0x54) = st;
    Flag_Set(2, 0xD, 0);
    U8(o + 0x11B) = 0;
    return 1;
}
/* ---- in water (table 0x738148): standing and wading; the dodge button crouches ---- */
/* action: a step up ahead (state 8) */
static int water_step(gptr o)
{
    if (!(U8(0x87DC08) & 0x20)) return 0;
    if (Wall_Ahead(o, 0x280) && Step_Ahead(o, 0x3E8, 0xC8, (U8(o + 0x11A) & 0x30) << 6)) {
        U8(o + 0x174) |= 1;
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 5;
        U32(o + 0x54) = 0x801;
        return 1;
    }
    Flag_Set(2, 0xD, 1);
    return 0;
}

/* the buttons common to both: crouch, sub-weapon, action flag, quick turn;
 * 1 if one was taken */
static int water_buttons(gptr o)
{
    u8 pr = U8(0x87DC94);
    if ((pr & 0x40) && !PcIn_ByEscape()) {
        Flag_Set(2, 0xD, 0);
        Flag_Set(1, 9, 1);
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x901;
        return 1;
    }
    pr = U8(0x87DC94);
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) { Flag_Set(2, 0xD, 0); U32(o + 0x54) = 0xA01; }
        return 1;
    }
    if (pr & 0x20) { Flag_Set(2, 1, 1); return 1; }
    if (pr & 2) {
        Flag_Set(2, 0xD, 0);
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701;
        return 1;
    }
    return 0;
}

void DC2CALL Player_WaterStand(gptr o)                             /* 0x47E050 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0x16, U16(PX() + 0x1E), 1, U16(PX() + 0x1C));
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1: Anim_Step(o); break;
    }
    if (water_step(o) || water_buttons(o)) return;
    u32 h = PAD_HELD;
    if ((h & 8) && (U8(G() + 0x10B4) & 0xF)) { Flag_Set(2, 0xD, 0); U32(o + 0x54) = 0x501; return; }
    if (h & 0x1000) {
        Flag_Set(2, 0xD, 0);
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x201;
        return;
    }
    if (h & 0x4000) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 8; U32(o + 0x54) = 0x401; return; }
    if (h & 0xA000) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x301; }
}

void DC2CALL Player_WaterWalk(gptr o)                              /* 0x47E640 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0x1D, U16(PX() + 0x1E), 1, U16(PX() + 0x1C));
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x60;
        break;
    case 1: U16(o + 0xF4) = 0x60; Anim_Step(o); break;
    }
    if (water_step(o)) return;
    if (PAD_HELD & 0x2000) U16(o + 0x4A) += 0x32;
    if (PAD_HELD & 0x8000) U16(o + 0x4A) -= 0x32;
    if (water_buttons(o)) return;
    u32 h = PAD_HELD;
    if ((h & 8) && (U8(G() + 0x10B4) & 0xF)) {          /* aim while wading */
        Flag_Set(2, 0xD, 0);
        U32(o + 0xF0) = 0; U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        Anim_Start(o, 0x25, U16(o + 0x92), 1, 4);
        U32(o + 0x54) = 0x1050501;
        return;
    }
    if (h & 0x4000) { U16(o + 0xF4) = 0; return; }
    if (!(h & 0x1000)) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 5; U32(o + 0x54) = 1; }
}


/* walking forward; turning; aim, sub-weapon, stop */
void DC2CALL Player_Walk(gptr o)                                   /* 0x47E290 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0x1D, U16(PX() + 0x1E), 1, U16(PX() + 0x1C));
        u8 s = (u8)(U8(o + 0x56) + 1);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x82;
        U8(o + 0x56) = s;
        break; }
    case 1: Anim_Step(o); break;
    case 0xFF:
        Anim_Start(o, 0x1D, U16(PX() + 0x1E), 0x101, U16(PX() + 0x1C));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x82;
        U8(o + 0x56) = 1;
        break;
    }
    if (try_step(o) || try_dodge(o)) return;
    u32 held = PAD_HELD;
    if (held & 0x2000) U16(o + 0x4A) += 0x32;
    if (held & 0x8000) U16(o + 0x4A) -= 0x32;
    u8 pr = U8(0x87DC94);
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (Player_SubDone()) return;
        Flag_Set(2, 0xD, 0);
        U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 0x20) { Flag_Set(2, 1, 1); return; }
    if (pr & 2) { Flag_Set(2, 0xD, 0); U8(o + 0x11B) = 0; U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    u16 w = U16(G() + 0x10B4) & 0xF;
    if ((held & 8) && w) {                          /* aim while walking */
        Flag_Set(2, 0xD, 0);
        U32(o + 0xF0) = 0;
        U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        u32 fr = (u32)((s32)(U16(o + 0x92) << 5) / 20) & 31;
        if (w < 5 && w != 3) Anim_Start(o, 0x23, U16(o + 0x92), 1, 4);
        else if (w == 6) { Anim_Start(o, 0x23, U16(o + 0x92), 1, 4); Arm_Relax(o, 1); Arm_Relax(o, 4); }
        else Anim_Start(o, 0x25, fr, 1, 8);
        U16(PX() + 0x3C) = (u16)(s16)S8(0x7034AC + w);
        U8(PX() + 0x2F) = 0;
        U32(o + 0x54) = 0x1050501;
        return;
    }
    if (held & 0x4000) { U16(o + 0xF4) = 0; return; }
    if (held & 0x1000) return;
    Flag_Set(2, 0xD, 0);                             /* released: stand */
    U32(o + 0x1DC) = 6;
    u8 ww = U8(G() + 0x10B4) & 0xF;
    U32(o + 0x54) = (ww == 6 || ww == 8) ? 1 : 0xFF0001;
}

/* quick 180-degree turn */
void DC2CALL Player_TurnBack(gptr o)                               /* 0x47EED0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0x1A, 0, 1, 3);
        u16 yaw = U16(o + 0x4A);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(PX() + 0x22) = yaw;
        U16(PX() + 0x20) = (u16)(U16(o + 0x4A) + 0x800);
        u8 s = (u8)(U8(o + 0x56) + 1);
        U8(o + 0x11B) = 0;
        U8(o + 0x56) = s;
        break; }
    case 1:
        Anim_Step(o);
        U16(PX() + 0x22) += 0x12C;
        U16(o + 0x4A) = U16(PX() + 0x22) & 0xFFF;
        if (S16(PX() + 0x22) >= S16(PX() + 0x20)) {
            U16(o + 0x4A) = U16(PX() + 0x20);
            U32(o + 0x1DC) = 5;
            U32(o + 0x54) = 1;
        }
        break;
    }
}

void DC2CALL Obj_ShadowOff(gptr o) { U8(o + 0x118) &= 0x7F; }      /* 0x48F8B0 */

/* climbing up / dropping down a step (state 8): turn to the ledge, climb
 * animation, then the new floor layer */
void DC2CALL Player_Climb(gptr o)                                  /* 0x47EF90 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0x1A, 0, 1, 3);
        u8 dir = U8(o + 0x11A) & 0x30;
        U8(o + 0x174) |= 1;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x11B) = 0;
        U32(o + 0x50) |= 0x800;
        U16(PX() + 0x20) = (u16)(dir << 6);
        U8(o + 0x56)++;
        break; }
    case 1: {
        Anim_Step(o);
        u16 r = (u16)Angle_Approach(U16(o + 0x4A), 0x40, U16(PX() + 0x20));
        U16(o + 0x4A) += r;
        if (r) break;
        if (U8(o + 0x11A) & 0x40) { U16(o + 0x5A) -= 2; Anim_Start(o, 0x14, 0, 0x40, 0); }
        else { U16(o + 0x5A) += 2; Anim_Start(o, 0x13, 0, 0x40, 0); }
        U32(o + 0x50) |= 0x30;
        Obj_ShadowOff(o);
        U8(o + 0x56)++;
        break; }
    case 2:
        if (Anim_Step(o)) {
            U16(o + 0x174) &= 0xFFFE;
            U32(o + 0x50) &= 0xFFFFF7CF;
            U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18u);
            Obj_Set118(o, 8, 0x1000, 0x1000);
            U32(o + 0x1DC) = 4;
            U32(o + 0x54) = 1;
        }
        break;
    }
}

/* pushing / bumping (substates 0-4 pick the animation; 6/7 slide along the
 * animation's offsets 0x7034C8) */
void DC2CALL Player_Push(gptr o)                                   /* 0x47FC70 */
{
    u8 s = U8(o + 0x56);
    if (s < 5) { U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U8(o + 0x11B) = 0; }
    switch (s) {
    case 0: Anim_Start(o, 0x30, 0, 0x40, 2); U8(o + 0x56) = 5; return;
    case 1: Anim_Start(o, 0x31, 0, 0x40, 2); U8(o + 0x56) = 5; return;
    case 2: Anim_Start(o, 0x32, 0, 0x40, 2); U8(o + 0x56) = 5; return;
    case 3: Anim_Start(o, 0x30, 0, 0, 2); U8(o + 0x56) = 7; return;
    case 4: Anim_Start(o, 0x30, 0, 0, 2); U8(o + 0x56) = 6; return;
    case 5: if (!Anim_Step(o)) return; break;
    case 6: case 7: {
        u16 f = U16(o + 0x92);
        if (f) {
            gptr t = Tmp_Push(0x28), v = t, m = t + 8;
            u16 d = (u16)(U16(0x7034C8 + 2 * f) - U16(0x7034C6 + 2 * f));
            U16(v) = s == 7 ? (u16)-d : d; U16(v + 2) = 0; U16(v + 4) = d;
            RotMatrix(o + 0x48, m);
            ApplyMatrixSV(m, v, v);
            U16(o + 0x40) += U16(v); U16(o + 0x44) += U16(v + 4);
            Tmp_Pop(0x28);
        }
        if (!Anim_Step(o)) return;
        break; }
    default: return;
    }
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U32(o + 0x1DC) = 8;
    U32(o + 0x54) = 1;
}

/* in water: dive (state 9); else the dodge; then the action button's step.
 * Shared by turning in place and walking backwards; 1 if a state was taken */
static int turn_common(gptr o)
{
    u8 pr = U8(0x87DC94);
    if (U8(G() + 0x108F) == 2) {
        if ((pr & 0x40) && !PcIn_ByEscape()) {
            Flag_Set(2, 0xD, 0);
            Flag_Set(1, 9, 1);
            U8(o + 0x11B) = 0;
            U32(o + 0x1DC) = 4;
            U32(o + 0x54) = 0x901;
            return 1;
        }
    } else if (try_dodge(o)) return 1;
    return try_step(o);
}

static int sub_weapon(gptr o)
{
    if ((U8(0x87DC94) & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) { Flag_Set(2, 0xD, 0); U32(o + 0x54) = 0xA01; }
        return 1;
    }
    return 0;
}

/* turning in place */
void DC2CALL Player_Turn(gptr o)                                   /* 0x47E890 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0x1A, U16(PX() + 0x1E), 1, 0xA);
        u8 s = (u8)(U8(o + 0x56) + 1);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U8(o + 0x11B) = 0;
        U8(o + 0x56) = s;
        break; }
    case 1: Anim_Step(o); break;
    }
    if (PAD_HELD & 0x2000) U16(o + 0x4A) += 0x4B;
    if (PAD_HELD & 0x8000) U16(o + 0x4A) -= 0x4B;
    if (turn_common(o) || sub_weapon(o)) return;
    u8 pr = U8(0x87DC94);
    u32 held = PAD_HELD;
    if (pr & 0x20) { Flag_Set(2, 1, 1); return; }
    if (pr & 2) { Flag_Set(2, 0xD, 0); U8(o + 0x11B) = 0; U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    if (held & 0x1000) {
        Flag_Set(2, 0xD, 0);
        U8(o + 0x11B) = 0;
        U32(o + 0x1DC) = 2;
        U32(o + 0x54) = (U8(G() + 0x108F) != 2 && !(U8(G() + 0x10B4) & 0xF)) ? 0xFF0201 : 0x201;
        return;
    }
    if (held & 0x4000) { Flag_Set(2, 0xD, 0); U8(o + 0x11B) = 0; U32(o + 0x1DC) = 8; U32(o + 0x54) = 0x401; return; }
    if ((held & 8) && (U8(G() + 0x10B4) & 0xF)) { Flag_Set(2, 0xD, 0); U32(o + 0x54) = 0x501; return; }
    if (!(held & 0xA000)) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 5; U32(o + 0x54) = 1; }
}

/* walking backwards */
void DC2CALL Player_Back(gptr o)                                   /* 0x47EBB0 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0x20, 4, 1, 6);
        u8 s = (u8)(U8(o + 0x56) + 1);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U8(o + 0x11B) = 0;
        U8(o + 0x56) = s;
        break; }
    case 1: Anim_Step(o); U16(o + 0xF4) = 0xFFD8; break;
    }
    if (PAD_HELD & 0x2000) U16(o + 0x4A) += 0x20;
    if (PAD_HELD & 0x8000) U16(o + 0x4A) -= 0x20;
    if (turn_common(o) || sub_weapon(o)) return;
    u8 pr = U8(0x87DC94);
    u32 held = PAD_HELD;
    if (pr & 0x20) { Flag_Set(2, 0xD, 1); return; }
    if (pr & 2) { Flag_Set(2, 0xD, 0); U8(o + 0x11B) = 0; U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    if (held & 0x1000) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x201; return; }
    u16 w = U16(G() + 0x10B4) & 0xF;
    if ((held & 8) && w) {                          /* aim walking backwards */
        Anim_Start(o, 0x24, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFD2;
        U16(PX() + 0x30) = 0x800;
        U32(o + 0x54) = 0x1060501;
        U16(PX() + 0x3C) = (u16)(s16)S8(0x7034AC + w);
        U8(PX() + 0x2F) = 0;
        return;
    }
    if (!(held & 0x4000)) { Flag_Set(2, 0xD, 0); U32(o + 0x1DC) = 5; U32(o + 0x54) = 1; }
}

/* set-up of the main weapon (G+0x10B4 low byte) and the sub-weapon */
void DC2CALL Player_LoadWeapons(gptr o)                            /* 0x47ADA0 */
{
    Orig_Call1(U32(0x737FA0 + U8(G() + 0x10B4) * 4), o);
    Orig_Call1(U32(0x737FF0 + (U16(G() + 0x10B4) >> 8) * 4), o);
}

/* turn angle cur toward target by at most step; the change (cur is signed,
 * target unsigned 16-bit, as the original compares them) */
u32 DC2CALL Angle_Step(u32 cur, u32 step, u32 target)              /* 0x47CAE0 */
{
    s32 c = (s16)cur, t = target & 0xFFFF, s = (s16)step;
    if (c == t) return 0;
    if (c < t) { s32 d = t - c; return (u32)(s < d ? s : d); }
    s32 d = c - t;
    if (s < d) d = s;
    return (u32)-d;
}

/* substate 5: the weapon's own handler (0x738018 by weapon) */
void DC2CALL Player_WeaponState(gptr o)                            /* 0x47ADE0 */
{
    Orig_Call1(U32(0x738018 + (U8(G() + 0x10B4) & 0xF) * 4), o);
}

/* substate 10: the sub-weapon's handler (0x738078 by sub-weapon) */
void DC2CALL Player_SubState(gptr o)                               /* 0x47AF20 */
{
    Orig_Call1(U32(0x738078 + (U16(G() + 0x10B4) >> 8) * 4), o);
}

/* substate 6, aiming: pitch the upper body (+0x30 of the player block)
 * toward the target found, then the weapon's aim handler (0x738048) */
void DC2CALL Player_Aim(gptr o)                                    /* 0x47AE00 */
{
    Target_Find(o, 0x200);
    gptr t = U32(PX() + 0x14);
    u32 want = 0x800;
    if (t) {
        gptr sv = Tmp_Push(0x28), ang = sv + 8, mm = sv + 0x10;
        gptr p = Obj_Part(U32(t + 0x64), U8(U32(t + 0x134) + 0xA));
        U16(sv) = 0x16; U16(sv + 2) = 0; U16(sv + 4) = 0xFF22;   /* the gun, from the root */
        RotMatrix(o + 0x48, mm);
        ApplyMatrixSV(mm, sv, sv);
        U16(sv) += U16(o + 0x40);
        U16(sv + 4) += U16(o + 0x44);
        U16(sv + 2) += (u16)(U16(o + 0x42) - 0x3E8);
        Angles_LookAt(p + 0x40, sv, ang);
        u32 a = U32(ang);
        s16 x = (s16)a;
        want = x > 0x200 ? 0x1000 : x < -0x200 ? 0 : a * 4 + 0x800;
        Tmp_Pop(0x28);
    }
    U16(PX() + 0x30) += (u16)Angle_Step(U16(PX() + 0x30), 0x180, want);
    Orig_Call1(U32(0x738048 + (U8(G() + 0x10B4) & 0xF) * 4), o);
}
