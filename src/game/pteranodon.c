/* The pteranodon (object type handler 0x429030, type 4, E20.DAT). It keeps
 * its state at o+0x1C0.., a set of bits first (Bits_Set(o+0x1C0, n)):
 * 1 moving, 2 turns away as it starts flying again.
 * +0xC..0xF the state of the last frame, +0x10 a timer, +0x14 the wait
 * before it may carry the player off again, +0x1E bank (turn a frame),
 * +0x22 swoop angle, +0x24/+0x26 timers, +0x28 a frame count, +0x2C the
 * swoop's length, +0x30 the carried player's offset, +0x36 the wait after
 * the player's shot, +0x3E the height it flies at, +0x40 its first height.
 * States (o+0x54): 0 init, 1 normal (think 0x72074C, then move 0x72076C by
 * o+0x55), 2 hit (0x72078C), 3 die (0x72079C), 4 (0x4A56D0). Attacks land
 * through 0x7207AC. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define PT_MAIN  0x720708u      /* state handlers */
#define PT_THINK 0x72074Cu      /* state 1: deciding, by o+0x55 */
#define PT_MOVE  0x72076Cu      /* state 1: moving, by o+0x55 */
#define PT_HIT   0x72078Cu      /* state 2 */
#define PT_DIE   0x72079Cu      /* state 3 */
#define PT_GRAB  0x7207ACu      /* an attack landed, by o+0x55 */
#define PT_BITS(o) ((o) + 0x1C0)
#define SHARED   0x87E974u      /* bits shared: 0 one holds the player, 1 keep off (the player shot) */
#define PL()     U32(G() + 0x940)

static s32 iabs(s32 v) { return v < 0 ? -v : v; }
static void stop(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static u16 y_level(gptr o) { return (u16)((s32)S16(o + 0x42) / -1000); }
static void approach(gptr p, u32 step, u32 target) { U16(p) += (u16)Angle_Approach(U16(p), step, target); }

void DC2CALL Ptera_Type(gptr o)                                     /* 0x429030 */
{
    Ptera_Handler(o);
}

void DC2CALL Ptera_Handler(gptr o)                                  /* 0x429040 */
{
    gptr g = G(), pl = U32(g + 0x940);
    U8(o + 0x206) = U8(g + 0x108F) ? 1 : 0;
    Obj_SavePos(o);
    U16(o + 0x10E) = (u16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
    Orig_Call1(U32(PT_MAIN + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (U8(pl + 0x54) == 1 && U8(pl + 0x55) == 6) {     /* the player shoots: keep off a while */
        u32 r = U16(G() + 0x10B4);
        if (r == 3 || (r >= 8 && r <= 9)) { Bits_Set(SHARED, 1, 1); U16(o + 0x1F6) = 0x3C; }
    }
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    u16 f = U16(o + 0x10E);
    if (S16(o + 0x42) > (s16)f) U16(o + 0x42) = f;
    if (S16(o + 0x42) < -0x3A98) U16(o + 0x42) = (u16)-0x3A98;
    U16(o + 0x1FE) = (u16)(f - 0x1388);                 /* 5000 over the floor, no lower than over the player */
    if ((s32)(s16)(f - 0x1388) > (s32)S16(pl + 0x42) - 0x1388) U16(o + 0x1FE) = (u16)(U16(pl + 0x42) - 0x1388);
    U8(o + 0x118) |= 0x80;
    s16 h = (s16)Floor_Height(o + 0x40, o + 0x40, 0x40, 0);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x118) & 0xF);
    s16 hp = (s16)Floor_Height(p + 0x40, p + 0x40, 0x40, 0);
    if (U8(G() + 0x1091) == 1 && U8(G() + 0x1090) == 0xC && h >= 0) U8(o + 0x118) &= 0x7F;
    if (hp > h) U8(o + 0x118) &= 0x7F;
    U16(o + 0x5A) = y_level(o);
    if (U16(pl + 0x1FA) > 0x80) Bits_Set(SHARED, 0, 0);
    Ptera_Timer(o);
    if (U32(o + 0x1D0)) U32(o + 0x1D0)--;
    if (U16(o + 0x1F6)) U16(o + 0x1F6)--;
    else Bits_Set(SHARED, 1, 0);
    Ptera_Grab(o);
    U8(o + 0x1CE) = U8(o + 0x56); U8(o + 0x1CC) = U8(o + 0x54);
    U8(o + 0x1CD) = U8(o + 0x55); U8(o + 0x1CF) = U8(o + 0x57);
    if (U32(o + 0x50) & 0x40000000) Obj_Disable(o);
}

/* state 0: health, model, animations */
void DC2CALL Ptera_Init(gptr o)                                     /* 0x429290 */
{
    gptr pl = PL();
    U32(o + 0x50) = 0xB;
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x4B0;
    if (U8(G() + 0x108E) == 2) U16(o + 0x108) = 0x960;
    if (!U8(G() + 0x108E)) S16(o + 0x108) >>= 1;
    U16(o + 0x10A) = U16(o + 0x108);
    U32(o + 0x12C) = 0x720720; U32(o + 0x128) = 0x720730; U32(o + 0x134) = 0x720740;
    stop(o);
    U16(o + 0x5A) = y_level(o);
    U32(o + 0x188) = 0;
    Model_Setup(o, 0x640000);
    U32(o + 0x88) = 0x6425DC;
    Anim_Start(o, 0, 0, 1, 5);
    Obj_Set118(o, 0, 0x1FA0, 0x1FA0);
    DrawSync(o);
    U8(o + 0x15F) = 2; U8(o + 0x160) = 1;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0x80; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = 0;
    U8(o + 0x15E) = U8(0x87E747) ? 0 : 7;
    Bits_Set(PT_BITS(o), 1, 1);
    U32(o + 0x54) = 1;
    U32(0x74AF10) = 0;
    U16(o + 0x1FE) = U16(o + 0x42); U16(o + 0x200) = U16(o + 0x42);
    U8(o + 0x118) |= 0x20;
    U32(pl + 0x50) |= 0x80;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

void DC2CALL Ptera_Normal(gptr o)                                   /* 0x429430 */
{
    Orig_Call1(U32(PT_THINK + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(PT_MOVE + U8(o + 0x55) * 4), o);
}

void DC2CALL Ptera_Hit(gptr o)                                      /* 0x429460 */
{
    Orig_Call1(U32(PT_HIT + U8(o + 0x55) * 4), o);
}

void DC2CALL Ptera_Die(gptr o)                                      /* 0x429480 */
{
    Orig_Call1(U32(PT_DIE + U8(o + 0x55) * 4), o);
}

/* its attacks hit the player: by the sub-state */
void DC2CALL Ptera_Grab(gptr o)                                     /* 0x4294A0 */
{
    Enemy_AttackHit(o);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(PT_GRAB + U8(o + 0x55) * 4), o);
}

/* the wait before it may carry the player again: kept up while the player is hurt or held */
u32 DC2CALL Ptera_Timer(gptr o)                                     /* 0x4338F0 */
{
    gptr pl = PL();
    if (U32(o + 0x1D4)) U32(o + 0x1D4)--;
    u8 s = U8(pl + 0x54);
    if (s == 2 || s == 5 || (U8(pl + 0x174) & 1)) {
        U32(o + 0x1D4) = 0x3C;
        if (!U8(G() + 0x108E)) U32(o + 0x1D4) = 0x78;
    }
    return U32(o + 0x1D4);
}

/* out: o's position plus v turned by its heading, pushed out by its distance to the player; the player's height */
void DC2CALL Ptera_HoldPoint(gptr o, gptr out, gptr v)              /* 0x433860 */
{
    gptr pl = PL();
    s32 d = SquareRoot0((s32)Dist2_XZ(o + 0x40, pl + 0x40));
    gptr t = Tmp_Push(8);
    U16(t) = U16(v); U16(t + 2) = U16(v + 2); U16(t + 4) = (u16)(U16(v + 4) + d);
    Vec_RotY(t, t, U16(o + 0x4A));
    U16(out) = (u16)(U16(t) + U16(o + 0x40));
    U16(out + 2) = (u16)(U16(pl + 0x42) + U16(t + 2));
    U16(out + 4) = (u16)(U16(o + 0x44) + U16(t + 4));
    Tmp_Pop(8);
}

/* ---- deciding (state 1, before the move) ---- */

/* sub-state 0: circling; near and facing the player (or where the running player will be): swoop */
void DC2CALL Ptera_ThinkFly(gptr o)                                 /* 0x4294D0 */
{
    gptr pl = PL();
    if (!U8(o + 0x56)) return;
    gptr v = Tmp_Push(8);
    if (U8(pl + 0x54) == 1 && U8(pl + 0x55) == 2 && !U16(pl + 0xE8) && !U16(pl + 0xEA) && !U16(pl + 0xEC)) {
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x1F40;
        Vec_RotY(v, v, U16(pl + 0x4A));
        U16(v) += U16(pl + 0x40); U16(v + 2) += U16(pl + 0x42); U16(v + 4) += U16(pl + 0x44);
    } else {
        U16(v) = U16(pl + 0x40); U16(v + 2) = U16(pl + 0x42); U16(v + 4) = U16(pl + 0x44);
    }
    u32 d2 = Dist2_XZ(o + 0x40, v);
    s16 a = (s16)Angle_DiffTo(o + 0x40, v, U16(o + 0x4A));
    Tmp_Pop(8);
    if (d2 < 0x3D09000 && d2 && !U16(o + 0x1F6) && iabs(a) < 0x400) { U32(o + 0x54) = 0x201; return; }
    if (U16(o + 0x1E4)) { U16(o + 0x1E4)--; return; }
    if (iabs(a) < 0x60 && !U16(o + 0xE8) && !U16(o + 0xEA) && !U16(o + 0xEC)) U8(o + 0x57) = 0;
    else if (a < 0) { if (U8(o + 0x57) != 2) U16(o + 0x1E6) = 100; U8(o + 0x57) = 2; }
    else { if (U8(o + 0x57) != 1) U16(o + 0x1E6) = 100; U8(o + 0x57) = 1; }
    if (!U16(o + 0x1E6)) {
        if (U8(o + 0x57)) { U8(o + 0x57) = 0; U16(o + 0x1E4) = 0x1E; U16(o + 0x1E6) = 100; }
    } else U16(o + 0x1E6)--;
    if (Rand8() < 5 && U8(o + 0x56) == 1) U8(o + 0x56) = 2;
}

/* ---- moving (state 1) ---- */

/* its height to o+0x1FE, banking by the turn o+0x57 (0 straight, 1 right, 2 left) */
void DC2CALL Ptera_Steer(gptr o)                                    /* 0x4297D0 */
{
    Value_Approach(o + 0x42, 0x14, U16(o + 0x1FE));
    switch (U8(o + 0x57)) {
    case 0: approach(o + 0x1DE, 5, 0); approach(o + 0x4C, 0x14, 0); break;
    case 1: approach(o + 0x1DE, 5, 0x28); approach(o + 0x4C, 0x14, 0x200); break;
    case 2: approach(o + 0x1DE, 5, 0xFFD8); approach(o + 0x4C, 0x14, 0xFE00); break;
    }
    U16(o + 0x4A) += U16(o + 0x1DE);
}

/* sub-state 0: flies about (a flap now and then) */
void DC2CALL Ptera_Fly(gptr o)                                      /* 0x4296A0 */
{
    gptr b = PT_BITS(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0, 0, 9, 0xA);
        U16(o + 0x1E4) = 0; U16(o + 0x48) = 0; U16(o + 0x4C) = 0;
        U16(o + 0xF0) = 0; U16(o + 0xF2) = 0; U16(o + 0xF4) = 0x226;
        U32(o + 0x50) &= ~0x30u;
        if (Bits_Check(b, 2)) {
            U8(o + 0x57) = Rand8() & 1 ? 1 : 2;
            U16(o + 0x1E4) = 10; U16(o + 0x1E6) = 100;
            Bits_Set(b, 2, 0);
        }
        break;
    case 1: Anim_Step(o); break;
    case 2: U8(o + 0x56)++; Anim_Start(o, 2, 0, 0, 0xA); break;
    case 3: if (Anim_Step(o)) { U8(o + 0x56) = 1; Anim_Start(o, 0, 0, 9, 0xA); } break;
    }
    Ptera_Steer(o);
}

/* sub-state 1: rises back to its height after a swoop */
void DC2CALL Ptera_Rise(gptr o)                                     /* 0x429890 */
{
    approach(o + 0x48, 0x28, 0);
    approach(o + 0x4C, 0x28, 0);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 6, 0, 1, 0xA);
        U16(o + 0x1E8) = 0;
        break;
    case 1:
        Anim_Step(o);
        if (++S16(o + 0x1E8) > 5) U8(o + 0x56)++;
        break;
    case 2:
        Anim_Step(o);
        Value_Approach(o + 0xF0, 0x32, 0); Value_Approach(o + 0xF2, 0x14, (u32)-300); Value_Approach(o + 0xF4, 0x14, 0x226);
        if (S16(o + 0x42) < S16(o + 0x1FE) + 0x5DC && (U16(o + 0x92) == 0x12 || U16(o + 0x92) == 0x26)) U8(o + 0x56)++;
        break;
    case 3:
        Anim_Step(o);
        Value_Approach(o + 0xF0, 0x32, 0); Value_Approach(o + 0xF2, 0x28, (u32)-100); Value_Approach(o + 0xF4, 0x28, 0x226);
        if (S16(o + 0x42) < S16(o + 0x1FE)) { U32(o + 0x54) = 1; Bits_Set(PT_BITS(o), 2, 1); }
        break;
    }
}

/* sub-state 2 (deciding): the swoop, along an arc about the player; grabs at the end */
void DC2CALL Ptera_ThinkSwoop(gptr o)                               /* 0x429A10 */
{
    gptr pl = PL();
    if (!U8(o + 0x56)) return;
    u32 d = (u32)SquareRoot0((s32)Dist2_XZ(o + 0x40, pl + 0x40));
    Obj_TurnToPoint(o, 0x50, pl + 0x40);
    if (Bits_Check(SHARED, 1)) { U32(o + 0x54) = 0x101; return; }
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0;
    U16(v + 4) = (u16)(U8(pl + 0x174) & 2 ? d / 30 : d / 80);
    Vec_RotY(v, v, (u16)(Angle_XZ(o + 0x40, pl + 0x40) + U16(o + 0x1E2)));
    U16(o + 0x44) += U16(v + 4); U16(o + 0x40) += U16(v); U16(o + 0x42) += U16(v + 2);
    Tmp_Pop(8);
    if (U8(o + 0x56) != 3) return;
    if (d > 0x1770) { U32(o + 0x54) = 0x101; return; }
    if (S16(o + 0x1E4) > S16(o + 0x1EC) && !(U8(pl + 0x174) & 2)) { U32(o + 0x54) = 0x101; return; }
    s16 a = (s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A));
    s16 h = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    if (iabs(a) < 0x60 && !Bits_Check(SHARED, 0) && U16(pl + 0x1FA) > Rand8()) {
        if (!U32(o + 0x1D4)) { Enemy_BiteOn(o); U32(o + 0x54) = 0x301; return; }
        if ((U8(pl + 0x174) & 2) && !Flag_Check(2, 0x1A) && iabs(h) < 0x10) {   /* from behind: carries the player off */
            Enemy_BiteOn(o);
            U32(o + 0x50) |= 0x30;
            U32(o + 0x54) = 0x601;
            U16(pl + 0x174) &= 0xFFFD;
            U32(o + 0x50) &= ~8u;
            Player_Hold(o, 0, 0x48E100);
            gptr w = Tmp_Push(8);
            U16(w) = 0; U16(w + 2) = 0; U16(w + 4) = 0xFD44;
            Ptera_HoldPoint(o, o + 0x1F0, w);
            Tmp_Pop(8);
            return;
        }
    }
    U16(o + 0x1E4)++;
}

/* sub-state 2: the swoop down at the player */
void DC2CALL Ptera_Swoop(gptr o)                                    /* 0x429C50 */
{
    gptr pl = PL();
    approach(o + 0x48, 0x28, 0);
    approach(o + 0x4C, 0x28, 0);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0xD, 0, 0x40, 0xA);
        U16(o + 0x1E4) = 0; U16(o + 0x1F2) = U16(pl + 0x42); U16(o + 0x1E6) = 0; U16(o + 0x1FC) = 0x1E;
        U16(o + 0x1EC) = (u16)((Rand8() & 0x3F) + 0x96);
        U16(o + 0x1E2) = (s16)Angle_DiffTo(pl + 0x40, o + 0x40, U16(pl + 0x4A)) < 0 ? 0x400 : 0xFC00;
        break;
    case 1:
        if (Anim_Step(o)) { U16(o + 0xF0) = 0; U16(o + 0xF2) = 0x14B; U16(o + 0xF4) = 0xF1; }
        if (U16(o + 0x1FC)) U16(o + 0x1FC)--;
        if (S16(o + 0x42) < S16(o + 0x1F2) - 0xBB8 && !U16(o + 0xE8) && !U16(o + 0xEA) && !U16(o + 0xEC) && U16(o + 0x1FC)) break;
        U8(o + 0x56)++;
        stop(o);
        Anim_Start(o, 0xE, 0, 0x40, 0xA);
        break;
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; stop(o); Anim_Start(o, 0xF, 0, 0x41, 0xA); }
        break;
    case 3: Anim_Step(o); break;
    }
}

/* sub-state 3 (deciding): perched near the player; leaves when far, shot at or at random */
void DC2CALL Ptera_ThinkPerch(gptr o)                               /* 0x429E40 */
{
    gptr pl = PL();
    if (!U8(o + 0x56)) return;
    Obj_TurnToPoint(o, 0x50, pl + 0x40);
    if (iabs((s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A))) < 0x400 && U16(pl + 0x1FA) > Rand8()) U16(o + 0x1F8) = 0x10;
    if (Bits_Check(SHARED, 1) && Rand8() < 0x46) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
    if (U8(o + 0x56) == 1 && U16(pl + 0x1FA) < Rand8()) {
        u32 d2 = Dist2_XZ(o + 0x40, pl + 0x40);
        if (d2 > 0x2255100 || d2 < 0xF4240) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
    }
}

/* sub-state 3: perched; at the frame o+0x1F8 it attacks (the kind by where the player stands) */
void DC2CALL Ptera_Perch(gptr o)                                    /* 0x429F20 */
{
    gptr pl = PL();
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x10, 0, 0x40, 0xA);
        U16(o + 0x1F8) = 0x26;
        break;
    case 1: {
        Anim_Step(o);
        if ((s32)U16(o + 0x92) < S16(o + 0x1F8)) break;
        gptr v = Tmp_Push(8);
        U16(v) = 0; U16(v + 2) = 0xFD44; U16(v + 4) = 0;
        Ptera_HoldPoint(o, o + 0x1F0, v);
        Tmp_Pop(8);
        if (iabs((s16)Angle_DiffTo(o + 0x40, pl + 0x40, U16(o + 0x4A))) >= 0x60) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; break; }
        if (Dist2_XZ(o + 0x40, o + 0x1F0) >= 0xF42400) { U32(o + 0x54) = 0x501; break; }
        if (iabs((s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A))) > 0x400 && !Flag_Check(2, 0x1A)) U32(o + 0x54) = 0x701;
        else U32(o + 0x54) = 0x401;
        break;
    }
    }
}

/* toward o+0x1F0 at speed sp; true when it reached the player's height (or hit something, or ran out of time) */
static int dive(gptr o, u32 sp)
{
    gptr v = Tmp_Push(8);
    Vec_Toward(o + 0x40, o + 0x1F0, sp, v);
    Value_Approach(o + 0xF0, 0x64, U32(v));
    Value_Approach(o + 0xF2, 0x64, U32(v + 2));
    Value_Approach(o + 0xF4, 0x64, U32(v + 4));
    Tmp_Pop(8);
    if (U16(o + 0x1FC)) U16(o + 0x1FC)--;
    return S16(o + 0x1F2) - 0x64 < S16(o + 0x42) || U16(o + 0xE8) || U16(o + 0xEA) || U16(o + 0xEC) || !U16(o + 0x1FC);
}

static void level(gptr o) { approach(o + 0x48, 0x28, 0); approach(o + 0x4C, 0x28, 0); }

/* sub-state 4: dives at the player and pecks */
void DC2CALL Ptera_Peck(gptr o)                                     /* 0x42A070 */
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 8, 0, 0, 5);
        U16(o + 0x1FC) = 0x1E;
        Se_Play(o, 0x36);
        U32(o + 0x50) |= 0x20;
        break;
    case 1:
        Anim_Step(o);
        if (dive(o, 0x1C2)) {
            U8(o + 0x56)++;
            Anim_Start(o, 0xC, 0, 0, 5);
            U16(o + 0xF4) = 0x226;
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    case 2:
        if (Anim_Step(o)) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
        if (U16(o + 0x92) <= 4) U16(o + 0xF2) += 0xFF9C;
        else U16(o + 0xF2) = 0xFF38;
        break;
    }
}

/* sub-state 5: dives at the player, a pass with the claws */
void DC2CALL Ptera_Claw(gptr o)                                     /* 0x42A210 */
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0, 0, 0, 5);
        U16(o + 0x1FC) = 0x1E;
        Se_Play(o, 0x36);
        U32(o + 0x50) |= 0x20;
        U16(o + 0x1E8) = 0;
        break;
    case 1:
        Anim_Step(o);
        if (dive(o, 0x28A)) { U8(o + 0x56)++; U16(o + 0x1E8) = 0; U32(o + 0x50) &= ~0x30u; }
        break;
    case 2: {
        Anim_Step(o);
        s16 c = S16(o + 0x1E8);
        if (c <= 4) U16(o + 0xF2) += 0xFF9C;
        else U16(o + 0xF2) = 0xFF38;
        if (c == 4) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
        U16(o + 0x1E8)++;
        break;
    }
    }
}

/* the player's animation set: 7 on the second difficulty set */
u32 DC2CALL Ptera_PlAnim(gptr o)                                    /* 0x42B6F0 */
{
    return U8(G() + 0x108F) ? 7 : 0;
}

#define PL_ANIM(o, off) U32(U32((o) + 0x88) + Ptera_PlAnim(o) * 4 + (off))

/* sub-state 6: carries the player up and drops it (the player shakes to get loose) */
void DC2CALL Ptera_Carry(gptr o)                                    /* 0x42A3B0 */
{
    gptr pl = PL();
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 1, 0, 0, 0xA);
        U16(o + 0x1FC) = 0x1E;
        Se_Play(o, 0x36);
        U32(o + 0x50) |= 0x20;
        U16(o + 0x1E8) = 0; U16(o + 0x1EA) = 0;
        break;
    case 1: {
        Anim_Step(o);
        gptr v = Tmp_Push(8);
        Vec_Toward(o + 0x40, o + 0x1F0, 0x28A, v);
        Value_Approach(o + 0xF0, 0x64, U32(v));
        Value_Approach(o + 0xF2, 0x64, U32(v + 2));
        Value_Approach(o + 0xF4, 0x64, U32(v + 4));
        if (S16(o + 0x1F2) - 0x578 < S16(o + 0x42)) {    /* down at the player: takes it */
            U8(o + 0x56)++;
            stop(o);
            Anim_Start(o, 9, 0, 0x40, 0xA);
            Flash_Play(o, 0x65D91C);
            U16(v) = 0; U16(v + 2) = 0xFC2C; U16(v + 4) = 0xFA88;
            Vec_RotY(v, v, U16(pl + 0x4A));
            U16(o + 0x40) = (u16)(U16(pl + 0x40) + U16(v));
            U16(o + 0x42) = (u16)(U16(pl + 0x42) + U16(v + 2));
            U16(o + 0x44) = (u16)(U16(pl + 0x44) + U16(v + 4));
            U16(pl + 0x4A) = U16(o + 0x4A);
        }
        Tmp_Pop(8);
        break;
    }
    case 2:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            stop(o);
            Anim_Start(o, 0xA, 0, 0x41, 5);
            Flash_Play(o, 0x65D92C);
            Anim_Start(pl, PL_ANIM(o, 0x70), 0, 0x41, 5);
        }
        if (U16(o + 0x92) == 0x13) Anim_Start(pl, PL_ANIM(o, 0x6C), 0, 0x40, 0xA);
        break;
    case 3:
        if (Anim_Step(o)) Flash_Play(o, 0x65D92C);
        U16(o + 0x1EA) += (u16)Pad_Shake();
        if ((U16(o + 0x1E8) == 0x28 && S16(o + 0x1EA) > 0x14) || U16(o + 0x1E8) == 0x50) {   /* shaken loose, or dropped */
            U8(o + 0x56)++;
            Anim_Start(o, 0xB, 0, 0x40, 0xA);
            if (U16(o + 0x1E8) == 0x50) {
                U8(pl + 0x57) = 1;
                Anim_Start(pl, PL_ANIM(o, 0x78), 0, 0x40, 0xA);
                Flash_Play(pl, 0x65D954);
            } else {
                U8(pl + 0x57) = 0;
                Anim_Start(pl, PL_ANIM(o, 0x74), 0, 0x40, 0xA);
                Flash_Play(pl, 0x65D940);
            }
            U32(G() + 0x92C) = 0x42A730;
        }
        U16(o + 0x1E8)++;
        break;
    case 4:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x16) {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 0x101;
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    }
}

/* the player's state while it falls (G+0x92C): lands, hurt by the height (o+0x57: dropped high) */
void DC2CALL Ptera_Dropped(gptr pl)                                 /* 0x42A730 */
{
    if (Anim_Step(pl) && S16(pl + 0x108) >= 0) {
        U32(pl + 0x50) &= ~0x30u;
        U32(pl + 0x54) = 0x30202;
        U16(pl + 0x42) = (u16)(U16(pl + 0x5A) * 0xFC18);
        Anim_Start(pl, 0xC, 0, 0, 0xA);
    }
    u16 at = U8(pl + 0x57) ? 0x24 : 0x20;
    if (U16(pl + 0x92) == at) U16(pl + 0x108) -= (u16)Damage_Scale(U8(pl + 0x57) ? 0xC8 : 0x96);
    if (U16(pl + 0x92) >= at) U16(pl + 0x42) = (u16)(U16(pl + 0x5A) * 0xFC18);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
    Player_Bleed(0x1F4);
}

static void hurt_player(gptr pl, u32 d)
{
    U16(pl + 0x108) -= (u16)Damage_Scale(d);
    if (S16(pl + 0x108) < 0) GameOver_Start(0);
}

/* sub-state 7: comes down on the player and pins it (the player shakes to get loose) */
void DC2CALL Ptera_Pin(gptr o)                                      /* 0x42A810 */
{
    gptr pl = PL();
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x17, 0, 0x40, 5);
        U32(o + 0x50) |= 0x20;
        U16(o + 0x1FC) = 0x1E;
        U16(o + 0x1EA) = 0;
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            stop(o);
            Anim_Start(o, 0x18, 0, 0, 0);
            Se_Play(o, 0x36);
        }
        break;
    case 2:
        Anim_Step(o);
        if (dive(o, 0x28A)) {                               /* missed: lands */
            U8(o + 0x56) = 6;
            stop(o);
            Anim_Start(o, 0x13, 0, 0x40, 0);
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    case 3:                                                 /* on the player (Ptera_PinHit) */
        U16(o + 0x40) += U16(pl + 0xE8); U16(o + 0x42) += U16(pl + 0xEA); U16(o + 0x44) += U16(pl + 0xEC);
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            stop(o);
            Anim_Start(o, 0x11, 0, 0x41, 0);
            Anim_Start(pl, PL_ANIM(o, 0x7C), 0, 0x41, 0);
            Se_Play(pl, 3);
            Effect_Attach(o, 0x65C924);
            Flash_Play(o, 0x65D8E0);
            hurt_player(pl, 0x28);
            Player_Bleed(0x1F4);
        }
        break;
    case 4:
        if (Anim_Step(o)) {
            Effect_Attach(o, 0x65C924);
            Flash_Play(o, 0x65D8E0);
            hurt_player(pl, 0x28);
        }
        U16(o + 0x1EA) += (u16)Pad_Shake();
        if ((U16(o + 0x1E8) == 0x1E && S16(o + 0x1EA) > 0xF) || U16(o + 0x1E8) == 0x3C) {   /* shaken off, or lets go */
            U8(o + 0x56)++;
            Anim_Start(o, 0x12, 0, 0x40, 0);
            Anim_Start(pl, PL_ANIM(o, 0x80), 0, 0x40, 0);
            Flash_Play(o, 0x65D8F4);
            U32(G() + 0x92C) = 0x42AC00;
        }
        U16(o + 0x1E8)++;
        break;
    case 5:
        U32(o + 0x50) &= ~0x10u;
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2C) {
            Enemy_BiteOff(o);
            U32(o + 0x54) = 0x101;
            U32(o + 0x50) &= ~0x30u;
        }
        break;
    case 6:
        Anim_Step(o);
        if (U16(o + 0x92) > 0xA) { Enemy_BiteOff(o); U32(o + 0x54) = 0x101; }
        break;
    }
}

/* the player's state when let go (G+0x92C): gets up */
void DC2CALL Ptera_Released(gptr pl)                                /* 0x42AC00 */
{
    if (Anim_Step(pl)) {
        Anim_Set(pl, 0x16, 0, 1, 0xA);
        Player_AnimReset();
    }
}

/* ---- its attacks land (by the sub-state) ---- */

static void strike(gptr o, gptr fx, gptr fl)
{
    gptr pl = PL();
    if (U8(o + 0x56) != 1) return;
    s16 h = (s16)Angle_Diff(U16(o + 0x4A), U16(pl + 0x4A));
    Effect_Attach(o, fx);
    Flash_Play(o, fl);
    if (iabs(h) < 0x400) Player_Hurt(o, 0xA0, 2, 0x28);     /* from behind: knocked down */
    else Player_Hurt(o, 0x78, 1, 0);
}

void DC2CALL Ptera_PeckHit(gptr o)                                  /* 0x42AC30 */
{
    strike(o, 0x65C4E0, 0x65D8D0);
}

void DC2CALL Ptera_ClawHit(gptr o)                                  /* 0x42ACB0 */
{
    strike(o, 0x65C69C, 0x65D8C0);
}

/* the dive at the player lands: pins it, facing away */
void DC2CALL Ptera_PinHit(gptr o)                                   /* 0x42AD30 */
{
    gptr pl = PL();
    if (U8(o + 0x56) != 2) return;
    U8(o + 0x56)++;
    stop(o);
    Anim_Start(o, 0x14, 0, 0x40, 0);
    Effect_Attach(o, 0x65C880);
    Flash_Play(o, 0x65D90C);
    U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x10;
    U16(pl + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0xFAEC; U16(v + 4) = 0x320;
    Vec_RotY(v, v, U16(pl + 0x4A));
    U16(o + 0x40) = (u16)(U16(pl + 0x40) + U16(v));
    U16(o + 0x42) = (u16)(U16(pl + 0x42) + U16(v + 2));
    U16(o + 0x44) = (u16)(U16(pl + 0x44) + U16(v + 4));
    Tmp_Pop(8);
    Player_Hold(o, 0x78, 0x48E100);
    Anim_Start(pl, PL_ANIM(o, 0x84), 0, 0x40, 0);
    U32(pl + 0x50) &= ~0x10u;
}

/* ---- hit (state 2) ---- */

/* sub-state 0: the damage; a strong or a heavy hit knocks it about, else back to what it did */
void DC2CALL Ptera_Damage(gptr o)                                   /* 0x42AE50 */
{
    static const u32 fx[3] = {0x65D4A0, 0x65D51C, 0x65D5E8};
    if (!(U8(o + 0x1CC) == 1 && !U8(o + 0x1CD)) && (U8(o + 0x184) & 8)) U16(o + 0x176) += U16(o + 0x176) >> 1;
    u16 k = U16(o + 0x172);
    if (k == 4) U16(o + 0x176) <<= 1;
    U32(o + 0x54) = (u32)U8(o + 0x1CF) << 24 | (u32)U8(o + 0x1CE) << 16 | (u32)U8(o + 0x1CD) << 8 | U8(o + 0x1CC);
    u32 dmg = U16(o + 0x176);
    U32(o + 0x1D0) += dmg;
    if ((s32)U32(o + 0x1D0) > S16(o + 0x10A) / 2) { U32(o + 0x54) = 0x102; U32(o + 0x1D0) = 0; }
    if (k >= 2) U32(o + 0x54) = 0x202;
    if (k == 4) U32(o + 0x54) = 0x302;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        Enemy_BiteOff(o);
        stop(o);
        U16(o + 0x1C8) = 0; U16(o + 0x1FA) = 0;
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65D6B4);
        if (!(U8(o + 0x1CC) == 1 && !U8(o + 0x1CD)) && (U8(o + 0x184) & 8)) Score_Event(o);
        U16(o + 0x108) -= U16(o + 0x176);
        U8(o + 0x17B) = U16(o + 0x172) == 4 ? 5 : 10;
        if (S16(o + 0x108) < 0) { U16(o + 0x108) = 0xFFFF; U32(o + 0x50) &= ~8u; }
    } else {                                                /* a light hit: a fifth of it */
        U16(o + 0x108) += (u16)((s32)dmg / -5);
        U8(o + 0x17B) = k == 4 ? 5 : 10;
        Effect_Attach(o, fx[Rand8() % 3]);
        if (U8(o + 0x170) & 2) Prop_Attach(o, 0x65D6B4);
        if (S16(o + 0x108) < 0) U16(o + 0x108) = 1;
    }
    Orig_Call1(U32(PT_MAIN + U8(o + 0x54) * 4), o);
}

/* knocked about, turned to the hit (o+0x178) or away; dead: on to state 3 */
static void knocked(gptr o, u32 af, gptr ff, u32 ab, gptr fb, u16 dead_at, u32 dead, u16 up_at, u16 f2, u16 f4)
{
    level(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (iabs((s16)Angle_Diff(U16(o + 0x178), U16(o + 0x4A))) < 0x400) {
            U16(o + 0x4A) = U16(o + 0x178);
            Anim_Start(o, af, 0, 0x40, 0);
            Effect_Attach(o, ff);
        } else {
            U16(o + 0x4A) = (U16(o + 0x178) - 0x800) & 0xFFF;
            Anim_Start(o, ab, 0, 0x40, 0);
            Effect_Attach(o, fb);
        }
        break;
    case 1:
        Anim_Step(o);
        if (S16(o + 0x108) < 0) { if (U16(o + 0x92) >= dead_at) U32(o + 0x54) = dead; }
        else if (U16(o + 0x92) >= up_at) { U32(o + 0x54) = 0x101; U16(o + 0xF0) = 0; U16(o + 0xF2) = f2; U16(o + 0xF4) = f4; }
        break;
    }
}

void DC2CALL Ptera_Knock(gptr o)                                    /* 0x42B0A0 */
{
    knocked(o, 0x19, 0x65D088, 0x15, 0x65CB80, 0x1E, 0x103, 0x1E, 0xFFC7, 0xA1);
}

void DC2CALL Ptera_KnockHard(gptr o)                                /* 0x42B1D0 */
{
    knocked(o, 0x1A, 0x65D1CC, 0x16, 0x65CD14, 0xF, 0x203, 0x2C, 0xFFA1, 0xBE);
}

void DC2CALL Ptera_KnockHeavy(gptr o)                               /* 0x42B300 */
{
    knocked(o, 0x19, 0x65D088, 0x15, 0x65CB80, 0x1E, 0x103, 0x1E, 0xFFBE, 0xD5);
}

/* ---- die (state 3) ---- */

/* falls (into water: a splash, gone), lies, fades */
void DC2CALL Ptera_Dying(gptr o)                                    /* 0x42B440 */
{
    level(o);
    Obj_DeathFade(o);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 3, 0, 0, 5);
        U32(o + 0x50) = (U32(o + 0x50) & ~8u) | 0x20;
        U16(o + 0xF4) = U16(o + 0xD4);
        Score_Kill(o);
        break;
    case 1: {
        if (Anim_Step(o)) Anim_Start(o, 7, 0, 1, 5);
        Value_Approach(o + 0xF0, 0x32, 0);
        U16(o + 0xF2) += 0x32;
        gptr g = G();
        u8 st = U8(g + 0x1091), rm = U8(g + 0x1090);
        if (st == 1 && rm == 0xC && S16(o + 0x40) < (s16)0xE0C0) {     /* off the cliff: gone */
            s16 lim = S16(o + 0x44) > -0x4C7 ? -0x1194 : -0x1964;
            if (S16(o + 0x42) >= lim) { U32(o + 0x50) |= 0x40000000; break; }
        }
        if ((!st && rm == 1) || (st == 6 && rm == 2)) {     /* rooms with water */
            s16 w = (s16)Water_Level(o + 0x40);
            if (!U16(o + 0x10E) && (S16(o + 0x42) >= w || S16(o + 0x42) >= 0)) {
                if (w == 0x7FFF) w = 0;
                gptr k = K2_AllocBack();
                if (k) {
                    U8(k + 0x58) = 0x10; U16(k + 0x6C) = 0x50; U16(k + 0x6E) = 0x46;
                    U16(k + 0x40) = U16(o + 0x40); U16(k + 0x42) = (u16)w; U16(k + 0x44) = U16(o + 0x44);
                }
                Se_Play(o, 0x40);
                U32(o + 0x50) |= 0x40000000;
                break;
            }
        }
        if (S16(o + 0x42) >= S16(o + 0x10E)) {              /* on the floor */
            U8(o + 0x56)++;
            Anim_Start(o, 4, 1, 0, 5);
            U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18);
            U32(o + 0x50) |= 0x10;
            stop(o);
            Se_Play(o, 0x3D);
        }
        break;
    }
    case 2:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 5, 0, 0, 5); }
        break;
    case 3:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Layers_Assign();
            Prop_SpawnC(0, o + 0x40, 5, 0x32, 1, 0);
        }
        break;
    }
}
