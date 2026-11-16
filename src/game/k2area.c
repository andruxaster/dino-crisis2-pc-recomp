/* Effect slot kinds of the area tables (*0x87F948, G+0x944 slots of 0xAC, run by
 * Prop_RunAll): the ones particular to some areas. Each kind's handler runs
 * its motion (0x7215E4 by +0x7B) and its step (+0x54).
 *
 * Kind 7 (0x449930), steps 0x721830: a shot at the player (or at G+0xB50):
 * on a wall or off its floor it bursts (0x721740); a sphere of 0xFA about it
 * that meets the player's (0x17C at 0x348 up) knocks the player down (state
 * 0x102) and the hit direction, unless the player is already hit (+0x174).
 * Kind 8 (0x449BB0), steps 0x721930: the same shot from the player's side,
 * against the enemies (Weapon_Hit, record 0xD; in the duel the other player),
 * its burst 0x721840.
 * Kind 12 (0x449DB0), steps 0x72193C: a glow growing by +0x98 every +0x9C
 * frames up to +0x9A, then for +0x9E frames, then dimming (K2Glow_Dim); gone
 * with flag 2:0x18 when +0x7A is 0. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define K2_MOTION 0x7215E4u     /* motion by +0x7B */
#define ESHOT_STEP 0x721830u
#define PSHOT_STEP 0x721930u
#define FLARE_STEP 0x72193Cu

/* moves by +0x7C.. (the last place to +0x74), its level from the height */
static void shot_move(gptr k)
{
    u32 last = U32(k + 0x40);
    U16(k + 0x40) += U16(k + 0x7C);
    U16(k + 0x42) += U16(k + 0x7E);
    U32(k + 0x74) = last;
    u16 z = U16(k + 0x44);
    U16(k + 0x78) = z;
    U16(k + 0x44) = (u16)(z + U16(k + 0x80));
    U16(k + 0x5A) = (u16)(S16(k + 0x42) / -1000);
    if (K2_AnimStep(k) && !(U8(k + 0x84) & 8)) U8(k + 0x54)++;
}

/* against a wall, or off the floor it was over */
static int shot_blocked(gptr k)
{
    return Eff_WallHit(k) || !(u16)Floor_Cross(k + 0x40, k + 0x74, k + 0x40, 0x10);
}

static void shot_burst(gptr k, gptr data)
{
    Effect_Spawn(k + 0x40, data, 0, 0, 0x20, 0);
    U8(k + 0x54)++;
}

void DC2CALL K2_EShot(gptr k)                                       /* 0x449930 */
{
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(ESHOT_STEP + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (!--U8(k + 0x46)) K2_Detach(k);
}

void DC2CALL K2_PShot(gptr k)                                       /* 0x449BB0 */
{
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(PSHOT_STEP + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (!--U8(k + 0x46)) K2_Detach(k);
}

void DC2CALL K2Shot_Init(gptr k)                                    /* 0x449BF0 */
{
    gptr r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    U8(k + 0x54)++;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    K2_SetAnim(k, 0x6822B0);
    U16(k + 0x6A) = 0x2B;
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= 2;
    U16(k + 0x68) = 0x7DC7;
    U8(k + 0x85) = 0x10;
    gptr p = U32(k + 0x60), e = U32(G() + 0x93C);
    if (p < e || p > e + U32(0x87DE04) * 0x70) U32(k + 0xA4) = K2_OwnerObj(k);   /* not from an effect */
}

/* the target t hit: knocked, turned toward the shot */
static void eshot_hit(gptr k, gptr t)
{
    u16 a = (u16)Angle_XZ(k + 0x40, t + 0x40);
    gptr f = Tmp_Push(0x188);                       /* a stand-in for the one that shot */
    U8(f + 0x184) = 0; U16(f + 0x4A) = a;
    Enemy_HitDir(f);
    Tmp_Pop(0x188);
    U16(t + 0x174) = 1; U16(t + 0x176) = 0xA0; U16(t + 0x170) = 0xFFFF;
    shot_burst(k, 0x721740);
}

/* the sphere of an object (0x17C at 0x348 up) against the shot's (0xFA) */
static int eshot_meets(gptr s, gptr t)
{
    gptr b = Tmp_Push(8);
    U16(b) = U16(t + 0x40); U16(b + 2) = (u16)(U16(t + 0x42) - 0x348); U16(b + 4) = U16(t + 0x44); U16(b + 6) = 0x17C;
    u32 r = Sphere_Hit(s, b, 0x348, 0x190);
    Tmp_Pop(8);
    return r != 0;
}

void DC2CALL K2EShot_Move(gptr k)                                   /* 0x449970 */
{
    shot_move(k);
    if (!U8(k + 0x7A)) return;
    if (shot_blocked(k)) { shot_burst(k, 0x721740); return; }
    gptr s = Tmp_Push(8);
    U16(s) = U16(k + 0x40); U16(s + 2) = U16(k + 0x42); U16(s + 4) = U16(k + 0x44); U16(s + 6) = 0xFA;
    gptr pl = U32(G() + 0x940), t = 0;
    if (!(U8(pl + 0x174) & 1) && eshot_meets(s, pl)) {
        Flash_Start(pl, 7);
        U32(pl + 0x54) = 0x102;
        t = pl;
    } else {
        gptr b = U32(G() + 0xB50);
        if (b && !(U8(b + 0x174) & 1) && eshot_meets(s, b)) { U32(b + 0x54) = 2; t = b; }
    }
    Tmp_Pop(8);
    if (t) eshot_hit(k, t);
}

void DC2CALL K2PShot_Move(gptr k)                                   /* 0x449CA0 */
{
    shot_move(k);
    if (!U8(k + 0x7A)) return;
    if (!shot_blocked(k)) {
        u32 hit;
        if (U8(G() + 0x927) == 2) hit = Kof_HitFoe(k + 0x20, k + 0x40, 0xD, U8(U32(k + 0xA4) + 0xFF) == 1 ? 2 : 1);
        else hit = Weapon_Hit(k + 0x20, k + 0x40, 0xD);
        if (!hit) return;
    }
    shot_burst(k, 0x721840);
}

void DC2CALL K2_Flare(gptr k)                                       /* 0x449DB0 */
{
    Orig_Call1(U32(FLARE_STEP + U8(k + 0x54) * 4), k);
    K2_UpdateHidden(k);
    if (!U8(k + 0x7A) && Flag_Check(2, 0x18)) K2_Unlink(k);
}

void DC2CALL K2Flare_Init(gptr k)                                   /* 0x449DF0 */
{
    U8(k + 0x54)++;
    if (U8(G() + 0x927)) { U16(k + 0x6A) = 0x2B; K2_Unlink(k); }
    else U16(k + 0x6A) = 0xB;
    U16(k + 0x68) = 0x7DC0;
    gptr r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    U8(k + 0x67) |= 2;
    U32(k + 0x50) |= 2;
    U16(k + 0x48) = 0x400;
    u8 kd = U8(k + 0x7A);
    if (kd <= 1) { U16(k + 0x6C) = 0; U16(k + 0x6E) = 0; }
    else if (kd == 2) { U16(k + 0x6E) = U16(k + 0x9A); U16(k + 0x6C) = U16(k + 0x9A); }   /* (2: grown at once) */
    U8(k + 0x84) |= 1;
    K2_SetAnim(k, 0x681E28);
    U16(k + 0xA2) = 0;
    if (U8(G() + 0x927)) {
        if (!U16(k + 0x9E)) U16(k + 0x9E) = 3;
        U16(k + 0x9E) = (u16)(U16(k + 0x9E) * 0x1E);
    } else U16(k + 0x9E) = 0;
}

void DC2CALL K2Flare_Grow(gptr k)                                   /* 0x449F00 */
{
    if ((s32)U16(k + 0x6E) < (s32)S16(k + 0x9A) && U8(k + 0x7A) != 2) {
        if ((s16)++U16(k + 0xA2) > S16(k + 0x9C)) {
            U16(k + 0xA2) = 0;
            U16(k + 0x6E) += U16(k + 0x98);
            U16(k + 0x6C) += U16(k + 0x98);
        }
        return;
    }
    U16(k + 0xA2) = 0;
    if (U8(G() + 0x927)) U8(k + 0x54)++;
}

void DC2CALL K2Flare_Wait(gptr k)                                   /* 0x449F70 */
{
    if ((s16)++U16(k + 0xA2) > S16(k + 0x9E)) U8(k + 0x54)++;
}

/* ---- kind 20 of area 1 (0x44B2C0), steps 0x7219E0: a thing swinging on its
   rope (+0x98 the point it turns toward by +0xA0 at most, its turn +0xA6),
   moving +0x9E ahead; its sway (+0x4C) grows by +0xA8 for +0xA2 frames, then
   dies down, turning the other way (+0xA4) as it ends; gone with +0x98 0 ---- */

void DC2CALL K2_Swing(gptr k)                                       /* 0x44B2C0 */
{
    Orig_Call1(U32(0x7219E0 + U8(k + 0x54) * 4), k);
    if (U32(k + 0x50) & 0x40000000) K2_Unlink(k);
}

void DC2CALL K2Swing_Init(gptr k)                                   /* 0x44B2F0 */
{
    K2_SetAnim(k, 0x6822A8);
    U16(k + 0xA4) = 0;
    U32(k + 0x50) |= 2;
    U8(k + 0x54) = 1;
}

void DC2CALL K2Swing_Move(gptr k)                                   /* 0x44B320 */
{
    U16(k + 0xA6) += (u16)Angle_TurnMax(k + 0x40, k + 0x98, U16(k + 0xA6), U16(k + 0xA0));
    gptr r = Tmp_Push(8);
    U16(r) = 0; U16(r + 2) = U16(k + 0xA6); U16(r + 4) = 0;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    U16(k + 0x7C) = 0; U16(k + 0x7E) = 0; U16(k + 0x80) = U16(k + 0x9E);
    Obj_MoveLocal(k, k + 0x7C);
    if (U16(k + 0xA2)) { U16(k + 0xA2)--; U16(k + 0xA8)++; }
    else if (U16(k + 0xA8) && --U16(k + 0xA8) == 1) U16(k + 0xA4) = (u16)(1 - U16(k + 0xA4));
    u16 side = U16(k + 0xA4);
    if (!side) U16(k + 0x4C) += (u16)(U16(k + 0xA8) << 2);
    else if (side == 1) U16(k + 0x4C) -= (u16)(U16(k + 0xA8) << 2);
    if (!U16(k + 0x98)) U32(k + 0x50) |= 0x40000000;
}

/* ---- kind 16 of area 4 (0x44BFA0), steps 0x721A48: a puff of fire or smoke
   (+0x9E 1: the other look) drifting by +0x7C.. until its animation ends (K2Lone_Move) ---- */

void DC2CALL K2_Fume(gptr k)                                        /* 0x44BFA0 */
{
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721A48 + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (U8(k + 0x50) & 4) K2_UpdateHidden(k);
    if (U8(k + 0x46) && !--U8(k + 0x46) && U32(k + 0x60)) K2_Detach(k);
}

void DC2CALL K2Fume_Init(gptr k)                                    /* 0x44C000 */
{
    gptr r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    u32 f = U32(k + 0x50);
    U8(k + 0x54)++;
    U32(k + 0x50) = f | 2;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
    int other = U16(k + 0x9E) == 1;
    K2_SetAnim(k, other ? 0x5EC23C : 0x5EC174);
    U8(k + 0x67) |= 2;
    U16(k + 0x6A) = other ? 0x39 : 0x37;
    U16(k + 0x68) = other ? 0x7E41 : 0x7E40;
}

void DC2CALL K2_Gone(gptr k)                                        /* 0x44C0E0 */
{
    K2_Unlink(k);
}

/* ---- kind 17 of area 4 (0x44C420), by +0x7A (0x721A60): 0 a flare shot
   (0x44C2D0, steps 0x721A54) that lights up where it lands or hits; 1 a glow
   kept at a light (the first after G+0x948) in the camera's view ---- */

void DC2CALL K2_Flare17(gptr k)                                     /* 0x44C420 */
{
    U32(k + 0x74) = U32(k + 0x40); U16(k + 0x78) = U16(k + 0x44);
    Orig_Call1(U32(0x721A60 + U8(k + 0x7A) * 4), k);
}

void DC2CALL K2_FlareShot(gptr k)                                   /* 0x44C2D0 */
{
    U32(k + 0x74) = U32(k + 0x40); U16(k + 0x78) = U16(k + 0x44);
    Orig_Call1(U32(0x721A54 + U8(k + 0x54) * 4), k);
}

void DC2CALL K2FlareShot_Init(gptr k)                               /* 0x44C0F0 */
{
    U8(k + 0x54)++;
    U8(k + 0x67) |= 2;
    U32(k + 0x50) = 1;
    U8(k + 0x85) = 0x10;
    K2_SetAnim(k, 0x5EE670);
}

/* a light where it ended (big: on the floor), and its sound */
void DC2CALL K2FlareShot_Light(gptr k, u32 big)                     /* 0x44C260 */
{
    gptr l = Light_Alloc();
    if (l) {
        U8(l + 1) = 1;
        U32(l + 0xC) = 0x5EE670;
        U8(l + 2) = 3;
        U32(l + 0x10) = big ? 0x5F0EDC : 0x5F05E0;
        U16(l + 0x1A) = U16(k + 0x40); U16(l + 0x1C) = U16(k + 0x42); U16(l + 0x1E) = U16(k + 0x44);
    }
    Se_PlayFlat(big ? 0x63 : 0x70, 0x64, 0x40);
}

void DC2CALL K2FlareShot_Fly(gptr k)                                /* 0x44C130 */
{
    gptr c = G() + 0x778;
    if (++U8(k + 0x98) == 0xA) U8(k + 0x54)++;
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    U16(k + 0x5A) = (u16)(S16(k + 0x42) / -1000);
    RotMatrixYXZ(k + 0x48, k);
    gptr v = Tmp_Push(0x10);
    S32(v) = S16(k + 0x40) - S16(c + 0x28); S32(v + 4) = S16(k + 0x42) - S16(c + 0x2A); S32(v + 8) = S16(k + 0x44) - S16(c + 0x2C);
    ApplyMatrixLV(c, v, k + 0x14);                  /* into the camera's view */
    U32(k + 0x14) += (u32)(s32)S16(c + 0x30); U32(k + 0x18) += (u32)(s32)S16(c + 0x32); U32(k + 0x1C) += U16(c + 0x34);
    u32 hit = Weapon_Hit(k, k + 0x40, 0x20);
    if (!hit) {
        s16 f = (s16)Floor_Cross(k + 0x40, k + 0x74, v, 8);
        if (f == -2) K2FlareShot_Light(k, 1);
        else if (!f || Eff_WallHit(k)) K2FlareShot_Light(k, 0);
        else { Tmp_Pop(0x10); return; }
    }
    Tmp_Pop(0x10);
    U8(k + 0x54) = 2;
}

void DC2CALL K2_FlareGlow(gptr k)                                   /* 0x44C300 */
{
    gptr l = U32(G() + 0x948) + 0x34;
    U32(k + 0x98) = l;
    gptr m = Tmp_Push(0x20);
    for (u32 i = 0; i < 0x12; i += 2) U16(m + i) = (i == 0 || i == 8 || i == 0x10) ? 0x1000 : 0;
    RotMatrixY(-(s32)S16(l + 0x1A), m);
    RotMatrixX(-(s32)S16(l + 0x18), m);
    switch (U8(k + 0x54)) {
    case 0:
        U16(k + 0x6C) = 0x30; U16(k + 0x6E) = 0x30;
        U8(k + 0x54)++;
        U32(k + 0x50) = 1;
        U8(k + 0x84) = 0x40;
        U16(k + 0x48) = 0; U16(k + 0x4A) = 0; U16(k + 0x4C) = 0;
        RotMatrix(k + 0x48, k + 0x20);
        U16(k + 0x6A) = 0x37; U16(k + 0x68) = 0x7E81;
        K2_SetAnim(k, 0x5EEA58);
        /* fall through */
    case 1:                                         /* along the light's axis */
        U16(k + 0x40) = (u16)(U16(m + 0xC) * 3 + 0x1B66);
        U16(k + 0x42) = (u16)(U16(m + 0xE) * 3 - 0x16D7);
        U16(k + 0x44) = (u16)(U16(m + 0x10) * 3 + 0x2F29);
        break;
    }
    Tmp_Pop(0x20);
}

/* ---- kind 21 of area 8 (0x44C450), steps 0x721AD4: a burst by +0x7A (looks
   at 0x721A68, 12 bytes each: u8 drawing bits, u16 +0x6A, u16 +0x68, animation);
   kind 0 adds two copies of itself, crossed, the second larger. Step 3
   (0x44C620) sets the animations of that table from +0x98 and +0x9E.. ---- */

#define BURST_LOOKS 0x721A68u

static void zero_rot(gptr k)
{
    gptr r = Tmp_Push(8);
    U32(r) = 0; U32(r + 4) = 0;
    RotMatrix(r, k + 0x20);
    Tmp_Pop(8);
}

void DC2CALL K2_Burst21(gptr k)                                     /* 0x44C450 */
{
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721AD4 + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (!--U8(k + 0x46)) K2_Detach(k);
}

void DC2CALL K2Burst21_Init(gptr k)                                 /* 0x44C490 */
{
    gptr src = U32(k + 0x8C);
    U8(k + 0x54)++;
    zero_rot(k);
    gptr rec = BURST_LOOKS + U8(k + 0x7A) * 12;
    U16(k + 0x6A) = U16(rec + 2);
    U16(k + 0x68) = U16(rec + 4);
    K2_SetAnim(k, U32(rec + 8));
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= U8(BURST_LOOKS + U8(k + 0x7A) * 12);
    if (src && (U8(src) & 1) && U8(src + 1) == 1) U8(src + 0x18)--;
    if (U8(k + 0x7A)) return;
    U8(k + 0x84) |= 1;
    gptr a = K2_AllocBack();
    if (!a) { K2_Unlink(k); return; }
    memcpy(MEM(a), MEM(k), 0xAC);
    K2_SyncParent(a);
    U16(a + 0x4A) = 0x400;                          /* crossed */
    gptr b = K2_AllocBack();
    if (!b) { K2_Unlink(k); K2_Unlink(a); return; }
    memcpy(MEM(b), MEM(k), 0xAC);
    K2_SyncParent(b);
    K2_SetAnim(b, U32(BURST_LOOKS + 0x14 + U8(k + 0x7A) * 12));
    U16(b + 0x48) = 0x400;
    U16(b + 0x6C) = (u16)((s32)U16(k + 0x6E) * 15 / 10);
    U16(b + 0x6E) = (u16)((s32)U16(k + 0x6E) * 15 / 10);
}

void DC2CALL K2Burst21_Looks(gptr k)                                /* 0x44C620 */
{
    U32(BURST_LOOKS + 8) = U32(k + 0x98);
    for (u32 i = 0; i < 8; i++)
        U32(BURST_LOOKS + 0x14 + 12 * i) = U32(BURST_LOOKS + 8 + 12 * i) + U16(k + 0x9E + 2 * i);
    K2_Unlink(k);
}

/* drifting by +0x7C.. until its animation ends */
void DC2CALL K2_Drift(gptr k)                                       /* 0x44D190 */
{
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    if (K2_AnimStep(k)) U8(k + 0x54)++;
}

/* ---- kind 19 of area 8 (0x44C670), steps 0x721AE4: a splash (+0x98 0:
   straight up at -+0x9A, else out at the angle +0x9C, speed +0x9A, up +0xA2),
   growing as it goes (by +0x9E a frame); its colour +0xA6.. ---- */

void DC2CALL K2_Splash19(gptr k)                                    /* 0x44C670 */
{
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721AE4 + U8(k + 0x54) * 4), k);
    if (U8(k + 0x50) & 4) K2_UpdateHidden(k);
}

static u16 mul12(s16 a, s16 b) { return (u16)((s32)a * b / 4096); }

void DC2CALL K2Splash19_Init(gptr k)                                /* 0x44C6B0 */
{
    U8(k + 0x54)++;
    K2_SetAnim(k, 0x5E0290);
    U16(k + 0x6A) = 0x37;
    U32(k + 0x50) |= 2;
    U8(k + 0x67) |= 2;
    U16(k + 0x68) = 0x7E80;
    U16(k + 0x6E) = U16(k + 0xA0); U16(k + 0x6C) = U16(k + 0xA0);
    U16(k + 0xA4) = 0;
    U8(k + 0x64) = U8(k + 0xA6); U8(k + 0x65) = U8(k + 0xA7); U8(k + 0x66) = U8(k + 0xA8);
    if (U16(k + 0x98)) {
        U16(k + 0x74) = (u16)-rcos(S16(k + 0x9C));
        U16(k + 0x78) = (u16)rsin(S16(k + 0x9C));
        U16(k + 0x7C) = mul12(S16(k + 0x74), S16(k + 0x9A));
        U16(k + 0x7E) = U16(k + 0xA2);
        U16(k + 0x80) = mul12(S16(k + 0x78), S16(k + 0x9A));
    } else U16(k + 0x7E) = (u16)-U16(k + 0x9A);
}

/* away from zero by d */
static u16 grow_by(u16 v, u16 d) { return (s16)v < 0 ? (u16)(v - d) : (u16)(d + v); }

void DC2CALL K2Splash19_Rise(gptr k)                                /* 0x44C7B0 */
{
    U16(k + 0x40) += U16(k + 0x7C);
    U16(k + 0x6C)++; U16(k + 0x6E)++;
    U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    U16(k + 0x7E) = grow_by(U16(k + 0x7E), U16(k + 0x9E));
    if (K2_AnimStep(k)) K2_Unlink(k);
}

void DC2CALL K2Splash19_Spread(gptr k)                              /* 0x44C810 */
{
    U16(k + 0x6C)++; U16(k + 0x6E)++;
    U16(k + 0x40) += U16(k + 0x7C); U16(k + 0x42) += U16(k + 0x7E); U16(k + 0x44) += U16(k + 0x80);
    U16(k + 0x9A) = grow_by(U16(k + 0x9A), U16(k + 0x9E));
    U16(k + 0x7C) = mul12(S16(k + 0x74), S16(k + 0x9A));
    U16(k + 0x80) = mul12(S16(k + 0x78), S16(k + 0x9A));
    if (K2_AnimStep(k)) K2_Unlink(k);
}

/* the start of a drop or splash: its animation +0x70 */
void DC2CALL K2Drop_Init(gptr k)                                    /* 0x44CA80 */
{
    gptr src = U32(k + 0x8C);
    U8(k + 0x54)++;
    zero_rot(k);
    K2_SetAnim(k, U32(k + 0x70));
    U32(k + 0x50) |= 2;
    if (src && (U8(k + 0x84) & 8)) U8(src + 0x18)--;
}

/* falling; back to its last place and bouncing up at 3/5 when it meets the floor */
void DC2CALL K2Drop_Fall(gptr k)                                    /* 0x44C920 */
{
    u32 last = U32(k + 0x40);
    U16(k + 0x40) += U16(k + 0x7C);
    U16(k + 0x42) += U16(k + 0x7E);
    U32(k + 0x74) = last;
    u16 z = U16(k + 0x44);
    U16(k + 0x78) = z;
    U16(k + 0x44) = (u16)(z + U16(k + 0x80));
    gptr src = U32(k + 0x8C);
    U16(k + 0x5A) = (u16)(S16(k + 0x42) / -1000);
    if ((u16)Floor_Cross(k + 0x40, k + 0x74, k + 0x40, 0x10) != 0xFFFF) {
        U16(k + 0x44) = U16(k + 0x78); U16(k + 0x40) = U16(k + 0x74); U16(k + 0x42) = U16(k + 0x76);
        U16(k + 0x7E) = (u16)(-3 * (s32)S16(k + 0x7E) / 5);
    }
    if (K2_AnimStep(k) && !(U8(k + 0x84) & 8)) {
        U8(k + 0x54)++;
        if (src) U8(src + 0x18)--;
    }
}

/* ---- kind 17 of areas 8 and 9 (0x44C8C0, 0x44CA00), steps 0x721AF0 /
   0x721AFC: drops from a source +0x8C, gone with it ---- */

void DC2CALL K2_Drops8(gptr k)                                      /* 0x44C8C0 */
{
    gptr src = U32(k + 0x8C);
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721AF0 + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (src && !(U8(src) & 1) && !(U8(k + 0x84) & 8)) K2_Unlink(k);
    if (!--U8(k + 0x46)) K2_Detach(k);
}

void DC2CALL K2_Drops9(gptr k)                                      /* 0x44CA00 */
{
    gptr src = U32(k + 0x8C);
    Orig_Call1(U32(K2_MOTION + U8(k + 0x7B) * 4), k);
    Orig_Call1(U32(0x721AFC + U8(k + 0x54) * 4), k);
    K2_SyncParent(k);
    if (U32(k + 0x8C) && !(U8(src) & 1) && !(U8(k + 0x84) & 8)) K2_Unlink(k);
    if (U8(k + 0x46) && !--U8(k + 0x46) && U32(k + 0x60)) {
        K2_MatrixAngles(U32(k + 0x60) + 0x20, k + 0x48);  /* turned as its owner, left behind */
        K2_Detach(k);
    }
}
