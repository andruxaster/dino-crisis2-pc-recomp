/* The vehicle's gun (area 9 object type 43, handler 0x4A4940; the PS1
 * overlay WEP0C): the player sits at the gun, turns it with left/right,
 * lifts it with up/down, fires the gun (0x20) and the homing missiles
 * (0x10); G+0x927 picks the heavier mount (more health, faster turns,
 * bigger missiles).
 * States (o+0x54, 0x73B220): 0 init, 1 normal, 2 and 5 hit, 3 die,
 * 4 the animation only. The move (o+0x55, 0x73B238): 0 still (its engine
 * sound stopped), 1..3 turning (the engine sound).
 * Variables: 0x759EE8 / 0x759EEA the two bars of the sight (0..0xB8, they
 * wrap), 0x759EEC the gun's lift this frame (1 up, 2 down), 0x759EEE the
 * gun's pitch (part 1's +0x4C). In the player's extra block: +0x7B the
 * missile's cool-down, +0x7C the missiles left. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"
#include "dc2/pcdraw.h"

#define V_BAR1  0x759EE8u
#define V_BAR2  0x759EEAu
#define V_LIFT  0x759EECu
#define V_PITCH 0x759EEEu
#define V_PX    0x87FD60u

static gptr px(void) { return U32(V_PX); }

/* the barrel (part 1) at the gun's pitch, raised by `up` */
static void barrel(gptr o, u16 up)
{
    gptr p = Obj_Part(U32(o + 0x64), 1);
    U16(p + 0x48) = up;
    U16(p + 0x4C) = U16(V_PITCH);
}

/* the speed back towards 0 by `step` */
static void slow_down(gptr o, s16 step)
{
    s16 v = S16(o + 0xF4);
    if (v < 0) {
        v = (s16)(v + step); S16(o + 0xF4) = v;
        if (v > 0) S16(o + 0xF4) = 0;
    } else if (v > 0) {
        v = (s16)(v - step); S16(o + 0xF4) = v;
        if (v < 0) S16(o + 0xF4) = 0;
    }
}

void DC2CALL Vgun_Handler(gptr o)                                   /* 0x4A4940 */
{
    Obj_SavePos(o);
    U32(V_PX) = o + 0x1C0;
    if (Flag_Check(0, 0xB)) U16(o + 0x108) = 0x4B0;
    Orig_Call1(U32(0x73B220 + U8(o + 0x54) * 4), o);
    U16(px() + 0x36)++;
    u16 h = (u16)(U16(o + 0x5A) * 1000);
    U16(o + 0x42) = U8(px() + 0x36) & 1 ? (u16)(-5 - h) : (u16)(U16(o + 0x5A) * 0xFC18);   /* (it shakes) */
    RotMatrixYXZ(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
    Thrown_Update(o);
    if (!(U8(o + 0x50) & 0x80)) Zone_Update(o);
    Player_Breath(o);
    for (gptr p = U32(G() + 0x940); p < U32(G() + 0x940) + U32(0x87F940) * 0x260; p += 0x260)
        if (U8(p + 0x17B)) U8(p + 0x17B)--;
}

/* state 0: at the checkpoint, the weapons loaded, its health and size by
 * the mount, six missiles */
void DC2CALL Vgun_Init(gptr o)                                      /* 0x4A4A80 */
{
    U32(o + 0x50) = 3;
    u16 hp = U8(G() + 0x927) ? 0x960 : 0x4B0;
    U16(o + 0x108) = hp; U16(o + 0x10A) = hp;
    U16(o + 0x40) = U16(o + 0xE0) = U16(G() + 0x840);
    U16(o + 0x42) = U16(o + 0xE2) = U16(G() + 0x842);
    U16(o + 0x44) = U16(o + 0xE4) = U16(G() + 0x844);
    U16(o + 0x4A) = U16(G() + 0x846);
    U16(o + 0x5A) = (u16)((s32)S16(o + 0x42) / -1000);
    DrawSync(o);
    u16 w = U16(G() + 0x10B4);
    Stage_Load(w & 0xFF, w >> 8);
    U8(o + 0x15F) = 7; U8(o + 0x160) = 0;
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60; U16(o + 0x16A) = 0x100; U16(o + 0x16E) = 0x280;
    U8(o + 0x15D) = 0xFF; U8(o + 0x15E) = 0;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x300;
    U8(o + 0x15C) = 0;
    U16(px() + 0x3A) = 0x80;
    if (U8(G() + 0x927)) {
        U32(o + 0x128) = 0x73B1E4; U32(o + 0x12C) = 0x73B1FC; U32(o + 0x134) = 0x73B214;
        Obj_Set118(o, 0, 0x55F0, 0x6978);
    } else {
        U32(o + 0x128) = 0x73B1D8; U32(o + 0x12C) = 0x73B1F0; U32(o + 0x134) = 0x73B208;
        Obj_Set118(o, 0, 0x2EE0, 0x3A98);
    }
    U32(o + 0x188) = 0;
    U32(o + 0x54) = 1;
    Anim_Start(o, 0, 0, 1, 0);
    Obj_UpdateMatrix(o);
    Obj_SavePos(o);
    U8(px() + 0x7C) = 6;
    U16(V_BAR1) = 0; U16(V_BAR2) = 0; U16(V_PITCH) = 0;
}

/* turning hard (held with the mount's gun): the gun's sweep hits what is
 * ahead (0x21) */
static void sweep(gptr o, gptr t, u16 ry)
{
    gptr p = Obj_Part(U32(o + 0x64), 0);
    U16(t + 0x10) = 0x400; U16(t + 0x12) = ry; U16(t + 0x14) = 0;
    RotMatrixYXZ(t + 0x10, t + 0x20);
    Weapon_Hit(t + 0x20, p + 0x40, 0x21);
}

/* one bar of the sight: two pieces of the frame at 0x280,y copied to
 * (0x380, at) by the bar's place v */
static void sight(gptr t, s32 v, u16 y, u16 at)
{
    gptr r = t + 0x18;
    U16(r) = (u16)(0x280 + v / 2); U16(r + 2) = y; U16(r + 4) = (u16)((0xB8 - v) / 2); U16(r + 6) = 0x24;
    PcDraw_VramCopy(r, 0x380, at);
    U16(r) = 0x280; U16(r + 2) = y; U16(r + 4) = (u16)(v / 2); U16(r + 6) = 0x24;
    PcDraw_VramCopy(r, 0x380 - (v - 0xB8) / 2, at);
}

/* a bar's place: past 0xB8 back to 0, below 0 to 0xB8 */
static s32 bar_wrap(u32 a)
{
    s16 v = S16(a);
    if (v > 0xB8) U16(a) = 0;
    else if (v < 0) U16(a) = 0xB8;
    return S16(a);
}

/* (x * the magic of a division by 10 >> 32 >> sh, rounded towards 0) */
static s32 div_m10(s32 x, int sh)
{
    s32 q = (s32)(((long long)x * (s32)0x99999999) >> 32) >> sh;
    return q + (s32)((u32)q >> 31);
}

/* state 1: the pad turns and lifts the gun, fires it and the missiles,
 * then the sight */
void DC2CALL Vgun_Normal(gptr o)                                    /* 0x4A4C70 */
{
    gptr t = Tmp_Push(0x40);
    int big = U8(G() + 0x927) != 0;
    Anim_Step(o);
    Orig_Call1(U32(0x73B238 + U8(o + 0x55) * 4), o);
    if (U32(0x87DC08) & 0x2000) { U16(o + 0x4A) += 0x18; U16(V_BAR1) -= 4; U16(V_BAR2) += 2; }
    if (U32(0x87DC94) & 0x2000) U32(o + 0x54) = 0x301;
    if (U32(0x87DC08) & 0x8000) { U16(o + 0x4A) -= 0x18; U16(V_BAR1) += 2; U16(V_BAR2) -= 4; }
    if (U32(0x87DC94) & 0x8000) U32(o + 0x54) = 0x301;
    if (U32(0x87DC08) & 0x1000) {
        if (big) {
            sweep(o, t, U16(o + 0x4A));
            S16(o + 0xF4) = (s16)(S16(o + 0xF4) + 0x18);
            if (S16(o + 0xF4) > 0x90) U16(o + 0xF4) = 0x90;
        } else {
            S16(o + 0xF4) = (s16)(S16(o + 0xF4) + 0xC);
            if (S16(o + 0xF4) > 0x48) U16(o + 0xF4) = 0x48;
        }
        if (U32(0x87DC94) & 0x1000) U32(o + 0x54) = 0x101;
    } else if (U32(0x87DC08) & 0x4000) {
        if (big) {
            sweep(o, t, (u16)((U16(o + 0x4A) - 0x800) & 0xFFF));
            S16(o + 0xF4) = (s16)(S16(o + 0xF4) - 0xC);
            if (S16(o + 0xF4) < -0x48) S16(o + 0xF4) = -0x48;
        } else {
            S16(o + 0xF4) = (s16)(S16(o + 0xF4) - 6);
            if (S16(o + 0xF4) < -0x24) S16(o + 0xF4) = -0x24;
        }
        if (U32(0x87DC94) & 0x4000) U32(o + 0x54) = 0x201;
    }
    slow_down(o, big ? 8 : 4);
    if (!(U32(0x87DC08) & 0xF000)) U32(o + 0x54) = 1;

    u16 hd = U16(0x87DC08), up = hd & 4, down = hd & 8;     /* the gun's lift */
    U8(V_LIFT) = 0;
    if (up) U8(V_LIFT) = 1;
    if (down) U8(V_LIFT) = 2;
    if (up && down) U8(V_LIFT) = 0;
    else if (U8(V_LIFT)) {
        Se_Play(o, 0x1F);
        if (U8(V_LIFT) == 1) U16(V_PITCH) += 0x28;
        else if (U8(V_LIFT) == 2) U16(V_PITCH) -= 0x28;
    }
    gptr b = Obj_Part(U32(o + 0x64), 1);
    U16(b + 0x48) = 0x400;
    U16(b + 0x4C) = U16(V_PITCH);

    if ((U8(0x87DC08) & 0x20) && U8(px() + 0x2F)) {        /* the gun */
        U16(px() + 0x3C) = 0;
        U8(px() + 0x2F) = 0;
        U32(G() + 0x684)++;
        Weapon_Hit(b + 0x20, b + 0x40, 0xC);
        Flash_Start(o, 0xF);
        Flash_StartFor(o, 0xF, 0xC8);
        Effect_Attach(o, 0x666D34);
        Se_Play(o, 0x10);
        S16(o + 0xF4) = (s16)(S16(o + 0xF4) >> 2);
    }
    if (U8(px() + 0x7B)) U8(px() + 0x7B)--;
    gptr e;
    if ((U8(0x87DC08) & 0x10) && !U8(px() + 0x7B) && (e = Eff_Alloc()) && U8(px() + 0x7C)) {   /* a missile */
        U8(px() + 0x7C)--;
        U8(px() + 0x7B) = 0x28;
        Obj_UpdateMatrix(o);
        U32(e + 0x50) = 3;
        U32(e + 0x68) = U8(G() + 0x927) ? 0x6668FC : 0x66674C;
        U8(e + 0x4E) = 3;
        U32(e + 0x54) = 0;
        U8(e + 0x4F) = 0xA;
        U16(e + 0x48) = 0x800;
        U16(e + 0x4A) = U16(o + 0x4A);
        U16(e + 0x4C) = 0;
        RotMatrixYXZ(e + 0x48, e + 0x20);
        U16(t + 0x10) = U16(b + 0x48); U16(t + 0x12) = U16(b + 0x4A);
        U16(t + 0x14) = (u16)(U16(b + 0x4C) - U16(o + 0x4A));
        RotMatrix(t + 0x10, t + 0x20);
        if (U8(G() + 0x927)) { U16(t) = 0x30C; U16(t + 2) = 0xFD80; U16(t + 4) = 0x190; }
        else { U16(t) = 0x186; U16(t + 2) = 0xFEC0; U16(t + 4) = 0xC8; }
        ApplyMatrixSV(t + 0x20, t, t);
        U16(e + 0x40) = (u16)(U16(b + 0x40) + U16(t));
        U16(e + 0x42) = (u16)(U16(b + 0x42) + U16(t + 2));
        U16(e + 0x44) = (u16)(U16(b + 0x44) + U16(t + 4));
        U32(e + 0x58) = U32(e + 0x40);
        U16(e + 0x5C) = U16(e + 0x44);
        U16(e + 0x60) = 0;
        U16(e + 0x62) = U8(G() + 0x927) ? 0x258 : 0x12C;
        U16(e + 0x64) = 0;
        Target_Find(o, 0x800);
        if (U32(px() + 0x14)) {                             /* homing on the first target's first sphere */
            gptr tg = U32(G() + 0x940) + U8(px() + 8) * 0x260;
            gptr d = U32(tg + 0x134);
            U16(0x87FD80 + S8(e + 0x46) * 4) = U8(tg + 0xFF);
            U16(0x87FD82 + S8(e + 0x46) * 4) = U8(d + 0xA);
        } else {
            U16(e + 0x48) = 0x800;
            U16(0x87FD80 + S8(e + 0x46) * 4) = 0xFFFF;
            U16(0x87FD82 + S8(e + 0x46) * 4) = 0;
        }
        U8(e + 0x46) = 0xFA; U8(e + 0x47) = 0x1E;
        Effect_Attach(o, 0x667080);
        Flash_Start(o, 0xA);
        Sub_AmmoUse(1);
        Se_Play(o, 0x14);
    }

    s32 f = S16(o + 0xF4), sh = U8(G() + 0x927) ? 4 : 3;   /* the sight's bars move against the turn */
    U16(V_BAR2) = (u16)(U16(V_BAR2) + div_m10(f, sh));
    U16(V_BAR1) = (u16)(U16(V_BAR1) + div_m10(S16(o + 0xF4), sh));
    sight(t, bar_wrap(V_BAR2), 0x1B8, 0xB8);
    sight(t, bar_wrap(V_BAR1), 0x1DC, 0xDC);
    Tmp_Pop(0x40);
}

/* states 2 and 5: thrown by the hit (o+0x178 odd: sideways, by the
 * table 0x7047A8 / 0x7047B0 for o+0x55), the damage, its health below 0:
 * dying; then 15 frames of shaking while it slows down */
void DC2CALL Vgun_Hit(gptr o)                                       /* 0x4A5430 */
{
    Anim_Step(o);
    barrel(o, 0x400);
    u8 c = U8(o + 0x56);
    if (!c) {
        s32 a = S16(o + 0x178);
        u16 v = U16((U8(o + 0x55) ? 0x7047B0 : 0x7047A8) + a * 2);
        if (a & 1) {
            U16(o + 0xF0) = v;
            if (U8(G() + 0x927)) U16(o + 0xF0) = (u16)(U16(o + 0xF0) << 1);
            U16(o + 0xF4) = 0; U16(o + 0xF2) = 0;
        } else {
            U32(o + 0xF0) = 0;
            U16(o + 0xF4) = v;
            if (U8(G() + 0x927)) U16(o + 0xF4) = (u16)(U16(o + 0xF4) << 1);
        }
        U16(o + 0x108) = (u16)(U16(o + 0x108) - Damage_Scale(U16(o + 0x176)));
        U16(o + 0x174) = 1;
        U8(o + 0x56) = c + 1;
        Effect_Attach(o, 0x6672BC);
        Se_Play(o, 3);
        if (S16(o + 0x108) < 0) U32(o + 0x54) = 3;
        return;
    }
    if (c > 0xF) {
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U16(o + 0x48) = 0; U16(o + 0x4C) = 0;
        U16(o + 0x174) = 0;
        U32(o + 0x54) = 1;
        return;
    }
    slow_down(o, U8(G() + 0x927) ? 4 : 2);
    U8(o + 0x56) = c + 1;
    u32 at = U8(o + 0x178) & 1 ? 0x48 : 0x4C;
    if (Rand8() & 1) U16(o + at) = (u16)((Rand8() & 0xF) + 5);
    else U16(o + at) = (u16)(-5 - (Rand8() & 0xF));
}

/* state 3: dead, game over */
void DC2CALL Vgun_Die(gptr o)                                       /* 0x4A5660 */
{
    switch (U8(o + 0x55)) {
    case 0:
        Anim_Step(o);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x55)++;
        GameOver_Start(0);
        break;
    case 1:
        Anim_Step(o);
        break;
    }
    barrel(o, 0x400);
}

/* move 0: still, the engine sound stopped */
void DC2CALL Vgun_Still(gptr o)                                     /* 0x4A56E0 */
{
    if (U8(o + 0x56)) return;
    Se_StopGroup(0x1E);
    U8(o + 0x56)++;
}

/* moves 1..3: turning, the engine sound */
void DC2CALL Vgun_Turn(gptr o)                                      /* 0x4A5700 */
{
    if (U8(o + 0x56)) return;
    Se_Play(o, 0x1E);
    U8(o + 0x56)++;
}
