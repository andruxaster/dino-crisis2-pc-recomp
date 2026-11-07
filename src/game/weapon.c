/* Weapons: set-up of the main weapon (table 0x737FA0 by G+0x10B4 low byte)
 * and of the sub-weapon (0x737FF0 by the high byte). A weapon picks the
 * player's animation table (+0x88); a sub-weapon puts its model(s) into the
 * object-effect slots G+0x93C (0x70 bytes: +0x40 offset, +0x48 rotation,
 * +0x4E 3 = held, +0x4F state, +0x54 part it follows, +0x68 model). */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"

#define PX() U32(0x87FD60)

/* light slot i of G+0x948 (0x34 bytes) reset; all lights 0x748160 cleared */
gptr DC2CALL Light_Reset(u32 i)                                    /* 0x41C970 */
{
    gptr e = U32(G() + 0x948) + (u8)i * 0x34;
    memset(MEM(e), 0, 0x34);
    memset(MEM(0x748160), 0, 0x900);
    U8(e) = 1; U8(e + 1) = 5;
    return e;
}

void DC2CALL Weapon_SetNone(gptr o) { U32(o + 0x88) = 0x665D00; }           /* 0x4B2C20 */

void DC2CALL Weapon_SetGun(gptr o)                                 /* 0x4AC980 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
}

void DC2CALL Weapon_SetLight(gptr o)                               /* 0x4AE2F0 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
    Light_Reset(0);
}

void DC2CALL Weapon_SetTwin(gptr o)                                /* 0x4AEF10 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
    U32(PX() + 0x58) = 0; U16(PX() + 0x5C) = 0;
    U32(PX() + 0x60) = 0; U16(PX() + 0x64) = 0;
}

void DC2CALL Weapon_SetHeavy(gptr o)                               /* 0x4B1C90 */
{
    U32(o + 0x88) = 0x665D7C;
    U8(PX() + 0x2F) = 1;
}

/* object-effect slot e as a model carried by part `part` of o */
static void carried(gptr e, gptr o, gptr model, u32 part, u8 state)
{
    memset(MEM(e), 0, 0x70);
    U32(e + 0x50) = 3;
    U32(e + 0x68) = model;
    U32(e + 0x54) = Obj_Part(U32(o + 0x64), part);
    U8(e + 0x4E) = 3;
    U8(e + 0x4F) = state;
}

static void blade(gptr e)
{
    U16(e + 0x40) = 0xFF56; U16(e + 0x42) = 0; U16(e + 0x44) = 0x64;
    U16(e + 0x48) = 0xFC7C; U16(e + 0x4A) = 0xFFEC; U16(e + 0x4C) = 0xA;
}

/* two knives (one in each hand model) */
void DC2CALL Sub_SetKnives(gptr o)                                 /* 0x4B2C30 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C988, 0xC, 0); blade(e);
    e += 0x70;
    carried(e, o, 0x67C878, 0xC, 0); blade(e);
}

void DC2CALL Sub_SetKnife(gptr o)                                  /* 0x4B2FC0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67CA00, 0xC, 1);
    U16(e + 0x40) = 0xFF88; U16(e + 0x42) = 0; U16(e + 0x44) = 0x28;
    U16(e + 0x48) = 0xFC7C; U16(e + 0x4A) = 0xFFEC; U16(e + 0x4C) = 0xA;
}

void DC2CALL Sub_SetGrenade(gptr o)                                /* 0x4B33C0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C508, 0xC, 0);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
}

void DC2CALL Sub_SetMine(gptr o)                                   /* 0x4B3A10 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C50C, 0xC, 0);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF92; U16(e + 0x44) = 0xFFCE; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
    U8(PX() + 0x78) = 0;
}

void DC2CALL Sub_SetKnives2(gptr o)                                /* 0x4B3D00 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C988, 0xC, 0); blade(e); U16(e + 0x5E) = 1;
    e += 0x70;
    carried(e, o, 0x67C878, 0xC, 0); blade(e); U16(e + 0x5E) = 1;
}
