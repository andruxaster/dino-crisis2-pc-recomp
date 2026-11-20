/* The gunmen of the extra game's duel (area 9 object types 22 and 24,
 * handlers 0x401000 / 0x4A9370; PS1 overlays WEP0F / WEP13): their handler,
 * init and normal state are copies (their numbers in Xg), their moves
 * their own. Type 22 below; type 24 (health 2200) further down.
 * Type 22, built like the duel's human
 * (kof_human.c): health 1500, the other player (or its target) as foe.
 * States (o+0x54, 0x71B0B0): 0 init, 1 normal (by o+0x55: the move's
 * control 0x71B0C0, then the move 0x71B100), 2 hit (0x71B140), 3 die
 * (0x71B170).
 * Moves: 0 stand, 1 turn on the spot, 2 turn to the foe, 3 walk, 4 run,
 * 5 walk back, 6 turn about (o+0x57 which way), 7 guard (aim), 8 side step
 * (o+0x57 the side), 9 the super move's pose, 10 the super move (armour,
 * glowing for 900 frames, 500 of damage), 11 roll, 12..15 the attacks
 * (their damage 0x71B1E0, reactions 0x71B200, sparks 0x71B1A0).
 * Variables: 0x746C48 the pad's new presses, 0x746C98 held (read as a
 * dword with 0x746C9A, the angle from its head to the foe), 0x746C94 the
 * foe, 0x746C50 bits (1 may look at the foe, 2 untouchable, 3 the super
 * move), 0x746C58 the kind of hit taken, 0x746C5A..5D the state of the
 * last frame, 0x746C76 the turn about's turn so far, 0x746C80 the super
 * move's armour, 0x746C82 its glow, 0x746C84 its time, 0x746C8C the point
 * turned to. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"


#define V_NEW   0x746C48u
#define V_BITS  0x746C50u
#define V_HITK  0x746C58u
#define V_SAVED 0x746C5Au
#define V_TURN  0x746C76u
#define V_ARMOR 0x746C80u
#define V_GLOW  0x746C82u
#define V_TIME  0x746C84u
#define V_POINT 0x746C8Cu
#define V_FOE   0x746C94u
#define V_HELD  0x746C98u
#define V_ANGLE 0x746C9Au
#define PX()    U32(0x87FD60)

#define HELD()  U32(V_HELD)                     /* (with the angle above: the bits tested are the low 16) */
#define SIDE()  ((u32)(S16(V_ANGLE) <= 0) << 24)  /* o+0x57: the foe to the left */

static void call(gptr tab, gptr o, u32 i) { Orig_Call1(U32(tab + i * 4), o); }
static void zero_move(gptr o) { U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0; }
static void turn_by_pad(gptr o)
{
    if (HELD() & 0x2000) U16(o + 0x4A) += 0x46;
    if (HELD() & 0x8000) U16(o + 0x4A) -= 0x46;
}
/* at frame f or later while the super move lasts */
static int late_super(gptr o, u16 f) { return U16(o + 0x92) >= f && Bits_Check(V_BITS, 3); }

void DC2CALL Xh22_Glow(gptr o)                                        /* 0x403320 */
{
    u16 t = U16(V_GLOW);
    if (Bits_Check(V_BITS, 3)) { if ((s16)t < 0x1E) U16(V_GLOW) = ++t; }
    else if (t) U16(V_GLOW) = --t;
    if (t && U32(o + 0x64))
        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {       /* towards {0x90, 0x60, 0x68} in 30 steps */
            s32 k = S16(V_GLOW);
            U8(p + 0x8C) = (u8)(k * 0x10 / 30 + 0x80);
            U8(p + 0x8D) = (u8)(k * -0x20 / 30 + 0x80);
            U8(p + 0x8E) = (u8)(k * -0x18 / 30 + 0x80);
        }
    if (!U16(V_TIME) || !--U16(V_TIME)) Bits_Set(V_BITS, 3, 0);
}

/* what differs between the copies of the handler, init and normal state */
typedef struct {
    u32 v_new, v_held, v_angle, v_foe, v_bits, v_saved, v_dmg;   /* v_dmg: damage counted down by 3 (0: none) */
    u32 t_main, t_ctl, t_move, t_spark, combo, dmg, react;
    u32 cyl, wall, spheres;
    u16 health;
    u8 anim0, glow;
    u32 s118b, s118c;
    u8 p15f, p160;
    u16 w164, w166;
    u8 super_mv;                    /* the super move's move (o+0x55) */
    u8 super_once;                  /* not again while in it (by the move, else by bit 3) */
    u32 alt;                        /* run instead while o+0x10C is 1 (0: none) */
    u32 foe_copy;                   /* the foe's position kept there too (0: no) */
    u32 timer;                      /* a u16 counted down each frame (0: none) */
    u8 no_push;                     /* it never pushes the others */
    u8 angle_late;                  /* the angle to the foe after the move */
    u8 s118a, no_fe;                /* Obj_Set118's first; o+0xFE left alone */
} Xg;

static const Xg K22 = {0x746C48, 0x746C98, 0x746C9A, 0x746C94, 0x746C50, 0x746C5A, 0,
                       0x71B0B0, 0x71B0C0, 0x71B100, 0x71B1A0, 0x71B0A0, 0x71B1E0, 0x71B200,
                       0x71B030, 0x71B060, 0x71B070, 0x5DC, 0x16, 1, 0x2770, 0x4E80, 3, 2, 0x40, 0x40, 0xA, 0, 0, 0, 0, 0, 0, 0, 0};
static const Xg K25 = {0x746CA8, 0x746D08, 0x746D0A, 0x746CFC, 0x746CB0, 0x746CBA, 0,
                       0x71D614, 0x71D624, 0x71D648, 0x71D6B4, 0x71D604, 0x71D6D8, 0x746D0C,
                       0x71D5D8, 0x71D5E8, 0x71D5F8, 1, 0x16, 0, 0x1388, 0x1388, 2, 1, 0x15, 0x20, 0, 0,
                       0x408370, 0x746CD4, 0x746CE6, 1, 0, 0, 0};
static const Xg K26 = {0x74B1A8, 0x74B1FC, 0x74B1FE, 0x74B1F8, 0x74B1B0, 0x74B1BA, 0,
                       0x722528, 0x722538, 0x722560, 0x7225D8, 0x722518, 0x722600, 0x722614,
                       0x7224C8, 0x7224F0, 0x722500, 0x4B0, 0x16, 0, 0x1FA0, 0x3AF8, 3, 2, 0x60, 0x80, 0, 0,
                       0, 0, 0, 0, 1, 1, 1};
static const Xg K24 = {0x75BD58, 0x75BDA4, 0x75BDA6, 0x75BDA0, 0x75BD60, 0x75BD6A, 0x75BD70,
                       0x73B344, 0x73B354, 0x73B37C, 0x73B3FC, 0x73B334, 0x73B424, 0x73B438,
                       0x73B2D8, 0x73B300, 0x73B310, 0x898, 4, 0, 0x4E20, 0x7530, 0xC, 8, 0x15, 0x20, 9, 1, 0, 0, 0, 0, 0, 0, 0};

/* the attack spheres of the move, then its sparks */
static void attack(const Xg *k, gptr o)
{
    u8 mv = U8(o + 0x55);
    Kof_Attack(o, 0, U16(k->dmg + mv * 2), U16(k->react + mv * 2), 0x1F);
    if ((s8)U8(o + 0x119) >= 0) call(k->t_spark, o, U8(o + 0x55));
}

static void head_angle(const Xg *k, gptr o)
{
    gptr head = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(k->v_angle) = head ? (u16)Angle_DiffTo(head + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
}

static void handler(const Xg *k, gptr o)
{
    if (k->alt && U16(o + 0x10C) == 1) { Orig_Call1(k->alt, o); return; }
    U16(k->v_held) = U16(0x87DC08);
    U32(0x87FD60) = o + 0x1C0;
    U16(k->v_new) = U16(0x87DC94);
    if (U16(k->v_new) & 8) Target_Find(o, 0x800);
    if ((U8(k->v_held) & 8) && (U16(k->v_new) & 4)) Target_Next(U32(PX() + 0x14));
    gptr t = U32(PX() + 0x14);
    if (!t || !(U8(t + 0x50) & 1)) Target_Next(t);
    t = U32(PX() + 0x14);
    U32(k->v_foe) = t ? t : o;
    U8(o + 0x15D) = U8(U32(k->v_foe) + 0xFF);
    gptr sph = U32(U32(k->v_foe) + 0x134);
    U8(o + 0x15E) = sph ? U8(sph + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    for (u32 i = 0; i < 6; i += 2) {
        U16(o + 0xD8 + i) = U16(U32(k->v_foe) + 0x40 + i);
        if (k->foe_copy) U16(k->foe_copy + i) = U16(o + 0xD8 + i);
    }
    if (!k->angle_late) head_angle(k, o);
    Obj_SavePos(o);
    call(k->t_main, o, U8(o + 0x54));
    if (k->glow) Xh22_Glow(o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    if (k->angle_late) head_angle(k, o);
    s32 a = S16(k->v_angle);
    U32(o + 0x158) = ((a < 0 ? -a : a) < 0x500 && U32(k->v_foe) != o && Bits_Check(k->v_bits, 1)) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (k->no_push) U32(o + 0x50) |= 0x20;
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (!(U8(o + 0x50) & 0x80)) Zone_Update(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
    U16(PX() + 0x36)++;
    attack(k, o);
    if (k->v_dmg && U32(k->v_dmg)) {
        s32 d = (s32)U32(k->v_dmg) - 3;
        U32(k->v_dmg) = (u32)d;
        if (d < 0) U32(k->v_dmg) = 0;
    }
    if (k->timer && U16(k->timer)) U16(k->timer)--;
    for (u32 i = 0; i < 4; i++) U8(k->v_saved + i) = U8(o + 0x54 + i);
}

static void init(const Xg *k, gptr o)
{
    U32(o + 0x50) = 3;
    U16(o + 0x40) = U16(o + 0xE0) = U16(G() + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    DrawSync(o);                                    /* (the object left on the stack) */
    u32 w = U16(G() + 0x10B4);
    Stage_Load(w & 0xFF, w >> 8);
    U16(PX() + 0x3A) = 0x80;
    Target_Find(o, 0x800);
    gptr t = U32(PX() + 0x14);
    U32(k->v_foe) = t ? t : o;
    U8(o + 0x54)++;
    U16(o + 0x108) = k->health; U16(o + 0x10A) = k->health;
    U32(o + 0x12C) = k->cyl; U32(o + 0x128) = k->wall; U32(o + 0x134) = k->spheres;
    zero_move(o);
    U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, k->anim0, 0, 1, 0);
    Obj_Set118(o, k->s118a, k->s118b, k->s118c);
    Head_Reset(o);
    U8(o + 0x15F) = k->p15f; U8(o + 0x160) = k->p160;
    U16(o + 0x164) = k->w164; U16(o + 0x166) = k->w166;
    U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(U32(k->v_foe) + 0xFF);
    gptr sph = U32(U32(k->v_foe) + 0x134);
    U8(o + 0x15E) = sph ? U8(sph + 0xA) : 0;
    Bits_Set(k->v_bits, 1, 0);
    if (!k->no_fe) U8(o + 0xFE) = 3;
    U8(o + 0x118) |= 0x20;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the button sequence at half health or less starts the super move */
static void normal(const Xg *k, gptr o)
{
    U16(o + 0x174) = 0;
    if (Combo_Input(k->combo, U16(k->v_new)) && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && (k->super_once ? U8(o + 0x55) != k->super_mv : !Bits_Check(k->v_bits, 3))) {
        U32(o + 0x54) = (u32)k->super_mv << 8 | 1;
        Kof_Super(o);
    }
    call(k->t_ctl, o, U8(o + 0x55));
    call(k->t_move, o, U8(o + 0x55));
}

void DC2CALL Xh22_Attack(gptr o) { attack(&K22, o); }                 /* 0x401560 */
void DC2CALL Xh22_Handler(gptr o) { handler(&K22, o); }               /* 0x401000 */
void DC2CALL Xh22_Init(gptr o) { init(&K22, o); }                     /* 0x4012A0 */
void DC2CALL Xh22_Normal(gptr o) { normal(&K22, o); }                 /* 0x401490 */
void DC2CALL Xh24_Attack(gptr o) { attack(&K24, o); }                 /* 0x4A98E0 */
void DC2CALL Xh24_Handler(gptr o) { handler(&K24, o); }               /* 0x4A9370 */
void DC2CALL Xh24_Init(gptr o) { init(&K24, o); }                     /* 0x4A9630 */
void DC2CALL Xh24_Normal(gptr o) { normal(&K24, o); }                 /* 0x4A9820 */
void DC2CALL Xh22_Hit(gptr o) { call(0x71B140, o, U8(o + 0x55)); }   /* 0x401520 */
void DC2CALL Xh22_Die(gptr o) { call(0x71B170, o, U8(o + 0x55)); }   /* 0x401540 */

/* ---- the controls: the next move by the buttons ---- */
/* the attack buttons (1 if they chose): 0x20 shoot (forward 0x40: the
   lunge, 8 aimed), 0x10 the kick (by side); 2 the turn about */
static int ctl_attack(gptr o, u32 h, u8 nw, u32 side, int lunge)
{
    if (lunge && (h & 0x1000) && (h & 0x40) && (h & 0x20)) { U32(o + 0x54) = 0xD01; return 1; }
    if (h & 0x20) { U32(o + 0x54) = h & 8 ? 0xC01 : 0xE01; return 1; }
    if (h & 0x10) { U32(o + 0x54) = side | 0xF01; return 1; }
    if (nw & 2) { U32(o + 0x54) = side | 0x601; return 1; }
    return 0;
}

void DC2CALL Xh22_StandCtl(gptr o)                                    /* 0x4015B0 */
{
    u32 side = SIDE(), h = HELD();
    u8 nw = U8(V_NEW);
    if (ctl_attack(o, h, nw, side, 1)) return;
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301; return; }
    if (h & 8) { U32(o + 0x54) = 0x701; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x501; return; }
    if (h & 0xA000) { U32(o + 0x54) = 0x101; return; }
    if (nw & 1) U32(o + 0x54) = 0xB01;
}

void DC2CALL Xh22_TurnCtl(gptr o)                                     /* 0x401750 */
{
    u32 side = SIDE(), h = HELD();
    u8 nw = U8(V_NEW);
    if (ctl_attack(o, h, nw, side, 1)) {}
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301;
    else if (h & 8) U32(o + 0x54) = 0x701;
    else if (h & 0x4000) U32(o + 0x54) = 0x501;
    else if (!(h & 0xA000)) U32(o + 0x54) = 1;
    else if (nw & 1) U32(o + 0x54) = 0xB01;
    turn_by_pad(o);
}

void DC2CALL Xh22_WalkCtl(gptr o)                                     /* 0x4019D0 */
{
    u32 side = SIDE(), h = HELD();
    u8 nw = U8(V_NEW);
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x1000)) U32(o + 0x54) = 1;
    else if (ctl_attack(o, h, nw, side, 1)) {}
    else if (h & 0x40) U32(o + 0x54) = 0x401;
    else if (nw & 1) U32(o + 0x54) = 0xB01;
    turn_by_pad(o);
}

void DC2CALL Xh22_RunCtl(gptr o)                                      /* 0x401B70 */
{
    u32 side = SIDE(), h = HELD();
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x1000)) U32(o + 0x54) = 1;
    else if (!(h & 0x40)) U32(o + 0x54) = 0x301;
    else if (h & 0x20) U32(o + 0x54) = 0xD01;
    else if (h & 0x10) U32(o + 0x54) = side | 0xF01;
    else {
        u8 nw = U8(V_NEW);
        if (nw & 2) U32(o + 0x54) = side | 0x601;
        else if (nw & 1) U32(o + 0x54) = 0xB01;
    }
    turn_by_pad(o);
}

void DC2CALL Xh22_BackCtl(gptr o)                                     /* 0x401CF0 */
{
    u32 side = SIDE(), h = HELD();
    u8 nw = U8(V_NEW);
    if (!(h & 0x4000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x4000)) U32(o + 0x54) = 1;
    else if (ctl_attack(o, h, nw, side, 0)) {}
    else if (nw & 1) U32(o + 0x54) = 0xB01;
    turn_by_pad(o);
}

void DC2CALL Xh22_GuardCtl(gptr o)                                    /* 0x401FC0 */
{
    u32 side = SIDE(), h = HELD();
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301; return; }
    if (!(h & 8)) { U32(o + 0x54) = 1; return; }
    u8 nw = U8(V_NEW);
    if (nw & 4) { U32(o + 0x54) = 0x201; return; }
    if (h & 0x20) { U32(o + 0x54) = 0xC01; return; }
    if (h & 0x10) { U32(o + 0x54) = side | 0xF01; return; }
    if (nw & 2) { U32(o + 0x54) = side | 0x601; return; }
    if (h & 0x2000) { U32(o + 0x54) = 0x801; return; }
    if (h & 0x8000) { U32(o + 0x54) = 0x1000801; return; }
    if (h & 0x4000) {                               /* a side step away from the nearer wall */
        u32 l = Wall_Dist(o, 0x400, 0x40), r = Wall_Dist(o, 0xFFFFFC00u, 0x40);
        U32(o + 0x54) = (u32)(l <= r) << 24 | 0x801;
        return;
    }
    if (nw & 1) U32(o + 0x54) = 0xB01;
}

void DC2CALL Xh22_StepCtl(gptr o)                                     /* 0x402250 */
{
    u32 side = SIDE(), h = HELD();
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301; return; }
    if (!(h & 8)) { U32(o + 0x54) = 1; return; }
    if (h & 0x20) { U32(o + 0x54) = 0xC01; return; }
    if (h & 0x10) { U32(o + 0x54) = side | 0xF01; return; }
    u8 nw = U8(V_NEW);
    if (nw & 2) U32(o + 0x54) = side | 0x601;
    else if (nw & 1) U32(o + 0x54) = 0xB01;
}

void DC2CALL Xh22_ShotCtl(gptr o)                                     /* 0x4025D0 */
{
    if (U16(o + 0x92) < 0xA) Obj_TurnToPoint(o, 0x23, o + 0xD8);
    turn_by_pad(o);
}
void DC2CALL Xh22_TurnPad(gptr o) { turn_by_pad(o); }                 /* 0x402750, 0x402870 */

/* ---- the moves ---- */
static void start(gptr o, u32 anim, u32 frame, u32 fl, u32 blend, u32 look)
{
    U8(o + 0x56)++;
    zero_move(o);
    Anim_Start(o, anim, frame, fl, blend);
    Bits_Set(V_BITS, 1, look);
}

void DC2CALL Xh22_Stand(gptr o)                                       /* 0x4016E0 */
{
    switch (U8(o + 0x56)) {
    case 0: start(o, 0x16, 0, 0x41, 0x14, 1); break;
    case 1: Anim_Step(o); break;
    }
}

void DC2CALL Xh22_Turn(gptr o)                                        /* 0x401870 */
{
    switch (U8(o + 0x56)) {
    case 0: start(o, 0, 0, 1, 0xA, 1); break;
    case 1: Anim_Step(o); break;
    }
}

static void keep_point(gptr o)
{
    U16(V_POINT) = U16(o + 0xD8); U16(V_POINT + 2) = U16(o + 0xDA); U16(V_POINT + 4) = U16(o + 0xDC);
}

void DC2CALL Xh22_TurnToFoe(gptr o)                                   /* 0x4018E0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(V_FOE) == o) { U32(o + 0x54) = 0x701; return; }
        start(o, 0, 0, 1, 0xA, 1);
        keep_point(o);
        break;
    case 1: {
        Anim_Step(o);
        u16 a = (u16)Angle_XZ(o + 0x40, V_POINT);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        u8 n = ++U8(o + 0x57);
        if (n >= 0x14 || U16(o + 0x4A) == a) U32(o + 0x54) = 0x701;
        break;
    }
    }
}

/* walk, run, walk back: the speed towards its own (faster in the super move) */
static void go(gptr o, u32 anim, s32 step, s32 speed, s32 step2, s32 speed2)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, anim, 0, 1, 0xA);
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(V_BITS, 1, 1);
        break;
    case 1:
        Anim_Step(o);
        if (Bits_Check(V_BITS, 3)) Value_Approach(o + 0xF4, (u32)step2, (u32)speed2);
        else Value_Approach(o + 0xF4, (u32)step, (u32)speed);
        break;
    }
}
void DC2CALL Xh22_Walk(gptr o) { go(o, 0, 0x1E, 0x82, 0x28, 0xA0); }        /* 0x401AD0 */
void DC2CALL Xh22_Run(gptr o) { go(o, 9, 0x28, 0xDC, 0x32, 0xFA); }         /* 0x401C50 */
void DC2CALL Xh22_Back(gptr o) { go(o, 1, 0x1E, -0x64, 0x28, -0x82); }      /* 0x401DC0 */

/* turn about (o+0x57: half a turn either way, a quarter either way) */
void DC2CALL Xh22_TurnAbout(gptr o)                                   /* 0x401E60 */
{
    static const s16 turn[4] = {0x800, -0x800, 0x400, -0x400}, len[4] = {0x30, 0x30, 0x1E, 0x1E};
    static const u32 anim[4] = {2, 5, 6, 7};
    u8 w = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, anim[w], 2, 0x40, 5);
        Bits_Set(V_BITS, 1, 0);
        U16(V_TURN) = 0;
        break;
    case 1:
        U16(V_TURN) += (u16)(turn[w] / len[w]);
        Anim_Step(o);
        if (U16(o + 0x92) < 0x23) break;
        U16(V_TURN) = 0;
        U16(U32(o + 0x64) + 0x4A) = (u16)((U16(U32(o + 0x64) + 0x4A) - turn[U8(o + 0x57)]) & 0xFFF);
        U16(o + 0x4A) = (u16)((turn[U8(o + 0x57)] + U16(o + 0x4A)) & 0xFFF);
        Anim_Set(o, 0, 0, 1, 0xA);
        U32(o + 0x54) = 1;
        break;
    }
}

void DC2CALL Xh22_Guard(gptr o)                                       /* 0x402100 */
{
    switch (U8(o + 0x56)) {
    case 0:
        start(o, 0x13, 0, 0x40, 0xA, 1);
        keep_point(o);
        break;
    case 1: {
        Anim_Step(o);
        u16 a;
        if (U32(V_FOE) != o) {
            a = (u16)Angle_XZ(o + 0x40, V_POINT);
            U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        } else a = U16(o + 0x4A);
        if (U16(o + 0x4A) == a && U16(o + 0x92) > 0xF) { U8(o + 0x56)++; Anim_Start(o, 0xA, 0, 0x41, 5); }
        break;
    }
    case 2:
        Anim_Step(o);
        break;
    case 3:
        U8(o + 0x56)--;
        zero_move(o);
        Anim_Start(o, 0xA, 0, 0x41, 0xA);
        Bits_Set(V_BITS, 1, 1);
        break;
    }
}

void DC2CALL Xh22_SideStep(gptr o)                                    /* 0x402300 */
{
    static const u32 anim[2] = {0xC, 0xD};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, anim[U8(o + 0x57)], 1, 0x40, 5);
        Bits_Set(V_BITS, 1, 1);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x14 || late_super(o, 0xF)) { U32(o + 0x54) = 0x30701; zero_move(o); }
        Obj_TurnToPoint(o, Bits_Check(V_BITS, 3) ? 0x78 : 0x5D, o + 0xD8);
        U16(o + 0xF0) = U16(o + 0xD0);
        break;
    }
}

void DC2CALL Xh22_Pose(gptr o)                                        /* 0x402400 */
{
    switch (U8(o + 0x56)) {
    case 0:
        start(o, 0x12, 0, 0x40, 2, 1);
        Effect_Attach(o, 0x6769B0);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2C && Bits_Check(V_BITS, 3)) U32(o + 0x54) = 1;
        break;
    }
}

void DC2CALL Xh22_Super(gptr o)                                       /* 0x402490 */
{
    switch (U8(o + 0x56)) {
    case 0:
        start(o, 8, 0, 0x40, 5, 0);
        Bits_Set(V_BITS, 3, 1);
        U16(V_TIME) = 0x384; U16(V_ARMOR) = 0x1F4;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* untouchable over frames lo..hi */
static void dodge_frames(gptr o, u16 lo, u16 hi)
{
    u16 f = U16(o + 0x92);
    if (f >= lo && f <= hi) { Bits_Set(V_BITS, 2, 1); U8(o + 0x17B) = 0; }
}

void DC2CALL Xh22_Roll(gptr o)                                        /* 0x402530 */
{
    switch (U8(o + 0x56)) {
    case 0: start(o, 3, 0, 0x40, 0xA, 0); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x30) U32(o + 0x54) = 1;
        dodge_frames(o, 0xE, 0x2B);
        break;
    }
}

static void start_attack(gptr o, u32 anim, u32 frame)
{
    start(o, anim, frame, 0x40, 5, 0);
    U32(0x87E4A4)++;
}

void DC2CALL Xh22_Lunge(gptr o)                                       /* 0x402620 */
{
    switch (U8(o + 0x56)) {
    case 0: start_attack(o, 0xE, 2); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x23 || late_super(o, 0x18)) { U32(o + 0x54) = 0x30701; zero_move(o); return; }
        dodge_frames(o, 5, 0x18);
        if (U16(o + 0x92) <= 0xD) return;
        if (Bits_Check(V_BITS, 3)) U16(o + 0xF4) = (u16)(S16(o + 0xD4) * 4 / 3);
        else U16(o + 0xF4) = U16(o + 0xD4);
        break;
    }
}

void DC2CALL Xh22_Dash(gptr o)                                        /* 0x402780 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0xF, 8, 0x40, 5);
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(V_BITS, 1, 0);
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) { U32(o + 0x54) = 1; return; }
        dodge_frames(o, 3, 0x18);
        if (Bits_Check(V_BITS, 3)) Value_Approach(o + 0xF4, 0x32, 0xFA);
        else Value_Approach(o + 0xF4, 0x28, 0xDC);
        break;
    }
}

void DC2CALL Xh22_Shot(gptr o)                                        /* 0x4028A0 */
{
    switch (U8(o + 0x56)) {
    case 0: start_attack(o, 0xB, 1); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x1C || late_super(o, 0x18)) U32(o + 0x54) = 1;
        break;
    }
}

void DC2CALL Xh22_Kick(gptr o)                                        /* 0x402940 */
{
    static const u32 anim[2] = {0x10, 0x11};
    switch (U8(o + 0x56)) {
    case 0: start_attack(o, anim[U8(o + 0x57)], 2); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x23 || late_super(o, 0x1E)) U32(o + 0x54) = 0x30701;
        break;
    }
}

/* the sparks of the attacks (their first frame) */
void DC2CALL Xh22_SparkShot(gptr o)                                   /* 0x402A00 */
{
    if (U8(o + 0x56) == 1) { Effect_Attach(o, 0x675450); Flash_Play(o, 0x677F5C); }
}
void DC2CALL Xh22_SparkShot2(gptr o)                                  /* 0x402A30 */
{
    if (U8(o + 0x56) == 1) Flash_Play(o, 0x677F4C);
}
void DC2CALL Xh22_SparkKick(gptr o)                                   /* 0x402A50 */
{
    if (U8(o + 0x56) == 1) { Se_Play(o, 0x44); Flash_Play(o, 0x677F7C); }
}

/* ---- hit and dying ---- */
/* hit (moves 0..7): the kind of hit by the move, the damage (half again
   off guard, a quarter in the super move against its armour), the
   reaction by side, then the state it leads to run at once */
void DC2CALL Xh22_HitReact(gptr o)                                    /* 0x402A80 */
{
    static const u8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(V_HITK) = kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) += (u16)(U16(o + 0x176) >> 1);
    if ((s8)U8(o + 0x17C) <= 1) U16(o + 0x176) += (u16)(U16(o + 0x176) >> 1);
    if (Bits_Check(V_BITS, 3)) U16(o + 0x176) >>= 2;
    U32(o + 0x54) = 0x802;
    if ((s8)U8(o + 0x17C) <= 1) {
        u16 d = (u16)Angle_Diff((U16(o + 0x178) - 0x800) & 0xFFF, U16(o + 0x4A));
        U32(o + 0x54) = ((s16)d <= 0 ? 0 : 0x1000000u) + 0xA02;
    }
    if (Bits_Check(V_BITS, 2)) U32(o + 0x54) = 0xB02;
    if (U16(V_HITK) == 3) U32(o + 0x54) = 0x902;
    if (Bits_Check(V_BITS, 3)) {
        u16 c = (u16)(U16(V_ARMOR) - Damage_Scale(U16(o + 0x176)));
        U16(V_ARMOR) = c;
        if ((s16)c <= 0) Bits_Set(V_BITS, 3, 0);
        else U32(o + 0x54) = 0x802;
    }
    Player_Release(o);
    U32(o + 0x50) |= 8;
    Bits_Set(V_BITS, 1, 0);
    Bits_Set(V_BITS, 2, 0);
    zero_move(o);
    if (U16(V_TURN)) { U16(o + 0x4A) = (u16)((U16(o + 0x4A) + U16(V_TURN)) & 0xFFF); U16(V_TURN) = 0; }
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) <= 0) {
        U32(o + 0x50) &= ~8u;
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = ((u32)U8(o + 0x57) << 16 | U8(o + 0x55)) << 8 | 3;
    }
    call(0x71B0B0, o, U8(o + 0x54));
}

/* a flinch (until frame end) */
static void flinch(gptr o, u16 end)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0x14, 1, 0x40, 2);
        Bits_Set(V_BITS, 1, 0);
        Effect_Spawn(o, 0x675818, 4, 0xFF, 0x100, 0);
        Se_Play(o, 0x3F);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) U32(o + 0x54) = 1;
        break;
    }
}
void DC2CALL Xh22_Flinch(gptr o) { flinch(o, 0x12); }                /* 0x402D10 */
void DC2CALL Xh22_Flinch2(gptr o) { flinch(o, 0x20); }               /* 0x402D90 */

void DC2CALL Xh22_Thrown(gptr o)                                      /* 0x402E10 */
{
    static const u32 anim[2] = {0x15, 0x17}, fx1[2] = {0x675A9C, 0x675D70}, fx2[2] = {0x677414, 0x677648};
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        Anim_Start(o, anim[U8(o + 0x57)], 1, 0x40, 2);
        Se_Play(o, 0x3E);
        u32 k = U16(V_HITK);
        Effect_Spawn(o, (k >= 2 && k <= 3 ? fx1 : fx2)[U8(o + 0x57)], 4, 0xFF, 0x100, 0);
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

void DC2CALL Xh22_Blocked(gptr o)                                     /* 0x402F30 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 4, 1, 0x40, 2);
        Effect_Spawn(o, 0x676D24, 6, 0xFF, 0x100, 0);
        Se_Play(o, 0x40);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x1D) U32(o + 0x54) = 1;
        break;
    }
}

/* lying dead: put down among the layers, the ground shaken, sinking, a
   prop where it lies; then the duel lost (or game over) */
static void dead(gptr o, u16 shake, u16 lo, u16 hi, u16 prop)
{
    if (Anim_Step(o)) { U8(o + 0x56)++; U8(o + 0x50) |= 0x30; Layers_Assign(); }
    if (U16(o + 0x92) == shake) Shake_Set(4, 0, 0x28);
    u16 f = U16(o + 0x92);
    if (f >= lo && f <= hi) U16(o + 0x42) += 6;
    if (f == prop) { Prop_SpawnC(1, o + 0x40, 2, 0x3C, 1, 0); U8(o + 0x50) |= 0x30; }
}
static void lost(void)
{
    if (U8(G() + 0x927) == 2) Flag_Set(7, 0x1E, 1);
    else GameOver_Start(0);
}
static void die_start(gptr o, u32 anim)
{
    U8(o + 0x56)++;
    Anim_Start(o, anim, 0, 0x40, 2);
    U32(o + 0x50) &= ~8u;
    zero_move(o);
    Bits_Set(V_BITS, 1, 0);
    U8(o + 0x118) &= 0xDF;
}

void DC2CALL Xh22_Fall(gptr o)                                        /* 0x402FA0 */
{
    switch (U8(o + 0x56)) {
    case 0: die_start(o, 0x14); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0xF) { U8(o + 0x56)++; Anim_Start(o, 0x18, 0, 0x40, 2); }
        break;
    case 2: dead(o, 0x30, 0x26, 0x4E, 0x56); break;
    case 3: lost(); break;
    }
}

void DC2CALL Xh22_FallThrown(gptr o)                                  /* 0x403100 */
{
    static const u32 anim[2] = {0x15, 0x17}, fx1[2] = {0x675A9C, 0x675D70}, fx2[2] = {0x677414, 0x677648};
    static const u32 anim2[2] = {0x19, 0x1A}, fx3[2] = {0x676430, 0x676704};
    switch (U8(o + 0x56)) {
    case 0: {
        die_start(o, anim[U8(o + 0x57)]);
        u32 k = U16(o + 0x172);
        Effect_Attach(o, (k >= 2 && k <= 3 ? fx1 : fx2)[U8(o + 0x57)]);
        break;
    }
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) < 0xA) break;
        U8(o + 0x56)++;
        Anim_Start(o, anim2[U8(o + 0x57)], 0, 0x40, 2);
        Effect_Attach(o, fx3[U8(o + 0x57)]);
        break;
    case 2: dead(o, 0x1B, 0x11, 0x39, 0x34); break;
    case 3: lost(); break;
    }
}

/* ---- type 24 (handler 0x4A9370, PS1 WEP13): health 2200, heavy and slow,
   a big gun. Moves (o+0x55, controls 0x73B354, moves 0x73B37C): 0 stand,
   1 turn on the spot, 2 turn to the foe, 3 walk, 4 run, 5 walk back, 6 the
   shot, 7 the burst, 8 the side shot (o+0x57 the side), 9 the super move
   (a charge). Hit 0x73B3A4: 0..7 the reaction (damage counted: over 160 it
   is knocked down), 8/9 flinches, 10 knocked down. Die 0x73B3D0.
   Variables: 0x75BD58 new presses, 0x75BDA4 held (with 0x75BDA6, the angle
   to the foe), 0x75BDA0 the foe, 0x75BD60 bits, 0x75BD68 the kind of hit,
   0x75BD70 the damage taken lately, 0x75BD98 the point turned to. */
#define W_NEW   0x75BD58u
#define W_BITS  0x75BD60u
#define W_HITK  0x75BD68u
#define W_DMG   0x75BD70u
#define W_POINT 0x75BD98u
#define W_FOE   0x75BDA0u
#define W_HELD  0x75BDA4u
#define W_ANGLE 0x75BDA6u
#define W_HELDV() U32(W_HELD)
#define W_SIDE()  ((u32)(S16(W_ANGLE) <= 0) << 24)

static void w_turn(gptr o)
{
    if (W_HELDV() & 0x2000) U16(o + 0x4A) += 0x32;
    if (W_HELDV() & 0x8000) U16(o + 0x4A) -= 0x32;
}

void DC2CALL Xh24_Hit(gptr o) { call(0x73B3A4, o, U8(o + 0x55)); }   /* 0x4A98A0 */
void DC2CALL Xh24_Die(gptr o) { call(0x73B3D0, o, U8(o + 0x55)); }   /* 0x4A98C0 */

void DC2CALL Xh24_StandCtl(gptr o)                                    /* 0x4A9930 */
{
    u32 side = W_SIDE();
    u8 nw = U8(W_NEW);
    if (nw & 8) { U32(o + 0x54) = 0x201; return; }
    u32 h = W_HELDV();
    if ((h & 8) && (nw & 4)) { U32(o + 0x54) = 0x201; return; }
    if (h & 0x20) { U32(o + 0x54) = 0x701; return; }
    if (h & 0x10) { U32(o + 0x54) = side | 0x801; return; }
    if (h & 0x1000) { U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x501; return; }
    if (h & 0xA000) { U32(o + 0x54) = 0x101; return; }
    if (nw & 1) U32(o + 0x54) = 0x601;
}

void DC2CALL Xh24_TurnCtl(gptr o)                                     /* 0x4A9A70 */
{
    u32 side = W_SIDE();
    u8 nw = U8(W_NEW);
    u32 h = W_HELDV();
    if (nw & 8) U32(o + 0x54) = 0x201;
    else if (h & 0x20) U32(o + 0x54) = 0x701;
    else if (h & 0x10) U32(o + 0x54) = side | 0x801;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x501;
    else if (!(h & 0xA000)) U32(o + 0x54) = 1;
    else if (nw & 1) U32(o + 0x54) = 0x601;
    w_turn(o);
}

/* walk (run: without 0x40, back to standing) */
static void w_walk_ctl(gptr o, int run)
{
    u32 side = W_SIDE(), h = W_HELDV();
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x1000)) U32(o + 0x54) = 1;
    else if (h & 0x20) U32(o + 0x54) = 0x701;
    else if (h & 0x10) U32(o + 0x54) = side | 0x801;
    else if (!run && (h & 0x40)) U32(o + 0x54) = 0x401;
    else if (run && !(h & 0x40)) U32(o + 0x54) = 1;
    else if (U8(W_NEW) & 1) U32(o + 0x54) = 0x601;
    w_turn(o);
}
void DC2CALL Xh24_WalkCtl(gptr o) { w_walk_ctl(o, 0); }               /* 0x4A9C70 */
void DC2CALL Xh24_RunCtl(gptr o) { w_walk_ctl(o, 1); }                /* 0x4A9D80 */

void DC2CALL Xh24_BackCtl(gptr o)                                     /* 0x4A9EA0 */
{
    u32 side = W_SIDE(), h = W_HELDV();
    if (!(h & 0x4000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x4000)) U32(o + 0x54) = 1;
    else if (h & 0x20) U32(o + 0x54) = 0x701;
    else if (h & 0x10) U32(o + 0x54) = side | 0x801;
    else if (U8(W_NEW) & 1) U32(o + 0x54) = 0x601;
    w_turn(o);
}

void DC2CALL Xh24_TurnPad(gptr o) { w_turn(o); }                      /* 0x4A9FA0, 0x4AA060, 0x4AA120 */
void DC2CALL Xh24_SuperCtl(gptr o) { if (U8(o + 0x56) <= 1) w_turn(o); }   /* 0x4AA200 */

/* a move on foot: started (anim), stepped, the speed towards speed */
static void w_go(gptr o, u32 anim, s32 speed)
{
    switch (U8(o + 0x56)) {
    case 0: U8(o + 0x56)++; Bits_Set(W_BITS, 1, 1); Anim_Start(o, anim, 0, 1, 5); break;
    case 1: Anim_Step(o); break;
    }
    Value_Approach(o + 0xF4, 0x1E, (u32)speed);
}
void DC2CALL Xh24_Stand(gptr o) { w_go(o, 0x16, 0); }                 /* 0x4A9A10 */
void DC2CALL Xh24_Turn(gptr o) { w_go(o, 4, 0); }                     /* 0x4A9B30 */
void DC2CALL Xh24_Walk(gptr o) { w_go(o, 4, 0x5A); }                  /* 0x4A9D20 */
void DC2CALL Xh24_Back(gptr o) { w_go(o, 2, -0x50); }                 /* 0x4A9F40 */

void DC2CALL Xh24_TurnToFoe(gptr o)                                   /* 0x4A9B90 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(W_FOE) == o) { U32(o + 0x54) = 1; return; }
        U8(o + 0x56)++;
        Bits_Set(W_BITS, 1, 1);
        Anim_Start(o, 4, 0, 1, 5);
        U16(W_POINT) = U16(o + 0xD8); U16(W_POINT + 2) = U16(o + 0xDA); U16(W_POINT + 4) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u16 a = (u16)Angle_XZ(o + 0x40, W_POINT);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        u8 n = ++U8(o + 0x57);
        if (n >= 0x14 || U16(o + 0x4A) == a) U32(o + 0x54) = 1;
        break;
    }
    }
    Value_Approach(o + 0xF4, 0x1E, 0);
}

void DC2CALL Xh24_Run(gptr o)                                         /* 0x4A9E30 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(W_BITS, 1, 1);
        Anim_Start(o, 0, 0, 1, 5);
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

void DC2CALL Xh24_Shot(gptr o)                                        /* 0x4A9FD0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(W_BITS, 1, 0);
        Anim_Start(o, 1, 0, 0x40, 5);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

static void w_attack_start(gptr o, u32 anim, u32 frame)
{
    U8(o + 0x56)++;
    U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    Anim_Start(o, anim, frame, 0x40, 5);
    Bits_Set(W_BITS, 1, 0);
    U32(0x87E4A4)++;
}

void DC2CALL Xh24_Burst(gptr o)                                       /* 0x4AA090 */
{
    switch (U8(o + 0x56)) {
    case 0: w_attack_start(o, 5, 0xC); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2A) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

void DC2CALL Xh24_SideShot(gptr o)                                    /* 0x4AA150 */
{
    static const u32 anim[2] = {6, 7};
    switch (U8(o + 0x56)) {
    case 0: w_attack_start(o, anim[U8(o + 0x57)], 0); break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0x12C);
        break;
    }
}

void DC2CALL Xh24_Super(gptr o)                                       /* 0x4AA230 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 1, 0, 0, 5);
        Bits_Set(W_BITS, 1, 0);
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

void DC2CALL Xh24_SparkShot(gptr o)                                   /* 0x4AA330 */
{
    if (U8(o + 0x56) == 1) { Flash_Play(o, 0x679004); Effect_Attach(o, 0x67863C); }
}
void DC2CALL Xh24_SparkBurst(gptr o)                                  /* 0x4AA360 */
{
    if (U8(o + 0x56) == 1) { Flash_Play(o, 0x679014); Effect_Attach(o, 0x67863C); }
}
void DC2CALL Xh24_SparkSide(gptr o)                                   /* 0x4AA390 */
{
    static const u32 fx[2] = {0x6787A8, 0x67889C};
    if (U8(o + 0x56) == 1) { Flash_Play(o, 0x679024); Effect_Attach(o, fx[U8(o + 0x57)]); }
}

void DC2CALL Xh24_HitReact(gptr o)                                    /* 0x4AA3E0 */
{
    static const u8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(W_HITK) = kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) += (u16)(U16(o + 0x176) >> 1);
    U32(o + 0x54) = 0x802;
    s32 d = (s32)U32(W_DMG) + U16(o + 0x176);
    U32(W_DMG) = (u32)d;
    if (d > 0xA0) { U32(W_DMG) = 0; U32(o + 0x54) = 0x902; }
    if (U16(W_HITK) == 3) U32(o + 0x54) = 0xA02;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(W_BITS, 1, 0);
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) { U16(o + 0x108) = 0xFFFF; U8(o + 0x50) &= 0xF7; U32(o + 0x54) = 0x803; }
    call(0x73B344, o, U8(o + 0x54));
}

/* back to standing (or to the foe with 8 held and not forward) */
static void w_recover(gptr o)
{
    u32 h = W_HELDV();
    U32(o + 0x54) = !(h & 0x1000) && (h & 8) ? 0x201 : 1;
}

static void w_flinch(gptr o, u16 end)
{
    switch (U8(o + 0x56)) {
    case 0: U8(o + 0x56)++; Anim_Start(o, 8, 0, 0x40, 2); Effect_Attach(o, 0x678990); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) w_recover(o);
        break;
    }
}
void DC2CALL Xh24_Flinch(gptr o) { w_flinch(o, 2); }                 /* 0x4AA580 */
void DC2CALL Xh24_Flinch2(gptr o) { w_flinch(o, 0x19); }             /* 0x4AA5F0 */

void DC2CALL Xh24_Down(gptr o)                                        /* 0x4AA660 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 9, 4, 0x40, 2);
        Effect_Spawn(o, 0x678CB8, 4, 0xFF, 0x100, 0);
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= 0x2A) w_recover(o);
        break;
    }
}

void DC2CALL Xh24_Fall(gptr o)                                        /* 0x4AA6E0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 0xA, 0, 0x40, 0x14);
        Effect_Attach(o, 0x678AAC);
        Bits_Set(W_BITS, 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; U8(o + 0x50) |= 0x30; Layers_Assign(); }
        break;
    case 2: lost(); break;
    }
}

/* ---- type 25 (handler 0x4071E0, PS1 WEP12): a small, quick one with 1 of
   health (every hit flinches it, one more kills it), built like the duel's
   compsognathus: its calls (move 7, up to five) ready the super move, a pack
   of others like it for 150 frames (every other frame one more, each with
   o+0x10C = 1 run by Xh25_Pack: it runs at the point by the foe kept in
   0x746CD4, bites once and is gone). Moves (controls 0x71D624, moves
   0x71D648): 0 stand, 1 turn, 2 turn to the foe, 3 walk, 4 run, 5 walk
   back, 6 spin (o+0x57 which way), 7 call, 8 leap (its attack). Variables:
   0x746CA8 new presses, 0x746D08 held (0x746D0A the angle to the foe),
   0x746CFC the foe, 0x746CB0 bits, 0x746CB8 the kind of hit, 0x746CE4 the
   spin so far, 0x746CE6 the super move's time, 0x746CE8 the calls,
   0x746CF0 the point turned to, 0x746CF8 the pack, 0x746D00/0x746D04 its
   model and animations (for the pack), 0x746D0C the reactions. */
#define C_NEW   0x746CA8u
#define C_BITS  0x746CB0u
#define C_HITK  0x746CB8u
#define C_SPIN  0x746CE4u
#define C_TIME  0x746CE6u
#define C_CALLS 0x746CE8u
#define C_POINT 0x746CF0u
#define C_PACK  0x746CF8u
#define C_FOE   0x746CFCu
#define C_HELD  0x746D08u
#define C_ANGLE 0x746D0Au
#define C_HELDV() U32(C_HELD)
#define C_SIDE()  ((u32)(S16(C_ANGLE) <= 0) << 24)

static void c_turn(gptr o)
{
    if (C_HELDV() & 0x2000) U16(o + 0x4A) += 0x64;
    if (C_HELDV() & 0x8000) U16(o + 0x4A) -= 0x64;
}

void DC2CALL Xh25_Handler(gptr o) { handler(&K25, o); }               /* 0x4071E0 */
void DC2CALL Xh25_Attack(gptr o) { attack(&K25, o); }                 /* 0x4077C0 */
void DC2CALL Xh25_Hit(gptr o) { call(0x71D66C, o, U8(o + 0x55)); }   /* 0x407780 */
void DC2CALL Xh25_Die(gptr o) { call(0x71D690, o, U8(o + 0x55)); }   /* 0x4077A0 */

void DC2CALL Xh25_Init(gptr o)                                        /* 0x4074C0 */
{
    U32(o + 0x50) = 3;
    U16(o + 0x40) = U16(o + 0xE0) = U16(G() + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    DrawSync(o);                                    /* (the object left on the stack) */
    u32 w = U16(G() + 0x10B4);
    Stage_Load(w & 0xFF, w >> 8);
    U16(PX() + 0x3A) = 0x80;
    U32(0x746D00) = U32(o + 0x60); U32(0x746D04) = U32(o + 0x88);
    Target_Find(o, 0x800);
    gptr t = U32(PX() + 0x14);
    U32(C_FOE) = t ? t : o;
    U8(o + 0x54)++;
    U16(o + 0x108) = 1; U16(o + 0x10A) = 1;
    U32(o + 0x12C) = 0x71D5D8; U32(o + 0x128) = 0x71D5E8; U32(o + 0x134) = 0x71D5F8;
    zero_move(o);
    U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x16, 0, 1, 0);
    Obj_Set118(o, 0, 0x1388, 0x1388);
    Head_Reset(o);
    U8(o + 0x15F) = 2; U8(o + 0x160) = 1;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20;
    U16(o + 0x16A) = 0; U16(o + 0x16E) = 0x200;
    U8(o + 0x15D) = U8(U32(C_FOE) + 0xFF);
    gptr sph = U32(U32(C_FOE) + 0x134);
    U8(o + 0x15E) = sph ? U8(sph + 0xA) : 0;
    Bits_Set(C_BITS, 1, 0);
    U32(o + 0x54) = 1;
    U8(o + 0x118) |= 0x20;
    U16(C_PACK) = 0;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* the button sequence after three calls starts the super move (150
   frames; every other frame one more of the pack) */
void DC2CALL Xh25_Normal(gptr o)                                      /* 0x4076C0 */
{
    U16(o + 0x174) = 0;
    if (Combo_Input(0x71D604, U16(C_NEW)) && (S16(C_CALLS) >= 3 || Flag_Check(0, 0xB)) && !U16(C_TIME)) {
        U16(C_TIME) = 0x96; U16(C_CALLS) = 0;
        Kof_Super(o);
    }
    if (U16(C_TIME) && (U8(C_TIME) & 1)) Xh25_Summon(o);
    Flag_Set(7, 0x1B, U16(C_PACK) ? 1 : 0);
    call(0x71D624, o, U8(o + 0x55));
    call(0x71D648, o, U8(o + 0x55));
}

/* one more of the pack, about 5000 away from the point by the foe */
void DC2CALL Xh25_Summon(gptr o)                                      /* 0x408580 */
{
    gptr k = Obj_AllocEnemy();
    if (!k) return;
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0;
    U16(v + 4) = (u16)((s8)Rand8() * 2 + 0x1388);
    u32 a = (u32)Rand8() << 4;
    Vec_RotY(v, v, a);
    U16(k + 0x42) = 0;
    U16(k + 0x40) = (u16)(U16(0x746CD4) + U16(v));
    U16(k + 0x44) = (u16)(U16(0x746CD8) + U16(v + 4));
    Tmp_Pop(8);
    U16(k + 0x4A) = (u16)(0u - a) & 0xFFF;
    U16(k + 0x10C) = 1;
    U8(k + 0x58) = U8(o + 0x58);
    Orig_Call1(U32(U32(0x87DCA0) + U8(o + 0x58) * 4), k);
}

void DC2CALL Xh25_StandCtl(gptr o)                                    /* 0x407810 */
{
    u32 side = C_SIDE();
    u8 nw = U8(C_NEW);
    u32 h = C_HELDV();
    if (nw & 8) U32(o + 0x54) = 0x201;
    else if ((h & 8) && (nw & 4)) U32(o + 0x54) = 0x201;
    else if (h & 0x20) U32(o + 0x54) = 0x801;
    else if (nw & 2) U32(o + 0x54) = side | 0x601;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x501;
    else if (h & 0xA000) U32(o + 0x54) = 0x101;
    else if (nw & 1) U32(o + 0x54) = 0x701;
    if (U8(o + 0x55)) U16(o + 0x42) = U16(o + 0x10E);
}

void DC2CALL Xh25_TurnCtl(gptr o)                                     /* 0x407970 */
{
    u32 side = C_SIDE();
    u8 nw = U8(C_NEW);
    u32 h = C_HELDV();
    if (nw & 8) U32(o + 0x54) = 0x201;
    else if (h & 0x20) U32(o + 0x54) = 0x801;
    else if (nw & 2) U32(o + 0x54) = side | 0x601;
    else if (h & 0x1000) U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x501;
    else if (!(h & 0xA000)) U32(o + 0x54) = 1;
    else if (nw & 1) U32(o + 0x54) = 0x701;
    c_turn(o);
}

/* walk (run: without 0x40, walk) */
static void c_walk_ctl(gptr o, int run)
{
    u32 side = C_SIDE(), h = C_HELDV();
    if (!(h & 0x1000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x1000)) U32(o + 0x54) = 1;
    else if (h & 0x20) U32(o + 0x54) = 0x801;
    else {
        u8 nw = U8(C_NEW);
        if (nw & 2) U32(o + 0x54) = side | 0x601;
        else if (!run && (h & 0x40)) U32(o + 0x54) = 0x401;
        else if (run && !(h & 0x40)) U32(o + 0x54) = 0x301;
        else if (nw & 1) U32(o + 0x54) = 0x701;
    }
    c_turn(o);
}
void DC2CALL Xh25_WalkCtl(gptr o) { c_walk_ctl(o, 0); }               /* 0x407B90 */
void DC2CALL Xh25_RunCtl(gptr o) { c_walk_ctl(o, 1); }                /* 0x407CA0 */

void DC2CALL Xh25_BackCtl(gptr o)                                     /* 0x407DB0 */
{
    u32 side = C_SIDE(), h = C_HELDV();
    if (!(h & 0x4000) && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!(h & 0x4000)) U32(o + 0x54) = 1;
    else if (h & 0x20) U32(o + 0x54) = 0x801;
    else {
        u8 nw = U8(C_NEW);
        if (nw & 2) U32(o + 0x54) = side | 0x601;
        else if (nw & 1) U32(o + 0x54) = 0x701;
    }
    c_turn(o);
}

void DC2CALL Xh25_TurnPad(gptr o) { c_turn(o); }                      /* 0x407FE0 */

static void c_start(gptr o, u32 anim, u32 fl, u32 look, int zero)
{
    U8(o + 0x56)++;
    if (zero) zero_move(o);
    Bits_Set(C_BITS, 1, look);
    Anim_Start(o, anim, 0, fl, 5);
}

void DC2CALL Xh25_Stand(gptr o)                                       /* 0x4078F0 */
{
    switch (U8(o + 0x56)) {
    case 0: c_start(o, 0x16, 0x41, 1, 1); break;
    case 1: if (Anim_Step(o)) U16(o + 0x42) = U16(o + 0x10E); break;
    }
}

void DC2CALL Xh25_Turn(gptr o)                                        /* 0x407A30 */
{
    switch (U8(o + 0x56)) {
    case 0: c_start(o, 2, 0x41, 1, 1); break;
    case 1: Anim_Step(o); break;
    }
}

void DC2CALL Xh25_TurnToFoe(gptr o)                                   /* 0x407AA0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(C_FOE) == o) { U32(o + 0x54) = 1; return; }
        c_start(o, 2, 0x41, 1, 1);
        U16(C_POINT) = U16(o + 0xD8); U16(C_POINT + 2) = U16(o + 0xDA); U16(C_POINT + 4) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u16 a = (u16)Angle_XZ(o + 0x40, C_POINT);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0xFA, a);
        u8 n = ++U8(o + 0x57);
        if (n >= 0x14 || U16(o + 0x4A) == a) U32(o + 0x54) = 1;
        break;
    }
    }
}

static void c_go(gptr o, u32 step, s32 speed)
{
    switch (U8(o + 0x56)) {
    case 0: c_start(o, 2, 1, 1, 0); break;
    case 1: Anim_Step(o); break;
    }
    Value_Approach(o + 0xF4, step, (u32)speed);
}
void DC2CALL Xh25_Walk(gptr o) { c_go(o, 0x14, 0x50); }               /* 0x407C40 */
void DC2CALL Xh25_Run(gptr o) { c_go(o, 0x1E, 0xC8); }                /* 0x407D50 */
void DC2CALL Xh25_Back(gptr o) { c_go(o, 0x1E, -0x46); }              /* 0x407E50 */

void DC2CALL Xh25_Spin(gptr o)                                        /* 0x407EB0 */
{
    static const s16 turn[2] = {0x100, -0x100};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Anim_Start(o, 2, 0, 1, 5);
        Bits_Set(C_BITS, 1, 0);
        U16(C_SPIN) = 0;
        break;
    case 1: {
        Anim_Step(o);
        U16(o + 0x4A) += (u16)turn[U8(o + 0x57)];
        u16 v = (u16)(U16(C_SPIN) + 0x100);
        U16(C_SPIN) = v;
        if ((s16)v >= 0x800) U32(o + 0x54) = 1;
        break;
    }
    }
}

void DC2CALL Xh25_Call(gptr o)                                        /* 0x407F60 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(C_BITS, 1, 0);
        Anim_Start(o, 3, 0, 0x40, 5);
        if (S16(C_CALLS) < 5) U16(C_CALLS)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

void DC2CALL Xh25_Leap(gptr o)                                        /* 0x408010 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 0x40, 5);
        Bits_Set(C_BITS, 1, 0);
        U32(0x87E4A4)++;
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        Value_Approach(o + 0xF4, 0x1E, 0xB4);
        break;
    }
}

/* a bite that lands */
static void c_bite(gptr o) { Effect_Attach(o, 0x667F00); Flash_Play(o, 0x667FCC); Se_Play(o, 0x39); }
void DC2CALL Xh25_SparkLeap(gptr o) { if (U8(o + 0x56) == 1) c_bite(o); }   /* 0x4080A0 */

void DC2CALL Xh25_HitReact(gptr o)                                    /* 0x4080D0 */
{
    static const u8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(C_HITK) = kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) += (u16)(U16(o + 0x176) >> 1);
    U32(o + 0x54) = 0x802;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(C_BITS, 1, 0);
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) { U16(o + 0x108) = 0xFFFF; U8(o + 0x50) &= 0xF7; U32(o + 0x54) = 0x803; }
    call(0x71D614, o, U8(o + 0x54));
}

void DC2CALL Xh25_Flinch(gptr o)                                      /* 0x408220 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x42) = U16(o + 0x10E);
        zero_move(o);
        Anim_Start(o, 1, 0, 0x40, 2);
        Bits_Set(C_BITS, 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) {
            u32 h = C_HELDV();
            U32(o + 0x54) = !(h & 0x1000) && (h & 8) ? 0x201 : 1;
        }
        break;
    }
}

void DC2CALL Xh25_Fall(gptr o)                                        /* 0x4082C0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0x42) = 0;
        zero_move(o);
        Anim_Start(o, 1, 0, 0x40, 2);
        Bits_Set(C_BITS, 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; U8(o + 0x50) |= 0x30; Layers_Assign(); }
        break;
    case 2: lost(); break;
    }
}

/* one of the pack (o+0x10C = 1): states 0x71D6EC */
void DC2CALL Xh25_Pack(gptr o)                                        /* 0x408370 */
{
    call(0x71D6EC, o, U8(o + 0x54));
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (U32(o + 0x50) & 0x40000000) {
        Obj_Disable(o);
        if (U16(C_PACK)) U16(C_PACK)--;
    }
}

void DC2CALL Xh25_PackInit(gptr o)                                    /* 0x4083D0 */
{
    U32(o + 0x50) = 3;
    Model_Setup(o, U32(0x746D00));
    U32(o + 0x88) = U32(0x746D04);
    U32(o + 0x50) |= 0x30;
    U8(o + 0x54)++;
    U16(o + 0x108) = 1; U16(o + 0x10A) = 1;
    U32(o + 0x12C) = 0x71D5D8; U32(o + 0x128) = 0x71D5E8; U32(o + 0x134) = 0x71D5F8;
    zero_move(o);
    U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 4, 0, 1, 0);
    Obj_Set118(o, 0, 0x1388, 0x1388);
    U8(o + 0x118) |= 0x20;
    U32(o + 0x54) = 1;
    U16(C_PACK)++;
}

/* running at the point by the foe; one bite every other frame until it lands,
   gone after 60 frames (10 after the bite) */
void DC2CALL Xh25_PackRun(gptr o)                                     /* 0x4084B0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U8(o + 0x57) = 0x3C;
        Anim_Start(o, 4, 0, 1, 0);
        break;
    case 1:
        Anim_Step(o);
        Obj_TurnToPoint(o, 0x64, 0x746CD4);
        if (U8(o + 0x57) & 1) {
            Kof_Attack(o, 0, 0x64, U16(0x746D0C + U8(o + 0x55) * 2), 0x1F);
            if ((s8)U8(o + 0x119) >= 0) { c_bite(o); U8(o + 0x57) = 0xA; }
        }
        if (!--U8(o + 0x57)) U32(o + 0x50) |= 0x40000000;
        break;
    }
    Value_Approach(o + 0xF4, 0x14, 0xC8);
}

/* ---- type 26 (handler 0x454CE0, PS1 WEP11): health 1200, a fighter that
   heals in its super move. Moves (o+0x55, controls 0x722538, moves
   0x722560): 0 stand, 1 turn on the spot, 2 turn to the foe, 3 walk,
   4 walk back, 5 the dodge (untouchable for a while), 6 turn about
   (o+0x57 which way), 7..9 the attacks (sparks 0x7225D8). Hit 0x722588:
   0..7 the reaction, 8/9 flinches. Die 0x7225B0 (8/9).
   Variables: 0x74B1A8 new presses, 0x74B1FC held (with 0x74B1FE, the
   angle to the foe), 0x74B1F8 the foe, 0x74B1B0 bits (1 may look at the
   foe, 3 the spin's armour, 4 untouchable, 6 the super move: healing),
   0x74B1B8 the kind of hit, 0x74B1E8 the turn about's turn so far,
   0x74B1F0 the point turned to. */
#define B_NEW   0x74B1A8u
#define B_BITS  0x74B1B0u
#define B_HITK  0x74B1B8u
#define B_TURN  0x74B1E8u
#define B_POINT 0x74B1F0u
#define B_FOE   0x74B1F8u
#define B_HELD  0x74B1FCu
#define B_ANGLE 0x74B1FEu
#define B_HELDV() U32(B_HELD)
#define B_SIDE()  ((u32)(S16(B_ANGLE) <= 0) << 24)

void DC2CALL Xh26_Handler(gptr o) { handler(&K26, o); }               /* 0x454CE0 */
void DC2CALL Xh26_Init(gptr o) { init(&K26, o); }                     /* 0x454F70 */
void DC2CALL Xh26_Attack(gptr o) { attack(&K26, o); }                 /* 0x455290 */
void DC2CALL Xh26_Hit(gptr o) { call(0x722588, o, U8(o + 0x55)); }   /* 0x455250 */
void DC2CALL Xh26_Die(gptr o) { call(0x7225B0, o, U8(o + 0x55)); }   /* 0x455270 */

/* the button sequence at half health or less starts the super move: it
   heals a point a frame up to full */
void DC2CALL Xh26_Normal(gptr o)                                      /* 0x455160 */
{
    Bits_Set(B_BITS, 4, 0);
    U16(o + 0x174) = 0;
    if (Combo_Input(0x722518, U16(B_NEW)) && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && !Bits_Check(B_BITS, 6)) {
        Bits_Set(B_BITS, 6, 1);
        Kof_Super(o);
    }
    if (Bits_Check(B_BITS, 6)) {
        U16(o + 0x108)++;
        if (S16(o + 0x108) > S16(o + 0x10A)) { U16(o + 0x108) = U16(o + 0x10A); Bits_Set(B_BITS, 6, 0); }
    }
    call(0x722538, o, U8(o + 0x55));
    call(0x722560, o, U8(o + 0x55));
}

/* ---- the controls ---- */
static void b_turn(gptr o)
{
    if (B_HELDV() & 0x2000) U16(o + 0x4A) += 0x28;
    if (B_HELDV() & 0x8000) U16(o + 0x4A) -= 0x28;
}
/* the attacks (0x20, 0x40, 0x10) and the turn about (new 2) */
static int b_attack(gptr o, u32 h, u8 nw)
{
    if (h & 0x20) { U32(o + 0x54) = 0x701; return 1; }
    if (h & 0x40) { U32(o + 0x54) = 0x801; return 1; }
    if (h & 0x10) { U32(o + 0x54) = 0x901; return 1; }
    if (nw & 2) { U32(o + 0x54) = B_SIDE() | 0x601; return 1; }
    return 0;
}

void DC2CALL Xh26_StandCtl(gptr o)                                    /* 0x4552E0 */
{
    u8 nw = U8(B_NEW);
    u32 h = B_HELDV();
    if ((nw & 8) || ((h & 8) && (nw & 4))) { U32(o + 0x54) = 0x201; return; }
    if (b_attack(o, h, nw)) return;
    if (h & 0x1000) U32(o + 0x54) = 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x401;
    else if (h & 0xA000) U32(o + 0x54) = 0x101;
    else if (nw & 1) U32(o + 0x54) = 0x501;
}

void DC2CALL Xh26_TurnCtl(gptr o)                                     /* 0x455440 */
{
    u8 nw = U8(B_NEW);
    u32 h = B_HELDV();
    if (nw & 8) U32(o + 0x54) = 0x201;
    else if (b_attack(o, h, nw)) {}
    else if (h & 0x1000) U32(o + 0x54) = 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x401;
    else if (!(h & 0xA000)) U32(o + 0x54) = 1;
    else if (nw & 1) U32(o + 0x54) = 0x501;
    b_turn(o);
}

/* walk (0x1000) or back (0x4000) while held */
static void b_walk_ctl(gptr o, u32 key)
{
    u32 h = B_HELDV();
    u8 nw = U8(B_NEW);
    if (!(h & key)) U32(o + 0x54) = h & 8 ? 0x201 : 1;
    else if (h & 0x20) U32(o + 0x54) = 0x701;
    else if (h & 0x40) U32(o + 0x54) = 0x801;
    else if (h & 0x10) U32(o + 0x54) = 0x901;
    else if (nw & 2) U32(o + 0x54) = B_SIDE() | 0x601;
    else if (nw & 1) U32(o + 0x54) = 0x501;
    b_turn(o);
}
void DC2CALL Xh26_WalkCtl(gptr o) { b_walk_ctl(o, 0x1000); }          /* 0x455670 */
void DC2CALL Xh26_BackCtl(gptr o) { b_walk_ctl(o, 0x4000); }          /* 0x4557A0 */
void DC2CALL Xh26_TurnPad(gptr o) { b_turn(o); }                      /* 0x455AA0, 0x455C80 */

/* the second attack: again (0x20 held) early on */
void DC2CALL Xh26_KickCtl(gptr o)                                     /* 0x455B80 */
{
    b_turn(o);
    if (U8(o + 0x56) && U16(o + 0x92) < 0x17 && (U8(B_HELD) & 0x20)) Anim_Start(o, 5, 5, 0x40, 5);
}

/* ---- the moves ---- */
static void b_start(gptr o, u32 anim, u32 frame, u32 fl, u32 blend, u32 look)
{
    U8(o + 0x56)++;
    zero_move(o);
    Anim_Start(o, anim, frame, fl, blend);
    Bits_Set(B_BITS, 1, look);
}

void DC2CALL Xh26_Stand(gptr o)                                       /* 0x4553D0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        zero_move(o);
        Bits_Set(B_BITS, 1, 1);
        Anim_Start(o, 0x16, 0, 1, 0xF);
        break;
    case 1: Anim_Step(o); break;
    }
}

void DC2CALL Xh26_Turn(gptr o)                                        /* 0x455510 */
{
    switch (U8(o + 0x56)) {
    case 0: b_start(o, 0, 0, 1, 0xA, 1); break;
    case 1: Anim_Step(o); break;
    }
}

void DC2CALL Xh26_TurnToFoe(gptr o)                                   /* 0x455580 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(B_FOE) == o) { U32(o + 0x54) = 1; return; }
        b_start(o, 0, 0, 1, 0xA, 1);
        U16(B_POINT) = U16(o + 0xD8); U16(B_POINT + 2) = U16(o + 0xDA); U16(B_POINT + 4) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u16 a = (u16)Angle_XZ(o + 0x40, B_POINT);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x96, a);
        u8 n = ++U8(o + 0x57);
        if (n >= 0x14 || U16(o + 0x4A) == a) U32(o + 0x54) = 1;
        break;
    }
    }
}

static void b_go(gptr o, u32 anim, u32 fl, u32 blend, s32 speed)
{
    switch (U8(o + 0x56)) {
    case 0: b_start(o, anim, 0, fl, blend, 1); break;
    case 1: Anim_Step(o); Value_Approach(o + 0xF4, 0x14, (u32)speed); break;
    }
}
void DC2CALL Xh26_Walk(gptr o) { b_go(o, 0, 1, 0xA, 0x3C); }          /* 0x455730 */
void DC2CALL Xh26_Back(gptr o) { b_go(o, 4, 9, 0x14, -0x1E); }        /* 0x455860 */

/* the dodge: untouchable from frame 14 to 49 */
void DC2CALL Xh26_Dodge(gptr o)                                       /* 0x4558D0 */
{
    switch (U8(o + 0x56)) {
    case 0: b_start(o, 1, 0, 0x40, 0xA, 0); break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        u16 f = U16(o + 0x92);
        if (f >= 0xE && f <= 0x31) Bits_Set(B_BITS, 4, 1);
        break;
    }
    }
}

/* turn about (o+0x57: half a turn either way), the turn carried by B_TURN
   while it plays and given to the body (and back to its first part) at
   the end */
void DC2CALL Xh26_TurnAbout(gptr o)                                   /* 0x455970 */
{
    static const s16 turn[2] = {0x800, -0x800}, len[2] = {0x21, 0x21};
    static const u32 anim[2] = {2, 3};
    u8 w = U8(o + 0x57);
    switch (U8(o + 0x56)) {
    case 0:
        b_start(o, anim[w], 1, 0x40, 5, 0);
        U16(B_TURN) = 0;
        break;
    case 1: {
        U16(B_TURN) += (u16)(turn[w] / len[w]);
        Anim_Step(o);
        if (U16(o + 0x92) < 0x26) break;
        U16(B_TURN) = 0;
        gptr p = U32(o + 0x64);
        U16(p + 0x4A) = (u16)((U16(p + 0x4A) - turn[w]) & 0xFFF);
        U16(o + 0x4A) = (u16)((turn[w] + U16(o + 0x4A)) & 0xFFF);
        Anim_Set(o, 0, 0, 1, 0xA);
        U32(o + 0x54) = 1;
        break;
    }
    }
}

/* an attack: untouchable between lo and hi, done at end */
static void b_attack_move(gptr o, u32 anim, u32 frame, u16 end, u16 lo, u16 hi)
{
    switch (U8(o + 0x56)) {
    case 0:
        b_start(o, anim, frame, 0x40, 0xA, 0);
        U32(0x87E4A4)++;
        break;
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= end) U32(o + 0x54) = 1;
        if (f >= lo && f <= hi) Bits_Set(B_BITS, 4, 1);
        break;
    }
    }
}
void DC2CALL Xh26_Punch(gptr o) { b_attack_move(o, 7, 2, 0x1D, 0xE, 0x18); }   /* 0x455AD0 */
void DC2CALL Xh26_Kick(gptr o) { b_attack_move(o, 5, 0, 0x2B, 0x15, 0x24); }   /* 0x455BD0 */

/* the spin: armoured from frame 21 to 53, moving forward */
void DC2CALL Xh26_Spin(gptr o)                                        /* 0x455CB0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        b_start(o, 6, 0xE, 0x40, 0xA, 0);
        Se_Play(o, 0x3E);
        U32(0x87E4A4)++;
        break;
    case 1: {
        Anim_Step(o);
        u16 f = U16(o + 0x92);
        if (f >= 0x41) U32(o + 0x54) = 1;
        Bits_Set(B_BITS, 3, f > 0x14 && f < 0x36);
        Value_Approach(o + 0xF4, 0x14, 0x78);
        break;
    }
    }
}

void DC2CALL Xh26_SparkPunch(gptr o)                                  /* 0x455D80 */
{
    if (U8(o + 0x56) == 1) { Flash_Play(o, 0x66CCE8); Effect_Attach(o, 0x66BF04); }
}
void DC2CALL Xh26_SparkKick(gptr o)                                   /* 0x455DB0 */
{
    if (U8(o + 0x56) == 1) { Flash_Play(o, 0x66CCC8); Effect_Attach(o, 0x66BC54); }
}
void DC2CALL Xh26_SparkSpin(gptr o)                                   /* 0x455DE0 */
{
    if (U8(o + 0x56) == 1) Flash_Play(o, 0x66CCD8);
}

/* ---- hit and dying ---- */
/* hit (moves 0..7): the kind of hit by the move, half again off guard;
   knocked back (0x902) by a heavy hit, while untouchable or armoured,
   else flinching (0x802); the super move's healing stops */
void DC2CALL Xh26_HitReact(gptr o)                                    /* 0x455E00 */
{
    static const u8 kind[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(B_HITK) = kind[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) += (u16)(U16(o + 0x176) >> 1);
    U32(o + 0x54) = 0x802;
    if (Bits_Check(B_BITS, 4)) U32(o + 0x54) = 0x902;
    if (U16(B_HITK) == 3) U32(o + 0x54) = 0x902;
    if (Bits_Check(B_BITS, 3)) U32(o + 0x54) = 0x902;
    Player_Release(o);
    U32(o + 0x50) |= 8;
    zero_move(o);
    Bits_Set(B_BITS, 3, 0);
    Bits_Set(B_BITS, 4, 0);
    Bits_Set(B_BITS, 6, 0);
    if (U16(B_TURN)) { U16(o + 0x4A) = (u16)((U16(o + 0x4A) + U16(B_TURN)) & 0xFFF); U16(B_TURN) = 0; }
    U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
    U8(o + 0x17B) = 0xA;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U32(o + 0x50) &= ~8u;
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = (u32)U8(o + 0x55) << 8 | 3;
    }
    call(0x722528, o, U8(o + 0x54));
}

static void b_flinch(gptr o, u16 end)
{
    switch (U8(o + 0x56)) {
    case 0: U8(o + 0x56)++; Anim_Start(o, 9, 0, 0x40, 2); Effect_Attach(o, 0x66BFD4); break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) { u32 h = B_HELDV(); U32(o + 0x54) = !(h & 0x1000) && (h & 8) ? 0x201 : 1; }
        break;
    }
}
void DC2CALL Xh26_Flinch(gptr o) { b_flinch(o, 0xA); }               /* 0x455FE0 */
void DC2CALL Xh26_Flinch2(gptr o) { b_flinch(o, 0x26); }             /* 0x456050 */

void DC2CALL Xh26_Fall(gptr o)                                        /* 0x4560C0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        Anim_Start(o, 0xF, 0, 0x40, 0x14);
        Effect_Attach(o, 0x66CA44);
        Bits_Set(B_BITS, 1, 0);
        break;
    case 1:
        if (Anim_Step(o)) { U8(o + 0x56)++; U8(o + 0x50) |= 0x30; Layers_Assign(); }
        if (U16(o + 0x92) == 0x2D) Prop_SpawnC(0, o + 0x40, 2, 0x28, 1, 0);
        break;
    case 2: lost(); break;
    }
}
