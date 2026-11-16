/* The compsognathus of the extra game's duel (object types 39 and 40;
 * handlers 0x4692F0 and 0x46A5C0). The two copies of the code are the same
 * but for the player they belong to: their pads, the other player's object,
 * the variables, tables and model (copy 2's model data sits 0x2A500 lower).
 * It dies from one hit (health 1). Its super move calls a pack: for 150
 * frames a compsognathus comes every other frame (objects of the same type
 * with o+0x10C 1, run by the same handler), runs at the other player, bites
 * it once and is gone.
 * States (o+0x54): 0 init, 1 normal (by o+0x55: the pad picks the move,
 * then the move runs), 2 hit (0..7 hurt, 8 flinch), 3 die (8).
 * Moves: 0 stand, 1 walk, 2 turn to the other player, 3 walk on, 4 run,
 * 5 back, 6 the spin (half a turn; o+0x57 the side), 7 the call (each one
 * adds to the pack's call, up to 5), 8 the leap (its attack).
 * Variables (v): +0 the pad's new presses, +8 bits (1 moving), +0x10 the
 * hurt kind, +0x12 o+0x54..0x57 of the last frame, +0x2C the other player's
 * place, +0x3C the spin's turn so far, +0x3E the pack's time, +0x40 the
 * calls (3 for the super move), +0x48 the point turned to, +0x50 the pack
 * running, +0x54 the other player, +0x58 the pad held, +0x5A the angle from
 * the head to the other player, +0x5C the hit reactions by move (u16). */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v, t, m;        /* variables, tables, model */
    u32 foe;            /* the other player's object index */
    u32 pad, held;      /* pad words: new presses, held */
    u32 d;              /* how much lower its model data is than copy 1's */
} Kcomp;

static const Kcomp K1 = {0x74B4F0, 0x723608, 0x662500, 2, 0x87DC94, 0x87DC08, 0};
static const Kcomp K2 = {0x74B560, 0x723730, 0x638000, 1, 0x87DC92, 0x87DE0A, 0x2A500};

/* tables (t+; the attack spheres at t, t+0x10, t+0x20) */
#define T_COMBO 0x2Cu
#define T_MAIN  0x3Cu
#define T_PICK  0x4Cu
#define T_MOVE  0x70u
#define T_HIT   0x94u
#define T_DIE   0xB8u
#define T_LAND  0xDCu
#define T_DMG   0x100u
#define T_PACK  0x114u

#define BITS(k) ((k)->v + 8)
#define HELD(k) U16((k)->v + 0x58)
#define PAD(k)  U8((k)->v)
#define FOE(k)  U32((k)->v + 0x54)
/* data in the model file, by copy 1's address */
#define D(k, a) ((gptr)((a) - (k)->d))

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }

static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }

static void pack_handler(const Kcomp *k, gptr o);
static void attack(const Kcomp *k, gptr o);

static void handler(const Kcomp *k, gptr o)
{
    gptr v = k->v;
    if (U16(o + 0x10C) == 1) { pack_handler(k, o); return; }
    U16(v + 0x58) = U16(k->held);
    gptr foe = U32(0x87E760) + k->foe * 0x260;
    U16(v) = U16(k->pad);
    U32(v + 0x54) = foe;
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(v + 0x2C) = U16(FOE(k) + 0x40);
    U16(o + 0xDA) = U16(v + 0x2E) = U16(FOE(k) + 0x42);
    U16(o + 0xDC) = U16(v + 0x30) = U16(FOE(k) + 0x44);
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(v + 0x5A) = p ? (u16)Angle_DiffTo(p + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    Obj_SavePos(o);
    call(k->t + T_MAIN, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    s32 aa = S16(v + 0x5A); if (aa < 0) aa = -aa;
    if (aa < 0x500 && FOE(k) != o && Bits_Check(BITS(k), 1)) U32(o + 0x158) = 2;
    else U32(o + 0x158) = 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    U32(o + 0x50) |= 0x20;          /* never pushes against other objects */
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (U8(o + 0x17B)) U8(o + 0x17B)--;
    attack(k, o);
    if (U16(v + 0x3E)) U16(v + 0x3E)--;
    U8(v + 0x12) = U8(o + 0x54); U8(v + 0x13) = U8(o + 0x55);
    U8(v + 0x14) = U8(o + 0x56); U8(v + 0x15) = U8(o + 0x57);
}

/* model, health 1, head, the other player as the head's target */
static void init(const Kcomp *k, gptr o)
{
    U32(o + 0x50) = 0xB;
    Model_Setup(o, k->m);
    U8(o + 0x54)++;
    U32(o + 0x88) = D(k, 0x662D68);
    U16(o + 0x108) = 1; U16(o + 0x10A) = 1;
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x10; U32(o + 0x134) = k->t + 0x20;
    zero_move(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x16, 0, 1, 0);
    Obj_Set118(o, 0, 0x1388, 0x1388);
    Head_Reset(o);
    U8(o + 0x15F) = 2; U8(o + 0x160) = 1;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(FOE(k) + 0xFF);
    gptr d = U32(FOE(k) + 0x134);
    U8(o + 0x15E) = d ? U8(d + 0xA) : 0;
    Bits_Set(BITS(k), 1, 0);
    U32(o + 0x54) = 1;
    U8(o + 0x118) |= 0x20;
    U16(k->v + 0x50) = 0;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

static void pack_spawn(const Kcomp *k, gptr o);

/* the super move by the button sequence (after 3 calls, and not while the
 * pack still comes); the pack comes every other frame of its time; event
 * flag 7/0x1B while some of it runs; then the pad's choice and the move */
static void normal(const Kcomp *k, gptr o)
{
    gptr v = k->v;
    U16(o + 0x174) = 0;
    if (Combo_Input(k->t + T_COMBO, U16(v))
        && (S16(v + 0x40) >= 3 || Flag_Check(0, 0xB))
        && !U16(v + 0x3E)) {
        U16(v + 0x3E) = 0x96;
        U16(v + 0x40) = 0;
        Kof_Super(o);
    }
    if (U16(v + 0x3E) & 1) pack_spawn(k, o);
    Flag_Set(7, 0x1B, U16(v + 0x50) ? 1 : 0);
    call(k->t + T_PICK, o, U8(o + 0x55));
    call(k->t + T_MOVE, o, U8(o + 0x55));
}

/* its attack spheres against the other player, then the landing */
static void attack(const Kcomp *k, gptr o)
{
    u32 s = U8(o + 0x55);
    Kof_Attack(o, k->foe, U16(k->t + T_DMG + s * 2), U16(k->v + 0x5C + s * 2), 0x1F);
    if (S8(o + 0x119) >= 0) call(k->t + T_LAND, o, U8(o + 0x55));
}

/* o+0x54 as a dword: state 1, move m, the side (the head's angle) */
static u32 side(const Kcomp *k) { return S16(k->v + 0x5A) <= 0; }
#define MOVE(m) (0x01u | (m) << 8)
#define SIDE(m) (side(k) << 24 | MOVE(m))

static void steer(const Kcomp *k, gptr o)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += 0x64;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= 0x64;
}

/* standing: 8 pressed (or held with 4) turns to the other player, 0x20 the
 * leap, 2 the spin, 0x1000 walk on (with 0x40 run), 0x4000 back,
 * 0x2000/0x8000 walk, 1 the call; with a move it leaves the ground's height */
static void pick_stand(const Kcomp *k, gptr o)
{
    u32 h = HELD(k), p = PAD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if ((h & 8) && (p & 4)) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(8);
    else if (p & 2) U32(o + 0x54) = SIDE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (h & 0x2000) U32(o + 0x54) = MOVE(1);
    else if (h & 0x8000) U32(o + 0x54) = MOVE(1);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
    if (U8(o + 0x55)) U16(o + 0x42) = U16(o + 0x10E);
}

static void pick_walk(const Kcomp *k, gptr o)
{
    u32 h = HELD(k), p = PAD(k);
    if (p & 8) U32(o + 0x54) = MOVE(2);
    else if (h & 0x20) U32(o + 0x54) = MOVE(8);
    else if (p & 2) U32(o + 0x54) = SIDE(6);
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? MOVE(4) : MOVE(3);
    else if (h & 0x4000) U32(o + 0x54) = MOVE(5);
    else if (!(h & 0xA000)) U32(o + 0x54) = MOVE(0);
    else if (p & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o);
}

/* walking on (0x1000) and running (with 0x40); back (0x4000): let go, 8
 * turns to the other player */
static void pick_go(const Kcomp *k, gptr o, u32 key, int run)
{
    u32 h = HELD(k);
    if (!(h & key) && (h & 8)) U32(o + 0x54) = MOVE(2);
    else if (!(h & key)) U32(o + 0x54) = MOVE(0);
    else if (h & 0x20) U32(o + 0x54) = MOVE(8);
    else if (PAD(k) & 2) U32(o + 0x54) = SIDE(6);
    else if (run == 1 && (h & 0x40)) U32(o + 0x54) = MOVE(4);
    else if (run == 2 && !(h & 0x40)) U32(o + 0x54) = MOVE(3);
    else if (PAD(k) & 1) U32(o + 0x54) = MOVE(7);
    steer(k, o);
}

/* standing: at the animation's end back on the ground's height */
static void mv_stand(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, 0x16, 0, 0x41, 5);
        break;
    case 1:
        if (Anim_Step(o)) U16(o + 0x42) = U16(o + 0x10E);
        break;
    }
}

static void mv_walk(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, 2, 0, 0x41, 5);
        break;
    case 1: Anim_Step(o); break;
    }
}

/* turns to where the other player was (0xFA a frame, 20 frames at most) */
static void mv_turn(const Kcomp *k, gptr o)
{
    gptr v = k->v;
    switch (U8(o + 0x56)) {
    case 0:
        if (FOE(k) == o) { U32(o + 0x54) = MOVE(0); return; }
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, 2, 0, 0x41, 5);
        U16(v + 0x48) = U16(o + 0xD8); U16(v + 0x4A) = U16(o + 0xDA); U16(v + 0x4C) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, v + 0x48);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0xFA, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = MOVE(0);
        break;
    }
    }
}

/* walking on, running and back: the speed comes up to its top */
static void mv_go(const Kcomp *k, gptr o, u32 step, s32 top)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Bits_Set(BITS(k), 1, 1);
        Anim_Start(o, 2, 0, 1, 5);
        break;
    case 1: Anim_Step(o); break;
    }
    Value_Approach(o + 0xF4, step, (u32)top);
}

/* the spin: half a turn (by the side), 0x100 a frame */
static void mv_spin(const Kcomp *k, gptr o)
{
    static const s16 by[2] = {0x100, -0x100};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 2, 0, 1, 5);
        Bits_Set(BITS(k), 1, 0);
        U16(k->v + 0x3C) = 0;
        break;
    case 1:
        Anim_Step(o);
        U16(o + 0x4A) += (u16)by[U8(o + 0x57)];
        U16(k->v + 0x3C) += 0x100;
        if (S16(k->v + 0x3C) >= 0x800) U32(o + 0x54) = MOVE(0);
        break;
    }
}

/* the call: one more toward the pack (5 at most) */
static void mv_call(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(BITS(k), 1, 0);
        Anim_Start(o, 3, 0, 0x40, 5);
        if (S16(k->v + 0x40) < 5) U16(k->v + 0x40)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = MOVE(0);
        break;
    }
}

/* the leap: forward up to 0xB4 */
static void mv_leap(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 0x40, 5);
        Bits_Set(BITS(k), 1, 0);
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = MOVE(0);
        Value_Approach(o + 0xF4, 0x1E, 0xB4);
        break;
    }
}

/* a bite landing: effect, flash and sound */
static void bite_lands(const Kcomp *k, gptr o)
{
    Effect_Attach(o, D(k, 0x664F68));
    Flash_Play(o, D(k, 0x665034));
    Se_Play(o, 0x39);
}

static void land_leap(const Kcomp *k, gptr o)
{
    if (U8(o + 0x56) == 1) bite_lands(k, o);
}

/* hit: the hurt kind by o+0x55, more damage from a hit in the back; every
 * hit flinches; health below 0: dies */
static void hurt(const Kcomp *k, gptr o)
{
    gptr v = k->v;
    static const s8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(v + 0x10) = (u16)kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    U32(o + 0x54) = 0x802;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(BITS(k), 1, 0);
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x50) &= ~8u;
        U32(o + 0x54) = 0x803;
    }
    call(k->t + T_MAIN, o, U8(o + 0x54));
}

/* the flinch: back on the ground's height; at its end standing (or turning
 * to the other player) */
static void flinch(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x42) = U16(o + 0x10E);
        zero_move(o);
        Anim_Start(o, 1, 0, 0x40, 2);
        Bits_Set(BITS(k), 1, 0);
        break;
    case 1:
        if (Anim_Step(o))
            U32(o + 0x54) = !(HELD(k) & 0x1000) && (HELD(k) & 8) ? MOVE(2) : MOVE(0);
        break;
    }
}

/* dies: falls, then the other player wins */
static void dying(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x42) = 0;
        zero_move(o);
        Anim_Start(o, 1, 0, 0x40, 2);
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

/* ---- the pack ------------------------------------------------------------------------------------ */

/* one of the pack: its state, the move; gone when done */
static void pack_handler(const Kcomp *k, gptr o)
{
    call(k->t + T_PACK, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (U32(o + 0x50) & 0x40000000) {
        Obj_Disable(o);
        if (U16(k->v + 0x50)) U16(k->v + 0x50)--;
    }
}

/* one of the pack: the model, health 1, not drawn in the layers */
static void pack_init(const Kcomp *k, gptr o)
{
    U32(o + 0x50) = 3;
    Model_Setup(o, k->m);
    U32(o + 0x50) |= 0x30;
    U8(o + 0x54)++;
    U32(o + 0x88) = D(k, 0x662D68);
    U16(o + 0x108) = 1; U16(o + 0x10A) = 1;
    U32(o + 0x12C) = k->t; U32(o + 0x128) = k->t + 0x10; U32(o + 0x134) = k->t + 0x20;
    zero_move(o);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 4, 0, 1, 0);
    Obj_Set118(o, 0, 0x1388, 0x1388);
    U8(o + 0x118) |= 0x20;
    U32(o + 0x54) = 1;
    U16(k->v + 0x50)++;
}

/* one of the pack: runs at the other player (60 frames at most); its bite
 * (tried every other frame) lands once, 9 frames before it goes */
static void pack_run(const Kcomp *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U8(o + 0x57) = 0x3C;
        Anim_Start(o, 4, 0, 1, 0);
        break;
    case 1:
        Anim_Step(o);
        Obj_TurnToPoint(o, 0x64, k->v + 0x2C);
        if (U8(o + 0x57) & 1) {
            Kof_Attack(o, k->foe, 0x64, U16(k->v + 0x5C + U8(o + 0x55) * 2), 0x1F);
            if (S8(o + 0x119) >= 0) {
                bite_lands(k, o);
                U8(o + 0x57) = 0xA;
            }
        }
        if (!--U8(o + 0x57)) U32(o + 0x50) |= 0x40000000;
        break;
    }
    Value_Approach(o + 0xF4, 0x14, 0xC8);
}

/* one of the pack comes: 0x1388 (give or take 0xFF) from the other player
 * at a random angle, facing it */
static void pack_spawn(const Kcomp *k, gptr o)
{
    gptr n = Obj_AllocEnemy();
    if (!n) return;
    gptr p = Tmp_Push(8);
    U16(p) = 0; U16(p + 2) = 0;
    U16(p + 4) = (u16)(0x1388 + 2 * (s8)Rand8());
    u32 a = (u32)Rand8() << 4;
    Vec_RotY(p, p, a);
    U16(n + 0x42) = 0;
    U16(n + 0x40) = (u16)(U16(p) + U16(k->v + 0x2C));
    U16(n + 0x44) = (u16)(U16(p + 4) + U16(k->v + 0x30));
    Tmp_Pop(8);
    U16(n + 0x4A) = (u16)(-a & 0xFFF);
    U16(n + 0x10C) = 1;
    U8(n + 0x58) = U8(o + 0x58);
    Orig_Call1(U32(U32(0x87DCA0) + U8(n + 0x58) * 4), n);
}

/* ---- the entry points of both copies ------------------------------------------------------------ */
#define KCOMP(n, K) \
void DC2CALL KComp##n##_Handler(gptr o)     { handler(&K, o); }                                      \
void DC2CALL KComp##n##_Init(gptr o)        { init(&K, o); }                                         \
void DC2CALL KComp##n##_Normal(gptr o)      { normal(&K, o); }                                       \
void DC2CALL KComp##n##_Hit(gptr o)         { call(K.t + T_HIT, o, U8(o + 0x55)); }                  \
void DC2CALL KComp##n##_Die(gptr o)         { call(K.t + T_DIE, o, U8(o + 0x55)); }                  \
void DC2CALL KComp##n##_Attack(gptr o)      { attack(&K, o); }                                       \
void DC2CALL KComp##n##_PickStand(gptr o)   { pick_stand(&K, o); }                                   \
void DC2CALL KComp##n##_Stand(gptr o)       { mv_stand(&K, o); }                                     \
void DC2CALL KComp##n##_PickWalk(gptr o)    { pick_walk(&K, o); }                                    \
void DC2CALL KComp##n##_Walk(gptr o)        { mv_walk(&K, o); }                                      \
void DC2CALL KComp##n##_Turn(gptr o)        { mv_turn(&K, o); }                                      \
void DC2CALL KComp##n##_PickOn(gptr o)      { pick_go(&K, o, 0x1000, 1); }                           \
void DC2CALL KComp##n##_On(gptr o)          { mv_go(&K, o, 0x14, 0x50); }                            \
void DC2CALL KComp##n##_PickRun(gptr o)     { pick_go(&K, o, 0x1000, 2); }                           \
void DC2CALL KComp##n##_Run(gptr o)         { mv_go(&K, o, 0x1E, 0xC8); }                            \
void DC2CALL KComp##n##_PickBack(gptr o)    { pick_go(&K, o, 0x4000, 0); }                           \
void DC2CALL KComp##n##_Back(gptr o)        { mv_go(&K, o, 0x1E, -0x46); }                           \
void DC2CALL KComp##n##_Spin(gptr o)        { mv_spin(&K, o); }                                      \
void DC2CALL KComp##n##_Call(gptr o)        { mv_call(&K, o); }                                      \
void DC2CALL KComp##n##_Steer8(gptr o)      { steer(&K, o); }                                        \
void DC2CALL KComp##n##_Leap(gptr o)        { mv_leap(&K, o); }                                      \
void DC2CALL KComp##n##_LandLeap(gptr o)    { land_leap(&K, o); }                                    \
void DC2CALL KComp##n##_Hurt(gptr o)        { hurt(&K, o); }                                         \
void DC2CALL KComp##n##_Flinch(gptr o)      { flinch(&K, o); }                                       \
void DC2CALL KComp##n##_Dying(gptr o)       { dying(&K, o); }                                        \
void DC2CALL KComp##n##_PackHandler(gptr o) { pack_handler(&K, o); }                                 \
void DC2CALL KComp##n##_PackInit(gptr o)    { pack_init(&K, o); }                                    \
void DC2CALL KComp##n##_PackRun(gptr o)     { pack_run(&K, o); }                                     \
void DC2CALL KComp##n##_PackSpawn(gptr o)   { pack_spawn(&K, o); }
KCOMP(1, K1)
KCOMP(2, K2)
