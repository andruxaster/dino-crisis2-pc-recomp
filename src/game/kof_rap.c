/* The raptor of the extra game's duel (object types 29 and 30; handlers
 * 0x456180 and 0x457BB0). The two copies of the code are the same but for
 * the player they belong to: their pads, the other player's object, the
 * variables, tables, model, the hit sparks and the first frame of the
 * standing animation at the start. Copy 1 has no die of its own: both use
 * 0x458310.
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the move), 2 hit (by
 * o+0x55: 0/4 a light hit, 1..3/5/6 knocked back, 7 knocked back hard),
 * 3 die.
 * Moves: 0 stand, 1 walk, 2 run, 3 back, 4 attack (by o+0x56: 0 claw (0/2
 * the start: 0 slashing, 2 standing), 1 the leaping claw, 2 the kick (1 the
 * high one), 3 the bite, 4 the super move), 5 turn, 6 spin round, 7 the
 * taunt, 8/9 turn to the other player.
 * Variables (v): +0 the pad's new presses, +4 o+0x1C0 (+0xE the spin's
 * step, +0x10 the angle to the other player), +8 the pad held. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
    u32 fx;             /* the effects of a light hit (4) */
    u32 frame0;         /* the first frame of the standing animation */
} Krap;

static const Krap K1 = {0x74B200, 0x722628, 0x662500, 2, 0x87DC94, 0x87DC08, 0x7033B8, 7};
static const Krap K2 = {0x74B20C, 0x7226F0, 0x633000, 1, 0x87DC92, 0x87DE0A, 0x7033D8, 0};

/* tables (t+) */
#define T_ATKS  0x00u       /* o+0x128 */
#define T_ATK2  0x10u       /* o+0x12C */
#define T_SPH   0x28u       /* o+0x134 */
#define T_COMBO 0x4Cu
#define T_MAIN  0x5Cu
#define T_MOVE  0x6Cu
#define T_HIT   0x94u
#define T_ATK   0xB4u

#define PAD(k)  U8((k)->v)
#define HELD(k) U16((k)->v + 8)
#define EXT(k)  U32((k)->v + 4)

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static gptr foe_obj(const Krap *k) { return U32(G() + 0x940) + k->foe * 0x260; }

static void handler(const Krap *k, gptr o)
{
    U16(k->v + 8) = U16(k->held);
    EXT(k) = o + 0x1C0;
    U16(k->v) = U16(k->pad);
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x4B0;
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

/* model, health 1200, the attack tables, head and feet */
static void init(const Krap *k, gptr o)
{
    U16(o + 0x108) = 0x4B0; U16(o + 0x10A) = 0x4B0;
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U32(o + 0x88) = k->m + 0x2550;
    U32(o + 0x12C) = k->t + T_ATK2; U32(o + 0x128) = k->t + T_ATKS; U32(o + 0x134) = k->t + T_SPH;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    Obj_Set118(o, 1, 0x170C, 0x1F40);
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

/* the super move by the button sequence (at half health or less), unless
 * attacking with it already; then the move */
static void normal(const Krap *k, gptr o)
{
    if (((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && U8(o + 0x55) != 4 && U8(o + 0x56) != 4
        && Combo_Input(k->t + T_COMBO, U16(k->v))) {
        U32(o + 0x54) = 0x40401;
        Kof_Super(o);
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
static void hurt_start(gptr o, u32 anim, u32 lock)
{
    Anim_Start(o, anim, 0, 0x40, 4);
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x4B0;
    U8(o + 0x11B) = 0;
    U8(o + 0x56)++;
    U16(o + 0x174) = 0;
    U8(o + 0x17B) = (u8)lock;
    if (S16(o + 0x108) < 0) { U32(o + 0x54) = 3; U32(o + 0x50) &= ~8u; }
}

static u32 hurt_end(const Krap *k, gptr o)
{
    if (!Anim_Step(o)) return 0;
    U32(o + 0x50) |= 8;
    U16(o + 0x174) = 0;
    U32(o + 0x54) = U8(k->v + 8) & 8 ? 0x801 : 1;
    return 1;
}

/* a light hit: a spark */
static void hurt_light(const Krap *k, gptr o)
{
    settle(o);
    switch (U8(o + 0x56)) {
    case 0:
        hurt_start(o, 0xC, 0xA);
        Effect_Attach(o, U32(k->fx + (Rand8() & 3) * 4));
        Se_Play(o, 0x48);
        break;
    case 1:
        hurt_end(k, o);
        break;
    }
}

/* knocked back (anim), then up again; hard: no bit 8 of o+0x50 kept */
static void hurt_back(const Krap *k, gptr o, u32 anim, u32 lock, u32 hard)
{
    settle(o);
    switch (U8(o + 0x56)) {
    case 0:
        hurt_start(o, anim, lock);
        Se_Play(o, 0x49);
        break;
    case 1:
        if (Anim_Step(o)) {
            Anim_Start(o, 0xF, 0, 0x40, 8);
            if (!hard) U32(o + 0x50) &= ~8u;
            U8(o + 0x56)++;
        }
        break;
    case 2:
        if (hurt_end(k, o)) U8(o + 0x17B) = 5;
        break;
    }
}

/* both copies' death: falls, lies; the other player wins (flag 7/0x1E) */
void DC2CALL KRap_Die(gptr o)                                       /* 0x458310 */
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
        }
        break;
    case 2:
        if (Anim_Step(o)) Flag_Set(7, 0x1E, 1);
        break;
    }
}

/* o+0x54 as a dword: state 1, move 4, attack, its start */
#define MOVE(m) (0x01u | (m) << 8)

/* held 0x20 (with 0x1000 and 0x40: the leaping claw, 0x4000: the claw
 * standing, else the claw) or 0x10 (with 0x8000: the kick, 0x2000: the
 * high kick, else the bite) start an attack */
static u32 chain(u32 h)
{
    if (h & 0x20) return (h & 0x1000) && (h & 0x40) ? 0x10401 : h & 0x4000 ? 0x2000401 : 0x401;
    if (h & 0x10) return h & 0x8000 ? 0x20401 : h & 0x2000 ? 0x1020401 : 0x30401;
    return 0;
}

static u32 chain_set(const Krap *k, gptr o)
{
    u32 c = chain(HELD(k));
    if (c) U32(o + 0x54) = c;
    return c;
}

static void steer(const Krap *k, gptr o, u32 d)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += (u16)d;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= (u16)d;
}

/* a walking move: o+0x56 0 starts the animation, 1 steps it */
static void walk_step(gptr o, u32 anim, u32 fwd, u32 speed)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, anim, 0, 1, 6);
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0;
        if (speed) U16(o + 0xF4) = (u16)fwd;
        break;
    case 1:
        Anim_Step(o);
        if (!speed) U16(o + 0xF4) = (u16)fwd;
        break;
    }
}

/* standing: 8 face the other player, held 8 and 4 spin round... */
static void mv_stand(const Krap *k, gptr o)
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

static void mv_walk(const Krap *k, gptr o)
{
    walk_step(o, 0, 0x64, 0);
    steer(k, o, 0x5A);
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

static void mv_run(const Krap *k, gptr o)
{
    walk_step(o, 1, 0xD2, 0);
    steer(k, o, 0x5A);
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

static void mv_back(const Krap *k, gptr o)
{
    walk_step(o, 2, (u32)-0x50, 1);
    steer(k, o, 0x5A);
    u32 p = PAD(k), h = HELD(k);
    if (chain_set(k, o)) ;
    else if (p & 2) U32(o + 0x54) = MOVE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(2) : MOVE(1);
    else if (h & 0x4000) { if (p & 1) U32(o + 0x54) = MOVE(7); }
    else U32(o + 0x54) = h & 8 ? MOVE(8) : MOVE(0);
}

static void mv_turn(const Krap *k, gptr o)
{
    walk_step(o, 0, 0, 1);
    steer(k, o, 0x5A);
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

/* spins round, away from the side the other player is on */
static void mv_spin(const Krap *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        if (Angle_DiffTo(o + 0x40, foe_obj(k) + 0x40, U16(o + 0x4A)) & 0xF000) {
            Anim_Start(o, 0x14, 0, 0, 2);
            U16(EXT(k) + 0xE) = 0xFF99;
        } else {
            Anim_Start(o, 0x13, 0, 0, 2);
            U16(EXT(k) + 0xE) = 0x67;
        }
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        U16(o + 0x4A) = (U16(EXT(k) + 0xE) + U16(o + 0x4A)) & 0xFFF;
        break;
    }
}

/* the taunt (held 0x1000: the other one; else one of two at random) */
static void mv_taunt(const Krap *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        if (HELD(k) & 0x1000) {
            Anim_Start(o, 6, 0, 0, 4);
            U8(o + 0x56)++;
            U16(o + 0xF4) = 0xA;
        } else {
            Anim_Start(o, Rand8() & 1 ? 4 : 5, 0, 0x40, 4);
            U8(o + 0x56)++;
        }
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = U8(k->v + 8) & 8 ? 0x801 : 1;
        break;
    }
}

/* turns to face the other player */
static void mv_face(const Krap *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0, 0, 4, 6);
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

/* the attack spheres of model parts `parts` (0xFF ends) against the other
 * player, from the frame turned to o+0x4A; 1 at a hit of the last part */
static u32 hit_foe(const Krap *k, gptr o, const u8 *parts, u32 slot)
{
    gptr f = Tmp_Push(0x28), sv = f, m = f + 8;
    U16(sv) = 0x400; U16(sv + 2) = U16(o + 0x4A); U16(sv + 4) = 0;
    RotMatrixYXZ(sv, m);
    Obj_UpdateMatrix(o);
    u32 r = 0;
    for (; *parts != 0xFF; parts++)
        r = Kof_HitFoe(m, Obj_Part(U32(o + 0x64), *parts) + 0x40, slot, k->foe);
    Tmp_Pop(0x28);
    return r;
}

static const u8 P_CLAW[] = {4, 0xFF}, P_KICK[] = {0xE, 0xF, 0x10, 0xFF}, P_BITE[] = {0, 0xFF};

static void count_attack(void) { U32(G() + 0x684)++; }

/* the claw; o+0x57 0 slashing, 2 standing still, 1/3 the swing */
static void atk_claw(const Krap *k, gptr o)
{
    u32 s = U8(o + 0x57);
    switch (s) {
    case 0: case 2:
        if (s) { Anim_Start(o, 0x12, 6, 0x40, 6); U16(o + 0xF4) = 0; }
        else { Anim_Start(o, 7, 6, 0x40, 6); U16(o + 0xF4) = 0x32; }
        U32(o + 0xF0) = 0;
        U8(o + 0x57)++;
        count_attack();
        break;
    case 1: case 3: {
        steer(k, o, 0x16);
        Anim_Step(o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0x24) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
        else if (fr >= 0x20) chain_set(k, o);
        u32 lo = U8(o + 0x57) == 1 ? 0x18 : 0x14, hi = U8(o + 0x57) == 1 ? 0x1C : 0x19;
        if (fr >= lo && fr <= hi && hit_foe(k, o, P_CLAW, 0xD)) Effect_SpawnAt(o, k->m + 0x16B54, 0);
        break; }
    }
}

static void atk_leap(const Krap *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 8, 5, 1, 2);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x64;
        count_attack();
        break;
    case 1: {
        steer(k, o, 0x16);
        Anim_Step(o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0x1C) U32(o + 0x54) = 1;
        else if (fr >= 0x18) chain_set(k, o);
        if (fr >= 0xD && fr <= 0x11 && hit_foe(k, o, P_CLAW, 0xD)) Effect_SpawnAt(o, k->m + 0x16BD0, 0);
        break; }
    }
}

static void atk_kick(const Krap *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0xA, 0x11, 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x57) = 2;
        count_attack();
        break;
    case 1:
        Anim_Start(o, 9, 0x11, 1, 4);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        count_attack();
        break;
    case 2: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x28) chain_set(k, o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0x1D && fr <= 0x27) hit_foe(k, o, P_KICK, 0xE);
        break; }
    }
}

static void atk_bite(const Krap *k, gptr o)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0xB, 3, 0x40, 4);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        count_attack();
        break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1E) chain_set(k, o);
        u32 fr = U16(o + 0x92);
        if (fr >= 0xE && fr <= 0x18 && hit_foe(k, o, P_BITE, 0xF)) Effect_SpawnAt(o, k->m + 0x16DDC, 0);
        break; }
    }
}

/* the super move: the bite, the claw three times, the slash, the leap */
static void atk_super(const Krap *k, gptr o)
{
    Flag_Set(7, 0x1B, 1);
    u32 fr;
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0xB, 0, 0x40, 4);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        count_attack();
        break;
    case 1:
        U8(o + 0x57)++;
        /* fall through */
    case 2:
        Anim_Step(o);
        if (U16(o + 0x92) == 0x1B) {
            Anim_Start(o, 7, 4, 0x40, 6);
            U8(o + 0x57)++;
            U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        }
        break;
    case 3: case 4: case 5:
        Anim_Step(o);
        fr = U16(o + 0x92);
        if (fr >= 0x18 && fr <= 0x1B) hit_foe(k, o, P_CLAW, 0xD);
        if (U16(o + 0x92) == 0x1B) {
            Anim_Start(o, 7, 7, 0x40, 4);
            U8(o + 0x57)++;
        }
        break;
    case 6:
        Anim_Step(o);
        fr = U16(o + 0x92);
        if (fr >= 0x18 && fr <= 0x1B) hit_foe(k, o, P_CLAW, 0xD);
        if (U16(o + 0x92) == 0x20) {
            Anim_Start(o, 0x12, 7, 0x40, 6);
            U8(o + 0x57)++;
            U16(o + 0xF4) = 0;
        }
        break;
    case 7:
        Anim_Step(o);
        fr = U16(o + 0x92);
        if (fr == 0x1B) {
            Anim_Start(o, 8, 6, 1, 2);
            U8(o + 0x57)++;
            U16(o + 0xF4) = 0xA;
        } else if (fr >= 0x14 && fr <= 0x19) hit_foe(k, o, P_CLAW, 0xD);
        break;
    case 8:
        if (Anim_Step(o)) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
        else {
            fr = U16(o + 0x92);
            if (fr >= 0xC && fr <= 0x11) hit_foe(k, o, P_CLAW, 0xF);
        }
        break;
    }
    steer(k, o, 0x16);
}

#define KRAP(n, K) \
void DC2CALL KRap##n##_Handler(gptr o)  { handler(&K, o); }                         \
void DC2CALL KRap##n##_Init(gptr o)     { init(&K, o); }                            \
void DC2CALL KRap##n##_Normal(gptr o)   { normal(&K, o); }                          \
void DC2CALL KRap##n##_HurtLight(gptr o) { hurt_light(&K, o); }                     \
void DC2CALL KRap##n##_HurtBack(gptr o) { hurt_back(&K, o, 0xD, 0xA, 0); }          \
void DC2CALL KRap##n##_HurtHard(gptr o) { hurt_back(&K, o, 0xE, 0xC, 1); }          \
void DC2CALL KRap##n##_Hit(gptr o)      { call(K.t + T_HIT, o, U8(o + 0x55)); }     \
void DC2CALL KRap##n##_Stand(gptr o)    { mv_stand(&K, o); }                        \
void DC2CALL KRap##n##_Walk(gptr o)     { mv_walk(&K, o); }                         \
void DC2CALL KRap##n##_Run(gptr o)      { mv_run(&K, o); }                          \
void DC2CALL KRap##n##_Back(gptr o)     { mv_back(&K, o); }                         \
void DC2CALL KRap##n##_Attack(gptr o)   { call(K.t + T_ATK, o, U8(o + 0x56)); }     \
void DC2CALL KRap##n##_Turn(gptr o)     { mv_turn(&K, o); }                         \
void DC2CALL KRap##n##_Spin(gptr o)     { mv_spin(&K, o); }                         \
void DC2CALL KRap##n##_Taunt(gptr o)    { mv_taunt(&K, o); }                        \
void DC2CALL KRap##n##_Face8(gptr o)    { mv_face(&K, o); }                         \
void DC2CALL KRap##n##_Face9(gptr o)    { mv_face(&K, o); }                         \
void DC2CALL KRap##n##_Claw(gptr o)     { atk_claw(&K, o); }                        \
void DC2CALL KRap##n##_Leap(gptr o)     { atk_leap(&K, o); }                        \
void DC2CALL KRap##n##_Kick(gptr o)     { atk_kick(&K, o); }                        \
void DC2CALL KRap##n##_Bite(gptr o)     { atk_bite(&K, o); }                        \
void DC2CALL KRap##n##_Super(gptr o)    { atk_super(&K, o); }
KRAP(1, K1)
KRAP(2, K2)
