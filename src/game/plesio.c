/* The plesiosaurus (object type handler 0x42B750, type 5, E30.DAT). It keeps
 * its state at o+0x1C0.., a set of bits first (Bits_Set(o+0x1C0, n)):
 * 1 swimming (its head looks), 2 circles the other way, 3 has come up close
 * (the room's event 0xF), 4 the lunge from below is on (it circles faster),
 * 5 goes back out, 6 the event 0x10 seen, 7 dives away for good.
 * +0xC damage taken since the last flinch, +0x10 the wait after the player
 * was hurt or held, +0x28 circling angle, +0x2A circling speed, +0x2C the
 * heading's offset from the circle, +0x2E its turn toward the player,
 * +0x30..0x34 timers, +0x38 the player's last squared distance.
 * States (o+0x54): 0 init, 1 normal (think 0x72084C, then move 0x720864 by
 * o+0x55), 2 hit (0x72087C), 3 die (0x72088C), 4 (0x4A56D0). Attacks land
 * through 0x72089C.
 * Moves (o+0x55): 0 swim, 1 turn about, 2 bite (the player on land), 3 lunge
 * from close by, 4 bite to the side (the player down in the water), 5 the
 * lunge from below. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define PS_MAIN  0x720838u      /* state handlers */
#define PS_THINK 0x72084Cu      /* state 1: deciding, by o+0x55 */
#define PS_MOVE  0x720864u      /* state 1: moving, by o+0x55 */
#define PS_HIT   0x72087Cu      /* state 2 */
#define PS_DIE   0x72088Cu      /* state 3 */
#define PS_GRAB  0x72089Cu      /* an attack landed, by o+0x55 */
#define PS_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u      /* bit 0: one holds the player */
#define PL()     U32(G() + 0x940)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }

/* back to circling from the start (bits 2/3 off) */
static void circle_reset(gptr o)
{
    gptr b = PS_BITS(o);
    Bits_Set(b, 3, 0);
    Bits_Set(b, 2, 0);
    U16(o + 0x1EE) = 0; U16(o + 0x1EA) = 0x6E; U16(o + 0x1EC) = 0xFC00; U16(o + 0x1E8) = 0x800;
}

/* ---- the handler ---- */

void DC2CALL Plesio_Handler(gptr o)                                 /* 0x42B750 */
{
    gptr g = G(), pl = U32(g + 0x940), b = PS_BITS(o);
    U8(o + 0x1F6) = U8(g + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    if (U8(G() + 0x7C4) == 0xF) Bits_Set(b, 3, 1);
    Orig_Call1(U32(PS_MAIN + U8(o + 0x54) * 4), o);
    for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {    /* parts darken as the player comes near */
        u32 d = (u32)SquareRoot0((s32)Dist2_XZ(p + 0x40, pl + 0x40));
        if (d > 20000) d = 20000;
        s16 v = (s16)(u16)(d * 96 / 20000);
        if (v < 0) v = 0;
        else if (v > 0x60) v = 0x60;
        u8 c = (u8)(0x80 - v);
        U8(p + 0x8C) = c; U8(p + 0x8D) = c; U8(p + 0x8E) = c;
    }
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    s16 h = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
    U16(o + 0x10E) = (u16)h;
    if (S16(o + 0x42) > h - 5000) U16(o + 0x42) = (u16)(h - 5000);
    Obj_UpdateMatrix(o);
    U32(o + 0x50) |= 0x20;
    if (Bits_Check(b, 3) && !Bits_Check(b, 5)) U32(o + 0x50) &= ~0x10u;
    else U32(o + 0x50) |= 0x10;
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Plesio_Wait(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    if (U16(o + 0x1F0)) U16(o + 0x1F0)--;
    if (U16(o + 0x1F2)) U16(o + 0x1F2)--;
    if (U16(o + 0x1F4)) U16(o + 0x1F4)--;
    Plesio_Grab(o);
    U8(o + 0x1C8) = U8(o + 0x54); U8(o + 0x1C9) = U8(o + 0x55);
    U8(o + 0x1CA) = U8(o + 0x56); U8(o + 0x1CB) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: health, model, animations; the room's event 8 starts it circling */
void DC2CALL Plesio_Init(gptr o)                                    /* 0x42B9A0 */
{
    gptr pl = PL(), b = PS_BITS(o);
    U32(o + 0x50) = 0xD;
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x2710;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x4E20;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x7207D0; U32(o + 0x128) = 0x7207F8; U32(o + 0x134) = 0x720808;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x642984;
    Anim_Start(o, 0, 0, 1, 5);
    Obj_Set118(o, 0, 0x3AF8, 0x4E80);
    DrawSync(o);
    Head_Reset(o);
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U8(o + 0x15F) = 8; U8(o + 0x160) = 7;
    U16(o + 0x16A) = 0x200; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(b, 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF14) = 0;
    U8(o + 0xFE) = 3;
    U8(o + 0x118) |= 0x20;
    U32(o + 0x184) |= 4;
    U32(pl + 0x50) |= 0x80;
    if (U8(0x87EEC9) == 8 && !U8(0x87EEC8)) circle_reset(o);
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Plesio_Normal(gptr o)                                  /* 0x42BBA0 */
{
    Orig_Call1(U32(PS_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(PS_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Plesio_Hit(gptr o)                                     /* 0x42BBD0 */
{
    Orig_Call1(U32(PS_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Plesio_Die(gptr o)                                     /* 0x42BBF0 */
{
    Orig_Call1(U32(PS_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Plesio_Grab(gptr o)                                    /* 0x42BC10 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(PS_GRAB + U8(o + 0x55) * 4), o);
}

/* the wait after the player was hurt or held: kept up while it is */
u32 DC2CALL Plesio_Wait(gptr o)                                     /* 0x42CCA0 */
{
    gptr pl = PL();
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D0) = 0x3C;
        if (!U8(G() + 0x108E)) U32(o + 0x1D0) = 0x78;
    }
    return U32(o + 0x1D0);
}

/* ---- moving about (every think) ---- */

/* dives away (bit 7), comes up close (bit 3) and goes back out (bit 5), or
 * circles the room's middle, turning its head toward the player */
void DC2CALL Plesio_Swim(gptr o)                                    /* 0x42BFF0 */
{
    gptr pl = PL(), b = PS_BITS(o);
    if (Bits_Check(b, 7)) {
        U16(o + 0x42) -= 100; U16(o + 0x44) -= 100;
        U32(o + 0x50) &= ~8u;
        U16(o + 0xF4) = 0;
        if (S16(o + 0x42) < -30000) U32(o + 0x50) |= 0x40000000;
        if (S16(o + 0x44) < -20000) U16(o + 0x44) = (u16)-20000;
        if (U8(G() + 0x7C4) == 0x10) return;
        U32(o + 0x50) |= 0x40000000;
        return;
    }
    if (Bits_Check(b, 3)) {
        if (Bits_Check(b, 5)) {                         /* back out to its place */
            gptr t = Tmp_Push(8);
            Value_Approach(o + 0xF4, 0x14, 0xC8);
            U16(t) = 0x2AF8; U16(t + 2) = U16(o + 0x42); U16(t + 4) = 0x16A8;
            Obj_TurnToPoint(o, 0x32, t);
            u32 d = Dist2_XZ(o + 0x40, t);
            Tmp_Pop(8);
            if (!Pos_Hidden(o + 0x40) || d < 0x895440) circle_reset(o);
        } else {                                        /* up and at the player */
            Value_Approach(o + 0x42, 0x64, (u32)-7000);
            u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
            if (d < 0x8954400) Value_Approach(o + 0xF4, 0x14, (u32)-200);
            if (d > 0x9502F90) Value_Approach(o + 0xF4, 0x14, 0xC8);
            if (S16(o + 0x44) > 6000) U16(o + 0x44) = 6000;
            Obj_TurnToPoint(o, 0x19, pl + 0x40);
        }
        u8 ev = U8(G() + 0x7C4);
        if (ev < 0xE || ev > 0x10) Bits_Set(b, 5, 1);
        else if (ev <= 0xF) { Bits_Set(b, 5, 0); Bits_Set(b, 6, 0); }
        else if (!Bits_Check(b, 6)) { Bits_Set(b, 6, 1); U16(o + 0x1F4) = 0x96; }
        return;
    }
    u32 st = Bits_Check(b, 4) ? 4 : 2;
    if (Bits_Check(b, 2)) {
        Value_Approach(o + 0x1EA, st, 0x6E);
        Value_Approach(o + 0x1EC, 0x20, 0x400);
    } else {
        Value_Approach(o + 0x1EA, st, (u32)-0x6E);
        Value_Approach(o + 0x1EC, 0x20, (u32)-0x400);
    }
    U16(o + 0x1E8) += (u16)(S16(o + 0x1EA) / 10);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x36B0;
    Vec_RotY(v, v, U16(o + 0x1E8) & 0xFFF);
    U16(o + 0x40) = (u16)(U16(v) - 5000);
    U16(o + 0x44) = (u16)(U16(v + 4) - 5000);
    Tmp_Pop(8);
    u32 a = Angle_DiffTo(o + 0x40, pl + 0x40, (u16)(U16(o + 0x1EC) + U16(o + 0x1E8)));
    if (iabs((s16)a) < 0x340 && !U16(o + 0x1F0)) {
        U16(o + 0x1EE) += (u16)Angle_Approach(U16(o + 0x1EE), 8, a);
        if (S16(o + 0x1EE) > 0x1A0) U16(o + 0x1EE) = 0x1A0;
        if (S16(o + 0x1EE) < -0x1A0) U16(o + 0x1EE) = (u16)-0x1A0;
    } else {
        U16(o + 0x1EE) += (u16)Angle_Approach((a & 0xFFFF0000u) | U16(o + 0x1EE), 8, 0);
    }
    U16(o + 0x4A) = (u16)(U16(o + 0x1EE) + U16(o + 0x1EC) + U16(o + 0x1E8));
    if (S16(o + 0x42) > S16(pl + 0x42) - 2000) Value_Approach(o + 0xF2, 10, (u32)-50);
    if (S16(o + 0x42) < S16(pl + 0x42) - 3000) Value_Approach(o + 0xF2, 10, 0x32);
}

/* an attack when the player is near and in front: 1 when one started */
u32 DC2CALL Plesio_Decide(gptr o)                                   /* 0x42BE70 */
{
    gptr pl = PL(), b = PS_BITS(o);
    if (Bits_Check(b, 7)) return 1;
    if (Bits_Check(SHARED, 0) || U32(o + 0x1D0)) return 0;
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (Bits_Check(b, 3)) {                             /* close by: the lunge */
        if (iabs(a) >= 0x100 || d >= 0xF424000) return 0;
        Enemy_BiteOn(o);
        u8 k = (u8)(~Rand8() & 1);
        U32(o + 0x54) = (u32)k << 24 | 0x301;
        return 1;
    }
    if (iabs(a) >= 0x200 || d >= 0xF424000) return 0;
    Enemy_BiteOn(o);
    if (S16(pl + 0x42) > -5000) U32(o + 0x54) = 0x201;  /* on land */
    else U32(o + 0x54) = (u32)(a < 0) << 24 | 0x401;    /* down in the water: from the side */
    return 1;
}

/* ---- deciding and moving (state 1) ---- */

/* sub-state 0: the attacks; far and going away: turn about */
void DC2CALL Plesio_ThinkSwim(gptr o)                               /* 0x42BC40 */
{
    gptr pl = PL(), b = PS_BITS(o);
    Plesio_Swim(o);
    if (Plesio_Decide(o)) return;
    if (Bits_Check(b, 3)) return;
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    if (!U16(o + 0x1F2) && U32(o + 0x1F8) < d && d > 0x134FD900) {
        u8 k;
        if (Bits_Check(b, 2)) { Bits_Set(b, 2, 0); k = 1; }
        else { Bits_Set(b, 2, 1); k = 0; }
        U16(o + 0x1F2) = 0xD2;
        U32(o + 0x54) = (u32)k << 24 | 0x101;
    }
    U32(o + 0x1F8) = d;
}

void DC2CALL Plesio_Swimming(gptr o)                                /* 0x42BD10 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 1, 0x14);
        Bits_Set(PS_BITS(o), 1, 1);
        Effect_Attach(o, 0x654C7C);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        if (Rand8() & 1) { Effect_Attach(o, 0x654C7C); Se_Play(o, 0x39); }
        else Se_Play(o, 0x3A);
        break;
    }
}

void DC2CALL Plesio_ThinkTurn(gptr o)                               /* 0x42BDB0 */
{
    Plesio_Swim(o);
    Plesio_Decide(o);
}

/* sub-state 1: turns about (o+0x57 the way) */
void DC2CALL Plesio_Turn(gptr o)                                    /* 0x42BDD0 */
{
    static const u8 anim[2] = { 2, 3 };
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, anim[U8(o + 0x57)], 6, 0, 0x14);
        Bits_Set(PS_BITS(o), 1, 0);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x34) U32(o + 0x54) = 1;
        break;
    }
}

void DC2CALL Plesio_ThinkBite(gptr o)                               /* 0x42C400 */
{
    Plesio_Swim(o);
}

/* an attack's animation to its end; at frame f the wait before the next */
static void attack_end(gptr o, u32 f)
{
    if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
    else if (U16(o + 0x92) == f) U16(o + 0x1F0) = 0x3C;
}

/* sub-state 2: the bite at the player on land */
void DC2CALL Plesio_Bite(gptr o)                                    /* 0x42C410 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 4, 0xC, 0, 0x14);
        Effect_Spawn(o, 0x65536C, 0x19, 0xFF, 0x100, 0);
        break;
    case 1:
        attack_end(o, 0x4F);
        break;
    }
}

void DC2CALL Plesio_ThinkLunge(gptr o)                              /* 0x42C4A0 */
{
    Plesio_Swim(o);
}

/* a two-sided attack (o+0x57 the side): its animation, its sound effect, the frame its wait starts */
static void attack_side(gptr o, const u8 *anim, const gptr *eff, u32 f)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, anim[U8(o + 0x57)], 0xF, 0, 0x14);
        Effect_Spawn(o, eff[U8(o + 0x57)], 0x1F, 0xFF, 0x100, 0);
        Bits_Set(PS_BITS(o), 1, 0);
        break;
    case 1:
        attack_end(o, f);
        break;
    }
}

/* sub-state 3: the lunge from close by */
void DC2CALL Plesio_Lunge(gptr o)                                   /* 0x42C4B0 */
{
    static const u8 anim[2] = { 7, 8 };
    static const gptr eff[2] = { 0x656F98, 0x6578FC };
    attack_side(o, anim, eff, 0x5A);
}

void DC2CALL Plesio_ThinkSide(gptr o)                               /* 0x42C590 */
{
    Plesio_Swim(o);
}

/* sub-state 4: the bite to the side, the player down in the water */
void DC2CALL Plesio_Side(gptr o)                                    /* 0x42C5A0 */
{
    static const u8 anim[2] = { 5, 6 };
    static const gptr eff[2] = { 0x655CD0, 0x656634 };
    attack_side(o, anim, eff, 0x60);
}

void DC2CALL Plesio_ThinkRise(gptr o)                               /* 0x42C680 */
{
    Plesio_Swim(o);
}

/* sub-state 5: the lunge from below (it circles faster meanwhile) */
void DC2CALL Plesio_Rise(gptr o)                                    /* 0x42C690 */
{
    gptr b = PS_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 1, 0xD, 0, 0x14);
        Effect_Spawn(o, 0x655018, 0x28, 0xFF, 0x100, 0);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 4, 1);
        break;
    case 1:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        else if (U16(o + 0x92) == 0x64) { U16(o + 0x1F0) = 0x3C; Bits_Set(b, 4, 0); }
        break;
    }
}

/* ---- the attacks landing ---- */

void DC2CALL Plesio_BiteHit(gptr o)                                 /* 0x42C750 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x658B8C);
    if (iabs(a) < 0x400) Player_Hurt(o, 0xAA, 2, 0x28);     /* face to face */
    else Player_Hurt(o, 0x96, 1, 0);
}

void DC2CALL Plesio_LungeHit(gptr o)                                /* 0x42C7C0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 a = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x658B8C);
    if (iabs(a) < 0x400) Player_Hurt(o, 0x96, 1, 0);
    else Player_Hurt(o, 0x82, 1, 0);
}

void DC2CALL Plesio_RiseHit(gptr o)                                 /* 0x42C830 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x658B78);
    Player_Hurt(o, 0x64, 1, 0);
}

/* ---- hit (state 2) and die (state 3) ---- */

/* sub-state 0: the damage taken; back to what it did, a flinch, or dead */
void DC2CALL Plesio_Damage(gptr o)                                  /* 0x42C890 */
{
    s32 hp = S16(o + 0x108);
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;
    u32 prev = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
    U32(o + 0x54) = prev;
    u32 dmg = U16(o + 0x176);
    U32(o + 0x1CC) += dmg;
    if ((s32)U32(o + 0x1CC) > S16(o + 0x10A) / 5) { U32(o + 0x1CC) = 0; U32(o + 0x54) = 0x102; }
    if (U8(o + 0x17D) == 0x12) U32(o + 0x54) = prev;
    u16 w = U16(o + 0x172);
    if (w == 3) U32(o + 0x54) = 0x202;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Enemy_BiteOff(o);
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(PS_BITS(o), 4, 0);
        S16(o + 0x1EA) >>= 1;
        if (U8(o + 0x184) & 8) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 0x14 : 0x1E;
        if (S16(o + 0x108) < 0) {
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
            Score_Kill(o);
            Flag_Set(4, U8(o + 0x11C), 1);
        } else Score_Hit(o, (u32)(hp - S16(o + 0x108)) / 5);
    } else {
        U16(o + 0x108) += (u16)((s32)dmg / -5);
        U8(o + 0x17B) = w == 4 ? 0xA : 0x14;
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(PS_MAIN + U8(o + 0x54) * 4), o);
}

static void flinch(gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 9, 0, 0, 2);
        Effect_Attach(o, 0x658260);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Plesio_Swim(o);
        break;
    }
}

/* sub-states 1..3: flinch, still swimming */
void DC2CALL Plesio_Flinch(gptr o)      { flinch(o); }          /* 0x42CAA0 */
void DC2CALL Plesio_Flinch2(gptr o)     { flinch(o); }          /* 0x42CB00 */
void DC2CALL Plesio_Flinch3(gptr o)     { flinch(o); }          /* 0x42CB60 */

/* die: the death animation, then fades; sinks all the while */
void DC2CALL Plesio_Dead(gptr o)                                    /* 0x42CBC0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x20;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0xA, 0, 0x40, 2);
        Effect_Attach(o, 0x658674);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        break;
    case 2:
        Flag_Set(7, 0x15, 1);
        Obj_DeathFade(o);
        break;
    }
    U16(o + 0x42) -= 30;
    if (S16(o + 0x42) < -20000) U16(o + 0x42) = (u16)-20000;
    if (Bits_Check(PS_BITS(o), 3)) {
        U16(o + 0x44) -= 0x50;
        if (S16(o + 0x44) < 0) U16(o + 0x44) = 0;
    }
}
