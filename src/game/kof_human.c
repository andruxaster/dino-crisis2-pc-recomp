/* The human of the extra game's duel (area 9 object type 23, handler
 * 0x4A2B10): a player with a gun and a machete against a dinosaur, played
 * like the duel's dinosaurs (moves, super move by a button sequence).
 * States (o+0x54, 0x73B0B0): 0 init, 1 normal (by o+0x55: the move, its
 * control 0x73B0C0 then its animation 0x73B0F0), 2 hit (0x73B120), 3 die
 * (0x73B14C).
 * Moves: 0 stand, 1 walk, 2 turn to the foe, 3 run, 4 dash, 5 back, 6 the
 * super move, 7 taunt, 8 the machete combo, 9..11 the gun's attacks (o+0x57
 * their side: 1 when the foe is to the left); 0x73B1A8/0x73B1C0 their
 * damage and reaction, 0x73B178 their sparks.
 * Variables: 0x759E88 the pad's new presses (0x87DC94), 0x759EE4 held
 * (0x87DC08), 0x759EE6 the angle to the foe from its head, 0x759EE0 the
 * foe (the player's target or itself), 0x759E90 bits (1: may be locked
 * on), 0x759E98 the last hit's strength, 0x759E9A the state before a hit,
 * 0x759EA0/0x759EA4 damage counted towards being knocked down/away,
 * 0x759EA8, 0x759EDC the dash's cool-down, 0x759ECC the combo's wait,
 * 0x759ED4 the place turned to. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define H_PAD   0x759E88u
#define H_BITS  0x759E90u
#define H_HIT   0x759E98u
#define H_SAVED 0x759E9Au
#define H_DMG1  0x759EA0u
#define H_DMG2  0x759EA4u
#define H_DMG3  0x759EA8u
#define H_WAIT  0x759ECCu
#define H_TURN  0x759ED4u
#define H_DASH  0x759EDCu
#define H_FOE   0x759EE0u
#define H_HELD  0x759EE4u
#define H_ANGLE 0x759EE6u
#define H_PX    0x87FD60u

static gptr px(void) { return U32(H_PX); }

/* the player's next target after o among the live, targetable objects
 * (the first object after the player if none), or none */
void DC2CALL Target_Next(gptr o)                                    /* 0x47BF70 */
{
    if (!o) o = U32(G() + 0x940) + 0x260;
    U32(px() + 0x14) = 0;
    u32 n = U32(0x87F940);
    gptr p = o;
    for (u32 i = 1; i < n; i++) {
        p += 0x260;
        if (p == o) break;
        gptr base = U32(G() + 0x940);
        if (p >= base + n * 0x260) p = base + 0x260;
        u32 f = U32(p + 0x50);
        if ((f & 1) && (f & 8) && !(f & 0x10000)) { U32(px() + 0x14) = p; break; }
    }
    gptr t = U32(px() + 0x14);
    if (t) {
        u32 f = U32(t + 0x50);
        if (!(f & 1) || !(f & 8) || (f & 0x10000)) U32(px() + 0x14) = 0;
    }
}

/* the foe's types it does not push against (0x4A2E44 by type - 3: 0 pushes) */
static int foe_solid(gptr foe, const u8 *map)
{
    u32 t = U8(foe + 0x58) - 3;
    return t <= 0x23 ? map[t] == 0 : 0;
}
static const u8 g_push_map[0x24] = {0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
static const u8 g_super_map[0x24] = {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0};

void DC2CALL Hum_Handler(gptr o)                                    /* 0x4A2B10 */
{
    U16(H_HELD) = U16(0x87DC08);
    U32(H_PX) = o + 0x1C0;
    U16(H_PAD) = U16(0x87DC94);
    if (U16(H_PAD) & 8) Target_Find(o, 0x800);
    if ((U8(H_HELD) & 8) && (U8(H_PAD) & 4)) Target_Next(U32(px() + 0x14));
    gptr t = U32(px() + 0x14);
    if (!t || !(U8(t + 0x50) & 1)) Target_Next(t);
    t = U32(px() + 0x14);
    U32(H_FOE) = t ? t : o;
    gptr foe = U32(H_FOE);
    U8(o + 0x15D) = U8(foe + 0xFF);
    gptr sph = U32(U32(H_FOE) + 0x134);
    U8(o + 0x15E) = sph ? U8(sph + 0xA) : 0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    U16(o + 0xD8) = U16(U32(H_FOE) + 0x40);
    U16(o + 0xDA) = U16(U32(H_FOE) + 0x42);
    U16(o + 0xDC) = U16(U32(H_FOE) + 0x44);
    gptr head = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    U16(H_ANGLE) = head ? (u16)Angle_DiffTo(head + 0x40, o + 0xD8, U16(o + 0x4A)) : 0;
    Obj_SavePos(o);
    Orig_Call1(U32(0x73B0B0 + U8(o + 0x54) * 4), o);
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    s32 a = (s16)U16(H_ANGLE);
    U32(o + 0x158) = ((a < 0 ? -a : a) < 0x500 && U32(H_FOE) != o && Bits_Check(H_BITS, 1)) ? 2 : 1;
    Head_Update(o);
    Obj_UpdateMatrix(o);
    if (foe_solid(U32(H_FOE), g_push_map)) U32(o + 0x50) &= ~0x20u;
    else U32(o + 0x50) |= 0x20;
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    if (!(U8(o + 0x50) & 0x80)) Zone_Update(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
    U16(px() + 0x36)++;
    Hum_Attack(o);
    s32 d1 = (s32)U32(H_DMG1), d2 = (s32)U32(H_DMG2), d3 = (s32)U32(H_DMG3);
    if (d1) U32(H_DMG1) = (u32)(d1 -= 3);
    if (d2) U32(H_DMG2) = (u32)(d2 -= 2);
    if (d3) U32(H_DMG3) = (u32)(d3 -= 1);
    if (d1 < 0) U32(H_DMG1) = 0;
    if (d2 < 0) U32(H_DMG2) = 0;
    if (d3 < 0) U32(H_DMG3) = 0;
    if (U8(H_DASH)) U8(H_DASH)--;
    for (u32 k = 0; k < 4; k++) U8(H_SAVED + k) = U8(o + 0x54 + k);
}

/* the attack spheres of the move (damage 0x73B1A8, reaction 0x73B1C0;
 * none while dashing again, nor the dash on some foes), then its sparks */
void DC2CALL Hum_Attack(gptr o)                                     /* 0x4A3120 */
{
    u8 mv = U8(o + 0x55);
    u16 dmg = U16(0x73B1A8 + mv * 2);
    if (mv == 4 && U8(H_DASH)) dmg = 0;
    if (foe_solid(U32(H_FOE), g_super_map) && mv == 4) dmg = 0;
    Kof_Attack(o, 0, dmg, U16(0x73B1C0 + mv * 2), mv == 4 ? 0x23 : 0x1F);
    if ((s8)U8(o + 0x119) >= 0) Orig_Call1(U32(0x73B178 + U8(o + 0x55) * 4), o);
}

/* state 0: at the checkpoint, the weapons loaded, 2000 health, targeting */
void DC2CALL Hum_Init(gptr o)                                       /* 0x4A2E70 */
{
    gptr g = G();
    U32(o + 0x50) = 3;
    U16(o + 0x40) = U16(o + 0xE0) = U16(g + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    DrawSync(o);
    u16 w = U16(G() + 0x10B4);
    Stage_Load(w & 0xFF, w >> 8);
    U16(px() + 0x3A) = 0x80;
    Target_Find(o, 0x800);
    gptr t = U32(px() + 0x14);
    U32(H_FOE) = t ? t : o;
    U8(o + 0x54)++;
    U16(o + 0x108) = 0x7D0; U16(o + 0x10A) = 0x7D0;
    U32(o + 0x12C) = 0x73B030; U32(o + 0x128) = 0x73B060; U32(o + 0x134) = 0x73B070;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    U32(o + 0x188) = 0;
    Anim_Start(o, 0x17, 0, 1, 0);
    Obj_Set118(o, 0, 0x4E20, 0x7530);
    Head_Reset(o);
    U8(o + 0x15F) = 3; U8(o + 0x160) = 2;
    U16(o + 0x164) = 0x15; U16(o + 0x166) = 0x20; U16(o + 0x16A) = 0x80; U16(o + 0x16E) = 0x300;
    U8(o + 0x15D) = U8(U32(H_FOE) + 0xFF);
    gptr sph = U32(U32(H_FOE) + 0x134);
    U8(o + 0x15E) = sph ? U8(sph + 0xA) : 0;
    Bits_Set(H_BITS, 1, 0);
    U8(o + 0x118) |= 0x20;
    U8(o + 0xFE) = 3;
    U32(o + 0x54) = 1;
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
}

/* state 1: the super move by its sequence (at half health or less, or
 * with flag 0:0xB), then the move */
void DC2CALL Hum_Normal(gptr o)                                     /* 0x4A3060 */
{
    U16(o + 0x174) = 0;
    if (Combo_Input(0x73B0A0, U16(H_PAD)) && ((s16)(S16(o + 0x10A) >> 1) >= S16(o + 0x108) || Flag_Check(0, 0xB))
        && U8(o + 0x55) != 6) {
        U32(o + 0x54) = 0x601;
        Kof_Super(o);
    }
    Orig_Call1(U32(0x73B0C0 + U8(o + 0x55) * 4), o);
    Orig_Call1(U32(0x73B0F0 + U8(o + 0x55) * 4), o);
}

void DC2CALL Hum_Hit(gptr o) { Orig_Call1(U32(0x73B120 + U8(o + 0x55) * 4), o); }   /* 0x4A30E0 */
void DC2CALL Hum_Die(gptr o) { Orig_Call1(U32(0x73B14C + U8(o + 0x55) * 4), o); }   /* 0x4A3100 */

/* ---- the moves' controls ---- */

static u32 side(void) { return (u32)(S16(H_ANGLE) <= 0) << 24; }

static void steer(gptr o, u16 step)
{
    if (U16(H_HELD) & 0x2000) U16(o + 0x4A) += step;
    if (U16(H_HELD) & 0x8000) U16(o + 0x4A) -= step;
}

/* move 0: standing; target (8), the gun's attacks (with up: 0x40 and
 * action 0x20 together, alone 0x20, 0x10), up runs (0x40 dashes), back,
 * turning, the taunt */
void DC2CALL Hum_StandCtl(gptr o)                                   /* 0x4A31F0 */
{
    u32 b = side();
    u8 pad = U8(H_PAD);
    u16 h = U16(H_HELD);
    if (pad & 8) { U32(o + 0x54) = 0x201; return; }
    if ((h & 8) && (pad & 4)) { U32(o + 0x54) = 0x201; return; }
    u16 up = h & 0x1000;
    if (!(up && (h & 0x40)) && (h & 0x20)) { U32(o + 0x54) = b | 0x901; return; }
    if (up && (h & 0x40) && (h & 0x20)) { U32(o + 0x54) = 0xA01; return; }
    if (h & 0x10) { U32(o + 0x54) = b | 0xB01; return; }
    if (up) { U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x501; return; }
    if (h & 0x2000) { U32(o + 0x54) = 0x101; return; }
    if (h & 0x8000) { U32(o + 0x54) = 0x101; return; }
    if (pad & 1) U32(o + 0x54) = 0x701;
}

/* move 1: walking (turning on the spot) */
void DC2CALL Hum_WalkCtl(gptr o)                                    /* 0x4A3390 */
{
    u32 b = side();
    u8 pad = U8(H_PAD);
    u16 h = U16(H_HELD), up = h & 0x1000;
    if (pad & 8) U32(o + 0x54) = 0x201;
    else if (up && (h & 0x40) && (pad & 0x20)) U32(o + 0x54) = 0xA01;
    else if (h & 0x20) U32(o + 0x54) = b | 0x901;
    else if (h & 0x10) U32(o + 0x54) = b | 0xB01;
    else if (up) U32(o + 0x54) = h & 0x40 ? 0x401 : 0x301;
    else if (h & 0x4000) U32(o + 0x54) = 0x501;
    else if (!(h & 0xA000)) U32(o + 0x54) = 1;
    else if (pad & 1) U32(o + 0x54) = 0x701;
    steer(o, 0x28);
}

/* move 3: running */
void DC2CALL Hum_RunCtl(gptr o)                                     /* 0x4A35D0 */
{
    u32 b = side();
    u16 h = U16(H_HELD), up = h & 0x1000, d = h & 0x40;
    if (!up && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!up) U32(o + 0x54) = 1;
    else if (d && (h & 0x20)) U32(o + 0x54) = 0xA01;
    else if (!d && (h & 0x20)) U32(o + 0x54) = b | 0x901;
    else if (h & 0x10) U32(o + 0x54) = b | 0xB01;
    else if (d) U32(o + 0x54) = 0x401;
    else if (U8(H_PAD) & 1) U32(o + 0x54) = 0x701;
    steer(o, 0x28);
}

/* move 4: dashing */
void DC2CALL Hum_DashCtl(gptr o)                                    /* 0x4A3780 */
{
    u32 b = side();
    u16 h = U16(H_HELD), up = h & 0x1000;
    if (!up && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!up) U32(o + 0x54) = 1;
    else if (!(h & 0x40)) U32(o + 0x54) = 0x301;
    else if (h & 0x20) U32(o + 0x54) = 0xA01;
    else if (h & 0x10) U32(o + 0x54) = b | 0xB01;
    else if (U8(H_PAD) & 1) U32(o + 0x54) = 0x701;
    steer(o, 0x28);
}

/* move 5: stepping back */
void DC2CALL Hum_BackCtl(gptr o)                                    /* 0x4A3910 */
{
    u32 b = side();
    u16 h = U16(H_HELD), dn = h & 0x4000;
    if (!dn && (h & 8)) U32(o + 0x54) = 0x201;
    else if (!dn) U32(o + 0x54) = 1;
    else if (h & 0x20) U32(o + 0x54) = b | 0x901;
    else if (h & 0x10) U32(o + 0x54) = b | 0xB01;
    else if (U8(H_PAD) & 1) U32(o + 0x54) = 0x701;
    steer(o, 0x28);
}

void DC2CALL Hum_SuperCtl(gptr o) { steer(o, 0x14); }              /* 0x4A3AA0 */
void DC2CALL Hum_Gun9Ctl(gptr o) { steer(o, 0x28); }               /* 0x4A3DD0 */
void DC2CALL Hum_Gun10Ctl(gptr o) { steer(o, 0x28); }              /* 0x4A3EE0 */
void DC2CALL Hum_Gun11Ctl(gptr o) { steer(o, 0x28); }              /* 0x4A3FF0 */

/* ---- the moves' animations ---- */

static void move_start(gptr o, u32 lock)
{
    U8(o + 0x56)++;
    U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
    Bits_Set(H_BITS, 1, lock);
}

/* a step's sound, the ground shaking a little */
static void stomp(gptr o, u32 se)
{
    Shake_Set(2, 0, 0x1E);
    Flash_Play(o, 0x679D80);
    if (se) Se_Play(o, se);
}

void DC2CALL Hum_Stand(gptr o)                                      /* 0x4A3300 */
{
    static const u32 anim[2] = {0x17, 5};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 1);
        Anim_Start(o, anim[Anim_FrameFlag(o)], 0, 1, 0x14);
        break;
    case 1: Anim_Step(o); break;
    }
}

void DC2CALL Hum_Walk(gptr o)                                       /* 0x4A3480 */
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 1, 0xA);
        Bits_Set(H_BITS, 1, 1);
        break;
    case 1: Anim_Step(o); break;
    }
}

/* move 2: turning to the foe (stands at once without one) */
void DC2CALL Hum_Turn(gptr o)                                       /* 0x4A34F0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        if (U32(H_FOE) == o) { U32(o + 0x54) = 1; return; }
        U8(o + 0x56)++;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Anim_Start(o, 0, 0, 1, 0xA);
        Bits_Set(H_BITS, 1, 1);
        U16(H_TURN) = U16(o + 0xD8); U16(H_TURN + 2) = U16(o + 0xDA); U16(H_TURN + 4) = U16(o + 0xDC);
        break;
    case 1: {
        Anim_Step(o);
        u32 a = Angle_XZ(o + 0x40, H_TURN);
        U16(o + 0x4A) += (u16)Angle_Approach(U16(o + 0x4A), 0x64, a);
        U8(o + 0x57)++;
        if (U8(o + 0x57) >= 0x14 || U16(o + 0x4A) == (u16)a) U32(o + 0x54) = 1;
        break; }
    }
}

/* runs and dashes: the walk cycle from a frame by the foot down, the
 * speed up to `top`, two stomps */
static void run(gptr o, u32 anim, u32 fr1, u32 fr0, s16 top, u16 s1, u16 s2)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 1);
        Anim_Start(o, anim, Anim_FrameFlag(o) ? fr1 : fr0, 1, 0x14);
        if (anim == 2) U8(H_DASH) = 0x1E;
        break;
    case 1:
        Anim_Step(o);
        Value_Approach(o + 0xF4, 0x14, (u32)(s32)top);
        break;
    }
    if (U16(o + 0x92) == s1) stomp(o, 0x30);
    if (U16(o + 0x92) == s2) stomp(o, 0x31);
}

void DC2CALL Hum_Run(gptr o)  { run(o, 0, 0xF, 3, 0x64, 0xB, 0x3C); }    /* 0x4A36A0 */
void DC2CALL Hum_Dash(gptr o) { run(o, 2, 0xD, 4, 0xD2, 0xA, 0x28); }    /* 0x4A3830 */
void DC2CALL Hum_Back(gptr o) { run(o, 1, 0x12, 3, -0x50, 0xA, 0x45); }  /* 0x4A39C0 */

/* move 6: the super move: the screen flashes white at frame 0x38 */
void DC2CALL Hum_Super(gptr o)                                      /* 0x4A3AD0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        move_start(o, 0);
        Anim_Start(o, 3, 0, 0x40, 0x14);
        Flash_Play(o, 0x679D90);
        U32(0x87E4A4)++;
        break;
    case 1: {
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        u16 f = U16(o + 0x92);
        if (f == 0x38) {
            Shake_Set(3, 0, 0x1C);
            gptr g = G();
            U8(g + 0xB58) = 0x80; U8(g + 0xB59) = 0x80; U8(g + 0xB5A) = 0x80; U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
            f = U16(o + 0x92);
        }
        if (f >= 0x38) DrawSync(0);
        break; }
    }
}

/* move 7: the taunt */
void DC2CALL Hum_Taunt(gptr o)                                      /* 0x4A3BC0 */
{
    switch (U8(o + 0x56)) {
    case 0:
        move_start(o, 0);
        Anim_Start(o, 4, 0, 0x40, 0x14);
        break;
    case 1:
        if (Anim_Step(o)) U32(o + 0x54) = 1;
        break;
    }
}

/* move 8: the machete: swing, held 120 frames at most, back, then run */
void DC2CALL Hum_Machete(gptr o)                                    /* 0x4A3C30 */
{
    static const u32 a0[2] = {0xF, 0x12}, fx[2] = {0x678F10, 0x679144}, a1[2] = {0x10, 0x13}, a2[2] = {0x11, 0x14};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U8(o + 0x57) = (u8)Anim_FrameFlag(o);
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 0);
        Anim_Start(o, a0[U8(o + 0x57)], 0, 0x40, 0x14);
        Effect_Attach(o, fx[U8(o + 0x57)]);
        Flash_Play(o, 0x679E00);
        U16(H_WAIT) = 0x78;
        U8(o + 0x17B) = 0xA;
        break;
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
        if (!U16(H_WAIT) || !--U16(H_WAIT)) {
            U8(o + 0x56)++;
            Anim_Start(o, a2[U8(o + 0x57)], 0, 0x40, 0xA);
        }
        break;
    case 3:
        if (Anim_Step(o)) U32(o + 0x54) = 0x301;
        break;
    }
}

/* the gun's attacks: start (the shot counted at 0x87E4A4), stand at frame `end` */
static void gun(gptr o, const u32 *anim, u32 frame, u32 blend, int ff, u16 end, u16 s1, u16 s2)
{
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        if (ff) U8(o + 0x57) = (u8)Anim_FrameFlag(o);
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 0);
        Anim_Start(o, anim[U8(o + 0x57)], frame, 0x40, blend);
        U32(0x87E4A4)++;
        break;
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) U32(o + 0x54) = 1;
        break;
    }
    if (s1 && (U16(o + 0x92) == s1 || U16(o + 0x92) == s2)) stomp(o, 0);
}

void DC2CALL Hum_Gun9(gptr o)                                       /* 0x4A3E00 */
{
    static const u32 a[3] = {8, 9, 0xA};
    gun(o, a, 4, 0x14, 0, 0x3C, 0xD, 0x3C);
}

void DC2CALL Hum_Gun10(gptr o)                                      /* 0x4A3F10 */
{
    static const u32 a[2] = {0xE, 0xD};
    gun(o, a, 8, 0xA, 1, 0x3C, 0x12, 0x32);
}

void DC2CALL Hum_Gun11(gptr o)                                      /* 0x4A4020 */
{
    static const u32 a[2] = {0xC, 0xB};
    gun(o, a, 1, 0x14, 0, 0x26, 0, 0);
}

/* ---- the sparks of the attacks (0x73B178 by move, on their first frame) ---- */

void DC2CALL Hum_Spark4(gptr o)                                     /* 0x4A41B0 */
{
    if (U8(o + 0x56) == 1) Flash_Play(o, 0x679DCC);
}

void DC2CALL Hum_Spark9(gptr o)                                     /* 0x4A40C0 */
{
    static const u32 fx[3] = {0x676E4C, 0x676F68, 0x677084};
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, 0x679DCC);
    Effect_Attach(o, fx[U8(o + 0x57)]);
}

void DC2CALL Hum_Spark10(gptr o)                                    /* 0x4A4110 */
{
    static const u32 fx[2] = {0x6778B8, 0x677A4C};
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, 0x679DEC);
    Effect_Attach(o, fx[U8(o + 0x57)]);
}

void DC2CALL Hum_Spark11(gptr o)                                    /* 0x4A4160 */
{
    static const u32 fx[2] = {0x677218, 0x677568};
    if (U8(o + 0x56) != 1) return;
    Flash_Play(o, 0x679DDC);
    Effect_Attach(o, fx[U8(o + 0x57)]);
}

/* ---- hit ---- */

/* hits 0..7: the strength (0x759E98: 0 light, 1, 2, 3 heavy), the move
 * goes on (state before the hit) unless enough damage knocks it down
 * (0x759EA0 over 160: hit 8; 0x759EA4 over 300: 9) or the hit is heavy
 * (10); the damage, dying below 0 */
void DC2CALL Hum_Hurt(gptr o)                                       /* 0x4A41D0 */
{
    static const u32 fx[3] = {0x677FD0, 0x67804C, 0x6780F0};
    static const s8 strength[8] = {0, 1, 2, 2, 0, 1, 2, 3};
    if (U8(o + 0x55) < 8) U16(H_HIT) = (u16)strength[U8(o + 0x55)];
    if (U8(o + 0x184) & 8) U16(o + 0x176) = (u16)(U16(o + 0x176) + (U16(o + 0x176) >> 1));
    U32(o + 0x54) = U8(H_SAVED) | U8(H_SAVED + 1) << 8 | U8(H_SAVED + 2) << 16 | (u32)U8(H_SAVED + 3) << 24;
    U32(H_DMG1) += U16(o + 0x176);
    U32(H_DMG2) += U16(o + 0x176);
    if ((s32)U32(H_DMG1) > 0xA0) {
        U32(H_DMG1) = 0;
        U32(o + 0x54) = 0x802;
        if ((s32)U32(H_DMG2) > 0x12C) { U32(H_DMG2) = 0; U32(o + 0x54) = 0x902; }
    }
    if (U16(H_HIT) == 3) U32(o + 0x54) = 0xA02;
    if (U8(o + 0x54) == 2 && !U8(o + 0x56)) {
        Player_Release(o);
        U32(o + 0x50) |= 8;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 0);
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U8(o + 0x17B) = 0xA;
    } else {
        U16(o + 0x108) -= (u16)Damage_Scale(U16(o + 0x176));
        U8(o + 0x17B) = 0xA;
        Effect_Attach(o, fx[(s32)(Rand8() & 0xFF) % 3]);
    }
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = U16(o + 0x10A);
    if (S16(o + 0x108) < 0) {
        U16(o + 0x108) = 0xFFFF;
        U32(o + 0x54) = 0x803;
        U32(o + 0x50) &= ~8u;
    }
    Orig_Call1(U32(0x73B0B0 + U8(o + 0x54) * 4), o);
}

/* knocked down (8, 9) or away (10): turned to the foe, the fall, its
 * sparks; up again at `end` (to targeting while it is held) */
static void knocked(gptr o, const u32 *anim, const u32 *fx, u16 end, int twice, int se)
{
    Obj_TurnFromHead(o, 0x28, o + 0xD8);
    switch (U8(o + 0x56)) {
    case 0: {
        U8(o + 0x56)++;
        U8(o + 0x57) = (u8)Anim_FrameFlag(o);
        u32 k = twice ? Anim_FrameFlag(o) : U8(o + 0x57);
        Anim_Start(o, anim[k], 1, 0x40, 5);
        Effect_Spawn(o, fx[U8(o + 0x57)], 4, 0xFF, 0x100, 0);
        if (se) Se_Play(o, 0x30);
        break; }
    case 1:
        Anim_Step(o);
        if (U16(o + 0x92) >= end) {
            u16 h = U16(H_HELD);
            U32(o + 0x54) = !(h & 0x1000) && (h & 8) ? 0x201 : 1;
        }
        break;
    }
}

void DC2CALL Hum_Down1(gptr o)                                      /* 0x4A4440 */
{
    static const u32 a[2] = {0x15, 0x16}, fx[2] = {0x677BE0, 0x677E14};
    knocked(o, a, fx, 0xE, 0, 1);
    if (U16(o + 0x92) == 0x1E || U16(o + 0x92) == 0x2D) stomp(o, 0);
}

void DC2CALL Hum_Down2(gptr o)                                      /* 0x4A4550 */
{
    static const u32 a[2] = {0x15, 0x16}, fx[2] = {0x677BE0, 0x677E14};
    knocked(o, a, fx, 0x1A, 0, 1);
    if (U16(o + 0x92) == 0x1E || U16(o + 0x92) == 0x2D) stomp(o, 0);
}

void DC2CALL Hum_Away(gptr o)                                       /* 0x4A4660 */
{
    static const u32 a[2] = {0x18, 0x19}, fx[2] = {0x679300, 0x679804};
    knocked(o, a, fx, 0x22, 1, 0);
    u16 f = U16(o + 0x92);
    if (f == 0xD || f == 0x1B || f == 0x64 || f == 0x73) stomp(o, 0);
}

/* state 3: falling dead, the body a prop; then game over (or the duel's
 * flag 7:0x1E) */
void DC2CALL Hum_Dead(gptr o)                                       /* 0x4A47E0 */
{
    static const u32 a[2] = {6, 7}, fx[2] = {0x678288, 0x6788F4};
    switch (U8(o + 0x56)) {
    case 0:
        U8(o + 0x56)++;
        U8(o + 0x57) = (u8)Anim_FrameFlag(o);
        Anim_Start(o, a[U8(o + 0x57)], 0, 0x40, 5);
        Effect_Attach(o, fx[U8(o + 0x57)]);
        U32(o + 0x50) &= ~8u;
        U16(o + 0xF4) = 0; U16(o + 0xF2) = 0; U16(o + 0xF0) = 0;
        Bits_Set(H_BITS, 1, 0);
        Flash_Play(o, 0x679DA4);
        break;
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
