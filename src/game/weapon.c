/* Weapons: set-up of the main weapon (table 0x737FA0 by G+0x10B4 low byte)
 * and of the sub-weapon (0x737FF0 by the high byte). A weapon picks the
 * player's animation table (+0x88); a sub-weapon puts its model(s) into the
 * object-effect slots G+0x93C (0x70 bytes: +0x40 offset, +0x48 rotation,
 * +0x4E 3 = held, +0x4F state, +0x54 part it follows, +0x68 model). */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define PX() U32(0x87FD60)

/* light slot i of G+0x948 (0x34 bytes) reset; all lights 0x748160 cleared */
gptr DC2CALL Light_Reset(u32 i)                                    /* 0x41C970 */
{
    gptr e = U32(G() + 0x948) + (u8)i * 0x34;
    memset(MEM(e), 0, 0x34);
    memset(MEM(0x748160), 0, 0x900);
    U8(e) = 1; U8(e + 1) = 5;
    return e;
}

void DC2CALL Weapon_SetNone(gptr o) { U32(o + 0x88) = 0x665D00; }           /* 0x4B2C20 */

void DC2CALL Weapon_SetGun(gptr o)                                 /* 0x4AC980 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
}

void DC2CALL Weapon_SetLight(gptr o)                               /* 0x4AE2F0 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
    Light_Reset(0);
}

void DC2CALL Weapon_SetTwin(gptr o)                                /* 0x4AEF10 */
{
    U32(o + 0x88) = 0x665D00;
    U8(PX() + 0x2F) = 1;
    U32(PX() + 0x58) = 0; U16(PX() + 0x5C) = 0;
    U32(PX() + 0x60) = 0; U16(PX() + 0x64) = 0;
}

void DC2CALL Weapon_SetHeavy(gptr o)                               /* 0x4B1C90 */
{
    U32(o + 0x88) = 0x665D7C;
    U8(PX() + 0x2F) = 1;
}

/* object-effect slot e as a model carried by part `part` of o */
static void carried(gptr e, gptr o, gptr model, u32 part, u8 state)
{
    memset(MEM(e), 0, 0x70);
    U32(e + 0x50) = 3;
    U32(e + 0x68) = model;
    U32(e + 0x54) = Obj_Part(U32(o + 0x64), part);
    U8(e + 0x4E) = 3;
    U8(e + 0x4F) = state;
}

static void blade(gptr e)
{
    U16(e + 0x40) = 0xFF56; U16(e + 0x42) = 0; U16(e + 0x44) = 0x64;
    U16(e + 0x48) = 0xFC7C; U16(e + 0x4A) = 0xFFEC; U16(e + 0x4C) = 0xA;
}

/* two knives (one in each hand model) */
void DC2CALL Sub_SetKnives(gptr o)                                 /* 0x4B2C30 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C988, 0xC, 0); blade(e);
    e += 0x70;
    carried(e, o, 0x67C878, 0xC, 0); blade(e);
}

void DC2CALL Sub_SetKnife(gptr o)                                  /* 0x4B2FC0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67CA00, 0xC, 1);
    U16(e + 0x40) = 0xFF88; U16(e + 0x42) = 0; U16(e + 0x44) = 0x28;
    U16(e + 0x48) = 0xFC7C; U16(e + 0x4A) = 0xFFEC; U16(e + 0x4C) = 0xA;
}

void DC2CALL Sub_SetGrenade(gptr o)                                /* 0x4B33C0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C508, 0xC, 0);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
}

void DC2CALL Sub_SetMine(gptr o)                                   /* 0x4B3A10 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C50C, 0xC, 0);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF92; U16(e + 0x44) = 0xFFCE; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
    U8(PX() + 0x78) = 0;
}

void DC2CALL Sub_SetKnives2(gptr o)                                /* 0x4B3D00 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C988, 0xC, 0); blade(e); U16(e + 0x5E) = 1;
    e += 0x70;
    carried(e, o, 0x67C878, 0xC, 0); blade(e); U16(e + 0x5E) = 1;
}

/* ---- the sub-weapon in use (normal substate 10: handlers 0x738078 by the
 * sub-weapon) and put back (0x7380A0) ------------------------------------------- */
#define HELD U32(0x87DC08)

/* n uses of the sub-weapon: taken from its item (sub-weapon | 0x10) unless
 * unlimited; counted */
void DC2CALL Sub_AmmoUse(u32 n)                                    /* 0x496350 */
{
    U8(G() + 0x75A) = 0;
    U8(G() + 0x75B) = (U8(0x87DC08) & 8) ? 0x1E : 0x64;
    u8 i = (u8)Item_Find(G() + 0x10BC, 0x14, U8(G() + 0x10B5) | 0x10);
    if (!(U8(G() + 0x1094) & 1)) U16(G() + i * 12 + 0x10C4) -= (u16)n;
    U32(G() + i * 12 + 0x10C0) += n & 0xFFFF;
}

/* an effect (kind 6) at pos */
gptr DC2CALL Effect_SpawnAt(gptr pos, gptr data, u32 unused)       /* 0x48DB80 */
{
    (void)unused;
    gptr k = K2_Alloc();
    if (k) {
        U8(k + 0x58) = 6;
        U32(k + 0xA0) = pos; U32(k + 0x98) = data;
        U32(k + 0xA4) = 1;
        Orig_Call1(U32(U32(0x87F948) + 0x18), k);
    }
    return k;
}

/* a free light from the end of G+0x948 down to the room's first (G+0xA74):
 * cleared, in use */
gptr DC2CALL Light_Alloc(void)                                     /* 0x48D410 */
{
    u32 n = U32(0x87DE14);
    gptr p = U32(G() + 0x948) + n * 0x34 - 0x34;
    for (u32 k = n - U32(G() + 0xA74) - 1; k; k--, p -= 0x34)
        if (!(U8(p) & 1)) { memset(MEM(p), 0, 0x34); U8(p) = 1; return p; }
    return 0;
}

/* the animation over: back to standing; 1 if so */
static int sub_end(gptr o)
{
    if (!Anim_Step(o)) return 0;
    U32(o + 0x1DC) = 8;
    Anim_Start(o, 0x16, U16(PX() + 0x1E), 9, U16(PX() + 0x1C));
    return 1;
}

/* a noise 1200 ahead of o */
static void noise_ahead(gptr o)
{
    gptr f = Tmp_Push(0x28), v = f, m = f + 8;
    RotMatrix(o + 0x48, m);
    U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x4B0;
    ApplyMatrixSV(m, v, v);
    U16(v) += U16(o + 0x40); U16(v + 4) += U16(o + 0x44); U16(v + 2) += U16(o + 0x42);
    u32 x = U32(v), y = UU32(v + 2), z = U32(v + 4);           /* (y: a dword at +2, as the original reads it) */
    Tmp_Pop(0x28);
    Noise_Make(x, y, z);
}

/* the blades (sub-weapons 1, 2, 7, 8): swung through frames 0..7 (hit from
 * part 6, turned 90 degrees), put back from frame 20 on when walking; kind
 * B (2, 8) is one blade at its own offset, its slot +0x50 bit 0x8000 set
 * while swinging */
struct blade_sub {
    gptr anim; u8 hit, b;
    gptr swing, back;           /* models: B swinging (frames 0..7), B put back */
};

static void blade_offset(gptr e, int b)
{
    if (b) { U16(e + 0x40) = 0xFF88; U16(e + 0x42) = 0; U16(e + 0x44) = 0x28; }
    else { U16(e + 0x40) = 0xFF56; U16(e + 0x42) = 0; U16(e + 0x44) = 0x64; }
    U16(e + 0x48) = 0xFC7C; U16(e + 0x4A) = 0xFFEC; U16(e + 0x4C) = 0xA;
}

static void blade_sub(gptr o, const struct blade_sub *s)
{
    gptr e = U32(G() + 0x93C);
    switch (U8(o + 0x56)) {
    case 0:
        Anim_Start(o, s->anim, 0, 0, 2);
        if (s->b) U32(e + 0x50) |= 0x8000;
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 0xA;
        U32(o + 0x50) |= 0x100;
        Flag_Set(1, 0xA, 1);
        Sub_AmmoUse(0);
        U8(o + 0x56)++;
        return;
    case 1: break;
    default: return;
    }
    if (sub_end(o)) {
        U32(o + 0x50) &= ~0x100u;
        U8(o + 0x17B) = 0;
        Flag_Set(1, 0xA, 0);
        U16(PX() + 0x38) = 0x200;
        U32(o + 0x54) = 0x10001;
        return;
    }
    u16 f = U16(o + 0x92);
    if ((s32)U16(o + 0x90) - 3 < (s32)f) {                  /* the end: in the hand again */
        if (s->b) U32(e + 0x50) &= ~0x8000u;
        U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
        if (!s->b) U32(e + 0x68) = 0x67C988;
        blade_offset(e, s->b);
    } else if (f) {                                         /* swinging: in the other hand */
        if (!s->b) U32(e + 0x68) = 0x67C510;
        U32(e + 0x54) = Obj_Part(U32(o + 0x64), 6);
        U16(e + 0x40) = 0; U16(e + 0x42) = 0; U16(e + 0x44) = 0;
        U16(e + 0x48) = 0; U16(e + 0x4A) = 0;
    }
    f = U16(o + 0x92);
    if (f < 8) {
        if (s->b) U32(e + 0x68) = s->swing;
        Obj_UpdateMatrix(o);
        gptr p = Obj_Part(U32(o + 0x64), 6);
        gptr t = Tmp_Push(0x28), a = t, m = t + 8;
        U16(a) = 0; U16(a + 2) = 0x400; U16(a + 4) = 0;
        RotMatrixYXZ(a, m);
        MulMatrix0(p + 0x20, m, m);
        Weapon_Hit(m, p + 0x40, s->hit);
        Tmp_Pop(0x28);
    } else if (f > 0x13) {
        if (HELD & 0x1000) {                                /* walking: put back */
            Flag_Set(1, 0xA, 0);
            U8(o + 0x17B) = 0; U8(o + 0x11B) = 0;
            U32(o + 0x1DC) = 4; U32(o + 0x54) = 0x201;
            if (s->b) { U32(e + 0x50) &= ~0x8000u; U32(e + 0x68) = s->back; }
            U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
            if (!s->b) U32(e + 0x68) = 0x67C988;
            blade_offset(e, s->b);
        }
    } else if (s->b && f > 0xD) {
        U32(e + 0x68) = s->back;
        U32(e + 0x50) &= ~0x8000u;
    }
    if (U16(o + 0x92) == 5) noise_ahead(o);
}

void DC2CALL Sub1_Use(gptr o)                                      /* 0x4B2CF0 */
{
    static const struct blade_sub s = {0x67CB68, 0x10, 0, 0, 0};
    blade_sub(o, &s);
}

void DC2CALL Sub2_Use(gptr o)                                      /* 0x4B3030 */
{
    static const struct blade_sub s = {0x67CEF4, 0x11, 1, 0x67C50C, 0x67CA00};
    blade_sub(o, &s);
}

void DC2CALL Sub7_Use(gptr o)                                      /* 0x4B3DD0 */
{
    static const struct blade_sub s = {0x67CB68, 0x1D, 0, 0, 0};
    blade_sub(o, &s);
}

void DC2CALL Sub8_Use(gptr o)                                      /* 0x4B41F0 */
{
    static const struct blade_sub s = {0x67CEF4, 0x1E, 1, 0x67CA00, 0x67C50C};
    if (U8(o + 0x56) == 0) U16(U32(G() + 0x93C) + 0x5E) = 1;
    blade_sub(o, &s);
}

/* sub-weapon 5: a kick forward (frames 10..18), hitting at frame 10 */
void DC2CALL Sub5_Use(gptr o)                                      /* 0x4B38E0 */
{
    if (U8(o + 0x56) == 0) {
        Anim_Start(o, 0x67C500, 7, 0, 4);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 5; U8(o + 0x11B) = 0;
        Se_Play(o, 0x20);
        Sub_AmmoUse(0);
        U8(o + 0x56)++;
        return;
    }
    if (U8(o + 0x56) != 1) return;
    if (sub_end(o)) {
        U32(o + 0x54) = 0x10001;
        U16(o + 0xF4) = 0;
        U16(PX() + 0x38) = 0x200;
        return;
    }
    u16 f = U16(o + 0x92);
    if (f <= 0x12 && f >= 0xA) U16(o + 0xF4) += 0x1E;
    if (f >= 0x13) U16(o + 0xF4) = 0;
    if (f != 0xA) return;
    gptr p = Obj_Part(U32(o + 0x64), 5);
    Weapon_Hit(p + 0x20, p + 0x40, 0x12);
    Flash_Start(o, 5);
    Effect_SpawnAt(o, 0x67D1E4, 0);
    U16(o + 0xF4) = 0xFEA2;
}

/* sub-weapon 9: a flare (a light and an effect at frame 7) */
void DC2CALL Sub9_Use(gptr o)                                      /* 0x4B4600 */
{
    gptr e = U32(G() + 0x93C);
    if (U8(o + 0x56) == 0) {
        Anim_Start(o, U8(G() + 0x108F) ? 0x67D420 : 0x67C978, 0, 0x40, 1);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        Sub_AmmoUse(0);
        U8(o + 0x17B) = 0x19; U8(o + 0x11B) = 0;
        U8(o + 0x56)++;
        return;
    }
    if (U8(o + 0x56) != 1) return;
    if (sub_end(o)) {
        U32(o + 0x54) = 0x10001;
        U16(PX() + 0x38) = 0x200;
    } else if (U16(o + 0x92) >= 1) {
        if (U16(o + 0x92) >= 0x15) {
            U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
            if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF92; U16(e + 0x44) = 0xFFCE; }
            else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFFC4; U16(e + 0x44) = 0; }
        } else {
            U32(e + 0x54) = Obj_Part(U32(o + 0x64), 6);
            U16(e + 0x40) = 0; U16(e + 0x42) = 0; U16(e + 0x44) = 0;
        }
    }
    if (U16(o + 0x92) != 7) return;
    gptr l = Light_Alloc();
    if (!l) return;
    U8(l + 1) = 0x16;
    U16(l + 0x10) = U16(o + 0x40); U16(l + 0x12) = U16(o + 0x42); U16(l + 0x14) = U16(o + 0x44);
    Se_Play(o, 0x20);
    Effect_SpawnAt(o, 0x67DF90, 0);
}

/* put back (0x7380A0 by the sub-weapon): the models into the hands again */
void DC2CALL Sub1_PutBack(gptr o)                                  /* 0x4B40A0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C988, 0xC, 0); blade(e);
    e += 0x70;
    carried(e, o, 0x67C878, 0xC, 0); blade(e);
    Flag_Set(1, 0xA, 0);
}

void DC2CALL Sub2_PutBack(gptr o)                                  /* 0x4B3330 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67CA00, 0xC, 1);
    blade_offset(e, 1);
    U32(e + 0x50) &= ~0x8000u;
    Flag_Set(1, 0xA, 0);
}

void DC2CALL Sub3_PutBack(gptr o)                                  /* 0x4B3650 */
{
    gptr e = U32(G() + 0x93C);
    U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
    U16(e + 0x40) = U8(G() + 0x108F) ? 0xFF4C : 0xFF7E;
    U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0;
}

void DC2CALL Sub4_PutBack(gptr o)                                  /* 0x4B3CA0 */
{
    gptr e = U32(G() + 0x93C);
    U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF92; U16(e + 0x44) = 0xFFCE; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
}

void DC2CALL Sub8_PutBack(gptr o)                                  /* 0x4B44F0 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C50C, 0xC, 1);
    blade_offset(e, 1);
    U16(e + 0x5E) = 1;
    U32(e + 0x50) &= ~0x8000u;
    Flag_Set(1, 0xA, 0);
}

void DC2CALL Sub9_PutBack(gptr o)                                  /* 0x4B4580 */
{
    gptr e = U32(G() + 0x93C);
    carried(e, o, 0x67C508, 0xC, 0);
    if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = 0xFF92; U16(e + 0x44) = 0xFFCE; }
    else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFFC4; U16(e + 0x44) = 0; }
}

/* ---- hits -------------------------------------------------------------------- */
/* candidates of a shot (16 bytes: u32 depth along the shot, then the target
 * point x/y/z in the shot's frame, its radius and height, object index,
 * descriptor index); one 0x24-byte entry per weapon kind in 0x737A90:
 * +0..+0xA the cone (Cone_HitX/Z), +0xC flags (0x800: low byte is the hit
 * range; 0x200: stops at the first object; 0x400/0x100/0x1000: reaction;
 * high half to target +0x170), +0x10/+0x12 near / middle range, +0x16
 * damage by range, +0x1C cone shape */
#define HITS          0x74E380u
#define A_WEAPON_HITS 0x737A90u

void DC2CALL Stat_Shot(void)                                       /* 0x496400 */
{
    U32(G() + 0x688)++;
}

/* quicksort of hit candidates by depth (pivot copy 0x6F0010, swap 0x6F0000) */
void DC2CALL Hit_Sort(gptr b, u32 lo, u32 hi)                      /* 0x47C030 */
{
    for (;;) {
        s32 i = (s16)lo, j = (s16)hi;
        memcpy(MEM(0x6F0010), MEM(b + (u32)(((i + j) >> 1) * 16)), 16);
        for (;;) {
            while (U32(b + (u32)(i * 16)) < U32(0x6F0010)) i++;
            while (U32(0x6F0010) < U32(b + (u32)(j * 16))) j--;
            if (i > j) break;
            memcpy(MEM(0x6F0000), MEM(b + (u32)(i * 16)), 16);
            memcpy(MEM(b + (u32)(i * 16)), MEM(b + (u32)(j * 16)), 16);
            memcpy(MEM(b + (u32)(j * 16)), MEM(0x6F0000), 16);
            i++; j--;
            if (i > j) break;
        }
        if ((s16)lo < j) Hit_Sort(b, lo, (u32)j);
        if (i >= (s16)hi) return;
        lo = (u32)i;
    }
}

/* a shot of weapon kind `kind` from pos, its frame m: hits the targets
 * (objects 1.. with aim descriptors +0x134) inside the kind's cone, nearest
 * first; returns the number hit */
u32 DC2CALL Weapon_Hit(gptr m, gptr pos, u32 kind)                 /* 0x47B430 */
{
    gptr e = A_WEAPON_HITS + kind * 0x24;
    if (kind == 5) {
        s16 r = S16(U32(G() + 0x948) + 0x18);
        U16(e + 2) = (u16)(r > 0x898 ? 0x898 : r);
    }
    gptr f = Tmp_Push(0x40), v = f, a1 = f + 8, a2 = f + 0x10, t = f + 0x18;
    TransposeMatrix(m, t);
    u32 n = 0;
    gptr r = HITS, ob = U32(G() + 0x940);
    for (u32 i = 1; i < U32(0x87F940); i++) {
        ob += 0x260;
        gptr d = U32(ob + 0x134);
        if (!d || !(U8(ob + 0x50) & 8) || U8(ob + 0x17B)) continue;
        u8 k = 0;
        d -= 0xC;
        do {
            d += 0xC;
            gptr p = Obj_Part(U32(ob + 0x64), U8(d + 0xA));
            ApplyMatrixSV(p + 0x20, d, v);
            U16(v) += (u16)(U16(p + 0x40) - U16(pos));
            U16(v + 2) += (u16)(U16(p + 0x42) - U16(pos + 2));
            U16(v + 4) += (u16)(U16(p + 0x44) - U16(pos + 4));
            ApplyMatrixSV(t, v, v);
            U32(r) = (u32)((s32)S16(v + 2) - (s32)U16(d + 6));
            U16(r + 4) = U16(v); U16(r + 6) = U16(v + 2); U16(r + 8) = U16(v + 4);
            U16(r + 0xA) = U16(d + 6); U16(r + 0xC) = U16(d + 8);
            U8(r + 0xE) = (u8)i; U8(r + 0xF) = k;
            n++; r += 0x10; k++;
        } while (U8(d + 0xB) & 1);
    }
    u32 hits = 0;
    if (!n) goto out;
    Hit_Sort(HITS, 0, n - 1);
    u32 ca = (u32)S16(e + 2) << 16 | (u32)(s32)S16(e);
    u32 cx = (u32)S16(e + 6) << 16 | (u32)(s32)S16(e + 4);
    u32 cz = (u32)S16(e + 0xA) << 16 | (u32)(s32)S16(e + 8);
    for (u32 j = 0; j < n; j++) {
        gptr c = HITS + j * 0x10, q = c + 4;
        if (!U32(c)) continue;
        if (!Cone_HitX(ca, cx, e + 0x1C, q)) continue;
        U32(a1) = U32(q); U16(a1 + 4) = (u16)(U16(q + 8) + U16(q + 4));    /* top */
        if (!Cone_HitZ(ca, cz, e + 0x1C, a1)) {
            U32(a2) = U32(q); U16(a2 + 4) = (u16)(U16(q + 4) - U16(q + 8)); /* bottom */
            if (!Cone_HitZ(ca, cz, e + 0x1C, a2) && (S16(a1 + 4) <= 0 || S16(a2 + 4) >= 0)) continue;
        }
        gptr o = U32(G() + 0x940) + U8(c + 0xE) * 0x260;
        gptr d = U32(o + 0x134) + U8(c + 0xF) * 0xC;
        if (kind < 0x10 || kind == 0x1F || kind == 0x22) {        /* bullets: not through walls */
            if (!hits) Stat_Shot();
            gptr p = Obj_Part(U32(o + 0x64), U8(d + 0xA));
            ApplyMatrixSV(p + 0x20, d, v);
            U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
            if (Wall_Between(pos, v)) continue;
        }
        if (kind == 0x10 || kind == 0x1D) Se_Play(o, U8(o + 0x58) == 0x17 ? 0x68 : 0x25);
        else if (kind == 0x11 || kind == 0x1E) {
            Se_Play(o, U8(o + 0x58) == 0x17 ? 0x68 : 0x27);
            Effect_Attach(U32(G() + 0x940), 0x67DAB0);
        }
        U8(o + 0x174) |= 1;
        U8(o + 0x180) = (u8)hits;
        U16(o + 0x178) = (u16)Angle_XZ(pos, Obj_Part(U32(o + 0x64), U8(d + 0xA)) + 0x40);
        u32 fl = U32(e + 0xC);
        if (fl & 0x800) U8(o + 0x17A) = (u8)fl;
        else U8(o + 0x17A) = U32(c) <= U16(e + 0x10) ? 0 : U32(c) <= U16(e + 0x12) ? 1 : 2;
        U16(o + 0x170) = (u16)(U32(e + 0xC) >> 16);
        fl = U32(e + 0xC);
        U16(o + 0x172) = fl & 0x400 ? 4 : fl & 0x100 ? 3 : fl & 0x1000 ? 5 : (u16)(2 - U8(o + 0x17A));
        u16 dmg = U16(e + 0x16 + U8(o + 0x17A) * 2);
        U16(o + 0x176) = dmg;
        if (U8(0x87E747) == 1 && (U8(G() + 0x108F) == 4 || U8(G() + 0x108F) >= 7)) {
            U16(o + 0x176) = (u16)(dmg * 5);
            U8(0x87E984) = 0xA;
        }
        if (kind == 2 && Rand8() >= 0xF0) { U16(o + 0x176) <<= 1; U16(o + 0x172) = 2; }   /* critical */
        U8(o + 0x17D) = (u8)kind;
        U8(o + 0x17B) = 1;
        DrawSync(U16(o + 0x176) >> 2);                  /* 0x474B10, a lone RET */
        U32(o + 0x54) = 2;
        U8(o + 0x17C) = U8(c + 0xF);
        hits++;
        if (U32(e + 0xC) & 0x200) break;
        for (u32 s = 0; s < n; s++)                     /* one hit per object */
            if (U8(HITS + s * 0x10 + 0xE) == U8(c + 0xE)) U32(HITS + s * 0x10) = 0;
    }
out:
    Tmp_Pop(0x40);
    return hits;
}

/* ---- thrown weapons ------------------------------------------------------------ */
#define PROBE 0x74E580u     /* stand-in effect slot for the wall test */

/* a weapon effect (0x2C words) moving from its +0x74/+0x78 to +0x40/+0x44
 * across a wall of its floor: 1 | block << 24 | edge << 16, or 0 */
u32 DC2CALL Eff_WallHit(gptr e)                                    /* 0x453060 */
{
    if (U16(e + 0x74) == U16(e + 0x40) && U16(e + 0x78) == U16(e + 0x44)) return 0;
    gptr f = Tmp_Push(0x30), seg = f, w = f + 8, d = f + 0x10, a = f + 0x20;
    U16(seg) = U16(e + 0x74); U16(seg + 2) = U16(e + 0x78); U16(seg + 4) = U16(e + 0x40); U16(seg + 6) = U16(e + 0x44);
    s16 h = (s16)(S16(e + 0x42) / 100 + U16(e + 0x5A) * 10);
    u32 r = 0;
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)); k++) {
        gptr c = U32(G() + 0xA4C), blk = c + U32(c + 8 + 4 * k);
        if (!(U16(blk + 4) & (1u << (U8(e + 0x5A) & 31)))) continue;
        if ((s32)h - (s32)U8(blk + 6) < -10) continue;
        if ((U8(blk + 3) & 0x10) && (U8(e + 0x85) & 0x10)) continue;
        if ((U8(blk + 3) & 8) && (U8(e + 0x85) & 8)) continue;
        gptr at = blk + 8, pt = blk + 8 + U8(blk + 1) * 4;
        for (u32 j = 0; j < U8(blk + 1); j++, at += 4, pt += 4) {
            U32(w) = U32(pt); U32(w + 4) = U32(pt + 4);
            if (!(U8(at) & 1)) continue;
            if ((U8(at) & 0x80) && (U8(e + 0x50) & 0x40)) continue;
            if ((U8(at + 1) & 0x10) && (U8(e + 0x85) & 0x10)) continue;
            if ((U8(at + 1) & 8) && (U8(e + 0x85) & 8)) continue;
            if (!Seg_Intersect(w, seg)) continue;
            S32(a) = S16(seg) - S16(w); S32(a + 4) = 0; S32(a + 8) = S16(seg + 2) - S16(w + 2);
            S32(d) = S16(w + 4) - S16(w); S32(d + 4) = 0; S32(d + 8) = S16(w + 6) - S16(w + 2);
            OuterProduct12(a, d, d);
            if (S32(d + 4) >= 0) { r = (k << 8 | j) << 16 | 1; goto done; }   /* from the front */
        }
    }
done:
    Tmp_Pop(0x30);
    return r;
}

/* an effect slot of kind 6 (an explosion or similar) at pos */
gptr DC2CALL Effect_Spawn(gptr pos, gptr data, u32 a, u32 b, u32 c, u32 d)   /* 0x48DBC0 */
{
    gptr k = K2_Alloc();
    if (k) {
        U8(k + 0x58) = 6;
        U32(k + 0x98) = data; U32(k + 0xA0) = pos;
        U8(k + 0x9C) = (u8)a; U8(k + 0x9D) = (u8)b;
        U32(k + 0xA4) = c; U32(k + 0xA8) = d;
        Orig_Call1(U32(U32(0x87F948) + 0x18), k);
    }
    return k;
}

/* a noise the enemies hear (grenades and the like) */
void DC2CALL Noise_Make(u32 x, u32 y, u32 z)                        /* 0x48B830 */
{
    U16(0x7598B8) = (u16)x; U16(0x7598BC) = (u16)z; U16(0x7598BA) = (u16)y;
    u8 s = U8(G() + 0x10B5);
    if (U8(G() + 0x10B4) == 0xB) { U16(0x7593B0) = 0x30; U16(0x7598BA) = (u16)(y + 0x2BC); }
    else if (s == 1 || s == 7) U16(0x7593B0) = 0x10;
    else if (s == 2 || s == 8) U16(0x7593B0) = 0x20;
    Flag_Set(2, 0xA, 1);
}

/* the sub-weapons in flight (object-effect slots G+0x93C held by nobody,
 * +0x4E 3): +0x4F 3 = lying (+0x46 frames to live, +0x47 to the blast),
 * 5.. flying weapon kind 0x10 + state (6 and 10 home on 0x87FD80[i] for
 * +0x46 frames), 0x10 = blast; o is the player */
void DC2CALL Thrown_Update(gptr o)                                 /* 0x47AFA0 */
{
    gptr e = U32(G() + 0x93C);
    memset(MEM(PROBE), 0, 0x2B * 4);
    gptr f = Tmp_Push(0x48), v = f + 0x18, ln = f + 0x20, mt = f + 0x28;
    /* i counts the slots and indexes the homing targets; the original reuses
     * its variable for the wall's edge index after a wall hit (so the loop
     * can end early or run on past the 12 slots) - kept */
    for (u32 i = 0; i < 12; i++, e += 0x70) {
        u32 fl = U32(e + 0x50);
        if (!(fl & 1) || U8(e + 0x4E) != 3) continue;
        if (U8(e + 0x4F) == 3) {
            if (U8(e + 0x46)) U8(e + 0x46)--; else U32(e + 0x50) = fl | 2;
            if (U8(e + 0x47)) U8(e + 0x47)--; else U8(e + 0x4F) = 0x10;
        }
        s8 st = S8(e + 0x4F);
        if (st < 5) continue;
        gptr p40 = e + 0x40;
        if (st == 0x10) {                                       /* blast */
            U16(v) = 0x400; U16(v + 2) = 0; U16(v + 4) = 0x400;
            RotMatrixYXZ(v, mt);
            Weapon_Hit(mt, p40, 0x13);
            Se_Play(o, 0x23);
            Effect_Spawn(p40, 0x67DAE0, 0, 0, 0x20, 0);
            Eff_Free(e);
            continue;
        }
        U16(e + 0x5C) = U16(e + 0x44);
        U32(e + 0x58) = U32(e + 0x40);
        int moved = 0;
        if (st == 6 || st == 0xA) {
            if (!U8(e + 0x46)) {                                /* homing over: straight on */
                ApplyMatrixSV(e + 0x20, e + 0x60, v);
                U16(p40) += U16(v); U16(e + 0x42) += U16(v + 2); U16(e + 0x44) += U16(v + 4);
                U16(v) = U16(p40); U16(v + 4) = U16(e + 0x44);
                moved = 1;
            } else {
                U8(e + 0x46)--;
                s16 ti = S16(0x87FD80 + i * 4);
                if (ti > 0) {
                    gptr t = U32(G() + 0x940) + (u32)(s32)ti * 0x260;
                    if (U8(t + 0x50) & 8) {
                        gptr p = Obj_Part(U32(t + 0x64), (u32)(s32)S16(0x87FD82 + i * 4));
                        Angles_LookAt(p + 0x40, p40, v);
                        u32 r = (Rand8() & 0x1F) + 0x50;
                        U16(e + 0x48) += (u16)Angle_Approach(U16(e + 0x48), r, U32(v) + 0x400);
                        U16(e + 0x4A) += (u16)Angle_Approach(U16(e + 0x4A), r, U32(v + 2));
                    }
                }
            }
        }
        if (!moved) {
            ApplyMatrixSV(e + 0x20, e + 0x60, v);
            U16(p40) += U16(v); U16(e + 0x42) += U16(v + 2); U16(e + 0x44) += U16(v + 4);
            if (S16(p40) > 30000 || S16(p40) < -30000 || S16(e + 0x42) > 30000 || S16(e + 0x42) < -30000
                || S16(e + 0x44) > 30000 || S16(e + 0x44) < -30000) { Eff_Free(e); continue; }
            U32(PROBE + 0x50) = 1;
            U16(PROBE + 0x5A) = (u16)(S16(e + 0x42) / -1000);
            U8(PROBE + 0x85) |= 8;
            U32(PROBE + 0x40) = U32(p40); U16(PROBE + 0x44) = U16(e + 0x44);
            U32(PROBE + 0x74) = U32(e + 0x58); U16(PROBE + 0x78) = U16(e + 0x5C);
            if (Weapon_Hit(e + 0x20, e + 0x58, (u32)(s32)S8(e + 0x4F) + 0x10)) goto hit;
            if ((u16)Floor_Cross(p40, e + 0x58, v, 8)) {
                u32 h = Eff_WallHit(PROBE);
                if (!h) continue;                               /* still flying */
                gptr c = U32(G() + 0xA4C), blk = c + U32(c + 8 + (h >> 24) * 4);
                i = (h >> 16) & 0xFF;
                U16(ln) = U16(e + 0x58); U16(ln + 2) = U16(e + 0x5C);
                U16(ln + 4) = U16(p40); U16(ln + 6) = U16(e + 0x44);
                Seg_Cross(blk + (U8(blk + 1) + ((h >> 16) & 0xFF)) * 4 + 8, ln, v);
            }
        }
        U16(p40) = U16(v); U16(e + 0x44) = U16(v + 4);
        if (S8(e + 0x4F) == 6) {                                /* the rocket blows up and ends the frame */
            Effect_Spawn(p40, 0x67A31C, 0, 0, 0x20, 0);
            Se_Play(o, 0x15);
            Eff_Free(e);
            break;
        }
    hit:
        switch (S8(e + 0x4F)) {
        case 5: Effect_Spawn(p40, 0x679E30, 0, 0, 0x20, 0); Se_Play(o, 0x16); break;
        case 6: Effect_Spawn(p40, 0x67A040, 0, 0, 0x20, 0); Se_Play(o, 0x15); break;
        case 7:
            Effect_Spawn(p40, 0x678CF4, 0, 0, 0x20, 0); Se_Play(o, 0x16);
            Noise_Make(U16(p40), U16(e + 0x42), U16(e + 0x44));
            break;
        case 10: Effect_Spawn(p40, 0x667290, 0, 0, 0x20, 0); Se_Play(o, 0x1D); break;
        }
        Eff_Free(e);
    }
    Tmp_Pop(0x48);
}

/* ---- the thrown sub-weapons (3, 4, 6) ----------------------------------------------- */
/* up to six mines 1200 ahead of o's facing, the first at pos, each further
 * one scattered from the last; a mine that would land across a wall is
 * dropped and ends the row; returns how many (the slot +0x46 counts them) */
u32 DC2CALL Mines_Place(gptr o, gptr pos)                          /* 0x47C8F0 */
{
    gptr e = Eff_Alloc();
    if (!e) return 0;
    gptr f = Tmp_Push(0x30), v = f, d = f + 8, m = f + 0x10;
    RotMatrixYXZ(o + 0x48, m);
    U32(v) = 0; U16(v + 4) = 0x4B0;
    ApplyMatrixSV(m, v, d);
    U16(e + 0x40) = U16(pos); U16(e + 0x42) = U16(pos + 2); U16(e + 0x44) = U16(pos + 4);
    memset(MEM(PROBE), 0, 0x2B * 4);
    u8 n = 0, step = 2;
    for (;;) {
        U32(e + 0x50) = 1;
        U32(e + 0x68) = 0x67CA14; U32(e + 0x54) = 0;
        U8(e + 0x4E) = 3; U8(e + 0x4F) = 3;
        U32(e + 0x58) = U32(e + 0x40); U16(e + 0x5C) = U16(e + 0x44);
        if (step != 2) {
            s16 r = (s8)Rand8();
            U16(e + 0x40) += (u16)(U16(d) + r * 4);
            U16(e + 0x42) += U16(d + 2);
            r = (s8)Rand8();
            U16(e + 0x44) += (u16)(U16(d + 4) + r * 4);
        }
        U16(e + 0x48) = 0x800;
        U8(e + 0x46) = n; U8(e + 0x47) = step;
        U32(PROBE + 0x50) = 1;
        U8(PROBE + 0x85) = 0;
        U16(PROBE + 0x5A) = (u16)(S16(e + 0x42) / -1000);
        U32(PROBE + 0x40) = U32(e + 0x40); U16(PROBE + 0x44) = U16(e + 0x44);
        U32(PROBE + 0x74) = U32(e + 0x58); U16(PROBE + 0x78) = U16(e + 0x5C);
        if (Eff_WallHit(PROBE)) { Eff_Free(e); break; }
        gptr k = K2_AllocBack();
        if (k) {                                            /* its shadow */
            U8(k + 0x7A) = 3;
            U16(k + 0x6E) = 0x14; U16(k + 0x6C) = 0x14;
            U8(k + 0x58) = 2;
            U8(k + 0x64) = 0; U8(k + 0x65) = 0; U8(k + 0x66) = 0xFF;
            U8(k + 0x84) |= 8;
            U32(k + 0x60) = e;
        }
        u32 xy = U32(e + 0x40); u16 z = U16(e + 0x44);
        step += 3; n++;
        if (step > 0x11) break;
        e = Eff_Alloc();
        if (!e) break;
        U32(e + 0x40) = xy; U16(e + 0x44) = z;
    }
    Tmp_Pop(0x30);
    return n;
}

/* the throw: wound up from frame 3 (the item in the other hand), let go of
 * in the hand at frame 0x1A on; at frame 6 the landing point 10000 along
 * part 6, if the floor test (0x4542F0) lets it */
struct throw_sub { gptr anim[2]; u8 blend, kind; u16 yz[2]; };

static void throw_sub(gptr o, const struct throw_sub *s)
{
    gptr e = U32(G() + 0x93C);
    if (U8(o + 0x56) == 0) {
        Anim_Start(o, s->anim[U8(G() + 0x108F) != 0], 0, 0x40, s->blend);
        U32(o + 0xF0) = 0; U16(o + 0xF4) = 0;
        U8(o + 0x17B) = 5; U8(o + 0x11B) = 0;
        if (s->kind == 6) Sub_AmmoUse(0);
        U8(o + 0x56)++;
        if (s->kind != 3) return;                           /* 3 goes on to the landing test, also below */
    } else if (U8(o + 0x56) != 1) {
        if (s->kind != 3) return;
    } else if (sub_end(o)) {
        U32(o + 0x54) = 0x10001;
        U16(PX() + 0x38) = 0x200;
    } else if (U16(o + 0x92) >= 3) {
        if (U16(o + 0x92) >= 0x1A) {
            U32(e + 0x54) = Obj_Part(U32(o + 0x64), 0xC);
            if (!U8(G() + 0x108F)) { U16(e + 0x40) = 0xFF7E; U16(e + 0x42) = s->yz[0]; U16(e + 0x44) = s->yz[1]; }
            else { U16(e + 0x40) = 0xFF4C; U16(e + 0x42) = 0xFF60; U16(e + 0x44) = 0; }
        } else {
            U32(e + 0x54) = Obj_Part(U32(o + 0x64), 6);
            U16(e + 0x40) = 0; U16(e + 0x42) = 0; U16(e + 0x44) = 0;
        }
    }
    if (U16(o + 0x92) != 6) return;
    Obj_UpdateMatrix(o);
    gptr p = Obj_Part(U32(o + 0x64), 6);
    gptr v = Tmp_Push(8);
    U16(v) = 0; U16(v + 2) = 0x2710; U16(v + 4) = 0;
    ApplyMatrixSV(p + 0x20, v, v);
    U16(v) += U16(p + 0x40); U16(v + 2) += U16(p + 0x42); U16(v + 4) += U16(p + 0x44);
    if (!(u16)Floor_Cross(v, p + 0x40, v, 0x80)) {
        if (s->kind == 3) {                                 /* a grenade: the effect takes the spot */
            Effect_SpawnAt(o, 0x67D80C, 0);
            Sub_AmmoUse(1);
            Se_Play(o, 0x20); Se_Play(o, 0x22); Se_Play(o, 0x24);
            U16(v + 6) = U16(o + 0x4A);
            Effect_Spawn(v, 0x67D888, 0, 0, 0x20, 0);
        } else if (s->kind == 4) {                          /* one mine (+0x46: as the original, an unset stack byte) */
            gptr m = Eff_Alloc();
            if (m) {
                Sub_AmmoUse(1);
                Effect_SpawnAt(o, 0x67DA64, 0);
                Se_Play(o, 0x20); Se_Play(o, 0x22);
                U32(m + 0x50) = 3; U32(m + 0x68) = 0x67CA14; U32(m + 0x54) = 0;
                U8(m + 0x4E) = 3; U8(m + 0x4F) = 3;
                U32(m + 0x40) = U32(v); U32(m + 0x44) = U32(v + 4);
                U8(m + 0x47) = 0; U16(m + 0x48) = 0x800;
            }
        } else if (Mines_Place(o, v)) {                     /* a row of mines */
            Sub_AmmoUse(1);
            Effect_SpawnAt(o, 0x67DA64, 0);
            Se_Play(o, 0x20); Se_Play(o, 0x22);
        }
    }
    Tmp_Pop(8);
}

void DC2CALL Sub3_Use(gptr o)                                      /* 0x4B3440 */
{
    static const struct throw_sub s = {{0x67C9F4, 0x67D0D0}, 4, 3, {0xFF60, 0}};
    throw_sub(o, &s);
}

void DC2CALL Sub4_Use(gptr o)                                      /* 0x4B36B0 */
{
    static const struct throw_sub s = {{0x67CB8C, 0x67D268}, 4, 4, {0xFF92, 0xFFCE}};
    throw_sub(o, &s);
}

void DC2CALL Sub6_Use(gptr o)                                      /* 0x4B3AA0 */
{
    static const struct throw_sub s = {{0x67CB8C, 0x67D268}, 2, 6, {0xFF92, 0xFFCE}};
    throw_sub(o, &s);
}
