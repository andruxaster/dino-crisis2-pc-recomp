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
    Fn_48C0B0(o, 0x10);
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
        Fn_48C0B0(o, 0x10);
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

/* substate 0: raise the gun at the nearest target */
void DC2CALL Gun_Ready(gptr o)                                     /* 0x4ACCC0 */
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
    if (dodge(o)) return;
    u32 h = HELD;
    if (!(h & 8)) {
        U8(o + 0x11B) = U8(o + 0x57) == 1 ? 0x11 : 0;
        U32(o + 0x54) = 0x20501;
        return;
    }
    if (sub_weapon(o, 0)) return;
    if (h & 0x1000) { U16(PX() + 0x3C) = 0xD; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U16(PX() + 0x3C) = 0xD; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x60501; return; }
    u32 d = Angle_Approach(U16(o + 0x4A), 0x104, U16(PX() + 0x20)) & 0xFFFF;
    U16(o + 0x4A) += (u16)d;
    if (d || !U8(PX() + 0x2E)) return;
    if (PRESSED & 2) { U32(o + 0x54) = 0x40501; U32(o + 0x54) = 0x10501; return; }   /* overwritten, as in the original */
    if ((U8(0x87DC08) & 0x20) && Ammo_Check(1) && U8(PX() + 0x2F)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x601;
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
        return;
    }
    U32(o + 0x54) = 0x10501;
}

/* substate 1: hold the aim, follow the target's height */
void DC2CALL Gun_Hold(gptr o)                                      /* 0x4AD0D0 */
{
    Target_Find(o, 0x200);
    gptr t = U32(PX() + 0x14);
    u32 want = t ? pitch_of(look_at(o, t, 1)) : 0x800;
    U16(PX() + 0x30) += (u16)Angle_Step(U16(PX() + 0x30), 0x180, want);
    Anim_StepLayers(U16(PX() + 0x30));
    if (dodge(o)) return;
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
            } else if (PRESSED & 0x20) Fn_48C0B0(o, 0x11);       /* empty click */
        }
    }
    if ((PRESSED & 4) && !Flag_Check(2, 0xE)) { U32(o + 0x54) = 0x30501; return; }
    if (HELD & 0xA000) U32(o + 0x54) = 0x70501;
}

/* substate 2: lower the gun, back to standing */
void DC2CALL Gun_Lower(gptr o)                                     /* 0x4AABD0 */
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
    if (dodge(o)) return;
    u8 pr = PRESSED;
    u32 h = HELD;
    if (sub_weapon(o, 1)) return;
    if (pr & 2) { U8(o + 0x11B) = 0; U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x701; return; }
    if (h & 0x1000) { U8(o + 0x11B) = 0; U16(PX() + 0x3C) = 0xD; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U8(o + 0x11B) = 0; U16(PX() + 0x3C) = 0xD; U8(PX() + 0x2F) = 0; U32(o + 0x54) = 0x60501; return; }
    if ((h & 8) && (U8(G() + 0x10B4) & 0xF)) { U8(o + 0x11B) = 0; U32(o + 0x54) = 0x501; }
}

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
        Fn_48C0B0(o, 0x50);
    }
}

/* substate 5: walk while aiming, firing on the way */
void DC2CALL Gun_Walk(gptr o)                                      /* 0x4AB640 */
{
    switch (U8(o + 0x57)) {
    case 0:
        Anim_Start(o, 0x23, 2, 1, 6);
        U32(o + 0xF0) = 0; U8(o + 0x11B) = 0;
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
        fire(o, 0x67A950, 0);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Fn_48C0B0(o, 0x11);
}

/* substate 6: walk backwards while aiming */
void DC2CALL Gun_Back(gptr o)                                      /* 0x4AB870 */
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
        gptr p = Obj_Part(U32(o + 0x64), 3);
        Weapon_Hit(p + 0x20, p + 0x40, 1);
        Effect_Attach(o, 0x67AAE4);
        Flash_Start(o, 8);
        Flash_StartFor(o, 5, 0x46);
        Ammo_Use(1);
        Fn_48C0B0(o, 0x10);
        U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
    } else if (PRESSED & 0x20) Fn_48C0B0(o, 0x11);
}

/* substate 7: turn in place while aiming */
void DC2CALL Gun_Turn(gptr o)                                      /* 0x4ABAA0 */
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
        } else if (U16(o + 0x92) == 0xA) Fn_48C0B0(o, 0x50);     /* footstep */
        break;
    }
    if (dodge(o)) return;
    u32 h = HELD;
    if (sub_weapon(o, 0)) return;
    if (h & 0x1000) { U32(o + 0x54) = 0x50501; return; }
    if (h & 0x4000) { U32(o + 0x54) = 0x60501; return; }
    if (!(h & 8)) { U32(o + 0x54) = 0x20501; return; }
    if (h & 0x20) {
        if (Ammo_Check(1)) {
            if (U8(PX() + 0x2F)) {
                U16(PX() + 0x3C) = 0; U8(PX() + 0x2F) = 0;
                U8(o + 0x11B) = 0;
                U32(o + 0x54) = 0x601;
            }
        } else if (PRESSED & 0x20) Fn_48C0B0(o, 0x11);
        else U8(PX() + 0x2E) = 0;
    } else if (!(HELD & 0xA000)) {
        U8(o + 0x11B) = 0;
        U32(o + 0x54) = 0x10501;
        return;
    }
    if (!(HELD & 0xA000)) return;
    U8(PX() + 0x2E) = 1;
    if (HELD & 0x2000) U16(o + 0x4A) += 0x64;
    if (HELD & 0x8000) U16(o + 0x4A) -= 0x64;
}
