/* The mosasaurus (object type handler 0x43F820, type 10, E80.DAT): it swims
 * under the surface (o+0x10E, the floor height above it; o+0x1EE how deep it
 * keeps), turning (yaw o+0x4A, at most o+0x1DA a frame) and pitching (o+0x48,
 * at most o+0x1D8) to its target o+0xD8, and attacks the player on the
 * surface: a bite from below, a leap that drags the player under, a seize.
 * Bits at o+0x1C0: 0 holds the bite, 1 moving (its head looks), 2 heading for
 * a point of its own (o+0x1F2; o+0x1FA/o+0x1FC its timers), 3 diving after a
 * hit, 4 does not attack while set.
 * Bits shared at 0x87E974: 0 one bites.
 * States (o+0x54): 0 init, 1 normal (think 0x7211F8, then move 0x721218 by
 * o+0x55), 2 hit (0x721238), 3 die (0x721254), 4 (0x4A56D0). Attacks land
 * through 0x721270.
 * Moves (o+0x55): 0 swim, 1 rush, 2 turn (o+0x57 the side), 3 pitch, 4
 * dive, 5 lunge (the bite), 6 leap, 7 seize. Hits: 0 the damage, 1/2
 * flinch, 3/6 dive, 4/5 knocked over; dying: 1/3/4/6 and 2/5 sink. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define MO_MAIN  0x7211E4u      /* state handlers */
#define MO_THINK 0x7211F8u      /* state 1: deciding, by o+0x55 */
#define MO_MOVE  0x721218u      /* state 1: moving, by o+0x55 */
#define MO_HIT   0x721238u      /* state 2 */
#define MO_DIE   0x721254u      /* state 3 */
#define MO_GRAB  0x721270u      /* an attack landed, by o+0x55 */
#define MO_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u
#define PL()     U32(G() + 0x940)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void dec16(gptr p) { if (U16(p)) U16(p)--; }
static void speed(gptr o, u32 step, u32 v) { Value_Approach(o + 0xF4, step, v); }
/* its pitch levels out */
static void level(gptr o) { U16(o + 0x48) += (u16)Angle_Approach(U16(o + 0x48), 0x28, 0); }
/* the animation's player motion moves both, the player where it is */
static void carry(gptr o, gptr pl)
{
    U16(o + 0x40) += U16(pl + 0xE8);
    U16(o + 0x42) += U16(pl + 0xEA);
    U16(o + 0x44) += U16(pl + 0xEC);
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
}
/* the player's animation from its table (+0x74.. of the mosasaurus' animations) */
static gptr pl_anim(gptr o, u32 k) { return U32(U32(o + 0x88) + k); }
static void pl_place(gptr pl)
{
    Obj_SyncLinks(pl);
    Obj_UpdateMatrix(pl);
    Obj_CollideWalls(pl);
}
/* the player loses health; the game ends when it runs out */
static void pl_damage(gptr pl)
{
    U16(pl + 0x108) -= (u16)Damage_Scale(0x64);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
    Player_Bleed(0x1F4);
}
/* a 12-bit angle with its sign (the upper bits kept as they are) */
static s16 sign12(u16 a) { return (s16)(a & 0x800 ? a | 0xF000 : a); }

void DC2CALL Mosa_Handler(gptr o)                                   /* 0x43F820 */
{
    gptr pos = o + 0x40, b = MO_BITS(o);
    U8(o + 0x206) = U8(G() + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    U16(o + 0x10E) = (u16)Floor_Height(pos, pos, 0x40, 0);
    Orig_Call1(U32(MO_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x4C) = 0;
    gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(hp + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x500 && !Bits_Check(b, 2) && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    /* under the surface, above the bottom, not too deep */
    if (S16(o + 0x42) > S16(o + 0x10E) - S16(o + 0x1EE)) U16(o + 0x42) = U16(o + 0x10E) - U16(o + 0x1EE);
    gptr v = Tmp_Push(0x10), w = v + 8;
    U16(v) = U16(pos); U16(v + 2) = 0x86E8; U16(v + 4) = U16(o + 0x44);
    U16(w) = U16(pos); U16(w + 2) = U16(o + 0x42); U16(w + 4) = U16(o + 0x44);
    s16 bottom = (s16)Floor_Height(v, w, 0x40, 0);
    Tmp_Pop(0x10);
    if (S16(o + 0x42) < bottom + S16(o + 0x1EE)) U16(o + 0x42) = (u16)(bottom + U16(o + 0x1EE));
    if (S16(o + 0x42) < S16(o + 0x1EE) - 15000) U16(o + 0x42) = (u16)(U16(o + 0x1EE) - 15000);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    s16 p = sign12(U16(o + 0x48) & 0xFFF);          /* its pitch within +-0x3E0 */
    if (p > 0x3E0) p = 0x3E0;
    if (p < -0x3E0) p = -0x3E0;
    U16(o + 0x48) = (u16)p & 0xFFF;
    if (U16(o + 0x208) && (s32)S16(o + 0x208) % 4 == 0) Se_Play(o, 0x38);
    Mosa_Timer(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    if (U8(o + 0x18D)) U8(o + 0x18D)--;
    dec16(o + 0x1FA); dec16(o + 0x1FC); dec16(o + 0x208);
    Mosa_Grab(o);
    U8(o + 0x1CA) = U8(o + 0x56); U8(o + 0x1C8) = U8(o + 0x54);
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1CB) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Mosa_Init(gptr o)                                      /* 0x43FB00 */
{
    U32(o + 0x50) = 0xB;
    U8(o + 0x54)++;
    U16(o + 0x108) = U8(G() + 0x108E) == 2 ? 0xAF0 : 0x578;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x721188;
    U32(o + 0x128) = 0x7211A0;
    U32(o + 0x134) = 0x7211C0;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x643100;
    Anim_Start(o, 2, 0, 1, 5);
    Obj_Set118(o, 2, 0x1FA0, 0x4E80);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 0xD;
    U8(o + 0x160) = 0;
    U16(o + 0x164) = 0x10;
    U16(o + 0x166) = 0x10;
    U16(o + 0x16A) = 0x80;
    U16(o + 0x16E) = 0x80;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = 0;
    Bits_Set(MO_BITS(o), 1, 1);
    U8(o + 0x118) |= 0x20;
    U32(o + 0x184) |= 0x10;
    U32(o + 0x54) = 1;
    U8(o + 0xFE) = 3;
    U16(o + 0x1EE) = 0x12C;
    Flag_Set(2, 0x26, 1);
    U16(o + 0x208) = 0xC;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Mosa_Normal(gptr o)                                    /* 0x43FCA0 */
{
    Orig_Call1(U32(MO_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(MO_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Mosa_Hit(gptr o)                                       /* 0x43FCD0 */
{
    Orig_Call1(U32(MO_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Mosa_Die(gptr o)                                       /* 0x43FCF0 */
{
    Orig_Call1(U32(MO_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Mosa_Grab(gptr o)                                      /* 0x43FD10 */
{
    Enemy_AttackHit(o);
    if (S8(o + 0x119) >= 0) Orig_Call1(U32(MO_GRAB + U8(o + 0x55) * 4), o);
}

/* its turning and pitching speeds approach these */
static void steer(gptr o, u32 yaw, u32 pitch)
{
    U16(o + 0x1DA) += (u16)Angle_Approach(U16(o + 0x1DA), 4, yaw);
    U16(o + 0x1D8) += (u16)Angle_Approach(U16(o + 0x1D8), 4, pitch);
}

/* deciding while swimming: rush when far */
void DC2CALL Mosa_ThinkSwim(gptr o)                                 /* 0x43FD40 */
{
    if (U8(o + 0x18D)) return;
    steer(o, 0xA, 0xA);
    if (!Mosa_Decide(o) && U32(o + 0x13C) > 0xBB8) U32(o + 0x54) = 0x101;
}

void DC2CALL Mosa_Swim(gptr o)                                      /* 0x43FDB0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 2, 0, 1, 0xF);
        Bits_Set(MO_BITS(o), 1, 1);
        Se_Play(o, 0x38); Se_Play(o, 0x39);
        return;
    case 1:
        if (Anim_Step(o)) { Se_Play(o, 0x38); Se_Play(o, 0x39); }
        speed(o, 0xA, 0x64);
        return;
    }
}

/* deciding while rushing: swim again when near */
void DC2CALL Mosa_ThinkRush(gptr o)                                 /* 0x43FE50 */
{
    steer(o, 0xA, 0xA);
    if (!Mosa_Decide(o) && U32(o + 0x13C) < 0xBB8) U32(o + 0x54) = 1;
}

void DC2CALL Mosa_Rush(gptr o)                                      /* 0x43FEB0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 3, 0, 1, 0xF);
        Bits_Set(MO_BITS(o), 1, 1);
        Se_Play(o, 0x3A); Se_Play(o, 0x3B);
        Effect_Attach(o, 0x65D600);
        return;
    case 1:
        if (Anim_Step(o)) {
            Se_Play(o, 0x3A); Se_Play(o, 0x3B);
            Effect_Attach(o, 0x65D600);
        }
        speed(o, 0x14, 0xC8);
        return;
    }
}

/* deciding while turning: swim on when facing the target */
void DC2CALL Mosa_ThinkTurn(gptr o)                                 /* 0x43FF60 */
{
    steer(o, 0x3C, 0xA);
    if (!Mosa_Decide(o) && iabs((s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A))) < 0x1E) U32(o + 0x54) = 1;
}

/* turning (o+0x57 the side) or pitching, in place */
static void turn(gptr o, u32 a0, u32 a1)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, U8(o + 0x57) ? a1 : a0, 0, 1, 0xF);
        Bits_Set(MO_BITS(o), 1, 0);
        Se_Play(o, 0x39);
        break;
    case 1:
        if (Anim_Step(o)) Se_Play(o, 0x39);
        break;
    }
    speed(o, 0xA, 0x78);
}

void DC2CALL Mosa_Turn(gptr o)                                      /* 0x43FFD0 */
{
    turn(o, 0, 1);
}

/* deciding while pitching: swim on when level with the target */
void DC2CALL Mosa_ThinkPitch(gptr o)                                /* 0x440070 */
{
    steer(o, 0xA, 0x32);
    if (!Mosa_Decide(o) && iabs((s16)Mosa_PitchDiff(o + 0x40, o + 0xD8, U16(o + 0x48))) < 0x1E) U32(o + 0x54) = 1;
}

void DC2CALL Mosa_Pitch(gptr o)                                     /* 0x4400E0 */
{
    turn(o, 0x1B, 0x1C);
}

/* the side to turn to (0 / 1, -1 none); exact: any angle off, else more than 0x1E */
static s32 side_of(s16 v, int exact)
{
    if (exact) return v > 0 ? 0 : v < 0 ? 1 : -1;
    return v > 0x1E ? 0 : v < -0x1E ? 1 : -1;
}

/* where to head, and an attack or a turn; 1 when it starts one */
u32 DC2CALL Mosa_Decide(gptr o)                                     /* 0x440180 */
{
    gptr pl = PL(), b = MO_BITS(o), pos = o + 0x40, t = o + 0xD8;
    if (Bits_Check(b, 2)) {                         /* to its own point while far from it */
        if (Dist2_XZ(pos, o + 0x1F2) >= 0x895440 && U16(o + 0x1FC)) {
            Bits_Set(b, 4, 1);
            Zone_TrackPoint(o, o + 0x1F2);
        } else {
            Bits_Set(b, 2, 0);
            Bits_Set(b, 4, 0);
            U16(o + 0x1FA) = 0x1E;
        }
    } else if (U32(o + 0x13C) >= 0x7D0) U16(o + 0x1FA) = 0x1E;
    else if (!U16(o + 0x1FA)) {                     /* near the player: away for a while */
        Bits_Set(b, 2, 1);
        Zone_RandomPoint(o, o + 0x1F2);
        U16(o + 0x1FC) = 0x3C;
    }
    Obj_TurnToPoint(o, U16(o + 0x1DA), t);
    Mosa_PitchTo(o, U16(o + 0x1D8), t);
    if (!U8(o + 0x13A) && !Bits_Check(b, 4) && !Bits_Check(SHARED, 0)) {
        s32 a = iabs((s16)Angle_DiffTo(pos, t, U16(o + 0x4A)));
        s16 yd = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
        s32 pd = iabs((s16)Mosa_PitchDiff(pos, t, U16(o + 0x48)));
        s32 pitch = iabs(sign12(U16(o + 0x48)));
        if (a < 0x180 && pd < 0x10 && !U32(o + 0x1D0)) {
            if (U32(o + 0x13C) < 0xBB8) {           /* the bite */
                Mosa_Bite(o);
                U32(o + 0x54) = 0x30501;
                return 1;
            }
            Rand8();                                /* (its roll decides nothing here) */
        }
        if (a < 0x100 && pd < 0x80 && pitch < 0x40) {
            if (!U32(o + 0x1D0) && Rand8() < 0x40 && U32(o + 0x13C) >= 0xE10 && U32(o + 0x13C) <= 0xFA0) {
                Mosa_Bite(o);                       /* the leap */
                U32(o + 0x54) = 0x601;
                return 1;
            }
            if ((U8(pl + 0x174) & 2) && iabs(yd) < 0x200 && U32(o + 0x13C) <= 0x1770 && U32(o + 0x13C) >= 0x7D0
                && !Flag_Check(2, 0x1A)) {
                Mosa_Bite(o);                       /* the seize */
                U32(o + 0x54) = 0x701;
                return 1;
            }
        }
    }
    s32 s = side_of((s16)Mosa_PitchDiff(pos, t, U16(o + 0x48)), U8(o + 0x55) == 3);
    if (s >= 0) {                                   /* pitch to the target */
        if (U8(o + 0x55) != 3 || U8(o + 0x57) != (u32)s) {
            if (U8(o + 0x55) == 3) { U16(o + 0x1D8) = 0; U16(o + 0x1DA) = 0; }
            U32(o + 0x54) = (u32)s << 24 | 0x301;
            return 1;
        }
    }
    if (U8(o + 0x55) == 3) return 0;
    s = side_of((s16)Angle_DiffTo(pos, t, U16(o + 0x4A)), U8(o + 0x55) == 2);
    if (s >= 0) {                                   /* turn to it */
        if (U8(o + 0x55) != 2 || U8(o + 0x57) != (u32)s) {
            if (U8(o + 0x55) == 2) { U16(o + 0x1D8) = 0; U16(o + 0x1DA) = 0; }
            U32(o + 0x54) = (u32)s << 24 | 0x201;
            return 1;
        }
    }
    return 0;
}

/* diving after a hit, then up again, rolling up and down (o+0x1FE) */
void DC2CALL Mosa_Dive(gptr o)                                      /* 0x4405A0 */
{
    gptr b = MO_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0x16, 0, 0x41, 0x14);
        Effect_Attach(o, 0x65EB50);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 3, 1);
        U16(o + 0x1E6) = 0x78;
        return;
    case 1: {
        if (Anim_Step(o)) Effect_Attach(o, 0x65EB50);
        U16(o + 0x1E6)--;
        if (S16(o + 0x1E6) <= 0) {
            U8(o + 0x56)++;
            Anim_Start(o, 0x13, 0, 0x40, 0xA);
            Bits_Set(b, 3, 0);
        }
        U16(o + 0x1FE) += U16(o + 0x200) ? 2 : 0xFFFE;
        s16 v = S16(o + 0x1FE);
        if (v > 0x1E) U16(o + 0x200) = 0;
        if (v < -0x1E) U16(o + 0x200) = 1;
        U16(o + 0x42) += (u16)v;
        return;
    }
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        speed(o, 2, 0x64);
        return;
    }
}

/* deciding in the lunge: face the target first */
void DC2CALL Mosa_ThinkLunge(gptr o)                                /* 0x4406F0 */
{
    if (U8(o + 0x56) <= 1) Obj_TurnToPoint(o, 0xA, o + 0xD8);
}

/* the lunge: up at the player, the bite, away */
void DC2CALL Mosa_Lunge(gptr o)                                     /* 0x440710 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 6, 0, 0x40, 0xA);
        Bits_Set(MO_BITS(o), 1, 0);
        return;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, 4, 0, 0, 0xA);
            Effect_Attach(o, 0x65D6F4);
            U16(o + 0xF4) = 0;
        }
        speed(o, 0x1E, 0);
        level(o);
        return;
    case 2:
        if (Anim_Step(o)) {
            Mosa_Release(o);
            Bits_Set(MO_BITS(o), 2, 1);
            U32(o + 0x54) = 1;
            Zone_RandomPoint(o, o + 0x1F2);
            U16(o + 0xF4) += 0x50;
            U16(o + 0x1FC) = 0x3C;
            return;
        }
        speed(o, 0x14, 0xC8);
        return;
    case 3:                                         /* (a lunge from close: the bite at once) */
        U8(o + 0x56)--;
        Anim_Start(o, 4, 6, 0, 5);
        Effect_Attach(o, 0x65D6F4);
        Bits_Set(MO_BITS(o), 1, 0);
        return;
    }
}

/* the leap: out of the water onto the player, dragging it in */
void DC2CALL Mosa_Leap(gptr o)                                      /* 0x440880 */
{
    gptr pl = PL();
    if (U8(o + 0x56) == 4 && U16(o + 0x92) < 8) carry(o, pl);
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0xD, 0, 0x41, 0xA);
        Bits_Set(MO_BITS(o), 1, 0);
        return;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, 5, 0, 0x40, 0);
            Effect_Attach(o, 0x65D888);
        }
        return;
    case 2:                                         /* (missed) */
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2E) {
            Mosa_Release(o);
            U32(o + 0x54) = 1;
        }
        return;
    case 3:                                         /* caught: the player thrown in */
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, 7, 0, 0x40, 0);
            Effect_Attach(o, 0x65DA94);
            U16(o + 0x48) = 0;
            Anim_Start(pl, pl_anim(o, 0x94), 0, 0x40, 0);
            Obj_SyncLinks(pl);
            Obj_UpdateMatrix(pl);
            Obj_CollideWalls(pl);
            U32(G() + 0x92C) = 0x440AF0;
            gptr p = U32(pl + 0x64);
            U16(o + 0x20A) = U16(p + 0x40); U16(o + 0x20C) = U16(p + 0x42); U16(o + 0x20E) = U16(p + 0x44);
            pl_damage(pl);
            Flash_Play(o, 0x65F290);
        }
        return;
    case 4:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x15) {
            Mosa_Release(o);
            U32(o + 0x54) = 1;
            Bits_Set(MO_BITS(o), 2, 1);
            Fn_454950(o, o + 0x1F2, 0xC0);
            U16(o + 0x1FC) = 0x3C;
        }
        return;
    }
}

/* the player thrown by the leap (o+0x1D8 the mosasaurus): flies, falls in,
   sinks, comes up */
void DC2CALL Mosa_PlThrown(gptr pl)                                 /* 0x440AF0 */
{
    gptr mo = U32(pl + 0x1D8);
    switch (U8(pl + 0x57)) {
    case 0: {
        Anim_Step(pl);
        gptr p = U32(pl + 0x64);
        if (U16(pl + 0x92) >= 0x2D) {
            U8(pl + 0x57)++;
            u16 y = U16(p + 0x42);
            Anim_Set(pl, pl_anim(mo, 0x8C), 0, 0, 2);
            U16(pl + 0x42) = y;
            U16(p + 0x66) = 0;
            Obj_UpdateMatrix(pl);
            U32(pl + 0x50) &= ~0x100u;
            return;
        }
        /* its body (part 0) not through the floor between where it was and is */
        gptr v = Tmp_Push(0x10), w = v + 8;
        U16(v) = U16(p + 0x40); U16(v + 2) = 0x86E8; U16(v + 4) = U16(p + 0x44);
        U16(w) = U16(mo + 0x20A); U16(w + 2) = U16(mo + 0x20C); U16(w + 4) = U16(mo + 0x20E);
        s16 f = (s16)Floor_Height(v, w, 0x80, 0);
        Tmp_Pop(0x10);
        if (f > S16(p + 0x42)) {
            U16(pl + 0x42) += (u16)(f - U16(p + 0x42) + 0x1F4);
            Obj_UpdateMatrix(pl);
        }
        U16(mo + 0x20A) = U16(p + 0x40); U16(mo + 0x20C) = U16(p + 0x42); U16(mo + 0x20E) = U16(p + 0x44);
        return;
    }
    case 1: {
        s16 f = (s16)Floor_Height(pl + 0x40, pl + 0x40, 0x80, 0);
        Anim_Step(pl);
        U16(pl + 0x42) += 0x40;
        if (S16(pl + 0x42) >= f - 0x1F4) {
            U16(pl + 0x42) = (u16)(f - 0x1F4);
            U16(mo + 0x204) = (u16)(f - 0x1F4);
            U8(pl + 0x57)++;
            Anim_Start(pl, pl_anim(mo, 0x90), 0, 0x40, 0);
            Effect_Attach(pl, 0x65DDE4);
        }
        return;
    }
    case 2:
        U16(pl + 0x42) = U16(mo + 0x204);
        if (Anim_Step(pl)) {
            gptr p = U32(pl + 0x64);
            U16(p + 0x4A) = (U16(p + 0x4A) - 0x390) & 0xFFF;
            U16(pl + 0x4A) = (U16(pl + 0x4A) + 0x390) & 0xFFF;
            Anim_Set(pl, 0xC, 0, 0, 0);
            U16(pl + 0x42) = U16(mo + 0x204) + 0x64;
            Obj_UpdateMatrix(pl);
            U32(pl + 0x54) = 0x30202;
            Anim_Start(pl, 0xC, 0, 0, 0);
            Obj_UpdateMatrix(pl);
            U32(pl + 0x50) &= ~0x20u;
        }
        return;
    }
}

/* deciding in the seize: face the target, then keep to its point */
void DC2CALL Mosa_ThinkSeize(gptr o)                                /* 0x440D30 */
{
    if (U8(o + 0x56) <= 1) { Obj_TurnToPoint(o, 0x1E, o + 0xD8); return; }
    if (U8(o + 0x56) <= 3) {
        Zone_TrackPoint(o, o + 0x1F2);
        Obj_TurnToPoint(o, 0x1E, o + 0xD8);
    }
}

/* the seize: holds the player, swims off with it under, lets it go */
void DC2CALL Mosa_Seize(gptr o)                                     /* 0x440D80 */
{
    gptr pl = PL();
    u8 st = U8(o + 0x56);
    if (st == 2 || st == 3 || (st == 4 && U16(o + 0x92) < 0xA)) carry(o, pl);
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U32(o + 0x50) &= ~8u;
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 8, 0, 0, 0xA);
        Player_Hold(o, 0, 0x48E100);
        Bits_Set(MO_BITS(o), 1, 0);
        return;
    case 1:                                         /* up to the player */
        if (Anim_Step(o)) {
            Anim_Start(o, 9, 0, 1, 0xA);
            Se_Play(o, 0x3A);
        }
        if (U32(o + 0x13C) <= 0x7D0) {
            U8(o + 0x56)++;
            U16(o + 0x42) = U16(pl + 0x42) - 0x3EC;
            U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
            Anim_Start(o, 0xA, 0, 0x40, 0xA);
            Flash_Play(o, 0x65F2A0);
            U16(pl + 0xF4) = 0; U16(pl + 0xF2) = 0; U16(pl + 0xF0) = 0;
            Anim_Start(pl, pl_anim(o, 0x7C), 0, 0x40, 0);
            U32(pl + 0x50) |= 0x120;
            U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
            U16(pl + 0x4A) = U16(o + 0x4A);
            pl_place(pl);
            Fn_454950(o, o + 0x1F2, 0xC0);
        }
        speed(o, 0x14, 0xC8);
        Value_Approach(o + 0x42, 0x14, (u16)(U16(pl + 0x42) - 0x3EC));
        return;
    case 2:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0xB, 0, 0x41, 0xA);
            Flash_Play(o, 0x65F2B0);
            U16(o + 0x48) = 0;
            Anim_Start(pl, pl_anim(o, 0x80), 0, 0x41, 0xA);
            pl_place(pl);
        }
        return;
    case 3:                                         /* swimming off with it to its point */
        if (Anim_Step(o)) Flash_Play(o, 0x65F2B0);
        if (Dist2_XZ(o + 0x40, o + 0x1F2) < 0x17D7840 || U16(o + 0xE8) || U16(o + 0xEA) || U16(o + 0xEC)
            || U16(pl + 0xE8) || U16(pl + 0xEA) || U16(pl + 0xEC)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0xC, 0, 0x40, 0);
            Flash_Play(o, 0x65F2BC);
            Effect_Attach(o, 0x65DC00);
            Anim_Start(pl, pl_anim(o, 0x84), 0, 0x40, 0);
            pl_damage(pl);
            pl_place(pl);
            U32(G() + 0x92C) = 0x4411E0;
        }
        speed(o, 0x14, 0xDC);
        return;
    case 4: {                                       /* lets it go, turns about */
        if (U16(o + 0x92) > 0xA) speed(o, 0x14, 0);
        if (!Anim_Step(o)) return;
        Mosa_Release(o);
        U32(o + 0x54) = 1;
        Bits_Set(MO_BITS(o), 2, 1);
        Fn_454950(o, o + 0x1F2, 0xC0);
        gptr p = U32(o + 0x64);
        U16(o + 0x1FC) = 0x3C;
        U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
        U16(o + 0xF4) = 0xA4;
        U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
        u16 y = U16(p + 0x42);
        Anim_Set(o, 2, 0, 1, 0xA);
        U16(o + 0x42) = y;
        U16(p + 0x66) = 0;
        return;
    }
    }
}

/* the player let go by the seize: falls to the floor */
void DC2CALL Mosa_PlDrop(gptr pl)                                   /* 0x4411E0 */
{
    Anim_Step(pl);
    gptr p = U32(pl + 0x64);
    s16 f = (s16)Floor_Height(p + 0x40, p + 0x40, 0x80, 0);
    if (S16(p + 0x42) < f && U16(pl + 0x42) != U16(pl + 0xE2)) return;
    U16(pl + 0xF4) = 0;
    U16(pl + 0x5A) = (u16)((s32)f / -1000);
    U16(pl + 0xF2) = 0;
    U32(pl + 0x50) &= ~0x120u;
    U16(pl + 0xF0) = 0;
    p = U32(pl + 0x64);
    U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
    U16(pl + 0x4A) = (U16(pl + 0x4A) - 0x800) & 0xFFF;
    Anim_Set(pl, 0xC, 0, 0, 0xA);
    Obj_UpdateMatrix(pl);
    U32(pl + 0x54) = 0x30202;
}

/* the lunge's bite landed: harder from behind (the player turned away) */
void DC2CALL Mosa_LungeHit(gptr o)                                  /* 0x4412B0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 2) return;
    s16 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65F270);
    if (iabs(d) < 0x400) {
        if (!U16(pl + 0x42)) Player_Hurt(o, 0x78, 2, 0x28);
        else Player_Hurt(o, 0x78, 1, 0);
    } else Player_Hurt(o, 0x96, 1, 0);
}

/* the seize landed: holds the player, unless it may not be held */
void DC2CALL Mosa_SeizeHit(gptr o)                                  /* 0x441340 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 2) return;
    if (Flag_Check(2, 0x1A)) {
        Flash_Play(o, 0x65F280);
        Player_Hurt(o, 0x78, 1, 0);
        return;
    }
    U8(o + 0x56)++;
    U32(o + 0x50) &= ~8u;
    Player_Hold(o, 0x78, 0x48E100);
    Anim_Start(pl, pl_anim(o, 0x74), U16(o + 0x92) >> 1, 0x40, 0);
    Flash_Play(o, 0x65F280);
    U32(pl + 0x50) |= 0x120;
    U16(pl + 0x40) = U16(o + 0x40);
    U16(o + 0x42) = U16(pl + 0x42);
    U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
    pl_place(pl);
    U16(o + 0x40) += U16(pl + 0xE8);
    U16(o + 0x42) += U16(pl + 0xEA);
    U16(o + 0x44) += U16(pl + 0xEC);
}

/* the hit's damage and how it reacts */
void DC2CALL Mosa_Damage(gptr o)                                    /* 0x441440 */
{
    gptr b = MO_BITS(o);
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;      /* in its attack: more */
    U32(o + 0x54) = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
    if (Bits_Check(b, 3)) U32(o + 0x54) = 0x402;
    else {
        U32(o + 0x1CC) += U16(o + 0x176);
        if (S32(o + 0x1CC) > S16(o + 0x10A) / 2) {  /* half its health lately: flinches */
            U32(o + 0x1CC) = 0;
            U32(o + 0x54) = 0x102;
        }
    }
    if (U16(o + 0x172) == 3) U32(o + 0x54) = Bits_Check(b, 3) ? 0x502 : 0x202;
    if (U8(o + 0x17D) == 0x12) U32(o + 0x54) = Bits_Check(b, 3) ? 0x602 : 0x302;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Mosa_Release(o);
        U16(o + 0x1FA) = 0; U16(o + 0x1FC) = 0;
        Bits_Set(b, 2, 0);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 3, 0);
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        if (U16(o + 0x1DE)) {
            U16(o + 0x4A) = (U16(o + 0x4A) + U16(o + 0x1DE)) & 0xFFF;
            U16(o + 0x1DE) = 0;
        }
        if (U8(o + 0x184) & 8) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 0xA;
        if (S16(o + 0x108) < 0) {                   /* killed */
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
            Score_Kill(o);
        }
    } else {
        U16(o + 0x108) += (u16)((s32)U16(o + 0x176) / -5);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 3 : 5;
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
        Effect_Attach(o, 0x65DED8);
    }
    Orig_Call1(U32(MO_MAIN + U8(o + 0x54) * 4), o);
}

/* a hit's animation a (and its effect e) on the first step */
static int hit_begin(gptr o, u32 a, gptr e, int dive)
{
    if (U8(o + 0x56)) return 0;
    U8(o + 0x56)++;
    Anim_Start(o, a, 0, 0x40, 3);
    Effect_Attach(o, e);
    if (dive) Bits_Set(MO_BITS(o), 3, 1);
    return 1;
}

/* hit: flinches */
void DC2CALL Mosa_Flinch(gptr o)                                    /* 0x4416B0 */
{
    if (hit_begin(o, 0xE, 0x65DED8, 0) || U8(o + 0x56) != 1) return;
    if (Anim_Step(o)) U32(o + 0x54) = 1;
    speed(o, 5, 0x50);
}

/* hit hard: flinches longer */
void DC2CALL Mosa_Flinch2(gptr o)                                   /* 0x441720 */
{
    if (hit_begin(o, 0xF, 0x65E01C, 0) || U8(o + 0x56) != 1) return;
    Anim_Step(o);
    if (U16(o + 0x92) >= 0x32) {
        U32(o + 0x54) = 1;
        speed(o, 0xA, 0x64);
    } else speed(o, 0xA, 0x50);
}

/* hit: dives (frame f: then dives) */
static void dive_hit(gptr o, u32 a, int at_end)
{
    if (hit_begin(o, a, 0x65E188, 1) || U8(o + 0x56) != 1) return;
    if (at_end) { if (Anim_Step(o)) U32(o + 0x54) = 0x401; }
    else { Anim_Step(o); if (U16(o + 0x92) > 0x2D) U32(o + 0x54) = 0x401; }
    speed(o, 0x14, 0);
}

void DC2CALL Mosa_HitDive(gptr o)                                   /* 0x4417B0 */
{
    dive_hit(o, 0x10, 1);
}

void DC2CALL Mosa_HitDive2(gptr o)                                  /* 0x4419A0 */
{
    dive_hit(o, 0x11, 0);
}

/* hit: knocked over, rights itself (e2: the effect then, or none) */
static void knock(gptr o, u32 a, gptr e, gptr e2)
{
    switch (U8(o + 0x56)) {
    case 0:
        hit_begin(o, a, e, 0);
        return;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x2D) {
            U8(o + 0x56)++;
            Anim_Start(o, 0x13, 0, 0x40, 0xA);
            if (e2) Effect_Attach(o, e2);
        }
        speed(o, 0x14, 0);
        return;
    case 2:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        speed(o, 2, 0x64);
        return;
    }
}

void DC2CALL Mosa_Knock(gptr o)                                     /* 0x441830 */
{
    knock(o, 0x11, 0x65E2A4, 0x65E5F4);
}

void DC2CALL Mosa_Knock2(gptr o)                                    /* 0x4418F0 */
{
    knock(o, 0x12, 0x65E438, 0);
}

/* dying: rolls over, sinks to the bottom, fades out */
static void dead(gptr o, u32 a0, gptr e0, u32 a1, gptr e1, u32 f1, u32 f2, u32 a2, gptr e2, gptr e3)
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, a0, 0, 0x40, 3);
        Effect_Attach(o, e0);
        Bits_Set(MO_BITS(o), 1, 0);
        U16(o + 0x1EE) = 0x96;
        U16(o + 0x202) = 0;
        U32(o + 0x128) = 0x7211B0;
        return;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, a1, 0, 8, 3);
            Effect_Attach(o, e1);
            U16(o + 0xF2) = (u16)f1;          /* sinking */
        }
        return;
    case 2: {
        U16(o + 0x202)++;
        if (S16(o + 0x202) > 0x78) Obj_DeathFade(o);
        if (Anim_Step(o)) Effect_Attach(o, e1);
        Value_Approach(o + 0xF2, 2, f2);
        s16 f = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
        if (S16(o + 0x42) < f - S16(o + 0x1EE)) return;
        U16(o + 0x42) = (u16)(f - U16(o + 0x1EE));
        U8(o + 0x56)++;
        Anim_Start(o, a2, 0, 0x48, 0xA);
        Effect_Attach(o, e2);
        Se_Play(o, 5);
        return;
    }
    case 3:
        Obj_DeathFade(o);
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            if (e3) Effect_Attach(o, e3);
        }
        return;
    case 4:
        Obj_DeathFade(o);
        return;
    }
}

void DC2CALL Mosa_Dead(gptr o)                                      /* 0x441A20 */
{
    dead(o, 0x14, 0x65E788, 0x17, 0x65EC44, 0x37, 0x4B, 0x18, 0x65ED10, 0);
}

void DC2CALL Mosa_Dead2(gptr o)                                     /* 0x441BD0 */
{
    dead(o, 0x15, 0x65E944, 0x19, 0x65EE7C, 0x44, 0x58, 0x1A, 0x65EF48, 0x65F0DC);
}

/* no attacks for a while when the player is down, dodging, or deep in the
   water of room 704; the time left */
u32 DC2CALL Mosa_Timer(gptr o)                                      /* 0x441D90 */
{
    gptr pl = PL();
    if (U8(G() + 0x1091) == 7 && U8(G() + 0x1090) == 4 && S16(pl + 0x42) <= -3000
        && ((U8(pl + 0x54) == 1 && U8(pl + 0x55) == 9) || Rand8() < 0x40))
        U32(o + 0x1D0) = 0xA;
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D0) = 0x3C;
        if (!U8(G() + 0x108E)) U32(o + 0x1D0) = 0x78;
    }
    return U32(o + 0x1D0);
}

/* an attack begins: it bites, no other one does */
void DC2CALL Mosa_Bite(gptr o)                                      /* 0x441E30 */
{
    U32(o + 0x184) |= 1;
    Bits_Set(MO_BITS(o), 0, 1);
    Bits_Set(MO_BITS(o), 4, 1);
    Bits_Set(SHARED, 0, 1);
}

/* an attack over: lets go */
void DC2CALL Mosa_Release(gptr o)                                   /* 0x441E80 */
{
    gptr b = MO_BITS(o);
    U32(o + 0x184) &= ~1u;
    U32(o + 0x50) |= 8;
    if (Bits_Check(b, 0)) {
        Bits_Set(b, 0, 0);
        Bits_Set(b, 4, 0);
        Bits_Set(SHARED, 0, 0);
    }
}

/* the pitch from 'from' to 0x3E8 above p */
static u32 pitch_to(gptr from, gptr p)
{
    gptr v = Tmp_Push(8);
    U16(v) = U16(p); U16(v + 2) = (u16)(U16(p + 2) - 0x3E8); U16(v + 4) = U16(p + 4);
    Angles_LookAt(from, v, v);
    u32 a = U32(v);
    Tmp_Pop(8);
    return a;
}

/* pitches toward 0x3E8 above p, at most max */
void DC2CALL Mosa_PitchTo(gptr o, u32 max, gptr p)                  /* 0x441EE0 */
{
    u32 a = pitch_to(o + 0x40, p);
    U16(o + 0x48) += (u16)Angle_Approach(U16(o + 0x48), max, (u32)-(s32)a);
}

/* how far its pitch (cur) is off toward 0x3E8 above p, signed */
u32 DC2CALL Mosa_PitchDiff(gptr from, gptr p, u32 cur)              /* 0x441F40 */
{
    u32 d = (u32)-(s32)(pitch_to(from, p) + cur) & 0xFFF;
    return d & 0x800 ? d | 0xFFFFF000 : d;
}
