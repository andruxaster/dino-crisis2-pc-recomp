/* The tyrannosaurus of the extra game's duel (object types 35 and 36;
 * handlers 0x459700 and 0x45B430). The two copies of the code are the same
 * but for the player they belong to: their pads, the other player's object,
 * the variables, tables and model (copy 2's data sits 0x2A530 lower).
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the pad picks the move,
 * then the move runs), 2 hit (0..7 hurt, 8/9 flinch, 10 staggers), 3 die
 * (8..10).
 * Moves: 0 stand, 1 walk, 2 turn to the other player, 3 walk on, 4 run (its
 * charge: it hurts what it runs into), 5 back, 6 the roar (its super move:
 * the screen goes dark), 7 a roar, 8 a long roar (not picked by the pad),
 * 9..11 the attacks (bite, the head swing, tail; o+0x57 the side).
 * Hits: the damage taken lately (v+0x18, v+0x1C, falling by 3 and 2 a frame)
 * decides: over 0xA0 a flinch, over 0x12C the longer one; else it shrugs the
 * hit off and goes on with what it was doing.
 * Variables (v): +0 the pad's new presses, +8 bits (1 moving, 2 attacking),
 * +0x10 the hurt kind, +0x12 o+0x54..0x57 of the last frame, +0x18/+0x1C
 * the damage taken lately, +0x20 a third such count (falling by 1 a frame;
 * nothing adds to it here), +0x44 the long roar's time, +0x4C the point
 * turned to, +0x54 the run's time without harm, +0x58 the other player,
 * +0x5C the pad held, +0x5E the angle from the head to the other player. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
    u32 d;              /* how much lower its data is than copy 1's */
} Krex;

static const Krex K1 = {0x74B218, 0x7227B8, 0x662500, 2, 0x87DC94, 0x87DC08, 0};
static const Krex K2 = {0x74B278, 0x722960, 0x638000, 1, 0x87DC92, 0x87DE0A, 0x2A530};

/* tables (t+) */
#define T_COMBO 0x70u
#define T_MAIN  0x80u
#define T_PICK  0x90u
#define T_MOVE  0xC0u
#define T_HIT   0xF0u
#define T_DIE   0x11Cu
#define T_LAND  0x148u
#define T_DMG   0x178u
#define T_REACT 0x190u

#define BITS(k) ((k)->v + 8)
#define HELD(k) U16((k)->v + 0x5C)
#define PAD(k)  U8((k)->v)
#define FOE(k)  U32((k)->v + 0x58)
/* data in the model file, by copy 1's address */
#define D(k, a) ((gptr)((a) - (k)->d))

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

/* the other player's kinds it does not push against (o+0x50 0x20); and
 * those its run does not hurt: by object type 3..0x26 */
static const u8 no_push[0x24] = {0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
static const u8 run_hurts[0x24] = {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};

static void attack(const Krex *k, gptr o);

static s32 fall_to(s32 x, s32 by) { return x ? x - by : x; }

static void handler(const Krex *k, gptr o)
{
    gptr v = k->v;
    U16(v + 0x5C) = U16(k->held);
    gptr foe = U32(0x87E760) + k->foe * 0x260;
    U16(v) = U16(k->pad);
    U32(v + 0x58) = foe;
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(FOE(k) + 0x40);
    U16(o + 0xDA) = U16(FOE(k) + 0x42);
    U16(o + 0xDC) = U16(FOE(k) + 0x44);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(v + 0x5E) = p ? (u16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    s32 aa = S16(v + 0x5E); if (aa < 0) aa = -aa;
    if (aa < 0x500 && FOE(k) != o && Bits_Check(BITS(k), 1)) U32(o + 0x158) = 2;
    else U32(o + 0x158) = 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    u32 ft = (u32)U8(FOE(k) + 0x58) - 3;
    if (ft <= 0x23 && !no_push[ft]) U32(o + 0x50) &= ~0x20u;
    else U32(o + 0x50) |= 0x20;
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    attack(k, o);
    s32 a = fall_to(S32(v + 0x18), 3), b = fall_to(S32(v + 0x1C), 2), c = fall_to(S32(v + 0x20), 1);
    S32(v + 0x18) = a < 0 ? 0 : a;
    S32(v + 0x1C) = b < 0 ? 0 : b;
    S32(v + 0x20) = c < 0 ? 0 : c;
    if (U8(v + 0x54)) U8(v + 0x54)--;
    U8(v + 0x12) = U8(o + 0x54); U8(v + 0x13) = U8(o + 0x55);
    U8(v + 0x14) = U8(o + 0x56); U8(v + 0x15) = U8(o + 0x57);
}

/* model, health 2000, head, the other player as the head's target */
static void init(const Krex *k, gptr o)
{
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x7D0; U16(o + 0x10A) = 0x7D0;
    U32(o + 0x88) = D(k, 0x665184);
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x30; U32(o + 0x134) = k->t + 0x40;
    zero_move(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x17, 0, 1, 0);
    Obj_Set118(o, 0, 0x4E20, 0x7530);
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0x80; U16(o + 0x16E) = 0x300;
    U8(o + 0x15D) = U8(FOE(k) + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    Bits_Set(BITS(k), 1, 0);
    U8(o + 0x118) |= 0x20;
    U8(o + 0xFE) = 3;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the roar by the button sequence (at half health or less), then the pad's
 * choice and the move */
static void normal(const Krex *k, gptr o)
{
    U16(o + 0x174) = 0;
    if (Combo_Input(k->t + T_COMBO, U16(k->v))
        && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && U8(o + 0x55) != 6) {
        U32(o + 0x54) = 0x601;
        Kof_Super(o);
    }
    call(k->t + T_PICK, o, U8(o + 0x55));
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

/* its attack spheres against the other player: the run hurts only kinds
 * it can run into, and not again for a while; then the landing */
static void attack(const Krex *k, gptr o)
{
    u32 s = U8(o + 0x55);
    u16 dmg = U16(k->t + T_DMG + s * 2);
    if (s == 4 && U8(k->v + 0x54)) dmg = 0;
    u32 ft = (u32)U8(FOE(k) + 0x58) - 3;
    if (ft <= 0x23 && !run_hurts[ft] && s == 4) dmg = 0;
    Kof_Attack(o, k->foe, dmg, U16(k->t + T_REACT + s * 2), s == 4 ? 0x23 : 0x1F);
    if (S8(o + 0x119) >= 0) call(k->t + T_LAND, o, U8(o + 0x55));
}

/* o+0x54 as a dword: state 1, move m, the side (the head's angle) */
static u32 side(const Krex *k) { return S16(k->v + 0x5E) <= 0; }
#define MOVE(m) (0x01u | (m) << 8)
#define SIDE(m) (side(k) << 24 | MOVE(m))

static void steer(const Krex *k, gptr o, u16 by)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += by;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= by;
}

/* the bite (0x20), the tail (0x10) */
static int pick_attack(const Krex *k, gptr o, u32 h)
{
    if (h & 0x20) { U32(o + 0x54) = SIDE(9); return 1; }
    if (h & 0x10) { U32(o + 0x54) = SIDE(11); return 1; }
    return 0;
}

/* standing: 8 pressed (or held with 4) turns to the other player, 0x1000
 * with 0x40 and 0x20 the head swing, the attacks, 0x1000 walk on (with 0x40
 * run), 0x4000 back, 0x2000/0x8000 walk, 1 roar */
static void pick_stand(const Krex *k, gptr o)
{
    u32 h = HELD(k), p = PAD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(2);
    else if ((h & 0x1000) && (h & 0x40) && (h & 0x20)) U32(o + 0x54) = MOVE(10);
    else if (pick_attack(k, o, h)) ;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(1);
    else if (h & 0x8000) U32(o + 0x54) = MOVE(1);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
}

/* walking (the head swing needs 0x20 pressed here) */
static void pick_walk(const Krex *k, gptr o)
{
    u32 h = HELD(k), p = PAD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if ((h & 0x1000) && (h & 0x40) && (p & 0x20)) U32(o + 0x54) = MOVE(10);
    else if (pick_attack(k, o, h)) ;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o, 0x28);
}

/* walking on (0x1000): let go, 8 turns to the other player */
static void pick_on(const Krex *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x1000)) U32(o + 0x54) = MOVE(0);
    else if ((h & 0x40) && (h & 0x20)) U32(o + 0x54) = MOVE(10);
    else if (!(h & 0x40) && (h & 0x20)) U32(o + 0x54) = SIDE(9);
    else if (h & 0x10) U32(o + 0x54) = SIDE(11);
    else if (h & 0x40) U32(o + 0x54) = MOVE(4);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o, 0x28);
}

/* running (0x1000 and 0x40): 0x20 the head swing, no bite */
static void pick_run(const Krex *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x1000)) U32(o + 0x54) = MOVE(0);
    else if (!(h & 0x40)) U32(o + 0x54) = MOVE(3);
    else if (h & 0x20) U32(o + 0x54) = MOVE(10);
    else if (h & 0x10) U32(o + 0x54) = SIDE(11);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o, 0x28);
}

static void pick_back(const Krex *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x4000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x4000)) U32(o + 0x54) = MOVE(0);
    else if (pick_attack(k, o, h)) ;
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o, 0x28);
}

/* its two standing animations, by the animation's flag */
static void mv_stand(const Krex *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, Anim_FrameFlag(o) ? 5 : 0x17, 0, 1, 0x14);
        break;
    case 1: Anim_Step(o); break;
    }
}

static void mv_walk(const Krex *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 0, 0, 1, 0xA);
        Bits_Set(BITS(k), 1, 1);
        break;
    case 1: Anim_Step(o); break;
    }
}

/* turns to where the other player was (0x64 a frame, 20 frames at most) */
static void mv_turn(const Krex *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        if (FOE(k) == o) { U32(o + 0x54) = MOVE(0); return; }
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 0, 0, 1, 0xA);
        Bits_Set(BITS(k), 1, 1);
        U16(v + 0x4C) = U16(o + 0xD8); U16(v + 0x4E) = U16(o + 0xDA); U16(v + 0x50) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, v + 0x4C);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x64, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = MOVE(0);
        break;
    }
    }
}

/* a heavy step: the ground shakes */
static void stomp(const Krex *k, gptr o)
{
    Shake_Set(2, 0, 0x1E);
    Flash_Play(o, D(k, 0x679204));
}

/* walking on, running and back: from the step it is on (by the animation's
 * flag) the speed comes up to its top; the footfalls shake the ground (with
 * their sounds) */
static void mv_go(const Krex *k, gptr o, u32 anim, u32 fr0, u32 fr1, s32 top, u32 f0, u32 f1, int run)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, anim, Anim_FrameFlag(o) ? fr1 : fr0, 1, 0x14);
        if (run) U8(k->v + 0x54) = 0x1E;
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, (u32)top);
        break;
    }
    if (U16(o + 0x92) == f0) { stomp(k, o); Se_Play(o, 0x30); }
    if (U16(o + 0x92) == f1) { stomp(k, o); Se_Play(o, 0x31); }
}

/* the super move: a roar that darkens the screen */
static void mv_super(const Krex *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, 3, 0, 0x40, 0x14);
        Flash_Play(o, D(k, 0x679214));
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = MOVE(0);
        if (U16(o + 0x92) == 0x38) {
            Shake_Set(3, 0, 0x1C);
            U8(G() + 0xB58) = 0x80; U8(G() + 0xB59) = 0x80; U8(G() + 0xB5A) = 0x80;
            U8(G() + 0xB5B) = 0; U8(G() + 0xB5C) = 0;
        }
        if (U16(o + 0x92) >= 0x38) DrawSync(0);
        break;
    }
}

/* a roar */
static void mv_roar(const Krex *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, 4, 0, 0x40, 0x14);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = MOVE(0);
        break;
    }
}

/* the long roar (by the animation's flag): starts, holds 120 frames, ends,
 * then walks on */
static void mv_long_roar(const Krex *k, gptr o)
{
    static const u32 a0[2] = {0xF, 0x12}, a1[2] = {0x10, 0x13}, a2[2] = {0x11, 0x14};
    static const u32 fx[2] = {0x678394, 0x6785C8};
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        u8 s = (u8)Anim_FrameFlag(o);
        U8(o + 0x57) = s;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, a0[s], 0, 0x40, 0x14);
        Effect_Attach(o, D(k, fx[U8(o + 0x57)]));
        Flash_Play(o, D(k, 0x679284));
        U16(k->v + 0x44) = 0x78;
        U8(o + 0x17B) = 0xA;
        break;
    }
    case 1:
        U8(o + 0x17B) = 0xA;
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            Anim_Start(o, a1[U8(o + 0x57)], 0, 0x41, 0xA);
        }
        if (U16(o + 0x92) == 0x10) Shake_Set(5, 0, 0x1E);
        break;
    case 2:
        Anim_Step(o);
        if (!U16(k->v + 0x44) || !--U16(k->v + 0x44)) {
            U8(o + 0x56)++;
            Anim_Start(o, a2[U8(o + 0x57)], 0, 0x40, 0xA);
        }
        break;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = MOVE(3);
        break;
    }
}

/* an attack (o+0x57 the side): its animation to its end frame */
static void mv_attack(const Krex *k, gptr o, const u32 *anim, u32 frame, u32 blend, u32 end, int flag)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (flag) U8(o + 0x57) = (u8)Anim_FrameFlag(o);
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], frame, 0x40, blend);
        U32(0x87E4A4)++;
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) U32(o + 0x54) = MOVE(0);
        break;
    }
}

static void mv_bite(const Krex *k, gptr o)
{
    static const u32 anim[3] = {8, 9, 0xA};
    mv_attack(k, o, anim, 4, 0x14, 0x3C, 0);
    u16 f = U16(o + 0x92);
    if (f == 0xD || f == 0x3C) stomp(k, o);
}

static void mv_swing(const Krex *k, gptr o)
{
    static const u32 anim[2] = {0xE, 0xD};
    mv_attack(k, o, anim, 8, 0xA, 0x3C, 1);
    u16 f = U16(o + 0x92);
    if (f == 0x12 || f == 0x32) stomp(k, o);
}

static void mv_tail(const Krex *k, gptr o)
{
    static const u32 anim[2] = {0xC, 0xB};
    mv_attack(k, o, anim, 1, 0x14, 0x26, 0);
}

/* an attack landing: on its first frame a flash and an effect by the side */
static void land(const Krex *k, gptr o, u32 flash, const u32 *fx)
{
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, D(k, flash));
    if (fx) Effect_Attach(o, D(k, fx[U8(o + 0x57)]));
}

static void land_bite(const Krex *k, gptr o)
{
    static const u32 fx[3] = {0x6762D0, 0x6763EC, 0x676508};
    land(k, o, 0x679250, fx);
}

static void land_swing(const Krex *k, gptr o)
{
    static const u32 fx[2] = {0x676D3C, 0x676ED0};
    land(k, o, 0x679270, fx);
}

static void land_tail(const Krex *k, gptr o)
{
    static const u32 fx[2] = {0x67669C, 0x6769EC};
    land(k, o, 0x679260, fx);
}

/* hit: the hurt kind by o+0x55; more damage from a hit in the back; the
 * damage taken lately decides: a flinch (0x802), the longer one (0x902),
 * kind 3 staggers (0xA02), else it goes on with the last frame's state
 * (with a spurt of blood); health below 0: dies */
static void hurt(const Krex *k, gptr o)
{
    gptr v = k->v;
    static const s8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    static const u32 blood[3] = {0x677454, 0x6774D0, 0x677574};
    if (U8(o + 0x55) < 8) U16(v + 0x10) = (u16)kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    U32(o + 0x54) = (u32)U8(v + 0x15) << 24 | (u32)U8(v + 0x14) << 16 | (u32)U8(v + 0x13) << 8 | U8(v + 0x12);
    S32(v + 0x18) += U16(o + 0x176);
    S32(v + 0x1C) += U16(o + 0x176);
    if (S32(v + 0x18) > 0xA0) {
        S32(v + 0x18) = 0;
        U32(o + 0x54) = 0x802;
        if (S32(v + 0x1C) > 0x12C) {
            S32(v + 0x1C) = 0;
            U32(o + 0x54) = 0x902;
        }
    }
    if (U16(v + 0x10) == 3) U32(o + 0x54) = 0xA02;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        U32(o + 0x50) |= 8;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U8(o + 0x17B) = 0xA;
    }
    else {
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U8(o + 0x17B) = 0xA;
        Effect_Attach(o, D(k, blood[Rand8() % 3]));
    }
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x50) &= ~8u;
        U32(o + 0x54) = 0x803;
    }
    call(k->t + T_MAIN, o, U8(o + 0x54));
}

/* the flinches and the stagger (by the animation's flag): turning to the
 * other player; at their end back to standing (or turning to it) */
static void flinch(const Krex *k, gptr o, const u32 *anim, const u32 *fx, u32 end, int stagger)
{
    Obj_TurnFromHead(o, 0x28, o + 0xD8);
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        u8 s = (u8)Anim_FrameFlag(o);
        U8(o + 0x57) = s;
        if (stagger) s = (u8)Anim_FrameFlag(o);
        Anim_Start(o, anim[s], 1, 0x40, 5);
        Effect_Spawn(o, D(k, fx[U8(o + 0x57)]), 4, 0xFF, 0x100, 0);
        if (!stagger) Se_Play(o, 0x30);
        break;
    }
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) U32(o + 0x54) = !(HELD(k) & 0x1000) && (HELD(k) & 8) ? MOVE(2) : MOVE(0);
        break;
    }
    u16 f = U16(o + 0x92);
    if (stagger ? f == 0xD || f == 0x1B || f == 0x64 || f == 0x73 : f == 0x1E || f == 0x2D) stomp(k, o);
}

static void flinch8(const Krex *k, gptr o)
{
    static const u32 anim[2] = {0x15, 0x16}, fx[2] = {0x677064, 0x677298};
    flinch(k, o, anim, fx, 0xE, 0);
}

static void flinch9(const Krex *k, gptr o)
{
    static const u32 anim[2] = {0x15, 0x16}, fx[2] = {0x677064, 0x677298};
    flinch(k, o, anim, fx, 0x1A, 0);
}

static void stagger(const Krex *k, gptr o)
{
    static const u32 anim[2] = {0x18, 0x19}, fx[2] = {0x678784, 0x678C88};
    flinch(k, o, anim, fx, 0x22, 1);
}

/* dies (by the animation's flag): falls, the dust, then the other player wins */
static void dying(const Krex *k, gptr o)
{
    static const u32 anim[2] = {6, 7}, fx[2] = {0x67770C, 0x677D78};
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        u8 s = (u8)Anim_FrameFlag(o);
        U8(o + 0x57) = s;
        Anim_Start(o, anim[s], 0, 0x40, 5);
        Effect_Attach(o, D(k, fx[U8(o + 0x57)]));
        U32(o + 0x50) &= ~8u;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Flash_Play(o, D(k, 0x679228));
        break;
    }
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        if (U16(o + 0x92) == 0x50) {
            Shake_Set(5, 0, 0x32);
            if (U16(o + 0x92) == 0x50) Prop_SpawnC(1, o + 0x40, 2, 0x64, 1, 0);
        }
        break;
    case 2:
        if (U8(G() + 0x927) == 2) Flag_Set(7, 0x1E, 1);
        else GameOver_Start(0);
        break;
    }
}

/* ---- the entry points of both copies ------------------------------------------------------------ */
#define KREX(n, K) \
void DC2CALL KRex##n##_Handler(gptr o)   { handler(&K, o); }                                        \
void DC2CALL KRex##n##_Init(gptr o)      { init(&K, o); }                                           \
void DC2CALL KRex##n##_Normal(gptr o)    { normal(&K, o); }                                         \
void DC2CALL KRex##n##_Hit(gptr o)       { call(K.t + T_HIT, o, U8(o + 0x55)); }                    \
void DC2CALL KRex##n##_Die(gptr o)       { call(K.t + T_DIE, o, U8(o + 0x55)); }                    \
void DC2CALL KRex##n##_Attack(gptr o)    { attack(&K, o); }                                         \
void DC2CALL KRex##n##_PickStand(gptr o) { pick_stand(&K, o); }                                     \
void DC2CALL KRex##n##_Stand(gptr o)     { mv_stand(&K, o); }                                       \
void DC2CALL KRex##n##_PickWalk(gptr o)  { pick_walk(&K, o); }                                      \
void DC2CALL KRex##n##_Walk(gptr o)      { mv_walk(&K, o); }                                        \
void DC2CALL KRex##n##_Turn(gptr o)      { mv_turn(&K, o); }                                        \
void DC2CALL KRex##n##_PickOn(gptr o)    { pick_on(&K, o); }                                        \
void DC2CALL KRex##n##_On(gptr o)        { mv_go(&K, o, 0, 3, 0xF, 0x64, 0xB, 0x3C, 0); }           \
void DC2CALL KRex##n##_PickRun(gptr o)   { pick_run(&K, o); }                                       \
void DC2CALL KRex##n##_Run(gptr o)       { mv_go(&K, o, 2, 4, 0xD, 0xD2, 0xA, 0x28, 1); }           \
void DC2CALL KRex##n##_PickBack(gptr o)  { pick_back(&K, o); }                                      \
void DC2CALL KRex##n##_Back(gptr o)      { mv_go(&K, o, 1, 3, 0x12, -0x50, 0xA, 0x45, 0); }         \
void DC2CALL KRex##n##_Steer6(gptr o)    { steer(&K, o, 0x14); }                                    \
void DC2CALL KRex##n##_Super(gptr o)     { mv_super(&K, o); }                                       \
void DC2CALL KRex##n##_Roar(gptr o)      { mv_roar(&K, o); }                                        \
void DC2CALL KRex##n##_LongRoar(gptr o)  { mv_long_roar(&K, o); }                                   \
void DC2CALL KRex##n##_Steer9(gptr o)    { steer(&K, o, 0x28); }                                    \
void DC2CALL KRex##n##_Bite(gptr o)      { mv_bite(&K, o); }                                        \
void DC2CALL KRex##n##_Steer10(gptr o)   { steer(&K, o, 0x28); }                                    \
void DC2CALL KRex##n##_Swing(gptr o)     { mv_swing(&K, o); }                                       \
void DC2CALL KRex##n##_Steer11(gptr o)   { steer(&K, o, 0x28); }                                    \
void DC2CALL KRex##n##_Tail(gptr o)      { mv_tail(&K, o); }                                        \
void DC2CALL KRex##n##_LandBite(gptr o)  { land_bite(&K, o); }                                      \
void DC2CALL KRex##n##_LandSwing(gptr o) { land_swing(&K, o); }                                     \
void DC2CALL KRex##n##_LandTail(gptr o)  { land_tail(&K, o); }                                      \
void DC2CALL KRex##n##_LandRun(gptr o)   { land(&K, o, 0x679250, 0); }                              \
void DC2CALL KRex##n##_Hurt(gptr o)      { hurt(&K, o); }                                           \
void DC2CALL KRex##n##_Flinch8(gptr o)   { flinch8(&K, o); }                                        \
void DC2CALL KRex##n##_Flinch9(gptr o)   { flinch9(&K, o); }                                        \
void DC2CALL KRex##n##_Stagger(gptr o)   { stagger(&K, o); }                                        \
void DC2CALL KRex##n##_Dying(gptr o)     { dying(&K, o); }
KREX(1, K1)
KREX(2, K2)
