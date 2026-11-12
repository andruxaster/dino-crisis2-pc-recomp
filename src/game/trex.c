/* The tyrannosaurus (object type handler 0x423680, type 3, E10.DAT). It keeps
 * its state at o+0x1C0.., a set of bits first (Bits_Set(o+0x1C0, n)):
 * 1 moving (its head looks), 2 holds the player in its jaws, 3 shakes the
 * held player, 4/5/6 the scripted kind's steps, 8 the hit was heavy, 9 runs
 * at a point, 0xA down (gets up again), 0xC a point is set.
 * +0x4 kind (0 the one that breaks the gates, 1 the scripted one, 2 the extra
 * mode), +0x8..0xB the state of the last frame, +0xC damage taken since the
 * last flinch, +0x10/+0x14 timers (+0x14 heavy damage taken), +0x18 the wait
 * after the player was hurt or held, +0x26 the point it runs at, +0x30..+0x48
 * timers, +0x30..+0x34 a point it walks to (1FE its time).
 * States (o+0x54): 0 init, 1 normal (think 0x7201C4, then move 0x7201F4 by
 * o+0x55), 2 hit (0x720224), 3 die (0x720234), 4 (0x4A56D0). Attacks land
 * through 0x720244.
 * Moves (o+0x55): 0 stand, 1 walk, 2 roar, 3 look about, 4 snatch the player
 * and shake it, 5 recoil, 6 the scripted break through, 7 stagger, 8 bite,
 * 9 bite while walking, 10 charge at a gate, 11 lunge (the player on another
 * level). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define TR_MAIN  0x7201B0u      /* state handlers */
#define TR_THINK 0x7201C4u      /* state 1: deciding, by o+0x55 */
#define TR_MOVE  0x7201F4u      /* state 1: moving, by o+0x55 */
#define TR_HIT   0x720224u      /* state 2 */
#define TR_DIE   0x720234u      /* state 3 */
#define TR_GRAB  0x720244u      /* an attack landed, by o+0x55 */
#define TR_GATES 0x720168u      /* the three gates' quads (0x10 each) */
#define TR_POINT 0x720198u      /* the three points it charges at (8 each) */
#define TR_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u      /* bit 0: one holds the player */
#define PL()     U32(G() + 0x940)
#define DIST(o)  U32((o) + 0x13C)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static void look(gptr o, u32 max) { Obj_TurnFromHead(o, max, o + 0xD8); }
static void quake(gptr o) { Shake_Set(2, 0, 0x1E); Flash_Play(o, 0x65B9EC); }
static void step(gptr o, u32 se) { quake(o); Se_Play(o, se); }
static s32 half(s32 v) { return v / 2; }

/* turns o toward pos as seen from its head (part 0xF) */
void DC2CALL Obj_TurnFromHead(gptr o, u32 max, gptr pos)            /* 0x45B3D0 */
{
    gptr p = Obj_Part(U32(o + 0x64), 0xF);
    u16 x = U16(o + 0x40), y = U16(o + 0x42), z = U16(o + 0x44);
    U16(o + 0x40) = U16(p + 0x40); U16(o + 0x42) = U16(p + 0x42); U16(o + 0x44) = U16(p + 0x44);
    Obj_TurnToPoint(o, max, pos);
    U16(o + 0x40) = x; U16(o + 0x42) = y; U16(o + 0x44) = z;
}

/* a random point of the room's zone list (G+0xA50) into out; o+0x138 its index */
void DC2CALL Zone_RandomPoint(gptr o, gptr out)                     /* 0x487340 */
{
    gptr t = U32(G() + 0xA50), e = t + 4;
    s32 r = (s32)Rand8() % S16(t);
    U8(o + 0x13A) = 1;
    U16(o + 0x138) = (u16)r;
    U16(out) = U16(e + S16(o + 0x138) * 20 + 8);
    U16(out + 2) = U16(e + S16(o + 0x138) * 20 + 0xA);
    U16(out + 4) = U16(e + S16(o + 0x138) * 20 + 0xC);
}

/* o's attack spheres (+0xA0) of the current frame against a quad (x/z): the
 * index of the sphere inside it in +0x119; 0xFF none on, 0xFE none inside */
u32 DC2CALL Enemy_AttackInQuad(gptr o, gptr quad)                   /* 0x479B30 */
{
    gptr a = U32(o + 0xA0);
    if (!a || (U16(o + 0x102) & 2)) { U8(o + 0x119) = 0xFF; return 0xFF; }
    gptr v = Tmp_Push(8), m = Tmp_Push(0x20);
    RotMatrix(o + 0x48, m);
    u32 div = (U16(o + 0x102) & 8) ? 1 : (U16(o + 0x102) & 4) ? 4 : 2;
    u8 res = 0xFF;
    for (u32 k = 0;; k++) {
        u32 f = U16(o + 0x92);
        if ((u32)(U8(a + 8) * 2) / div <= f && f <= (u32)(U8(a + 9) * 2) / div) {
            U16(v + 6) = U16(a + 6);
            gptr b = o, mm = m;
            if (U8(a + 0xA) != 0xFF) { b = Obj_Part(U32(o + 0x64), U8(a + 0xA)); mm = b + 0x20; }
            ApplyMatrixSV(mm, a, v);
            U16(v) += U16(b + 0x40); U16(v + 2) += U16(b + 0x42); U16(v + 4) += U16(b + 0x44);
            if (PointInQuad(v, quad)) { Tmp_Pop(0x28); U8(o + 0x119) = (u8)k; return (u8)k; }
            res = 0xFE;
        }
        u8 more = U8(a + 0xB);
        a += 0x10;
        if (!more) break;
    }
    Tmp_Pop(0x28);
    U8(o + 0x119) = res;
    return res;
}

/* ---- the handler ---- */

void DC2CALL TRex_Handler(gptr o)                                   /* 0x423680 */
{
    gptr g = G(), b = TR_BITS(o);
    U8(o + 0x206) = U8(g + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    if (!U32(o + 0x1C4)) {                              /* hurt: the gates' flag; no lower than half health */
        s32 m = S16(o + 0x10A);
        if (S16(o + 0x108) < m - m / 10) Flag_Set(7, 0x13, 1);
        s16 h = (s16)(S16(o + 0x10A) >> 1);
        if (S16(o + 0x108) < h) U16(o + 0x108) = (u16)h;
    }
    u8 mv = U8(o + 0x55);
    if (mv <= 3 || mv == 9) U8(o + 0x11B) = 0;
    else U8(o + 0x11B) = Anim_FrameFlag(o) ? 1 : 0x11;
    if (U16(o + 0x1FE)) Zone_TrackPoint(o, o + 0x1F0);
    if (U32(o + 0x1C4) == 0) TRex_Gates(o);
    else if (U32(o + 0x1C4) == 1) TRex_Event(o);
    Orig_Call1(U32(TR_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x600 && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    u8 r = U8(G() + 0x108F);
    if (!(r == 3 || (r > 7 && r <= 0xA))) U32(o + 0x50) |= 0x20;
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    TRex_Timer(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U32(o + 0x1D4)) U32(o + 0x1D4)--;
    if (U16(o + 0x1F8)) U16(o + 0x1F8)--;
    if (U16(o + 0x1FC)) U16(o + 0x1FC)--;
    if (U16(o + 0x1FE)) U16(o + 0x1FE)--;
    if (U16(o + 0x200)) U16(o + 0x200)--;
    if (U16(o + 0x202)) U16(o + 0x202)--;
    if (U8(o + 0x208)) U8(o + 0x208)--;
    if (U16(o + 0x1FE) == 1) U16(o + 0x200) = 0x3C;
    TRex_Grab(o);
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1C8) = U8(o + 0x54);
    U8(o + 0x1CA) = U8(o + 0x56); U8(o + 0x1CB) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: kind, health, model, animations */
void DC2CALL TRex_Init(gptr o)                                      /* 0x423970 */
{
    U32(o + 0x1C4) = 0;
    if (U8(G() + 0x1091) == 9 && U8(G() + 0x1090) == 3) U32(o + 0x1C4) = 1;
    if (U8(G() + 0x927)) U32(o + 0x1C4) = 2;
    U8(o + 0x54)++;
    U32(o + 0x50) = 0xB;
    U16(o + 0x108) = 0x2710;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x4E20;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    switch (U32(o + 0x1C4)) {
    case 1:
        U32(o + 0x12C) = 0x7200D8; U32(o + 0x128) = 0x7200F8; U32(o + 0x134) = 0x720138;
        Obj_Set118(o, 0, 0x36B0, 0x4E20);
        break;
    case 0: case 2:
        U32(o + 0x12C) = 0x7200A8; U32(o + 0x128) = 0x7200E8; U32(o + 0x134) = 0x720108;
        Obj_Set118(o, 0, 0x4E20, 0x7530);
        break;
    }
    stop(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x642C84;
    Anim_Start(o, 0, 0, 1, 5);
    DrawSync(o);
    Head_Reset(o);
    Obj_SetFeet(o, 0xE, 0xA);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0x80; U16(o + 0x16E) = 0x300;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(TR_BITS(o), 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF04) = 0;
    U8(o + 0xFE) = 3;
    U32(o + 0x184) |= 0x20;
    U8(o + 0x118) |= 0x20;
    Flag_Set(2, 0x26, 1);
    U8(o + 0x207) = 0;
    U32(SHARED) = 0;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL TRex_Normal(gptr o)                                    /* 0x423BC0 */
{
    Obj_SaveFeet(o);
    Orig_Call1(U32(TR_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(TR_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL TRex_Hit(gptr o)                                       /* 0x423BF0 */
{
    Orig_Call1(U32(TR_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL TRex_Die(gptr o)                                       /* 0x423C10 */
{
    Orig_Call1(U32(TR_DIE + U8(o + 0x55) * 4), o);
}

/* its attacks hit the player: by the sub-state */
void DC2CALL TRex_Grab(gptr o)                                      /* 0x423C30 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(TR_GRAB + U8(o + 0x55) * 4), o);
}

/* the wait after the player was hurt or held: kept up while it is */
u32 DC2CALL TRex_Timer(gptr o)                                      /* 0x426320 */
{
    gptr pl = PL();
    if (U32(o + 0x1D8)) U32(o + 0x1D8)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D8) = 0x1E;
        if (!U8(G() + 0x108E)) U32(o + 0x1D8) = 0x3C;
    }
    return U32(o + 0x1D8);
}

/* the scripted kind: down where it can't be seen (dies off), or the room's
 * event 0xC (bit 4) */
u32 DC2CALL TRex_Event(gptr o)                                      /* 0x426390 */
{
    gptr b = TR_BITS(o);
    if (!Pos_Hidden(o + 0x40) && Bits_Check(b, 0xA)) { U32(o + 0x54) = 0x30103; return 1; }
    if (U8(G() + 0x7C4) == 0xC && !Bits_Check(b, 4)) { Bits_Set(b, 4, 1); return 1; }
    return 0;
}

/* the scripted kind's steps by the room's event (G+0x7C4) */
u32 DC2CALL TRex_Script(gptr o)                                     /* 0x426410 */
{
    gptr b = TR_BITS(o);
    if (U32(o + 0x1C4) != 1) return 0;
    if (Bits_Check(b, 4) && !Bits_Check(b, 5)) {
        Bits_Set(b, 5, 1);
        U32(o + 0x54) = 0x601;
        U8(o + 0x17B) = 0xA;
        return 1;
    }
    if (U8(G() + 0x7C4) == 0x10 && !Bits_Check(b, 6)) {
        Bits_Set(b, 6, 1);
        Enemy_BiteOn(o);
        U32(o + 0x54) = 0x401;
        U16(o + 0x40) = 0x13F4; U16(o + 0x42) = 0xFC18; U16(o + 0x44) = 0x532; U16(o + 0x4A) = 0;
        return 1;
    }
    if (U8(G() + 0x7C4) <= 9 && Bits_Check(b, 5)) {
        Bits_Set(b, 5, 0);
        Bits_Set(b, 4, 0);
        Enemy_BiteOn(o);
        U32(o + 0x54) = 0x401;
        U16(o + 0x40) = 0xEA96; U16(o + 0x42) = 0xFC18; U16(o + 0x44) = 0xA352; U16(o + 0x4A) = 0xC00;
        return 1;
    }
    return 0;
}

/* the gate kind: when the player is beyond one of three gates and it faces
 * the gate's point: charge at it */
u32 DC2CALL TRex_Target(gptr o)                                     /* 0x426530 */
{
    gptr pl = PL();
    if (U32(o + 0x1C4) || U16(o + 0x202)) return 0;
    for (s16 i = 0; i < 3; i++) {
        gptr p = TR_POINT + i * 8;
        s32 a = (s16)Angle_DiffTo(o + 0x40, p, U16(o + 0x4A));
        u32 d2 = Dist2_XZ(o + 0x40, p);
        u32 ok = 0;
        switch (i) {
        case 0: ok = S16(pl + 0x40) < 0x2328; break;
        case 1: ok = S16(pl + 0x40) <= (s16)0xF060 || S16(pl + 0x44) >= 0x1F4; break;
        case 2: ok = S16(pl + 0x40) < (s16)0xFC18; break;
        }
        if (Flag_Check(7, 0x10 + i) || !ok) continue;
        a = iabs((s16)a);
        if ((a < 0x200 && d2 < 0xF42400) || (a < 0x400 && d2 < 0x2255100 && Rand8() < 8)) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = (Anim_FrameFlag(o) << 24) | 0xA01;
            Bits_Set(TR_BITS(o), 9, 1);
            Bits_Set(TR_BITS(o), 0xC, 1);
            U16(o + 0x1E6) = (u16)i;
            U16(o + 0x202) = 0x3C;
            return 1;
        }
    }
    return 0;
}

/* the gate kind: its attacks hit a gate's quad: the gate's flag */
u32 DC2CALL TRex_Gates(gptr o)                                      /* 0x4266A0 */
{
    u32 r = 0;
    if (U32(o + 0x1C4)) return 0;
    if (U8(o + 0x54) == 1 && (U8(o + 0x55) < 8 || U8(o + 0x55) > 0xB)) return 0;
    for (u32 k = 0; k < 3; k++)
        if ((s8)Enemy_AttackInQuad(o, TR_GATES + k * 0x10) >= 0) { Flag_Set(7, 0x10 + k, 1); r = 1; }
    return r;
}

/* ---- deciding (state 1, before the move) ---- */

/* the attacks from standing or walking; 1 when one was chosen */
u32 DC2CALL TRex_Decide(gptr o)                                     /* 0x4243B0 */
{
    gptr pl = PL();
    u8 thr = U32(o + 0x1C4) == 1 ? 2 : 0x20;
    s16 a = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
    u8 ff = (u8)Anim_FrameFlag(o);
    if (!U8(o + 0x13A) && !U16(o + 0x1FE) && !Bits_Check(SHARED, 0) && !U32(o + 0x1D8)) {
        if (U16(o + 0x5A) == U16(pl + 0x5A)) {
            u32 d = DIST(o);
            if (d > 0xDAC && d < 0x157C && iabs(a) < 0x100 && Rand8() < 0x10) {
                Enemy_BiteOn(o);
                U32(o + 0x54) = ((u32)ff << 24) | 0xA01;
                Bits_Set(TR_BITS(o), 9, 0);
                return 1;
            }
            d = DIST(o);
            if (d > 0x1388 && d < 0x2EE0 && !ff && iabs(a) < 0x100 && Rand8() < thr && !U16(o + 0x1F8)) goto grab;
            if (d < 0x1388 && d > 0x7D0 && iabs(a) < 0x200) {
                Enemy_BiteOn(o);
                u8 k = a < 0x20 ? 1 : a == 0x20 ? 2 : 0;
                if (Rand8() < 0x20) k = (u8)(Rand8() % 3);
                U32(o + 0x54) = ((u32)k << 24) | 0x801;
                return 1;
            }
        } else {
            u32 d = DIST(o);
            if (d < 0x1388 && iabs(a) < 0x80) {
                Enemy_BiteOn(o);
                U32(o + 0x54) = ((u32)ff << 24) | 0xB01;
                return 1;
            }
            if (d > 0x1388 && d < 0x2EE0 && !ff && iabs(a) < 0x100 && Rand8() < 4 && !U16(o + 0x1F8)) goto grab;
        }
    }
    if (U8(pl + 0x174) & 2) { U32(o + 0x54) = 0x501; return 1; }
    if (U16(o + 0x5A) != U16(pl + 0x5A) && (U16(o + 0x1FE) || Bits_Check(SHARED, 0) || U32(o + 0x1D8))
        && DIST(o) < 0x1388) {
        U32(o + 0x54) = 0x201;
        return 1;
    }
    return 0;
grab:
    Enemy_BiteOn(o);
    U32(o + 0x54) = 0x401;
    return 1;
}

/* move 0, standing: the kind's script, the gates, the attacks; far: walk;
 * near: wander to a point (the gate kind to the next gate's) */
void DC2CALL TRex_ThinkStand(gptr o)                                /* 0x423C60 */
{
    if (U8(o + 0x56) != 1) return;
    look(o, 0x23);
    if (TRex_Script(o) || TRex_Target(o) || TRex_Decide(o)) return;
    u32 d = DIST(o);
    if (d > 0x1964) { U32(o + 0x54) = 0x1000101; return; }
    if (d >= 0xBB8) return;
    u32 k = U32(o + 0x1C4);
    if (k == 0) {
        if (U16(o + 0x200)) return;
        u8 c = 0;
        if (Flag_Check(7, 0x10)) c = 1;
        if (Flag_Check(7, 0x11)) c = 2;
        if (Flag_Check(7, 0x12)) c = 3;
        switch (c) {
        case 0:
            if (S16(o + 0x44) > 0) { U16(o + 0x1F0) = 0x2AF8; U16(o + 0x1F2) = 0; U16(o + 0x1F4) = 0xE4A8; break; }
            goto other;
        case 1: case 2:
            if (S16(o + 0x40) > 0x1B58) { U16(o + 0x1F0) = 0x7D0; U16(o + 0x1F2) = 0; U16(o + 0x1F4) = 0xF448; break; }
            goto other;
        case 3:
            if (S16(o + 0x40) > 0) { U16(o + 0x1F0) = 0xCD38; U16(o + 0x1F2) = 0; U16(o + 0x1F4) = 0x7D0; break; }
        other:
            U16(o + 0x1F0) = 0x2710; U16(o + 0x1F2) = 0; U16(o + 0x1F4) = 0x1F40;
            break;
        }
        U16(o + 0x1FE) = 0x5A;
    } else if (k <= 2) {
        if (U16(o + 0x200)) return;
        Zone_RandomPoint(o, o + 0x1F0);
        U16(o + 0x1FE) = 0x5A;
    }
}

void DC2CALL TRex_Stand(gptr o)                                     /* 0x423E40 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(TR_BITS(o), 1, 1);
        if (!U8(o + 0x57)) Anim_Start(o, 0, Anim_FrameFlag(o) ? 0xF : 3, 1, 0x14);
        else {
            s32 f = U16(o + 0x92) * 94;
            Anim_Start(o, 0, (u16)(((f / 64 + 3) % 94) / 4), 1, 0x14);
        }
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, 0x3F);
        break;
    }
    if (U16(o + 0x92) == 0xB) step(o, 0x30);
    if (U16(o + 0x92) == 0x3C) step(o, 0x31);
}

/* move 1, walking: bite, grab or stand */
void DC2CALL TRex_ThinkWalk(gptr o)                                 /* 0x423F70 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    look(o, 0x2D);
    if (TRex_Script(o) || TRex_Target(o)) return;
    s16 a = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
    u8 ff = (u8)Anim_FrameFlag(o);
    if (!U8(o + 0x13A) && !U16(o + 0x1FE) && !Bits_Check(SHARED, 0) && iabs(a) < 0x180 && !U32(o + 0x1D8)) {
        if (U16(o + 0x5A) == U16(pl + 0x5A)) {
            u32 d = DIST(o);
            if (d > 0x7D0 && d < 0x1B58) {
                Enemy_BiteOn(o);
                U32(o + 0x54) = ((u32)ff << 24) | 0x901;
                return;
            }
        } else if (!ff && iabs(a) < 0x100 && Rand8() < 0x20 && !U16(o + 0x1F8)) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = 0x401;
            return;
        }
    }
    if (DIST(o) < 0x157C) U32(o + 0x54) = 0x1000001;
}

void DC2CALL TRex_Walk(gptr o)                                      /* 0x4240C0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(TR_BITS(o), 1, 1);
        if (!U8(o + 0x57)) Anim_Start(o, 1, Anim_FrameFlag(o) ? 0xD : 4, 1, 0x14);
        else {
            s32 f = U16(o + 0x92) << 6;
            Anim_Start(o, 1, (u16)(((f / 94 + 3) % 64) / 4), 1, 0x14);
        }
        U8(o + 0x208) = 0x1E;
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, 0xBE);
        break;
    }
    if (U16(o + 0x92) == 0xA) step(o, 0x30);
    if (U16(o + 0x92) == 0x28) step(o, 0x31);
}

/* move 2, the roar */
void DC2CALL TRex_ThinkRoar(gptr o)                                 /* 0x4241F0 */
{
    if (U8(o + 0x56) == 1) look(o, 0x23);
}

void DC2CALL TRex_Roar(gptr o)                                      /* 0x424210 */
{
    static const u8 anim[2] = {4, 3};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, anim[Anim_FrameFlag(o)], 4, 0x48, 0x14);
        Bits_Set(TR_BITS(o), 1, 1);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 0x301;
        break;
    }
    if (U16(o + 0x92) == 0x11) quake(o);
}

/* move 3, looking about a while */
void DC2CALL TRex_ThinkLook(gptr o)                                 /* 0x4242D0 */
{
    if (U8(o + 0x56) != 1) return;
    look(o, 0x11);
    if (!U16(o + 0x1FC)) U32(o + 0x54) = 1;
}

void DC2CALL TRex_Look(gptr o)                                      /* 0x424300 */
{
    static const u8 anim[2] = {7, 8};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 1);
        Anim_Start(o, anim[Anim_FrameFlag(o)], 0, 1, 0x14);
        U16(o + 0x1FC) = (u16)((s32)Rand8() % 10 + 10);
        break;
    case 1:
        Anim_Step(o);
        break;
    }
}

/* move 4, snatching the player up and shaking it */
void DC2CALL TRex_ThinkSnatch(gptr o)                               /* 0x4246A0 */
{
    if (U16(o + 0x92) < 0x28) look(o, 0xA);
}

void DC2CALL TRex_Snatch(gptr o)                                    /* 0x4246D0 */
{
    gptr pl = PL(), b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 2, 1);
        Bits_Set(b, 3, 0);
        Anim_Start(o, 5, 0, 0x40, 0x14);
        Flash_Play(o, 0x65BA10);
        U16(o + 0x1F8) = 0xF0;
        return;
    case 1:
        break;
    default:
        return;
    }
    if (Anim_Step(o)) {
        Enemy_BiteOff(o);
        U32(o + 0x54) = 1;
        Bits_Set(b, 2, 0);
    }
    if (U16(o + 0x92) == 0x38) {
        U32(o + 0x50) &= ~8u;
        Shake_Set(3, 0, 0x1C);
        u8 s = U8(pl + 0x54);
        if (!(s == 2 || s == 5 || (U8(pl + 0x174) & 1) || Flag_Check(2, 0x1A))) {
            Player_Hold(o, 0, 0x48E100);
            Anim_Start(pl, U32(U32(o + 0x88) + 0x80), 0, 0x40, 0);
            U32(G() + 0x92C) = 0x4248A0;
        }
        U8(G() + 0xB58) = 0x80; U8(G() + 0xB59) = 0x80; U8(G() + 0xB5A) = 0x80;
        U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
    }
    if (U16(o + 0x92) >= 0x38) DrawSync(0);
    Bits_Set(b, 3, U16(o + 0x92) >= 0x38 && U16(o + 0x92) < 0x50);
}

/* the player in its jaws (the player's callback G+0x92C): turned with it,
 * dragged along while shaken; let go: falls */
void DC2CALL TRex_PlSnatched(gptr pl)                               /* 0x4248A0 */
{
    gptr t = U32(pl + 0x1D8), tb = t + 0x1C0;
    Obj_TurnToPoint(pl, 0x230, t + 0x40);
    switch (U8(pl + 0x57)) {
    case 0:
        Anim_Step(pl);
        if (Bits_Check(tb, 2)) {
            if (Bits_Check(tb, 3)) {
                gptr v = Tmp_Push(8);
                U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0xA;
                Vec_RotY(v, v, U16(t + 0x4A));
                U16(pl + 0x40) += U16(v); U16(pl + 0x44) += U16(v + 4);
                Tmp_Pop(8);
                Obj_CollideWalls(pl);
            }
        } else {
            U8(pl + 0x57)++;
            Anim_Start(pl, U32(U32(t + 0x88) + 0x84), 0, 0x40, 0);
        }
        break;
    case 1:
        if (Anim_Step(pl)) {
            Anim_Set(pl, 0x16, 0, 1, 0xA);
            Player_AnimReset();
        }
        break;
    }
}

/* move 5, recoiling */
void DC2CALL TRex_Recoil(gptr o)                                    /* 0x4249B0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, 6, 0, 0x40, 0x14);
        break;
    case 1:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        break;
    }
}

/* move 6, the scripted kind: walks to the wall and breaks through */
void DC2CALL TRex_Burst(gptr o)                                     /* 0x424A30 */
{
    gptr v;
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, 1, 0, 1, 0x14);
        U32(o + 0x50) |= 0x60;
        break;
    case 1:
        Anim_Step(o);
        v = Tmp_Push(8);
        U16(v) = 0x2710; U16(v + 2) = 0xFC18; U16(v + 4) = 0xA04C;
        Zone_TrackPoint(o, v);
        Tmp_Pop(8);
        Obj_TurnToPoint(o, 0x23, o + 0xD8);
        Value_Approach(o + 0xF4, 0x14, 0xBE);
        if (S16(o + 0x40) > 0xBB8 && S16(o + 0x44) < (s16)0xAA10) {
            U8(o + 0x56)++;
            Anim_Start(o, 2, 0, 0x40, 0x14);
            Flash_Play(o, 0x65B9FC);
            Effect_Attach(o, 0x659CC0);
            U8(o + 0x174) |= 4;
            stop(o);
            U32(o + 0x50) |= 0x10;
        }
        if (U16(o + 0x92) == 0xA) step(o, 0x30);
        if (U16(o + 0x92) == 0x28) step(o, 0x31);
        break;
    case 2:
        v = Tmp_Push(8);
        U16(v) = 0x1770; U16(v + 2) = 0; U16(v + 4) = 0xC568;
        Obj_TurnToPoint(o, 0x23, v);
        Tmp_Pop(8);
        if (U16(o + 0x92) == 0x1C) step(o, 0x30);
        if (U16(o + 0x92) == 0x1F) step(o, 0x31);
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U16(o + 0x174) &= 0xFFFB;
            U16(o + 0x42) = 0; U16(o + 0x5A) = 0;
            Anim_Set(o, 1, 0, 1, 0);
            U8(o + 0x17B) = 0;
            U32(o + 0x50) &= ~0x70u;
            U32(o + 0x54) = 1;
        }
        if (S16(o + 0x42) > 0) U16(o + 0x42) = 0;
        break;
    }
}

/* move 7, staggering: down a while, then up again */
void DC2CALL TRex_Stagger(gptr o)                                   /* 0x424C80 */
{
    static const u8 anim[6] = {0x16, 0x19, 0x17, 0x1A, 0x18, 0x1B};
    u8 k = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, anim[k], 0, 0x40, 0x14);
        Flash_Play(o, 0x65BAAC);
        U16(o + 0x204) = 0x1E;
        U8(o + 0x17B) = 0xA;
        break;
    case 1:
        U8(o + 0x17B) = 0xA;
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, anim[U8(o + 0x57) + 2], 0, 0x41, 0xA);
        }
        if (U16(o + 0x92) == 0x10) Shake_Set(5, 0, 0x1E);
        break;
    case 2:
        Anim_Step(o);
        if (U16(o + 0x204)) U16(o + 0x204)--;
        if (!U16(o + 0x204)) {
            U8(o + 0x56)++;
            Anim_Start(o, anim[U8(o + 0x57) + 4], 0, 0x40, 0xA);
        }
        break;
    case 3:
        if (Anim_Step(o)) {
            U32(o + 0x54) = 0x501;
            U32(o + 0x1D4) = 0;
            U32(o + 0x1D0) = 0;
        }
        break;
    }
}

/* move 8, the bite (left, ahead, right by o+0x57) */
void DC2CALL TRex_ThinkBite(gptr o)                                 /* 0x424E20 */
{
    u8 r = U8(G() + 0x108F);
    if (U32(o + 0x1C4) == 2 && (r >= 7 || r == 4)) look(o, 5);
    else look(o, 0xA);
}

void DC2CALL TRex_Bite(gptr o)                                      /* 0x424E70 */
{
    static const u8 anim[3] = {0xB, 0xC, 0xD};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 0x14);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x4B) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        break;
    }
    if (U16(o + 0x92) == 0xD || U16(o + 0x92) == 0x3C) quake(o);
}

/* move 9, a bite while walking */
void DC2CALL TRex_ThinkWalkBite(gptr o)                             /* 0x424F50 */
{
    look(o, 0x2D);
}

void DC2CALL TRex_WalkBite(gptr o)                                  /* 0x424F70 */
{
    static const u8 anim[2] = {0x13, 0x12};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], 7, 0x40, 0xA);
        break;
    case 1:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        break;
    }
    if (U16(o + 0x92) == 0x12 || U16(o + 0x92) == 0x32) quake(o);
}

/* move 10, the charge (at a gate's point, or at the player) */
void DC2CALL TRex_ThinkCharge(gptr o)                               /* 0x425040 */
{
    if (Bits_Check(TR_BITS(o), 9)) Obj_TurnFromHead(o, 0x23, TR_POINT + S16(o + 0x1E6) * 8);
}

void DC2CALL TRex_Charge(gptr o)                                    /* 0x425080 */
{
    static const u8 anim[2] = {0xE, 0x10};
    gptr pl = PL();
    if (U8(o + 0x56) == 2 && U16(o + 0x92) < 0xF) {     /* the player in its jaws: carried along */
        U16(o + 0x40) += U16(pl + 0xE8); U16(o + 0x42) += U16(pl + 0xEA); U16(o + 0x44) += U16(pl + 0xEC);
        U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
        U16(pl + 0x4A) = U16(o + 0x4A);
    }
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 0x14);
        break;
    case 1: case 2:
        if (Anim_Step(o)) {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 1;
            Bits_Set(TR_BITS(o), 0xC, 0);
        }
        break;
    }
}

/* move 11, a lunge (the player on another level) */
void DC2CALL TRex_Lunge(gptr o)                                     /* 0x425190 */
{
    static const u8 anim[2] = {0x14, 0x15};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Bits_Set(TR_BITS(o), 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 0x14);
        break;
    case 1:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 0x201; }
        Value_Approach(o + 0xF4, 0x14, 0);
        break;
    }
}

/* ---- the attacks landing ---- */

static void hurt(gptr o, u32 dmg_front, u32 dmg_back)
{
    gptr pl = PL();
    s32 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (iabs(a) < 0x400) Player_Hurt(o, dmg_front, 2, 0xA);
    else Player_Hurt(o, dmg_back, 1, 0);
}

static void land(gptr o, gptr flash, gptr eff, u32 dmg_front, u32 dmg_back)
{
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, flash);
    if (U8(G() + 0x108F) != 3) Effect_Attach(o, eff);
    hurt(o, dmg_front, dmg_back);
}

void DC2CALL TRex_BiteHit(gptr o)                                   /* 0x425240 */
{
    static const u32 eff[3] = {0x65855C, 0x658678, 0x658794};
    land(o, 0x65BA4C, eff[U8(o + 0x57)], 0xFA, 0xC8);
}

void DC2CALL TRex_WalkBiteHit(gptr o)                               /* 0x425300 */
{
    static const u32 eff[2] = {0x658FC8, 0x65915C};
    land(o, 0x65BA84, eff[U8(o + 0x57)], 0xFA, 0xDC);
}

void DC2CALL TRex_LungeHit(gptr o)                                  /* 0x425690 */
{
    static const u32 eff[2] = {0x6592F0, 0x659484};
    land(o, 0x65BA98, eff[U8(o + 0x57)], 0x12C, 0xFA);
}

/* the charge lands: catches the player in its jaws (not while it may not be
 * held), else knocks it over */
void DC2CALL TRex_ChargeHit(gptr o)                                 /* 0x4253B0 */
{
    static const u8 anim[2] = {0xF, 0x11}, plan[2] = {0x22, 0x23};
    static const u32 eff[2] = {0x658A44, 0x658D94}, pleff[2] = {0x659B7C, 0x659B7C},
                     miss[2] = {0x658928, 0x658C78};
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    if (!Flag_Check(2, 0x1A) && !Bits_Check(TR_BITS(o), 0xC)) {
        U8(o + 0x56)++;
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 2);
        Flash_Play(o, 0x65BA6C);
        Effect_Attach(o, eff[U8(o + 0x57)]);
        U32(o + 0x50) &= ~8u;
        stop(o);
        Player_Hold(o, 0xFA, 0x4255D0);
        Anim_Set(pl, U32(U32(o + 0x88) + plan[U8(o + 0x57)] * 4), 0, 0x40, 0);
        Effect_Attach(pl, pleff[U8(o + 0x57)]);
        Player_Bleed(0x1F4);
        U32(pl + 0x50) |= 0x120;
        Flag_Set(1, 1, 1);
        U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
        U16(pl + 0x4A) = U16(o + 0x4A);
        Obj_SyncLinks(pl);
        Obj_UpdateMatrix(pl);
        Obj_CollideWalls(pl);
        return;
    }
    s32 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65BA5C);
    if (U8(G() + 0x108F) != 3) Effect_Attach(o, miss[U8(o + 0x57)]);
    if (iabs(a) < 0x400) Player_Hurt(o, 0xFA, 2, 0xA);
    else Player_Hurt(o, 0xFA, 1, 0);
}

/* the player in its jaws after the charge (the player's callback): thrown
 * off to the side when the animation ends */
void DC2CALL TRex_PlBitten(gptr pl)                                 /* 0x4255D0 */
{
    static const u16 turn[2] = {0xFC00, 0x400};
    gptr t = U32(pl + 0x1D8);
    if (U8(pl + 0x57)) return;
    if (U16(pl + 0x92) >= 0x14) Flag_Set(1, 1, 0);
    if (!Anim_Step(pl)) return;
    U32(pl + 0x50) &= ~0x120u;
    Flag_Set(1, 1, 0);
    gptr m = U32(pl + 0x64);
    U16(m + 0x4A) = (u16)((turn[U8(t + 0x57)] + U16(m + 0x4A)) & 0xFFF);
    U16(pl + 0x4A) = (u16)((turn[U8(t + 0x57)] + U16(pl + 0x4A)) & 0xFFF);
    Anim_Set(pl, 0xD, 0, 0, 0);
    Obj_UpdateMatrix(pl);
    U32(pl + 0x54) = 0x30302;
}

/* ---- hit (state 2) ---- */

/* the damage: by the kind and the weapon (o+0x176); flinches when enough was
 * taken, staggers on heavy damage; the gate kind keeps half its health */
void DC2CALL TRex_Damage(gptr o)                                    /* 0x425740 */
{
    static const u32 eff[3] = {0x659A08, 0x659A84, 0x659B28};
    gptr b = TR_BITS(o);
    s32 hp0 = S16(o + 0x108);
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    if (U8(o + 0x17D) == 4) { U16(o + 0x172) = 4; U16(o + 0x176) = (u16)((s32)U16(o + 0x176) / 3); }
    u32 kind = U32(o + 0x1C4);
    if (kind == 2) {
        switch (U8(G() + 0x108F)) {
        case 0: case 1: case 3: case 5: case 6: case 8:
            U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1)); break;
        case 7: U16(o + 0x176) = (u16)(U16(o + 0x176) * 3); break;
        case 9: U16(o + 0x176) >>= 2; break;
        case 10: U16(o + 0x176) >>= 1; break;
        default: U16(o + 0x176) <<= 1; break;
        }
    }
    switch (kind) {
    case 0:
        U32(o + 0x54) = U32(o + 0x1C8);
        U32(o + 0x1CC) += U16(o + 0x176);
        if ((s32)U32(o + 0x1CC) > half(S16(o + 0x10A))) { U32(o + 0x1CC) = 0; U32(o + 0x54) = 0x102; }
        break;
    case 1:
        U32(o + 0x54) = 0x102;
        U32(o + 0x1CC) += U16(o + 0x176);
        U32(o + 0x1D4) += U16(o + 0x176);
        if ((s32)U32(o + 0x1CC) > half(S16(o + 0x10A))) { U32(o + 0x1CC) = 0; U32(o + 0x54) = 0x202; }
        if (U16(o + 0x172) == 3) U32(o + 0x54) = 0x302;
        break;
    case 2:
        U32(o + 0x54) = U32(o + 0x1C8);
        U32(o + 0x1CC) += U16(o + 0x176) >> 1;
        if ((s32)U32(o + 0x1CC) > half(S16(o + 0x10A))) { U32(o + 0x1CC) = 0; U32(o + 0x54) = 0x102; }
        if (U16(o + 0x172) == 3) U32(o + 0x54) = 0x102;
        break;
    }
    Bits_Set(b, 8, 0);
    if (U8(o + 0x55) == 1 && U32(o + 0x1C4)) {
        s32 m = S16(o + 0x10A) / 4 * 3;
        if ((s32)U32(o + 0x1D4) > m) {
            U32(o + 0x1D4) = 0; U32(o + 0x1D0) = 0;
            Bits_Set(b, 8, 1);
            U32(o + 0x54) = 0x202;
        }
    }
    if (U8(o + 0x1C9) == 6) U32(o + 0x54) = U32(o + 0x1C8);
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Enemy_BiteOff(o);
        stop(o);
        Bits_Set(b, 2, 0);
        Bits_Set(b, 3, 0);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 0xC, 0);
        if (U32(o + 0x1C4) && (U8(o + 0x184) & 8)) Score_Event(o);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x659FBC);
        U16(o + 0x108) -= U16(o + 0x176);
        s16 h = S16(o + 0x108);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 0x14 : 0xA;
        switch (U32(o + 0x1C4)) {
        case 0:
            if (h < 0) U16(o + 0x108) = 1;
            break;
        case 1:
            if (h < 0) {
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = 0x103;
                Score_Hit(o, 0x3E8);
            } else {
                u32 v = (u32)(hp0 - h) / 5 - (s32)(s8)U8(o + 0x207) * 100;
                if ((s16)v < 100) v = 100;
                Score_Hit(o, v);
            }
            break;
        case 2:
            if (h < 0) {
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = 0x203;
                Score_Kill(o);
            }
            break;
        }
    } else {
        U16(o + 0x108) += (u16)((s32)U16(o + 0x176) / -5);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 0x14 : 0xA;
        Effect_Attach(o, eff[(s32)Rand8() % 3]);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65A498);
        if (U32(o + 0x1C4) == 2) {
            if (S16(o + 0x108) < 0) {
                Player_Release(o);
                Enemy_BiteOff(o);
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = 0x203;
                Score_Kill(o);
            }
        } else if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(TR_MAIN + U8(o + 0x54) * 4), o);
}

static void flinch_start(gptr o)
{
    static const u8 anim[2] = {0x1C, 0x1D};
    static const u32 eff[2] = {0x659618, 0x65984C};
    U8(o + 0x56)++;
    U8(o + 0x57) = (u8)Anim_FrameFlag(o);
    Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 5);
    Effect_Attach(o, eff[U8(o + 0x57)]);
}

void DC2CALL TRex_Flinch(gptr o)                                    /* 0x425CA0 */
{
    look(o, 0x23);
    switch (U8(o + 0x56)) {
    case 0:
        flinch_start(o);
        break;
    case 1:
        Anim_Step(o);
        switch (U32(o + 0x1C4)) {
        case 0: if (U16(o + 0x92) >= 0x18) U32(o + 0x54) = 1; break;
        case 2:
            if (U16(o + 0x172) == 3) { if (U16(o + 0x92) >= 5) U32(o + 0x54) = 1; break; }
            /* fall through */
        case 1: if (U16(o + 0x92) >= 0xA) U32(o + 0x54) = 1; break;
        }
        break;
    }
    if (U16(o + 0x92) == 0x1E || U16(o + 0x92) == 0x2D) quake(o);
}

void DC2CALL TRex_FlinchHard(gptr o)                                /* 0x425DC0 */
{
    look(o, 0x23);
    switch (U8(o + 0x56)) {
    case 0:
        flinch_start(o);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        if (Bits_Check(TR_BITS(o), 8)) {
            U8(o + 0x17B) = 0xA;
            if (U16(o + 0x92) >= 0x29) U32(o + 0x54) = ((u32)U8(o + 0x57) << 24) | 0x701;
        }
        break;
    }
    if (U16(o + 0x92) == 0x1E || U16(o + 0x92) == 0x2D) quake(o);
}

void DC2CALL TRex_Reel(gptr o)                                      /* 0x425ED0 */
{
    static const u8 anim[2] = {0x1E, 0x1F};
    look(o, 0x23);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, anim[Anim_FrameFlag(o)], 0, 0x40, 5);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
    u16 f = U16(o + 0x92);
    if (f == 13 || f == 27 || f == 100 || f == 115) quake(o);
    if (U16(o + 0x92) < 0x32) U8(o + 0x11B) = 0;
}

/* ---- die (state 3) ---- */

static const u8 die_anim[2] = {9, 0xA};
static const u32 die_eff[2] = {0x65A974, 0x65AFE0};

static void fall_start(gptr o)
{
    U8(o + 0x56)++;
    U8(o + 0x57) = (u8)Anim_FrameFlag(o);
    Anim_Start(o, die_anim[U8(o + 0x57)], 0, 0x40, 5);
    Effect_Attach(o, die_eff[U8(o + 0x57)]);
    U32(o + 0x50) &= ~8u;
    stop(o);
    Bits_Set(TR_BITS(o), 1, 0);
}

/* the scripted kind goes down, and gets up again with fresh health */
void DC2CALL TRex_Down(gptr o)                                      /* 0x426010 */
{
    gptr b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        fall_start(o);
        if ((s8)U8(o + 0x207) < 4) U8(o + 0x207)++;
        Flag_Set(7, 0x1F, 1);
        Flash_Play(o, 0x65BA24);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Bits_Set(b, 0xA, 1);
        }
        if (U16(o + 0x92) == 0x50) Shake_Set(5, 0, 0x32);
        break;
    case 3:
        Bits_Set(b, 0xA, 0);
        U16(o + 0x108) = 0x1388;
        U32(o + 0x50) = (U32(o + 0x50) & ~0x30u) | 8;
        if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x2710;
        if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
        U32(o + 0x1CC) = 0;
        U32(o + 0x1D4) = 0;
        Enemy_BiteOn(o);
        U32(o + 0x54) = 0x401;
        Flag_Set(7, 0x1F, 0);
        break;
    }
}

void DC2CALL TRex_Dead(gptr o)                                      /* 0x4261D0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        fall_start(o);
        Flash_Play(o, 0x65BA24);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        if (U16(o + 0x92) == 0x50) {
            Shake_Set(5, 0, 0x32);
            if (U16(o + 0x92) == 0x50) Prop_SpawnC(1, o + 0x40, 2, 0x64, 1, 0);
        }
        break;
    case 2:
        if (U32(o + 0x1C4) == 2) Obj_DeathFade(o);
        break;
    }
}
