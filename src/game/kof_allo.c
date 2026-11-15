/* The allosaurus of the extra game's duel (object types 33 and 34; handlers
 * 0x45F760 and 0x461A80). The two copies of the code are the same but for
 * the player they belong to: their pads, the other player's object, the
 * variables, tables and model.
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the pad picks the move,
 * then the move runs), 2 hit (0..7 hurt, 8/9 flinch, 10 knocked about
 * (o+0x57 the side), 11 thrown back), 3 die (8/9/11, 10 by the side).
 * Moves: 0 stand, 1 walk, 2 turn to the other player, 3 walk on, 4 run,
 * 5 back, 6 turn about (o+0x57 the side), 7 face the other player, 8 side
 * step (o+0x57 the side), 9 (unused: a slow roar), 10 the rage (its super
 * move), 11 roar, 12..15 the attacks (bite, charge, tail, sweep).
 * Variables (v): +0 the pad's new presses, +8 bits (1 moving, 2 attacking,
 * 3 rage), +0x10 the hurt kind, +0x12 o+0x54..0x57 of the last frame, +0x2E
 * the turn about so far, +0x38 the rage's armour, +0x3A its glow, +0x3C its
 * time left, +0x44 the point turned to, +0x4C the other player, +0x50 the
 * pad held, +0x52 the angle from the head to the other player. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
} Kallo;

static const Kallo K1 = {0x74B388, 0x722DC8, 0x662500, 2, 0x87DC94, 0x87DC08};
static const Kallo K2 = {0x74B3E0, 0x722FB8, 0x638000, 1, 0x87DC92, 0x87DE0A};

/* tables (t+) */
#define T_COMBO 0x70u
#define T_MAIN  0x80u
#define T_PICK  0x90u
#define T_MOVE  0xD0u
#define T_HIT   0x110u
#define T_DIE   0x140u
#define T_LAND  0x170u
#define T_DMG   0x1B0u
#define T_REACT 0x1D0u

#define BITS(k) ((k)->v + 8)
#define HELD(k) U16((k)->v + 0x50)
#define PAD(k)  U8((k)->v)
#define RAGE(k) Bits_Check(BITS(k), 3)

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

static void attack(const Kallo *k, gptr o);
static void glow(const Kallo *k, gptr o);

static void handler(const Kallo *k, gptr o)
{
    gptr v = k->v;
    U16(v + 0x50) = U16(k->held);
    gptr foe = U32(0x87E760) + k->foe * 0x260;
    U16(v) = U16(k->pad);
    U32(v + 0x4C) = foe;
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr d = U32(U32(v + 0x4C) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(U32(v + 0x4C) + 0x40);
    U16(o + 0xDA) = U16(U32(v + 0x4C) + 0x42);
    U16(o + 0xDC) = U16(U32(v + 0x4C) + 0x44);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(v + 0x52) = p ? (u16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    glow(k, o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    s32 aa = S16(v + 0x52); if (aa < 0) aa = -aa;
    if (aa < 0x500 && U32(v + 0x4C) != o && Bits_Check(BITS(k), 1)) U32(o + 0x158) = 2;
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

/* model, health 1500, head, the other player as the head's target */
static void init(const Kallo *k, gptr o)
{
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x5DC; U16(o + 0x10A) = 0x5DC;
    U32(o + 0x88) = k->m + 0x2D4C;
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x30; U32(o + 0x134) = k->t + 0x40;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x16, 0, 1, 0);
    Obj_Set118(o, 0, 0x2770, 0x4E80);
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x40; U16(o + 0x166) = 0x40; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(U32(k->v + 0x4C) + 0xFF);
    gptr d = U32(U32(k->v + 0x4C) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    Bits_Set(BITS(k), 1, 0);
    U8(o + 0x118) |= 0x20;
    U8(o + 0xFE) = 3;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the rage by the button sequence (at half health or less), then the pad's
 * choice and the move */
static void normal(const Kallo *k, gptr o)
{
    U16(o + 0x174) = 0;
    if (Combo_Input(k->t + T_COMBO, U16(k->v))
        && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && !RAGE(k)) {
        U32(o + 0x54) = 0xA01;
        Kof_Super(o);
    }
    call(k->t + T_PICK, o, U8(o + 0x55));
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

static void attack(const Kallo *k, gptr o)
{
    u32 s = U8(o + 0x55);
    Kof_Attack(o, k->foe, U16(k->t + T_DMG + s * 2), U16(k->t + T_REACT + s * 2), 0x1F);
    if (S8(o + 0x119) >= 0) call(k->t + T_LAND, o, U8(o + 0x55));
}

/* the rage: its red glow comes up (30 frames) and fades; it ends with its time */
static void glow(const Kallo *k, gptr o)
{
    gptr v = k->v;
    if (RAGE(k)) {
        if (S16(v + 0x3A) < 0x1E) U16(v + 0x3A)++;
    }
    else if (U16(v + 0x3A)) U16(v + 0x3A)--;
    if (U16(v + 0x3A) && U32(o + 0x64)) {
        static const s32 c[3] = {0x90 - 0x80, 0x60 - 0x80, 0x68 - 0x80};
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54))
            for (int i = 0; i < 3; i++) U8(p + 0x8C + i) = (u8)(S16(v + 0x3A) * c[i] / 30 + 0x80);
    }
    if (!U16(v + 0x3C) || !--U16(v + 0x3C)) Bits_Set(BITS(k), 3, 0);
}

/* in the rage an attack can be cut short from a frame on (a 16-bit frame) */
static int cut(const Kallo *k, gptr o, u32 frame)
{
    return U16(o + 0x92) >= (u16)frame && RAGE(k);
}

/* o+0x54 as a dword: state 1, move m, the side */
static u32 side(const Kallo *k) { return S16(k->v + 0x52) <= 0; }
#define MOVE(m) (0x01u | (m) << 8)
#define SIDE(m) (side(k) << 24 | MOVE(m))

static void steer(const Kallo *k, gptr o)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += 0x46;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= 0x46;
}

/* the attack buttons: 0x20 with 8 the bite, else the tail; 0x10 the sweep */
static int pick_attack(const Kallo *k, gptr o, u32 h)
{
    if (h & 0x20) { U32(o + 0x54) = h & 8 ? MOVE(12) : MOVE(14); return 1; }
    if (h & 0x10) { U32(o + 0x54) = SIDE(15); return 1; }
    if (PAD(k) & 2) { U32(o + 0x54) = SIDE(6); return 1; }
    return 0;
}

/* standing: 0x1000 with 0x40 and 0x20 the charge, the attacks, 2 turn
 * about, 0x1000 walk on (with 0x40 run), 8 face, 0x4000 back, 0x2000/0x8000
 * walk, 1 roar */
static void pick_stand(const Kallo *k, gptr o)
{
    u32 h = HELD(k);
    if ((h & 0x1000) && (h & 0x40) && (h & 0x20)) U32(o + 0x54) = MOVE(13);
    else if (pick_attack(k, o, h)) ;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 8) U32(o + 0x54) = MOVE(7);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(1);
    else if (h & 0x8000) U32(o + 0x54) = MOVE(1);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
}

static void pick_walk(const Kallo *k, gptr o)
{
    u32 h = HELD(k);
    if ((h & 0x1000) && (h & 0x40) && (h & 0x20)) U32(o + 0x54) = MOVE(13);
    else if (pick_attack(k, o, h)) ;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 8) U32(o + 0x54) = MOVE(7);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
    steer(k, o);
}

/* walking on (0x1000): let go, 8 turns to the other player */
static void pick_on(const Kallo *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x1000)) U32(o + 0x54) = MOVE(0);
    else if ((h & 0x40) && (h & 0x20)) U32(o + 0x54) = MOVE(13);
    else if (pick_attack(k, o, h)) ;
    else if (h & 0x40) U32(o + 0x54) = MOVE(4);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
    steer(k, o);
}

/* running (0x1000 and 0x40): 0x20 the charge, no tail or bite */
static void pick_run(const Kallo *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x1000)) U32(o + 0x54) = MOVE(0);
    else if (!(h & 0x40)) U32(o + 0x54) = MOVE(3);
    else if (h & 0x20) U32(o + 0x54) = MOVE(13);
    else if (h & 0x10) U32(o + 0x54) = SIDE(15);
    else if (PAD(k) & 2) U32(o + 0x54) = SIDE(6);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
    steer(k, o);
}

static void pick_back(const Kallo *k, gptr o)
{
    u32 h = HELD(k);
    if (!(h & 0x4000) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & 0x4000)) U32(o + 0x54) = MOVE(0);
    else if (pick_attack(k, o, h)) ;
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
    steer(k, o);
}

/* facing (8 held): 0x2000/0x8000 side steps, 0x4000 a side step away from
 * the nearer wall, 4 turns to it at once */
static void pick_face(const Kallo *k, gptr o)
{
    u32 c = side(k), h = HELD(k), p = PAD(k);
    if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (!(h & 8)) U32(o + 0x54) = MOVE(0);
    else if (p & 4) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(12);
    else if (h & 0x10) U32(o + 0x54) = c << 24 | MOVE(15);
    else if (p & 2) U32(o + 0x54) = c << 24 | MOVE(6);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(8);
    else if (h & 0x8000) U32(o + 0x54) = 0x1000000u | MOVE(8);
    else if (h & 0x4000) {
        u32 l = Wall_Dist(o, 0x400, 0x40);
        u32 r = Wall_Dist(o, (u32)-0x400, 0x40);
        U32(o + 0x54) = (u32)(l <= r) << 24 | MOVE(8);
    }
    else if (p & 1) U32(o + 0x54) = MOVE(11);
}

/* after a side step */
static void pick_step(const Kallo *k, gptr o)
{
    u32 c = side(k), h = HELD(k);
    if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (!(h & 8)) U32(o + 0x54) = MOVE(0);
    else if (h & 0x20) U32(o + 0x54) = MOVE(12);
    else if (h & 0x10) U32(o + 0x54) = c << 24 | MOVE(15);
    else if (PAD(k) & 2) U32(o + 0x54) = c << 24 | MOVE(6);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(11);
}

/* the bite turns to the other player over its first frames */
static void pick_bite(const Kallo *k, gptr o)
{
    if (U16(o + 0x92) < 0xA) Obj_TurnToPoint(o, 0x23, o + 0xD8);
    steer(k, o);
}

/* a move's start: o+0x56 0 -> 1, stopped, animation, moving or not */
static void start(const Kallo *k, gptr o, u32 anim, u32 frame, u32 fl, u32 blend, u32 moving)
{
    U8(o + 0x56)++;
    zero_move(o);
    Anim_Start(o, anim, frame, fl, blend);
    Bits_Set(BITS(k), 1, moving);
}

/* the same keeping its speed on (o+0xF4) */
static void start_on(const Kallo *k, gptr o, u32 anim, u32 frame, u32 fl, u32 blend, u32 moving)
{
    U8(o + 0x56)++;
    Anim_Start(o, anim, frame, fl, blend);
    U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    Bits_Set(BITS(k), 1, moving);
}

static void mv_stand(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 0x16, 0, 0x41, 0x14, 1); break;
    case 1: Anim_Step(o); break;
    }
}

static void mv_walk(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 0, 0, 1, 0xA, 1); break;
    case 1: Anim_Step(o); break;
    }
}

static void save_point(const Kallo *k, gptr o)
{
    U16(k->v + 0x44) = U16(o + 0xD8); U16(k->v + 0x46) = U16(o + 0xDA); U16(k->v + 0x48) = U16(o + 0xDC);
}

/* turns to where the other player was (0x96 a frame, 20 frames at most), then faces it */
static void mv_turn(const Kallo *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(v + 0x4C) == o) { U32(o + 0x54) = MOVE(7); return; }
        start(k, o, 0, 0, 1, 0xA, 1);
        save_point(k, o);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, v + 0x44);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = MOVE(7);
        break;
    }
    }
}

/* walking on, running and back: the speed comes up to its top (more in the rage) */
static void mv_go(const Kallo *k, gptr o, u32 anim, s32 step, s32 top, s32 rstep, s32 rtop)
{
    switch (U8(o + 0x56)) {
    case 0: start_on(k, o, anim, 0, 1, 0xA, 1); break;
    case 1:
        Anim_Step(o);
        if (RAGE(k)) Value_Approach(o + 0xF4, (u32)rstep, (u32)rtop);
        else Value_Approach(o + 0xF4, (u32)step, (u32)top);
        break;
    }
}

/* half a turn either way (o+0x57) over the animation; the root part keeps
 * its direction until the end */
static void mv_about(const Kallo *k, gptr o)
{
    static const u16 w[2] = {0x800, 0xF800};
    static const u32 anim[2] = {2, 5};
    u32 s = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, anim[s], 2, 0x40, 5);
        Bits_Set(BITS(k), 1, 0);
        U16(k->v + 0x2E) = 0;
        break;
    case 1:
        U16(k->v + 0x2E) += (u16)((s32)(s16)w[s] / 0x30);
        Anim_Step(o);
        if (U16(o + 0x92) < 0x23) break;
        U16(k->v + 0x2E) = 0;
        gptr p = U32(o + 0x64);
        U16(p + 0x4A) = (u16)((U16(p + 0x4A) - w[s]) & 0xFFF);
        U16(o + 0x4A) = (u16)((w[s] + U16(o + 0x4A)) & 0xFFF);
        Anim_Set(o, 0, 0, 1, 0xA);
        U32(o + 0x54) = 1;
        break;
    }
}

/* facing the other player: turns to it, then stands guard (3: back from an attack) */
static void mv_face(const Kallo *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        start(k, o, 0x13, 0, 0x40, 0xA, 1);
        save_point(k, o);
        break;
    case 1: {
        Anim_Step(o);
        u16 a;
        if (U32(v + 0x4C) != o) {
            a = (u16)Angle_XZ(o + 0x40, v + 0x44);
            U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        }
        else a = U16(o + 0x4A);
        if (U16(o + 0x4A) == a && U16(o + 0x92) > 0xF) {
            U8(o + 0x56)++;
            Anim_Start(o, 0xA, 0, 0x41, 5);
        }
        break;
    }
    case 2: Anim_Step(o); break;
    case 3:
        U8(o + 0x56)--;
        zero_move(o);
        Anim_Start(o, 0xA, 0, 0x41, 0xA);
        Bits_Set(BITS(k), 1, 1);
        break;
    }
}

/* back on guard after an attack or a side step */
#define GUARD 0x30701u

/* a side step (o+0x57 the side), turning to the other player */
static void mv_step(const Kallo *k, gptr o)
{
    static const u32 anim[2] = {0xC, 0xD};
    switch (U8(o + 0x56)) {
    case 0: start(k, o, anim[U8(o + 0x57)], 1, 0x40, 5, 1); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x14 || cut(k, o, 0xF)) { U32(o + 0x54) = GUARD; zero_move(o); }
        Obj_TurnToPoint(o, RAGE(k) ? 0x78 : 0x5D, o + 0xD8);
        U16(o + 0xF0) = U16(o + 0xD0);
        break;
    }
}

/* a roar not picked by the pad: ends in the rage only */
static void mv_roar9(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        start(k, o, 0x12, 0, 0x40, 2, 1);
        Effect_Attach(o, k->m + 0x139FC);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2C && RAGE(k)) U32(o + 0x54) = 1;
        break;
    }
}

/* the rage: armour 500, 900 frames */
static void mv_rage(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 8, 0, 0x40, 5);
        Bits_Set(BITS(k), 1, 0);
        Bits_Set(BITS(k), 3, 1);
        U16(k->v + 0x3C) = 0x384;
        U16(k->v + 0x38) = 0x1F4;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* attacking in frames lo..hi: the attack bit, no hit stun */
static void hitting(const Kallo *k, gptr o, u32 lo, u32 hi)
{
    u16 f = U16(o + 0x92);
    if (f >= lo && f <= hi) {
        Bits_Set(BITS(k), 2, 1);
        U8(o + 0x17B) = 0;
    }
}

static void mv_roar(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: start(k, o, 3, 0, 0x40, 0xA, 0); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x30) U32(o + 0x54) = 1;
        hitting(k, o, 0xE, 0x2B);
        break;
    }
}

static void atk_start(const Kallo *k, gptr o, u32 anim, u32 frame)
{
    start(k, o, anim, frame, 0x40, 5, 0);
    U32(0x87E4A4)++;
}

/* the bite: it lunges on by its animation's root motion (faster in the rage) */
static void mv_bite(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: atk_start(k, o, 0xE, 2); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x23 || cut(k, o, 0x18)) { U32(o + 0x54) = GUARD; zero_move(o); break; }
        hitting(k, o, 5, 0x18);
        if (U16(o + 0x92) <= 0xD) break;
        if (RAGE(k)) U16(o + 0xF4) = (u16)((s32)S16(o + 0xD4) * 4 / 3);
        else U16(o + 0xF4) = U16(o + 0xD4);
        break;
    }
}

/* the charge */
static void mv_charge(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0xF, 8, 0x40, 5);
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(BITS(k), 1, 0);
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; break; }
        hitting(k, o, 3, 0x18);
        if (RAGE(k)) Value_Approach(o + 0xF4, 0x32, 0xFA);
        else Value_Approach(o + 0xF4, 0x28, 0xDC);
        break;
    }
}

/* the tail */
static void mv_tail(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: atk_start(k, o, 0xB, 1); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x1C || cut(k, o, 0x18)) U32(o + 0x54) = 1;
        break;
    }
}

/* the sweep (o+0x57 the side) */
static void mv_sweep(const Kallo *k, gptr o)
{
    static const u32 anim[2] = {0x10, 0x11};
    switch (U8(o + 0x56)) {
    case 0: atk_start(k, o, anim[U8(o + 0x57)], 2); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x23 || cut(k, o, 0x1E)) U32(o + 0x54) = GUARD;
        break;
    }
}

/* an attack landing: on its first frame a flash (and an effect, a sound) */
static void land_bite(const Kallo *k, gptr o)
{
    if (U8(o + 0x56) != 1) return;
    Effect_Attach(o, k->m + 0x1249C);
    Flash_Play(o, k->m + 0x14FA8);
}

static void land_tail(const Kallo *k, gptr o)
{
    if (U8(o + 0x56) == 1) Flash_Play(o, k->m + 0x14F98);
}

static void land_sweep(const Kallo *k, gptr o)
{
    if (U8(o + 0x56) != 1) return;
    Se_Play(o, 0x44);
    Flash_Play(o, k->m + 0x14FC8);
}

/* hit: the hurt kind by o+0x55; more damage from a hit in the back and
 * close (o+0x17C); the rage takes a quarter and its armour the rest, until
 * it breaks. 0x802 hurt, 0xA02 thrown by the side (close), 0xB02 while
 * attacking, 0x902 kind 3; health 0: dies the same way */
static void hurt(const Kallo *k, gptr o)
{
    gptr v = k->v;
    static const s8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(v + 0x10) = (u16)kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    if (S8(o + 0x17C) <= 1) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    if (RAGE(k)) U16(o + 0x176) >>= 2;
    U32(o + 0x54) = 0x802;
    if (S8(o + 0x17C) <= 1) {
        s16 d = (s16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
        U32(o + 0x54) = d <= 0 ? 0xA02 : 0x1000A02;
    }
    if (Bits_Check(BITS(k), 2)) U32(o + 0x54) = 0xB02;
    if (U16(v + 0x10) == 3) U32(o + 0x54) = 0x902;
    if (RAGE(k)) {
        U16(v + 0x38) -= (u16)Damage_Scale(U16(o + 0x176));
        if (S16(v + 0x38) <= 0) Bits_Set(BITS(k), 3, 0);
        else U32(o + 0x54) = 0x802;
    }
    Player_Release(o);
    U32(o + 0x50) |= 8;
    Bits_Set(BITS(k), 1, 0);
    Bits_Set(BITS(k), 2, 0);
    zero_move(o);
    if (U16(v + 0x2E)) {
        U16(o + 0x4A) = (u16)((U16(o + 0x4A) + U16(v + 0x2E)) & 0xFFF);
        U16(v + 0x2E) = 0;
    }
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) <= 0) {
        U32(o + 0x50) &= ~8u;
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = 3 | ((u32)U8(o + 0x57) << 16 | U8(o + 0x55)) << 8;
    }
    call(k->t + T_MAIN, o, U8(o + 0x54));
}

/* the flinches (kinds 8, 9): to its end frame */
static void flinch(const Kallo *k, gptr o, u32 end)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x14, 1, 0x40, 2);
        Bits_Set(BITS(k), 1, 0);
        Effect_Spawn(o, k->m + 0x12864, 4, 0xFF, 0x100, 0);
        Se_Play(o, 0x3F);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) U32(o + 0x54) = 1;
        break;
    }
}

/* the effects of a throw by the side: light (hurt kinds 2, 3) or heavy */
static gptr throw_fx(const Kallo *k, u32 s, int light)
{
    static const u32 fx[2][2] = {{0x12AE8, 0x12DBC}, {0x14460, 0x14694}};
    return k->m + fx[!light][s];
}

/* thrown by the side (o+0x57), turning to the other player */
static void thrown(const Kallo *k, gptr o)
{
    static const u32 anim[2] = {0x15, 0x17};
    u32 s = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        Anim_Start(o, anim[s], 1, 0x40, 2);
        Se_Play(o, 0x3E);
        u32 kd = U16(k->v + 0x10);
        Effect_Spawn(o, throw_fx(k, s, kd >= 2 && kd <= 3), 4, 0xFF, 0x100, 0);
        break;
    }
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= 0x1B) U32(o + 0x54) = 1;
        if (f < 0xC) Obj_TurnToPoint(o, 0x6E, o + 0xD8);
        break;
    }
    }
}

/* hit while attacking: knocked back */
static void knocked(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 4, 1, 0x40, 2);
        Effect_Spawn(o, k->m + 0x13D70, 6, 0xFF, 0x100, 0);
        Se_Play(o, 0x40);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x1D) U32(o + 0x54) = 1;
        break;
    }
}

/* the fall's end: the dust, then the other player wins */
static void fallen(gptr o, u32 shake, u32 lo, u32 hi, u32 dust)
{
    if (Anim_Step(o)) {
        U8(o + 0x56)++;
        U32(o + 0x50) |= 0x30;
        Layers_Assign();
    }
    if (U16(o + 0x92) == shake) Shake_Set(4, 0, 0x28);
    u16 f = U16(o + 0x92);
    if (f >= lo && f <= hi) U16(o + 0x42) += 6;
    if (f == dust) {
        Prop_SpawnC(1, o + 0x40, 2, 0x3C, 1, 0);
        U32(o + 0x50) |= 0x30;
    }
}

static void game_end(void)
{
    if (U8(G() + 0x927) == 2) Flag_Set(7, 0x1E, 1);
    else GameOver_Start(0);
}

static void dying_start(const Kallo *k, gptr o, u32 anim)
{
    U8(o + 0x56)++;
    Anim_Start(o, anim, 0, 0x40, 2);
    U32(o + 0x50) &= ~8u;
    zero_move(o);
    Bits_Set(BITS(k), 1, 0);
    U8(o + 0x118) &= ~0x20;
}

/* dies (kinds 8, 9, 11): staggers, falls */
static void dying(const Kallo *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: dying_start(k, o, 0x14); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0xF) {
            U8(o + 0x56)++;
            Anim_Start(o, 0x18, 0, 0x40, 2);
        }
        break;
    case 2: fallen(o, 0x30, 0x26, 0x4E, 0x56); break;
    case 3: game_end(); break;
    }
}

/* dies thrown by the side (o+0x57) */
static void dying_side(const Kallo *k, gptr o)
{
    static const u32 anim[2] = {0x15, 0x17}, fall[2] = {0x19, 0x1A}, fx[2] = {0x1347C, 0x13750};
    u32 s = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0: {
        dying_start(k, o, anim[s]);
        u32 kd = U16(o + 0x172);
        Effect_Attach(o, throw_fx(k, s, kd >= 2 && kd <= 3));
        break;
    }
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0xA) {
            U8(o + 0x56)++;
            Anim_Start(o, fall[s], 0, 0x40, 2);
            Effect_Attach(o, k->m + fx[s]);
        }
        break;
    case 2: fallen(o, 0x1B, 0x11, 0x39, 0x34); break;
    case 3: game_end(); break;
    }
}

/* ---- the entry points of both copies ------------------------------------------------------------ */
#define KALLO(n, K) \
void DC2CALL KAllo##n##_Handler(gptr o)   { handler(&K, o); }                                       \
void DC2CALL KAllo##n##_Init(gptr o)      { init(&K, o); }                                          \
void DC2CALL KAllo##n##_Normal(gptr o)    { normal(&K, o); }                                        \
void DC2CALL KAllo##n##_Hit(gptr o)       { call(K.t + T_HIT, o, U8(o + 0x55)); }                   \
void DC2CALL KAllo##n##_Die(gptr o)       { call(K.t + T_DIE, o, U8(o + 0x55)); }                   \
void DC2CALL KAllo##n##_Attack(gptr o)    { attack(&K, o); }                                        \
void DC2CALL KAllo##n##_PickStand(gptr o) { pick_stand(&K, o); }                                    \
void DC2CALL KAllo##n##_Stand(gptr o)     { mv_stand(&K, o); }                                      \
void DC2CALL KAllo##n##_PickWalk(gptr o)  { pick_walk(&K, o); }                                     \
void DC2CALL KAllo##n##_Walk(gptr o)      { mv_walk(&K, o); }                                       \
void DC2CALL KAllo##n##_Turn(gptr o)      { mv_turn(&K, o); }                                       \
void DC2CALL KAllo##n##_PickOn(gptr o)    { pick_on(&K, o); }                                       \
void DC2CALL KAllo##n##_On(gptr o)        { mv_go(&K, o, 0, 0x1E, 0x82, 0x28, 0xA0); }              \
void DC2CALL KAllo##n##_PickRun(gptr o)   { pick_run(&K, o); }                                      \
void DC2CALL KAllo##n##_Run(gptr o)       { mv_go(&K, o, 9, 0x28, 0xDC, 0x32, 0xFA); }              \
void DC2CALL KAllo##n##_PickBack(gptr o)  { pick_back(&K, o); }                                     \
void DC2CALL KAllo##n##_Back(gptr o)      { mv_go(&K, o, 1, 0x1E, -0x64, 0x28, -0x82); }            \
void DC2CALL KAllo##n##_About(gptr o)     { mv_about(&K, o); }                                      \
void DC2CALL KAllo##n##_PickFace(gptr o)  { pick_face(&K, o); }                                     \
void DC2CALL KAllo##n##_Face(gptr o)      { mv_face(&K, o); }                                       \
void DC2CALL KAllo##n##_PickStep(gptr o)  { pick_step(&K, o); }                                     \
void DC2CALL KAllo##n##_Step(gptr o)      { mv_step(&K, o); }                                       \
void DC2CALL KAllo##n##_Roar9(gptr o)     { mv_roar9(&K, o); }                                      \
void DC2CALL KAllo##n##_Rage(gptr o)      { mv_rage(&K, o); }                                       \
void DC2CALL KAllo##n##_Roar(gptr o)      { mv_roar(&K, o); }                                       \
void DC2CALL KAllo##n##_PickBite(gptr o)  { pick_bite(&K, o); }                                     \
void DC2CALL KAllo##n##_Bite(gptr o)      { mv_bite(&K, o); }                                       \
void DC2CALL KAllo##n##_Steer13(gptr o)   { steer(&K, o); }                                         \
void DC2CALL KAllo##n##_Charge(gptr o)    { mv_charge(&K, o); }                                     \
void DC2CALL KAllo##n##_Steer14(gptr o)   { steer(&K, o); }                                         \
void DC2CALL KAllo##n##_Tail(gptr o)      { mv_tail(&K, o); }                                       \
void DC2CALL KAllo##n##_Sweep(gptr o)     { mv_sweep(&K, o); }                                      \
void DC2CALL KAllo##n##_LandBite(gptr o)  { land_bite(&K, o); }                                     \
void DC2CALL KAllo##n##_LandTail(gptr o)  { land_tail(&K, o); }                                     \
void DC2CALL KAllo##n##_LandSweep(gptr o) { land_sweep(&K, o); }                                    \
void DC2CALL KAllo##n##_Hurt(gptr o)      { hurt(&K, o); }                                          \
void DC2CALL KAllo##n##_Flinch8(gptr o)   { flinch(&K, o, 0x12); }                                  \
void DC2CALL KAllo##n##_Flinch9(gptr o)   { flinch(&K, o, 0x20); }                                  \
void DC2CALL KAllo##n##_Thrown(gptr o)    { thrown(&K, o); }                                        \
void DC2CALL KAllo##n##_Knocked(gptr o)   { knocked(&K, o); }                                       \
void DC2CALL KAllo##n##_Dying(gptr o)     { dying(&K, o); }                                         \
void DC2CALL KAllo##n##_DyingSide(gptr o) { dying_side(&K, o); }                                    \
void DC2CALL KAllo##n##_Glow(gptr o)      { glow(&K, o); }                                          \
u32  DC2CALL KAllo##n##_Cut(gptr o, u32 frame) { return (u32)cut(&K, o, frame); }
KALLO(1, K1)
KALLO(2, K2)
