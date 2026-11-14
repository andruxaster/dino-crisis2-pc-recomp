/* The compsognathus (object type handler 0x445690, type 13, EA0.DAT). The
 * PC game keeps only a small part of it: it turns to its target o+0xD8,
 * clings to the player's height, loses health when hit and fades out when
 * dead.
 * States (o+0x54): 0 init, 1 normal (0x7213E0 then 0x7213E4 by o+0x55: the
 * two tables overlap, so move k runs entries k and k+1), 2 hit (0x7213E8),
 * 3 die (0x7213FC), 4 (0x4A56D0). An attack landing goes through 0x721410
 * by o+0x55; its entries 2 and 3 are the two screen flashes below. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define CO_MAIN  0x7213CCu      /* state handlers */
#define CO_MOVE  0x7213E0u      /* state 1, by o+0x55 */
#define CO_MOVE2 0x7213E4u
#define CO_HIT   0x7213E8u      /* state 2 */
#define CO_DIE   0x7213FCu      /* state 3 */
#define CO_GRAB  0x721410u      /* an attack landed, by o+0x55 */
#define PRIMS    0x87DC98u      /* the next free primitive */
#define OTS      0x87F94Cu      /* the ordering tables */

void DC2CALL Compso_Handler(gptr o)                                 /* 0x445690 */
{
    Obj_SavePos(o);
    Orig_Call1(U32(CO_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    Enemy_AttackHit(o);
    Compso_Grab(o);
}

/* state 0: model 0x640000, animations 0x640868, health 1000 */
void DC2CALL Compso_Init(gptr o)                                    /* 0x445720 */
{
    U16(o + 0x108) = 0x3E8;
    U16(o + 0x10A) = 0x3E8;
    U8(o + 0x54)++;
    U32(o + 0x50) = 0xD;
    U32(o + 0x12C) = 0x7213A0;
    U32(o + 0x128) = 0x7213B0;
    U32(o + 0x134) = 0x7213C0;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x640868;
    Anim_Start(o, 0, 0, 1, 5);
    Obj_Set118(o, 0, 0x1FA0, 0x1FA0);
    DrawSync(o);
    U32(o + 0x54) = 1;
}

void DC2CALL Compso_Normal(gptr o)                                  /* 0x4457F0 */
{
    Orig_Call1(U32(CO_MOVE + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(CO_MOVE2 + U8(o + 0x55) * 4), o);
}

void DC2CALL Compso_Hit(gptr o)                                     /* 0x445820 */
{
    Orig_Call1(U32(CO_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Compso_Die(gptr o)                                     /* 0x445840 */
{
    Orig_Call1(U32(CO_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Compso_Grab(gptr o)                                    /* 0x445860 */
{
    if (S8(o + 0x119) >= 0) Orig_Call1(U32(CO_GRAB + U8(o + 0x55) * 4), o);
}

void DC2CALL Compso_Turn(gptr o)                                    /* 0x445880 */
{
    Obj_TurnToPoint(o, 0x19, o + 0xD8);
}

/* starts its animation, then plays it speeding up, at the player's height */
void DC2CALL Compso_Cling(gptr o)                                   /* 0x4458A0 */
{
    gptr pl = U32(G() + 0x940);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 1, 0xA);
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, 0x8C);
        U16(o + 0x42) = U16(pl + 0x42);
        break;
    }
}

void DC2CALL Compso_Damage(gptr o)                                  /* 0x445910 */
{
    Compso_Hurt(o);
}

/* the hit's damage (o+0x176) taken; it lets go of the player and stops */
void DC2CALL Compso_Hurt(gptr o)                                    /* 0x445920 */
{
    gptr pl = U32(G() + 0x940);
    U32(o + 0x50) &= ~0x30u;
    U32(o + 0x184) &= ~1u;
    U16(pl + 0x1E8) = 0;
    U16(o + 0x108) -= U16(o + 0x176);
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
}

void DC2CALL Compso_Fade(gptr o)                                    /* 0x445980 */
{
    Obj_DeathFade(o);
    U32(o + 0x50) |= 0x30;
}

/* the whole screen tinted: f+0 the colour, f+3 the blend mode */
void DC2CALL Flash_Full(gptr f)                                     /* 0x4459A0 */
{
    gptr p = U32(PRIMS);
    u32 c = (U32(f) & 0xFFFFFF) | 0x8E000000;
    U32(p + 0xC) = 0; U32(p + 8) = 0;
    U8(p + 0x12) = 0; U16(p + 0x10) = 0;
    U16(p + 0x18) = 0x140; U16(p + 0x14) = 0x140;
    U16(p + 0x1A) = 0xF0; U16(p + 0x16) = 0xF0;
    U8(p + 0x13) = (u8)(U8(f + 3) << 5);
    U32(p + 4) = c;
    gptr ot = U32(OTS) + 0x78;
    U32(p) = U32(ot) | 0x2000000; U32(ot) = p & 0xFFFFFF;
    U32(PRIMS) += 0x1C;
}

/* the screen in four quarters, each drawn from (k, k) or the far side of
   its own quarter (f+5 the signed k), so the picture moves out or in */
void DC2CALL Flash_Quads(gptr f)                                    /* 0x445A60 */
{
    s32 k = S8(f + 5);
    u32 c = (U32(f) & 0xFFFFFF) | 0x8E000000;
    u8 mode = (u8)(U8(f + 3) << 5);
    static const u32 X[4] = {0, 0x43200000, 0, 0x43200000};   /* 0, 160.0f */
    static const u32 Y[4] = {0, 0, 0x42F00000, 0x42F00000};   /* 0, 120.0f */
    for (int i = 0; i < 4; i++) {
        gptr p = U32(PRIMS);
        U32(p + 8) = X[i]; U16(p + 0x14) = 0xA0; U32(p + 0xC) = Y[i]; U16(p + 0x16) = 0x78;
        U16(p + 0x10) = (u16)(i & 1 ? 0xA0 : k);
        U16(p + 0x18) = (u16)(0xA0 - k);
        U8(p + 0x12) = (u8)(i & 2 ? 0x78 : k);
        U16(p + 0x1A) = (u16)(0x78 - k);
        U8(p + 0x13) = mode;
        U32(p + 4) = c;
        gptr ot = U32(OTS) + 0x78;
        U32(p) = U32(ot) | 0x2000000; U32(ot) = p & 0xFFFFFF;
        U32(PRIMS) += 0x1C;
    }
}
