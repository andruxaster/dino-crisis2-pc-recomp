/* The roamer: object type 22 of area 3 (type handler 0x42CD10; rooms 303,
 * 304 and 305). It wanders along a graph of points (o+0x1C0 the graph:
 * u16 points, u16 links, then 8 bytes a point: s16 x, u16 mark, s16 z,
 * u8 kind, s8 its flag; then 8 bytes a link: u8 a, u8 b, s16 length). It
 * keeps away from the player, bumps the player over without harm, waits at
 * some points, and goes on into the next room through the exits that are
 * open (G+0x1274: the room it is in).
 * Point kinds: 1 an exit (its flag: which room, +3), 2 the exit is shut,
 * 4 a place to wait (its flag: group 7), 8 the way out for good (event 0x6C),
 * 0x10 where it turns away from the player, 0x20 the exit's other door
 * flag, 0x40 the point it goes to when all three exits of room 304 are open.
 * Its own state:
 *   +0x1C4 u16 points, +0x1C6 u16 links, +0x1C8 the points, +0x1CC the
 *   links, +0x1D0 s8 the point it is at, +0x1D1 s8 the point it goes to,
 *   +0x1D2 times it waited, +0x1D3 a wall turned it this frame, +0x1D4 the
 *   wait's timer, +0x1D8 its heading before the walls, +0x1DC how near the
 *   player may come (2500..4000), +0x1E0 open exits counted.
 * States (o+0x54, 0x7208E0): 0 init, 1 start (or gone: not this room),
 * 2 hit, 3 gone, 4 normal, by o+0x55 (0x7208F4): 0 walks off, 1 waits,
 * 2 runs along the path.
 * Shared (0x74AF18..): the points of kinds 1/4, the path (each point's
 * previous one), the exits, the point of kind 0x40, the first plain point. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define RM_MAIN   0x7208E0u     /* states */
#define RM_MOVE   0x7208F4u     /* state 4 by o+0x55 */
#define RM_NEAR   0x7208ACu     /* by room: how near an exit counts */
#define RM_LIST   0x74AF18u     /* points of kinds 1 or 4 */
#define RM_PREV   0x74AF38u     /* the path: each point's previous one */
#define RM_EXITS  0x74AF58u     /* points of kind 1 */
#define RM_NLIST  0x74AF78u
#define RM_NEXITS 0x74AF79u
#define RM_P40    0x74AF7Au     /* the point of kind 0x40 */
#define RM_FIRST  0x74AF7Bu     /* the first point of none of the kinds 0x1D */
#define PT(o, n)  (U32((o) + 0x1C8) + (s32)(s8)(n) * 8)

void DC2CALL Obj_Vanish(gptr o)                                     /* 0x426C60 */
{
    U32(o + 0x50) |= 0x40000000;
}

/* ---- the handler ---- */

void DC2CALL Roam_Type(gptr o)                                      /* 0x42CD10 */
{
    Obj_SavePos(o);
    Roam_Exits(o);
    Orig_Call1(U32(RM_MAIN + U8(o + 0x54) * 4), o);
    U16(o + 0x4A) &= 0xFFF;
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    if (!(U8(o + 0x50) & 0x10)) {
        U16(o + 0x1D8) = U16(o + 0x4A);
        u16 x = U16(o + 0x40), z = U16(o + 0x44);
        Obj_CollideWalls(o);
        U8(o + 0x1D3) = 0;
        if (x != U16(o + 0x40) || z != U16(o + 0x44)) {     /* a wall: along it */
            u16 a = (u16)Angle_XZ(o + 0xE0, o + 0x40);
            U16(o + 0x4A) = a;
            U16(o + 0x1D8) -= a;
            if (!U16(o + 0x1D8)) U16(o + 0x4A) += 3;
            RotMatrix(o + 0x48, o + 0x20);
            U8(o + 0x1D3) = 1;
        }
    }
    Obj_UpdateMatrix(o);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Roam_Init(gptr o)                                      /* 0x42CDF0 */
{
    U8(o + 0x54)++;
    U32(o + 0x50) = 1;
    for (u32 k = 0; k < 0x20; k += 4) U32(RM_PREV + k) = 0;
    Model_Setup(o, U32(o + 0x60));
    U32(o + 0x128) = 0x7208C8;
    U32(o + 0x134) = 0x7208D4;
    U32(o + 0x50) |= 0x10;
    if (!U8(G() + 0x1274)) U8(G() + 0x1274) = 3;
}

/* a random point of none of the kinds 0x1D (after 52 tries: the first such
 * point, into *at only when found at random) */
static gptr random_point(gptr o, u32 at)
{
    for (s32 t = 0;; t++) {
        U8(o + at) = (u8)(Rand8() % U16(o + 0x1C4));
        gptr e = PT(o, U8(o + at));
        if (!(U8(e + 6) & 0x1D)) return e;
        if (t > 0x32) return PT(o, U8(RM_FIRST));
    }
}

void DC2CALL Roam_Start(gptr o)                                     /* 0x42CE60 */
{
    gptr g = G(), pl = U32(g + 0x940);
    if (U8(g + 0x1090) != U8(g + 0x1274)) { U8(o + 0x54) = 3; return; }
    U8(o + 0x54) += 3;
    U8(o + 0x55) = 0; U8(o + 0x56) = 0;
    U16(o + 0x108) = 0;
    U32(o + 0x50) = 0xAD;
    gptr p = U32(o + 0x1C0);
    U32(o + 0x1DC) = 0x9C4;
    U32(o + 0x1C4) = U32(p);
    u32 n = U16(p);
    U32(o + 0x1C8) = p + 4;
    U32(o + 0x1CC) = p + 4 + n * 8;
    u32 k = 0;
    while (k < n && (U8(p + 4 + k * 8 + 6) & 0x1D)) k++;
    U8(RM_FIRST) = (u8)k;
    gptr e = random_point(o, 0x1D0);
    U16(o + 0x40) = U16(e); U16(o + 0x44) = U16(e + 4);
    U8(RM_NLIST) = 0; U8(RM_NEXITS) = 0;
    for (u32 i = 0; i < U16(o + 0x1C4); i++) {
        gptr f = U32(o + 0x1C8) + i * 8 + 6;
        if (U8(f) & 5) U8(RM_LIST + U8(RM_NLIST)++) = (u8)i;
        if (U8(f) & 1) U8(RM_EXITS + U8(RM_NEXITS)++) = (u8)i;
        if (U8(f) & 0x40) U8(RM_P40) = (u8)i;
    }
    Anim_Start(o, 1, 0, 9, 0);
    Se_Play(pl, 0x61);
}

/* an exit open or shut by an event flag (group 6); counted: in o+0x1E0 */
static void exit_flag(gptr o, gptr e, u32 flag, int counted)
{
    if (Flag_Check(6, flag)) {
        if (counted) U8(o + 0x1E0)++;
        U8(e + 6) |= 2;
    } else U8(e + 6) &= ~2;
}

void DC2CALL Roam_Exits(gptr o)                                     /* 0x42CFF0 */
{
    gptr g = G(), rm = U32(g + 0xA4C), q = rm + U32(rm + 0x3C);
    U8(o + 0x1E0) = 0;
    if (!U32(o + 0x1C8)) return;
    for (s8 i = 0; (s32)i < (s32)U8(RM_NEXITS); i++) {
        gptr e = U32(o + 0x1C8) + U8(RM_EXITS + i) * 8;
        u8 f = U8(e + 6);
        if (!(f & 1)) continue;
        u8 room = U8(G() + 0x1090);
        switch ((s8)U8(e + 7)) {
        case 0:
            if (room == 4) exit_flag(o, e, f & 0x20 ? 0x7C : 0x6A, 1);
            else if (room == 5) exit_flag(o, e, 0x69, 0);
            break;
        case 1:
            if (room == 3) exit_flag(o, e, f & 0x20 ? 0x7C : 0x6A, 0);
            else if (room == 5) exit_flag(o, e, 0x6B, 0);
            break;
        case 2:
            if (room == 3) exit_flag(o, e, 0x69, 0);
            else if (room == 4) {
                if (Flag_Check(6, 0x6B)) { U8(o + 0x1E0)++; U8(e + 6) |= 2; U8(q + 3) |= 0x60; }
                else { U8(e + 6) &= ~2; U8(q + 3) &= 0x9F; }
            }
            break;
        }
    }
}

/* ---- moving ---- */

/* the next point on the path from where it is toward o+0x1D1 */
static s32 path_step(gptr o, s32 to)
{
    s32 c = (s8)to, prev;
    do { prev = c; c = U8(RM_PREV + c); } while (c != (s8)U8(o + 0x1D0));
    return prev;
}

void DC2CALL Roam_Walk(gptr o)                                      /* 0x42D180 */
{
    gptr pl = U32(G() + 0x940), m = Tmp_Push(0x38), d2 = m + 0x20, v = m + 0x24, r = m + 0x2C;
    Anim_Step(o);
    U16(o + 0xF4) = 0;
    s8 c = (s8)Roam_Nearest(o, d2);
    U8(o + 0x1D0) = (u8)c;
    if (c != -1) {
        u32 n = U32(RM_NEAR + U8(G() + 0x1090) * 4);
        if (U32(d2) < n * n && (U8(PT(o, c) + 6) & 0x10) && U16(o + 0x1C6)) {
            u8 res = 0;
            for (u32 i = 0; i < U16(o + 0x1C6); i++) {
                gptr l = U32(o + 0x1CC) + i * 8;
                s32 cur = (s8)U8(o + 0x1D0);
                if (U8(l) == cur) res = (u8)Roam_Pick(o, U8(l + 1), res);
                else if (U8(l + 1) == cur) res = (u8)Roam_Pick(o, U8(l), res);
            }
            if (res) {
                Roam_Path(o);
                U32(o + 0x50) |= 0x10;
                U8(o + 0x55) = 2; U8(o + 0x56) = 0;
                Tmp_Pop(0x38);
                return;
            }
        }
    }
    /* the player near: away from it */
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    U32(o + 0x1DC) += Rand8() % 20;
    if (U32(o + 0x1DC) > 0xFA0) U32(o + 0x1DC) = 0x9C4;
    u32 k = U32(o + 0x1DC);
    if (d < k * k) {
        U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0; U16(r + 6) = 0;
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x9C4; U16(v + 6) = 0;
        U16(r + 2) = (u16)Angle_XZ(pl + 0x40, o + 0x40);
        RotMatrix(r, m);
        ApplyMatrixSV(m, v, v);
        U16(v + 4) += U16(o + 0x44);
        U16(v) += U16(o + 0x40);
        U16(o + 0xF4) = 0x78;
        if (!U8(o + 0x1D3)) Obj_TurnToPoint(o, 0x66, v);
    }
    Tmp_Pop(0x38);
}

/* the next point n (from a link of where it is): a place to wait, the way
 * out (event 0x6C), an open exit; better than res wins */
u32 DC2CALL Roam_Pick(gptr o, u32 n, u32 res)                       /* 0x42D380 */
{
    gptr e = PT(o, n);
    s8 r = (s8)res;
    if ((U8(e + 6) & 4) && !r) { U8(o + 0x1D1) = (u8)n; return 1; }
    if ((U8(e + 6) & 8) && r < 2 && Flag_Check(6, 0x6C)) { U8(o + 0x1D1) = (u8)n; return 2; }
    if ((U8(e + 6) & 1) && r < 3 && !(U8(e + 6) & 2)) { U8(o + 0x1D1) = (u8)n; return 3; }
    return (u8)r;
}

void DC2CALL Roam_Wait(gptr o)                                      /* 0x42D400 */
{
    gptr g = G(), pl = U32(g + 0x940);
    u16 t = U16(o + 0x1D4)++;
    if (t > 0x96) {
        Flag_Set(7, 0x1F, 1);
        U16(o + 0x1D4) = (u16)(Rand8() % 3 * 30);
    }
    if (Flag_Check(2, 0x19)) {
        if (Dist2_XZ(o + 0x40, pl + 0x40) > 0x2255100) { Anim_Step(o); return; }
        Flag_Set(7, (u32)(s32)S8(PT(o, U8(o + 0x1D0)) + 7), 0);
    } else if (Flag_Check(7, (u32)(s32)S8(PT(o, U8(o + 0x1D0)) + 7))) { Anim_Step(o); return; }
    U8(o + 0x1D2)++;
    Anim_Start(o, 1, 0, 9, 0);
    Roam_Path(o);
    for (s32 t2 = 0;;) {
        if (U8(G() + 0x1090) == 4 && U8(o + 0x1E0) == 3) { U8(o + 0x1D1) = U8(RM_P40); break; }
        U8(o + 0x1D1) = (u8)(Rand8() % U16(o + 0x1C4));
        if (!(U8(PT(o, U8(o + 0x1D1)) + 6) & 0x1D)) break;
        if (t2++ > 0x32) { U8(o + 0x1D1) = U8(RM_FIRST); break; }
    }
    s32 p = path_step(o, U8(o + 0x1D1));
    U8(o + 0x1D0) = (u8)p;
    U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, PT(o, p));
    U32(o + 0x50) |= 8;
    U8(o + 0x55) = 2; U8(o + 0x56) = 0;
    Anim_Step(o);
}

void DC2CALL Roam_Run(gptr o)                                       /* 0x42D5C0 */
{
    gptr ang = Tmp_Push(8), d2 = ang + 4;
    U32(ang) = 0x64;
    gptr e = PT(o, U8(o + 0x1D0));
    Anim_Step(o);
    if (U8(PT(o, U8(o + 0x1D1)) + 6) & 2) {                   /* its exit shut */
        s8 c = (s8)Roam_Nearest(o, d2);
        gptr g = G();
        u32 n = U32(RM_NEAR + U8(g + 0x1090) * 4);
        if (U32(d2) < n * n && (U8(PT(o, c) + 6) & 1)) {
            U8(g + 0x1274) = (u8)(U8(e + 7) + 3);
            U32(o + 0x50) |= 0x40000000;
        } else {
            U32(o + 0x50) &= ~0x10u;
            U8(o + 0x55) = 0;
            U16(o + 0xF4) = 0;
        }
        Tmp_Pop(8);
        return;
    }
    switch (U8(o + 0x56)) {
    case 0:
        if ((u8)Roam_TurnTo(o, ang, e)) break;
        U16(o + 0xF4) = 0x78;
        U8(o + 0x56)++;
        U16(o + 0x4A) = U16(ang);
        /* fall through */
    case 1:
        if (Dist2_XZ(o + 0x40, e) >= 0x3D090) break;
        U8(o + 0x56) = 0;
        if (U8(o + 0x1D0) != U8(o + 0x1D1)) {                 /* on to the next point */
            U8(o + 0x1D0) = (u8)path_step(o, U8(o + 0x1D1));
            U16(o + 0xF4) = 0;
            break;
        }
        U16(o + 0xF4) = 0;
        U16(o + 0x40) = U16(e); U16(o + 0x44) = U16(e + 4);
        if (U8(e + 6) & 4) {                                  /* a place to wait */
            U8(o + 0x55) = 1;
            Flag_Set(7, (u32)(s32)S8(e + 7), 1);
            U32(o + 0x50) &= ~8u;
            U16(o + 0x1D4) = 0;
            U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x10;
            Anim_Start(o, 1, 0, 0x19, 0);
        } else if (U8(e + 6) & 1) {                           /* an exit: into that room */
            U8(G() + 0x1274) = (u8)(U8(e + 7) + 3);
            U32(o + 0x50) |= 0x40000000;
        } else if (U8(e + 6) & 8) {                           /* the way out */
            Flag_Set(6, 0x6D, 1);
            U32(o + 0x50) |= 0x40000000;
        } else {
            U8(o + 0x55) = 0;
            U32(o + 0x50) &= ~0x10u;
        }
        break;
    }
    Tmp_Pop(8);
}

/* turns (by at most *max) toward the point e; *max then the angle to it */
u32 DC2CALL Roam_TurnTo(gptr o, gptr max, gptr e)                   /* 0x42D7E0 */
{
    u16 m = U16(max);
    u32 a = Angle_XZ(o + 0x40, e);
    U16(max) = (u16)a;
    u32 r = Angle_Approach(U16(o + 0x4A), m, a);
    U16(o + 0x4A) += (u16)r;
    return r;
}

void DC2CALL Roam_Normal(gptr o)                                    /* 0x42D820 */
{
    gptr pl = U32(G() + 0x940);
    if (U8(pl + 0x54) != 2 && U8(o + 0x55) != 1) Roam_Bump(o);
    Orig_Call1(U32(RM_MOVE + U8(o + 0x55) * 4), o);
    if (Rand8() % 0xB4 == 0) Se_Play(o, 0x61);
}

/* the player bumped over (no harm) when the two cylinders meet */
void DC2CALL Roam_Bump(gptr o)                                      /* 0x42D880 */
{
    gptr pl = U32(G() + 0x940);
    if (U16(pl + 0x174)) return;
    gptr a = U32(o + 0x134), b = U32(pl + 0x12C);
    s16 ox = (s16)(U16(o + 0x40) + U16(a)), oz = (s16)(U16(o + 0x44) + U16(a + 4));
    s16 px = (s16)(U16(pl + 0x40) + U16(b)), pz = (s16)(U16(pl + 0x44) + U16(b + 4));
    s32 dx = px - ox, dz = pz - oz;
    if (dx < 0) dx = -dx;
    if (dz < 0) dz = -dz;
    u32 r = (u32)U16(a + 6) + U16(b + 6);
    if ((u32)dz * (u32)dz + (u32)dx * (u32)dx < r * r) {
        U8(pl + 0x54) = 2; U8(pl + 0x55) = 0; U8(pl + 0x56) = 0;
        U16(pl + 0x176) = 0;
        U16(pl + 0x174) = 1;
    }
}

/* hit: runs to the nearest place to wait or exit that is not shut */
void DC2CALL Roam_Hit(gptr o)                                       /* 0x42D940 */
{
    if (U8(o + 0x55) != 1) {
        gptr d2 = Tmp_Push(4);
        s8 c = (s8)Roam_Nearest(o, d2);
        Tmp_Pop(4);
        U8(o + 0x1D0) = (u8)c;
        if (c != -1) {
            Roam_Path(o);
            u32 best = 0x35A4E900;
            u8 bi = 0;
            for (u32 i = 0; i < U8(RM_NLIST); i++) {
                u8 n = U8(RM_LIST + i);
                gptr e = U32(o + 0x1C8) + n * 8;
                if (U8(e + 6) & 2) continue;
                u32 d = Dist2_XZ(o + 0x40, e);
                if (d < best) { best = d; bi = n; }
            }
            U8(o + 0x1D1) = bi;
            s32 p = path_step(o, bi);
            U8(o + 0x1D0) = (u8)p;
            U16(o + 0x4A) = (u16)Angle_XZ(o + 0x40, PT(o, p));
            U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x10;
            U8(o + 0x55) = 2; U8(o + 0x56) = 0;
            U16(o + 0xF4) = 0;
        }
    }
    U8(o + 0x54) = 4;
    Anim_Step(o);
}

/* ---- the graph ---- */

/* shortest paths from o+0x1D0 over the links (marks: done); RM_PREV each
 * point's previous one */
void DC2CALL Roam_Path(gptr o)                                      /* 0x42DA50 */
{
    u16 dist[32 + 2];
    u16 *dd = dist + 1;                                       /* (dd[-1]: point -1) */
    u32 bi = 0;
    for (u32 i = 0; i < U16(o + 0x1C4); i++) {
        U16(U32(o + 0x1C8) + i * 8 + 2) = 0;
        dd[i] = 0x7530;
    }
    dd[(s8)U8(o + 0x1D0)] = 0;
    for (u32 cnt = 0; (s32)cnt < (s32)U16(o + 0x1C4);) {
        gptr pts = U32(o + 0x1C8);
        u32 nn = U16(o + 0x1C4);
        s16 min = 0x7530;
        for (u32 i = 0; i < nn; i++)
            if (!U16(pts + i * 8 + 2) && (s32)min >= (s32)dd[i]) { bi = i; min = (s16)dd[i]; }
        U16(pts + bi * 8 + 2) = 1;
        cnt++;
        for (u32 j = 0; j < U16(o + 0x1C4); j++) {
            gptr l = U32(o + 0x1CC);
            if (U16(pts + j * 8 + 2)) continue;
            for (u32 k = 0; k < U16(o + 0x1C6); k++, l += 8) {
                u32 a = U8(l), b = U8(l + 1);
                if (!((a == bi && b == j) || (a == j && b == bi))) continue;
                if ((s32)dd[j] > (s32)dd[bi] + S16(l + 2)) {
                    dd[j] = (u16)(dd[bi] + U16(l + 2));
                    U8(RM_PREV + j) = (u8)bi;
                    break;
                }
            }
        }
    }
}

/* the nearest point it can see; *d2 its squared distance; -1: none */
u32 DC2CALL Roam_Nearest(gptr o, gptr d2)                           /* 0x42DC20 */
{
    gptr pt = Tmp_Push(8);
    s32 res = -1;
    U32(d2) = 0x35A4E900;
    U32(pt) = 0; U32(pt + 4) = 0;
    for (u32 i = 0; i < U16(o + 0x1C4); i++) {
        gptr e = U32(o + 0x1C8) + i * 8;
        U16(pt) = U16(e); U16(pt + 4) = U16(e + 4);
        if ((u8)Line_Blocked(o, o + 0x40, pt, 1)) continue;
        u32 d = Dist2_XZ(o + 0x40, e);
        if (U32(d2) > d) { U32(d2) = d; res = (s32)i; }
    }
    Tmp_Pop(8);
    return (u32)res;
}
