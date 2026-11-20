/* Area 9's own objects (the extra game: rooms ST900..ST906).
 *  type 17 (0x47FFA0, ST905): watches the other objects: one whose +0x1C0
 *    was set (a hit it has to answer) goes into state 2 with the next
 *    reaction (+0x1C2 by 4), and flag 7:0x1C (object 1) or 0x1D is set.
 *  type 19 (0x4454E0, ST903): a thing that blows up once its health
 *    (+0x10C) is gone: a burst (effect 0x667124) above it, a sound, the
 *    object +0x1C0 names put into state 2, a light; then it is put away
 *    (flag 4:+0x11C set). States (0x721390): 0 set up, 1 waiting,
 *    2 blowing up, 3 gone. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

void DC2CALL Ao17_Handler(gptr o)                                     /* 0x47FFA0 */
{
    gptr p = U32(G() + 0x940);
    U32(o + 0x50) = 1;
    for (u32 i = 1; i < U32(0x87F940); i++) {
        u8 f = U8(p + 0x260 + 0x50);
        p += 0x260;
        if (!(f & 1) || !U16(p + 0x1C0)) continue;
        U16(p + 0x1C2) += 4;
        U16(p + 0x1C0) = 0;
        U32(p + 0x54) = (u32)U16(p + 0x1C2) << 8 | 2;
        Flag_Set(7, p == U32(0x87E760) + 0x260 ? 0x1C : 0x1D, 1);
    }
}

void DC2CALL Ao19_Handler(gptr o)                                     /* 0x4454E0 */
{
    Orig_Call1(U32(0x721390 + U8(o + 0x54) * 4), o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_UpdateMatrix(o);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Ao19_Init(gptr o)                                        /* 0x445520 */
{
    U8(o + 0x54)++;
    U32(o + 0x50) = 0x1040B;
    Model_Setup(o, U32(o + 0x60));
    U32(o + 0x12C) = 0x721348; U32(o + 0x134) = 0x721348;
    U32(o + 0x74) = 0x800; U32(o + 0x70) = 0x800; U32(o + 0x6C) = 0x800;
    if (U16(o + 0x10C) != 10) return;
    gptr p = U32(o + 0x64);
    for (u32 i = 0; i < U32(o + 0x68); i++, p = U32(p + 0x54)) {   /* (a lighter colour) */
        U8(p + 0x8C) = 0x40; U8(p + 0x8D) = 0x40; U8(p + 0x8E) = 0x80;
    }
}

void DC2CALL Ao19_Gone(gptr o)                                        /* 0x445660 */
{
    U32(o + 0x50) |= 0x40000000;
    Flag_Set(4, U8(o + 0x11C), 1);
}

void DC2CALL Ao19_Blow(gptr o)                                        /* 0x4455A0 */
{
    if (U16(o + 0x10C)) return;
    gptr pos = Tmp_Push(8);
    U32(pos) = U32(o + 0x40); U32(pos + 4) = U32(o + 0x44);
    s32 s = 0x1000 / (s32)U32(o + 0x70);
    U16(pos + 2) += (u16)(-0x5DC / s);                  /* 1500 up, at the object's scale */
    Effect_Spawn(pos, 0x667124, 0, 0, 0x20, 0);
    Tmp_Pop(8);
    Se_Play(o, 0x6C);
    U8(U32(G() + 0x940) + S16(o + 0x1C0) * 0x260 + 0x54) = 2;
    gptr l = Light_Alloc();
    if (!l) return;
    U8(l + 1) = 6; U8(l + 2) = 1;
    U32(l + 0xC) = 0x5E0048;
    U16(l + 0x10) = U16(o + 0x1C0); U16(l + 0x12) = 1;
    Ao19_Gone(o);
}
