/* The inostrancevia (object type handler 0x437230, type 14, E50.DAT). It
 * keeps a set of bits at o+0x1C0 (Bits_Set(o+0x1C0, n)): 0 holds the bite,
 * 1 moving (its head looks), 2 down (hits knock it over, not back), 3 the
 * grab's hold frames (hits do a quarter), 4 the roar's or a swing's frames
 * (a hit stops it), 5 enough damage while down to stay down, 6 killed by a
 * hit (its fall then is not scored again).
 * Fields: +0x1C8..0x1CB the state of the last frame, +0x1CC/+0x1D0 counters
 * (+0x1D0 damage taken while down), +0x1D4 no close attacks while set,
 * +0x1D8 the player's struggle while eaten, +0x1EC.. the last place on the
 * floor, +0x1F4.. where it fell from, +0x1FC.. its fall's step, +0x202/
 * +0x203 the state before the fall, +0x204 its turn so far, +0x206 the
 * turn's step, +0x208 the wait after a roar, +0x20A after a turn, +0x20C
 * after a flinch, +0x20E lying down, +0x210 walking, +0x212 between hit
 * marks, +0x214 the fall's push, +0x216 the player is in the extra costume
 * (the other set of player animations, Ino_Side).
 * Bits shared by the inostranceviae at 0x87E974: 0 one bites, 1 one is
 * dying, 2 one is knocked over.
 * States (o+0x54): 0 init, 1 normal (think 0x720E94, then move 0x720EB8 by
 * o+0x55), 2 hit (0x720EDC), 3 die (0x720F00), 4 (0x4A56D0). Attacks land
 * through 0x720F24.
 * Moves (o+0x55): 0 wait, 1 walk, 2 roar, 3 turn (o+0x57 the side), 4 fall
 * off a ledge, 5/6 bites, 7 seize and eat the player, 8 pounce on the
 * player. Hits: 0 the damage, 1 flinch, 2 knocked over, 3 stagger, 4 down,
 * 5..8 hit while down; dying: 1..3 falls, 4..7 falls from the side, 8
 * burnt. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define IN_MAIN  0x720E80u      /* state handlers */
#define IN_THINK 0x720E94u      /* state 1: deciding, by o+0x55 */
#define IN_MOVE  0x720EB8u      /* state 1: moving, by o+0x55 */
#define IN_HIT   0x720EDCu      /* state 2 */
#define IN_DIE   0x720F00u      /* state 3 */
#define IN_GRAB  0x720F24u      /* an attack landed, by o+0x55 */
#define IN_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u
#define PL()     U32(G() + 0x940)
#define DIST(o)  U32((o) + 0x13C)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static void face(gptr o, u32 sp) { Obj_TurnToPoint(o, sp, o + 0xD8); }
static void dec16(gptr p) { if (U16(p)) U16(p)--; }

/* the player placed on o */
static void place_player(gptr o, gptr pl)
{
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
}

/* o moved by the player's root motion, the player kept on it */
static void carry_player(gptr o, gptr pl)
{
    U16(o + 0x40) += U16(pl + 0xE8); U16(o + 0x42) += U16(pl + 0xEA); U16(o + 0x44) += U16(pl + 0xEC);
    place_player(o, pl);
}

/* the player's animation from o's set (+0x7C.. or, in the extra costume, +0x8C..) */
static gptr pl_anim(gptr o, u32 off) { return U32(U32(o + 0x88) + Ino_Side(o) * 4 + off); }

/* the player taken into o's jaws */
static void seize_player(gptr o, gptr pl)
{
    place_player(o, pl);
    Obj_SyncLinks(pl);
    Obj_UpdateMatrix(pl);
    Obj_CollideWalls(pl);
}

/* the player and its root part turned by d */
static void turn_player(gptr pl, s32 d)
{
    gptr p = U32(pl + 0x64);
    U16(p + 0x4A) = (U16(p + 0x4A) + d) & 0xFFF;
    U16(pl + 0x4A) = (U16(pl + 0x4A) + d) & 0xFFF;
}

/* the player's health less d; at none the game is over */
static void hurt_player(gptr pl, u32 d)
{
    U16(pl + 0x108) -= Damage_Scale(d);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
    Player_Bleed(0x1F4);
}

/* a move's start: the next step, still, an animation, the head looking or not */
static void begin(gptr o, u32 anim, u32 fl, u32 blend, u32 look)
{
    U8(o + 0x56)++;
    stop(o);
    Anim_Start(o, anim, 0, fl, blend);
    Bits_Set(IN_BITS(o), 1, look);
}

/* ---- the handler ---- */

void DC2CALL Ino_Handler(gptr o)                                    /* 0x437230 */
{
    gptr pl = PL(), b = IN_BITS(o);
    U8(o + 0x216) = U8(G() + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    if (U8(G() + 0x1091) == 8 && U8(G() + 0x1090) == 0xB && Flag_Check(7, 0x1F))
        U8(o + 0x17B) = 4;
    if (((U8(o + 0x50) & 8) || U8(o + 0x54) == 3) && !(U8(o + 0x54) == 1 && U8(o + 0x55) == 4)
        && (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0) > S16(o + 0x42)) {
        U32(o + 0x50) &= ~8u;
        U8(o + 0x203) = U8(o + 0x55);
        U8(o + 0x202) = U8(o + 0x54);
        U32(o + 0x54) = 0x401;
    }
    u8 st = U8(o + 0x54);
    if (!(st == 1 && U8(o + 0x55) == 4)) {
        U16(o + 0x1EC) = U16(o + 0x40); U16(o + 0x1EE) = U16(o + 0x42); U16(o + 0x1F0) = U16(o + 0x44);
    }
    Orig_Call1(U32(IN_MAIN + st * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x500 && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    u32 f = U32(o + 0x50);
    U32(o + 0x50) = f | 0x1000;
    if (!(f & 0x10)) Obj_CollideWalls(o);
    U32(o + 0x50) &= ~0x1000u;
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Ptera_Timer(o);
    if (U32(o + 0x1CC)) U32(o + 0x1CC)--;
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U8(o + 0x18D)) U8(o + 0x18D)--;
    dec16(o + 0x208); dec16(o + 0x20A); dec16(o + 0x20C); dec16(o + 0x20E); dec16(o + 0x212);
    Ino_Grab(o);
    if (!Pos_Hidden(o + 0x40) && !U8(0x87E747) && DIST(o) > 0x2710) {
        U16(pl + 0x1E8) = 0;
        Enemy_BiteOff(o);
        Enemy_Remove(o);
    }
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1C8) = U8(o + 0x54);
    U8(o + 0x1CA) = U8(o + 0x56); U8(o + 0x1CB) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) {
        Obj_Disable(o);
        Bits_Set(SHARED, 1, 0);
    }
}

/* state 0: health by difficulty, model, animations */
void DC2CALL Ino_Init(gptr o)                                       /* 0x437540 */
{
    U32(o + 0x50) = 0xD;
    U8(o + 0x54)++;
    if (U8(G() + 0x927)) U32(o + 0x50) = 0xB;
    U16(o + 0x108) = 0x8FC;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x11F8;
    if (U8(G() + 0x108E) == 0) U16(o + 0x108) = (u16)(S16(o + 0x108) >> 1);
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720E30;
    U32(o + 0x128) = 0x720E58;
    U32(o + 0x134) = 0x720E68;
    stop(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x642350;
    Anim_Start(o, 0, 0, 1, 5);
    Obj_Set118(o, 1, 0x1FA0, 0x3AF8);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 3;
    U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x60;
    U16(o + 0x166) = 0x80;
    U16(o + 0x16A) = 0;
    U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(IN_BITS(o), 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF9C) = 0;
    U8(o + 0x118) |= 0x20;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Ino_Normal(gptr o)                                     /* 0x4376D0 */
{
    Bits_Set(IN_BITS(o), 4, 0);
    Orig_Call1(U32(IN_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(IN_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ino_Hit(gptr o)                                        /* 0x437710 */
{
    Orig_Call1(U32(IN_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Ino_Die(gptr o)                                        /* 0x437730 */
{
    Orig_Call1(U32(IN_DIE + U8(o + 0x55) * 4), o);
}

/* an attack landing: the move's hit */
void DC2CALL Ino_Grab(gptr o)                                       /* 0x437750 */
{
    Enemy_AttackHit(o);
    if (S8(o + 0x119) >= 0) Orig_Call1(U32(IN_GRAB + U8(o + 0x55) * 4), o);
}

/* 0 when the player's costume is the normal one, else 4 (the other half of
 * o's set of player animations) */
u32 DC2CALL Ino_Side(gptr o)                                        /* 0x4397A0 */
{
    return U8(o + 0x216) ? 4 : 0;
}

/* ---- deciding ---- */

/* an attack, a roar or a turn by distance and angle: 1 when one began */
u32 DC2CALL Ino_Decide(gptr o)                                      /* 0x437960 */
{
    gptr pl = PL();
    s16 a = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
    s16 pd = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    s32 aa = iabs(a);
    if (!U8(o + 0x13A) && aa < 0x100) {
        if (!Bits_Check(SHARED, 0)) {
            if (DIST(o) < 0x1770 && DIST(o) > 0xFA0 && Rand8() < 0x10 && !U16(o + 0x208)) {
                U32(o + 0x54) = 0x201; return 1;
            }
            if (!U32(o + 0x1D4)) {
                if (DIST(o) < 0xBB8) {
                    if (Rand8() < 0x80) { Enemy_BiteOn(o); U32(o + 0x54) = 0x701; return 1; }
                    if (DIST(o) < 0xBB8 && Rand8() < 0x80) { Enemy_BiteOn(o); U32(o + 0x54) = 0x501; return 1; }
                }
                if (DIST(o) > 0x157C && DIST(o) < 0x1964 && aa < 0x40) {
                    Enemy_BiteOn(o); U32(o + 0x54) = 0x601; return 1;
                }
            }
            if (U8(pl + 0x174) & 2) {
                if (DIST(o) > 0xDAC && DIST(o) < 0xFA0 && !Flag_Check(2, 0x1A) && iabs(pd) < 0x60) {
                    Enemy_BiteOn(o); U32(o + 0x54) = 0x801; return 1;
                }
                if (DIST(o) < 0x1770 && Rand8() < 0x40 && !U16(o + 0x208)) {
                    U32(o + 0x54) = 0x201; return 1;
                }
            }
        } else if (DIST(o) < 0x1770 && Rand8() < 0x80 && !U16(o + 0x208)) {
            U32(o + 0x54) = 0x201; return 1;
        }
    }
    if (U16(o + 0x20A)) return 0;
    s8 side = -1;
    if (a < -0x400) side = 1;
    if (a > 0x400) side = 0;
    if (side != -1) { U32(o + 0x54) = ((u32)(s32)side << 24) | 0x301; return 1; }
    if (DIST(o) < 0xBB8 && aa >= 0x100) {
        U32(o + 0x54) = ((u32)(a < 0) << 24) | 0x301; return 1;
    }
    return 0;
}

/* ---- the moves (state 1) ---- */

/* move 0: close enough bites; else decides, walks or faces the player */
void DC2CALL Ino_ThinkWait(gptr o)                                  /* 0x437780 */
{
    u8 t = U8(o + 0x18D);
    if (t == 1 && DIST(o) < 0xBB8) {
        Enemy_BiteOn(o);
        U32(o + 0x54) = 0x501;
        return;
    }
    if (t || !U8(o + 0x56) || Ino_Decide(o)) return;
    if (DIST(o) < 0xBB8 && !U8(o + 0x13A)) { U32(o + 0x54) = 0x101; return; }
    face(o, 0x1E);
}

void DC2CALL Ino_Wait(gptr o)                                       /* 0x437800 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) begin(o, 0, 0x41, 0xA, 1);
    else if (s == 1) Anim_Step(o);
}

/* move 1: decides; far again it waits */
void DC2CALL Ino_ThinkWalk(gptr o)                                  /* 0x437860 */
{
    if (!U8(o + 0x56) || Ino_Decide(o)) return;
    if (DIST(o) > 0xFA0) { U32(o + 0x54) = 1; return; }
    face(o, 0x1E);
}

/* walks closer; after a while it pounces */
void DC2CALL Ino_Walk(gptr o)                                       /* 0x4378B0 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        begin(o, 5, 9, 0x14, 1);
        U16(o + 0x210) = 0;
    } else if (s == 1) {
        U16(o + 0x210)++;
        if (S16(o + 0x210) > 0x78) {
            Enemy_BiteOn(o);
            U32(o + 0x54) = 0x601;
            return;
        }
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, (u32)-15);
    }
}

/* move 2: the roar */
void DC2CALL Ino_ThinkRoar(gptr o)                                  /* 0x437BF0 */
{
    face(o, 0x1E);
}

void DC2CALL Ino_Roar(gptr o)                                       /* 0x437C10 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        begin(o, 1, 0x40, 0xA, 0);
        U16(o + 0x208) = 0x78;
    } else if (s == 1) {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        u16 f = U16(o + 0x92);
        if (f >= 0xE && f <= 0x31) Bits_Set(IN_BITS(o), 4, 1);
    }
}

/* move 3: a turn of 0x600 to the side o+0x57 (0 left, 1 right) */
void DC2CALL Ino_Turn(gptr o)                                       /* 0x437ED0 */
{
    static const s16 tot[2] = {0x600, -0x600}, rot[2] = {0x400, -0x400}, div[2] = {0x2E, 0x2E};
    static const u32 anim[2] = {3, 4};
    u8 s = U8(o + 0x56), d = U8(o + 0x57);
    if (s == 0) {
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, anim[d], 0, 0x40, 2);
        Bits_Set(IN_BITS(o), 1, 0);
        U32(o + 0x50) |= 0x2000;
        U16(o + 0x204) = 0;
        U16(o + 0x20A) = 0x96;
        s32 a = (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A));
        U16(o + 0x206) = (u16)((a - rot[d]) / div[d]);
    } else if (s == 1) {
        U16(o + 0x4A) += U16(o + 0x206);
        U16(o + 0x204) += (u16)(tot[d] / div[d]);
        if (Anim_Step(o)) {
            U32(o + 0x50) &= ~0x2000u;
            U16(o + 0x204) = 0;
            gptr p = U32(o + 0x64);
            U16(p + 0x4A) = (U16(p + 0x4A) - tot[d]) & 0xFFF;
            U16(o + 0x4A) = (U16(o + 0x4A) + tot[d]) & 0xFFF;
            Anim_Set(o, 0, 0, 1, 0xA);
            U32(o + 0x54) = 1;
        }
    }
}

/* move 4: falling off a ledge, facing back where it stood */
void DC2CALL Ino_ThinkFall(gptr o)                                  /* 0x437CC0 */
{
    if (U8(o + 0x56)) Obj_TurnToPoint(o, 0x46, o + 0x1F4);
}

void DC2CALL Ino_Fall(gptr o)                                       /* 0x437CE0 */
{
    gptr pl = PL();
    u8 s = U8(o + 0x56);
    if (s == 0) {
        begin(o, 0x1C, 0x40, 0xA, 0);
        U16(o + 0x1E2) = 0x1E;
        U16(o + 0x1F4) = U16(o + 0x1EC); U16(o + 0x1F6) = U16(o + 0x1EE); U16(o + 0x1F8) = U16(o + 0x1F0);
        Obj_TurnToPoint(o, 0x46, o + 0x1F4);
        U16(o + 0x214) = 0xC8;
        return;
    }
    if (s != 1) return;
    u32 ang = Angle_XZ(o + 0x1F4, o + 0x40);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = U16(o + 0x214); U16(v + 6) = 0;
    Vec_RotY(o + 0x1FC, v, ang);
    Tmp_Pop(8);
    if (S16(o + 0x214) > 0) U16(o + 0x214) -= 4;
    U16(o + 0x40) += U16(o + 0x1FC);
    U16(o + 0x44) += U16(o + 0x200);
    Anim_Step(o);
    U16(o + 0xF2) += U16(o + 0x1E2);
    if (S16(o + 0x42) <= 0) return;
    U16(pl + 0x1E8) = 0;
    Enemy_BiteOff(o);
    Se_Play(o, 0x65);
    gptr k = K2_AllocBack();
    if (k) {
        U8(k + 0x58) = 0x15; U8(k + 0x7A) = 5;
        U16(k + 0x6C) = 0x3C; U16(k + 0x6E) = 0x50;
        U16(k + 0x42) = 0xFCE0;
        U16(k + 0x40) = U16(o + 0x40);
        U16(k + 0x44) = U16(o + 0x44);
        U8(k + 0x64) = 0xFF; U8(k + 0x65) = 0x30; U8(k + 0x66) = 0x10;
    }
    u8 was = U8(o + 0x202);
    if ((was == 2 || was == 3) && !Bits_Check(IN_BITS(o), 6)) {
        Score_Kill(o);
        U32(o + 0x50) |= 0x40000000;
    } else Enemy_Remove(o);
}

/* moves 5/6: facing the player until the swing */
void DC2CALL Ino_ThinkBite(gptr o)                                  /* 0x438110 */
{
    if (U16(o + 0x92) < 0x14) face(o, 0x1E);
}

/* a bite; anim, its swing's frames */
static void bite(gptr o, u32 anim, u16 f0, u16 f1)
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        begin(o, anim, 0x40, 0xA, 0);
    } else if (s == 1) {
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        u16 f = U16(o + 0x92);
        if (f >= f0 && f <= f1) Bits_Set(IN_BITS(o), 4, 1);
    }
}

void DC2CALL Ino_Bite(gptr o)                                       /* 0x438060 */
{
    bite(o, 0xC, 0xE, 0x18);
}

void DC2CALL Ino_Bite2(gptr o)                                      /* 0x438140 */
{
    bite(o, 6, 0x15, 0x24);
}

/* moves 7/8: facing the player until it is held */
void DC2CALL Ino_ThinkGrab(gptr o)                                  /* 0x4381F0 */
{
    if (U8(o + 0x56) <= 1) face(o, 0x1E);
}

/* move 7: seizes the player (Ino_EatHit), shakes and eats it; the player
 * struggles free with the pad */
void DC2CALL Ino_Eat(gptr o)                                        /* 0x438210 */
{
    static const u32 end_anim[2] = {0xA, 0xB}, end_fx[2] = {0x65EEB8, 0x65EECC};
    gptr pl = PL(), b = IN_BITS(o);
    if (U8(o + 0x56) == 3) carry_player(o, pl);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 7, 0, 0x40, 0xA);
        Effect_Attach(o, 0x65E8E0);
        Bits_Set(b, 1, 0);
        break;
    case 1: {
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 1; }
        u16 f = U16(o + 0x92);
        Bits_Set(b, 3, f > 0x14 && f < 0x36);
        if (U16(o + 0x92) == 0x3C) Effect_Attach(o, 0x65D720);
        break;
    }
    case 2:
        if (!Anim_Step(o)) break;
        U8(o + 0x56)++;
        Anim_Start(o, 9, 0, 0x41, 2);
        Flash_Play(o, 0x65EE84);
        Effect_Attach(o, 0x65D88C);
        U32(o + 0x1D8) = 0;
        Anim_Start(pl, pl_anim(o, 0x7C), 0, 0x41, 2);
        seize_player(o, pl);
        Flag_Set(1, 1, 0);
        break;
    case 3:
        if (Anim_Step(o)) {
            Flash_Play(o, 0x65EE84);
            Effect_Attach(o, 0x65D88C);
            U32(o + 0x1D8)++;
        }
        if (U16(o + 0x92) == 0x14) hurt_player(pl, 0x64);
        U32(o + 0x1D8) += Pad_Shake();
        if ((s32)U32(o + 0x1D8) > 0x14 && U16(o + 0x92) >= 0x64) {
            U8(o + 0x56)++;
            Anim_Start(o, end_anim[U8(o + 0x216)], 0, 0x40, 0xA);
            U32(pl + 0x50) &= ~0x100u;
            Flag_Set(1, 1, 0);
            Anim_Set(pl, pl_anim(o, 0x80), 0, 0x40, 0);
            Obj_UpdateMatrix(pl);
            Flash_Play(o, end_fx[U8(o + 0x216)]);
            U32(G() + 0x92C) = 0x4385E0;
        }
        break;
    case 4:
        if (Anim_Step(o)) {
            Enemy_BiteOff(o);
            if (U8(o + 0x216)) {
                Anim_Start(o, 0x11, 0, 0x41, 5);
                U16(o + 0x20E) = 0x5A;
                U32(o + 0x54) = 0x20402;
                Bits_Set(b, 2, 1);
            } else U32(o + 0x54) = 1;
        } else if (U16(o + 0x92) >= 0x44) U32(o + 0x50) |= 8;
        break;
    }
}

/* the player thrown free of Ino_Eat (G+0x92C) */
void DC2CALL Ino_EatEnd(gptr pl)                                    /* 0x4385E0 */
{
    if (!Anim_Step(pl)) return;
    U32(pl + 0x50) &= ~0x20u;
    turn_player(pl, 0x400);
    Anim_Set(pl, 0x16, 0, 0, 0);
    Obj_UpdateMatrix(pl);
    Player_AnimReset();
}

/* move 8: leaps on the player (from behind when it runs), holds it down
 * and bites */
void DC2CALL Ino_Pounce(gptr o)                                     /* 0x438650 */
{
    gptr pl = PL();
    if (U8(o + 0x56) == 2 && U16(o + 0x92) > 0x12 && U16(o + 0x92) < 0x5A) carry_player(o, pl);
    switch (U8(o + 0x56)) {
    case 0:
        begin(o, 2, 0x40, 0xA, 0);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        if (U8(pl + 0x174) & 2) {
            U8(o + 0x56)++;
            U32(o + 0x50) &= ~8u;
            Anim_Start(o, 0xD, 0, 0x40, 0xA);
            Flash_Play(o, 0x65EEF8);
            Effect_Attach(o, 0x65DB64);
            Player_Hold(o, 0, 0x48E100);
        } else {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 1;
        }
        break;
    case 2:
        if (Anim_Step(o)) {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 1;
            U32(pl + 0x50) &= ~0x120u;
            Flag_Set(1, 1, 0);
            turn_player(pl, -0x400);
            Anim_Set(pl, 0xD, 0, 0, 0);
            Obj_UpdateMatrix(pl);
            U32(pl + 0x54) = 0x30302;
        }
        if (U16(o + 0x92) == 0x12) {
            Anim_Start(pl, pl_anim(o, 0x84), 6, 0x40, 0);
            U16(pl + 0x1FA) = 0x70;
            Bits_Set(SHARED, 0, 1);
            U32(pl + 0x50) |= 0x120;
            Flag_Set(1, 1, 1);
            seize_player(o, pl);
        }
        if (U16(pl + 0x92) == 0x5A) hurt_player(pl, 0xC8);
        break;
    }
}

/* ---- the attacks landing ---- */

/* a bite's hit: harder from behind the player */
static void bite_hit(gptr o, gptr fl, gptr fx, u32 back, u32 front)
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, fl);
    Effect_Attach(o, fx);
    if (iabs(d) < 0x400) Player_Hurt(o, back, 2, 0x28);
    else Player_Hurt(o, front, 1, 0);
}

void DC2CALL Ino_BiteHit(gptr o)                                    /* 0x4388F0 */
{
    bite_hit(o, 0x65EEE8, 0x65DAC0, 0xB4, 0x78);
}

void DC2CALL Ino_Bite2Hit(gptr o)                                   /* 0x438970 */
{
    bite_hit(o, 0x65EE64, 0x65D5DC, 0xC8, 0xA0);
}

/* move 7's hit: seizes the player unless flag 2/0x1A (then only hurts) */
void DC2CALL Ino_EatHit(gptr o)                                     /* 0x4389F0 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    Effect_Attach(o, 0x65D720);
    if (Flag_Check(2, 0x1A)) {
        Flash_Play(o, 0x65EE74);
        Player_Hurt(o, 0xA0, 1, 0);
        return;
    }
    U8(o + 0x56)++;
    Anim_Start(o, 8, 0, 0x40, 3);
    U32(o + 0x50) &= ~8u;
    stop(o);
    Flash_Play(o, 0x65EE74);
    Bits_Set(IN_BITS(o), 3, 0);
    U16(pl + 0x1FA) = 0x70;
    Bits_Set(SHARED, 0, 1);
    Player_Hold(o, 0xA0, 0x48E100);
    Anim_Set(pl, pl_anim(o, 0x78), 0, 0x40, 0);
    U32(pl + 0x50) |= 0x120;
    Flag_Set(1, 1, 1);
    seize_player(o, pl);
}

/* ---- hit (state 2) ---- */

/* hit 0: the damage by weapon and state, then the reaction */
void DC2CALL Ino_Damage(gptr o)                                     /* 0x438B40 */
{
    static const u32 fx[3] = {0x65DCF8, 0x65E888, 0x65E8B4};
    gptr b = IN_BITS(o);
    u32 hit = 0;
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;
    if (Bits_Check(b, 3)) U16(o + 0x176) >>= 2;
    switch (U8(o + 0x17D)) {
    case 1: case 2: case 9: case 0x10: case 0x11:
        U16(o + 0x176) >>= 1;
        break;
    case 4:
        if (Bits_Check(b, 2)) U16(o + 0x176) <<= 1;
        break;
    }
    if (U16(o + 0x172) == 4) U16(o + 0x176) >>= 1;
    if (Bits_Check(b, 2)) {
        hit = 1;
        Bits_Set(b, 5, 0);
        U32(o + 0x1D0) += U16(o + 0x176);
        if ((s32)U32(o + 0x1D0) > S16(o + 0x10A) / 3) {
            U32(o + 0x1D0) = 0;
            Bits_Set(b, 5, 1);
        }
        U32(o + 0x54) = 0x502;
        if (U16(o + 0x172) == 3) U32(o + 0x54) = 0x602;
        if (U16(o + 0x172) == 4) U32(o + 0x54) = 0x702;
        if (U8(o + 0x17D) == 4) U32(o + 0x54) = 0x802;
    } else {
        U32(o + 0x54) = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
        if (U8(o + 0x170) & 2) {
            if (!U16(o + 0x20C)) U32(o + 0x54) = 0x302;
            U16(o + 0x20C) = 0x28;
        }
        if (Bits_Check(b, 4)) { U16(o + 0x20C) = 0; U32(o + 0x54) = 0x102; hit = 1; }
        if (U16(o + 0x172) == 3) { U16(o + 0x20C) = 0; U32(o + 0x54) = 0x102; hit = 1; }
        if (Bits_Check(b, 3)) {
            U16(o + 0x20C) = 0;
            U32(o + 0x54) = U16(o + 0x172) == 3 ? 0x202 : 0x102;
            hit = 1;
        }
        if (U8(o + 0x17D) == 0x13) {
            U16(o + 0x20C) = 0; U32(o + 0x54) = 0x402; hit = 1;
            Bits_Set(b, 2, 1);
        }
        u8 c = U8(G() + 0x108F);
        if (c == 3 || c == 4 || c >= 7) {
            U16(o + 0x20C) = 0;
            if (S16(o + 0x108) - (s32)U16(o + 0x176) < 0) { U32(o + 0x54) = 0x102; hit = 1; }
            else {
                U32(o + 0x54) = 0x402; hit = 1;
                Bits_Set(b, 2, 1);
            }
        }
    }
    if (U8(o + 0x54) == 2 && U8(o + 0x56) == 0) {
        Player_Release(o);
        Enemy_BiteOff(o);
        stop(o);
        Bits_Set(b, 3, 0);
        Bits_Set(b, 4, 0);
        if (U16(o + 0x204)) {
            u16 a = U16(o + 0x204) + U16(o + 0x4A);
            U16(o + 0x204) = 0;
            U16(o + 0x4A) = a & 0xFFF;
        }
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65EC58);
        if (hit) {
            if (U8(o + 0x184) & 8) Score_Event(o);
            U16(o + 0x108) -= U16(o + 0x176);
            U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 0xA;
            if (S16(o + 0x108) < 0) {
                U32(o + 0x50) &= ~8u;
                U16(o + 0x108) = 0xFFFF;
                U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
                Score_Kill(o);
                Bits_Set(b, 6, 1);
            }
        }
    } else {
        U8(o + 0x17B) = 0xA;
        Effect_Attach(o, fx[Rand8() % 3]);
        if ((U8(o + 0x170) & 2) && !U16(o + 0x212)) {
            Prop_Attach(o, 0x65EC58);
            U16(o + 0x212) = 0xA;
        }
    }
    Orig_Call1(U32(IN_MAIN + U8(o + 0x54) * 4), o);
}

/* hit 1: a flinch */
void DC2CALL Ino_Flinch(gptr o)                                     /* 0x438F70 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0x12, Bits_Check(SHARED, 2) ? 3 : 0, 0x40, 0);
        Bits_Set(SHARED, 2, 1);
        Effect_Attach(o, 0x65DD24);
    } else if (s == 1 && Anim_Step(o)) {
        U32(o + 0x54) = 1;
        Bits_Set(SHARED, 2, 0);
    }
}

/* the turn onto its side when it goes down */
static void lie_down(gptr o)
{
    gptr p = U32(o + 0x64);
    U16(p + 0x4A) = (U16(p + 0x4A) - 0x400) & 0xFFF;
    U16(o + 0x4A) = (U16(o + 0x4A) + 0x400) & 0xFFF;
    Anim_Set(o, 0x11, 0, 0x41, 5);
    U16(o + 0x20E) = 0x5A;
    U32(o + 0x54) = 0x20402;
    Bits_Set(IN_BITS(o), 2, 1);
}

/* hit 2: knocked over */
void DC2CALL Ino_Knock(gptr o)                                      /* 0x439000 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0x1B, 0, 0x40, 0);
        Effect_Attach(o, 0x65E58C);
    } else if (s == 1 && Anim_Step(o)) lie_down(o);
}

/* hit 3: staggers, shakes it off */
void DC2CALL Ino_Stagger(gptr o)                                    /* 0x4390B0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0xE, 0, 0x40, 0xA);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 0xF, 0, 0x41, 0xA); }
        face(o, 0x1E);
        break;
    case 2:
        Anim_Step(o);
        if (!U16(o + 0x20C)) { U8(o + 0x56)++; Anim_Start(o, 0x10, 0, 0x40, 0xA); }
        face(o, 0xF);
        if (Rand8() < 0x10) U8(o + 0x56) = 0;
        break;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* hit 4: down, lies a while, gets up */
void DC2CALL Ino_Down(gptr o)                                       /* 0x4391A0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (Bits_Check(SHARED, 2)) Anim_Start(o, 0x13, 1, 0x40, 0);
        else Anim_Start(o, 0x13, 0, 0x40, 5);
        Bits_Set(SHARED, 2, 1);
        Effect_Attach(o, 0x65DEB8);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        U8(o + 0x56)++;
        Anim_Start(o, 0x11, 0, 0x41, 5);
        U16(o + 0x20E) = (u16)((Rand8() & 0x1F) + 0x5A);
        Bits_Set(SHARED, 2, 0);
        break;
    case 2:
        Anim_Step(o);
        if (!U16(o + 0x20E)) { U8(o + 0x56)++; Anim_Start(o, 0x1A, 0, 0x40, 0xA); }
        break;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        if (U16(o + 0x92) == 0x16) Bits_Set(IN_BITS(o), 2, 0);
        break;
    }
}

/* back to lying (state 2 hit 4 step 3: getting up) */
static void lie_again(gptr o)
{
    Anim_Start(o, 0x1A, 0, 0x40, 0xA);
    U32(o + 0x54) = 0x30402;
}

/* hit 5: hit while down; with enough damage it stays down */
void DC2CALL Ino_Hit5(gptr o)                                       /* 0x4392D0 */
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0x17, 0, 0x40, 0);
        Effect_Attach(o, 0x65E1E8);
        if (Bits_Check(IN_BITS(o), 5)) U8(o + 0x17B) = 0x1E;
    } else if (s == 1) {
        if (Anim_Step(o) || (U16(o + 0x92) > 0xF && Bits_Check(IN_BITS(o), 5))) lie_again(o);
    }
}

/* a hit while down: anim, effect, then lying again */
static void hit_down(gptr o, u32 anim, gptr fx)
{
    u8 s = U8(o + 0x56);
    if (s == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, anim, 0, 0x40, 0);
        Effect_Attach(o, fx);
    } else if (s == 1 && Anim_Step(o)) lie_again(o);
}

/* hit 6 (shot 3 while down) */
void DC2CALL Ino_Hit6(gptr o)                                       /* 0x439370 */
{
    hit_down(o, 0x18, 0x65E2B4);
}

/* hit 7 (shot 4 while down): one of three */
void DC2CALL Ino_Hit7(gptr o)                                       /* 0x4393E0 */
{
    static const u32 anim[3] = {0x14, 0x15, 0x16}, fx[3] = {0x65E0EC, 0x65E118, 0x65E16C};
    u8 s = U8(o + 0x56);
    if (s == 0) {
        U8(o + 0x56)++;
        u32 r = Rand8() % 3;
        Anim_Start(o, anim[r], 0, 0x40, 0);
        Effect_Attach(o, fx[r]);
    } else if (s == 1 && Anim_Step(o)) lie_again(o);
}

/* hit 8 (burnt while down) */
void DC2CALL Ino_Hit8(gptr o)                                       /* 0x4394B0 */
{
    hit_down(o, 0x19, 0x65E448);
}

/* ---- dying (state 3) ---- */

/* the body left: shown as part of the room, a pool of blood at frame f */
static void dead_settle(gptr o, u16 f)
{
    if (Anim_Step(o)) {
        U8(o + 0x56)++;
        U32(o + 0x50) |= 0x30;
        Layers_Assign();
    }
    if (U16(o + 0x92) == f) Prop_SpawnC(0, o + 0x40, 2, 0x28, 1, 0);
}

/* die 1..3: falls */
void DC2CALL Ino_Dead(gptr o)                                       /* 0x439520 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x1C, Bits_Check(SHARED, 1) ? 3 : 0, 0x40, 0x14);
        Bits_Set(SHARED, 1, 1);
        Bits_Set(IN_BITS(o), 1, 0);
        Effect_Attach(o, 0x65E9D4);
        break;
    case 1:
        dead_settle(o, 0x2D);
        break;
    case 2:
        Obj_DeathFade(o);
        break;
    }
}

/* die 4..7: falls from lying */
void DC2CALL Ino_DeadDown(gptr o)                                   /* 0x4395F0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x18, 0, 0x44, 2);
        Effect_Attach(o, 0x65E2B4);
        Bits_Set(IN_BITS(o), 1, 0);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0x16) { U8(o + 0x56)++; Anim_Start(o, 0x1D, 0, 0x48, 0xA); }
        break;
    case 2:
        dead_settle(o, 0xA);
        break;
    case 3:
        Obj_DeathFade(o);
        break;
    }
}

/* die 8: burnt */
void DC2CALL Ino_DeadFire(gptr o)                                   /* 0x4396D0 */
{
    Obj_DeathFade(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x19, 0, 0x40, 0);
        Effect_Attach(o, 0x65E448);
        Bits_Set(IN_BITS(o), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 0x1D, 0, 0x48, 0xA); }
        break;
    case 2:
        dead_settle(o, 0xA);
        break;
    }
}
