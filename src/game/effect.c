/* Object effects (G+0x93C, 0x70 each; handler by +0x4E through *0x87F320,
 * step +0x46 through each handler's own table). +0x47 a speed or shade,
 * +0x4F a kind, +0x68 data (a model block: +0 polygons of 0x28, +4 of
 * 0x34, +8/+0xA their counts). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

/* a step that does nothing */
void DC2CALL Nothing(gptr e)                                        /* 0x402520 */
{
    (void)e;
}

/* turning: step 0 turns it on y */
void DC2CALL ObjEff_Turning(gptr e)                                 /* 0x474BB0 */
{
    Orig_Call1(U32(0x73776C + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_Turn(gptr e)                                    /* 0x474BD0 */
{
    U16(e + 0x4A) += 0x30;
}

/* the colour of all polygons of the effect's model */
void DC2CALL ObjEff_Shade(gptr e, u32 v)                            /* 0x474CC0 */
{
    gptr d = U32(e + 0x68);
    u8 b = (u8)v;
    for (u32 i = 0; i < U16(d + 8); i++) {
        gptr p = U32(d) + i * 0x28;
        static const u8 o3[] = {0x26, 0x25, 0x24, 0x22, 0x21, 0x20, 0x1E, 0x1D, 0x1C};
        for (int k = 0; k < 9; k++) U8(p + o3[k]) = b;
    }
    for (u32 i = 0; i < U16(d + 0xA); i++) {
        gptr p = U32(d + 4) + i * 0x34;
        static const u8 o4[] = {0x32, 0x31, 0x30, 0x2E, 0x2D, 0x2C, 0x2A, 0x29, 0x28, 0x26, 0x25, 0x24};
        for (int k = 0; k < 12; k++) U8(p + o4[k]) = b;
    }
}

/* blinking: the polygons' texture pages moved by +0x47, then dark / lit by +0x4F */
void DC2CALL ObjEff_Blink(gptr e)                                   /* 0x474BE0 */
{
    Orig_Call1(U32(0x737778 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_BlinkMove(gptr e)                               /* 0x474C00 */
{
    gptr d = U32(e + 0x68);
    for (u32 i = 0; i < U16(d + 8); i++) U16(U32(d) + 0x1A + i * 0x28) += (u16)(s16)S8(e + 0x47);
    for (u32 i = 0; i < U16(d + 0xA); i++) U16(U32(d + 4) + 0x22 + i * 0x34) += (u16)(s16)S8(e + 0x47);
    if (!U8(e + 0x4F)) U8(e + 0x4F) = 8;
    U8(e + 0x4F)++;
    ObjEff_Shade(e, 0);
    U8(e + 0x46) = 1;
}

void DC2CALL ObjEff_BlinkDark(gptr e)                               /* 0x474C80 */
{
    ObjEff_Shade(e, 0);
    U8(e + 0x46) = 2;
}

void DC2CALL ObjEff_BlinkLit(gptr e)                                /* 0x474CA0 */
{
    ObjEff_Shade(e, (u8)(U8(e + 0x4F) << 1));
    U8(e + 0x46) = 1;
}

/* thrown out along +0x4C, falling until it is below the player
   (per kind +0x4F, 5 bytes at 0x737790: divisor, spin, -, gravity, model) */
void DC2CALL ObjEff_Thrown(gptr e)                                  /* 0x474E50 */
{
    Orig_Call1(U32(0x7377B8 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_ThrownInit(gptr e)                              /* 0x474D50 */
{
    gptr t = 0x737790 + S8(e + 0x4F) * 5;
    U32(e + 0x68) += U32(0x737784 + U8(t + 4) * 4);
    U16(e + 0x60) = (u16)(rsin(S16(e + 0x4C)) / (s32)U8(t));
    U16(e + 0x62) = 0xFF38;
    U16(e + 0x64) = (u16)(rcos(S16(e + 0x4C)) / (s32)U8(t));
    U8(e + 0x46)++;
}

void DC2CALL ObjEff_ThrownMove(gptr e)                              /* 0x474DC0 */
{
    gptr t = 0x737790 + S8(e + 0x4F) * 5, pl = U32(G() + 0x940);
    U16(e + 0x48) += (u16)(U8(t + 1) << 3);
    U16(e + 0x40) += (u16)(rsin(S16(e + 0x4C)) / (s32)U8(t));
    U16(e + 0x44) += (u16)(rcos(S16(e + 0x4C)) / (s32)U8(t));
    u16 vy = U16(e + 0x62);
    U16(e + 0x42) += vy;
    U16(e + 0x62) = (u16)(vy + U8(t + 3));
    if (S16(e + 0x42) > S16(pl + 0x42)) U8(e + 0x46)++;
}

/* debris: spins (kind +0x4F, 5 bytes at 0x7377C4: divisor, spin x, spin y,
   speed, model), rises then falls until below the player, then goes */
void DC2CALL ObjEff_Debris(gptr e)                                  /* 0x474E70 */
{
    Orig_Call1(U32(0x7377EC + S8(e + 0x46) * 4), e);
    if (U32(e + 0x50) & 0x40000000) Eff_Free(e);
}

void DC2CALL ObjEff_DebrisInit(gptr e)                              /* 0x474EA0 */
{
    static const u32 model[3] = {0, 0x368, 0x5E0};
    gptr t = 0x7377C4 + S8(e + 0x4F) * 5;
    U16(e + 0x48) = 0; U16(e + 0x4A) = 0;
    U32(e + 0x68) += model[U8(t + 4)];
    U8(e + 0x47) = U8(t + 3);
    U8(e + 0x46) = 1;
    ObjEff_DebrisMove(e);
}

void DC2CALL ObjEff_DebrisMove(gptr e)                              /* 0x474F10 */
{
    gptr t = 0x7377C4 + S8(e + 0x4F) * 5, pl = U32(G() + 0x940);
    s8 v = S8(e + 0x47);
    U16(e + 0x48) += U8(t + 1);
    U16(e + 0x4A) += U8(t + 2);
    if (v < 0x78) U8(e + 0x47) = (u8)(v + 1);
    U16(e + 0x42) += (u16)((S8(e + 0x47) - 0xC) * 10);
    U16(e + 0x40) += (u16)(rsin(S16(e + 0x4C)) / (s32)U8(t));
    U16(e + 0x44) += (u16)(rcos(S16(e + 0x4C)) / (s32)U8(t));
    if (S16(e + 0x42) > S16(pl + 0x42)) U32(e + 0x50) |= 0x40000000;
}

/* in the player's hand (part 7): Dylan (G+0x108F 0) with flag 9/0x2E
   holds it there, else it is hidden */
void DC2CALL ObjEff_InHand(gptr e)                                  /* 0x474FB0 */
{
    Orig_Call1(U32(0x7377F4 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_InHandInit(gptr e)                              /* 0x474FD0 */
{
    gptr pl = U32(G() + 0x940);
    U8(e + 0x46)++;
    if (U8(G() + 0x108F) || !Flag_Check(9, 0x2E)) {
        U8(e + 0x46)++;
        U32(e + 0x50) &= ~2u;
        return;
    }
    gptr p = Obj_Part(U32(pl + 0x64), 7);
    U32(p + 0x50) |= 0x10000000;
    U16(e + 0x40) = U16(p + 0x64); U16(e + 0x42) = U16(p + 0x66); U16(e + 0x44) = U16(p + 0x68);
    U16(e + 0x48) = U16(p + 0x48); U16(e + 0x4A) = U16(p + 0x4A); U16(e + 0x4C) = U16(p + 0x4C);
    U32(e + 0x54) = U32(p + 0x58);
    U32(e + 0x50) = (U32(e + 0x50) & ~4u) | 2;
}

void DC2CALL ObjEff_InHandHold(gptr e)                              /* 0x475080 */
{
    gptr p = Obj_Part(U32(U32(G() + 0x940) + 0x64), 7);
    U32(p + 0x50) |= 0x10000000;
    U16(e + 0x48) = U16(p + 0x48); U16(e + 0x4A) = U16(p + 0x4A); U16(e + 0x4C) = U16(p + 0x4C);
    U32(e + 0x50) |= 2;
}
