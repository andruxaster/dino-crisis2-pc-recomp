/* Two humans of the extra game fighting hand to hand (area 9 object types
 * 20 and 21, handlers 0x480040 and 0x473310; PS1 overlays WEP0D, WEP0E).
 * The two copies of the code differ in their numbers (health 1200 / 1000,
 * speeds, animations, sounds, effects), in the buttons that pick an attack,
 * in walking back and in their attacks; they share the dying (0x480840).
 * States (o+0x54): 0 init, 1 normal, 2 hit, 3 die.
 * Normal (o+0x55): 0 stand, 1 walk, 2 run, 3 walk back, 4 attack (o+0x56:
 * 0 punches, 1 kick, 2 spin, 3 low strike, 4 the super move; o+0x57 the
 * step), 5 turn on the spot, 6 turn about, 7 dodge, 8/9 turn to the first /
 * next target. Hit (o+0x55): 0 and 4 light, the others knocked down.
 * The pad: v+0 its new presses, v+2 held (copied from 0x87DC94 / 0x87DC08
 * each frame); bits 0x20 punch, 0x10 kick, 0x40 with them a variant, 1
 * dodge, 2 turn about, 4 next target, 8 aim, 0x1000 forward, 0x4000 back,
 * 0x2000 / 0x8000 turn. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

typedef struct {
    u32 v;                          /* the pad's copies */
    u16 health;
    u32 t_main, t_normal, t_hit, t_attack, combo;
    u32 cyl, wall, spheres;         /* o+0x12C, o+0x128, o+0x134 */
    u32 s118b, s118c;               /* Obj_Set118's sizes */
    u8 two;                         /* the second copy (type 21) */
    u16 turn, walk_speed;
    u8 walk_blend;
} Xh;

static const Xh K20 = {0x74E62C, 0x4B0, 0x7381E4, 0x7381F4, 0x73821C, 0x73823C, 0x7381D4,
                       0x738198, 0x738188, 0x7381B0, 0x170C, 0x1F40, 0, 0x5A, 0x64, 6};
static const Xh K21 = {0x74DB8C, 0x3E8, 0x737700, 0x737710, 0x737738, 0x737758, 0x7376F0,
                       0x7376C8, 0x7376B8, 0x7376D8, 0x12C0, 0x1518, 1, 0x64, 0x50, 3};

#define NEW(k)   U8((k)->v)
#define HELD(k)  U32((k)->v + 2)                /* (read as a dword: the bits tested are the low 16) */
#define PX()     U32(0x87FD60)
#define G32(off) U32(G() + (off))

static void zero_move(gptr o) { U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; }

static void handler(const Xh *k, gptr o)
{
    U16(k->v + 2) = U16(0x87DC08);
    U32(0x87FD60) = o + 0x1C0;
    U16(k->v) = U16(0x87DC94);
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = k->health;
    Obj_SavePos(o);
    Orig_Call1(U32(k->t_main + U8(o + 0x54) * 4), o);
    U16(PX() + 0x36)++;
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Head_Update(o);
    Obj_UpdateMatrix(o);
    Obj_AnchorFoot(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (!(U8(o + 0x50) & 0x80)) Zone_Update(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
}

static void init(const Xh *k, gptr o)
{
    U16(o + 0x108) = k->health; U16(o + 0x10A) = k->health;
    U32(o + 0x50) = 3;
    U16(o + 0x40) = U16(o + 0xE0) = U16(G() + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    DrawSync(o);                                    /* (the object left on the stack) */
    u32 w = U16(G() + 0x10B4);
    Stage_Load(w & 0xFF, w >> 8);
    U16(PX() + 0x3A) = 0x80;
    U32(o + 0x12C) = k->cyl; U32(o + 0x128) = k->wall; U32(o + 0x134) = k->spheres;
    U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
    Obj_Set118(o, 1, k->s118b, k->s118c);
    U8(o + 0x118) |= 0x20;
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U16(o + 0x16A) = 0x100; U16(o + 0x16E) = 0x280;
    U8(o + 0x15D) = 0xFF; U8(o + 0x15E) = 0;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x300;
    U8(o + 0x15C) = 0; U32(o + 0x188) = 0;
    Obj_SetFeet(o, 0xC, 0xC);
    Anim_Start(o, 0x16, 0, 1, 4);
    U32(o + 0x54) = 0x10001;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* at half health or less (or with the cheat) the button sequence starts
   the super move */
static void normal(const Xh *k, gptr o)
{
    if ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        if (U8(o + 0x55) != 4 && U8(o + 0x56) != 4) {
            if (k->two) Flag_Set(7, 0x1B, 0);
            if (Combo_Input(k->combo, U16(k->v))) {
                U32(o + 0x54) = 0x40401;
                Kof_Super(o);
            }
        }
    Orig_Call1(U32(k->t_normal + U8(o + 0x55) * 4), o);
}

/* down onto the floor below (200 a frame), never under it */
static void to_floor(gptr o)
{
    s16 h = (s16)Floor_Height(o + 0xE0, o + 0x40, 0x80, 0);
    s16 layer = (s16)(h / -1000);
    U16(o + 0x5A) = (u16)layer;
    s32 lim = -1000 * layer;
    if (lim > S16(o + 0x42)) U16(o + 0x42) += 0xC8;
    if (lim < S16(o + 0x42)) U16(o + 0x42) = (u16)(-1000 * layer);
}

/* the damage taken; dead below 0 */
static void take_damage(const Xh *k, gptr o)
{
    zero_move(o);
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = k->health;
    U8(o + 0x11B) = 0;
    U16(o + 0x174) = 1;
    U8(o + 0x56)++;
    if (S16(o + 0x108) < 0) { U32(o + 0x54) = 3; U8(o + 0x50) &= 0xF7; }
}

static void back_up(const Xh *k, gptr o) { U16(o + 0x174) = 0; U32(o + 0x54) = U8(k->v + 2) & 8 ? 0x801 : 1; }

static void hurt_light(const Xh *k, gptr o)
{
    to_floor(o);
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0xC, 0, 0x40, 4);
        take_damage(k, o);
        if (k->two) { Effect_Attach(o, Rand8() & 0x10 ? 0x678A4C : 0x678FE8); Se_Play(o, 0x41); }
        else { Effect_Attach(o, U32(0x703500 + (Rand8() & 3) * 4)); Se_Play(o, 0x48); }
        break;
    case 1:
        if (Anim_Step(o)) back_up(k, o);
        break;
    }
}

/* knocked down (anim; fx the table of its hit effects, 0: none) and up again */
static void hurt_down(const Xh *k, gptr o, u32 anim, gptr fx)
{
    to_floor(o);
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, anim, 0, 0x40, 4);
        take_damage(k, o);
        if (fx) Effect_Attach(o, U32(fx + (Rand8() & 3) * 4));
        Se_Play(o, k->two ? 0x42 : 0x49);
        break;
    case 1:
        if (Anim_Step(o)) { Anim_Start(o, 0xF, 0, 0x40, 8); U8(o + 0x56)++; }
        break;
    case 2:
        if (Anim_Step(o)) { back_up(k, o); U8(o + 0x17B) = 5; }
        break;
    }
}

/* the attacks the held buttons start (1 if one did) */
static int pick_attack(const Xh *k, gptr o, u32 h)
{
    if (h & 0x20) {
        U32(o + 0x54) = (h & 0x1000) && (h & 0x40) ? 0x10401 : (h & 0x4000) ? 0x2000401 : k->two ? 0x30401 : 0x401;
        return 1;
    }
    if (h & 0x10) {
        if (k->two) U32(o + 0x54) = h & 0x4000 ? 0x20401 : 0x401;
        else U32(o + 0x54) = (h & 0x8000) ? 0x20401 : (h & 0x2000) ? 0x1020401 : 0x30401;
        return 1;
    }
    return 0;
}

/* late in an attack: the next one (0x20 / 0x10) */
static void chain(gptr o, u32 h)
{
    if (h & 0x20) U32(o + 0x54) = 0x30401;
    else if (h & 0x10) U32(o + 0x54) = h & 0x4000 ? 0x20401 : 0x401;
}

static void turn_by_pad(const Xh *k, gptr o, u16 step)
{
    if (HELD(k) & 0x2000) U16(o + 0x4A) += step;
    if (HELD(k) & 0x8000) U16(o + 0x4A) -= step;
}

static void stand(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: Anim_Start(o, 0x16, 0, 0x49, 0xA); U8(o + 0x56)++; zero_move(o); break;
    case 1: Anim_Step(o); break;
    }
    u8 nw = NEW(k);
    u32 h = HELD(k);
    if (nw & 8) { U32(o + 0x54) = 0x801; return; }
    if ((h & 8) && (nw & 4)) { U32(o + 0x54) = 0x901; return; }
    if (pick_attack(k, o, h)) return;
    if (nw & 2) { U32(o + 0x54) = 0x601; return; }
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x201 : 0x101; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x301; return; }
    if (h & 0xA000) { U32(o + 0x54) = 0x501; return; }
    if (nw & 1) U32(o + 0x54) = 0x701;
}

static void walk(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 0, 0, 1, k->walk_blend); U8(o + 0x56)++; U32(o + 0xF0) = 0;
        if (k->two) U16(o + 0xF4) = 0;
        break;
    case 1: Anim_Step(o); U16(o + 0xF4) = k->walk_speed; break;
    }
    turn_by_pad(k, o, k->turn);
    u8 nw = NEW(k);
    u32 h = HELD(k);
    if (nw & 8) { U32(o + 0x54) = 0x801; return; }
    if ((h & 8) && (nw & 4)) { U32(o + 0x54) = 0x901; return; }
    if (pick_attack(k, o, h)) return;
    if (nw & 2) { U32(o + 0x54) = 0x601; return; }
    if (h & 0x1000) {
        if (h & 0x40) U32(o + 0x54) = 0x201;
        else if (nw & 1) U32(o + 0x54) = 0x701;
        return;
    }
    if (h & 0x4000) { U32(o + 0x54) = 0x301; return; }
    if (nw & 1) { U32(o + 0x54) = 0x701; return; }
    U32(o + 0x54) = (h & 8) ? 0x801 : 1;
}

static void run(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, 1, 0, 1, 6); U8(o + 0x56)++; U32(o + 0xF0) = 0;
        if (k->two) U16(o + 0xF4) = 0;
        break;
    case 1: Anim_Step(o); U16(o + 0xF4) = 0xD2; break;
    }
    turn_by_pad(k, o, k->turn);
    u32 h = HELD(k);
    if (pick_attack(k, o, h)) return;
    u8 nw = NEW(k);
    if (nw & 2) { U32(o + 0x54) = 0x601; return; }
    if (h & 0x1000) {
        if (!(h & 0x40)) U32(o + 0x54) = 0x101;
        else if (nw & 1) U32(o + 0x54) = 0x701;
        return;
    }
    if (h & 0x4000) { U32(o + 0x54) = 0x301; return; }
    if (nw & 1) { U32(o + 0x54) = 0x701; return; }
    U32(o + 0x54) = (h & 8) ? 0x801 : 1;
}

/* walking back: what the buttons choose */
static void back_pick(const Xh *k, gptr o)
{
    turn_by_pad(k, o, k->turn);
    u32 h = HELD(k);
    if (pick_attack(k, o, h)) return;
    u8 nw = NEW(k);
    if (nw & 2) { U32(o + 0x54) = 0x601; return; }
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x201 : 0x101; return; }
    if (!(h & 0x4000)) { U32(o + 0x54) = (h & 8) ? 0x801 : 1; return; }
    if (k->two && (nw & 0x40)) { U32(o + 0x54) = 0x20301; return; }   /* (the back kick) */
    if (nw & 1) U32(o + 0x54) = 0x701;
}

static void turn_spot(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0: Anim_Start(o, 0, 0, 1, k->walk_blend); U8(o + 0x56)++; zero_move(o); break;
    case 1: Anim_Step(o); break;
    }
    turn_by_pad(k, o, k->turn);
    u8 nw = NEW(k);
    u32 h = HELD(k);
    if (nw & 8) { U32(o + 0x54) = 0x801; return; }
    if ((h & 8) && (nw & 4)) { U32(o + 0x54) = 0x901; return; }
    if (pick_attack(k, o, h)) return;
    if (nw & 2) { U32(o + 0x54) = 0x601; return; }
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x201 : 0x101; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x301; return; }
    if (!(h & 0xA000)) { U32(o + 0x54) = 1; return; }
    if (nw & 1) U32(o + 0x54) = 0x701;
}

static void turn_about(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        if (k->two) Anim_Start(o, 3, 0xA, 0, 4); else Anim_Start(o, 3, 0, 1, 2);
        U8(o + 0x56)++; zero_move(o);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        U16(o + 0x4A) = (u16)((U16(o + 0x4A) - 0x800) & 0xFFF);
        U16(U32(o + 0x64) + 0x4A) = (u16)((U16(U32(o + 0x64) + 0x4A) - 0x800) & 0xFFF);
        Anim_Set(o, 0x16, 0, 1, 0);
        U32(o + 0x54) = 0x10001;
        break;
    }
}

static void dodge(const Xh *k, gptr o)
{
    switch (U8(o + 0x56)) {
    case 0:
        if (k->two) {
            Anim_Start(o, Rand8() & 1 ? 4 : 5, 0, 0x40, 4);
            U8(o + 0x56)++; zero_move(o);
            return;
        }
        zero_move(o);
        if (HELD(k) & 0x1000) { Anim_Start(o, 6, 0, 0, 4); U8(o + 0x56)++; U16(o + 0xF4) = 0xA; return; }
        Anim_Start(o, Rand8() & 1 ? 4 : 5, 0, 0x40, 4);
        U8(o + 0x56)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = U8(k->v + 2) & 8 ? 0x801 : 1;
        break;
    }
}

/* turn to the first / the next target */
static void turn_to_target(const Xh *k, gptr o, int next)
{
    switch (U8(o + 0x56)) {
    case 0: {
        Anim_Start(o, 0, 0, k->two ? 0 : 4, 6);
        zero_move(o);
        if (Flag_Check(2, 0xE)) U32(PX() + 0x14) = 0;
        else if (next) Target_Next(U32(PX() + 0x14));
        else Target_Find(o, 0x800);
        gptr t = U32(PX() + 0x14);
        if (!t) { U32(o + 0x54) = 1; return; }
        gptr a = Tmp_Push(8);
        Angles_LookAt(t + 0x40, o + 0x40, a);
        U16(PX() + 0x20) = U16(a + 2);
        Tmp_Pop(8);
        U8(o + 0x56)++;
        break;
    }
    case 1: {
        Anim_Step(o);
        u16 d = (u16)Angle_Approach(U16(o + 0x4A), 0xFA, U16(PX() + 0x20));
        U16(o + 0x4A) += d;
        if (!d) U32(o + 0x54) = 1;
        break;
    }
    }
}

/* the hit of a blow: the part's position against the targets (kind),
   the body turned {0x400, yaw, 0} */
static u32 blow(gptr o, u32 part, u32 kind)
{
    gptr f = Tmp_Push(0x28), r = f, m = f + 8;
    U16(r) = 0x400; U16(r + 2) = U16(o + 0x4A); U16(r + 4) = 0;
    RotMatrixYXZ(r, m);
    Obj_UpdateMatrix(o);
    u32 h = Weapon_Hit(m, Obj_Part(U32(o + 0x64), part) + 0x40, kind);
    Tmp_Pop(0x28);
    return h;
}

static void start_attack(gptr o, u32 anim, u32 frame, u32 fl, u32 blend)
{
    Anim_Start(o, anim, frame, fl, blend);
    zero_move(o);
    U8(o + 0x57)++;
    G32(0x684)++;
}

/* ---- type 20 ---------------------------------------------------------------- */
void DC2CALL Xh20_Handler(gptr o) { handler(&K20, o); }               /* 0x480040 */
void DC2CALL Xh20_Init(gptr o) { init(&K20, o); }                     /* 0x480170 */
void DC2CALL Xh20_Normal(gptr o) { normal(&K20, o); }                 /* 0x480310 */
void DC2CALL Xh20_Hit(gptr o) { Orig_Call1(U32(0x73821C + U8(o + 0x55) * 4), o); }   /* 0x480820 */
void DC2CALL Xh20_Attack(gptr o) { Orig_Call1(U32(0x73823C + U8(o + 0x56) * 4), o); } /* 0x480E10 */
void DC2CALL Xh20_HurtLight(gptr o) { hurt_light(&K20, o); }          /* 0x480390 */
void DC2CALL Xh20_HurtDown(gptr o) { hurt_down(&K20, o, 0xD, 0); }    /* 0x480500 */
void DC2CALL Xh20_HurtFall(gptr o) { hurt_down(&K20, o, 0xE, 0); }    /* 0x480690 */
void DC2CALL Xh20_Stand(gptr o) { stand(&K20, o); }                   /* 0x480950 */
void DC2CALL Xh20_Walk(gptr o) { walk(&K20, o); }                     /* 0x480A70 */
void DC2CALL Xh20_Run(gptr o) { run(&K20, o); }                       /* 0x480BC0 */
void DC2CALL Xh20_TurnSpot(gptr o) { turn_spot(&K20, o); }            /* 0x480E30 */
void DC2CALL Xh20_TurnAbout(gptr o) { turn_about(&K20, o); }          /* 0x480F70 */
void DC2CALL Xh20_Dodge(gptr o) { dodge(&K20, o); }                   /* 0x481000 */
void DC2CALL Xh20_TurnFirst(gptr o) { turn_to_target(&K20, o, 0); }   /* 0x4810C0 */
void DC2CALL Xh20_TurnNext(gptr o) { turn_to_target(&K20, o, 1); }    /* 0x4811A0 */

void DC2CALL Xh20_Back(gptr o)                                        /* 0x480CF0 */
{
    switch (U8(o + 0x56)) {
    case 0: Anim_Start(o, 2, 0, 1, 6); U8(o + 0x56)++; U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFB0; break;
    case 1: Anim_Step(o); break;
    }
    back_pick(&K20, o);
}

void DC2CALL Xh20_Die(gptr o)                                         /* 0x480840 (both) */
{
    to_floor(o);
    switch (U8(o + 0x55)) {
    case 0:
        Anim_Start(o, 0x10, 0, 0x40, 8);
        U8(o + 0x55)++;
        U8(o + 0x50) &= 0xF7;
        zero_move(o);
        break;
    case 1:
        if (!Anim_Step(o)) break;
        Anim_Start(o, 0x11, 0, 0, 8);
        U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18u);
        U8(o + 0x55)++;
        GameOver_Start(0);
        break;
    case 2:
        Anim_Step(o);
        break;
    }
}

void DC2CALL Xh20_Punch(gptr o)                                       /* 0x481280 */
{
    u8 s = U8(o + 0x57);
    if (s > 3) return;
    if (!(s & 1)) {                                 /* 0, 2: the first or the second punch */
        if (s) { Anim_Start(o, 0x12, 6, 0x40, 6); U16(o + 0xF4) = 0; }
        else { Anim_Start(o, 7, 6, 0x40, 6); U16(o + 0xF4) = 0x32; }
        U32(o + 0xF0) = 0;
        U8(o + 0x57)++;
        G32(0x684)++;
        return;
    }
    turn_by_pad(&K20, o, 0x16);
    Anim_Step(o);
    u16 f = U16(o + 0x92);
    if (f >= 0x24) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
    else if (f >= 0x20) pick_attack(&K20, o, HELD(&K20));
    if (U8(o + 0x57) == 1) {
        if (f < 0x18 || f > 0x1C) return;
    } else if (f < 0x14 || f > 0x19) return;
    if (blow(o, 4, 0xD)) Effect_SpawnAt(o, 0x67A304, 0);
}

void DC2CALL Xh20_Kick(gptr o)                                        /* 0x481480 */
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 8, 5, 1, 2);
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x64;
        G32(0x684)++;
        break;
    case 1: {
        turn_by_pad(&K20, o, 0x16);
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= 0x1C) U32(o + 0x54) = 1;
        else if (f >= 0x18) pick_attack(&K20, o, HELD(&K20));
        if (f >= 0xD && f <= 0x11 && blow(o, 4, 0xD)) Effect_SpawnAt(o, 0x67A380, 0);
        break;
    }
    }
}

void DC2CALL Xh20_Spin(gptr o)                                        /* 0x4815F0 */
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0xA, 0x11, 1, 4);
        zero_move(o);
        U8(o + 0x57) = 2;
        G32(0x684)++;
        break;
    case 1:
        start_attack(o, 9, 0x11, 1, 4);
        break;
    case 2: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x28) pick_attack(&K20, o, HELD(&K20));
        u16 f = U16(o + 0x92);
        if (f < 0x1D || f > 0x27) break;
        gptr t = Tmp_Push(0x28), r = t, m = t + 8;
        U16(r) = 0x400; U16(r + 2) = U16(o + 0x4A); U16(r + 4) = 0;
        RotMatrixYXZ(r, m);
        Obj_UpdateMatrix(o);
        Weapon_Hit(m, Obj_Part(U32(o + 0x64), 0xE) + 0x40, 0xE);
        Weapon_Hit(m, Obj_Part(U32(o + 0x64), 0xF) + 0x40, 0xE);
        Weapon_Hit(m, Obj_Part(U32(o + 0x64), 0x10) + 0x40, 0xE);
        Tmp_Pop(0x28);
        break;
    }
    }
}

void DC2CALL Xh20_Low(gptr o)                                         /* 0x481790 */
{
    switch (U8(o + 0x57)) {
    case 0:
        start_attack(o, 0xB, 3, 0x40, 4);
        break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1E) pick_attack(&K20, o, HELD(&K20));
        u16 f = U16(o + 0x92);
        if (f >= 0xE && f <= 0x18 && blow(o, 0, 0xF)) Effect_SpawnAt(o, 0x67A58C, 0);
        break;
    }
    }
}

void DC2CALL Xh20_Super(gptr o)                                       /* 0x4818E0 */
{
    u16 f;
    switch (U8(o + 0x57)) {
    case 0:
        start_attack(o, 0xB, 0, 0x40, 4);
        break;
    case 1:
        U8(o + 0x57)++;
        /* fall through */
    case 2:
        Anim_Step(o);
        if (U16(o + 0x92) == 0x1B) { Anim_Start(o, 7, 4, 0x40, 6); U8(o + 0x57)++; zero_move(o); }
        break;
    case 3: case 4: case 5:
        Anim_Step(o);
        f = U16(o + 0x92);
        if (f >= 0x18 && f <= 0x1B) blow(o, 4, 0xD);
        if (U16(o + 0x92) == 0x1B) { Anim_Start(o, 7, 7, 0x40, 4); U8(o + 0x57)++; }
        break;
    case 6:
        Anim_Step(o);
        f = U16(o + 0x92);
        if (f >= 0x18 && f <= 0x1B) blow(o, 4, 0xD);
        if (U16(o + 0x92) == 0x20) { Anim_Start(o, 0x12, 7, 0x40, 6); U8(o + 0x57)++; U16(o + 0xF4) = 0; }
        break;
    case 7:
        Anim_Step(o);
        f = U16(o + 0x92);
        if (f == 0x1B) { Anim_Start(o, 8, 6, 1, 2); U8(o + 0x57)++; U16(o + 0xF4) = 0xA; }
        else if (f >= 0x14 && f <= 0x19) blow(o, 4, 0xD);
        break;
    case 8:
        if (Anim_Step(o)) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; break; }
        f = U16(o + 0x92);
        if (f >= 0xC && f <= 0x11) blow(o, 4, 0xF);
        break;
    }
    turn_by_pad(&K20, o, 0x16);
}

/* ---- type 21 ---------------------------------------------------------------- */
void DC2CALL Xh21_Handler(gptr o) { handler(&K21, o); }               /* 0x473310 */
void DC2CALL Xh21_Init(gptr o) { init(&K21, o); }                     /* 0x473440 */
void DC2CALL Xh21_Normal(gptr o) { normal(&K21, o); }                 /* 0x4735E0 */
void DC2CALL Xh21_Hit(gptr o) { Orig_Call1(U32(0x737738 + U8(o + 0x55) * 4), o); }   /* 0x473B30 */
void DC2CALL Xh21_Attack(gptr o) { Orig_Call1(U32(0x737758 + U8(o + 0x56) * 4), o); } /* 0x474090 */
void DC2CALL Xh21_HurtLight(gptr o) { hurt_light(&K21, o); }          /* 0x473660 */
void DC2CALL Xh21_HurtDown(gptr o) { hurt_down(&K21, o, 0xD, 0x703478); }   /* 0x4737D0 */
void DC2CALL Xh21_HurtFall(gptr o) { hurt_down(&K21, o, 0xE, 0x703488); }   /* 0x473980 */
void DC2CALL Xh21_Stand(gptr o) { stand(&K21, o); }                   /* 0x473B50 */
void DC2CALL Xh21_Walk(gptr o) { walk(&K21, o); }                     /* 0x473C60 */
void DC2CALL Xh21_Run(gptr o) { run(&K21, o); }                       /* 0x473DB0 */
void DC2CALL Xh21_TurnSpot(gptr o) { turn_spot(&K21, o); }            /* 0x4740B0 */
void DC2CALL Xh21_TurnAbout(gptr o) { turn_about(&K21, o); }          /* 0x4741E0 */
void DC2CALL Xh21_Dodge(gptr o) { dodge(&K21, o); }                   /* 0x474270 */
void DC2CALL Xh21_TurnFirst(gptr o) { turn_to_target(&K21, o, 0); }   /* 0x4742F0 */
void DC2CALL Xh21_TurnNext(gptr o) { turn_to_target(&K21, o, 1); }    /* 0x4743D0 */

/* walking back; with the kick button pressed late on, a kick back */
void DC2CALL Xh21_Back(gptr o)                                        /* 0x473EE0 */
{
    switch (U8(o + 0x56)) {
    case 0: Anim_Start(o, 2, 0, 1, 3); U8(o + 0x56)++; U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFB0; break;
    case 1: Anim_Step(o); break;
    case 2: Anim_Start(o, 6, 2, 0x40, 1); U8(o + 0x56)++; zero_move(o); return;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1C && (HELD(&K21) & 0x4000) && (NEW(&K21) & 0x40)) U8(o + 0x56) = 2;
        return;
    }
    back_pick(&K21, o);
}

void DC2CALL Xh21_Punch(gptr o)                                       /* 0x4744B0 */
{
    u8 s = U8(o + 0x57);
    if (s > 3) return;
    if (!(s & 1)) {
        if (s) Anim_Start(o, 0x12, 4, 0x40, 6); else Anim_Start(o, 7, 0xA, 0x40, 6);
        U8(o + 0x57)++;
        zero_move(o);
        G32(0x684)++;
        return;
    }
    if (Anim_Step(o)) { U16(o + 0xF4) = 0; U32(o + 0x54) = 1; }
    else if (U16(o + 0x92) >= 0x24) chain(o, HELD(&K21));
    if (U8(o + 0x57) == 1) {
        if (U16(o + 0x92) == 0x18) Effect_SpawnAt(o, 0x6796C4, 0);
        return;
    }
    turn_by_pad(&K21, o, 0x32);
    u16 f = U16(o + 0x92);
    if ((f >= 0xD && f <= 0x19) || (f >= 0x20 && f <= 0x26) || (f >= 0x2D && f <= 0x32) || (f >= 0x3A && f <= 0x40))
        blow(o, 4, 0xD);
}

void DC2CALL Xh21_Kick(gptr o)                                        /* 0x474650 */
{
    switch (U8(o + 0x57)) {
    case 0:
        start_attack(o, 8, 6, 0x40, 2);
        break;
    case 1: {
        turn_by_pad(&K21, o, 0x19);
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f == 0x19) {
            if (NEW(&K21) & 0x20) Anim_Start(o, 8, 0xA, 1, 4);       /* (once more) */
        } else if (f >= 0x1C) {
            U32(o + 0x54) = 1;
            u32 h = HELD(&K21);
            if (h & 0x10) U32(o + 0x54) = (h & 0x4000) << 3 | 0x401;
        }
        f = U16(o + 0x92);
        if (f >= 0xD && f <= 0x11) blow(o, 4, 0xD);
        break;
    }
    }
}

/* a blow that lands: a sound and a spark */
static void landed(gptr o) { Se_Play(o, 0x45); Effect_SpawnAt(o, 0x677410, 0); }

void DC2CALL Xh21_Spin(gptr o)                                        /* 0x4748C0 */
{
    switch (U8(o + 0x57)) {
    case 0:
        start_attack(o, 0xA, 2, 0x40, 4);
        break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        else if (U16(o + 0x92) >= 0x1E) chain(o, HELD(&K21));
        u16 f = U16(o + 0x92);
        if (f >= 0x10 && f <= 0x16 && blow(o, 0, 0x22)) landed(o);
        break;
    }
    }
}

void DC2CALL Xh21_Low(gptr o)                                         /* 0x474790 */
{
    switch (U8(o + 0x57)) {
    case 0:
        start_attack(o, 0xB, 2, 0x40, 4);
        break;
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= 0x22) U32(o + 0x54) = 1;
        else if (f >= 0x1E) chain(o, HELD(&K21));
        if (f >= 0xC && f <= 0x15 && blow(o, 0, 0xF)) landed(o);
        break;
    }
    }
}

/* the super move: the same punch again and again (eleven times), the
   spark at frame 0x18 */
void DC2CALL Xh21_Super(gptr o)                                       /* 0x474A00 */
{
    u8 s = U8(o + 0x57);
    if (s > 12) return;
    if (s == 0) { start_attack(o, 7, 0xA, 0x40, 6); return; }
    if (s < 12) {
        Anim_Step(o);
        turn_by_pad(&K21, o, 0x50);
        if (U16(o + 0x92) >= 0x1C) { Anim_Start(o, 7, 0x15, 0x40, 1); U8(o + 0x57)++; }
    } else if (Anim_Step(o)) U32(o + 0x54) = 1;
    if (U16(o + 0x92) == 0x18) Effect_SpawnAt(o, 0x6796C4, 0);
}
