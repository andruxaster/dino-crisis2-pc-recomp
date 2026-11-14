/* The inostrancevia of the extra game's duel (object types 41 and 42,
 * KOF_P51P.DAT / KOF_P50P.DAT; handlers 0x45D100 and 0x45E430). The two
 * copies of the code are the same but for the player they belong to: their
 * pads, the other player's object, the variables, tables and model.
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the pad picks the move,
 * then the move runs), 2 hit (0..7 hurt, 8/9 flinch), 3 die (8/9).
 * Moves: 0 stand, 1 walk, 2 turn to the other player, 3 walk on, 4 walk
 * back, 5 roar, 6 turn about (o+0x57 the side), 7/8/9 the three attacks
 * (their landing: 0x722C18 + 7..9).
 * Variables (v): +0 the pad's new presses, +8 bits (1 moving, 3 lunging,
 * 4 attacking, 6 super move), +0x10 the hurt kind, +0x12 o+0x54..0x57 of
 * the last frame, +0x40 the turn about so far, +0x48 the point turned to,
 * +0x50 the other player, +0x54 the pad held, +0x56 the angle from the
 * head to the other player. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
} Kino;

static const Kino K1 = {0x74B2D8, 0x722B08, 0x662500, 2, 0x87DC94, 0x87DC08};
static const Kino K2 = {0x74B330, 0x722C68, 0x638000, 1, 0x87DC92, 0x87DE0A};

/* tables (t+) */
#define T_MAIN  0x60u
#define T_PICK  0x70u
#define T_MOVE  0x98u
#define T_HIT   0xC0u
#define T_DIE   0xE8u
#define T_LAND  0x110u
#define T_DMG   0x138u
#define T_REACT 0x14Cu
#define T_COMBO 0x50u

#define BITS(k) ((k)->v + 8)
#define HELD(k) U32((k)->v + 0x54)
#define PAD(k)  U8((k)->v)

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

static void attack(const Kino *k, gptr o);

static void handler(const Kino *k, gptr o)
{
    gptr v = k->v;
    U16(v + 0x54) = U16(k->held);
    gptr foe = U32(0x87E760) + k->foe * 0x260;
    U16(v) = U16(k->pad);
    U32(v + 0x50) = foe;
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr d = U32(U32(v + 0x50) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(U32(v + 0x50) + 0x40);
    U16(o + 0xDA) = U16(U32(v + 0x50) + 0x42);
    U16(o + 0xDC) = U16(U32(v + 0x50) + 0x44);
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    u16 a = p ? (u16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    U16(v + 0x56) = a;
    s32 aa = (s16)a; if (aa < 0) aa = -aa;
    if (aa < 0x500 && U32(v + 0x50) != o && Bits_Check(BITS(k), 1)) U32(o + 0x158) = 2;
    else U32(o + 0x158) = 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    attack(k, o);
    U8(v + 0x12) = U8(o + 0x54); U8(v + 0x13) = U8(o + 0x55);
    U8(v + 0x14) = U8(o + 0x56); U8(v + 0x15) = U8(o + 0x57);
}

/* model, health 1200, head, the other player as the head's target */
static void init(const Kino *k, gptr o)
{
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x4B0; U16(o + 0x10A) = 0x4B0;
    U32(o + 0x88) = k->m + 0x2350;
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x28; U32(o + 0x134) = k->t + 0x38;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x16, 0, 1, 0);
    Obj_Set118(o, 1, 0x1FA0, 0x3AF8);
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x80; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(U32(k->v + 0x50) + 0xFF);
    gptr d = U32(U32(k->v + 0x50) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    Bits_Set(BITS(k), 1, 0);
    U8(o + 0x118) |= 0x20;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the super move by the button sequence (at half health or less), its
 * health coming back while it lasts; then the pad's choice and the move */
static void normal(const Kino *k, gptr o)
{
    Bits_Set(BITS(k), 4, 0);
    U16(o + 0x174) = 0;
    if (Combo_Input(k->t + T_COMBO, U16(k->v))
        && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && !Bits_Check(BITS(k), 6)) {
        Bits_Set(BITS(k), 6, 1);
        Kof_Super(o);
    }
    if (Bits_Check(BITS(k), 6)) {
        U16(o + 0x108)++;
        if (S16(o + 0x108) > S16(o + 0x10A)) {
            U16(o + 0x108) = U16(o + 0x10A);
            Bits_Set(BITS(k), 6, 0);
        }
    }
    call(k->t + T_PICK, o, U8(o + 0x55));
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

static void attack(const Kino *k, gptr o)
{
    u32 s = U8(o + 0x55);
    Kof_Attack(o, k->foe, U16(k->t + T_DMG + s * 2), U16(k->t + T_REACT + s * 2), 0x1F);
    if (S8(o + 0x119) >= 0) call(k->t + T_LAND, o, U8(o + 0x55));
}

/* o+0x54 as a dword: state 1, move m, the side to turn about */
static u32 side(const Kino *k) { return S16(k->v + 0x56) <= 0; }
#define MOVE(m) (0x01u | (m) << 8)

static void steer(const Kino *k, gptr o)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += 0x28;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= 0x28;
}

/* standing: 8 turn to the other player (or held 8 and 4), 0x20/0x40/0x10
 * the attacks, 2 turn about, 0x1000 walk on, 0x4000 back, 0x2000/0x8000
 * walk, 1 roar */
static void pick_stand(const Kino *k, gptr o)
{
    u32 c = side(k), p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x40) U32(o + 0x54) = MOVE(8);
    else if (h & 0x10) U32(o + 0x54) = MOVE(9);
    else if (p & 2) U32(o + 0x54) = c << 24 | MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(4);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(1);
    else if (h & 0x8000) U32(o + 0x54) = MOVE(1);
    else if (p & 1) U32(o + 0x54) = MOVE(5);
}

static void pick_walk(const Kino *k, gptr o)
{
    u32 c = side(k), p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x40) U32(o + 0x54) = MOVE(8);
    else if (h & 0x10) U32(o + 0x54) = MOVE(9);
    else if (p & 2) U32(o + 0x54) = c << 24 | MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(4);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (p & 1) U32(o + 0x54) = MOVE(5);
    steer(k, o);
}

/* walking on (dir 0x1000) or back (0x4000): stops when the button is let go */
static void pick_dir(const Kino *k, gptr o, u32 dir)
{
    u32 c = side(k), h = HELD(k), keep = h & dir;
    if (!keep && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!keep) U32(o + 0x54) = MOVE(0);
    else if (h & 0x20) U32(o + 0x54) = MOVE(7);
    else if (h & 0x40) U32(o + 0x54) = MOVE(8);
    else if (h & 0x10) U32(o + 0x54) = MOVE(9);
    else if (PAD(k) & 2) U32(o + 0x54) = c << 24 | MOVE(6);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(5);
    steer(k, o);
}

/* the second attack goes on into the first (frame 5) when 0x20 is held */
static void pick_atk8(const Kino *k, gptr o)
{
    steer(k, o);
    if (U8(o + 0x56) && U16(o + 0x92) < 0x17 && (U8(k->v + 0x54) & 0x20))
        Anim_Start(o, 5, 5, 0x40, 5);
}

/* a move's start: o+0x56 0 -> 1, stopped, animation; 1 the move itself */
static void start(const Kino *k, gptr o, u32 anim, u32 frame, u32 fl, u32 blend, u32 moving, int bits_first)
{
    U8(o + 0x56)++;
    zero_move(o);
    if (bits_first) Bits_Set(BITS(k), 1, moving);
    Anim_Start(o, anim, frame, fl, blend);
    if (!bits_first) Bits_Set(BITS(k), 1, moving);
}

static void mv_stand(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 0x16, 0, 1, 0xF, 1, 1); break;
    case 1: Anim_Step(o); break;
    }
}

static void mv_walk(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 0, 0, 1, 0xA, 1, 0); break;
    case 1: Anim_Step(o); break;
    }
}

/* turns to where the other player was (0x96 a frame, 20 frames at most) */
static void mv_turn(const Kino *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(v + 0x50) == o) { U32(o + 0x54) = 1; return; }
        start(k, o, 0, 0, 1, 0xA, 1, 0);
        U16(v + 0x48) = U16(o + 0xD8); U16(v + 0x4A) = U16(o + 0xDA); U16(v + 0x4C) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, v + 0x48);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = 1;
        break;
    }
    }
}

static void mv_on(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 0, 0, 1, 0xA, 1, 0); break;
    case 1: Anim_Step(o); Value_Approach(o + 0xF4, 0x14, 0x3C); break;
    }
}

static void mv_back(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 4, 0, 9, 0x14, 1, 0); break;
    case 1: Anim_Step(o); Value_Approach(o + 0xF4, 0x14, (u32)-0x1E); break;
    }
}

static void mv_roar(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 1, 0, 0x40, 0xA, 0, 0); break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        u16 f = U16(o + 0x92);
        if (f >= 0xE && f <= 0x31) Bits_Set(BITS(k), 4, 1);
        break;
    }
    }
}

/* half a turn either way (o+0x57) over the animation; the root part keeps
 * its direction until the end */
static void mv_about(const Kino *k, gptr o)
{
    static const u16 w[8] = {0x800, 0xF800, 0x21, 0x21, 2, 0, 3, 0};
    u32 s = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, (u32)w[4 + 2 * s] | (u32)w[5 + 2 * s] << 16, 1, 0x40, 5);
        Bits_Set(BITS(k), 1, 0);
        U16(k->v + 0x40) = 0;
        break;
    case 1:
        U16(k->v + 0x40) += (u16)((s32)(s16)w[s] / (s32)(s16)w[s + 2]);
        Anim_Step(o);
        if (U16(o + 0x92) < 0x26) break;
        U16(k->v + 0x40) = 0;
        gptr p = U32(o + 0x64);
        U16(p + 0x4A) = (u16)((U16(p + 0x4A) - w[s]) & 0xFFF);
        U16(o + 0x4A) = (u16)((w[s] + U16(o + 0x4A)) & 0xFFF);
        Anim_Set(o, 0, 0, 1, 0xA);
        U32(o + 0x54) = 1;
        break;
    }
}

/* the bites: animation, its end and the frames it can hit in */
static void mv_bite(const Kino *k, gptr o, u32 anim, u32 frame, u32 end, u32 lo, u32 hi)
{
    switch (U8(o + 0x56)) {
    case 0:
        start(k, o, anim, frame, 0x40, 0xA, 0, 0);
        U32(0x87E4A4)++;
        break;
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= end) U32(o + 0x54) = 1;
        if (f >= lo && f <= hi) Bits_Set(BITS(k), 4, 1);
        break;
    }
    }
}

/* the lunge */
static void mv_lunge(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        start(k, o, 6, 0xE, 0x40, 0xA, 0, 0);
        Se_Play(o, 0x3E);
        U32(0x87E4A4)++;
        break;
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= 0x41) U32(o + 0x54) = 1;
        Bits_Set(BITS(k), 3, f > 0x14 && f < 0x36);
        Value_Approach(o + 0xF4, 0x14, 0x78);
        break;
    }
    }
}

/* an attack landing: a flash and the effect on its first frame */
static void land(gptr o, gptr flash, gptr eff)
{
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, flash);
    if (eff) Effect_Attach(o, eff);
}

/* hit: the hurt kind by o+0x55, more damage while its own attack is out,
 * flinch 9 while attacking or lunging (or kind 3), else 8; health 0: dies */
static void hurt(const Kino *k, gptr o)
{
    gptr v = k->v;
    static const s8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(v + 0x10) = (u16)kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    U32(o + 0x54) = 0x802;
    if (Bits_Check(BITS(k), 4)) U32(o + 0x54) = 0x902;
    if (U16(v + 0x10) == 3) U32(o + 0x54) = 0x902;
    if (Bits_Check(BITS(k), 3)) U32(o + 0x54) = 0x902;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(BITS(k), 3, 0);
    Bits_Set(BITS(k), 4, 0);
    Bits_Set(BITS(k), 6, 0);
    if (U16(v + 0x40)) {
        U16(o + 0x4A) = (u16)((U16(o + 0x4A) + U16(v + 0x40)) & 0xFFF);
        U16(v + 0x40) = 0;
    }
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U32(o + 0x50) &= ~8u;
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = 3 | (u32)U8(o + 0x55) << 8;
    }
    call(k->t + T_MAIN, o, U8(o + 0x54));
}

static void flinch(const Kino *k, gptr o, u32 end)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 9, 0, 0x40, 2);
        Effect_Attach(o, k->m + 0x8624);
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

/* falls, a puff of dust (frame 0x2D), then the other player wins */
static void dying(const Kino *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0xF, 0, 0x40, 0x14);
        Effect_Attach(o, k->m + 0x9094);
        Bits_Set(BITS(k), 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            U8(o + 0x56)++;
            U32(o + 0x50) |= 0x30;
            Layers_Assign();
        }
        if (U16(o + 0x92) == 0x2D) Prop_SpawnC(0, o + 0x40, 2, 0x28, 1, 0);
        break;
    case 2:
        if (U8(G() + 0x927) == 2) Flag_Set(7, 0x1E, 1);
        else GameOver_Start(0);
        break;
    }
}

/* ---- the entry points of both copies ------------------------------------------------------------ */
#define KINO(n, K) \
void DC2CALL KIno##n##_Handler(gptr o)  { handler(&K, o); }                                         \
void DC2CALL KIno##n##_Init(gptr o)     { init(&K, o); }                                            \
void DC2CALL KIno##n##_Normal(gptr o)   { normal(&K, o); }                                          \
void DC2CALL KIno##n##_Hit(gptr o)      { call(K.t + T_HIT, o, U8(o + 0x55)); }                     \
void DC2CALL KIno##n##_Die(gptr o)      { call(K.t + T_DIE, o, U8(o + 0x55)); }                     \
void DC2CALL KIno##n##_Attack(gptr o)   { attack(&K, o); }                                          \
void DC2CALL KIno##n##_PickStand(gptr o) { pick_stand(&K, o); }                                     \
void DC2CALL KIno##n##_Stand(gptr o)    { mv_stand(&K, o); }                                        \
void DC2CALL KIno##n##_PickWalk(gptr o) { pick_walk(&K, o); }                                       \
void DC2CALL KIno##n##_Walk(gptr o)     { mv_walk(&K, o); }                                         \
void DC2CALL KIno##n##_Turn(gptr o)     { mv_turn(&K, o); }                                         \
void DC2CALL KIno##n##_PickOn(gptr o)   { pick_dir(&K, o, 0x1000); }                                \
void DC2CALL KIno##n##_On(gptr o)       { mv_on(&K, o); }                                           \
void DC2CALL KIno##n##_PickBack(gptr o) { pick_dir(&K, o, 0x4000); }                                \
void DC2CALL KIno##n##_Back(gptr o)     { mv_back(&K, o); }                                         \
void DC2CALL KIno##n##_Roar(gptr o)     { mv_roar(&K, o); }                                         \
void DC2CALL KIno##n##_About(gptr o)    { mv_about(&K, o); }                                        \
void DC2CALL KIno##n##_Steer7(gptr o)   { steer(&K, o); }                                           \
void DC2CALL KIno##n##_Bite(gptr o)     { mv_bite(&K, o, 7, 2, 0x1D, 0xE, 0x18); }                  \
void DC2CALL KIno##n##_PickBite2(gptr o) { pick_atk8(&K, o); }                                      \
void DC2CALL KIno##n##_Bite2(gptr o)    { mv_bite(&K, o, 5, 0, 0x2B, 0x15, 0x24); }                 \
void DC2CALL KIno##n##_Steer9(gptr o)   { steer(&K, o); }                                           \
void DC2CALL KIno##n##_Lunge(gptr o)    { mv_lunge(&K, o); }                                        \
void DC2CALL KIno##n##_Land7(gptr o)    { land(o, K.m + 0x9338, K.m + 0x8554); }                    \
void DC2CALL KIno##n##_Land8(gptr o)    { land(o, K.m + 0x9318, K.m + 0x82A4); }                    \
void DC2CALL KIno##n##_Land9(gptr o)    { land(o, K.m + 0x9328, 0); }                               \
void DC2CALL KIno##n##_Hurt(gptr o)     { hurt(&K, o); }                                            \
void DC2CALL KIno##n##_Flinch8(gptr o)  { flinch(&K, o, 0xA); }                                     \
void DC2CALL KIno##n##_Flinch9(gptr o)  { flinch(&K, o, 0x26); }                                    \
void DC2CALL KIno##n##_Dying(gptr o)    { dying(&K, o); }
KINO(1, K1)
KINO(2, K2)
