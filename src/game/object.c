/* Generic object handler (type 1, 0x490120) and the helpers it shares with
 * the player and enemies: head tracking (+0x140..+0x16E), foot anchoring
 * (+0x11B/+0x120), angle and distance helpers.
 * Object: +0x20 matrix, +0x40 position, +0x48 rotation, +0x50 flags,
 * +0x54 mode, +0x64 first linked part, +0xE0 previous position, +0xF0 local
 * velocity. Head block: +0x140/142 current yaw/pitch, +0x150 target point,
 * +0x158 head mode (table 0x737A64), +0x15D target object, +0x15E target part
 * (0xFF: the object itself), +0x15F head part, +0x161 "reached" bits,
 * +0x164/166 turn speeds, +0x168/16C wanted angles, +0x16A/16E limits. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/gte.h"

#define A_HEAD_MODES    0x737A64u
#define A_GENERIC_MODES 0x739B1Cu

/* hand-over byte: a nonzero argument stores it, zero takes (and clears) it */
u32 DC2CALL Latch_74B16C(u32 a)                                    /* 0x448730 */
{
    if ((u8)a) { U8(0x74B16C) = (u8)a; return 0; }
    u8 v = U8(0x74B16C);
    if (v) U8(0x74B16C) = 0;
    return v;
}

/* +0xFD = 1 when group 0 has at least as many active objects as group 1 */
void DC2CALL Obj_PickGroup(gptr obj)                               /* 0x46B8F0 */
{
    u32 n[2] = {0, 0};
    gptr o = U32(G() + 0x940), end = o + U32(0x87F940) * 0x260;
    for (; o < end; o += 0x260)
        if (U8(o + 0x50) & 1) n[U8(o + 0xFD)]++;
    U8(obj + 0xFD) = n[0] >= n[1];
}

/* step from angle cur toward target (12-bit), at most max (signed) */
u32 DC2CALL Angle_Approach(u32 cur, u32 max, u32 target)           /* 0x47A5E0 */
{
    u32 d = (target - cur) & 0xFFF;
    if (d & 0x800) {
        d |= 0xF000;
        return (s32)d >= 0x10000 - (s32)(s16)max ? d : 0u - max;
    }
    return (s32)d > (s32)(s16)max ? (max & 0xFFFF) : d;
}

/* same, but the direction comes from target itself when |target| > lim */
u32 DC2CALL Angle_ApproachLim(u32 cur, u32 max, u32 target, u32 lim) /* 0x47A6F0 */
{
    u32 c = target;
    if (c & 0x800) c |= 0xFFFFF000u;
    s32 m = (s16)c; if (m < 0) m = -m;
    u32 d = (c - cur) & 0xFFF;
    int neg = m > (s16)lim ? (c & 0x800) != 0 : (d & 0x800) != 0;
    if (neg) {
        u16 x = (u16)(d | 0xF000);
        return (s16)x >= (s16)(0u - max) ? x : 0u - max;
    }
    return (s16)d > (s16)max ? max : d;
}

void DC2CALL Head_Reset(gptr o)                                    /* 0x47A880 */
{
    U32(o + 0x150) = 0; U32(o + 0x154) = 0;
    U32(o + 0x140) = 0; U32(o + 0x144) = 0;
    U16(o + 0x168) = 0; U16(o + 0x16C) = 0;
    U32(o + 0x158) = 0;
}

void DC2CALL Head_Update(gptr o)                                   /* 0x47A930 */
{
    u8 st = U8(G() + 0x108F);
    if (U8(0x87E747) != 2 && st >= 2 && st <= 4) U8(o + 0x15E) = 0;
    Orig_Call1(U32(A_HEAD_MODES + (s32)S8(o + 0x158) * 4), o);
}

/* head mode 1: the stored angles follow the head part back */
void DC2CALL Head_Relax(gptr o)                                    /* 0x47A970 */
{
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    for (int k = 0; k < 2; k++) {
        gptr cur = o + 0x140 + 2 * k, pa = p + 0x48 + 2 * k;
        if (U16(o + 0x16A + 4 * k) && !(U8(o + 0x161) & (1 << k))) {
            u16 v = (u16)((Angle_Approach(U16(cur), U16(o + 0x164 + 2 * k), U16(pa)) + U16(cur)) & 0xFFF);
            U16(cur) = v;
            if (U16(pa) == v) U8(o + 0x161) |= 1 << k;
            U16(pa) = v;
        } else U16(cur) = U16(pa);
    }
}

/* head mode 2: look at object +0x15D (its part +0x15E, or its position) */
void DC2CALL Head_LookAtObj(gptr o)                                /* 0x47AA60 */
{
    U8(o + 0x161) = 0;
    gptr t = U32(G() + 0x940) + U8(o + 0x15D) * 0x260;
    if (U8(o + 0x15E) != 0xFF) t = Obj_Part(U32(t + 0x64), U8(o + 0x15E));
    U16(o + 0x150) = U16(t + 0x40);
    U16(o + 0x152) = U16(t + 0x42);
    U16(o + 0x154) = U16(t + 0x44);
    Head_LookAtPoint(o);
}

/* head mode 3: look at the point +0x150 */
void DC2CALL Head_LookAtPoint(gptr o)                              /* 0x47AB40 */
{
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    gptr a = Tmp_Push(8);
    Angles_LookAt(o + 0x150, p + 0x40, a);
    U16(o + 0x168) = U16(a);
    U16(o + 0x16C) = U16(a + 2);
    Tmp_Pop(8);
    Head_Turn(o);
}

static void head_clamp(gptr cur, u16 lim)
{
    u16 v = U16(cur);
    if (v & 0x800) { u16 n = (u16)(-lim & 0xFFF); if ((s16)v < (s16)n) U16(cur) = n; }
    else if ((s32)(s16)v > (s32)lim) U16(cur) = lim;
}

/* head mode 4: turn toward the wanted angles +0x168/+0x16C within limits */
void DC2CALL Head_Turn(gptr o)                                     /* 0x47ABA0 */
{
    gptr p = Obj_Part(U32(o + 0x64), U8(o + 0x15F));
    if (U16(o + 0x16A)) {
        u32 r = Angle_Approach(U16(o + 0x140), U16(o + 0x164), U16(o + 0x168));
        U16(o + 0x140) = (u16)((r + U16(o + 0x140)) & 0xFFF);
        head_clamp(o + 0x140, U16(o + 0x16A));
        U16(p + 0x48) = U16(o + 0x140);
    }
    if (U16(o + 0x16E)) {
        u16 w = (u16)((U16(o + 0x16C) - U16(o + 0x4A)) & 0xFFF);
        U16(o + 0x16C) = w;
        u32 r = Angle_ApproachLim(U16(o + 0x142), U16(o + 0x166), w, U16(o + 0x16E));
        U16(o + 0x142) = (u16)((r + U16(o + 0x142)) & 0xFFF);
        head_clamp(o + 0x142, U16(o + 0x16E));
        U16(p + 0x4A) = U16(o + 0x142);
    }
}

/* squared horizontal distance (wraps like the original) */
u32 DC2CALL Dist2_XZ(gptr a, gptr b)                               /* 0x487300 */
{
    s32 dx = S16(b) - S16(a), dz = S16(b + 4) - S16(a + 4);
    u32 x = (u32)(dx < 0 ? -dx : dx), z = (u32)(dz < 0 ? -dz : dz);
    return z * z + x * x;
}

/* move by vector v given in the object's frame */
void DC2CALL Obj_MoveLocal(gptr o, gptr v)                         /* 0x48F3A0 */
{
    gptr t = Tmp_Push(8);
    ApplyMatrixSV(o + 0x20, v, t);
    U16(o + 0x40) += U16(t);
    U16(o + 0x42) += U16(t + 2);
    U16(o + 0x44) += U16(t + 4);
    Tmp_Pop(8);
}

/* SquareRoot0 with one Newton step */
u32 DC2CALL Sqrt_Refined(u32 x)                                    /* 0x48F590 */
{
    u32 r = (u32)SquareRoot0((s32)x);
    return r ? (x / r + r) >> 1 : 0;
}

u32 DC2CALL Camera_Distance(gptr a, gptr b)                        /* 0x48F5C0 */
{
    s32 d[3];
    for (int k = 0; k < 3; k++) { d[k] = S16(a + 2 * k) - S16(b + 2 * k); if (d[k] < 0) d[k] = -d[k]; }
    return Sqrt_Refined((u32)d[2] * (u32)d[2] + (u32)d[1] * (u32)d[1] + (u32)d[0] * (u32)d[0]);
}

/* angles (x, y, 0) of the direction from eye to target */
void DC2CALL Angles_LookAt(gptr target, gptr eye, gptr out)        /* 0x48F620 */
{
    u32 h = Sqrt_Refined(Dist2_XZ(target, eye));
    U16(out) = (u16)ratan2((float)(S16(eye + 2) - S16(target + 2)), (float)h);
    U16(out + 2) = (u16)(ratan2((float)(S16(eye) - S16(target)), (float)(S16(eye + 4) - S16(target + 4))) + 0x800);
    U16(out + 4) = 0;
}

u32 DC2CALL Angle_XZ(gptr a, gptr b)                               /* 0x48F6C0 */
{
    return (u32)ratan2((float)(S16(b) - S16(a)), (float)(S16(b + 4) - S16(a + 4))) & 0xFFF;
}

/* q: x0, z0, x1, z1 */
u32 DC2CALL Angle_Seg(gptr q)                                      /* 0x48F710 */
{
    return (u32)ratan2((float)(S16(q + 4) - S16(q)), (float)(S16(q + 6) - S16(q + 2))) & 0xFFF;
}

void DC2CALL Obj_Set118(gptr o, u32 a, u32 b, u32 c)               /* 0x48F880 */
{
    U8(o + 0x118) = (u8)a | 0x80;
    U16(o + 0x110) = (u16)b;
    U16(o + 0x112) = (u16)c;
}

void DC2CALL Obj_SetFeet(gptr o, u32 a, u32 b)                     /* 0x48FCA0 */
{
    U8(o + 0x11B) = 0;
    U32(o + 0x120) = Obj_Part(U32(o + 0x64), a);
    U32(o + 0x124) = Obj_Part(U32(o + 0x64), b);
}

/* keep the planted foot (+0x11B bits 4-5) in place: shift the object and
 * all parts back by the foot's horizontal drift since +0x6C */
u32 DC2CALL Obj_AnchorFoot(gptr o)                                 /* 0x48FD20 */
{
    u8 n = U8(o + 0x11B);
    if (!n) return 0;
    gptr f = U32(o + 0x120 + ((n >> 4) & 3) * 4);
    s32 dx = S16(f + 0x40) - S16(f + 0x6C), dz = S16(f + 0x44) - S16(f + 0x70);
    gptr p = U32(o + 0x64);
    do {
        U16(p + 0x40) -= (u16)(U16(o + 0xE8) + dx);
        U16(p + 0x44) -= (u16)(U16(o + 0xEC) + dz);
        p = U32(p + 0x54);
    } while (p);
    U16(o + 0x44) -= (u16)dz;
    U16(o + 0x40) -= (u16)dx;
    return 1;
}

void DC2CALL Generic_Handler(gptr o)                               /* 0x490120 */
{
    U16(o + 0xE0) = U16(o + 0x40);
    U16(o + 0xE2) = U16(o + 0x42);
    U16(o + 0xE4) = U16(o + 0x44);
    Orig_Call1(U32(A_GENERIC_MODES + U8(o + 0x54) * 4), o);
    RotMatrix(o + 0x48, o + 0x20);
    Obj_MoveLocal(o, o + 0xF0);
    Obj_UpdateMatrix(o);
    Head_Update(o);
    Obj_AnchorFoot(o);
    if (!(U8(o + 0x50) & 0x20)) Fn_452BD0(o);
    if (!(U8(o + 0x50) & 0x10)) Fn_453430(o);
}

/* mode 0 */
void DC2CALL Generic_Init(gptr o)                                  /* 0x4901B0 */
{
    U32(o + 0x50) = 3;
    U16(o + 0x108) = 0x4B0;
    U32(o + 0x12C) = 0x739B10;
    U32(o + 0x128) = 0x739B00;
    Model_Setup(o, U32(o + 0x60));
    Obj_Set118(o, 8, 0x1000, 0x1000);
    Head_Reset(o);
    U16(o + 0x164) = 0x60; U16(o + 0x166) = 0x60;
    U8(o + 0x15F) = 7; U8(o + 0x160) = 0;
    U16(o + 0x16A) = 0x200; U16(o + 0x16E) = 0x300;
    U8(o + 0x15D) = 1;
    U16(o + 0x168) = 0x200; U16(o + 0x16C) = 0x300;
    U8(o + 0x15C) = 5;
    Obj_SetFeet(o, 0xB, 0xE);
    U32(o + 0x54) = 4;
}

/* modes 1-5 */
void DC2CALL Generic_Step(gptr o) { Anim_Step(o); }                /* 0x4A56D0 */

/* mode 6: room callback G+0x930 first */
void DC2CALL Generic_StepCb(gptr o)                                /* 0x490270 */
{
    Orig_Call1(U32(G() + 0x930), o);
    Anim_Step(o);
}
