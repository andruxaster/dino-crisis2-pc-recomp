/* Area objects of areas 4 and 8 (handlers 0x490670, 0x4912F0, 0x4918D0).
 *
 * Type 20 of area 8 (0x490670): a creature that keeps to its place o+0xD8.
 * It turns to follow the player (anim 1 while turning, 0 still). Its
 * parameter o+0x10C (from o+0x1C0) picks how it reacts. With 0 it goes back
 * home once the player is 2500 away and comes back to watching at 2000.
 * Otherwise it walks home while the player is near (under 2500) and stops
 * at 5000 or when the player is within 1500 of it. States 0x739B60, the
 * modes of state 1 0x739B54 by o+0x55.
 *
 * Type 25 of area 4 (0x4912F0): the one the player must keep alive; its
 * health is the area's, G+0x928 (of G+0x92A), half of it (0x384) slows
 * it. It walks between the two points 0x739B78 (o+0x10C picks the far
 * one) following the player; blocked in front by the player for 0x2D
 * frames it waits, at a ledge it climbs down a level (o+0x5A); its zone
 * at G+0xA44 picks the camera o+0x1CC. Other live objects (not in state
 * 3, dying) or the flag 7:0x1F make it cower. Hit, it loses 100 (o+0x170 0xFFFF) or
 * 250; at 0 the game is over. Flag 7:0x1D while it reached its point.
 * States 0x739BC8: 0 start (0x739BA4 by o+0x55), 1 normal (0x739BAC),
 * 2 hit, 3 dead.
 *
 * Type 28 of area 4 (0x4918D0): the same with a fixed place 0x739BD8, turned
 * to it and walking back to it; hit, it loses 0x104 unless flag 7:0x18.
 * States 0x739C18: init, normal (0x739BFC), hit (0x739C08), dead. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define A20_MAIN 0x739B60u      /* type 20 states */
#define A20_MODE 0x739B54u      /* type 20 state 1, by o+0x55 */
#define A25_MAIN 0x739BC8u      /* type 25 states */
#define A25_START 0x739BA4u     /* type 25 state 0, by o+0x55 */
#define A25_MODE 0x739BACu      /* type 25 state 1, by o+0x55 */
#define A25_PTS  0x739B78u      /* its two points, 8 bytes each */
#define A25_HITA 0x739BC4u      /* hit animations by o+0x178 */
#define A28_MAIN 0x739C18u
#define A28_MODE 0x739BFCu
#define A28_HIT  0x739C08u
#define A28_HITA 0x739C04u
#define A28_PT   0x739BD8u      /* its place */
#define HEALTH() (G() + 0x928)
#define PL()     U32(G() + 0x940)

/* the angle to p less the heading within +-0x1E (only from below: -0x1E..0) */
static int faced(gptr o, gptr p) { return (u16)(Angle_DiffTo(o + 0x40, p, U16(o + 0x4A)) + 0x1E) <= 0x1E; }
static void save_pos(gptr o) { U16(o + 0xE0) = U16(o + 0x40); U16(o + 0xE2) = U16(o + 0x42); U16(o + 0xE4) = U16(o + 0x44); }
static void set_mode(gptr o, u8 m) { U8(o + 0x55) = m; U8(o + 0x56) = 0; }

/* ---- type 20 (area 8) ---- */

void DC2CALL Ao20_Handler(gptr o)                                   /* 0x490670 */
{
    save_pos(o);
    if (Flag_Check(7, 0x13)) {
        Flag_Set(7, 0x13, 0);
        U16(o + 0x104) = 0x9C; U16(o + 0x106) = 0x7F40;
    }
    Orig_Call1(U32(A20_MAIN + U8(o + 0x54) * 4), o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Obj_UpdateMatrix(o);
}

void DC2CALL Ao20_Init(gptr o)                                      /* 0x490290 */
{
    U8(o + 0x54)++;
    U32(o + 0x50) = 0x20005;
    U16(o + 0x108) = 0x4B0;
    Model_Setup(o, U32(o + 0x60));
    Obj_Set118(o, 8, 0x1000, 0x1000);
    Anim_Start(o, 1, 0, 1, 0);
    U32(o + 0x12C) = 0x739B48;
    U32(o + 0x128) = 0x739B38;
    U16(o + 0x10C) = U16(o + 0x1C0);
}

void DC2CALL Ao20_Normal(gptr o)                                    /* 0x490630 */
{
    gptr pl = PL();
    U32(o + 0x1C0) = U32(pl + 0x40); U16(o + 0x1C4) = U16(pl + 0x44);   /* where the player is */
    Zone_TrackPoint(o, o + 0x1C0);
    Orig_Call1(U32(A20_MODE + U8(o + 0x55) * 4), o);
}

/* turns toward p by up to max; whether it turned at all */
u32 DC2CALL Ao20_Turn(gptr o, u32 max, gptr p)                      /* 0x490450 */
{
    u16 d = (u16)Angle_Approach(U16(o + 0x4A), max, Angle_XZ(o + 0x40, p));
    U16(o + 0x4A) += d;
    return d != 0;
}

/* mode 0: watching the player */
void DC2CALL Ao20_Watch(gptr o)                                     /* 0x490300 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0;
        Anim_Start(o, 1, 0, 1, 10);
        /* fall through */
    case 1:
        Obj_TurnToPoint(o, 0x1E, pl + 0x40);
        if (faced(o, pl + 0x40)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0, 0, 1, 10);
            U8(o + 0x1C8) = 0; U8(o + 0x1C9) = 0;
        }
        break;
    case 2: {
        u8 prev = U8(o + 0x1C8);
        U8(o + 0x1C9) = prev;
        u8 now = (u8)Ao20_Turn(o, 0x1E, pl + 0x40);
        U8(o + 0x1C8) = now;
        if (now & (u8)~prev) Anim_Start(o, 1, 0, 1, 10);             /* started turning */
        else if ((u8)~now & prev) Anim_Start(o, 0, 0, 1, 10);       /* stopped */
        u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
        if (!U16(o + 0x10C)) { if (d >= 0x5F5E10) set_mode(o, 1); }
        else if (d <= 0x5F5E10) set_mode(o, 2);
        break;
    }
    }
    Anim_Step(o);
}

/* mode 1: going home, until the player comes within 2000 */
void DC2CALL Ao20_Home(gptr o)                                      /* 0x490490 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0;
        Anim_Start(o, 1, 0, 1, 10);
        /* fall through */
    case 1:
        Obj_TurnToPoint(o, 0x1E, o + 0xD8);
        if (faced(o, o + 0xD8)) {
            U8(o + 0x56)++;
            Anim_Start(o, 2, 0, 1, 10);
            U16(o + 0xF4) = 0x82;
        }
        break;
    case 2:
        Obj_TurnToPoint(o, 0x20, o + 0xD8);
        if (Dist2_XZ(o + 0x40, pl + 0x40) <= 0x3D0900) set_mode(o, 0);
        break;
    }
    Anim_Step(o);
}

/* mode 2: walking home while the player is between 1500 and 5000 */
void DC2CALL Ao20_Away(gptr o)                                      /* 0x490570 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 1, 0, 1, 10);
        U16(o + 0xF4) = 0x36;
        /* fall through */
    case 1:
        Obj_TurnToPoint(o, 0x1E, o + 0xD8);
        if (Dist2_XZ(o + 0x40, pl + 0x40) >= 0x17D7840) { set_mode(o, 0); return; }
        if (Dist2_XZ(o + 0x40, o + 0x1C0) <= 0x225510) set_mode(o, 0);
        break;
    }
    Anim_Step(o);
}

/* ---- type 25 (area 4) ---- */

void DC2CALL Ao25_Handler(gptr o)                                   /* 0x4912F0 */
{
    gptr pl = PL();
    save_pos(o);
    U8(o + 0x1D0) = S16(HEALTH()) <= 0x384 ? 0x1E : 0x3C;          /* turning speed */
    Orig_Call1(U32(A25_MAIN + U8(o + 0x54) * 4), o);
    Ao25_Zone(o);
    Obj_MoveLocal(o, o + 0xF0);
    RotMatrix(o + 0x48, o + 0x20);
    u32 b = U32(o + 0x1C0);
    U32(o + 0x1C4) = b;                                             /* last frame's bits */
    U32(o + 0x1C0) = b & ~8u;
    if (!(U8(o + 0x50) & 0x20) && Obj_CollideObjs(o) && U8(o + 0x54) == 1 && (U8(o + 0x130) & 1)
        && (s16)((Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A)) + 0x400) & 0xFFF) <= 0x800)
        U32(o + 0x1C0) |= 8;                                        /* the player blocks it in front */
    if ((U8(o + 0x1C0) & 8) && (U8(o + 0x1C4) & 8)) {
        if (++U16(o + 0x1C8) >= 0x2D) set_mode(o, 5);
    } else U16(o + 0x1C8) = 0;
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Obj_UpdateMatrix(o);
    if (U8(o + 0x1C0) & 1) Flag_Set(7, 0x1D, 1);
}

/* the camera of the zone it stands in */
void DC2CALL Ao25_Zone(gptr o)                                      /* 0x491450 */
{
    gptr z = U32(G() + 0xA44);
    gptr v = Tmp_Push(8);
    U16(v) = U16(o + 0x40); U16(v + 2) = U16(o + 0x42); U16(v + 4) = U16(o + 0x44);
    for (u32 i = 0; (i & 0xFFFF) < U32(G() + 0xA48); i++, z += 0x2C) {
        s16 lv = S16(o + 0x5A);
        u8 lo = U8(z + 6);
        if ((s16)lo > lv || (s32)lo + U8(z + 7) < lv || (U8(z) & 0x80)) continue;
        if (PointInQuad(v, z + 0xC)) {
            if (U8(z + 4) != U16(o + 0x1CC)) U16(o + 0x1CC) = U8(z + 4);
            break;
        }
    }
    Tmp_Pop(8);
}

/* another live object about that is not dying */
u32 DC2CALL Ao25_Other(gptr o)                                      /* 0x490B40 */
{
    gptr p = PL() + 0x260;
    for (u32 n = U32(0x87F940) - 1; n; n--, p += 0x260)
        if ((U8(p + 0x50) & 1) && p != o && U8(p + 0x54) != 3) return 1;
    return 0;
}

/* turns toward p (mode 1: 0x300 to its left) by up to max; whether it turned 10 or more */
u32 DC2CALL Ao25_Turn(gptr o, u32 max, gptr p, u32 mode)           /* 0x490B90 */
{
    u32 a = Angle_XZ(o + 0x40, p);
    if ((u8)mode == 1) a = (a - 0x300) & 0xFFF;
    s16 d = (s16)Angle_Approach(U16(o + 0x4A), max, a);
    U16(o + 0x4A) += (u16)d;
    return d <= -10 || d >= 10;
}

void DC2CALL Ao25_Start(gptr o)                                     /* 0x490880 */
{
    Orig_Call1(U32(A25_START + U8(o + 0x55) * 4), o);
}

void DC2CALL Ao25_Init(gptr o)                                      /* 0x490720 */
{
    U32(o + 0x50) = 0x20005;
    U16(o + 0x10C) = U16(o + 0x1C0);
    U16(o + 0x1C0) = 0;
    Model_Setup(o, U32(o + 0x60));
    Obj_Set118(o, 8, 0x1000, 0x1000);
    U32(o + 0x128) = 0x739B88;
    U32(o + 0x12C) = 0x739B98;
    U32(o + 0x134) = 0x739B98;
    U32(G() + 0xB50) = o;
    Flag_Set(3, 0xB, 1);
    U16(G() + 0x92A) = 0x708;
    if (!U16(o + 0x10C)) U16(HEALTH()) = 0x708;
    U16(o + 0x1CA) = 0x1E;
    U8(o + 0x55)++;
    Anim_Start(o, S16(HEALTH()) > 0x384 ? 2 : 4, 0, 1, 0);
    U16(o + 0x1CC) = 0xFFFF;
}

/* a while standing, then (at the first point) a turn before it sets off */
void DC2CALL Ao25_Wait(gptr o)                                      /* 0x490810 */
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Step(o);
        if (--U16(o + 0x1CA)) return;
        if (!U16(o + 0x10C)) {
            U8(o + 0x56)++;
            Anim_Start(o, 3, 0, 1, 5);
            return;
        }
        break;
    case 1:
        if (!Anim_Step(o)) return;
        break;
    default:
        return;
    }
    U8(o + 0x55) = 1; U8(o + 0x56) = 0; U8(o + 0x54)++;
}

/* mode 0: following the player about */
void DC2CALL Ao25_Follow(gptr o)                                    /* 0x4908A0 */
{
    gptr pl = PL();
    s16 h = S16(HEALTH());
    u32 run = h <= 0x384 ? 4 : 2, idle = h <= 0x384 ? 1 : 0;
    u8 hale = h > 0x384;
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56) = 1;
        U16(o + 0xF4) = 0;
        Anim_Start(o, idle, 0, 1, 10);
        U8(o + 0x1CE) = 0; U8(o + 0x1CF) = 0;
        break;
    case 1: {
        U8(o + 0x1CF) = U8(o + 0x1CE);
        u8 now = (u8)Ao25_Turn(o, U8(o + 0x1D0), pl + 0x40, hale);
        u8 prev = U8(o + 0x1CE);
        U8(o + 0x1CE) = now;
        U8(o + 0x1CF) = prev;
        if (prev & (u8)~now) {                                      /* done turning */
            if (S16(HEALTH()) > 0x384) { Anim_Start(o, 3, 0, 0, 10); U8(o + 0x56) = 2; }
            else { Anim_Start(o, 4, 0, 9, 10); U8(o + 0x56) = 3; }
            U8(o + 0x1CE) = 0; U8(o + 0x1CF) = 0;
        }
        Anim_Step(o);
        break;
    }
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56) = 3; Anim_Start(o, idle, 0, 1, 10); }
        break;
    case 3: {
        U8(o + 0x1CF) = U8(o + 0x1CE);
        u8 now = (u8)Ao25_Turn(o, U8(o + 0x1D0), pl + 0x40, 0);
        u8 prev = U8(o + 0x1CE);
        U8(o + 0x1CF) = prev;
        U8(o + 0x1CE) = now;
        u32 b = U32(o + 0x1C0);
        if (!(b & 4)) {
            if (now & (u8)~prev) { U32(o + 0x1C0) = b | 2; Anim_Start(o, idle, 0, 1, 10); }
            else if ((u8)~now & prev) { U32(o + 0x1C0) = b & ~2u; Anim_Start(o, run, 0, 9, 10); }
        }
        if (Anim_Step(o) && (U32(o + 0x1C0) & 4)) {
            U32(o + 0x1C0) &= ~4u;
            Anim_Start(o, run, 0, 9, 10);
        }
        break;
    }
    }
    if (!(U8(o + 0x1C0) & 1)) {
        if (Dist2_XZ(o + 0x40, pl + 0x40) <= 0x5F5E10) set_mode(o, 1);
        u32 a = Angle_XZ(o + 0x40, o + 0xD8);
        if ((s16)((Angle_DiffTo(o + 0x40, pl + 0x40, a) + 0x400) & 0xFFF) <= 0x800) set_mode(o, 1);
    } else Flag_Set(7, 0x1D, 1);
    if (Flag_Check(7, 0x1F) || Ao25_Other(o)) set_mode(o, 2);
}

/* mode 1: walking on to its point */
void DC2CALL Ao25_Walk(gptr o)                                      /* 0x490BF0 */
{
    gptr pl = PL();
    u32 slow = S16(HEALTH()) <= 0x384;
    s16 back = (s16)((Angle_DiffTo(o + 0x40, pl + 0x40, Angle_XZ(o + 0x40, o + 0xD8)) + 0x400) & 0xFFF);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, slow, 0, 1, 10);
        /* fall through */
    case 1:
        Obj_TurnToPoint(o, U8(o + 0x1D0), o + 0xD8);
        if (faced(o, o + 0xD8)) {
            U8(o + 0x56)++;
            U16(o + 0xF4) = S16(HEALTH()) > 0x384 ? 0x28 : 0x36;
        }
        break;
    case 2:
        if (!(U32(G() + 4) % 5) && Wall_Ahead(o, 0x280)) { set_mode(o, 4); return; }
        Obj_TurnToPoint(o, U8(o + 0x1D0), o + 0xD8);
        if (back > 0x800 && Dist2_XZ(o + 0x40, pl + 0x40) >= 0x17D7840) { set_mode(o, 0); U32(o + 0x1C0) |= 4; }
        if (Dist2_XZ(o + 0x40, A25_PTS + U16(o + 0x10C) * 8) <= 0xF4240) { U32(o + 0x1C0) |= 1; set_mode(o, 0); }
        break;
    }
    Anim_Step(o);
    if (Flag_Check(7, 0x1F) || Ao25_Other(o)) set_mode(o, 2);
}

/* mode 2: frightened */
void DC2CALL Ao25_Fright(gptr o)                                    /* 0x490DC0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0;
        Anim_Start(o, 7, 0, 0x40, 10);
        break;
    case 1:
        if (Anim_Step(o)) set_mode(o, 3);
        break;
    }
}

/* mode 3: cowering while others are about */
void DC2CALL Ao25_Cower(gptr o)                                     /* 0x490E10 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 8, 0, 9, 7);
        break;
    case 1:
        Anim_Step(o);
        if (!Ao25_Other(o)) set_mode(o, 1);
        break;
    }
}

/* mode 4: down a ledge, to the level below */
void DC2CALL Ao25_Drop(gptr o)                                      /* 0x490E60 */
{
    switch (U8(o + 0x56)) {
    case 0: {
        u32 f = U32(o + 0x50);
        U8(o + 0x174) |= 1;
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0;
        U16(o + 0x4A) = 0x800;
        U32(o + 0x50) = f | 0x30;
        Anim_Start(o, 9, 0, 0x40, 10);
        break;
    }
    case 1:
        if (Anim_Step(o)) {
            U16(o + 0x5A) += 2;
            U16(o + 0x174) &= ~1;
            set_mode(o, 1);
            U16(o + 0x42) = (u16)(-1000 * (s32)U16(o + 0x5A));
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    }
}

/* mode 5: the player in its way */
void DC2CALL Ao25_Blocked(gptr o)                                   /* 0x490F00 */
{
    gptr pl = PL();
    u8 an = S16(HEALTH()) > 0x384 ? 2 : 4;
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0;
        Anim_Start(o, an, 0, 9, 10);
        break;
    case 1:
        Anim_Step(o);
        if (Dist2_XZ(o + 0x40, pl + 0x40) >= 0xF4240) set_mode(o, 1);
        break;
    }
    if (Flag_Check(7, 0x1F)) set_mode(o, 2);
}

void DC2CALL Ao25_Normal(gptr o)                                    /* 0x490FB0 */
{
    Zone_TrackPoint(o, A25_PTS + U16(o + 0x10C) * 8);
    Orig_Call1(U32(A25_MODE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ao25_Hit(gptr o)                                       /* 0x490FE0 */
{
    static const u16 turn[4] = {0, 0x400, 0x800, 0xC00};
    U32(o + 0x1C0) &= ~1u;
    Flag_Set(7, 0x1D, 0);
    switch (U8(o + 0x55)) {
    case 0:
        U8(o + 0x174) |= 1;
        U8(o + 0x55)++;
        if (U16(o + 0x170) == 0xFFFF) { U16(o + 0x170) = 0; U16(HEALTH()) -= 100; }
        else U16(HEALTH()) -= 250;
        if (S16(HEALTH()) <= 0) {
            U16(HEALTH()) = 0;
            U8(o + 0x54) = 3; U8(o + 0x55) = 0;
            U16(o + 0xF4) = 0;
            return;
        }
        U16(o + 0xF4) = 0;
        if (U16(o + 0x178)) {
            U8(o + 0x55) = 3;
            Anim_Start(o, U8(A25_HITA + S16(o + 0x178)), 0, 0x40, 10);
        } else Anim_Start(o, 5, 0, 0x40, 10);
        Se_Play(o, 0x5E);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x55)++; U16(o + 0x1CA) = 0x1E; }
        break;
    case 2:
        if (!U16(o + 0x1CA)) { U8(o + 0x55)++; Anim_Start(o, 6, 0, 0, 10); }
        else U16(o + 0x1CA)--;
        break;
    case 3:
        U16(o + 0xF4) -= 4;
        if (S16(o + 0xF4) <= 0) U16(o + 0xF4) = 0;
        if (Anim_Step(o)) {
            U16(o + 0x174) &= ~1;
            U8(o + 0x54) = 1; U8(o + 0x55) = 3;
            U16(o + 0xF4) = 0;
        }
        break;
    }
    gptr r = Tmp_Push(8);                                           /* facing the side it was hit from */
    U16(r) = 0; U16(r + 2) = (u16)((turn[S16(o + 0x178) & 3] + U16(o + 0x4A)) & 0xFFF); U16(r + 4) = 0;
    RotMatrix(r, o + 0x20);
    Tmp_Pop(8);
}

void DC2CALL Ao25_Die(gptr o)                                       /* 0x4911C0 */
{
    gptr pl = PL();
    Flag_Set(1, 0, 1);
    switch (U8(o + 0x55)) {
    case 0:
        U32(o + 0x50) = (U32(o + 0x50) & ~4u) | 2;
        U8(o + 0x55)++;
        Flag_Set(1, 0xA, 1);
        if (U8(G() + 0x7C4) != U16(o + 0x1CC)) {                     /* to its camera, its effects gone */
            Flag_Set(1, 3, 1);
            U32(pl + 0x50) &= ~2u;
            Camera_Switch(U16(o + 0x1CC));
            Eff_KillRoom(0);
            for (u32 i = 0; i < U32(0x87DE04); i++) {
                gptr e = U32(G() + 0x93C) + i * 0x70;
                if ((U8(e + 0x50) & 1) && U8(e + 0x4E) == 3 && S8(e + 0x4F) >= 3 && U8(e + 0x4F) != 0x10) Eff_Free(e);
            }
        }
        Flag_Set(7, 0x1C, 1);
        Anim_Start(o, 0xA, 0, 8, 10);
        Se_Play(o, 0x5F);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x55)++; GameOver_Start(0); }
        break;
    }
}

/* ---- type 28 (area 4) ---- */

void DC2CALL Ao28_Handler(gptr o)                                   /* 0x4918D0 */
{
    save_pos(o);
    Orig_Call1(U32(A28_MAIN + U8(o + 0x54) * 4), o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Obj_UpdateMatrix(o);
}

void DC2CALL Ao28_Init(gptr o)                                      /* 0x491510 */
{
    u8 s = U8(o + 0x54);
    U8(o + 0x55) = 0;
    U32(o + 0x50) = 0x1005;
    U8(o + 0x54) = s + 1;
    U16(HEALTH()) = 0x708; U16(G() + 0x92A) = 0x708;
    Model_Setup(o, U32(o + 0x60));
    Obj_Set118(o, 8, 0x1000, 0x1000);
    Anim_Start(o, 6, 0, 1, 0);
    U32(o + 0x128) = 0x739BE0;
    U32(o + 0x12C) = 0x739BF0;
    U32(o + 0x134) = 0x739BF0;
    U32(G() + 0xB50) = o;
    Flag_Set(3, 0xB, 1);
}

void DC2CALL Ao28_Normal(gptr o)                                    /* 0x491750 */
{
    Orig_Call1(U32(A28_MODE + U8(o + 0x55) * 4), o);
}

/* mode 0: at its place, turned to it */
void DC2CALL Ao28_Stay(gptr o)                                      /* 0x4915B0 */
{
    Obj_TurnToPoint(o, 0x3C, A28_PT);
    switch (U8(o + 0x56)) {
    case 0:
        U16(o + 0xF4) = 0;
        if (faced(o, A28_PT)) { Anim_Start(o, 6, 0, 1, 10); U8(o + 0x56)++; }
        break;
    case 1:
        U8(o + 0x56)++;
        break;
    case 2:
        if (Anim_Step(o) || !(U16(o + 0x92) & 3)) Flag_Set(7, 0x14, 1);
        if (Dist2_XZ(o + 0x40, A28_PT) > 0xF4240) set_mode(o, 1);
        break;
    }
}

/* mode 1: back to its place */
void DC2CALL Ao28_Back(gptr o)                                      /* 0x491680 */
{
    u32 an = 4 + (S16(HEALTH()) <= 0x384);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, an, 0, 1, 10);
        /* fall through */
    case 1:
        Obj_TurnToPoint(o, 0x3C, A28_PT);
        if (faced(o, A28_PT)) { U16(o + 0xF4) = 0x36; U8(o + 0x56)++; }
        break;
    case 2:
        Obj_TurnToPoint(o, 0x3C, A28_PT);
        if (Dist2_XZ(o + 0x40, A28_PT) <= 0x77A10) set_mode(o, 0);
        break;
    }
    Anim_Step(o);
}

void DC2CALL Ao28_Hit(gptr o)                                       /* 0x491840 */
{
    Orig_Call1(U32(A28_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Ao28_Hurt(gptr o)                                      /* 0x491770 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U16(o + 0x174) |= 1;
        U8(o + 0x56) = 1;
        U16(o + 0xF4) = 0;
        if (!Flag_Check(7, 0x18)) U16(HEALTH()) -= 0x104;
        if (S16(HEALTH()) < 0) {
            U16(HEALTH()) = 0;
            U8(o + 0x54) = 3; U8(o + 0x55) = 0; U8(o + 0x56) = 0;
            return;
        }
        Anim_Start(o, U8(A28_HITA + S16(o + 0x178)), 0, 0x40, 10);
        Se_Play(o, 0x61);
        break;
    case 1:
        if (Anim_Step(o)) {
            U16(o + 0x174) &= ~1;
            U8(o + 0x54) = 1; U8(o + 0x55) = 1; U8(o + 0x56) = 0;
        }
        break;
    }
}

void DC2CALL Ao28_Die(gptr o)                                       /* 0x491860 */
{
    switch (U8(o + 0x55)) {
    case 0:
        U8(o + 0x55)++;
        GameOver_Start(0);
        Flag_Set(1, 0, 1);
        Flag_Set(7, 0x17, 1);
        Anim_Start(o, 7, 0, 8, 10);
        Se_Play(o, 0x62);
        break;
    case 1:
        if (Anim_Step(o)) U8(o + 0x55)++;
        break;
    }
}
