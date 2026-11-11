/* The allosaurus (object type handler 0x4397C0, type 8, E60.DAT). It keeps
 * its state at o+0x1C0.., a set of bits first (Bits_Set(o+0x1C0, n)):
 * 0 holding the bite, 1 moving (its head looks), 2 the attack hits, 3 angry,
 * 4 gets up to attack, 5 strong stagger, 6 turns about after a wall, 7 the
 * hit was from close, 8 the scripted run (kind 1), 9 climbs after a leap.
 * Then +0x4 kind (0 normal, 1 the scripted one, 2 the extra mode), +0x8..0xB
 * the state of the last frame, +0xC/+0x10/+0x14 timers, +0x18 the hold (o
 * that holds the player), +0x1C the player's struggling, +0x26 the turn's
 * angle a frame, +0x2E its rest, +0x36 the leap's height, +0x38 anger left,
 * +0x3A..+0x40 timers, +0x42 tint, +0x44 anger timer, +0x46 hits, +0x47 the
 * wall edge, +0x48.. more timers, +0x50 the player's weapon kind.
 * States (o+0x54): 0 init, 1 normal (think 0x720FCC, then move 0x72100C by
 * o+0x55), 2 hit (0x72104C), 3 die (0x72105C), 4 (0x4A56D0). Attacks land
 * through 0x72106C. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define AL_MAIN  0x720FB8u      /* state handlers */
#define AL_THINK 0x720FCCu      /* state 1: deciding, by o+0x55 */
#define AL_MOVE  0x72100Cu      /* state 1: moving, by o+0x55 */
#define AL_HIT   0x72104Cu      /* state 2 */
#define AL_DIE   0x72105Cu      /* state 3 */
#define AL_GRAB  0x72106Cu      /* an attack landed, by o+0x55 */
#define AL_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u      /* bits shared by the allosauri: 0 one bites, 1 one died, 2 flinch */
#define PL()     U32(G() + 0x940)
#define DIST(o)  U32((o) + 0x13C)
#define PL_ANIM(o, off) U32(U32((o) + 0x88) + Allo_PlAnim(o) * 4 + (off))

static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static s32 ang_to(gptr o) { return (s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A)); }
static u16 y_level(gptr o) { return (u16)((s32)S16(o + 0x42) / -1000); }

/* the player follows o (held) */
static void hold_player(gptr o, gptr pl)
{
    U16(o + 0x40) += U16(pl + 0xE8); U16(o + 0x42) += U16(pl + 0xEA); U16(o + 0x44) += U16(pl + 0xEC);
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
}

/* the player placed at o and its matrices, walls */
static void place_player(gptr o, gptr pl)
{
    U16(pl + 0x40) = U16(o + 0x40); U16(pl + 0x42) = U16(o + 0x42); U16(pl + 0x44) = U16(o + 0x44);
    U16(pl + 0x4A) = U16(o + 0x4A);
    Obj_SyncLinks(pl); Obj_UpdateMatrix(pl); Obj_CollideWalls(pl);
}

/* the player thrown down, turned about */
static void drop_player(gptr pl)
{
    U32(pl + 0x50) &= ~0x120u;
    Flag_Set(1, 1, 0);
    gptr p = U32(pl + 0x64);
    U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
    U16(pl + 0x4A) = (U16(pl + 0x4A) - 0x800) & 0xFFF;
    Anim_Set(pl, 0xC, 0, 0, 0xA);
    Obj_UpdateMatrix(pl);
    U32(pl + 0x54) = 0x30202;
}

static void hurt_player(gptr pl, u32 d)
{
    U16(pl + 0x108) -= (u16)Damage_Scale(d);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
}

void DC2CALL Allo_Handler(gptr o)                                   /* 0x4397C0 */
{
    gptr b = AL_BITS(o);
    U8(o + 0x210) = U8(G() + 0x108F) ? 1 : 0;
    if (U32(o + 0x1C4) == 1) {                      /* the scripted one: angry, runs off */
        Bits_Set(b, 3, 1);
        U16(o + 0x204) = 0x1C2; U8(o + 0x206) = 0; U16(o + 0x1F8) = 1000;
        if (S16(o + 0x44) > 0x9C4 && U8(G() + 0x108F) == 1 && U8(G() + 0x7C4) == 0xC) Bits_Set(b, 8, 1);
        if (Bits_Check(b, 8)) {
            gptr v = Tmp_Push(8);
            U16(v) = 0xCC1E; U16(v + 2) = 0; U16(v + 4) = 0x1B26;
            Zone_TrackPoint(o, v);
            Tmp_Pop(8);
            if (S16(o + 0x40) < -0x1B58) Enemy_Remove(o);
        }
    }
    Obj_SavePos(o);
    Orig_Call1(U32(AL_MAIN + U8(o + 0x54) * 4), o);
    Allo_Tint(o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    s32 a = (s16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A));
    U32(o + 0x158) = iabs(a) < 0x500 && Bits_Check(b, 1) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Allo_Timer(o);
    for (u32 k = 0x1CC; k <= 0x1D4; k += 4) if (U32(o + k)) U32(o + k)--;
    static const u16 tm[] = {0x1FA, 0x1FC, 0x1FE, 0x200, 0x208, 0x20A, 0x20C};
    for (u32 k = 0; k < 7; k++) if (U16(o + tm[k])) U16(o + tm[k])--;
    Allo_Grab(o);
    U8(o + 0x1C8) = U8(o + 0x54); U8(o + 0x1CB) = U8(o + 0x57);
    U8(o + 0x1C9) = U8(o + 0x55); U8(o + 0x1CA) = U8(o + 0x56);
    if (U32(o + 0x50) & 0x40000000) {
        Obj_Disable(o);
        Bits_Set(SHARED, 1, 0);
    }
}

/* state 0: kind, health, model, animations */
void DC2CALL Allo_Init(gptr o)                                      /* 0x439A80 */
{
    gptr b = AL_BITS(o);
    U32(o + 0x1C4) = 0;
    if (U8(G() + 0x1091) == 8 && U8(G() + 0x1090) == 8) U32(o + 0x1C4) = 1;
    if (U8(G() + 0x927)) U32(o + 0x1C4) = 2;
    U32(o + 0x50) = 0x100D;
    U8(o + 0x54)++;
    if (U8(G() + 0x927)) U32(o + 0x50) = 0x100B;
    U16(o + 0x108) = 5000;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 10000;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720F48; U32(o + 0x128) = 0x720F78; U32(o + 0x134) = 0x720F88;
    stop(o);
    U16(o + 0x5A) = y_level(o);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x638000);
    U32(o + 0x88) = 0x63AD4C;
    Anim_Start(o, 0, 0, 1, 0);
    Obj_Set118(o, 0, 0x2770, 0x4E80);
    DrawSync(o);
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x40; U16(o + 0x166) = 0x40; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(b, 1, 1);
    U32(o + 0x54) = U8(o + 0x10C) & 0x20 ? 0x701 : 0x201;
    U32(0x74AFA0) = 0;
    Obj_SetFeet(o, 0x10, 0xC);
    U8(o + 0xFE) = 3;
    Bits_Set(b, 1, 1);
    U32(o + 0x184) |= 0x20;
    U8(o + 0x118) |= 0x20;
    U32(SHARED) = 0;
    Flag_Set(2, 0x27, 1);
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* state 1: decides, then moves */
void DC2CALL Allo_Normal(gptr o)                                    /* 0x439CA0 */
{
    Obj_SaveFeet(o);
    Bits_Set(AL_BITS(o), 2, 0);
    Orig_Call1(U32(AL_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(AL_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Allo_Hit(gptr o)                                       /* 0x439CE0 */
{
    Orig_Call1(U32(AL_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Allo_Die(gptr o)                                       /* 0x439D00 */
{
    Orig_Call1(U32(AL_DIE + U8(o + 0x55) * 4), o);
}

/* its attacks hit the player: by the sub-state */
void DC2CALL Allo_Grab(gptr o)                                      /* 0x439D20 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(AL_GRAB + U8(o + 0x55) * 4), o);
}

/* ---- deciding (state 1, before the move) ---- */

/* the player in reach, on its level, no other biting */
static int can_bite(gptr o, gptr pl)
{
    return !U8(o + 0x13A) && U16(o + 0x5A) == U16(pl + 0x5A) && !Bits_Check(SHARED, 0);
}

/* sub-state 0: standing */
void DC2CALL Allo_ThinkIdle(gptr o)                                 /* 0x439D50 */
{
    gptr pl = PL(), b = AL_BITS(o);
    if (U8(o + 0x56) != 2) return;
    if (U8(o + 0x206) == 2 || !U16(o + 0x204) || DIST(o) > 0x2EE0) {
        Bits_Set(b, 3, 0);
        U16(o + 0x204) = 0;
    }
    s32 a = ang_to(o), d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (can_bite(o, pl)) {
        s32 aa = iabs(a);
        if (aa < 0x100 && !U32(o + 0x1D8) && DIST(o) < 5000) { Enemy_BiteOn(o); U32(o + 0x54) = 0x901; return; }
        if (aa < 0x60 && (U8(pl + 0x174) & 2) && iabs(d) < 0x200 && DIST(o) > 0x9C4 && DIST(o) < 5000 && !Flag_Check(2, 0x1A)) {
            Enemy_BiteOn(o); U32(o + 0x54) = 0xD01; return;
        }
        if (!U32(o + 0x1D8) && DIST(o) < 0xDAC) {
            if (a > 0x400) { Enemy_BiteOn(o); U32(o + 0x54) = 0xE01; return; }
            if (a < -0x400) { Enemy_BiteOn(o); U32(o + 0x54) = 0x1000E01; return; }
        }
    }
    if (DIST(o) > 5000 || U16(o + 0x5A) != U16(pl + 0x5A)) { U32(o + 0x54) = 0x201; return; }
    if (iabs(a) > 0x60 || Rand8() < 8 || Bits_Check(b, 3) || DIST(o) < 0xBB8) Allo_Pick(o);
}

/* sub-state 1: walking */
void DC2CALL Allo_ThinkWalk(gptr o)                                 /* 0x43A040 */
{
    gptr pl = PL(), b = AL_BITS(o);
    if (!U8(o + 0x56)) return;
    s32 a = ang_to(o);
    if (can_bite(o, pl) && iabs(a) < 0x80) {
        if (!U32(o + 0x1D8) && !Bits_Check(b, 3) && DIST(o) < 0xBB8) { Enemy_BiteOn(o); U32(o + 0x54) = 0xC01; return; }
        if ((U8(pl + 0x174) & 2) && DIST(o) > 0x9C4 && DIST(o) < 5000 && !Flag_Check(2, 0x1A)) {
            Enemy_BiteOn(o); U32(o + 0x54) = 0xD01; return;
        }
    }
    if (Allo_Steer(o)) return;
    if (U16(o + 0x5A) != U16(PL() + 0x5A)) return;
    if (DIST(o) >= 0xFA0 && !Bits_Check(b, 3)) {
        if (DIST(o) > 5000 && !U16(o + 0x1FE)) U32(o + 0x54) = 0x201;
        return;
    }
    U32(o + 0x54) = 1;
}

/* sub-state 2: running */
void DC2CALL Allo_ThinkRun(gptr o)                                  /* 0x43A240 */
{
    gptr pl = PL(), b = AL_BITS(o);
    if (U8(o + 0x56) <= 1) return;
    s32 a = ang_to(o);
    if (can_bite(o, pl) && iabs(a) < 0x80 && !U32(o + 0x1D8) && DIST(o) < 6000) {
        Enemy_BiteOn(o); U32(o + 0x54) = 0xA01; return;
    }
    if (Allo_Steer(o)) return;
    if (U16(o + 0x5A) != U16(PL() + 0x5A) || Bits_Check(b, 8) || U16(o + 0x20C)) return;
    if (!((DIST(o) < 5000 && U32(o + 0x1D8)) || DIST(o) < 0x7D0)) {
        if (U8(o + 0x13A)) return;
        if (!U16(o + 0xE8) && !U16(o + 0xEA) && !U16(o + 0xEC)) return;
    }
    U32(o + 0x54) = 0x601;
    U16(o + 0x20C) = 0x96;
}

/* walking or running: a bite from below, turning, a wall ahead (a ledge to
 * leap), stuck; 1 when it chose */
u32 DC2CALL Allo_Steer(gptr o)                                      /* 0x43A4C0 */
{
    gptr pl = PL(), b = AL_BITS(o);
    s32 a = ang_to(o);
    if (!U8(o + 0x13A) && S16(o + 0x5A) + 2 == S16(pl + 0x5A) && !Bits_Check(SHARED, 0) && iabs(a) < 0x80
        && !U32(o + 0x1D8) && DIST(o) < 0xDAC) {
        Enemy_BiteOn(o); U32(o + 0x54) = 0xB01; return 1;
    }
    if (DIST(o) > 0xDAC && !U16(o + 0x20A) && Rand8() < 0x10) {
        U32(o + 0x54) = 0xF01; U16(o + 0x20A) = 0xB4; return 1;
    }
    if (!Bits_Check(b, 3)) {
        s32 dir = -1;
        if (a < 0) { if (a < -0x400) dir = 3; if (a < -0x600) dir = 1; }
        else { if (a > 0x400) dir = 2; if (a > 0x600) dir = 0; }
        if (dir >= 0) {
            if (!U16(o + 0x208)) { U16(o + 0x208) = 0x3C; U32(o + 0x54) = (u32)dir << 24 | 0x401; return 1; }
            Enemy_BiteOn(o);
            U32(o + 0x54) = a > 0 ? 0xE01 : 0x1000E01;
            return 1;
        }
    }
    if (U16(o + 0x1FA)) {
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x50, (u32)((U8(o + 0x207) >> 4) & 3) << 10);
        if (U16(o + 0x1FA) == 1) {
            Allo_Pick(o);
            if (Rand8() & 1) Bits_Set(b, 6, 1);
            U16(o + 0x200) = 0x3C;
        }
    }
    if (Wall_Ahead(o, 0x6A4) && (!U32(o + 0x1D8) || DIST(o) > 0xDAC)) {
        if (!U16(o + 0x1FA)) U16(o + 0x1FA) = 0x14;
        U8(o + 0x207) = U8(o + 0x11A);
        if (Step_Ahead(o, 0xDAC, 0x4B0, U16(o + 0x4A))) {
            U32(o + 0x54) = U8(o + 0x11A) & 0x40 ? 0x1000301 : 0x301;
            return 1;
        }
    }
    if (!U16(o + 0x200)) Obj_TurnToPoint(o, 0x28, o + 0xD8);
    if (U16(o + 0x1FA)) return 1;
    if (!U16(o + 0xE8) && !U16(o + 0xEA) && !U16(o + 0xEC)) { U16(o + 0x20E) = 0; return 0; }
    if (++S16(o + 0x20E) >= 0x3C) {
        U16(o + 0x20E) = 0;
        Allo_Pick(o);
        if (Rand8() & 1) Bits_Set(b, 6, 1);
        U16(o + 0x200) = 0x3C;
    }
    return 0;
}

/* sub-state 5: backing off, facing the player */
void DC2CALL Allo_ThinkBack(gptr o)                                 /* 0x43ABA0 */
{
    gptr b = AL_BITS(o);
    if (!U8(o + 0x56)) return;
    if (!Bits_Check(b, 6)) Obj_TurnToPoint(o, 0x28, o + 0xD8);
    if (iabs(ang_to(o)) <= 0x60 && DIST(o) >= 0x7D0) return;
    u16 f = U16(o + 0x92);
    if (f > 9 && f < 0x11) Value_Approach(o + 0xF4, 0x14, Bits_Check(b, 3) ? (u32)-0x82 : (u32)-0x64);
    else Value_Approach(o + 0xF4, 0x14, 0);
}

/* sub-state 10: the lunge turns to the player when angry */
void DC2CALL Allo_ThinkLunge(gptr o)                                /* 0x43B390 */
{
    if (Bits_Check(AL_BITS(o), 3)) Obj_TurnToPoint(o, 0xD, o + 0xD8);
}

/* ---- moving (state 1) ---- */

/* sub-state 0: stands, looks about */
void DC2CALL Allo_Idle(gptr o)                                      /* 0x439F70 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 0x16, 0, 0x40, 0xA);
        Bits_Set(AL_BITS(o), 1, 1);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) > 0xF) { U8(o + 0x56)++; Anim_Start(o, 9, 0, 0x41, 5); }
        break;
    case 2: Anim_Step(o); break;
    case 3:
        U8(o + 0x56)--; stop(o);
        Anim_Start(o, 9, 0, 0x41, 0xA);
        Bits_Set(AL_BITS(o), 1, 1);
        break;
    }
}

/* sub-state 1: walks */
void DC2CALL Allo_Walk(gptr o)                                      /* 0x43A1B0 */
{
    gptr b = AL_BITS(o);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0, 0, 1, 0xA);
        Bits_Set(b, 1, 1);
    } else if (U8(o + 0x56) == 1) {
        Anim_Step(o);
        if (Bits_Check(b, 3)) Value_Approach(o + 0xF4, 0x28, 0xA0);
        else Value_Approach(o + 0xF4, 0x1E, 0x82);
    }
}

/* sub-state 2: runs (a start with a bite when near) */
void DC2CALL Allo_Run(gptr o)                                       /* 0x43A390 */
{
    gptr b = AL_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 2, 0, 0x40, 0xA);
        Bits_Set(b, 1, 1);
        if (DIST(o) < 0x1B58 || U16(o + 0x1FC)) { U8(o + 0x56) = 2; Anim_Start(o, 8, 0, 1, 0xA); }
        U16(o + 0x1FC) = 0x708;
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 8, 0, 1, 0xA); break; }
        if (U16(o + 0x92) >= 0x11 && U16(o + 0x92) <= 0x28) { Bits_Set(b, 2, 1); U8(o + 0x17B) = 0; }
        break;
    case 2:
        Anim_Step(o);
        if (Bits_Check(b, 3)) Value_Approach(o + 0xF4, 0x32, 0xE6);
        else Value_Approach(o + 0xF4, 0x28, 0xBE);
        break;
    }
}

/* sub-state 3: leaps a level down or up (o+0x57) */
void DC2CALL Allo_Leap(gptr o)                                      /* 0x43A7E0 */
{
    static const u32 hit_lo[2] = {0, 0}, hit_hi[2] = {0x47, 0x29}, walls[2] = {0x40, 0x1A};
    static const u32 shake[2] = {0x42, 0x23}, land[2] = {0x54, 0x2E}, anim[2] = {0x18, 0x19};
    static const u32 dy[2] = {(u32)-0x7D0, 0x7D0}, eff[2] = {0x65C6FC, 0x65BF70};
    gptr b = AL_BITS(o);
    u32 k = U8(o + 0x57);
    if (U8(o + 0x56) == 0) {
        U16(o + 0x174) |= 4;
        U32(o + 0x50) |= 0x70;
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, anim[k], 0, 0x40, 5);
        Bits_Set(b, 1, 0);
        U16(o + 0x1F6) = (u16)(dy[k] + U16(o + 0x42));
        Effect_Attach(o, eff[k]);
        U8(o + 0x17B) = 0xA;
    } else if (U8(o + 0x56) == 1) {
        U16(o + 0x5A) = y_level(o);
        u32 f = U16(o + 0x92);
        if (f >= hit_lo[k] && f <= hit_hi[k]) U8(o + 0x17B) = 0xA;
        else U32(o + 0x50) &= ~0x50u;
        if (f >= walls[k] && !Bits_Check(b, 9)) U32(o + 0x50) &= ~0x10u;
        if (U16(o + 0x92) == shake[k]) Shake_Set(3, 0, 0x1E);
        if (!Anim_Step(o) && !Allo_FrameNear(o, land[k], 0x1388)) return;
        U16(o + 0x42) = U16(o + 0x1F6);
        U32(o + 0x50) &= ~0x70u;
        U16(o + 0x174) &= 0xFFFB;
        U8(o + 0x17B) = 0;
        U32(o + 0x54) = 0x101;
        U16(o + 0x5A) = y_level(o);
        Bits_Set(b, 9, 0);
    }
}

/* sub-state 4: turns about by o+0x57 (half or quarter, either way) */
void DC2CALL Allo_Turn(gptr o)                                      /* 0x43AA10 */
{
    static const s16 turn[4] = {0x800, -0x800, 0x400, -0x400}, frames[4] = {0x30, 0x30, 0x1E, 0x1E};
    static const u32 anim[4] = {1, 4, 5, 6};
    u32 k = U8(o + 0x57);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, anim[k], 0, 0x40, 4);
        Bits_Set(AL_BITS(o), 1, 0);
        U16(o + 0x1E6) = 0;
        U16(o + 0x1EE) = (u16)(((s32)(s16)Angle_DiffTo(o + 0x40, o + 0xD8, U16(o + 0x4A)) - turn[k]) / frames[k]);
    } else if (U8(o + 0x56) == 1) {
        s16 step = (s16)(turn[k] / frames[k]);
        U16(o + 0x4A) += U16(o + 0x1EE);
        U16(o + 0x1E6) += (u16)step;
        if (!Anim_Step(o)) return;
        gptr p = U32(o + 0x64);
        U16(o + 0x1E6) = 0;
        U16(p + 0x4A) = (U16(p + 0x4A) - turn[k]) & 0xFFF;
        U16(o + 0x4A) = (U16(o + 0x4A) + turn[k]) & 0xFFF;
        Anim_Set(o, 0, 0, 1, 0xA);
        U32(o + 0x54) = 0x101;
    }
}

/* sub-state 5: steps back */
void DC2CALL Allo_Back(gptr o)                                      /* 0x43AC70 */
{
    static const u32 anim[2] = {0xB, 0xC};
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 5);
        Bits_Set(AL_BITS(o), 1, 1);
    } else if (U8(o + 0x56) == 1) {
        if (!Anim_Step(o) && !Allo_FrameNear(o, 0x16, 0xBB8)) return;
        U32(o + 0x54) = 0x30001;
        Bits_Set(AL_BITS(o), 6, 0);
        stop(o);
    }
}

/* sub-state 6: roars */
void DC2CALL Allo_Roar(gptr o)                                      /* 0x43AD40 */
{
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 0x15, 0, 0x40, 2);
        Bits_Set(AL_BITS(o), 1, 1);
        Effect_Attach(o, 0x65C104);
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o)) { U32(o + 0x54) = 0x101; U16(o + 0x1FE) = 0x5A; }
        if (U16(o + 0x92) >= 0x2C && (DIST(o) < 0xBB8 || Bits_Check(AL_BITS(o), 3))) {
            U32(o + 0x54) = 0x30001; U16(o + 0x1FE) = 0x5A;
        }
    }
}

/* sub-state 7: comes in (the level's entry: from below, a leap) */
void DC2CALL Allo_Enter(gptr o)                                     /* 0x43AE00 */
{
    u32 c = U8(o + 0x18C) & 0xF;
    U8(o + 0x18D) = 0;
    U32(o + 0x50) &= ~0x10u;
    if (c == 4) {
        U32(o + 0x54) = 0x1000301;
        Bits_Set(AL_BITS(o), 9, 1);
    } else U32(o + 0x54) = 0x101;
}

/* sub-state 8: gets angry */
void DC2CALL Allo_Anger(gptr o)                                     /* 0x43AE60 */
{
    gptr b = AL_BITS(o);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 7, 0, 0x40, 5);
        Bits_Set(b, 1, 0);
        Bits_Set(b, 3, 1);
        U16(o + 0x204) = 0x1C2; U8(o + 0x206) = 0; U16(o + 0x1F8) = 1000;
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o)) Allo_Pick(o);
    }
}

/* sub-state 9: bites and shakes the player */
void DC2CALL Allo_Bite(gptr o)                                      /* 0x43AF00 */
{
    gptr pl = PL(), b = AL_BITS(o);
    u8 s = U8(o + 0x56);
    if ((s >= 2 && s <= 3) || (s == 4 && U16(o + 0x92) <= 0x28)) hold_player(o, pl);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 0xD, 0, 0x40, 5);
        Bits_Set(b, 1, 0);
        break;
    case 1:
        if (Anim_Step(o) || (Allo_FrameNear(o, 0x1B, 0xBB8) && Allo_Chance(o, 0x80)) || Allo_FrameFar(o, 0xA, 0x1388)) {
            Enemy_BiteOff(o);
            Allo_Pick(o);
            stop(o);
            break;
        }
        if (U16(o + 0x92) >= 5 && U16(o + 0x92) <= 0x18) { Bits_Set(b, 2, 1); U8(o + 0x17B) = 0; }
        if (U16(o + 0x92) > 0xD) U16(o + 0xF4) = Bits_Check(b, 3) ? U16(o + 0xD4) : (u16)(S16(o + 0xD4) >> 1);
        break;
    case 2:
        if (!Anim_Step(o)) break;
        U8(o + 0x56)++;
        Anim_Start(o, 0x10, 0, 0x41, 2);
        Effect_Attach(o, 0x65A734);
        Flash_Play(o, 0x65D908);
        U32(o + 0x1DC) = 0;
        Anim_Start(pl, PL_ANIM(o, 0x84), 0, 0x41, 2);
        Obj_SyncLinks(pl); Obj_UpdateMatrix(pl); Obj_CollideWalls(pl);
        Flag_Set(1, 1, 0);
        break;
    case 3:
        if (Anim_Step(o)) {
            hurt_player(pl, 0x28);
            Effect_Attach(o, 0x65A734);
            Flash_Play(o, 0x65D908);
        }
        U32(o + 0x1DC) += Pad_Shake();
        if (!U16(pl + 0xE8) && !U16(pl + 0xEA) && !U16(pl + 0xEC) && (s32)U32(o + 0x1DC) <= 0x14) break;
        if (U16(o + 0x92) > 5 && U16(o + 0x92) < 0x37) break;
        U8(o + 0x56)++;
        Anim_Start(o, 0x11, 0, 0x40, 0xA);
        Flash_Play(o, 0x65D918);
        Anim_Start(pl, PL_ANIM(o, 0x88), 0, 0x40, 0);
        Obj_SyncLinks(pl); Obj_UpdateMatrix(pl); Obj_CollideWalls(pl);
        Effect_Attach(pl, 0x65A828);
        U32(G() + 0x92C) = 0x43B2E0;
        break;
    case 4:
        if (!Anim_Step(o)) break;
        if (Rand8() & 1) { U8(o + 0x56)++; Anim_Start(o, 7, 0, 0x40, 5); break; }
        Enemy_BiteOff(o);
        U32(o + 0x54) = 0x30001;
        U32(o + 0x50) &= ~0x30u;
        break;
    case 5:
        if (!Anim_Step(o)) break;
        Enemy_BiteOff(o);
        U32(o + 0x54) = 1;
        U32(o + 0x50) &= ~0x30u;
        break;
    }
}

/* the player's state while thrown off the bite (G+0x92C) */
void DC2CALL Allo_Thrown(gptr pl)                                   /* 0x43B2E0 */
{
    if (Anim_Step(pl)) drop_player(pl);
    if (U16(pl + 0x92) == 0x2B) {
        U16(pl + 0x108) -= (u16)Damage_Scale(0x82);
        if (S16(pl + 0x108) < 0) GameOver_Start(0);
    }
}

/* sub-state 10: lunges with its jaws */
void DC2CALL Allo_Lunge(gptr o)                                     /* 0x43B3C0 */
{
    gptr b = AL_BITS(o);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0xE, 8, 0x40, 5);
        Bits_Set(b, 1, 0);
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o)) {
            Enemy_BiteOff(o);
            if (Allo_Chance(o, 0x80)) Allo_Pick(o);
            else U32(o + 0x54) = 0x101;
            return;
        }
        if (Allo_FrameFar(o, 0x11, 0x1770)) { Enemy_BiteOff(o); Allo_Pick(o); return; }
        if (U16(o + 0x92) >= 3 && U16(o + 0x92) <= 0x18) { Bits_Set(b, 2, 1); U8(o + 0x17B) = 0; }
        if (Bits_Check(b, 3)) Value_Approach(o + 0xF4, 0x32, 0xE6);
        else Value_Approach(o + 0xF4, 0x28, 0xBE);
    }
}

/* sub-state 11: snaps upward (the player above) */
void DC2CALL Allo_SnapUp(gptr o)                                    /* 0x43B4E0 */
{
    gptr b = AL_BITS(o);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 0x17, 0, 0x40, 4);
        Bits_Set(b, 1, 0);
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o) || Allo_FrameFar(o, 0xD, 0xDAC)) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; return; }
        if (U16(o + 0x92) >= 7 && U16(o + 0x92) <= 0x15) { Bits_Set(b, 2, 1); U8(o + 0x17B) = 0; }
    }
}

/* sub-state 12: a short snap */
void DC2CALL Allo_Snap(gptr o)                                      /* 0x43B5A0 */
{
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 0xA, 0, 0x40, 5);
        Bits_Set(AL_BITS(o), 1, 0);
    } else if (U8(o + 0x56) == 1) {
        if (!Anim_Step(o)) return;
        Enemy_BiteOff(o);
        if (Allo_Chance(o, 0x80)) Allo_Pick(o);
        else U32(o + 0x54) = 0x101;
    }
}

/* sub-state 13: picks the player up (when down) and throws */
void DC2CALL Allo_Toss(gptr o)                                      /* 0x43B640 */
{
    gptr pl = PL(), b = AL_BITS(o);
    u8 s = U8(o + 0x56);
    if ((s == 2 && U16(o + 0x92) >= 0x10) || (s == 3 && U16(o + 0x92) <= 0x3E)) hold_player(o, pl);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 7, 0, 0x40, 5);
        stop(o);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        if (!(U8(pl + 0x174) & 2)) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; break; }
        U8(o + 0x56)++;
        U32(o + 0x50) &= ~8u;
        stop(o);
        Anim_Start(o, 0x12, 0, 0x40, 0xA);
        Bits_Set(b, 1, 0);
        Effect_Attach(o, 0x65A9E4);
        Flash_Play(o, 0x65D92C);
        U8(o + 0x206)++;
        Player_Hold(o, 0, 0x48E100);
        break;
    case 2:
        Anim_Step(o);
        if (U16(o + 0x92) < 0x10) break;
        U8(o + 0x56)++;
        Anim_Start(pl, PL_ANIM(o, 0x8C), 0, 0x40, 0);
        Effect_Attach(pl, 0x65AB00);
        U32(pl + 0x50) |= 0x120;
        Flag_Set(1, 1, 1);
        place_player(o, pl);
        break;
    case 3:
        if (Anim_Step(o)) {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 0x101;
            drop_player(pl);
        }
        if (U16(pl + 0x92) == 0x33) {
            hurt_player(pl, 0xFA);
            Player_Bleed(0x1F4);
        }
        break;
    }
}

/* sub-state 14: a sidestep (o+0x57 the side) */
void DC2CALL Allo_Sidestep(gptr o)                                  /* 0x43B910 */
{
    static const u32 anim[2] = {0x13, 0x14};
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, anim[U8(o + 0x57)], 0, 0x40, 4);
        Bits_Set(AL_BITS(o), 1, 0);
    } else if (U8(o + 0x56) == 1) {
        if (!Anim_Step(o)) return;
        Enemy_BiteOff(o);
        U32(o + 0x54) = 0x30001;
    }
}

/* sub-state 15: a quick run */
void DC2CALL Allo_Dash(gptr o)                                      /* 0x43B9B0 */
{
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++; stop(o);
        Anim_Start(o, 2, 0, 1, 0xA);
        Bits_Set(AL_BITS(o), 1, 0);
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o)) U32(o + 0x54) = 0x30001;
    }
}

/* ---- the attacks land (Allo_Grab, by the sub-state) ---- */

/* sub-state 9: the bite catches the player (unless the flag 2:0x1A) */
void DC2CALL Allo_BiteHit(gptr o)                                   /* 0x43BA30 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    if (Flag_Check(2, 0x1A)) {
        s32 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
        Effect_Attach(o, 0x65A36C);
        Flash_Play(o, 0x65D8E8);
        if (iabs(d) < 0x400) Player_Hurt(o, 0x82, 2, 0x1E);
        else Player_Hurt(o, 0x82, 1, 0);
        return;
    }
    U8(o + 0x56)++;
    Anim_Start(o, 0xF, 1, 0x40, 3);
    U32(o + 0x50) &= ~8u;
    stop(o);
    Effect_Attach(o, 0x65A4B0);
    Flash_Play(o, 0x65D8F8);
    U8(o + 0x206)++;
    Player_Hold(o, 0x82, 0x48E100);
    Anim_Set(pl, PL_ANIM(o, 0x80), 1, 0x40, 3);
    Player_Bleed(0x1F4);
    U32(pl + 0x50) |= 0x120;
    Flag_Set(1, 1, 1);
    place_player(o, pl);
    Se_Play(o, 0x3D);
}

/* sub-state 10 */
void DC2CALL Allo_LungeHit(gptr o)                                  /* 0x43BBC0 */
{
    if (U8(o + 0x56) != 1) return;
    gptr pl = PL();
    s32 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Effect_Attach(o, 0x65A36C);
    Flash_Play(o, 0x65D8E8);
    if (iabs(d) < 0x400) Player_Hurt(o, 0xFA, 2, 0x1E);
    else Player_Hurt(o, 0xC8, 1, 0);
}

/* sub-state 11 */
void DC2CALL Allo_SnapUpHit(gptr o)                                 /* 0x43BC40 */
{
    if (U8(o + 0x56) != 1) return;
    gptr pl = PL();
    s32 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Effect_Attach(o, 0x65AD0C);
    Flash_Play(o, 0x65D968);
    Player_Hurt(o, iabs(d) < 0x400 ? 0xFA : 0xC8, 1, 0);
}

/* sub-state 12 */
void DC2CALL Allo_SnapHit(gptr o)                                   /* 0x43BCC0 */
{
    if (U8(o + 0x56) != 1) return;
    gptr pl = PL();
    s32 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Flash_Play(o, 0x65D8D8);
    if (iabs(d) < 0x400) Player_Hurt(o, 0xDC, 2, 0x1E);
    else Player_Hurt(o, 0xB4, 1, 0);
}

/* sub-state 14 */
void DC2CALL Allo_SideHit(gptr o)                                   /* 0x43BD30 */
{
    if (U8(o + 0x56) != 1) return;
    gptr pl = PL();
    s32 d = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Se_Play(o, 0x44);
    Flash_Play(o, 0x65D948);
    if (iabs(d) < 0x400) Player_Hurt(o, 0xDC, 2, 0x1E);
    else Player_Hurt(o, 0xB4, 1, 0);
}

/* sub-state 3: landing on the player */
void DC2CALL Allo_LeapHit(gptr o)                                   /* 0x43BDB0 */
{
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, 0x65D978);
    Player_Hurt(o, 0xDC, 2, 0x1E);
}

/* ---- hit (state 2) ---- */

static u16 fifth(u16 v) { return (u16)-(s32)(v / 5); }

/* sub-state 0: the damage taken; staggers, flinches or dies, then the state */
void DC2CALL Allo_Damage(gptr o)                                    /* 0x43BE00 */
{
    static const u32 eff_a[2] = {0x65B05C, 0x65B330}, eff_b[2] = {0x65CCAC, 0x65CEE0};
    static const u32 burn[2] = {0x65D114, 0x65D2F8}, splash[4] = {0x65D5A8, 0x65D64C, 0x65D6F0, 0x65D7E4};
    gptr pl = PL(), b = AL_BITS(o);
    s32 hp0 = S16(o + 0x108);
    if (U8(o + 0x184) & 8) U16(o + 0x176) += U16(o + 0x176) >> 1;
    if ((s8)U8(o + 0x17C) <= 1) U16(o + 0x176) += U16(o + 0x176) >> 1;
    if (Bits_Check(b, 3)) U16(o + 0x176) >>= 2;
    u8 how = U8(o + 0x17D);
    if (how == 4) { U16(o + 0x172) = 4; U16(o + 0x176) = U16(o + 0x176) / 3; }
    Bits_Set(b, 7, how == 1 || how == 2 || how == 0x10 || how == 0x11);
    u32 last = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
    U32(o + 0x54) = last;
    if ((s8)U8(o + 0x17C) <= 1) {
        s16 a = (s16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
        U32(o + 0x54) = a <= 0 ? 0x202 : 0x1000202;
        if (U16(o + 0x5A) != U16(pl + 0x5A)) U32(o + 0x54) = 0x302;
    }
    if (Bits_Check(b, 2) && !(U8(o + 0x17D) >= 0x10 && U8(o + 0x17D) <= 0x11)) {
        U32(o + 0x54) = 0x302;
        U8(o + 0x11D) -= 5;
        if (S8(o + 0x11D) < -50) U8(o + 0x11D) = 0xCE;
    }
    if (U16(o + 0x172) == 3) U32(o + 0x54) = 0x102;
    if (Bits_Check(b, 3) && U32(o + 0x1C4) != 1) {                /* anger wears off */
        if (U8(o + 0x54) == 2 && !U8(o + 0x56)) U16(o + 0x1F8) -= U16(o + 0x176);
        else U16(o + 0x1F8) += fifth(U16(o + 0x176));
        if (S16(o + 0x1F8) <= 0) Bits_Set(b, 3, 0);
        else U32(o + 0x54) = U8(o + 0x1C8) | U8(o + 0x1C9) << 8 | U8(o + 0x1CA) << 16 | (u32)U8(o + 0x1CB) << 24;
    }
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {                   /* staggers: full damage */
        Player_Release(o);
        Enemy_BiteOff(o);
        Bits_Set(b, 1, 0); Bits_Set(b, 3, 0); Bits_Set(b, 6, 0); Bits_Set(b, 2, 0); Bits_Set(b, 9, 0);
        stop(o);
        U16(o + 0x204) = 0;
        U32(o + 0x1CC) = 0;
        if (U16(o + 0x1E6)) {
            u16 t = U16(o + 0x1E6);
            U16(o + 0x1E6) = 0;
            U16(o + 0x4A) = (U16(o + 0x4A) + t) & 0xFFF;
        }
        if (U32(o + 0x1C4) == 1 && U8(o + 0x17D) == 0x1C) Boss_Flash(o, 1);
        if (U8(o + 0x184) & 8) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 0x14 : 0xA;
        if (S16(o + 0x108) <= 0) {
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = (u32)U8(o + 0x57) << 24 | (u32)U8(o + 0x55) << 8 | 3;
            Score_Kill(o);
            Bits_Set(b, 3, 0);
            if (U32(o + 0x1C4) == 1) {
                if (Bits_Check(SHARED, 1)) U32(o + 0x54) = (u32)U8(o + 0x57) << 24 | 0x203;
                Bits_Set(SHARED, 1, 1);
            }
        } else if (!U32(o + 0x1C4)) Score_Hit(o, (u32)(hp0 - S16(o + 0x108)) / 5);
    } else {                                                    /* takes a fifth */
        if ((s8)U8(o + 0x17C) <= 1) {
            s16 a = (s16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
            u32 side = a > 0;
            Effect_Attach(o, U16(o + 0x172) >= 2 && U16(o + 0x172) <= 3 ? eff_a[side] : eff_b[side]);
            if (U8(o + 0x170) & 2) Prop_Attach(o, burn[side]);
        } else if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65D4B4);
        else Effect_Attach(o, splash[Rand8() & 3]);
        U16(o + 0x108) += fifth(U16(o + 0x176));
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 0x14 : 0xA;
        if (S16(o + 0x108) < 0) {
            Player_Release(o);
            Enemy_BiteOff(o);
            U32(o + 0x50) &= ~8u;
            U16(o + 0x108) = 0xFFFF;
            U32(o + 0x54) = 0x103;
            Bits_Set(b, 3, 0);
            Score_Kill(o);
        }
    }
    Orig_Call1(U32(AL_MAIN + U8(o + 0x54) * 4), o);
}

/* sub-state 1: flinches (the next one in the shared bit 2 later) */
void DC2CALL Allo_Flinch(gptr o)                                    /* 0x43C3C0 */
{
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 0x1A, Bits_Check(SHARED, 2) ? 4 : 0, 0x40, 2);
        Bits_Set(SHARED, 2, 1);
        Effect_Attach(o, 0x65ADD8);
    } else if (U8(o + 0x56) == 1) {
        if (!Anim_Step(o)) return;
        Allo_Pick(o);
        Bits_Set(SHARED, 2, 0);
    }
}

/* sub-state 2: staggers aside (o+0x57) */
void DC2CALL Allo_Stagger(gptr o)                                   /* 0x43C450 */
{
    static const u32 anim[2] = {0x1B, 0x1C}, eff_a[2] = {0x65B05C, 0x65B330}, eff_b[2] = {0x65CCAC, 0x65CEE0};
    static const u32 burn[2] = {0x65D114, 0x65D2F8};
    gptr b = AL_BITS(o);
    u32 k = U8(o + 0x57);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, anim[k], 0, 0x40, 2);
        Effect_Attach(o, U16(o + 0x172) >= 2 && U16(o + 0x172) <= 3 ? eff_a[k] : eff_b[k]);
        if (U8(o + 0x170) & 2) Prop_Attach(o, burn[k]);
        Bits_Set(b, 5, 0);
        U32(o + 0x1D0) += U16(o + 0x176);
        if ((s32)U32(o + 0x1D0) > 1000) {
            U32(o + 0x1D0) = 0;
            Bits_Set(b, 5, 1);
            U8(o + 0x17B) = 0x3C;
        }
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o) || (U16(o + 0x92) == 0x1E && Bits_Check(b, 7)) || (U16(o + 0x92) == 0x28 && Bits_Check(b, 5)))
            Allo_Pick(o);
        if (U16(o + 0x92) < 0xC) Obj_TurnToPoint(o, 0x6E, o + 0xD8);
    }
}

/* sub-state 3: falls; gets up to bite when it can */
void DC2CALL Allo_Fall(gptr o)                                      /* 0x43C600 */
{
    gptr b = AL_BITS(o);
    if (U8(o + 0x56) == 0) {
        U8(o + 0x56)++;
        Anim_Start(o, 3, 0, 0x40, 2);
        Effect_Attach(o, 0x65C478);
        Bits_Set(b, 4, 0);
        if ((s32)U32(o + 0x1D4) > 0 && U8(G() + 0x108E)) { Bits_Set(b, 4, 1); U8(o + 0x17B) = 0x50; }
        U32(o + 0x1D4) = 0xB4;
    } else if (U8(o + 0x56) == 1) {
        if (Anim_Step(o) || (U16(o + 0x92) == 0x47 && Bits_Check(b, 7))) Allo_Pick(o);
        if (U16(o + 0x92) == 0x46 && Bits_Check(b, 4)) { U32(o + 0x54) = 0x801; U8(o + 0x17B) = 0; }
    }
}

/* ---- dies (state 3) ---- */

/* the last frames: a key item drops, it goes */
static void die_end(gptr o, u32 frame)
{
    if (U16(o + 0x92) != frame) return;
    Prop_SpawnC(U32(o + 0x1C4) == 1 ? 0 : 1, o + 0x40, 2, 0x3C, 1, 0);
    U32(o + 0x50) |= 0x30;
}

static void die_fade(gptr o)
{
    U32(o + 0x50) |= 0x30;
    if (U32(o + 0x1C4) && U32(o + 0x1C4) <= 2) Obj_DeathFade(o);
}

/* sub-states 1, 3: falls forward */
void DC2CALL Allo_DieA(gptr o)                                      /* 0x43C700 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x1A, 0, 0x40, 2);
        Effect_Attach(o, 0x65ADD8);
        U32(o + 0x50) &= ~8u;
        stop(o);
        Bits_Set(AL_BITS(o), 1, 0);
        Flag_Set(4, U8(o + 0x11C), 1);
        U8(o + 0x118) &= 0xDF;
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) < 0xF) break;
        U8(o + 0x56)++;
        Anim_Start(o, 0x1D, 0, 0x40, 2);
        Effect_Attach(o, 0x65B62C);
        break;
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; Layers_Assign(); }
        if (U16(o + 0x92) == 0x30) Shake_Set(4, 0, 0x28);
        if (U16(o + 0x92) >= 0x26 && U16(o + 0x92) <= 0x4E) U16(o + 0x42) += 6;
        die_end(o, 0x56);
        break;
    case 3: die_fade(o); break;
    }
}

/* sub-state 2: falls aside (o+0x57) */
void DC2CALL Allo_DieB(gptr o)                                      /* 0x43C890 */
{
    static const u32 anim[2] = {0x1B, 0x1C}, eff_a[2] = {0x65B05C, 0x65B330}, eff_b[2] = {0x65CCAC, 0x65CEE0};
    static const u32 burn[2] = {0x65D114, 0x65D2F8}, anim2[2] = {0x1E, 0x1F}, eff2[2] = {0x65B9F0, 0x65BCC4};
    u32 k = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, anim[k], 0, 0x40, 2);
        U32(o + 0x50) &= ~8u;
        stop(o);
        Bits_Set(AL_BITS(o), 1, 0);
        U8(o + 0x118) &= 0xDF;
        Effect_Attach(o, U16(o + 0x172) >= 2 && U16(o + 0x172) <= 3 ? eff_a[k] : eff_b[k]);
        if (U8(o + 0x170) & 2) Prop_Attach(o, burn[k]);
        Flag_Set(4, U8(o + 0x11C), 1);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) < 0xA) break;
        U8(o + 0x56)++;
        Anim_Start(o, anim2[k], 0, 0x40, 2);
        Effect_Attach(o, eff2[k]);
        break;
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; Layers_Assign(); }
        if (U16(o + 0x92) == 0x1B) Shake_Set(4, 0, 0x28);
        if (U16(o + 0x92) >= 0x11 && U16(o + 0x92) <= 0x39) U16(o + 0x42) += 6;
        die_end(o, 0x34);
        break;
    case 3: die_fade(o); break;
    }
    if (U8(o + 0x170) & 2) Allo_Burn(o);
}

/* ---- helpers ---- */

/* which of the player's animations (with the other weapon: 4 on) */
u32 DC2CALL Allo_PlAnim(gptr o)                                     /* 0x43CAF0 */
{
    return U8(o + 0x210) ? 4 : 0;
}

/* the wait before it bites again: set while the player is hurt */
u32 DC2CALL Allo_Timer(gptr o)                                      /* 0x43CB10 */
{
    gptr pl = PL();
    if (U32(o + 0x1D8)) U32(o + 0x1D8)--;
    if (U8(pl + 0x54) == 2 || U8(pl + 0x54) == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D8) = Bits_Check(AL_BITS(o), 3) ? 0x1E : 0x3C;
        if (!U8(G() + 0x108E)) U32(o + 0x1D8) <<= 1;
    }
    return U32(o + 0x1D8);
}

/* the anger's tint (o+0x202 up to 30 while angry) on all its parts */
void DC2CALL Allo_Tint(gptr o)                                      /* 0x43CB90 */
{
    if (Bits_Check(AL_BITS(o), 3)) { if (S16(o + 0x202) < 0x1E) U16(o + 0x202)++; }
    else if (U16(o + 0x202)) U16(o + 0x202)--;
    if (U16(o + 0x202))
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {
            s32 t = S16(o + 0x202);
            U8(p + 0x8C) = (u8)(t * 0x10 / 30 + 0x80);
            U8(p + 0x8D) = (u8)(t * -0x10 / 30 + 0x80);
            U8(p + 0x8E) = (u8)(t * -8 / 30 + 0x80);
        }
    if (U16(o + 0x204)) U16(o + 0x204)--;
}

/* turns aside: to the side with the farther wall, sometimes the other way */
void DC2CALL Allo_Pick(gptr o)                                      /* 0x43CCB0 */
{
    u32 l = Wall_Dist(o, 0x400, 0x40), r = Wall_Dist(o, (u32)-0x400, 0x40);
    U32(o + 0x54) = l > r ? 0x501 : 0x1000501;
    if (Rand8() < 0x40) U8(o + 0x57) = !U8(o + 0x57);
}

/* by the frame the player got away (far or aside); half the time unless angry */
u32 DC2CALL Allo_FrameFar(gptr o, u32 frame, u32 dist)              /* 0x43CD00 */
{
    if ((Rand8() & 1) && !Bits_Check(AL_BITS(o), 3)) return 0;
    s32 a = ang_to(o);
    if (U16(o + 0x92) >= (u16)frame) return 0;
    return DIST(o) > dist || iabs(a) > 0x100;
}

/* from the frame on, the player near (or it angry) */
u32 DC2CALL Allo_FrameNear(gptr o, u32 frame, u32 dist)             /* 0x43CD80 */
{
    if (U16(o + 0x92) < (u16)frame) return 0;
    return DIST(o) < dist || Bits_Check(AL_BITS(o), 3);
}

/* angry, or a chance of n in 256 */
u32 DC2CALL Allo_Chance(gptr o, u32 n)                              /* 0x43CDC0 */
{
    if (Bits_Check(AL_BITS(o), 3)) return 1;
    return Rand8() < (u8)n;
}

/* burning: seven parts darken, down to their least */
void DC2CALL Allo_Burn(gptr o)                                      /* 0x43CDF0 */
{
    static const u8 part[7] = {0, 1, 0x11, 9, 0xA, 0xD, 0xE};
    static const u8 least[7] = {0x30, 0x40, 0x50, 0x40, 0x70, 0x40, 0x70};
    for (u32 k = 0; k < 7; k++) {
        gptr p = Obj_Part(U32(o + 0x64), part[k]);
        for (u32 c = 0x8C; c < 0x8F; c++) {
            u8 v = (u8)(U8(p + c) - 0x10);
            U8(p + c) = v < least[k] ? least[k] : v;
        }
    }
}
