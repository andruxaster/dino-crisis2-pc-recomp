/* The pteranodon of area 0 (area object type 24, handler 0x431F70): a
 * lighter kin of the type 4 pteranodon (pteranodon.c). It circles, swoops
 * along an arc about the player, perches and dives at it (a peck or a pass
 * with the claws); it never carries the player off. While it does not
 * attack (bit 3 of o+0x1C0 clear after its state) it drifts along +z at
 * o+0x202 (sped up for the frames o+0x204 when it bumped into something).
 * States (o+0x54): 0 init, 1 normal (think 0x720BC4, then move 0x720BE4 by
 * o+0x55), 2 hit (0x720C04), 3 die (0x720C14), 4 (0x4A56D0). Attacks land
 * through 0x720C24. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define P2_MAIN  0x720B80u      /* state handlers */
#define P2_THINK 0x720BC4u      /* state 1: deciding, by o+0x55 */
#define P2_MOVE  0x720BE4u      /* state 1: moving, by o+0x55 */
#define P2_HIT   0x720C04u      /* state 2 */
#define P2_DIE   0x720C14u      /* state 3 */
#define P2_GRAB  0x720C24u      /* an attack landed, by o+0x55 */
#define P2_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u      /* bit 0: one holds the player */
#define PL()     U32(G() + 0x940)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static u16 y_level(gptr o) { return (u16)((s32)S16(o + 0x42) / -1000); }
static void approach(gptr p, u32 step, u32 target) { U16(p) += (u16)Angle_Approach(U16(p), step, target); }
static void level(gptr o) { approach(o + 0x48, 0x28, 0); approach(o + 0x4C, 0x28, 0); }

void DC2CALL Pt2_Type(gptr o)                                       /* 0x431F70 */
{
    Pt2_Handler(o);
}

void DC2CALL Pt2_Handler(gptr o)                                    /* 0x431F80 */
{
    gptr g = G(), pl = U32(g + 0x940), b = P2_BITS(o);
    U8(o + 0x206) = U8(g + 0x108F) ? 1 : 0;
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    Bits_Set(b, 3, 0);
    Obj_SavePos(o);
    U16(o + 0x10E) = (u16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
    Orig_Call1(U32(P2_MAIN + U8(o + 0x54) * 4), o);
    if (!Bits_Check(b, 3)) {                            /* the drift along +z */
        if (U16(o + 0x204)) {
            U16(o + 0x204)--;
            Value_Approach(o + 0x202, 0x1E, 0x320);
            if (S16(o + 0x44) > 0x4650) U16(o + 0x204) = 0;
        } else Value_Approach(o + 0x202, 0x1E, 0x64);
        U16(o + 0x44) += U16(o + 0x202);
        if ((U16(o + 0xE8) || U16(o + 0xEA) || U16(o + 0xEC)) && S16(o + 0x44) < 0)
            U16(o + 0x204) = (u16)((Rand8() & 0x3F) + 0x5A);
    }
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    u16 f = U16(o + 0x10E);
    if (S16(o + 0x42) > (s16)f) U16(o + 0x42) = f;
    if (S16(o + 0x42) < -0x3A98) U16(o + 0x42) = (u16)-0x3A98;
    U16(o + 0x1FE) = (u16)(f - 0x1388);                 /* 5000 over the floor, no lower than over the player */
    if ((s32)(s16)(f - 0x1388) > (s32)S16(pl + 0x42) - 0x1388) U16(o + 0x1FE) = (u16)(U16(pl + 0x42) - 0x1388);
    U8(o + 0x118) |= 0x80;
    s16 h = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x118) & 0xF);
    s16 hp = (s16)Floor_Height(p + 0x40, p + 0x40, 0x40, 0);
    if (U8(G() + 0x1091) == 1 && U8(G() + 0x1090) == 0xC && h >= 0) U8(o + 0x118) &= 0x7F;
    if (hp > h) U8(o + 0x118) &= 0x7F;
    U16(o + 0x5A) = y_level(o);
    Ptera_Timer(o);
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U16(o + 0x1C8)) U16(o + 0x1C8)--;
    if (U16(o + 0x1FA)) U16(o + 0x1FA)--;
    Pt2_Grab(o);
    U8(o + 0x1CE) = U8(o + 0x56); U8(o + 0x1CC) = U8(o + 0x54);
    U8(o + 0x1CD) = U8(o + 0x55); U8(o + 0x1CF) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: health (1 on the first difficulty), model, animations */
void DC2CALL Pt2_Init(gptr o)                                       /* 0x432240 */
{
    gptr pl = PL();
    U32(o + 0x50) = 0xB;
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x12C;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x258;
    if (!U8(G() + 0x108E)) U16(o + 0x108) = 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720B98; U32(o + 0x128) = 0x720BA8; U32(o + 0x134) = 0x720BB8;
    stop(o);
    U16(o + 0x5A) = y_level(o);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x642348);
    U32(o + 0x88) = 0x644924;
    Anim_Start(o, 0x17, 0, 1, 5);
    Obj_Set118(o, 0, 0x1FA0, 0x1FA0);
    DrawSync(o);
    U8(o + 0x15F) = 2; U8(o + 0x160) = 1;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0x80; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(P2_BITS(o), 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF88) = 0;
    U16(o + 0x1FE) = U16(o + 0x42); U16(o + 0x200) = U16(o + 0x42);
    U8(o + 0x118) |= 0x20;
    U32(pl + 0x50) |= 0x80;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Pt2_Normal(gptr o)                                     /* 0x4323E0 */
{
    Orig_Call1(U32(P2_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(P2_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Pt2_Hit(gptr o)                                        /* 0x432410 */
{
    Orig_Call1(U32(P2_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Pt2_Die(gptr o)                                        /* 0x432430 */
{
    Orig_Call1(U32(P2_DIE + U8(o + 0x55) * 4), o);
}

/* its attacks hit the player: by the sub-state */
void DC2CALL Pt2_Grab(gptr o)                                       /* 0x432450 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(P2_GRAB + U8(o + 0x55) * 4), o);
}

/* ---- deciding (state 1, before the move) ---- */

/* sub-state 0: circling; at a middle distance now and then it swoops (unless another holds the player) */
void DC2CALL Pt2_ThinkFly(gptr o)                                   /* 0x432480 */
{
    gptr pl = PL();
    if (!U8(o + 0x56)) return;
    gptr v = Tmp_Push(8);
    if (U8(pl + 0x54) == 1 && U8(pl + 0x55) == 2 && !U16(pl + 0xE8) && !U16(pl + 0xEA) && !U16(pl + 0xEC)) {
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1F40;  /* where the running player will be */
        Vec_RotY(v, v, U16(pl + 0x4A));
        U16(v) += U16(pl + 0x40); U16(v + 2) += U16(pl + 0x42); U16(v + 4) += U16(pl + 0x44);
    } else {
        U16(v) = U16(pl + 0x40); U16(v + 2) = U16(pl + 0x42); U16(v + 4) = U16(pl + 0x44);
    }
    u32 d2 = Dist2_XZ(o + 0x40, v);
    s16 a = (s16)Angle_DiffTo(o + 0x40, v, U16(o + 0x4A));
    Tmp_Pop(8);
    if (d2 < 0xD693A40 && d2 > 0x5F5E100 && Rand8() < 0x40 && !Bits_Check(SHARED, 0)) {
        Enemy_BiteOn(o);
        if (Rand8() < 0x40) { U16(o + 0x1C8) = 0x53; U16(o + 0x1FA) = 0x1E; }
        else { U16(o + 0x1C8) = 0x35; U16(o + 0x1FA) = 0; }
        U32(o + 0x54) = 0x201;
        return;
    }
    if (U16(o + 0x1E4)) { U16(o + 0x1E4)--; return; }
    if (iabs(a) < 0x60 && !U16(o + 0xE8) && !U16(o + 0xEA) && !U16(o + 0xEC)) U8(o + 0x57) = 0;
    else if (a < 0) { if (U8(o + 0x57) != 2) U16(o + 0x1E6) = 100; U8(o + 0x57) = 2; }
    else { if (U8(o + 0x57) != 1) U16(o + 0x1E6) = 100; U8(o + 0x57) = 1; }
    if (Rand8() > 0x40) { U16(o + 0x1E4) = (u16)((Rand8() & 0x3F) + 0x1E); return; }
    U32(o + 0x54) = 0x101;
}

/* sub-state 2: the swoop along an arc about the player; at its end it perches when facing the player */
void DC2CALL Pt2_ThinkSwoop(gptr o)                                 /* 0x432A10 */
{
    gptr pl = PL();
    if (!U8(o + 0x56)) return;
    u32 d = (u32)SquareRoot0((s32)Dist2_XZ(o + 0x40, pl + 0x40));
    Obj_TurnToPoint(o, 0x28, pl + 0x40);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0;
    U16(v + 4) = (u16)(U8(pl + 0x174) & 2 ? d / 30 : d / 80);
    Vec_RotY(v, v, (u16)(Angle_XZ(o + 0x40, pl + 0x40) + U16(o + 0x1E2)));
    U16(o + 0x44) += U16(v + 4); U16(o + 0x40) += U16(v); U16(o + 0x42) += U16(v + 2);
    Tmp_Pop(8);
    if (U8(o + 0x56) != 3) return;
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    if (iabs(a) < 0x60 && !U16(o + 0x1FA) && !U32(o + 0x1D4)) { Enemy_BiteOn(o); U32(o + 0x54) = 0x301; return; }
    U16(o + 0x1E4)++;
}

/* sub-state 3: perched, turned to the player */
void DC2CALL Pt2_ThinkPerch(gptr o)                                 /* 0x432D00 */
{
    if (U8(o + 0x56)) Obj_TurnToPoint(o, 0x28, PL() + 0x40);
}

/* ---- moving (state 1) ---- */

/* its height to o+0x1FE, banking by the turn o+0x57 (0 straight, 1 right, 2 left) */
void DC2CALL Pt2_Steer(gptr o)                                      /* 0x4327D0 */
{
    Value_Approach(o + 0x42, 0x14, U16(o + 0x1FE));
    switch (U8(o + 0x57)) {
    case 0: approach(o + 0x1DE, 2, 0); approach(o + 0x4C, 0xD, 0); break;
    case 1: approach(o + 0x1DE, 2, 0x28); approach(o + 0x4C, 0xD, 0x200); break;
    case 2: approach(o + 0x1DE, 2, 0xFFD8); approach(o + 0x4C, 0xD, 0xFE00); break;
    }
    U16(o + 0x4A) += U16(o + 0x1DE);
}

/* sub-state 0: flies about (a flap now and then), heading kept within 0x700..0x900 */
void DC2CALL Pt2_Fly(gptr o)                                        /* 0x432680 */
{
    gptr b = P2_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x17, 0, 9, 0xA);
        U16(o + 0x1E4) = 0; U16(o + 0x48) = 0; U16(o + 0x4C) = 0;
        U16(o + 0xF0) = 0; U16(o + 0xF2) = 0; U16(o + 0xF4) = 0x258;
        U32(o + 0x50) &= ~0x30u;
        if (Bits_Check(b, 2)) {
            U8(o + 0x57) = Rand8() & 1 ? 1 : 2;
            U16(o + 0x1E4) = 10; U16(o + 0x1E6) = 100;
            Bits_Set(b, 2, 0);
        }
        break;
    case 1: Anim_Step(o); break;
    case 2: U8(o + 0x56)++; Anim_Start(o, 0x18, 0, 0, 0xA); break;
    case 3: if (Anim_Step(o)) { U8(o + 0x56) = 1; Anim_Start(o, 0x17, 0, 9, 0xA); } break;
    }
    Pt2_Steer(o);
    if (S16(o + 0x4A) > 0x900) approach(o + 0x4A, 0x28, 0x900);
    if (S16(o + 0x4A) < 0x700) approach(o + 0x4A, 0x28, 0x700);
}

/* sub-state 1: rises back to its height after a swoop */
void DC2CALL Pt2_Rise(gptr o)                                       /* 0x432890 */
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x1C, 0, 1, 0xA);
        U16(o + 0x1E8) = 0;
        break;
    case 1:
        Anim_Step(o);
        if (++S16(o + 0x1E8) > 5) U8(o + 0x56)++;
        break;
    case 2:
        Anim_Step(o);
        Value_Approach(o + 0xF0, 0x32, 0); Value_Approach(o + 0xF2, 0x14, (u32)-300); Value_Approach(o + 0xF4, 0x14, 0x258);
        if (S16(o + 0x42) < S16(o + 0x1FE) + 0x5DC && (U16(o + 0x92) == 0x12 || U16(o + 0x92) == 0x26)) U8(o + 0x56)++;
        break;
    case 3:
        Anim_Step(o);
        Value_Approach(o + 0xF0, 0x32, 0); Value_Approach(o + 0xF2, 0x28, (u32)-100); Value_Approach(o + 0xF4, 0x28, 0x258);
        if (S16(o + 0x42) < S16(o + 0x1FE)) { U32(o + 0x54) = 1; Bits_Set(P2_BITS(o), 2, 1); }
        break;
    }
}

/* sub-state 2: the swoop down at the player */
void DC2CALL Pt2_Swoop(gptr o)                                      /* 0x432B40 */
{
    gptr pl = PL();
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0x20, 0, 0x40, 0xA);
        U16(o + 0x1F2) = U16(pl + 0x42); U16(o + 0x1E4) = 0; U16(o + 0x1E6) = 0; U16(o + 0x1FC) = 0x96;
        U16(o + 0x1E2) = Rand8() & 1 ? 0x400 : 0xFC00;
        break;
    case 1:
        if (Anim_Step(o)) { U16(o + 0xF0) = 0; U16(o + 0xF2) = 0x14B; U16(o + 0xF4) = 0xF1; }
        if (U16(o + 0x1FC)) U16(o + 0x1FC)--;
        if (S16(o + 0x42) < S16(o + 0x1F2) - 0xBB8 && U16(o + 0x1FC)) break;
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0x21, 0, 0x40, 0xA);
        break;
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; stop(o); Anim_Start(o, 0x22, 0, 0x41, 0xA); }
        break;
    case 3: Anim_Step(o); break;
    }
    Bits_Set(P2_BITS(o), 3, 1);
}

/* sub-state 3: perched; at the frame o+0x1F8 it dives (a peck when near, else the claws) or leaves */
void DC2CALL Pt2_Perch(gptr o)                                      /* 0x432D30 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x23, 0, 0x40, 0xA);
        U16(o + 0x1F8) = 0x10;
        break;
    case 1: {
        Anim_Step(o);
        if ((s32)U16(o + 0x92) < S16(o + 0x1F8)) break;
        gptr v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0xFD44; U16(v + 4) = 0;
        Ptera_HoldPoint(o, o + 0x1F0, v);
        Tmp_Pop(8);
        if (iabs((s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A))) >= 0x60) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; break; }
        U32(o + 0x54) = Dist2_XZ(o + 0x40, o + 0x1F0) < 0x17D7840 ? 0x401 : 0x501;
        break;
    }
    }
    Bits_Set(P2_BITS(o), 3, 1);
}

/* toward o+0x1F0 at speed sp (by step); true when it reached the player's height or ran out of time */
static int dive(gptr o, u32 sp, u32 step)
{
    gptr v = Tmp_Push(8);
    Vec_Toward(o + 0x40, o + 0x1F0, sp, v);
    Value_Approach(o + 0xF0, step, U32(v));
    Value_Approach(o + 0xF2, step, U32(v + 2));
    Value_Approach(o + 0xF4, step, U32(v + 4));
    Tmp_Pop(8);
    if (U16(o + 0x1FC)) U16(o + 0x1FC)--;
    return S16(o + 0x1F2) - 0x64 < S16(o + 0x42) || !U16(o + 0x1FC);
}

/* sub-state 4: dives at the player and pecks */
void DC2CALL Pt2_Peck(gptr o)                                       /* 0x432E40 */
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x1E, 0, 0, 5);
        U16(o + 0x1FC) = 0x96;
        Se_Play(o, 0x36);
        U32(o + 0x50) |= 0x30;
        break;
    case 1:
        Anim_Step(o);
        if (dive(o, 0x1F4, 0x64)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0x1F, 0, 0, 5);
            U16(o + 0xF4) = 0x258;
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    case 2:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
        if (U16(o + 0x92) <= 4) U16(o + 0xF2) += 0xFF9C;
        else U16(o + 0xF2) = 0xFF38;
        break;
    }
    Bits_Set(P2_BITS(o), 3, 1);
}

/* sub-state 5: dives at the player, a pass with the claws */
void DC2CALL Pt2_Claw(gptr o)                                       /* 0x432FD0 */
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x17, 0, 0, 5);
        U16(o + 0x1FC) = 0x96;
        Se_Play(o, 0x36);
        U32(o + 0x50) |= 0x30;
        U16(o + 0x1E8) = 0;
        break;
    case 1:
        Anim_Step(o);
        if (dive(o, 0x2BC, 0x14)) { U8(o + 0x56)++; U32(o + 0x50) &= ~0x30u; U16(o + 0x1E8) = 0; }
        break;
    case 2: {
        Anim_Step(o);
        s16 c = S16(o + 0x1E8);
        if (c <= 4) U16(o + 0xF2) += 0xFF9C;
        else U16(o + 0xF2) = 0xFF38;
        if (c == 4) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
        U16(o + 0x1E8)++;
        break;
    }
    }
    Bits_Set(P2_BITS(o), 3, 1);
}

/* the dive lands: a flash; from behind the player is knocked down */
static void strike(gptr o, gptr fl)
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 h = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, fl);
    if (iabs(h) < 0x400) Player_Hurt(o, 2, 2, 0x28);
    else Player_Hurt(o, 2, 1, 0);
}

void DC2CALL Pt2_PeckHit(gptr o)                                    /* 0x433160 */
{
    strike(o, 0x65D6E4);
}

void DC2CALL Pt2_ClawHit(gptr o)                                    /* 0x4331D0 */
{
    strike(o, 0x65D6D4);
}

/* ---- hit (state 2) ---- */

/* sub-state 0: the damage; every hit knocks it about (state 2 at once, so the original's light-hit path is dead) */
void DC2CALL Pt2_Damage(gptr o)                                     /* 0x433240 */
{
    if (!(U8(o + 0x1CC) == 1 && !U8(o + 0x1CD)) && (U8(o + 0x184) & 8)) U16(o + 0x176) += U16(o + 0x176) >> 1;
    if (U16(o + 0x172) == 4) U16(o + 0x176) <<= 1;
    U32(o + 0x54) = 0x102;
    Player_Release(o);
    Enemy_BiteOff(o);
    U16(o + 0x108) -= U16(o + 0x176);
    stop(o);
    U16(o + 0x1C8) = 0; U16(o + 0x1FA) = 0;
    U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 10;
    if (S16(o + 0x108) < 0) { U32(o + 0x50) &= ~8u; U16(o + 0x108) = 0xFFFF; }
    Orig_Call1(U32(P2_MAIN + U8(o + 0x54) * 4), o);
}

/* knocked about, turned to the hit (o+0x178) or away; dead: on to state 3 */
static void knocked(gptr o, u32 af, gptr ff, u32 ab, gptr fb, u16 dead_at, u32 dead, u16 up_at, u16 f2, u16 f4)
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (iabs((s16)Angle_Diff(U16(o + 0x178), U16(o + 0x4A))) < 0x400) {
            U16(o + 0x4A) = U16(o + 0x178);
            Anim_Start(o, af, 0, 0x40, 0);
            Effect_Attach(o, ff);
        } else {
            U16(o + 0x4A) = (U16(o + 0x178) - 0x800) & 0xFFF;
            Anim_Start(o, ab, 0, 0x40, 0);
            Effect_Attach(o, fb);
        }
        break;
    case 1:
        Anim_Step(o);
        if (S16(o + 0x108) < 0) { if (U16(o + 0x92) >= dead_at) U32(o + 0x54) = dead; }
        else if (U16(o + 0x92) >= up_at) { U32(o + 0x54) = 0x101; U16(o + 0xF0) = 0; U16(o + 0xF2) = f2; U16(o + 0xF4) = f4; }
        break;
    }
}

void DC2CALL Pt2_Knock(gptr o)                                      /* 0x4333A0 */
{
    knocked(o, 0x26, 0x65D2BC, 0x24, 0x65CDB4, 0x1E, 0x103, 0x1E, 0xFFC7, 0xA1);
}

void DC2CALL Pt2_KnockHard(gptr o)                                  /* 0x4334D0 */
{
    knocked(o, 0x27, 0x65D400, 0x25, 0x65CF48, 0xF, 0x203, 0x2C, 0xFFA1, 0xBE);
}

/* ---- die (state 3) ---- */

/* falls; off the cliff or into water it is gone (with a splash) */
void DC2CALL Pt2_Dying(gptr o)                                      /* 0x433600 */
{
    level(o);
    Obj_DeathFade(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x19, 0, 0, 5);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x20;
        U16(o + 0xF4) = U16(o + 0xD4);
        Combo_Kill(U8(o + 0x58));
        break;
    case 1: {
        if (Anim_Step(o)) Anim_Start(o, 0x1D, 0, 1, 5);
        Value_Approach(o + 0xF0, 0x32, 0);
        U16(o + 0xF2) += 0x32;
        gptr g = G();
        u8 st = U8(g + 0x1091), rm = U8(g + 0x1090);
        if (st == 1 && rm == 0xC && S16(o + 0x40) < (s16)0xE0C0) {     /* off the cliff: gone */
            s16 lim = S16(o + 0x44) > -0x4C7 ? -0x1194 : -0x1964;
            if (S16(o + 0x42) >= lim) { U32(o + 0x50) |= 0x40000000; break; }
        }
        if ((!st && rm == 1) || (st == 6 && rm == 2)) {     /* rooms with water */
            s16 w = (s16)Water_Level(o + 0x40);
            if (!U16(o + 0x10E) && (S16(o + 0x42) >= w || S16(o + 0x42) >= 0)) {
                if (w == 0x7FFF) w = 0;
                gptr k = K2_AllocBack();
                if (k) {
                    U8(k + 0x58) = 0x10; U8(k + 0x7A) = 5; U16(k + 0x6C) = 0x50; U16(k + 0x6E) = 0x46;
                    U16(k + 0x42) = (u16)w; U16(k + 0x40) = U16(o + 0x40); U16(k + 0x44) = U16(o + 0x44);
                }
                Se_Play(o, 0x4B);
                U32(o + 0x50) |= 0x40000000;
            }
        }
        break;
    }
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 0x1B, 0, 0, 5); }
        break;
    case 3:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Layers_Assign();
            Prop_SpawnC(0, o + 0x40, 5, 0x32, 1, 0);
        }
        break;
    }
}
