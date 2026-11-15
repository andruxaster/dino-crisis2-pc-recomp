/* The oviraptor of the extra game's duel (object types 31 and 32; handlers
 * 0x466340 and 0x467A80). Built like the raptor (kof_rap.c): the two copies
 * differ in their pads, the other player's object, the variables, tables,
 * model, hit sparks and the first frame of the standing animation at the
 * start; copy 1 has no die of its own: both use 0x468210.
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the move), 2 hit (by
 * o+0x55: 0/4 a light hit, 1/5 knocked back, 2/3/6/7 knocked back hard),
 * 3 die.
 * Moves: 0 stand, 1 walk, 2 run, 3 back (o+0x56 2/3: the hop back), 4
 * attack (by o+0x56: 0 the claw (o+0x57 0/2 the start: 0 slashing, 2
 * standing), 1 the leaping claw (repeated by a new press of 0x20), 2 the
 * kick, 3 the head butt, 4 the super move), 5 turn, 6 spin round, 7 the
 * taunt, 8/9 turn to the other player.
 * Variables (v): +0 the pad's new presses, +4 o+0x1C0 (+0x10 the angle to
 * the other player), +8 the pad held. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
    u32 fx_back, fx_hard;   /* the sparks of a knock back (4 each) */
    u32 frame0;         /* the first frame of the standing animation */
} Kovi;

static const Kovi K1 = {0x74B4D8, 0x723498, 0x662500, 2, 0x87DC94, 0x87DC08, 0x7033F8, 0x703408, 7};
static const Kovi K2 = {0x74B4E4, 0x723550, 0x633000, 1, 0x87DC92, 0x87DE0A, 0x703418, 0x703428, 0};

/* tables (t+) */
#define T_ATKS  0x00u       /* o+0x128 */
#define T_ATK2  0x10u       /* o+0x12C */
#define T_SPH   0x20u       /* o+0x134 */
#define T_COMBO 0x38u
#define T_MAIN  0x48u
#define T_MOVE  0x58u
#define T_HIT   0x80u
#define T_ATK   0xA0u

#define PAD(k)  U8((k)->v)
#define HELD(k) U16((k)->v + 8)
#define EXT(k)  U32((k)->v + 4)

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static gptr foe_obj(const Kovi *k) { return U32(G() + 0x940) + k->foe * 0x260; }

static void handler(const Kovi *k, gptr o)
{
    U16(k->v + 8) = U16(k->held);
    EXT(k) = o + 0x1C0;
    U16(k->v) = U16(k->pad);
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x3E8;
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Head_Update(o);
    Obj_UpdateMatrix(o);
    Obj_AnchorFoot(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
}

/* model, health 1000, the attack tables, head and feet */
static void init(const Kovi *k, gptr o)
{
    U16(o + 0x108) = 0x3E8; U16(o + 0x10A) = 0x3E8;
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U32(o + 0x88) = k->m + 0x2078;
    U32(o + 0x12C) = k->t + T_ATK2; U32(o + 0x128) = k->t + T_ATKS; U32(o + 0x134) = k->t + T_SPH;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    Obj_Set118(o, 1, 0x12C0, 0x1518);
    U8(o + 0x118) |= 0x20;
    Head_Reset(o);
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x16A) = 0x100; U16(o + 0x16E) = 0x280;
    U8(o + 0x15D) = 0xFF; U8(o + 0x15E) = 0;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x300;
    U8(o + 0x15C) = 0; U32(o + 0x188) = 0;
    Obj_SetFeet(o, 0xC, 0xC);
    Anim_Start(o, 0x16, k->frame0, 1, 4);
    U32(o + 0x54) = 0x10001;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the super move by the button sequence (at half health or less, flag
 * 7/0x1B cleared first), unless attacking with it already; then the move */
static void normal(const Kovi *k, gptr o)
{
    if (((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && U8(o + 0x55) != 4 && U8(o + 0x56) != 4) {
        Flag_Set(7, 0x1B, 0);
        if (Combo_Input(k->t + T_COMBO, U16(k->v))) {
            U32(o + 0x54) = 0x40401;
            Kof_Super(o);
        }
    }
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

/* the level of the floor under it (o+0x5A); rises to it */
static void settle(gptr o)
{
    s32 lv = (s32)(s16)Floor_Height(o + 0xE0, o + 0x40, 0x40, 0) / -1000;
    U16(o + 0x5A) = (u16)lv;
    s32 f = lv * -1000;
    if (f > S16(o + 0x42)) U16(o + 0x42) += 0xC8;
    if (f < S16(o + 0x42)) U16(o + 0x42) = (u16)f;
}

/* hit: the animation, the damage (none while flag 0/0xB), dead below 0 */
static void hurt_start(gptr o, u32 anim)
{
    Anim_Start(o, anim, 0, 0x40, 4);
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x3E8;
    U8(o + 0x11B) = 0;
    U8(o + 0x56)++;
    U16(o + 0x174) = 0;
    U8(o + 0x17B) = 0xA;
    if (S16(o + 0x108) < 0) { U32(o + 0x54) = 3; U32(o + 0x50) &= ~8u; }
}

static u32 hurt_end(const Kovi *k, gptr o)
{
    if (!Anim_Step(o)) return 0;
    U32(o + 0x50) |= 8;
    U16(o + 0x174) = 0;
    U32(o + 0x54) = U8(k->v + 8) & 8 ? 0x801 : 1;
    return 1;
}

/* a light hit: one of two sparks */
static void hurt_light(const Kovi *k, gptr o)
{
    settle(o);
    switch (U8(o + 0x56)) {
    case 0:
        hurt_start(o, 0xC);
        Effect_Attach(o, Rand8() & 0x10 ? k->m + 0x14DC4 : k->m + 0x15360);
        Se_Play(o, 0x41);
        break;
    case 1:
        hurt_end(k, o);
        break;
    }
}

/* knocked back (anim), then up again */
static void hurt_back(const Kovi *k, gptr o, u32 anim, u32 fx)
{
    settle(o);
    switch (U8(o + 0x56)) {
    case 0:
        hurt_start(o, anim);
        Effect_Attach(o, U32(fx + (Rand8() & 3) * 4));
        Se_Play(o, 0x42);
        break;
    case 1:
        if (Anim_Step(o)) {
            Anim_Start(o, 0xF, 0, 0x40, 8);
            U32(o + 0x50) &= ~8u;
            U8(o + 0x56)++;
        }
        break;
    case 2:
        if (hurt_end(k, o)) U8(o + 0x17B) = 5;
        break;
    }
}

/* both copies' death: falls, lies; the other player wins (flag 7/0x1E) */
void DC2CALL KOvi_Die(gptr o)                                       /* 0x468210 */
{
    settle(o);
    switch (U8(o + 0x55)) {
    case 0:
        Anim_Start(o, 0x10, 0, 0x40, 8);
        U8(o + 0x55)++;
        U32(o + 0x50) &= ~8u;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) {
            Anim_Start(o, 0x11, 0, 0, 8);
            U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18u);
            U8(o + 0x55)++;
            Flag_Set(7, 0x1E, 1);
        }
        break;
    case 2:
        if (Anim_Step(o)) Flag_Set(7, 0x1E, 1);
        break;
    }
}

/* o+0x54 as a dword: state 1, the move */
#define MOVE(m) (0x01u | (m) << 8)

/* held 0x20 (with 0x1000 and 0x40: the leaping claw, 0x4000: the claw
 * standing, else the head butt) or 0x10 (with 0x4000: the kick, else the
 * claw) start an attack */
static u32 chain(u32 h)
{
    if (h & 0x20) return (h & 0x1000) && (h & 0x40) ? 0x10401 : h & 0x4000 ? 0x2000401 : 0x30401;
    if (h & 0x10) return h & 0x4000 ? 0x20401 : 0x401;
    return 0;
}

static u32 chain_set(const Kovi *k, gptr o)
{
    u32 c = chain(HELD(k));
    if (c) U32(o + 0x54) = c;
    return c;
}

/* late in an attack: held 0x20 the head butt, 0x10 (with 0x4000) the kick
 * or the claw */
static void chain_late(const Kovi *k, gptr o)
{
    u32 h = HELD(k);
    if (h & 0x20) U32(o + 0x54) = 0x30401;
    else if (h & 0x10) U32(o + 0x54) = h & 0x4000 ? 0x20401 : 0x401;
}

static void steer(const Kovi *k, gptr o, u32 d)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += (u16)d;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= (u16)d;
}

/* a walking move: o+0x56 0 starts the animation, 1 steps it (and sets the
 * speed when `step`) */
static void walk_step(gptr o, u32 anim, u32 blend, u32 fwd, u32 step)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, anim, 0, 1, blend);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0;
        U16(o + 0xF4) = step ? 0 : (u16)fwd;
        break;
    case 1:
        Anim_Step(o);
        if (step) U16(o + 0xF4) = (u16)fwd;
        break;
    }
}

/* standing: 8 face the other player, held 8 and 4 turn to it... */
static void mv_stand(const Kovi *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0x16, 0, 0x49, 0xA);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        Anim_Step(o);
        break;
    }
    u32 p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(8);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(9);
    else if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(2) : MOVE(1);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(3);
    else if (h & 0xA000) U32(o + 0x54) = MOVE(5);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
}

static void mv_walk(const Kovi *k, gptr o)
{
    walk_step(o, 0, 3, 0x50, 1);
    steer(k, o, 0x64);
    u32 p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(8);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(9);
    else if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) {
        if (h & 0x40) U32(o + 0x54) = MOVE(2);
        else if (p & 1) U32(o + 0x54) = MOVE(7);
    }
    else if (h & 0x4000) U32(o + 0x54) = MOVE(3);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
    else U32(o + 0x54) = h & 8 ? MOVE(8) : MOVE(0);
}

static void mv_run(const Kovi *k, gptr o)
{
    walk_step(o, 1, 6, 0xD2, 1);
    steer(k, o, 0x64);
    u32 p = PAD(k), h = HELD(k);
    if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) {
        if (!(h & 0x40)) U32(o + 0x54) = MOVE(1);
        else if (p & 1) U32(o + 0x54) = MOVE(7);
    }
    else if (h & 0x4000) U32(o + 0x54) = MOVE(3);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
    else U32(o + 0x54) = h & 8 ? MOVE(8) : MOVE(0);
}

/* stepping back; held 0x4000 with a new 0x40: the hop back (o+0x56 2/3),
 * again late in it */
static void mv_back(const Kovi *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 2, 0, 1, 3);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = (u16)-0x50;
        break;
    case 1:
        Anim_Step(o);
        break;
    case 2:
        Anim_Start(o, 6, 2, 0x40, 1);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        return;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1C && (HELD(k) & 0x4000) && (PAD(k) & 0x40)) U8(o + 0x56) = 2;
        return;
    }
    steer(k, o, 0x64);
    u32 p = PAD(k), h = HELD(k);
    if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(2) : MOVE(1);
    else if (h & 0x4000) {
        if (p & 0x40) U32(o + 0x54) = 0x20301;
        else if (p & 1) U32(o + 0x54) = MOVE(7);
    }
    else U32(o + 0x54) = h & 8 ? MOVE(8) : MOVE(0);
}

static void mv_turn(const Kovi *k, gptr o)
{
    walk_step(o, 0, 3, 0, 0);
    steer(k, o, 0x64);
    u32 p = PAD(k), h = HELD(k);
    if (p & 8) U32(o + 0x54) = MOVE(8);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(9);
    else if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(2) : MOVE(1);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(3);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
}

/* spins round (the way from the side the other player is on), then faces
 * the other way, standing */
static void mv_spin(const Kovi *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, Angle_DiffTo(o + 0x40, foe_obj(k) + 0x40, U16(o + 0x4A)) & 0xF000 ? 9 : 3, 0xA, 0, 4);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) {
            U16(o + 0x4A) = (U16(o + 0x4A) - 0x800) & 0xFFF;
            gptr p = U32(o + 0x64);
            U16(p + 0x4A) = (U16(p + 0x4A) - 0x800) & 0xFFF;
            Anim_Set(o, 0x16, 0, 1, 0);
            U32(o + 0x54) = 0x10001;
        }
        break;
    }
}

/* the taunt: one of two at random */
static void mv_taunt(const Kovi *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, Rand8() & 1 ? 4 : 5, 0, 0x40, 4);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = U8(k->v + 8) & 8 ? 0x801 : 1;
        break;
    }
}

/* turns to face the other player */
static void mv_face(const Kovi *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0, 0, 0, 6);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        gptr a = Tmp_Push(8);
        Angles_LookAt(foe_obj(k) + 0x40, o + 0x40, a);
        U16(EXT(k) + 0x10) = U16(a + 2);
        Tmp_Pop(8);
        U8(o + 0x56)++;
        break; }
    case 1: {
        Anim_Step(o);
        u32 d = Angle_Approach(U16(o + 0x4A), 0xFA, U16(EXT(k) + 0x10));
        U16(o + 0x4A) += (u16)d;
        if (!(u16)d) U32(o + 0x54) = 1;
        break; }
    }
}

/* the attack sphere of model part `part` against the other player, from the
 * frame turned to o+0x4A; 1 at a hit */
static u32 hit_foe(const Kovi *k, gptr o, u32 part, u32 slot)
{
    gptr f = Tmp_Push(0x28), sv = f, m = f + 8;
    U16(sv) = 0x400; U16(sv + 2) = U16(o + 0x4A); U16(sv + 4) = 0;
    RotMatrixYXZ(sv, m);
    Obj_UpdateMatrix(o);
    u32 r = Kof_HitFoe(m, Obj_Part(U32(o + 0x64), part) + 0x40, slot, k->foe);
    Tmp_Pop(0x28);
    return r;
}

static void count_attack(void) { U32(G() + 0x684)++; }

static void attack_start(gptr o, u32 anim, u32 frame, u32 fl, u32 blend)
{
    Anim_Start(o, anim, frame, fl, blend);
    U8(o + 0x57)++;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    count_attack();
}

/* the claw (o+0x57 0 slashing, 2 standing, 1/3 the swings): four strikes */
static void atk_claw(const Kovi *k, gptr o)
{
    u32 s = U8(o + 0x57);
    switch (s) {
    case 0: case 2:
        if (s) attack_start(o, 0x12, 4, 0x40, 6);
        else attack_start(o, 7, 0xA, 0x40, 6);
        break;
    case 1: case 3: {
        if (Anim_Step(o)) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
        else if (U16(o + 0x92) >= 0x24) chain_late(k, o);
        if (U8(o + 0x57) == 1) {
            if (U16(o + 0x92) == 0x18) Effect_SpawnAt(o, k->m + 0x15A3C, 0);
            break;
        }
        steer(k, o, 0x32);
        u32 fr = U16(o + 0x92);
        if ((fr >= 0xD && fr <= 0x19) || (fr >= 0x20 && fr <= 0x26) || (fr >= 0x2D && fr <= 0x32)
            || (fr >= 0x3A && fr <= 0x40))
            hit_foe(k, o, 4, 0xD);
        break; }
    }
}

/* the leaping claw; a new press of 0x20 at frame 0x19 leaps again */
static void atk_leap(const Kovi *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        attack_start(o, 8, 6, 0x40, 2);
        break;
    case 1: {
        steer(k, o, 0x19);
        Anim_Step(o);
        u32 fr = U16(o + 0x92);
        if (fr == 0x19) {
            if (PAD(k) & 0x20) Anim_Start(o, 8, 0xA, 1, 4);
        } else if (fr >= 0x1C) {
            U32(o + 0x54) = 1;
            u32 h = U32(k->v + 8);
            if (h & 0x10) U32(o + 0x54) = (h & 0x4000) << 3 | 0x401;
        }
        fr = U16(o + 0x92);
        if (fr >= 0xD && fr <= 0x11) hit_foe(k, o, 4, 0xD);
        break; }
    }
}

/* the kick (anim 0xA) and the head butt (anim 0xB): a thud and a spark at a hit */
static void atk_kick(const Kovi *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0xA, 2, 0x40, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        count_attack();
        U8(o + 0x57)++;
        break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1E) chain_late(k, o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0x10 && fr <= 0x16 && hit_foe(k, o, 0, 0x22)) {
            Se_Play(o, 0x45);
            Effect_SpawnAt(o, k->m + 0x13788, 0);
        }
        break; }
    }
}

static void atk_butt(const Kovi *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        attack_start(o, 0xB, 2, 0x40, 4);
        break;
    case 1: {
        Anim_Step(o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0x22) U32(o + 0x54) = 1;
        else if (fr >= 0x1E) chain_late(k, o);
        if (fr >= 0xC && fr <= 0x15 && hit_foe(k, o, 0, 0xF)) {
            Se_Play(o, 0x45);
            Effect_SpawnAt(o, k->m + 0x13788, 0);
        }
        break; }
    }
}

/* the super move: the claw twelve times over */
static void atk_super(const Kovi *k, gptr o)
{
    Flag_Set(7, 0x1B, 1);
    u32 s = U8(o + 0x57);
    if (s == 0) {
        attack_start(o, 7, 0xA, 0x40, 6);
        return;
    }
    if (s <= 0xB) {
        Anim_Step(o);
        steer(k, o, 0x50);
        if (U16(o + 0x92) >= 0x1C) {
            Anim_Start(o, 7, 0x15, 0x40, 1);
            U8(o + 0x57)++;
        }
    } else if (s == 0xC) {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
    } else return;
    if (U16(o + 0x92) == 0x18) Effect_SpawnAt(o, k->m + 0x15A3C, 0);
}

#define KOVI(n, K) \
void DC2CALL KOvi##n##_Handler(gptr o)   { handler(&K, o); }                        \
void DC2CALL KOvi##n##_Init(gptr o)      { init(&K, o); }                           \
void DC2CALL KOvi##n##_Normal(gptr o)    { normal(&K, o); }                         \
void DC2CALL KOvi##n##_HurtLight(gptr o) { hurt_light(&K, o); }                     \
void DC2CALL KOvi##n##_HurtBack(gptr o)  { hurt_back(&K, o, 0xD, K.fx_back); }      \
void DC2CALL KOvi##n##_HurtHard(gptr o)  { hurt_back(&K, o, 0xE, K.fx_hard); }      \
void DC2CALL KOvi##n##_Hit(gptr o)       { call(K.t + T_HIT, o, U8(o + 0x55)); }    \
void DC2CALL KOvi##n##_Stand(gptr o)     { mv_stand(&K, o); }                       \
void DC2CALL KOvi##n##_Walk(gptr o)      { mv_walk(&K, o); }                        \
void DC2CALL KOvi##n##_Run(gptr o)       { mv_run(&K, o); }                         \
void DC2CALL KOvi##n##_Back(gptr o)      { mv_back(&K, o); }                        \
void DC2CALL KOvi##n##_Attack(gptr o)    { call(K.t + T_ATK, o, U8(o + 0x56)); }    \
void DC2CALL KOvi##n##_Turn(gptr o)      { mv_turn(&K, o); }                        \
void DC2CALL KOvi##n##_Spin(gptr o)      { mv_spin(&K, o); }                        \
void DC2CALL KOvi##n##_Taunt(gptr o)     { mv_taunt(&K, o); }                       \
void DC2CALL KOvi##n##_Face8(gptr o)     { mv_face(&K, o); }                        \
void DC2CALL KOvi##n##_Face9(gptr o)     { mv_face(&K, o); }                        \
void DC2CALL KOvi##n##_Claw(gptr o)      { atk_claw(&K, o); }                       \
void DC2CALL KOvi##n##_Leap(gptr o)      { atk_leap(&K, o); }                       \
void DC2CALL KOvi##n##_Butt(gptr o)      { atk_butt(&K, o); }                       \
void DC2CALL KOvi##n##_Kick(gptr o)      { atk_kick(&K, o); }                       \
void DC2CALL KOvi##n##_Super(gptr o)     { atk_super(&K, o); }
KOVI(1, K1)
KOVI(2, K2)
