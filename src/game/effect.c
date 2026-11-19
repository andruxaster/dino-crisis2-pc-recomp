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

/* ---- the object effects particular to some areas (tab 3, index 16, 17) ---- */

/* on a part (+0x54): into the part's frame, then on */
void DC2CALL ObjEff_Attach(gptr e)                                  /* 0x4750D0 */
{
    gptr p = U32(e + 0x54);
    if (p) {
        ApplyMatrixSV(p + 0x20, e + 0x40, e + 0x40);
        U16(e + 0x40) += U16(p + 0x40); U16(e + 0x42) += U16(p + 0x42); U16(e + 0x44) += U16(p + 0x44);
        U32(e + 0x54) = 0;
    }
    U8(e + 0x46)++;
}

/* the same, staying on this step without a part */
void DC2CALL ObjEff_Carry(gptr e)                                   /* 0x475510 */
{
    if (U32(e + 0x54)) ObjEff_Attach(e);
}

void DC2CALL ObjEff_Move(gptr e)                                    /* 0x475130 */
{
    Obj_MoveLocal(e, e + 0x60);
}

/* a light that follows it (0x737808) */
void DC2CALL ObjEff_AddLight(gptr e)                                /* 0x475150 */
{
    gptr l = Light_Alloc();
    if (l) {
        U8(l + 1) = 2; U32(l + 0xC) = 0x737808; U32(l + 0x14) = e;
        U16(l + 0x1E) = 0x39; U16(l + 0x1C) = 0x7EC1; U32(l + 0x18) |= 4;
    }
    U8(e + 0x46)++;
}

/* area 2: on a part, moved, lit, gone */
void DC2CALL ObjEff_Lit(gptr e)                                     /* 0x4751A0 */
{
    Orig_Call1(U32(0x7378A8 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_Gone(gptr e)                                    /* 0x4752F0 */
{
    Eff_Free(e);
}

/* area 4: flung from part 3 of the second object, 5 frames */
void DC2CALL ObjEff_Flung(gptr e)                                   /* 0x475300 */
{
    Orig_Call1(U32(0x7378BC + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_FlungInit(gptr e)                               /* 0x4751C0 */
{
    gptr o = U32(G() + 0x940) + 0x260;
    gptr f = Tmp_Push(0x28), v = f, m = f + 8;
    U32(e + 0x54) = 0; U16(e + 0x4C) = 0; U8(e + 0x46)++;
    U32(e + 0x50) = 3; U16(e + 0x48) = 0x400; U16(e + 0x4A) = 0x400;
    RotMatrixYXZ(e + 0x48, e + 0x20);
    RotMatrix(o + 0x48, m);
    U16(v) = 0xFFDE; U16(v + 2) = 0xFFE0; U16(v + 4) = 0xFE89;
    ApplyMatrixSV(m, v, v);
    gptr p = Obj_Part(U32(o + 0x64), 3);
    U16(e + 0x40) = (u16)(U16(p + 0x40) + U16(v));
    U16(e + 0x42) = (u16)(U16(p + 0x42) + U16(v + 2));
    U16(e + 0x44) = (u16)(U16(p + 0x44) + U16(v + 4));
    U16(e + 0x62) = 0x1F4; U16(e + 0x5E) = 5;
    Tmp_Pop(0x28);
}

void DC2CALL ObjEff_FlungMove(gptr e)                               /* 0x475290 */
{
    gptr v = Tmp_Push(8);
    ApplyMatrixSV(e + 0x20, e + 0x60, v);
    U16(e + 0x40) += U16(v); U16(e + 0x42) += U16(v + 2); U16(e + 0x44) += U16(v + 4);
    Tmp_Pop(8);
    if (U16(e + 0x5E)) U16(e + 0x5E)--;
    else U8(e + 0x46)++;
}

/* area 5: its model's shade from the kind (+0x4F), or dark */
void DC2CALL ObjEff_Shaded(gptr e)                                  /* 0x475320 */
{
    Orig_Call1(U32(0x7378C8 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_ShadeSkip(gptr e)                               /* 0x475340 */
{
    U8(e + 0x46) = 2;
}

void DC2CALL ObjEff_ShadeOff(gptr e)                                /* 0x475350 */
{
    ObjEff_Shade(e, 0);
    U8(e + 0x46) = 2;
}

void DC2CALL ObjEff_ShadeKind(gptr e)                               /* 0x475370 */
{
    ObjEff_Shade(e, U8(e + 0x4F));
}

/* area 7: hurled up and aside (kind 0 back, 1 forward, else straight),
   tumbling, falling until 5000 down */
void DC2CALL ObjEff_Hurl(gptr e)                                    /* 0x4754D0 */
{
    Orig_Call1(U32(0x7378D4 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_HurlInit(gptr e)                                /* 0x475390 */
{
    U16(e + 0x62) = 0;
    U32(e + 0x50) = (U32(e + 0x50) & ~4u) | 2;
    U16(e + 0x64) = (u16)((Rand8() % 6 + 0x14) * 5);
    U16(e + 0x5E) = (u16)(Rand8() % 5 + 3);
    u8 k = U8(e + 0x4F);
    if (k == 0) U16(e + 0x60) = (u16)((-2 - Rand8() % 5) * 5);
    else if (k == 1) U16(e + 0x60) = (u16)((Rand8() % 5 + 2) * 5);
    else U16(e + 0x60) = 0;
    U8(e + 0x46)++;
}

void DC2CALL ObjEff_HurlMove(gptr e)                                /* 0x475450 */
{
    U16(e + 0x48) = (u16)(((Rand8() & 3) << 4) + U16(e + 0x48)) & 0xFFF;
    u16 a = (u16)(((Rand8() & 3) << 4) + U16(e + 0x4A)) & 0xFFF;
    U16(e + 0x40) += U16(e + 0x60);
    U16(e + 0x44) += U16(e + 0x64);
    U16(e + 0x4A) = a;
    U16(e + 0x62) += U16(e + 0x5E);
    U16(e + 0x42) += U16(e + 0x62);
    if (S16(e + 0x42) >= -5000) U8(e + 0x46)++;
}

/* areas 3, 5, 9: on a part, moved, then a burst effect there */
static void burst_end(gptr e, gptr data)
{
    Effect_Spawn(e + 0x40, data, 0, 0, 0x20, 0x387EC0);
    Eff_Free(e);
}

void DC2CALL ObjEff_Burst1(gptr e)                                  /* 0x4754F0 */
{
    Orig_Call1(U32(0x7378E0 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_Burst1End(gptr e) { burst_end(e, 0x646A80); }  /* 0x475560 */

void DC2CALL ObjEff_Burst2(gptr e)                                  /* 0x475590 */
{
    Orig_Call1(U32(0x7378F0 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_Burst2End(gptr e) { burst_end(e, 0x65F2E0); }  /* 0x4755B0 */

void DC2CALL ObjEff_Burst3(gptr e)                                  /* 0x4755E0 */
{
    Orig_Call1(U32(0x737900 + S8(e + 0x46) * 4), e);
}

void DC2CALL ObjEff_Burst3End(gptr e) { burst_end(e, 0x64CE28); }  /* 0x475600 */
