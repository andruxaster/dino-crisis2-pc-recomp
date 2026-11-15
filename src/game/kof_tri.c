/* The triceratops of the extra game's duel (object types 37 and 38;
 * handlers 0x463DA0 and 0x465070). The two copies of the code are the same
 * but for the player they belong to: their pads, the other player's object,
 * the variables, tables and model (copy 2 also has its three attack
 * landings in another order).
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the pad picks the move,
 * then the move runs), 2 hit (0..7 hurt, 8/9 flinch, 10 knocked back),
 * 3 die (8..10).
 * Moves: 0 stand, 1 walk, 2 turn to the other player, 3 walk on, 4 charge,
 * 5 back, 6 roar, 7 gore, 8 the side swipe (o+0x57 the side), 9 the super
 * move (rears up, then gores).
 * Variables (v): +0 the pad's new presses, +8 bits (1 moving), +0x10 the
 * hurt kind, +0x12 o+0x54..0x57 of the last frame, +0x18 the damage taken
 * lately (3 less a frame; over 0xA0 it staggers), +0x40 the point turned
 * to, +0x48 the other player, +0x4C the pad held, +0x4E the angle from the
 * head to the other player. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
} Ktri;

static const Ktri K1 = {0x74B438, 0x7231A8, 0x662500, 2, 0x87DC94, 0x87DC08};
static const Ktri K2 = {0x74B488, 0x723320, 0x638000, 1, 0x87DC92, 0x87DE0A};

/* tables (t+) */
#define T_COMBO 0x5Cu
#define T_MAIN  0x6Cu
#define T_PICK  0x7Cu
#define T_MOVE  0xA4u
#define T_HIT   0xCCu
#define T_DIE   0xF8u
#define T_LAND  0x124u
#define T_DMG   0x14Cu
#define T_REACT 0x160u

#define BITS(k) ((k)->v + 8)
#define HELD(k) U32((k)->v + 0x4C)
#define PAD(k)  U8((k)->v)
#define FOE(k)  U32((k)->v + 0x48)

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

static void attack(const Ktri *k, gptr o);

static void handler(const Ktri *k, gptr o)
{
    gptr v = k->v;
    U16(v + 0x4C) = U16(k->held);
    gptr foe = U32(0x87E760) + k->foe * 0x260;
    U16(v) = U16(k->pad);
    FOE(k) = foe;
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(FOE(k) + 0x40);
    U16(o + 0xDA) = U16(FOE(k) + 0x42);
    U16(o + 0xDC) = U16(FOE(k) + 0x44);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(v + 0x4E) = p ? (u16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    s32 aa = S16(v + 0x4E); if (aa < 0) aa = -aa;
    if (aa < 0x500 && FOE(k) != o && Bits_Check(BITS(k), 1)) U32(o + 0x158) = 2;
    else U32(o + 0x158) = 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    attack(k, o);
    if (U32(v + 0x18)) {
        s32 t = (s32)U32(v + 0x18) - 3;
        U32(v + 0x18) = t < 0 ? 0 : (u32)t;
    }
    U8(v + 0x12) = U8(o + 0x54); U8(v + 0x13) = U8(o + 0x55);
    U8(v + 0x14) = U8(o + 0x56); U8(v + 0x15) = U8(o + 0x57);
}

/* model, health 2200, head, the other player as the head's target */
static void init(const Ktri *k, gptr o)
{
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x898; U16(o + 0x10A) = 0x898;
    U32(o + 0x88) = k->m + 0x2DC4;
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x28; U32(o + 0x134) = k->t + 0x38;
    zero_move(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 4, 0, 1, 0);
    Obj_Set118(o, 0, 0x4E20, 0x7530);
    Head_Reset(o);
    U8(o + 0x15F) = 0xC; U8(o + 0x160) = 8;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(FOE(k) + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    Bits_Set(BITS(k), 1, 0);
    U8(o + 0xFE) = 3;
    U8(o + 0x118) |= 0x20;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the super move by the button sequence (at half health or less), unless it
 * is already on; then the pad's choice and the move */
static void normal(const Ktri *k, gptr o)
{
    U16(o + 0x174) = 0;
    if (Combo_Input(k->t + T_COMBO, U16(k->v))
        && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && U8(o + 0x55) != 9) {
        U32(o + 0x54) = 0x901;
        Kof_Super(o);
    }
    call(k->t + T_PICK, o, U8(o + 0x55));
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

static void attack(const Ktri *k, gptr o)
{
    u32 s = U8(o + 0x55);
    Kof_Attack(o, k->foe, U16(k->t + T_DMG + s * 2), U16(k->t + T_REACT + s * 2), 0x1F);
    if (S8(o + 0x119) >= 0) call(k->t + T_LAND, o, U8(o + 0x55));
}

/* o+0x54 as a dword: state 1, move m, the side of the swipe */
static u32 side(const Ktri *k) { return S16(k->v + 0x4E) <= 0; }
#define MOVE(m) (0x01u | (m) << 8)

static void steer(const Ktri *k, gptr o)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += 0x32;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= 0x32;
}

/* standing: 8 turn to the other player (or held 8 and 4), 0x20 gore, 0x10
 * swipe, 0x1000 walk on (charge with 0x40), 0x4000 back, 0x2000/0x8000
 * walk, 1 roar */
static void pick_stand(const Ktri *k, gptr o)
{
    u32 c = side(k), p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x10) U32(o + 0x54) = c << 24 | MOVE(8);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(1);
    else if (h & 0x8000) U32(o + 0x54) = MOVE(1);
    else if (p & 1) U32(o + 0x54) = MOVE(6);
}

static void pick_walk(const Ktri *k, gptr o)
{
    u32 c = side(k), p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x10) U32(o + 0x54) = c << 24 | MOVE(8);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (p & 1) U32(o + 0x54) = MOVE(6);
    steer(k, o);
}

/* walking on (dir 0x1000, charge: also 0x40) or back (0x4000): stops when
 * the button is let go */
static void pick_dir(const Ktri *k, gptr o, u32 dir, u32 charge)
{
    u32 c = side(k), h = HELD(k), keep = h & dir;
    if (!keep && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!keep) U32(o + 0x54) = MOVE(0);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x10) U32(o + 0x54) = c << 24 | MOVE(8);
    else if (dir == 0x1000 && !charge && (h & 0x40)) U32(o + 0x54) = MOVE(4);
    else if (charge && !(h & 0x40)) U32(o + 0x54) = MOVE(0);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(6);
    steer(k, o);
}

/* a move's start: o+0x56 0 -> 1, the moving bit, animation */
static void start(const Ktri *k, gptr o, u32 anim, u32 moving)
{
    U8(o + 0x56)++;
    Bits_Set(BITS(k), 1, moving);
    Anim_Start(o, anim, 0, 1, 5);
}

/* stand, walk, walk on, back: the speed goes to `speed` */
static void mv_step(const Ktri *k, gptr o, u32 anim, u32 speed)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, anim, 1); break;
    case 1: Anim_Step(o); break;
    }
    Value_Approach(o + 0xF4, 0x1E, speed);
}

/* turns to where the other player was (0x96 a frame, 20 frames at most) */
static void mv_turn(const Ktri *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        if (FOE(k) == o) { U32(o + 0x54) = 1; return; }
        start(k, o, 4, 1);
        U16(v + 0x40) = U16(o + 0xD8); U16(v + 0x42) = U16(o + 0xDA); U16(v + 0x44) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, v + 0x40);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = 1;
        break;
    }
    }
    Value_Approach(o + 0xF4, 0x1E, 0);
}

static void mv_charge(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, 0, 0, 1, 5);
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

static void mv_roar(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, 1, 0, 0x40, 5);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

/* an attack's start: the animation, standing, one more attack counted */
static void atk_start(const Ktri *k, gptr o, u32 anim, u32 frame)
{
    U8(o + 0x56)++;
    U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    Anim_Start(o, anim, frame, 0x40, 5);
    Bits_Set(BITS(k), 1, 0);
    U32(0x87E4A4)++;
}

static void mv_gore(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: atk_start(k, o, 5, 0xC); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2A) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

/* the swipe to the side o+0x57 (0 or 1: animation 6 or 7) */
static void mv_swipe(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: atk_start(k, o, U8(o + 0x57) ? 7 : 6, 0); break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

/* the super move: rears up (animation 1), then the gore at full speed */
static void mv_super(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 1, 0, 0, 5);
        Bits_Set(BITS(k), 1, 0);
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; Anim_Start(o, 5, 0xC, 0x40, 5); }
        break;
    case 2:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; zero_move(o); }
        if (U16(o + 0x92) > 0xF) Value_Approach(o + 0xF4, 0x1E, 0x12C);
        else Value_Approach(o + 0xF4, 0x12C, 0x2580);
        break;
    }
}

static void steer_early(const Ktri *k, gptr o) { if (U8(o + 0x56) <= 1) steer(k, o); }

/* an attack landing: a flash and the effect on its first frame */
static void land(gptr o, gptr flash, gptr eff)
{
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, flash);
    Effect_Attach(o, eff);
}

/* hit: the hurt kind by o+0x55, more damage while its own attack is out;
 * flinch 8, 9 when it has taken over 0xA0 lately, knocked back (10) by
 * kind 3; health 0: dies */
static void hurt(const Ktri *k, gptr o)
{
    gptr v = k->v;
    static const s8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(v + 0x10) = (u16)kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    U32(o + 0x54) = 0x802;
    U32(v + 0x18) += U16(o + 0x176);
    if ((s32)U32(v + 0x18) > 0xA0) { U32(v + 0x18) = 0; U32(o + 0x54) = 0x902; }
    if (U16(v + 0x10) == 3) U32(o + 0x54) = 0xA02;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(BITS(k), 1, 0);
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = 0x803;
        U32(o + 0x50) &= ~8u;
    }
    call(k->t + T_MAIN, o, U8(o + 0x54));
}

/* flinch (8, 9) or knocked back (10): after the frame `end`, turns to the
 * other player when 8 is held and 0x1000 is not */
static void flinch(const Ktri *k, gptr o, u32 end, int knock)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (knock) {
            Anim_Start(o, 9, 4, 0x40, 2);
            Effect_Spawn(o, k->m + 0x15D7C, 4, 0xFF, 0x100, 0);
        } else {
            Anim_Start(o, 8, 0, 0x40, 2);
            Effect_Attach(o, k->m + 0x15A54);
        }
        break;
    case 1: {
        Anim_Step(o);
        if (U16(o + 0x92) < end) break;
        u32 h = HELD(k);
        U32(o + 0x54) = !(h & 0x1000) && (h & 8) ? MOVE(2) : 1;
        break;
    }
    }
}

/* falls, then the other player wins */
static void dying(const Ktri *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 0xA, 0, 0x40, 0x14);
        Effect_Attach(o, k->m + 0x15B70);
        Bits_Set(BITS(k), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        break;
    case 2:
        if (U8(G() + 0x927) == 2) Flag_Set(7, 0x1E, 1);
        else GameOver_Start(0);
        break;
    }
}

/* ---- the entry points of both copies ------------------------------------------------------------ */
#define KTRI(n, K) \
void DC2CALL KTri##n##_Handler(gptr o)   { handler(&K, o); }                                        \
void DC2CALL KTri##n##_Init(gptr o)      { init(&K, o); }                                           \
void DC2CALL KTri##n##_Normal(gptr o)    { normal(&K, o); }                                         \
void DC2CALL KTri##n##_Hit(gptr o)       { call(K.t + T_HIT, o, U8(o + 0x55)); }                    \
void DC2CALL KTri##n##_Die(gptr o)       { call(K.t + T_DIE, o, U8(o + 0x55)); }                    \
void DC2CALL KTri##n##_Attack(gptr o)    { attack(&K, o); }                                         \
void DC2CALL KTri##n##_PickStand(gptr o) { pick_stand(&K, o); }                                     \
void DC2CALL KTri##n##_Stand(gptr o)     { mv_step(&K, o, 0x16, 0); }                               \
void DC2CALL KTri##n##_PickWalk(gptr o)  { pick_walk(&K, o); }                                      \
void DC2CALL KTri##n##_Walk(gptr o)      { mv_step(&K, o, 4, 0); }                                  \
void DC2CALL KTri##n##_Turn(gptr o)      { mv_turn(&K, o); }                                        \
void DC2CALL KTri##n##_PickOn(gptr o)    { pick_dir(&K, o, 0x1000, 0); }                            \
void DC2CALL KTri##n##_On(gptr o)        { mv_step(&K, o, 4, 0x5A); }                               \
void DC2CALL KTri##n##_PickCharge(gptr o) { pick_dir(&K, o, 0x1000, 1); }                           \
void DC2CALL KTri##n##_Charge(gptr o)    { mv_charge(&K, o); }                                      \
void DC2CALL KTri##n##_PickBack(gptr o)  { pick_dir(&K, o, 0x4000, 0); }                            \
void DC2CALL KTri##n##_Back(gptr o)      { mv_step(&K, o, 2, (u32)-0x50); }                         \
void DC2CALL KTri##n##_Steer6(gptr o)    { steer(&K, o); }                                          \
void DC2CALL KTri##n##_Roar(gptr o)      { mv_roar(&K, o); }                                        \
void DC2CALL KTri##n##_Steer7(gptr o)    { steer(&K, o); }                                          \
void DC2CALL KTri##n##_Gore(gptr o)      { mv_gore(&K, o); }                                        \
void DC2CALL KTri##n##_Steer8(gptr o)    { steer(&K, o); }                                          \
void DC2CALL KTri##n##_Swipe(gptr o)     { mv_swipe(&K, o); }                                       \
void DC2CALL KTri##n##_PickSuper(gptr o) { steer_early(&K, o); }                                    \
void DC2CALL KTri##n##_Super(gptr o)     { mv_super(&K, o); }                                       \
void DC2CALL KTri##n##_Land4(gptr o)     { land(o, K.m + 0x160C8, K.m + 0x15700); }                 \
void DC2CALL KTri##n##_Land7(gptr o)     { land(o, K.m + 0x160D8, K.m + 0x15700); }                 \
void DC2CALL KTri##n##_Land8(gptr o)     { land(o, K.m + 0x160E8, U8(o + 0x57) ? K.m + 0x15960 : K.m + 0x1586C); } \
void DC2CALL KTri##n##_Hurt(gptr o)      { hurt(&K, o); }                                           \
void DC2CALL KTri##n##_Flinch8(gptr o)   { flinch(&K, o, 2, 0); }                                   \
void DC2CALL KTri##n##_Flinch9(gptr o)   { flinch(&K, o, 0x19, 0); }                                \
void DC2CALL KTri##n##_Knock(gptr o)     { flinch(&K, o, 0x2A, 1); }                                \
void DC2CALL KTri##n##_Dying(gptr o)     { dying(&K, o); }
KTRI(1, K1)
KTRI(2, K2)
