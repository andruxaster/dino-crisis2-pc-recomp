/* The triceratops (object type handler 0x43CF30, type 9, E70.DAT): the
 * stampede down the long street. It runs along -z toward the player, swerves
 * across x (the street is about +-0x59D8 wide) and charges or gores when the
 * player is near. Kind (o+0x1C4, from o+0x10C): 1 the lead one (health
 * 0x14D5, tinted by the player's distance), 0 the others (0x1F40).
 * Bits at o+0x1C0: 0 holds the bite, 1 moving (its head looks), 3 (not set:
 * the handler pushes it 0x258 along +z each frame), 5 a charge's or gore's
 * frames (o+0x1E8 counts them down), 7 a hit slows it (not knocked), 8 a hit
 * knocks it aside, 9 a knock-down slows it less, 10 it holds back while
 * another one charges, 11 killed, 12 the swerve's debris started.
 * Bits shared at 0x87E974: 0 one bites, 1 one holds back the others, 2 one
 * is down (the herd's last phases), 3 the lead one is dead, 4 the herd is
 * hurt.
 * Fields: +0x1C8..0x1CB the state of the last frame, +0x1CC damage taken
 * lately, +0x1D0 no attacks while set, +0x1E6 sideways speed, +0x1E8 the
 * attack's frames, +0x1EC/+0x1EE waits after a roar or an attack, +0x1F0
 * how it steers (0/1 at the player, 2 weaving, 3 to the street's side, 4
 * along the side wall, 5 to the far side, 6 back), +0x1F2 its phase (by
 * health and the shared bits, Tri_Phase), +0x1F4 the phase whose values
 * are set, +0x1F6 holding back, +0x1F8 the weave's time, +0x1FA the weave's
 * side (0x80/0xFF80), +0x1FC..+0x238 the phase's values, +0x23C the hold
 * after an attack, +0x23E dust timer, +0x240/+0x241 the hit's move and
 * strength, +0x242.. where it heads.
 * States (o+0x54): 0 init, 1 normal (think 0x721120, then move 0x721138 by
 * o+0x55), 2 hit (0x721150), 3 die (0x721160), 4 (0x4A56D0). Attacks land
 * through 0x721170.
 * Moves (o+0x55): 0 run, 1 run after an attack, 2 roar, 3 swerve (o+0x57
 * the side), 4 charge, 5 gore. Hits: 0 the damage, 1 flinch, 2 knocked
 * aside; dying: 1/2 falls, 3 runs off. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define TR_MAIN  0x72110Cu      /* state handlers */
#define TR_THINK 0x721120u      /* state 1: deciding, by o+0x55 */
#define TR_MOVE  0x721138u      /* state 1: moving, by o+0x55 */
#define TR_HIT   0x721150u      /* state 2 */
#define TR_DIE   0x721160u      /* state 3 */
#define TR_GRAB  0x721170u      /* an attack landed, by o+0x55 */
#define TR_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u
#define PL()     U32(G() + 0x940)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void dec16(gptr p) { if (U16(p)) U16(p)--; }
static void face_player(gptr o) { Obj_TurnToPoint(o, 0xA, PL() + 0x40); }
static void speed(gptr o, u32 step, u32 v) { Value_Approach(o + 0xF4, step, v); }

/* a move's first frame: animation a, still sideways */
static void begin(gptr o, u32 a, u32 fl, u32 moving)
{
    U8(o + 0x56)++;
    U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    Anim_Start(o, a, 0, fl, 5);
    Bits_Set(TR_BITS(o), 1, moving);
}

/* weave again, to a random side */
static void weave(gptr o)
{
    U16(o + 0x1FA) = Rand8() & 1 ? 0xFF80 : 0x80;
    U16(o + 0x1E6) = 0;
    U16(o + 0x1F8) = U16(o + 0x1FE);
}

/* an attack's end: run on, weaving */
static void attack_end(gptr o)
{
    Tri_Recover(o);
    S16(o + 0xF4) >>= 1;
    U32(o + 0x54) = 0x101;
    U16(o + 0x1EE) = U16(o + 0x204);
    U16(o + 0x1F0) = 2;
}

void DC2CALL Tri_Handler(gptr o)                                    /* 0x43CF30 */
{
    gptr pl = PL(), b = TR_BITS(o);
    U8(o + 0x24A) = U8(G() + 0x108F) ? 1 : 0;
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    Obj_SavePos(o);
    Orig_Call1(U32(TR_MAIN + U8(o + 0x54) * 4), o);
    if (!Bits_Check(b, 3)) U16(o + 0x44) += 0x258;
    if (U32(o + 0x1C4) == 1) {                      /* the lead one: lit by how near the player is */
        gptr p = U32(o + 0x64);
        u32 d = (u32)SquareRoot0(Dist2_XZ(o + 0x40, pl + 0x40));
        u32 v = (u32)(((unsigned long long)(d << 5) * 0x45E7B273u) >> 45);
        if (v > 0x20) v = 0x20;
        for (; p; p = U32(p + 0x54)) {
            U8(p + 0x8C) = 0x80;
            U8(p + 0x8D) = (u8)(v + 0x60); U8(p + 0x8E) = (u8)(v + 0x60);
        }
    }
    Tri_Phase(o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    Tri_Dust(o);
    gptr hp = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(hp + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x500 && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Tri_Timer(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    dec16(o + 0x1EC); dec16(o + 0x1EE); dec16(o + 0x1F8); dec16(o + 0x23C); dec16(o + 0x1F6);
    if (U16(o + 0x23C) == 1) Bits_Set(SHARED, 1, 0);
    Tri_Grab(o);
    U8(o + 0x1CA) = U8(o + 0x56); U8(o + 0x1C8) = U8(o + 0x54);
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1CB) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

void DC2CALL Tri_Init(gptr o)                                       /* 0x43D1E0 */
{
    U8(o + 0x54)++;
    u32 kind = U16(o + 0x10C);
    U32(o + 0x1C4) = kind;
    U32(o + 0x50) = 0xD;
    U16(o + 0x108) = kind == 1 ? 0x14D5 : 0x1F40;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x7210B0;
    U32(o + 0x128) = 0x7210D8;
    U32(o + 0x134) = 0x7210E8;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x650000);
    U32(o + 0x88) = 0x652DC4;
    Anim_Start(o, 0, 0, 1, 3);
    Obj_Set118(o, 0, 0x1FA0, 0x1FA0);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 0xC;
    U8(o + 0x160) = 8;
    U16(o + 0x164) = 0x15;
    U16(o + 0x166) = 0x20;
    U16(o + 0x16A) = 0;
    U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(TR_BITS(o), 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AFA4) = 0;
    U8(o + 0xFE) = 3;
    U16(o + 0x1F0) = 0;
    U16(o + 0x1F2) = Bits_Check(SHARED, 2) ? 3 : 0;
    U16(o + 0x1F4) = 0xFFFF;
    U8(o + 0x118) |= 0x20;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Tri_Normal(gptr o)                                     /* 0x43D3A0 */
{
    Orig_Call1(U32(TR_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(TR_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Tri_Hit(gptr o)                                        /* 0x43D3D0 */
{
    Orig_Call1(U32(TR_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Tri_Die(gptr o)                                        /* 0x43D3F0 */
{
    Orig_Call1(U32(TR_DIE + U8(o + 0x55) * 4), o);
}

void DC2CALL Tri_Grab(gptr o)                                       /* 0x43D410 */
{
    Enemy_AttackHit(o);
    if (S8(o + 0x119) >= 0) Orig_Call1(U32(TR_GRAB + U8(o + 0x55) * 4), o);
}

/* deciding while running */
void DC2CALL Tri_ThinkRun(gptr o)                                   /* 0x43D440 */
{
    if (U8(o + 0x56) && !Tri_Steer(o)) Tri_Decide(o);
}

/* running down the street, its speed by how it steers */
void DC2CALL Tri_Run(gptr o)                                        /* 0x43D470 */
{
    gptr b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, 0, 1, 1);
        return;
    case 1:
        break;
    default:
        return;
    }
    Anim_Step(o);
    if (Bits_Check(SHARED, 2) && S16(o + 0x44) < S16(o + 0x216)) {
        if (!Bits_Check(b, 10) && Bits_Check(SHARED, 1)) U16(o + 0x1F6) = U16(o + 0x218);
        else { Bits_Set(b, 10, 1); Bits_Set(SHARED, 1, 1); }
    }
    if (U16(o + 0x1F6)) { speed(o, 0x3C, U16(o + 0x22E)); return; }
    switch (U16(o + 0x1F0)) {
    case 1: speed(o, 0x14, U16(o + 0x21C)); return;
    case 2: speed(o, 0x14, U16(o + 0x21E)); return;
    case 3: speed(o, 0x14, U16(o + 0x220)); return;
    case 5: speed(o, 0x14, S16(o + 0x44) > S16(o + 0x212) ? U16(o + 0x222) : 0); return;
    default: speed(o, 0x14, U16(o + 0x224)); return;
    }
}

/* deciding after an attack: weave */
void DC2CALL Tri_ThinkAfter(gptr o)                                 /* 0x43D650 */
{
    U16(o + 0x1F0) = 2;
    if (U8(o + 0x56)) Tri_Steer(o);
}

/* running on after an attack until its wait is over */
void DC2CALL Tri_After(gptr o)                                      /* 0x43D670 */
{
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, 0, 0x41, 1);
        return;
    case 1:
        break;
    default:
        return;
    }
    Anim_Step(o);
    if (!U16(o + 0x1EE)) {
        U32(o + 0x54) = 1;
        if (U32(o + 0x1C4) && Rand8() >= 0x40 && U16(o + 0x1F2) != 5) {
            U16(o + 0x1F0) = 4;
            Bits_Set(TR_BITS(o), 0xC, 0);
        } else {
            U16(o + 0x1F0) = 2;
            weave(o);
        }
    }
    speed(o, 0x14, U16(o + 0x228));
}

/* steering across the street; 1 when it swerved at the player */
u32 DC2CALL Tri_Steer(gptr o)                                       /* 0x43D780 */
{
    gptr pl = PL(), pos = o + 0x40, pp = pl + 0x40;
    s16 a = (s16)Angle_DiffTo(pos, pp, U16(o + 0x4A));
    Dist2_XZ(pos, pp);
    s32 x;
    switch (U16(o + 0x1F0)) {
    case 0: case 1:                                 /* at the player */
        U16(o + 0x1FA) = 0;
        U16(o + 0x242) = U16(pp); U16(o + 0x244) = 0; U16(o + 0x246) = 0x86E8;
        if (iabs(S16(pos)) > S16(o + 0x236) >> 1 && U16(o + 0x1F2)) U16(o + 0x1F0) = 3;
        break;
    case 2:                                         /* weaving */
        if (!U16(o + 0x1F8)) {
            U16(o + 0x1F8) = U16(o + 0x1FE);
            if (U32(o + 0x1C4) == 1 && !Bits_Check(SHARED, 3))
                U16(o + 0x1FA) = S16(U32(0x87E760) + 0x2A0) < 0 ? 0xFF80 : 0x80;
            else
                U16(o + 0x1FA) = S16(o + 0x1FA) <= 0 ? 0x80 : 0xFF80;
        }
        x = S16(pos);
        if (x <= -(s32)S16(o + 0x202)) U16(o + 0x1FA) = 0xFF80;
        if (x >= S16(o + 0x202)) U16(o + 0x1FA) = 0x80;
        if (S16(o + 0x44) < S16(o + 0x200) && U8(o + 0x55) != 1) {
            if ((u16)Rand8() < U16(o + 0x20C)) { U16(o + 0x1F0) = 3; return 0; }
            if ((u16)Rand8() < U16(o + 0x20A)) { U16(o + 0x1F0) = 0; return 0; }
        }
        if (S16(o + 0x44) < -10000 && U8(o + 0x55) != 1) {
            U16(o + 0x1F0) = iabs(S16(pos)) <= S16(o + 0x236) >> 1 ? 0 : 3;
            return 0;
        }
        break;
    case 3:                                         /* to the side it is on */
        U16(o + 0x1FA) = 0;
        U16(o + 0x242) = S16(pos) < 0 ? (u16)-U16(o + 0x236) : U16(o + 0x236);
        U16(o + 0x244) = 0; U16(o + 0x246) = 0x86E8;
        break;
    case 4:                                         /* along the side wall, knocking debris off */
        x = S16(pos);
        U16(o + 0x1FA) = x > 0 ? 0xFF80 : 0x80;
        if (iabs(x) > 0x1F40 && iabs(x) < 0x2EE0) {
            Tri_Debris(o, (o & ~0xFFu) | (x > 0));
            if (!Bits_Check(TR_BITS(o), 0xC)) {
                Bits_Set(TR_BITS(o), 0xC, 1);
                Se_Play(o, 0x62);
            }
        }
        if (iabs(S16(pos)) > 0x59D8) U16(o + 0x1F0) = 5;
        break;
    case 5:                                         /* over to the far side */
        U16(o + 0x1FA) = 0;
        U16(o + 0x242) = S16(pos) < 0 ? 0xB1E0 : 0x4E20;
        U16(o + 0x244) = 0; U16(o + 0x246) = 0x86E8;
        if (S16(o + 0x44) < S16(o + 0x210) && S16(o + 0x44) > S16(o + 0x212) && (u16)Rand8() < U16(o + 0x20E))
            U16(o + 0x1F0) = 6;
        break;
    case 6:                                         /* back across, swerving at the player */
        x = S16(pos);
        U16(o + 0x1FA) = x < 0 ? 0xFF80 : 0x80;
        if (S16(o + 0x44) < S16(o + 0x214)) U16(o + 0x44) = U16(o + 0x214);
        if (iabs(x) < 0x32C8) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = (u32)(a <= 0) << 24 | 0x301;
            return 1;
        }
        break;
    }
    if (!U16(o + 0x1FA)) Value_Approach(o + 0x1E6, 0x28, 0);
    Value_Approach(o + 0x1E6, 0x28, S16(o + 0x1FA) > 0 ? (u16)-U16(o + 0x1FC) : U16(o + 0x1FC));
    U16(pos) += U16(o + 0x1E6);
    if (U16(o + 0x1FA)) U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0xA, U16(o + 0x1FA) + 0x800);
    else Obj_TurnToPoint(o, 0xA, o + 0x242);
    return 0;
}

/* an attack when the player is near enough; 1 when it starts one */
u32 DC2CALL Tri_Decide(gptr o)                                      /* 0x43DBE0 */
{
    if (U16(o + 0x23C) || Bits_Check(SHARED, 0) || U32(o + 0x1D0)) return 0;
    u32 f = U16(o + 0x1F0);
    if (f <= 2) {
        s16 z = S16(o + 0x44);
        if (z < 0x32C8 && z > 0x2710 && !U16(o + 0x1EC) && f != 1 && (u16)Rand8() < U16(o + 0x208)) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = 0x201;
            return 1;
        }
        if (S16(o + 0x44) < S16(o + (f == 1 ? 0x232 : 0x230))) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = 0x401;
            return 1;
        }
        return 0;
    }
    if (f == 3 && S16(o + 0x44) < S16(o + 0x234)) {
        Enemy_BiteOn(o);
        U32(o + 0x54) = S16(o + 0x40) > 0 ? 0x501 : 0x1000501;
        return 1;
    }
    return 0;
}

void DC2CALL Tri_Roar(gptr o)                                       /* 0x43DD00 */
{
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, 1, 0x40, 0);
        U16(o + 0x1EC) = U16(o + 0x206);
        return;
    case 1:
        if (Anim_Step(o)) {
            Tri_Recover(o);
            U32(o + 0x54) = 1;
            U16(o + 0x1F0) = 1;
        }
        speed(o, 0x14, U16(o + 0x226));
        return;
    }
}

/* swerving at the player, with the animation's own motion */
void DC2CALL Tri_Swerve(gptr o)                                     /* 0x43DDB0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, U8(o + 0x57) ? 5 : 4, 0x40, 0);
        return;
    case 1:
        break;
    default:
        return;
    }
    u16 fr = U16(o + 0x92);
    if (fr > 0xA && fr < 0x19) Tri_Debris(o, (o & ~0xFFu) | (S16(o + 0x40) > 0));
    if (U16(o + 0x92) == 0xA) Se_Play(o, 0x62);
    if (Anim_Step(o)) {
        Tri_Recover(o);
        U32(o + 0x54) = 1;
        U16(o + 0x1F0) = 2;
        weave(o);
    }
    speed(o, 0x14, U16(o + 0x224));
    gptr sv = Tmp_Push(0x28), m = sv + 8;
    RotMatrix(o + 0x48, m);
    ApplyMatrixSV(m, o + 0xD0, sv);
    U16(o + 0x40) += (u16)(U16(sv) * 2);
    U16(o + 0x42) += (u16)(U16(sv + 2) * 2);
    U16(o + 0x44) += (u16)(U16(sv + 4) * 2);
    Tmp_Pop(0x28);
}

/* deciding in an attack: keep facing the player */
void DC2CALL Tri_Face(gptr o)                                       /* 0x43DF30 */
{
    face_player(o);
}

/* the charge (or the gore, from the side o+0x57) and on, weaving */
static void attack(gptr o, u32 anim, u32 frames, u32 fast, int side_weave)
{
    gptr b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, anim, 0x40, 0);
        Bits_Set(b, 5, 1);
        U32(o + 0x1E8) = frames;
        return;
    case 1:
        break;
    default:
        return;
    }
    if (Anim_Step(o)) {
        attack_end(o);
        if (side_weave) U16(o + 0x1FA) = S16(o + 0x40) < 0 ? 0xFF80 : 0x80;
        else U16(o + 0x1FA) = Rand8() & 1 ? 0xFF80 : 0x80;
        U16(o + 0x1E6) = 0;
        U16(o + 0x1F8) = U16(o + 0x1FE);
    }
    if (Bits_Check(b, 5) && U32(o + 0x1E8)) U32(o + 0x1E8)--;
    speed(o, 0x28, U16(o + fast));
}

void DC2CALL Tri_Charge(gptr o)                                     /* 0x43DF50 */
{
    attack(o, 3, 0x16, 0x224, 0);
}

void DC2CALL Tri_Gore(gptr o)                                       /* 0x43E070 */
{
    attack(o, U8(o + 0x57) ? 5 : 4, 0x10, 0x220, 1);
}

/* the charge or the gore landed */
static void attack_hit(gptr o, gptr flash)
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    Player_Hurt(o, 0x96, 1, 0);
    Flash_Play(o, flash);
    Bits_Set(TR_BITS(o), 5, 0);
    U8(pl + 0x17B) = 0xA;
}

void DC2CALL Tri_ChargeHit(gptr o)                                  /* 0x43E1B0 */
{
    attack_hit(o, 0x65EC60);
}

void DC2CALL Tri_GoreHit(gptr o)                                    /* 0x43E200 */
{
    attack_hit(o, 0x65EC70);
}

/* the hit's damage: how it reacts, its health, the score */
void DC2CALL Tri_Damage(gptr o)                                     /* 0x43E250 */
{
    static const s8 thr[7] = {0xC, 8, 5, 4, 2, 1, 0};           /* the attack's frames left, for each strength */
    static const u16 dmg[7] = {0x190, 0x258, 0x352, 0x44C, 0x578, 0x7D0, 0x7D0};
    static const u16 score[7] = {0xC8, 0x190, 0x258, 0x384, 0x4B0, 0x5DC, 0x5DC};
    gptr pl = PL(), b = TR_BITS(o);
    U16(o + 0x176) = 0xC8;
    Bits_Set(b, 7, 1);
    U32(o + 0x1CC) += U16(o + 0x176);
    U32(o + 0x54) = 0x102;
    u32 d = Dist2_XZ(o + 0x40, pl + 0x40);
    if ((d < 0x5F5E100 && U32(o + 0x1CC) > U32(o + 0x238)) || d < 0x3D09000) {
        U32(o + 0x1CC) = 0;
        Bits_Set(b, 7, 0);
    }
    if (U8(o + 0x1C8) == 2 && U8(o + 0x1C9) == 2) {           /* hit again while flinching */
        Bits_Set(b, 7, 0);
        U32(o + 0x54) = 0x102;
        Score_Hit(o, score[S8(o + 0x241)]);
        U16(o + 0x176) = (u16)((s16)dmg[S8(o + 0x241)] >> 1);
    }
    if (U8(o + 0x184) & 8) {                        /* in its attack: the strength by the frames left */
        U8(o + 0x241) = 0;
        for (;;) {
            s8 i = S8(o + 0x241);
            if (S32(o + 0x1E8) >= thr[i]) { U16(o + 0x176) = dmg[i]; break; }
            U8(o + 0x241) = (u8)++i;
            if (i > 6) break;                       /* (none: the strength stays 7) */
        }
        U32(o + 0x54) = 0x202;
    }
    Bits_Set(b, 8, 0);
    if (U16(o + 0x1F2) == 5) {
        Bits_Set(b, 8, 1);
        Bits_Set(b, 7, 0);
        U32(o + 0x54) = 0x102;
    }
    Bits_Set(b, 9, 0);
    if (U16(o + 0x1F2) == 2) {
        Bits_Set(b, 9, 1);
        Bits_Set(b, 7, 1);
    }
    if (Bits_Check(b, 0xB))
        U32(o + 0x54) = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Tri_Recover(o);
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; U16(o + 0x1E6) = 0;
        Bits_Set(b, 1, 0);
        Bits_Set(b, 5, 0);
        if (U8(o + 0x184) & 8) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 0xA;
        if (S16(o + 0x108) < 0) {                   /* killed */
            Bits_Set(b, 0xB, 1);
            if (!Bits_Check(SHARED, 2)) U32(o + 0x54) = 0x303;
            else {
                U32(o + 0x50) &= ~8u;
                U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
                if (!Bits_Check(SHARED, 3)) {
                    gptr c = U32(0x87E760);
                    Bits_Set(SHARED, 3, 1);
                    if (o == c + 0x260) {           /* the lead one: the other loses health */
                        s32 h = S16(c + 0x5CA) / 3;
                        if (S16(c + 0x5C8) > h) {
                            U16(c + 0x5C8) = (u16)h;
                            Score_Hit(o, 0xBB8);
                            goto next;
                        }
                    } else {
                        s32 h = S16(c + 0x36A) / 3;
                        if (S16(c + 0x368) > h) U16(c + 0x368) = (u16)h;
                    }
                }
                Score_Hit(o, 0xBB8);
            }
        } else if (U8(o + 0x184) & 8) Score_Hit(o, score[S8(o + 0x241)]);
    } else {
        U16(o + 0x108) += (u16)((s32)U16(o + 0x176) / -5);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 0xA;
        if (S16(o + 0x108) <= 0) U16(o + 0x108) = 1;
    }
next:
    Orig_Call1(U32(TR_MAIN + U8(o + 0x54) * 4), o);
}

/* hit: flinch, slowed or knocked aside */
void DC2CALL Tri_Flinch(gptr o)                                     /* 0x43E6A0 */
{
    gptr b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        Anim_Start(o, 6, 0, 0x40, 2);
        u16 v = 0;
        if (Bits_Check(b, 7)) v = U16(o + 0xF4);
        if (Bits_Check(b, 8)) v = (u16)((S16(o + 0xF4) * 2) / 3);
        U16(o + 0xF4) = v;
        U8(o + 0x240) = U8(o + 0x1C9);
        break;
    }
    case 1: {
        if (Anim_Step(o) || (U16(o + 0x92) >= 2 && Bits_Check(b, 7)) || (U16(o + 0x92) >= 0xF && Bits_Check(b, 8))) {
            U32(o + 0x54) = U32(o + 0x1C4) == 1 ? 0x101 : 1;
            U16(o + 0x1F0) = 2;
            weave(o);
        }
        u16 v = U16(o + 0x22A);
        if (Bits_Check(b, 7)) v = U16(o + 0x224);
        if (Bits_Check(b, 8)) v = U16(o + 0x224);
        speed(o, 0x14, v);
        break;
    }
    }
    if (Bits_Check(b, 8)) {
        if (S16(o + 0x1FA) > 0) U16(o + 0x40) -= 0xC8;
        else U16(o + 0x40) += 0xC8;
    }
    face_player(o);
}

/* hit: knocked down */
void DC2CALL Tri_Knock(gptr o)                                      /* 0x43E880 */
{
    gptr b = TR_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 7, 0, 0x40, 2);
        U16(o + 0xF4) = Bits_Check(b, 9) ? (u16)(S16(o + 0xF4) / 6) : 0;
        break;
    case 1: {
        s32 lim = Bits_Check(b, 9) ? 0x23 : 0x2D;
        Anim_Step(o);
        if ((s32)U16(o + 0x92) >= lim) {
            S16(o + 0xF4) >>= 1;
            U32(o + 0x54) = 0x101;
            U16(o + 0x1EE) = 0;
            U16(o + 0x1F0) = 2;
            weave(o);
        }
        speed(o, 0x14, U16(o + 0x22C));
        break;
    }
    }
    if (S16(o + 0x40) > 0) U16(o + 0x40) -= 0x32;
    else U16(o + 0x40) += 0x32;
    face_player(o);
}

/* dying: falls, and is gone when it slid off the street's end */
void DC2CALL Tri_Dead(gptr o)                                       /* 0x43E9D0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 8, 0, 0x40, 0x14);
        Bits_Set(TR_BITS(o), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        break;
    case 2:
        if (S16(o + 0x44) > 0x59D8) U32(o + 0x50) |= 0x40000000;
        break;
    }
    face_player(o);
}

/* dying while the herd is down: staggers, runs off the street's end */
void DC2CALL Tri_DeadRun(gptr o)                                    /* 0x43EAB0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 7, 0, 0x40, 2);
        Bits_Set(TR_BITS(o), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0, 0, 0x41, 3);
        }
        break;
    case 2:
        Anim_Step(o);
        if (S16(o + 0x44) > 0x59D8) {
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
            U32(o + 0x50) |= 0x40000000;
            Bits_Set(SHARED, 2, 1);
        }
        speed(o, 0x14, 0xFED4);
        break;
    }
    face_player(o);
}

/* no attacks for a while when the player is down or dodging; the time left */
u32 DC2CALL Tri_Timer(gptr o)                                       /* 0x43EBD0 */
{
    gptr pl = PL();
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D0) = 0xA;
        if (!U8(G() + 0x108E)) U32(o + 0x1D0) = 0x14;
    }
    return U32(o + 0x1D0);
}

/* an attack over: lets go of the player and the others */
void DC2CALL Tri_Recover(gptr o)                                    /* 0x43EC40 */
{
    gptr b = TR_BITS(o);
    U32(o + 0x184) &= ~1u;
    U32(o + 0x50) |= 8;
    if (Bits_Check(b, 0)) {
        Bits_Set(b, 0, 0);
        Bits_Set(SHARED, 0, 0);
    }
    if (Bits_Check(b, 0xA)) {
        Bits_Set(b, 0xA, 0);
        U16(o + 0x23C) = U16(o + 0x21A);
    }
}

/* dust at its feet every fourth frame, darker the nearer the player */
void DC2CALL Tri_Dust(gptr o)                                       /* 0x43ECC0 */
{
    gptr pl = PL();
    u32 d = (u32)SquareRoot0(Dist2_XZ(o + 0x40, pl + 0x40));
    u32 v = (u32)(((unsigned long long)(d << 7) * 0x45E7B273u) >> 45);
    if (v > 0x80) v = 0x80;
    U16(o + 0x23E)++;
    if (S16(o + 0x23E) < 4) return;
    U16(o + 0x23E) = 0;
    gptr k = K2_AllocBack();
    if (!k) return;
    U8(k + 0x58) = 3;
    U8(k + 0x7A) = 0;
    U16(k + 0x7C) = 0; U16(k + 0x7E) = 0; U16(k + 0x80) = 0xC8;
    U16(k + 0x6C) = (u16)(Rand8() % 50 + 50);
    u16 h = (u16)(Rand8() % 50 + 50);
    U8(k + 0x64) = U8(k + 0x65) = U8(k + 0x66) = (u8)(0xA0 - v);
    U16(k + 0x6E) = h;
    U16(k + 0x40) = (u16)(Rand8() + U16(o + 0x40));
    U16(k + 0x42) = (u16)(Rand8() + U16(o + 0x42));
    U16(k + 0x44) = (u16)(Rand8() + U16(o + 0x44));
}

/* the phase by its health (and the herd's), and its values for that phase */
void DC2CALL Tri_Phase(gptr o)                                      /* 0x43EDD0 */
{
    /* by phase: 0..2 the herd running (health above 3/4, 1/3, below), 3..5
       the last ones (one down; the herd hurt; the lead one dead) */
    static const s16 tab[30][6] = {
        {0x32, 0x3C, 0x3C, 0x3C, 0x46, 0x46},                   /* +0x1FC sideways speed */
        {0xBB8, -0x1770, -0x2710, -0x1770, -0x1B58, -0x2710},   /* +0x200 deciding from this z */
        {0xFA0, 0xFA0, 0x7D0, 0xFA0, 0xFA0, 0x7D0},             /* +0x202 the weave's width */
        {0x3C, 0x3C, 0x50, 0x5A, 0x5A, 0x1E},                   /* +0x204 the wait after an attack */
        {0x258, 0x258, 0x258, 0x258, 0x258, 0x258},             /* +0x206 the wait after a roar */
        {0x10, 0x18, 0x20, 0x18, 0x20, 0x20},                   /* +0x208 the roar's chance */
        {0xFF, 0x80, 0x80, 0x80, 0x40, 0x80},                   /* +0x20A ... at the player */
        {0, 0x80, 0, 0x80, 0xFF, 0},                            /* +0x20C ... to the side */
        {1, 1, 1, 0x80, 0xFF, 0xFF},                            /* +0x20E ... back across */
        {1, 1, 1, 0x3E8, -0x3E8, -0x3E8},                       /* +0x210 */
        {1, 1, 1, -0xFA0, -0x1388, -0x1388},                    /* +0x212 */
        {1, 1, 1, -0xFA0, -0x1388, -0x1388},                    /* +0x214 */
        {1, 1, 1, -0x7D0, -0xFA0, -0xFA0},                      /* +0x216 holding back from this z */
        {1, 1, 1, 0x14, 0x14, 0x14},                            /* +0x218 the hold */
        {4, 4, 4, 0xA, 0xA, 4},                                 /* +0x21A the hold after an attack */
        {0x190, 0x258, 0x2BC, 0x1F4, 0x2BC, 0x2BC},             /* +0x21C.. speeds (+0x258) */
        {0x12C, 0x1F4, 0x2BC, 0x190, 0x258, 0x2BC},
        {0x1F4, 0x258, 0x2BC, 0x258, 0x320, 0x2BC},
        {1, 1, 1, 0x258, 0x320, 0x320},
        {0xC8, 0x12C, 0x2BC, 0x190, 0x190, 0x2BC},
        {0xC8, 0xC8, 0x2BC, 0xC8, 0x190, 0x2BC},
        {-0xC8, -0xC8, -0xC8, -0xC8, -0xC8, -0xC8},
        {-0x64, -0x64, -0x1F4, -0x64, -0x64, -0x1F4},
        {-0x64, -0x64, -0x64, -0x64, -0x64, -0x64},
        {1, 1, 1, -0xC8, -0xC8, -0xC8},
        {-0x2710, -0x2710, -0x1F40, -0x2AF8, -0x2AF8, -0x1F40}, /* +0x230 charging from this z */
        {-0x2328, -0x2328, -0x1F40, -0x2710, -0x2710, -0x1F40}, /* +0x232 ... after an attack */
        {-0x2EE0, -0x2EE0, -0x1F40, -0x2EE0, -0x32C8, -0x1F40}, /* +0x234 goring from this z */
        {0x7D0, 0xFA0, 0xBB8, 0xFA0, 0xFA0, 0xBB8},             /* +0x236 the street's half width to keep */
        {0x1E, 0x3C, 0x20, 0x3C, 0x1E, 0x20},                   /* +0x1FE the weave's time, at least */
    };
    static const u32 damage[6] = {0x2BC, 0x2BC, 0x44C, 0x12C, 0x1F4, 0x44C};  /* +0x238 enough to flinch */
    u16 ph;
    if (!Bits_Check(SHARED, 2)) {
        ph = 0;
        if (S16(o + 0x108) < S16(o + 0x10A) * 3 / 4) ph = 1;
        if (S16(o + 0x108) < S16(o + 0x10A) / 3) ph = 2;
    } else {
        ph = 3;
        if (S16(o + 0x108) < S16(o + 0x10A) * 2 / 3) Bits_Set(SHARED, 4, 1);
        if (Bits_Check(SHARED, 4)) ph = 4;
        if (Bits_Check(SHARED, 3)) ph = 5;
    }
    U16(o + 0x1F2) = ph;
    if (ph != U16(o + 0x1F4)) {
        U16(o + 0x1FC) = (u16)tab[0][ph];
        for (int i = 1; i < 29; i++)                /* +0x200..+0x236 */
            U16(o + 0x200 + 2 * (i - 1)) = (u16)(tab[i][ph] + (i >= 15 && i < 25 ? 0x258 : 0));
        U32(o + 0x238) = damage[ph];
    }
    U16(o + 0x1FE) = (u16)(Rand8() % 30 + tab[29][ph]);
    U16(o + 0x1F4) = U16(o + 0x1F2);
}

/* debris off the side wall, flying away from it (side: o is on +x) */
void DC2CALL Tri_Debris(gptr o, u32 side)                           /* 0x43F6D0 */
{
    gptr k = K2_AllocBack();
    if (!k) return;
    U8(k + 0x58) = 0x14;
    U8(k + 0x7A) = (u8)((Rand8() & 7) + 5);
    U16(k + 0x42) = (u16)((s8)Rand8() * 4 - 0x7D0);
    U16(k + 0x44) = (u16)((s8)Rand8() * 4 - 0x7D0 + U16(o + 0x44));
    if (!(u8)side) {
        U16(k + 0x40) = (u16)(U16(o + 0x40) + 0x3E8);
        U16(k + 0x7C) = (u16)(Rand8() * 4 + 0xC8);
    } else {
        U16(k + 0x40) = (u16)(U16(o + 0x40) - 0x3E8);
        U16(k + 0x7C) = (u16)((-0x32 - (s32)Rand8()) * 4);
    }
    U16(k + 0x7E) = (u16)(Rand8() * 4 - 0x320);
    U16(k + 0x80) = (u16)(((s32)Rand8() - 0x20D) * 5);
    U16(k + 0x6E) = (u16)(Rand8() % 14 + 0x20);
    u16 w = (u16)(Rand8() % 14 + 0x20);
    U16(k + 0x9A) = 0x18; U16(k + 0x98) = 0x3C; U16(k + 0x9C) = 0x80; U16(k + 0x9E) = 0x3C;
    U16(k + 0x6C) = w;
    U16(k + 0xA0) = (u16)((s32)(Rand8() * 4) % 0x258 - 0x12C);
}
