/* Firearms: the weapon substates of the player's normal state (5: the
 * weapon's state handler, by +0x56 through 0x73B44C + weapon * 0x24; 6:
 * firing, the aim handler 0x738048), ammunition, muzzle flashes.
 * Player block (*0x87FD60): +0x14 target, +0x20 yaw toward it, +0x2A
 * target cycling index, +0x2E aim held, +0x2F may fire, +0x30 upper-body
 * pitch (0x800 level), +0x32 pitch while switching, +0x3C frames since the
 * last shot. Substates (+0x56): 0 ready, 1 hold aim, 2 lower, 3 next
 * target, 4 aimed turn back, 5 walk, 6 walk back, 7 turn, 8 nothing. */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define PX()     U32(0x87FD60)
#define HELD     U32(0x87DC08)
#define PRESSED  U8(0x87DC94)

/* ---- ammunition, flashes ---------------------------------------------------------- */
/* n if the weapon has n rounds (or ammunition is unlimited), else 0 */
u32 DC2CALL Ammo_Check(u32 n)                                      /* 0x4961D0 */
{
    if (U8(G() + 0x1094) & 1) return n & 0xFFFF;
    u8 i = (u8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B4));
    return S16(G() + i * 12 + 0x10C4) < (s32)(n & 0xFFFF) ? 0 : n & 0xFFFF;
}

/* n rounds fired: taken from the weapon's item unless unlimited; counted */
void DC2CALL Ammo_Use(u32 n)                                       /* 0x4962B0 */
{
    U8(G() + 0x75A) = 100;
    U8(G() + 0x75B) = 0;
    u8 i = (u8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B4));
    if (!(U8(G() + 0x1094) & 1)) U16(G() + i * 12 + 0x10C4) -= (u16)n;
    U32(G() + i * 12 + 0x10C0) += n & 0xFFFF;
    U32(G() + 0x684)++;
}

/* a free flash slot (0x18 bytes, +0xC flags): the room's ten at G+0x94C,
 * none while flag 2/4 is set */
gptr DC2CALL Flash_Alloc(void)                                     /* 0x478AA0 */
{
    if (Flag_Check(2, 4)) return 0;
    for (gptr p = G() + 0x94C; p < G() + 0xA3C; p += 0x18)
        if (!(U8(p + 0xC) & 1)) { memset(MEM(p), 0, 0x18); U8(p + 0xC) |= 1; return p; }
    return 0;
}

/* the same among an object's two at +0x190 */
gptr DC2CALL Flash_AllocObj(gptr o)                                /* 0x478AF0 */
{
    if (Flag_Check(2, 4)) return 0;
    for (gptr p = o + 0x190; p < o + 0x1C0; p += 0x18)
        if (!(U8(p + 0xC) & 1)) { memset(MEM(p), 0, 0x18); U8(p + 0xC) |= 1; return p; }
    return 0;
}

/* muzzle flash of kind k (one frame) */
gptr DC2CALL Flash_Start(gptr o, u32 k)                            /* 0x478A60 */
{
    gptr p = o ? Flash_AllocObj(o) : Flash_Alloc();
    if (p) {
        U8(p + 0xE) = (u8)k;
        U8(p + 0x13) = 1;
        U8(p + 0xC) = (u8)((U8(p + 0xC) & 0xFB) | 3);
    }
    return p;
}

/* light of kind k for n frames */
gptr DC2CALL Flash_StartFor(gptr o, u32 k, u32 n)                  /* 0x478B40 */
{
    gptr p = o ? Flash_AllocObj(o) : Flash_Alloc();
    if (p) {
        U8(p + 0x13) = (u8)n;
        U8(p + 0xC) |= 7;
        U8(p + 0xE) = (u8)k;
    }
    return p;
}

/* ---- shared pieces ---------------------------------------------------------------- */
/* quick turn / dodge; 1 if taken */
static int dodge(gptr o)
{
    if (!(PRESSED & 0x40) || !Flag_Check(2, 0x1C)) return 0;
    u8 h = (u8)(HELD >> 8);
    u32 st = 0;
    if (h & 0x40) st = (h & 0x20) ? 0x30B01 : (h & 0x80) ? 0x40B01 : 0xB01;
    else if (h & 0x20) st = 0x10B01;
    else if (h & 0x80) st = 0x20B01;
    if (!st) return 0;
    U32(o + 0x54) = st;
    Flag_Set(2, 0xD, 0);
    U8(o + 0x11B) = 0;
    return 1;
}

/* flags of the shared substates: HEAVY (weapons 10, 11: the dodge button
 * crouches), BARE (weapon 0: checks for 0 rounds) */
#define HEAVY 1
#define BARE  2

/* weapons 10 and 11: the dodge button only crouches (state 0x901) */
static int heavy_dodge(gptr o)
{
    if (!(PRESSED & 0x40) || Fn_4BF330()) return 0;
    Flag_Set(2, 0xD, 0);
    Flag_Set(1, 9, 1);
    U8(o + 0x11B) = 0;
    U32(o + 0x1DC) = 4;
    U32(o + 0x54) = 0x901;
    return 1;
}

static int dodge_by(gptr o, int heavy) { return heavy ? heavy_dodge(o) : dodge(o); }

/* sub-weapon button with a sub-weapon: 1 (state 0xA01 unless it is done) */
static int sub_weapon(gptr o, int clear_foot)
{
    if (!(PRESSED & 0x10) || !(U16(G() + 0x10B4) & 0xFF00)) return 0;
    if (!Player_SubDone()) {
        if (clear_foot) U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0xA01;
    }
    return 1;
}

/* angles from the eye (o's position 1000 up, plus the gun's offset when
 * gun is set) to t's aim part: low half pitch, high half yaw */
static u32 look_at(gptr o, gptr t, int gun)
{
    gptr f = Tmp_Push(0x30), sv = f, ang = f + 8, mm = f + 0x10;
    gptr p = Obj_Part(U32(t + 0x64), U8(U32(t + 0x134) + 0xA));
    U16(sv) = 0x16; U16(sv + 2) = 0; U16(sv + 4) = 0xFF22;
    RotMatrix(o + 0x48, mm);
    ApplyMatrixSV(mm, sv, sv);
    if (gun) {
        U16(sv) += U16(o + 0x40);
        U16(sv + 4) += U16(o + 0x44);
        U16(sv + 2) += (u16)(U16(o + 0x42) - 0x3E8);
    } else {                                    /* the offset is computed, then overwritten */
        U16(sv) = U16(o + 0x40);
        U16(sv + 2) = (u16)(U16(o + 0x42) - 0x3E8);
        U16(sv + 4) = U16(o + 0x44);
    }
    Angles_LookAt(p + 0x40, sv, ang);
    u32 a = U32(ang);
    Tmp_Pop(0x30);
    return a;
}

/* pitch of the upper body from a look_at result */
static u16 pitch_of(u32 a)
{
    s16 x = (s16)a;
    return (u16)(x > 0x200 ? 0x1000 : x < -0x200 ? 0 : a * 4 + 0x800);
}

/* fire one round: hit along the gun (part 3), flash, effect, sound */
static void fire(gptr o, gptr effect, int ammo_first)
{
    if (ammo_first) Ammo_Use(1);
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 1);
    if (!ammo_first) Ammo_Use(1);
    Flash_Start(o, 8);
    Flash_StartFor(o, 5, 0x46);
    Effect_Attach(o, effect);
    Se_Play(o, 0x10);
}

/* ---- the weapons' state handlers (counting frames since the last shot) -------------- */
static void wstate(gptr o, s16 delay, gptr table)
{
    if (delay >= 0) {
        s16 n = S16(PX() + 0x3C);
        U16(PX() + 0x3C) = (u16)(n + 1);
        if (n > delay) U8(PX() + 0x2F) = 1;
    }
    Orig_Call1(U32(table + U8(o + 0x56) * 4), o);
}

void DC2CALL WState_0(gptr o)  { wstate(o, -1, 0x73B44C); }       /* 0x4AA7A0 */
void DC2CALL WState_1(gptr o)  { wstate(o, 0x12, 0x73B470); }     /* 0x4AB450 */
void DC2CALL WState_2(gptr o)  { wstate(o, 0xC, 0x73B494); }      /* 0x4ABD10 */
void DC2CALL WState_3(gptr o)  { wstate(o, 0x12, 0x73B4B8); }     /* 0x4AC9A0 */
void DC2CALL WState_4(gptr o)  { wstate(o, 0x12, 0x73B4DC); }     /* 0x4AD6A0 */
void DC2CALL WState_5(gptr o)  { wstate(o, 2, 0x73B500); }        /* 0x4AE320 */
void DC2CALL WState_6(gptr o)  { wstate(o, 1, 0x73B524); }        /* 0x4AEF60 */
void DC2CALL WState_7(gptr o)  { wstate(o, 1, 0x73B548); }        /* 0x4AF500 */
void DC2CALL WState_8(gptr o)  { wstate(o, 0x12, 0x73B56C); }     /* 0x4B0490 */
void DC2CALL WState_9(gptr o)  { wstate(o, 0x12, 0x73B590); }     /* 0x4B08D0 */
void DC2CALL WState_10(gptr o) { wstate(o, 9, 0x73B5B4); }        /* 0x4B0ED0 */
void DC2CALL WState_11(gptr o) { wstate(o, 0xF, 0x73B5D8); }      /* 0x4B1CB0 */

/* ---- handgun ------------------------------------------------------------------------ */
/* firing (normal substate 6) */
void DC2CALL Handgun_Fire(gptr o)                                  /* 0x4AB490 */
{
    if (U8(o + 0x56) == 0) {
        Anim_StartLayers(0x2A, 0x2C, 0, 0, 1, U16(PX() + 0x30));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        Ammo_Use(1);
        gptr p = Obj_Part(U32(o + 0x64), 3);
        Weapon_Hit(p + 0x20, p + 0x40, 1);
        Flash_Start(o, 8);
        Flash_StartFor(o, 5, 0x46);
        Effect_Attach(o, 0x67A7BC);
        Se_Play(o, 0x10);
        return;
    }
    if (U8(o + 0x56) != 1) return;
    if (!Anim_StepLayers(U16(PX() + 0x30))) { U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0; return; }
    U8(PX() + 0x2F) = 1;
    u32 h = HELD;
    if ((h & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (h & 2) { U32(o + 0x54) = 0x40501; return; }
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) { U32(o + 0x54) = 0x20501; return; }
    if (h & 0x20) {
        if (Ammo_Check(1)) { U32(o + 0x54) = 0x601; return; }
    } else {
        if ((PRESSED & 4) && !Flag_Check(2, 0xE)) { U32(o + 0x54) = 0x30501; return; }
        if (HELD & 0xA000) { U32(o + 0x54) = 0x70501; return; }
    }
    U32(o + 0x1DC) = 0;
    U32(o + 0x54) = 0x10501;
}

/* substate 0: raise the gun at the nearest target (wait: the frames to the
 * next shot after a walk, through PX+0x3C) */
static void ready(gptr o, u16 wait, int heavy)
{
    u8 s = U8(o + 0x57);
    if (s == 1 || s == 2) {
        if (Anim_StepLayers(U16(PX() + 0x30))) { U8(PX() + 0x2E) = 1; U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 1; }
    } else if (s == 0 || s == 0xFF) {
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        if (Flag_Check(2, 0xE)) U32(PX() + 0x14) = 0; else Target_Find(o, 0x800);
        gptr t = U32(PX() + 0x14);
        if (t) {
            u32 a = look_at(o, t, 1);
            U16(PX() + 0x20) = (u16)(a >> 16);
            U16(PX() + 0x30) = pitch_of(a);
            U16(PX() + 0x2A) = U8(PX() + 8);
        } else {
            U16(PX() + 0x20) = U16(o + 0x4A);
            U16(PX() + 0x30) = 0x800;
            U16(PX() + 0x2A) = 0;
        }
        U8(o + 0x11B) = 0;
        if (U8(o + 0x57) == 0xFF) {
            Anim_StartLayers(0x39, 0x37, 0, 0, 8, U16(PX() + 0x30));
            U8(PX() + 0x2E) = 1;
            U8(o + 0x57) = 2;
        } else {
            Anim_StartLayers(0x27, 0x29, 0, 0x40, 8, U16(PX() + 0x30));
            U8(PX() + 0x2E) = 0;
            U8(o + 0x57)++;
        }
    }
    if (dodge_by(o, heavy & HEAVY)) return;
    u32 h = HELD;
    if (!(h & 8)) {
        U8(o + 0x11B) = U8(o + 0x57) == 1 ? 0x11 : 0;
        U32(o + 0x54) = 0x20501;
        return;
    }
    if (sub_weapon(o, 0)) return;
    if (h & 0x1000) { U16(PX() + 0x3C) = wait; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U16(PX() + 0x3C) = wait; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x60501; return; }
    u32 d = Angle_Approach(U16(o + 0x4A), 0x104, U16(PX() + 0x20)) & 0xFFFF;
    U16(o + 0x4A) += (u16)d;
    if (d || !U8(PX() + 0x2E)) return;
    if (PRESSED & 2) { U32(o + 0x54) = 0x40501; U32(o + 0x54) = 0x10501; return; }   /* overwritten, as in the original */
    if ((U8(0x87DC08) & 0x20) && Ammo_Check(heavy & BARE ? 0 : 1) && U8(PX() + 0x2F)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x601;
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
        return;
    }
    U32(o + 0x54) = 0x10501;
}

void DC2CALL Bare_Ready(gptr o)  { ready(o, 0xD, BARE); }         /* 0x4AA7C0 */
void DC2CALL Gun_Ready(gptr o)   { ready(o, 0xD, 0); }              /* 0x4ACCC0 */
void DC2CALL Shotgun_Ready(gptr o) { ready(o, 7, 0); }              /* 0x4AC140 */
void DC2CALL MGun_Ready(gptr o)    { ready(o, 0xFFFD, 0); }          /* 0x4ADCE0 */

/* substate 1: hold the aim, follow the target's height */
static void hold(gptr o, int heavy)
{
    Target_Find(o, 0x200);
    gptr t = U32(PX() + 0x14);
    u32 want = t ? pitch_of(look_at(o, t, 1)) : 0x800;
    U16(PX() + 0x30) += (u16)Angle_Step(U16(PX() + 0x30), 0x180, want);
    Anim_StepLayers(U16(PX() + 0x30));
    if (dodge_by(o, heavy & HEAVY)) return;
    u8 pr = PRESSED;
    if (pr & 0x10 && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
    } else if (pr & 2) U32(o + 0x54) = 0x40501;
    else {
        u32 h = HELD;
        if (h & 0x1000) U32(o + 0x54) = 0x50501;
        else if (h & 0x4000) U32(o + 0x54) = 0x60501;
        else if (!(h & 8)) U32(o + 0x54) = 0x20501;
        else if (h & 0x20) {
            if (Ammo_Check(1)) {
                if (U8(PX() + 0x2F)) {
                    U8(o + 0x11B) = 0;
                    U32(o + 0x54) = 0x601;
                    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
                    return;
                }
            } else if (PRESSED & 0x20) Se_Play(o, 0x11);       /* empty click */
        }
    }
    if ((PRESSED & 4) && !Flag_Check(2, 0xE)) { U32(o + 0x54) = 0x30501; return; }
    if (HELD & 0xA000) U32(o + 0x54) = 0x70501;
}

void DC2CALL Gun_Hold(gptr o)   { hold(o, 0); }                   /* 0x4AD0D0 */
void DC2CALL Heavy_Hold(gptr o) { hold(o, HEAVY); }                   /* 0x4B23A0 */

/* substate 2: lower the gun, back to standing */
static void lower(gptr o, u16 wait, int heavy)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_StartLayers(0x2D, 0x2F, 0, 0x40, 8, U16(PX() + 0x30));
        U8(o + 0x57)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        break;
    case 1:
        if (Anim_StepLayers(U16(PX() + 0x30))) {
            U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0; U16(PX() + 0x38) = 0x200;
            Anim_Start(o, 0x16, 0, 9, 4);
            U32(o + 0x54) = 0x10001;
            U8(o + 0x11B) = 0;
            return;
        }
        break;
    }
    if (dodge_by(o, heavy & HEAVY)) return;
    u8 pr = PRESSED;
    u32 h = HELD;
    if (sub_weapon(o, 1)) return;
    if (pr & 2) { U8(o + 0x11B) = 0; U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    if (h & 0x1000) { U8(o + 0x11B) = 0; U16(PX() + 0x3C) = wait; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U8(o + 0x11B) = 0; U16(PX() + 0x3C) = wait; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x60501; return; }
    if ((h & 8) && (U8(G() + 0x10B4) & 0xF)) { U8(o + 0x11B) = 0; U32(o + 0x54) = 0x501; }
}

void DC2CALL Gun_Lower(gptr o)     { lower(o, 0xD, 0); }            /* 0x4AABD0 */
void DC2CALL Shotgun_Lower(gptr o) { lower(o, 7, 0); }              /* 0x4AC550 */
void DC2CALL MGun_Lower(gptr o)    { lower(o, 0xFFFD, 0); }          /* 0x4AE0F0 */

/* substate 3: aim at the next target in the object list */
void DC2CALL Gun_NextTarget(gptr o)                                /* 0x4AE9E0 */
{
    if (U8(o + 0x57) == 0) {
        U8(o + 0x11B) = 0x11;
        U16(PX() + 0x32) = U16(PX() + 0x30);
        gptr t = U32(G() + 0x940) + U16(PX() + 0x2A) * 0x260;
        U32(PX() + 0x14) = 0;
        if (!Flag_Check(2, 0xE) && U32(0x87F940) > 1)
            for (u32 j = 1; ; ) {
                U16(PX() + 0x2A)++;
                t += 0x260;
                if (t >= U32(G() + 0x940) + U32(0x87F940) * 0x260) {
                    U16(PX() + 0x2A) = 1;
                    t = U32(G() + 0x940) + 0x260;
                }
                if ((U32(t + 0x50) & 8) && !(U32(t + 0x50) & 0x10000)) { U32(PX() + 0x14) = t; break; }
                if (++j >= U32(0x87F940)) break;
            }
        if (U32(PX() + 0x14)) {
            u32 a = look_at(o, t, 0);
            U16(PX() + 0x20) = (u16)(a >> 16);
            U16(PX() + 0x30) = pitch_of(a);
            Anim_StartLayers(0x39, 0x37, 0, 0, 0, U16(PX() + 0x32));
        } else U16(PX() + 0x20) = U16(o + 0x4A);
        U8(o + 0x57)++;
        return;
    }
    if (U8(o + 0x57) != 1) return;
    u32 s = Angle_Step(U16(PX() + 0x32), 0x180, U16(PX() + 0x30)) & 0xFFFF;
    u32 d = Angle_Approach(U16(o + 0x4A), 0x104, U16(PX() + 0x20)) & 0xFFFF;
    if (!d && !s) U32(o + 0x54) = 0x10501;
    else { U16(PX() + 0x32) += (u16)s; U16(o + 0x4A) += (u16)d; }
    Anim_StepLayers(U16(PX() + 0x32));
}

/* substate 4: turn back while aiming */
void DC2CALL Gun_TurnBack(gptr o)                                  /* 0x4B27C0 */
{
    if (U8(o + 0x57) == 0) {
        Anim_StartLayers(0x39, 0x37, 0, 0, 6, U16(PX() + 0x30));
        U8(o + 0x11B) = 0x11;
        U16(o + 0xF4) = 0;
        U8(o + 0x57)++;
        U16(PX() + 0x22) = U16(o + 0x4A);
        U16(PX() + 0x20) = (u16)(U16(o + 0x4A) + 0x800);
        return;
    }
    if (U8(o + 0x57) != 1) return;
    Anim_StepLayers(U16(PX() + 0x30));
    U16(PX() + 0x22) += 0x12C;
    U16(o + 0x4A) = U16(PX() + 0x22) & 0xFFF;
    if (S16(PX() + 0x22) >= S16(PX() + 0x20)) {
        U16(o + 0x4A) = U16(PX() + 0x20);
        U32(o + 0x54) = 0x10501;
        Se_Play(o, 0x50);
    }
}

/* substate 5: walk while aiming, firing on the way */
static void walk_aim(gptr o, void (*shoot)(gptr), int stride, u32 mode)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x23, 2, mode, 6);
        U32(o + 0xF0) = 0;
        if (stride) U16(o + 0xF4) = 0x73;
        U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1: Anim_Step(o); U16(o + 0xF4) = 0x73; break;
    }
    if (dodge(o)) return;
    if (HELD & 0x2000) U16(o + 0x4A) += 0x3C;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x3C;
    u8 pr = PRESSED;
    u32 h = HELD;
    if (sub_weapon(o, 0)) return;
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 0x1000)) { U32(o + 0x54) = 0xFF000501; return; }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10201;
        Anim_Start(o, 0x1D, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x82;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(1)) {
        if (!U8(PX() + 0x2F)) return;
        shoot(o);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

/* substate 6: walk backwards while aiming */
static void back_aim(gptr o, void (*shoot)(gptr))
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x24, 5, 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFD2; U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1: Anim_Step(o); break;
    }
    if (HELD & 0x2000) U16(o + 0x4A) += 0x20;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x20;
    if (dodge(o)) return;
    u32 pr = U32(0x87DC94), h = HELD;
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if (pr & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (!(h & 0x4000)) { U32(o + 0x54) = 0xFF000501; return; }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10401;
        Anim_Start(o, 0x20, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFD8;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(1)) {
        if (!U8(PX() + 0x2F)) return;
        shoot(o);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

/* substate 7: turn in place while aiming (step: turning speed with the aim held) */
static void turn_aim(gptr o, u16 step, int heavy)
{
    switch (U8(o + 0x57)) {
    case 0:
        U16(PX() + 0x30) = 0x800;
        Anim_StartLayers(0x38, 0x38, 0, 0, 6, U16(PX() + 0x30));
        U16(o + 0xF4) = 0; U8(o + 0x11B) = 0x11;
        U8(o + 0x57)++;
        if (HELD & 0x2000) U16(o + 0x4A) += 0x1E;
        if (HELD & 0x8000) U16(o + 0x4A) -= 0x1E;
        U8(PX() + 0x2E) = 1;
        break;
    case 1:
        if (Anim_StepLayers(U16(PX() + 0x30))) {
            if (U8(PX() + 0x2E)) Anim_StartLayers(0x38, 0x38, 0, 0, 6, U16(PX() + 0x30));
        } else if (U16(o + 0x92) == 0xA) Se_Play(o, 0x50);     /* footstep */
        break;
    }
    if (dodge_by(o, heavy & HEAVY)) return;
    u32 h = HELD;
    if (sub_weapon(o, 0)) return;
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) { U32(o + 0x54) = 0x20501; return; }
    if (h & 0x20) {
        if (Ammo_Check(heavy & BARE ? 0 : 1)) {
            if (U8(PX() + 0x2F)) {
                U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
                U8(o + 0x11B) = 0;
                U32(o + 0x54) = 0x601;
            }
        } else if (PRESSED & 0x20) Se_Play(o, 0x11);
        else U8(PX() + 0x2E) = 0;
    } else if (!(HELD & 0xA000)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10501;
        return;
    }
    if (!(HELD & 0xA000)) return;
    U8(PX() + 0x2E) = 1;
    if (HELD & 0x2000) U16(o + 0x4A) += step;
    if (HELD & 0x8000) U16(o + 0x4A) -= step;
}

/* ---- the shots of the walking substates ---------------------------------------------- */
static void shoot_walk(gptr o) { fire(o, 0x67A950, 0); }

static void shoot_back(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 1);
    Effect_Attach(o, 0x67AAE4);
    Flash_Start(o, 8);
    Flash_StartFor(o, 5, 0x46);
    Ammo_Use(1);
    Se_Play(o, 0x10);
}

#define PROBE2 0x75BDA8u    /* stand-in effect slot for the spray's wall test */

/* weapon 4's spray: an effect 1000 along the gun, a different one when a
 * wall is between */
static void spray_effect(gptr p)
{
    gptr sv = Tmp_Push(8);
    U16(sv) = 0; U16(sv + 2) = 0x3E8; U16(sv + 4) = 0;
    ApplyMatrixSV(p + 0x20, sv, sv);
    U16(sv) += U16(p + 0x40);
    U16(sv + 2) += U16(p + 0x42);
    U16(sv + 4) += U16(p + 0x44);
    memset(MEM(PROBE2), 0, 0x2B * 4);
    U32(PROBE2 + 0x50) = 1;
    U16(PROBE2 + 0x5A) = (u16)(S16(sv + 2) / -1000);
    U8(PROBE2 + 0x85) |= 8;
    U32(PROBE2 + 0x40) = U32(sv); U16(PROBE2 + 0x44) = U16(sv + 4);
    U32(PROBE2 + 0x74) = U32(p + 0x40); U16(PROBE2 + 0x78) = U16(p + 0x44);
    Tmp_Pop(8);
    Effect_Spawn(p, Eff_WallHit(PROBE2) ? 0x67B358 : 0x67B19C, 0, 0, 0x200, 0);
}

static void shoot_spray(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Ammo_Use(1);
    Flash_Start(o, 0xA);
    Flash_StartFor(o, 0xA, 0x78);
    spray_effect(p);
    Se_Play(o, 0x10);
}

/* weapon 2 (the shotgun): a wider hit, a smaller flash */
static void shotgun_hit(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 2);
}

static void shotgun_flash(gptr o)
{
    Flash_Start(o, 3);
    Flash_StartFor(o, 5, 0x32);
}

static void shoot_shotgun_walk(gptr o)
{
    shotgun_hit(o);
    shotgun_flash(o);
    Effect_Attach(o, 0x67942C);
    Ammo_Use(1);
    Se_Play(o, 0x10);
}

static void shoot_shotgun_back(gptr o)
{
    shotgun_hit(o);
    Effect_Attach(o, 0x67942C);
    shotgun_flash(o);
    Ammo_Use(1);
    Se_Play(o, 0x10);
}

void DC2CALL Gun_Walk(gptr o)   { walk_aim(o, shoot_walk, 0, 1); }/* 0x4AB640 */
void DC2CALL Gun_Back(gptr o)   { back_aim(o, shoot_back); }      /* 0x4AB870 */
void DC2CALL Gun_Turn(gptr o)   { turn_aim(o, 0x64, 0); }          /* 0x4ABAA0 */
void DC2CALL Shotgun_Walk(gptr o) { walk_aim(o, shoot_shotgun_walk, 1, 1); }  /* 0x4ABF00 */
void DC2CALL Shotgun_Back(gptr o) { back_aim(o, shoot_shotgun_back); }     /* 0x4AC750 */
void DC2CALL Spray_Walk(gptr o) { walk_aim(o, shoot_spray, 0, 1); }/* 0x4AD9B0 */
void DC2CALL Spray_Back(gptr o) { back_aim(o, shoot_spray); }     /* 0x4AD370 */
void DC2CALL Rifle_Turn(gptr o) { turn_aim(o, 0x46, 0); }          /* 0x4B0220 */
void DC2CALL Bare_Turn(gptr o)  { turn_aim(o, 0x64, BARE); }       /* 0x4AB1E0 */
void DC2CALL Heavy_Turn(gptr o) { turn_aim(o, 0x46, HEAVY); }          /* 0x4B1A50 */

/* ---- the heavier weapons (3, 8, 9: the whole body moves) ----------------------------- */
/* substate 5: walk aiming; the trigger goes to the firing state, or shoots
 * on the way */
struct rifle_walk {
    u16 stride0, stride;        /* starting (0: unchanged) and walking stride */
    u8 heavy, sideways_pressed, stop_clear;   /* heavy: the flags */
    u16 crouch_stride;
    void (*shoot)(gptr);
};

static void rifle_walk(gptr o, const struct rifle_walk *w)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x25, 0, 1, 4);
        U32(o + 0xF0) = 0;
        if (w->stride0) U16(o + 0xF4) = w->stride0;
        U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1: U16(o + 0xF4) = w->stride; Anim_Step(o); break;
    }
    if (HELD & 0x2000) U16(o + 0x4A) += 0x3C;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x3C;
    if (dodge_by(o, w->heavy & HEAVY)) return;
    u32 pr = U32(0x87DC94), h = HELD;
    if (sub_weapon(o, 0)) return;
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if ((w->sideways_pressed ? pr : h) & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 0x1000)) {
        if (w->stop_clear) U16(o + 0xF4) = 0;
        U32(o + 0x54) = 0xFF000501;
        return;
    }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10201;
        Anim_Start(o, 0x1D, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = w->crouch_stride;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(w->heavy & BARE ? 0 : 1)) {
        if (!U8(PX() + 0x2F)) return;
        if (w->shoot) w->shoot(o); else U32(o + 0x54) = 0x601;
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

void DC2CALL Rifle_Walk(gptr o)                                    /* 0x4B0AE0 */
{
    static const struct rifle_walk w = {0x35, 0x36, 0, 0, 1, 0x82, 0};
    rifle_walk(o, &w);
}

/* substate 6: walk back aiming; the trigger goes to the firing state, or
 * shoots on the way */
static void rifle_back(gptr o, void (*shoot)(gptr), int heavy)
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x24, 5, 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFD2; U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1: Anim_Step(o); break;
    }
    if (HELD & 0x2000) U16(o + 0x4A) += 0x20;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x20;
    if (dodge_by(o, heavy & HEAVY)) return;
    u32 pr = U32(0x87DC94), h = HELD;
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if (pr & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (!(h & 0x4000)) { U32(o + 0x54) = 0xFF000501; return; }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10401;
        Anim_Start(o, 0x20, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0xFFD8;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(heavy & BARE ? 0 : 1)) {
        if (!U8(PX() + 0x2F)) return;
        if (shoot) shoot(o); else U32(o + 0x54) = 0x601;
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

void DC2CALL Rifle_Back(gptr o) { rifle_back(o, 0, 0); }           /* 0x4B0CE0 */

/* ---- weapon 0 (checks for 0 rounds; the back walk shoots weapon 0's hit) ---------- */
void DC2CALL Bare_Walk(gptr o)                                     /* 0x4AADD0 */
{
    static const struct rifle_walk w = {0x35, 0x36, BARE, 0, 1, 0x82, 0};
    rifle_walk(o, &w);
}

static void bare_back_shot(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 0);
    Ammo_Use(0);
    Se_Play(o, 0x10);
}

void DC2CALL Bare_Back(gptr o) { rifle_back(o, bare_back_shot, BARE); }      /* 0x4AAFD0 */

/* ---- weapons 10 and 11 (heavy: the dodge button crouches) --------------------------- */
void DC2CALL Heavy10_Ready(gptr o) { ready(o, 4, HEAVY); }             /* 0x4B12C0 */
void DC2CALL Heavy10_Lower(gptr o) { lower(o, 4, HEAVY); }             /* 0x4B1690 */
void DC2CALL Heavy11_Ready(gptr o) { ready(o, 0xA, HEAVY); }           /* 0x4B1FD0 */
void DC2CALL Heavy11_Lower(gptr o) { lower(o, 0xA, HEAVY); }           /* 0x4B2600 */

/* weapon 10 uses no ammunition */
static void heavy10_hit(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 0xA);
}

static void heavy10_flash(gptr o)
{
    Flash_Start(o, 5);
    Flash_StartFor(o, 5, 0x3C);
}

static void heavy10_walk_shot(gptr o)
{
    heavy10_hit(o);
    heavy10_flash(o);
    Effect_Attach(o, 0x677920);
    Se_Play(o, 0x10);
}

static void heavy10_back_shot(gptr o)
{
    heavy10_hit(o);
    Effect_Attach(o, 0x677920);
    heavy10_flash(o);
    Se_Play(o, 0x10);
}

void DC2CALL Heavy10_Walk(gptr o)                                  /* 0x4B10C0 */
{
    static const struct rifle_walk w = {0, 0x35, HEAVY, 1, 0, 0x60, heavy10_walk_shot};
    rifle_walk(o, &w);
}

void DC2CALL Heavy11_Walk(gptr o)                                  /* 0x4B2890 */
{
    static const struct rifle_walk w = {0x35, 0x36, HEAVY, 0, 1, 0x82, 0};
    rifle_walk(o, &w);
}

void DC2CALL Heavy10_Back(gptr o) { rifle_back(o, heavy10_back_shot, HEAVY); }   /* 0x4B1850 */
void DC2CALL Heavy11_Back(gptr o) { rifle_back(o, 0, HEAVY); }                   /* 0x4B2A60 */

/* ---- firing: the follow-up once the shot's animation ends ---------------------------- */
/* layered (upper body) or whole-body animation. Back to holding the aim
 * (0x10501): mode 0 plainly; mode 1 restarting the hold animation, when
 * out of ammunition plainly; mode 2 always restarting it */
static void after_shot(gptr o, int layers, int mode)
{
    if (!(layers ? Anim_StepLayers(U16(PX() + 0x30)) : Anim_Step(o))) {
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
        return;
    }
    U8(PX() + 0x2F) = 1;
    u32 h = HELD;
    if ((h & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (h & 2) { U32(o + 0x54) = 0x40501; return; }
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) { U32(o + 0x54) = 0x20501; return; }
    if (h & 0x20) {
        if (Ammo_Check(1)) { U32(o + 0x54) = 0x601; return; }
        if (mode < 2) { U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501; return; }
    } else {
        if ((PRESSED & 4) && !Flag_Check(2, 0xE)) { U32(o + 0x54) = 0x30501; return; }
        if (HELD & 0xA000) { U32(o + 0x54) = 0x70501; return; }
    }
    if (!mode) { U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501; return; }
    U32(o + 0x54) = 0x10501;
    U16(PX() + 0x30) = 0x800;
    Anim_StartLayers(0x35, 0x35, 0, 0, 0xA, U16(PX() + 0x30));
}

/* weapon 2: the shotgun */
void DC2CALL Shotgun_Fire(gptr o)                                  /* 0x4ABD50 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 1, 0); return; }
    if (U8(o + 0x56)) return;
    Anim_StartLayers(0x2A, 0x2C, 0, 0, 0, U16(PX() + 0x30));
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x56)++;
    Ammo_Use(1);
    shotgun_flash(o);
    Effect_Attach(o, 0x6792C0);
    Se_Play(o, 0x10);
    shotgun_hit(o);
}

/* ---- weapon 5: the machine gun (fires while the trigger is held, light on) ---------- */
static void mgun_light(void) { U8(U32(G() + 0x948) + 0x21) = 1; }

static void mgun_shot(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 5);
}

/* firing (normal substate 6): a burst while the trigger is held, turning slowly */
void DC2CALL MGun_Fire(gptr o)                                     /* 0x4AE360 */
{
    if (U8(o + 0x56) == 0) {
        Anim_StartLayers(0x2A, 0x2C, 0, 0, 4, U16(PX() + 0x30));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        Ammo_Use(1);
        Flash_Start(o, 2);
        Flash_StartFor(o, 0x14, 0x5A);
        mgun_light();
        return;
    }
    if (U8(o + 0x56) != 1) return;
    U8(o + 0x11B) = 0;
    if (HELD & 0x2000) { U16(o + 0x4A) += 0x32; U8(o + 0x11B) = 0x11; }
    if (HELD & 0x8000) { U16(o + 0x4A) -= 0x32; U8(o + 0x11B) = 0x11; }
    u32 h = HELD;
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) {
        Anim_StepLayers(U16(PX() + 0x30));
        U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x20501;
        return;
    }
    if (PRESSED & 2) { U32(o + 0x54) = 0x40501; return; }
    if (h & 0x20) {
        if (Ammo_Check(1)) {
            s16 n = S16(PX() + 0x3C);
            U16(PX() + 0x3C) = (u16)(n + 1);
            if (n > 2) U8(PX() + 0x2F) = 1;
            if (U16(o + 0x92) >= 0x18) Anim_StartLayers(0x2A, 0x2C, 7, 0, 0, U16(PX() + 0x30));
            else Anim_StepLayers(U16(PX() + 0x30));
            if (!U8(PX() + 0x2F)) return;
            mgun_shot(o);
            Flash_Start(o, 2);
            Ammo_Use(1);
            U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
            mgun_light();
            return;
        }
        if (PRESSED & 0x20) { Se_Play(o, 0x11); return; }
    }
    if (!Anim_StepLayers(U16(PX() + 0x30))) return;
    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501;
}

/* the walking substates shoot on their own */
static void mgun_walk_shot(gptr o)
{
    mgun_shot(o);
    Ammo_Use(1);
    Flash_Start(o, 2);
    mgun_light();
}

/* substate 5: walk aiming, shooting on the way (turn: turning speed;
 * stride_first: the stride is set before the animation steps) */
static void heavy_walk(gptr o, u16 turn, int stride_first, void (*shoot)(gptr))
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x25, 0, 1, 4);
        U32(o + 0xF0) = 0; U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1:
        if (stride_first) U16(o + 0xF4) = 0x35;
        Anim_Step(o);
        if (!stride_first) U16(o + 0xF4) = 0x35;
        break;
    }
    if (dodge(o)) return;
    if (HELD & 0x2000) U16(o + 0x4A) += turn;
    if (HELD & 0x8000) U16(o + 0x4A) -= turn;
    u32 pr = U32(0x87DC94), h = HELD;
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if (pr & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 0x1000)) { U32(o + 0x54) = 0xFF000501; return; }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10201;
        Anim_Start(o, 0x1D, U16(o + 0x92), 1, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0x82;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(1)) {
        if (!U8(PX() + 0x2F)) return;
        shoot(o);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

void DC2CALL MGun_Walk(gptr o) { heavy_walk(o, 0x20, 0, mgun_walk_shot); }   /* 0x4AE5A0 */

/* substate 6: walk back aiming */
void DC2CALL MGun_Back(gptr o)                                     /* 0x4AE7D0 */
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x24, 4, 1, 2);
        U32(o + 0xF0) = 0; U8(o + 0x11B) = 0;
        U16(PX() + 0x30) = 0x800;
        U8(o + 0x57)++;
        break;
    case 1: Anim_Step(o); U16(o + 0xF4) = 0xFFD2; break;
    }
    if (dodge(o)) return;
    if (HELD & 0x2000) U16(o + 0x4A) += 0x20;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x20;
    u32 pr = U32(0x87DC94), h = HELD;
    if ((pr & 0x10) && (U16(G() + 0x10B4) & 0xFF00)) {
        if (!Player_SubDone()) U32(o + 0x54) = 0xA01;
        return;
    }
    if (pr & 2) { U32(o + 0x54) = 0x40501; return; }
    if (pr & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (!(h & 0x4000)) { U32(o + 0x54) = 0xFF000501; return; }
    if (!(h & 8)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x401;
        U32(o + 0x1DC) = (u32)U16(o + 0x92) << 16 | 2;
        return;
    }
    if (!(h & 0x20)) return;
    if (Ammo_Check(1)) {
        if (!U8(PX() + 0x2F)) return;
        mgun_walk_shot(o);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Se_Play(o, 0x11);
}

/* ---- weapons 6 (twin guns: shots from both hands, parts 3 and 6) and 7 ------------- */
void DC2CALL Twin_Ready(gptr o) { ready(o, 0xFFFC, 0); }             /* 0x4AF7B0 */
void DC2CALL Twin_Lower(gptr o) { lower(o, 0xFFFC, 0); }             /* 0x4AFDF0 */

/* frames since the last shot, counted on entry by both firing handlers */
static void twin_count(void)
{
    s16 n = S16(PX() + 0x3C);
    U16(PX() + 0x3C) = (u16)(n + 1);
    if (n > 1) U8(PX() + 0x2F) = 1;
}

static void twin_hits(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 6);
    p = Obj_Part(U32(o + 0x64), 6);
    Weapon_Hit(p + 0x20, p + 0x40, 6);
}

/* the muzzle effects take turns (counter 0x75BE54) */
static void twin_muzzle(gptr o)
{
    u8 c = U8(0x75BE54) & 3;
    U8(0x75BE54) = c;
    Effect_Attach(o, c == 0 ? 0x67A2C8 : c == 2 ? 0x67A410 : 0x67A394);
    U8(0x75BE54) = (u8)(c + 1);
}

static void twin_walk_shot(gptr o)
{
    twin_hits(o);
    Ammo_Use(1);
    Se_Play(o, 0x10);
    Flash_Start(o, 2);
    twin_muzzle(o);
}

static void twin_back_shot(gptr o)
{
    twin_hits(o);
    twin_muzzle(o);
    Flash_Start(o, 2);
    Ammo_Use(1);
    Se_Play(o, 0x10);
}

/* firing: the guns alternate while the trigger is held */
void DC2CALL Twin_Fire(gptr o)                                     /* 0x4AEFA0 */
{
    twin_count();
    if (U8(o + 0x56) == 0) {
        Anim_StartLayers(0x2B, 0x2B, 0, 0, 4, U16(PX() + 0x30));
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x56)++;
        Ammo_Use(1);
        Effect_Attach(o, 0x67A2C8);
        U8(0x75BE54) = 1;
        Flash_Start(o, 2);
        twin_hits(o);
        Se_Play(o, 0x10);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
        return;
    }
    if (U8(o + 0x56) != 1) return;
    Anim_StepLayers(U16(PX() + 0x30));
    U8(o + 0x11B) = 0;
    if (HELD & 0x2000) { U16(o + 0x4A) += 0x3C; U8(o + 0x11B) = 0x11; }
    if (HELD & 0x8000) { U16(o + 0x4A) -= 0x3C; U8(o + 0x11B) = 0x11; }
    u32 h = HELD;
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) {
        Anim_StartLayers(0x2A, 0x2C, 0, 0, 0xF, U16(PX() + 0x30));
        U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x20501;
        return;
    }
    if (h & 0x20) {
        if (U16(o + 0x92) >= 3) Anim_StartLayers(0x2B, 0x2B, 0, 0, 0, U16(PX() + 0x30));
        if (!Ammo_Check(1) || !U8(PX() + 0x2F)) return;
        twin_hits(o);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
        Ammo_Use(1);
        Flash_Start(o, 2);
        Se_Play(o, 0x10);
        twin_muzzle(o);
        if (Ammo_Check(1)) return;
    }
    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501;
}

void DC2CALL Twin_Walk(gptr o) { walk_aim(o, twin_walk_shot, 1, 0x101); }   /* 0x4AF260 */
void DC2CALL Twin_Back(gptr o) { back_aim(o, twin_back_shot); }             /* 0x4AEC70 */

/* weapon 7: one shot with two effects */
static void w7_hit(gptr o)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 7);
}

static void w7_shot(gptr o)
{
    w7_hit(o);
    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    Ammo_Use(1);
    Flash_Start(o, 2);
    Se_Play(o, 0x10);
    Effect_Attach(o, 0x67C290);
    Effect_Attach(o, 0x67C264);
}

static void w7_back_shot(gptr o)
{
    w7_hit(o);
    Effect_Attach(o, 0x67C290);
    Effect_Attach(o, 0x67C264);
    Flash_Start(o, 2);
    Ammo_Use(1);
    Se_Play(o, 0x10);
}

void DC2CALL Weapon7_Fire(gptr o)                                  /* 0x4AF540 */
{
    twin_count();
    if (U8(o + 0x56) == 0) {
        Anim_StartLayers(0x2A, 0x2C, 0, 0, 3, U16(PX() + 0x30));
        U8(o + 0x56)++;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        w7_shot(o);
        return;
    }
    if (U8(o + 0x56) != 1) return;
    Anim_StepLayers(U16(PX() + 0x30));
    U8(o + 0x11B) = 0;
    if (HELD & 0x2000) { U16(o + 0x4A) += 0x3C; U8(o + 0x11B) = 0x11; }
    if (HELD & 0x8000) { U16(o + 0x4A) -= 0x3C; U8(o + 0x11B) = 0x11; }
    u32 h = HELD;
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) {
        Anim_StartLayers(0x2A, 0x2C, 0, 0, 0xF, U16(PX() + 0x30));
        U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x20501;
        return;
    }
    if (PRESSED & 2) { U32(o + 0x54) = 0x40501; return; }
    if (h & 0x20) {
        if (U16(o + 0x92) >= 0xE) Anim_StartLayers(0x2A, 0x2C, 0, 0, 0, U16(PX() + 0x30));
        if (Ammo_Check(1) && U8(PX() + 0x2F)) w7_shot(o);
        if (Ammo_Check(1)) return;
    }
    Anim_StartLayers(0x2A, 0x2C, 0xF, 0, 4, U16(PX() + 0x30));
    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501;
}

static void w7_walk_shot(gptr o)
{
    w7_hit(o);
    Ammo_Use(1);
    Flash_Start(o, 2);
    Se_Play(o, 0x10);
    Effect_Attach(o, 0x67C290);
    Effect_Attach(o, 0x67C264);
}

void DC2CALL Weapon7_Walk(gptr o) { heavy_walk(o, 0x3C, 1, w7_walk_shot); }   /* 0x4AFBC0 */
void DC2CALL Weapon7_Back(gptr o) { rifle_back(o, w7_back_shot, 0); }         /* 0x4AFFF0 */

/* weapon 10 */
void DC2CALL Heavy10_Fire(gptr o)                                  /* 0x4B0F10 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 1, 0); return; }
    if (U8(o + 0x56)) return;
    Anim_StartLayers(0x2A, 0x2C, 0, 0, 0, U16(PX() + 0x30));
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x56)++;
    Ammo_Use(0);
    heavy10_flash(o);
    heavy10_hit(o);
    Effect_Attach(o, 0x677920);
    Se_Play(o, 0x10);
}

/* weapon 11: a shell (object effect e, state 7) along the gun, aimed 2600 ahead */
static void heavy11_shell(gptr o, gptr e)
{
    gptr p = Obj_Part(U32(o + 0x64), 3);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0xA28; U16(v + 4) = 0;
    ApplyMatrixSV(p + 0x20, v, v);
    U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
    Angles_LookAt(p + 0x40, v, v);
    U32(e + 0x50) = 3; U32(e + 0x68) = 0x6789C4; U32(e + 0x54) = 0;
    U8(e + 0x4E) = 3; U8(e + 0x4F) = 7;
    U16(e + 0x48) = (u16)(0x400 - U32(v));
    U16(e + 0x4C) = 0; U16(e + 0x4A) = U16(o + 0x4A);
    Tmp_Pop(8);
    RotMatrixYXZ(e + 0x48, e + 0x20);
    U16(e + 0x40) = U16(p + 0x40);
    U16(e + 0x42) = (u16)(U16(p + 0x42) + 0x1E);
    U16(e + 0x44) = U16(p + 0x44);
    U32(e + 0x58) = U32(e + 0x40); U16(e + 0x5C) = U16(e + 0x44);
    U16(e + 0x60) = 0; U16(e + 0x62) = 0x1F4; U16(e + 0x64) = 0;
    Effect_Attach(o, 0x678B88);
    Effect_Spawn(e, 0x678D48, 0, 0, 0xA, 0);
    Ammo_Use(1);
    Flash_Start(o, 8);
    Flash_StartFor(o, 8, 0x5A);
    Se_Play(o, 0x10);
}

void DC2CALL Heavy11_Fire(gptr o)                                  /* 0x4B1CF0 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 1, 0); return; }
    if (U8(o + 0x56)) return;
    gptr e = Eff_Alloc();
    if (!e) { U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501; Se_Play(o, 0x11); return; }
    Anim_StartLayers(0x2A, 0x2C, 0, 0, 6, U16(PX() + 0x30));
    U8(o + 0x56)++;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    heavy11_shell(o, e);
}

/* ---- substate 9: crouched with a heavy weapon (10, 11), the view lowered --------------
 * Player block: +0x40 offset of the eye (moved along +0x50 rotation), +0x48
 * its target offset, +0x4C sideways lean (+-0x2D0), +0x34 height, +0x7A
 * just crouched, +0x7D turning done (+0x20 yaw wanted, +0x22 before). */
u32 DC2CALL Crouch_Fire(gptr o)                                    /* 0x47FA80 */
{
    U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    if (!Ammo_Check(1)) {
        if (PRESSED & 0x20) Se_Play(o, 0x11);
        return 1;
    }
    Obj_UpdateMatrix(o);
    if ((U8(G() + 0x10B4) & 0xF) == 0xA) {
        gptr p = Obj_Part(U32(o + 0x64), 3);
        Weapon_Hit(p + 0x20, p + 0x40, 0xA);
        Flash_Start(o, 5);
        Flash_StartFor(o, 5, 0x3C);
        Effect_Attach(o, 0x677920);
        Ammo_Use(0);
        Se_Play(o, 0x10);
        return 0;
    }
    gptr e = Eff_Alloc();
    if (e) heavy11_shell(o, e);
    return 0;
}

/* turn toward the wanted yaw; done when there */
static void crouch_turn(gptr o)
{
    u16 d = (u16)Angle_Approach(U16(o + 0x4A), 0x96, U16(PX() + 0x20));
    U16(o + 0x4A) += d;
    if (!d) U8(PX() + 0x7D) = 1;
}

/* the crouch's end (anim 0x3C): stand up, then turn if not done */
static void crouch_rise(gptr o, u32 blend)
{
    Anim_Start(o, 0x3C, 0, 0, blend);
    U8(o + 0x56)++;
    U8(o + 0x17B) = 0;
    U16(PX() + 0x42) = 0;
    if (!U8(PX() + 0x7D)) crouch_turn(o);
}

/* while crouched: turn back (button 2), aim at the nearest target (8),
 * fire, turn, lean (eye: the camera offset +0x40) */
static void crouch_control(gptr o, gptr eye)
{
    u8 pr = PRESSED;
    if (pr & 2) {
        U8(PX() + 0x7D) = 0;
        U16(PX() + 0x22) = U16(o + 0x4A);
        U16(PX() + 0x20) = (u16)(U16(o + 0x4A) + 0x800);
        Effect_Attach(o, 0x6826F4);
        return;
    }
    if (pr & 8) {
        if (Flag_Check(2, 0xE)) U32(PX() + 0x14) = 0; else Target_Find(o, 0x800);
        gptr t = U32(PX() + 0x14);
        if (t) {
            Obj_Part(U32(t + 0x64), U8(U32(t + 0x134) + 0xA));   /* the part is looked up but not used */
            gptr a = Tmp_Push(8);
            Angles_LookAt(t + 0x40, eye, a);
            U8(PX() + 0x7D) = 0;
            U16(PX() + 0x22) = U16(o + 0x4A);
            U16(PX() + 0x20) = U16(a + 2);
            Tmp_Pop(8);
            Effect_Attach(o, 0x6826F4);
            return;
        }
    }
    u32 h = HELD;
    if ((h & 0x20) && (h & 8) && U8(PX() + 0x2F)) { Crouch_Fire(o); h = HELD; }
    if (h & 0x2000) U16(o + 0x4A) += 0x50;
    else if (h & 0x8000) U16(o + 0x4A) -= 0x50;
    if (h & 0x1000) {
        U16(PX() + 0x4C) += 0x20;
        if (S16(PX() + 0x4C) > 0x2D0) U16(PX() + 0x4C) = 0x2D0;
    } else if (h & 0x4000) {
        U16(PX() + 0x4C) -= 0x20;
        if (S16(PX() + 0x4C) < -0x2D0) U16(PX() + 0x4C) = (u16)-0x2D0;
    }
}

/* the eye moves along its rotation; a lean past `lim` restarts it sideways */
static void crouch_eye(gptr o, s16 lim, u16 back, gptr d)
{
    s16 c = S16(PX() + 0x4C);
    if (c >= lim || c <= -lim) {
        U16(PX() + 0x50) = 0; U16(PX() + 0x52) = U16(o + 0x4A); U16(PX() + 0x54) = 0;
        U16(PX() + 0x44) = U16(PX() + 0x4C);
        U16(PX() + 0x48) = 0; U16(PX() + 0x4A) = back; U16(PX() + 0x4C) = 0;
    }
    gptr m = Tmp_Push(0x20);
    RotMatrix(PX() + 0x50, m);
    ApplyMatrixSV(m, PX() + 0x40, d);
    Tmp_Pop(0x20);
}

void DC2CALL Player_Crouch(gptr o)                                  /* 0x47F0F0 */
{
    s16 n = S16(PX() + 0x3C);
    U16(PX() + 0x3C) = (u16)(n + 1);
    if (n >= ((U8(G() + 0x10B4) & 0xF) == 0xA ? 8 : 0xF)) U8(PX() + 0x2F) = 1;
    gptr f = Tmp_Push(0x10), d = f, b = f + 8;
    switch (U8(o + 0x56)) {
    case 0: case 4:                                         /* crouching down (4: again, from standing up) */
        U16(o + 0x5A) = (u16)(S16(o + 0x42) / -1000);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 0x64; U8(o + 0x11B) = 0;
        Flag_Set(1, 9, 1);
        U16(PX() + 0x34) = (u16)(U16(o + 0x42) - 0xA28);
        if (U8(o + 0x56) == 0) {
            Anim_Start(o, 0x3B, 0, 0, 2);
            U16(PX() + 0x40) = 0; U16(PX() + 0x42) = 0xFEC0; U16(PX() + 0x44) = 0;
            if (HELD & 0x1000) U16(PX() + 0x44) = 0x5A;
            if (HELD & 0x4000) U16(PX() + 0x44) = 0xFFA6;
            U16(PX() + 0x48) = 0; U16(PX() + 0x4A) = 0xFEC0; U16(PX() + 0x4C) = 0;
            U8(PX() + 0x7A) = 1;
            Effect_Attach(o, 0x6826F4);
        } else {
            U32(PX() + 0x40) = 0; U16(PX() + 0x44) = 0;
            U32(PX() + 0x48) = 0; U16(PX() + 0x4C) = 0;
        }
        U16(PX() + 0x50) = 0; U16(PX() + 0x52) = U16(o + 0x4A); U16(PX() + 0x54) = 0;
        U8(PX() + 0x7D) = 1;
        if (++U8(o + 0x56) != 5) break;
        U16(PX() + 0x42) += 0xF; U16(PX() + 0x4A) += 0xF;
        if (S16(PX() + 0x42) >= 0) crouch_rise(o, 4);
        break;
    case 1: case 5: {                                       /* going down */
        U8(o + 0x17B) = 0x64;
        crouch_eye(o, 0x50, 0xFEC0, d);
        U16(o + 0x42) += U16(d + 2); U16(o + 0x40) += U16(d); U16(o + 0x44) += U16(d + 4);
        U16(d) = U16(o + 0x40); U16(d + 2) = (u16)(U16(o + 0x42) - 0x7A8); U16(d + 4) = U16(o + 0x44);
        U16(b) = U16(o + 0xE0); U16(b + 2) = (u16)(U16(o + 0xE2) - 0x7A8); U16(b + 4) = U16(o + 0xE4);
        s16 h = (s16)Floor_Height(d, b, 0x80, 0);
        U16(o + 0x5A) = (u16)(h / -1000);
        if (S16(o + 0x5A) == 2 - S16(o + 0x42) / 1000) { crouch_rise(o, 0xA); break; }
        U16(PX() + 0x42) += 0xF; U16(PX() + 0x4A) += 0xF;
        if (S16(PX() + 0x42) >= 0) { crouch_rise(o, 0xA); break; }
        Anim_Step(o);
        if (!U8(PX() + 0x7D)) crouch_turn(o);
        else crouch_control(o, o + 0x40);
        break;
    }
    case 2: case 6: {                                       /* down */
        crouch_eye(o, 0x28, 0xFF56, d);
        U16(o + 0x40) += U16(d); U16(o + 0x42) += U16(d + 2); U16(o + 0x44) += U16(d + 4);
        s16 h = (s16)Floor_Height(o + 0x40, o + 0xE0, 0x80, 0);
        U16(o + 0x5A) = (u16)(h / -1000);
        U16(PX() + 0x42) += 2; U16(PX() + 0x4A) += 2;
        if (S16(o + 0x42) >= S16(o + 0x5A) * -1000) {       /* on the floor: stand up */
            if ((s16)Fn_4542F0(o + 0x40, o + 0xE0, d, 0x80) >= 0) {
                U16(o + 0x40) = U16(d); U16(o + 0x42) = U16(d + 2); U16(o + 0x44) = U16(d + 4);
            }
            Anim_Start(o, 0x3D, 0, 0, 0);
            U16(o + 0x42) = (u16)(U16(o + 0x5A) * 0xFC18);
            U16(PX() + 0x42) = 0; U16(PX() + 0x44) = 0;
            U8(o + 0x56) = 3;
            break;
        }
        Anim_Step(o);
        if ((PRESSED & 0x40) && !Fn_4BF330() && !U8(PX() + 0x7A) && U8(o + 0x56) == 6) {
            Flag_Set(2, 0xD, 0);
            Flag_Set(1, 9, 1);
            U8(o + 0x11B) = 0;
            U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x901;
            break;
        }
        if (!U8(PX() + 0x7D)) crouch_turn(o);
        else crouch_control(o, o + 0x40);
        break;
    }
    case 3:                                                 /* standing up */
        if (!Anim_Step(o)) break;
        U16(o + 0x174) = 0; U8(o + 0x17B) = 0; U8(PX() + 0x7A) = 0;
        if ((PRESSED & 0x40) && !Fn_4BF330()) { U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x901; break; }
        Flag_Set(1, 9, 0);
        U32(o + 0x1DC) = 4; U32(o + 0x54) = 1;
        break;
    }
    Tmp_Pop(0x10);
}

/* weapon 3: a grenade (object effect, state 5) */
void DC2CALL Weapon3_Fire(gptr o)                                  /* 0x4AC9E0 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 0, 2); return; }
    if (U8(o + 0x56)) return;
    gptr e = Eff_Alloc();
    if (!e) { U32(o + 0x1DC) = 0; U32(o + 0x54) = 0x10501; Se_Play(o, 0x11); return; }
    Anim_Start(o, 0x2A, 0, 0, 6);
    U8(o + 0x56)++;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    gptr p = Obj_Part(U32(o + 0x64), 3);
    U32(e + 0x50) = 3; U32(e + 0x68) = 0x67967C; U32(e + 0x54) = 0;
    U8(e + 0x4E) = 3; U8(e + 0x4F) = 5;
    U16(e + 0x48) = 0x400; U16(e + 0x4A) = U16(o + 0x4A); U16(e + 0x4C) = 0;
    RotMatrixYXZ(e + 0x48, e + 0x20);
    gptr sv = Tmp_Push(0x28), mm = sv + 8;
    RotMatrix(o + 0x48, mm);
    U16(sv) = 0xFFDE; U16(sv + 2) = 0xFF4A; U16(sv + 4) = 0xFE89;     /* the muzzle */
    ApplyMatrixSV(mm, sv, sv);
    U16(e + 0x40) = (u16)(U16(p + 0x40) + U16(sv));
    U16(e + 0x42) = (u16)(U16(p + 0x42) + U16(sv + 2));
    U16(e + 0x44) = (u16)(U16(p + 0x44) + U16(sv + 4));
    Tmp_Pop(0x28);
    U32(e + 0x58) = U32(e + 0x40); U16(e + 0x5C) = U16(e + 0x44);
    U16(e + 0x60) = 0; U16(e + 0x62) = 0x2BC; U16(e + 0x64) = 0;
    Effect_Spawn(e, 0x679F9C, 0, 0, 2, 0);
    Effect_Spawn(e, 0x679FC8, 0, 0, 0xA, 0);
    Effect_Attach(o, 0x679A1C);
    Ammo_Use(1);
    Flash_Start(o, 0xF);
    Flash_StartFor(o, 0xF, 0xAA);
    Se_Play(o, 0x10);
}

/* weapon 4: the spray */
void DC2CALL Weapon4_Fire(gptr o)                                  /* 0x4AD6E0 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 1, 1); return; }
    if (U8(o + 0x56)) return;
    Anim_StartLayers(0x2A, 0x2C, 0, 0, 6, U16(PX() + 0x30));
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    U8(o + 0x56)++;
    Ammo_Use(1);
    Flash_Start(o, 0xA);
    Flash_StartFor(o, 0xA, 0x78);
    spray_effect(Obj_Part(U32(o + 0x64), 3));
    Se_Play(o, 0x10);
}

/* weapon 8: three homing missiles (object effects, state 6) at the targets
 * found, the muzzles 0x7047BC */
void DC2CALL Weapon8_Fire(gptr o)                                  /* 0x4B04D0 */
{
    if (U8(o + 0x56) == 1) { after_shot(o, 0, 2); return; }
    if (U8(o + 0x56)) return;
    Anim_Start(o, 0x2A, 0, 0, 6);
    U8(o + 0x56)++;
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
    Target_Find(o, 0x800);
    gptr p = Obj_Part(U32(o + 0x64), 3);
    gptr sv = Tmp_Push(0x28), mm = sv + 8;
    for (u32 j = 0; j < 3; j++) {
        gptr e = Eff_Alloc();
        if (!e) continue;
        U32(e + 0x54) = 0; U32(e + 0x50) = 3; U32(e + 0x68) = 0x679AF0;
        U8(e + 0x4E) = 3; U8(e + 0x4F) = 6;
        U16(e + 0x48) = 0x480;
        U16(e + 0x4A) = (u16)((Rand8() & 0xFF) + U16(o + 0x4A));
        U16(e + 0x4C) = 0;
        RotMatrixYXZ(e + 0x48, e + 0x20);
        RotMatrix(o + 0x48, mm);
        gptr m = 0x7047BC + j * 6;
        U16(sv) = U16(m); U16(sv + 2) = U16(m + 2); U16(sv + 4) = U16(m + 4);
        ApplyMatrixSV(mm, sv, sv);
        U16(e + 0x40) = (u16)(U16(p + 0x40) + U16(sv));
        U16(e + 0x42) = (u16)(U16(p + 0x42) + U16(sv + 2));
        U16(e + 0x44) = (u16)(U16(p + 0x44) + U16(sv + 4));
        U32(e + 0x58) = U32(e + 0x40); U16(e + 0x5C) = U16(e + 0x44);
        U16(e + 0x60) = 0; U16(e + 0x62) = 0x1F4; U16(e + 0x64) = 0;
        s8 slot = S8(e + 0x46);
        if (U32(PX() + 0x14)) {                                 /* the j-th target, or the last part of the first */
            gptr t, d;
            if (j < U32(PX() + 4)) {
                t = U32(G() + 0x940) + U8(PX() + 8 + j) * 0x260;
                d = U32(t + 0x134);
            } else {
                t = U32(G() + 0x940) + U8(PX() + 8) * 0x260;
                gptr d0 = U32(t + 0x134);
                s32 last = -1;
                for (gptr c = d0 - 1; ; ) { c += 0xC; last++; if (!(U8(c) & 1)) break; }
                d = last >= (s32)j ? d0 + j * 0xC : d0 + last * 0xC;
            }
            U16(0x87FD80 + slot * 4) = U8(t + 0xFF);
            U16(0x87FD82 + slot * 4) = U8(d + 0xA);
        } else {
            U16(e + 0x48) = (u16)((Rand8() & 0xF) + 0x400);
            U16(0x87FD80 + slot * 4) = 0xFFFF;
            U16(0x87FD82 + slot * 4) = 0;
        }
        U8(e + 0x46) = 0x3C; U8(e + 0x47) = 0x1E;
        Effect_Spawn(e, 0x67A2C4, 0, 0, 2, 0);
        Effect_Spawn(e, 0x67A2F0, 0, 0, 0xA, 0);
    }
    Tmp_Pop(0x28);
    Effect_Attach(o, 0x679DE4);
    Ammo_Use(1);
    Flash_Start(o, 0xF);
    Flash_StartFor(o, 0xF, 0xAA);
    Se_Play(o, 0x10);
}

/* weapon 9 */
void DC2CALL Weapon9_Fire(gptr o)                                  /* 0x4B0910 */
{
    if (U8(o + 0x56) == 1) {
        U16(o + 0xF4) = U16(o + 0x92) == 3 ? 0xFF83 : 0;      /* the recoil */
        after_shot(o, 1, 0);
        return;
    }
    if (U8(o + 0x56)) return;
    Anim_StartLayers(0x2A, 0x2C, 0, 0, 0, U16(PX() + 0x30));
    U32(o + 0xF0) = 0; U16(o + 0xF4) = 0; U8(o + 0x11B) = 0;
    U8(o + 0x56)++;
    Ammo_Use(1);
    Effect_Attach(o, 0x67A498);
    Flash_Start(o, 0xF);
    Flash_StartFor(o, 0xF, 0xA0);
    gptr p = Obj_Part(U32(o + 0x64), 3);
    Weapon_Hit(p + 0x20, p + 0x40, 9);
    Se_Play(o, 0x10);
}
