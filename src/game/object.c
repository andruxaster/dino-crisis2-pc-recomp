/* Generic object handler (type 1, 0x490120) and the helpers it shares with
 * the player and enemies: head tracking (+0x140..+0x16E), foot anchoring
 * (+0x11B/+0x120), angle and distance helpers.
 * Object: +0x20 matrix, +0x40 position, +0x48 rotation, +0x50 flags,
 * +0x54 mode, +0x64 first linked part, +0xE0 previous position, +0xF0 local
 * velocity. Head block: +0x140/142 current yaw/pitch, +0x150 target point,
 * +0x158 head mode (table 0x737A64), +0x15D target object, +0x15E target part
 * (0xFF: the object itself), +0x15F head part, +0x161 "reached" bits,
 * +0x164/166 turn speeds, +0x168/16C wanted angles, +0x16A/16E limits. */
#include <string.h>
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
    if (!(U8(o + 0x50) & 0x20)) Obj_CollideObjs(o);
    if (!(U8(o + 0x50) & 0x10)) Obj_CollideWalls(o);
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

/* ---- collision ------------------------------------------------------------- */
/* Collision shapes: o+0x12C list of 12-byte cylinders against other objects,
 * o+0x128 one cylinder against the room's wall polygons (G+0xA4C).
 * Cylinder: +0 offset (SVECTOR x,y,z), +6 radius, +8 half height, +0xA part
 * (0xFF: the object itself), +0xB flags (bit 0 another entry follows, bit 1
 * soft: push 1/16). Points of 2D segments are (x, z) halfword pairs. */

static u32 imul(u32 a, u32 b) { return a * b; }

/* cylinder centre in world space (and its radius in +6) */
static void cyl_pos(gptr obj, gptr m, gptr c, gptr p)
{
    gptr src = obj;
    if (U8(c + 0xA) != 0xFF) { src = Obj_Part(U32(obj + 0x64), U8(c + 0xA)); m = src + 0x20; }
    ApplyMatrixSV(m, c, p);
    U16(p) += U16(src + 0x40); U16(p + 2) += U16(src + 0x42); U16(p + 4) += U16(src + 0x44);
    U16(p + 6) = U16(c + 6);
}

/* two cylinders (+6 radius); heights = (a << 16 | b) half heights; out: push
 * of a away from b in x/z */
u32 DC2CALL Coll_Cylinders(gptr a, gptr b, gptr out, u32 heights)  /* 0x452EE0 */
{
    s32 dy = S16(a + 2) - S16(b + 2); if (dy < 0) dy = -dy;
    if ((s32)(s16)(heights >> 16) + (s32)(s16)heights < dy) return 0;
    s32 rs = S16(a + 6) + S16(b + 6);
    u32 dx = (u32)(S16(a) - S16(b)), dz = (u32)(S16(a + 4) - S16(b + 4));
    u32 d2 = imul(dz, dz) + imul(dx, dx);
    if ((s32)imul((u32)rs, (u32)rs) <= (s32)d2) { U16(out) = 0; U16(out + 4) = 0; return 0; }
    s32 d = SquareRoot0((s32)d2);
    gptr t = Tmp_Push(0x30), q = t, r = t + 8, m = t + 0x10;
    U16(q) = U16(b); U16(q + 2) = U16(b + 4); U16(q + 4) = U16(a); U16(q + 6) = U16(a + 4);
    u32 ang = Angle_Seg(q);
    U16(r) = 0; U16(r + 2) = (u16)ang; U16(r + 4) = 0;
    RotMatrix(r, m);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = (u16)(rs - d + 4);
    ApplyMatrixSV(m, r, out);
    Tmp_Pop(0x30);
    return 1;
}

/* push o out of every other active object's cylinders; o+0x130 = bit mask
 * of the objects touched; returns the number of contacts */
u32 DC2CALL Obj_CollideObjs(gptr o)                                /* 0x452BD0 */
{
    if (!U32(o + 0x12C)) return 0;
    if (U32(o + 0x50) & 0x800) return 0;
    U32(o + 0x130) = 0;
    u32 count = 0;
    gptr t = Tmp_Push(0x58), mo = t, mb = t + 0x20, pa = t + 0x40, pb = t + 0x48, out = t + 0x50;
    RotMatrix(o + 0x48, mo);
    for (u32 i = 0; i < U32(0x87F940); i++) {
        gptr b = U32(G() + 0x940) + i * 0x260;
        u32 bf = U32(b + 0x50);
        if (!(bf & 1) || !U32(b + 0x12C) || (bf & 0x20) || b == o) continue;
        if ((U32(o + 0x50) & 0x800) && (bf & 0x800)) continue;
        s32 dx = S16(o + 0x40) - S16(b + 0x40), dz = S16(o + 0x44) - S16(b + 0x44);
        if ((dx < 0 ? -dx : dx) > 10000 || (dz < 0 ? -dz : dz) > 10000) continue;
        RotMatrix(b + 0x48, mb);
        for (gptr sa = U32(o + 0x12C);; sa += 0xC) {
            cyl_pos(o, mo, sa, pa);
            for (gptr sb = U32(b + 0x12C);; sb += 0xC) {
                cyl_pos(b, mb, sb, pb);
                if (Coll_Cylinders(pa, pb, out, (u32)U16(sa + 8) << 16 | U16(sb + 8))) {
                    if (!(U32(o + 0x50) & 0x400)) {
                        if ((U8(sb + 0xB) & 2) || (U8(sa + 0xB) & 2) || (U32(b + 0x50) & 0x2000)) {
                            S16(out) = (s16)(S16(out) >> 4); S16(out + 4) = (s16)(S16(out + 4) >> 4);
                        }
                        U16(o + 0x40) += U16(out); U16(o + 0x44) += U16(out + 4);
                        for (gptr p = U32(o + 0x64); p; p = U32(p + 0x54)) {
                            U16(p + 0x40) += U16(out); U16(p + 0x44) += U16(out + 4);
                        }
                    }
                    U32(o + 0x130) |= 1u << (i & 31);
                    count++;
                }
                if (!(U8(sb + 0xB) & 1)) break;
            }
            if (!(U8(sa + 0xB) & 1)) break;
        }
    }
    Tmp_Pop(0x58);
    return count;
}

/* bounding boxes of the two segments overlap on one axis */
static int span_overlap(s16 a0, s16 a1, s16 b0, s16 b1)
{
    if (a0 > a1) {
        if (b1 > b0) return b0 <= a0 && b1 >= a1;
        return b1 <= a0 && b0 >= a1;
    }
    if (b1 > b0) return b0 <= a1 && b1 >= a0;
    return b1 <= a1 && b0 >= a0;
}

static int in_range(s32 t, s32 den) { return den > 0 ? t >= 0 && t <= den : t >= den && t <= 0; }

/* segments a (x0,z0,x1,z1) and b intersect */
u32 DC2CALL Seg_Intersect(gptr a, gptr b)                          /* 0x453290 */
{
    if (!span_overlap(S16(a), S16(a + 4), S16(b), S16(b + 4))) return 0;
    if (!span_overlap(S16(a + 2), S16(a + 6), S16(b + 2), S16(b + 6))) return 0;
    u32 dbx = (u32)(S16(b + 4) - S16(b)), dbz = (u32)(S16(b + 6) - S16(b + 2));
    u32 dax = (u32)(S16(a + 4) - S16(a)), daz = (u32)(S16(a + 6) - S16(a + 2));
    s32 den = (s32)(imul(dbz, dax) - imul(dbx, daz));
    if (!den) return 0;
    u32 dz0 = (u32)(S16(a + 2) - S16(b + 2)), dx0 = (u32)(S16(a) - S16(b));
    s32 t1 = (s32)(imul(dz0, dbx) - imul(dx0, dbz)), t2 = (s32)(imul(dz0, dax) - imul(dx0, daz));
    return in_range(t1, den) && in_range(t2, den);
}

/* side of point p relative to the line c -> b: (p-c) x (b-c) >= 0 (y of the
 * cross product; the original computes it with OuterProduct12) */
u32 DC2CALL Seg_Side(gptr p, gptr b, gptr c)                       /* 0x453EE0 */
{
    u32 ux = (u32)(S16(p) - S16(c)), uz = (u32)(S16(p + 2) - S16(c + 2));
    u32 vx = (u32)(S16(b) - S16(c)), vz = (u32)(S16(b + 2) - S16(c + 2));
    return (s32)(imul(vx, uz) - imul(ux, vz)) >= 0;
}

/* circle p (x, -, z, radius) against segment s: 3 touches the first end,
 * 2 the second, 1 the segment between them, 0 no contact */
u32 DC2CALL Circle_Seg(gptr s, gptr p)                             /* 0x453F50 */
{
    s32 px = S16(p), pz = S16(p + 4), r = S16(p + 6);
    s32 x0 = S16(s), z0 = S16(s + 2), x1 = S16(s + 4), z1 = S16(s + 6);
    u32 r2 = imul((u32)r, (u32)r);
    u32 dx0 = (u32)(px - x0), dz0 = (u32)(pz - z0);
    if (imul(dz0, dz0) + imul(dx0, dx0) <= r2) return 3;
    u32 dx1 = (u32)(px - x1), dz1 = (u32)(pz - z1);
    if (imul(dz1, dz1) + imul(dx1, dx1) <= r2) return 2;
    s32 hx = (s32)dx0 >> 1, hz = (s32)dz0 >> 1, sx = (x1 - x0) >> 1, sz = (z1 - z0) >> 1;
    u32 dot0 = imul((u32)hz, (u32)sz) + imul((u32)hx, (u32)sx);
    u32 dot1 = imul((u32)sz, dz1) + imul((u32)sx, dx1);
    if (!((dot0 ^ dot1) & 0x80000000u)) return 0;
    u32 ax = (u32)(sx < 0 ? -sx : sx), az = (u32)(sz < 0 ? -sz : sz);
    u32 len = (u32)SquareRoot0((s32)(imul(az, az) + imul(ax, ax)));
    u32 lim = imul(len, (u32)r) >> 1;
    s32 c = (s32)(imul((u32)sx, (u32)hz) - imul((u32)hx, (u32)sz));
    return (c < 0 ? 0u - (u32)c : (u32)c) <= lim;
}

/* intersection point of segments a and b into out (x, 0, z, 0) */
u32 DC2CALL Seg_Cross(gptr a, gptr b, gptr out)                    /* 0x4540B0 */
{
    s32 a0 = S16(a), a1 = S16(a + 2), a2 = S16(a + 4), a3 = S16(a + 6);
    s32 b0 = S16(b), b1 = S16(b + 2), b2 = S16(b + 4), b3 = S16(b + 6);
    U32(out) = 0; U32(out + 4) = 0;
    u32 dax = (u32)(a2 - a0), daz = (u32)(a3 - a1), dbx = (u32)(b2 - b0), dbz = (u32)(b3 - b1);
    s32 den = (s32)(imul(dax, dbz) - imul(dbx, daz));
    if (!den) return 0;
    u32 e1 = (u32)(a1 - b1), e0 = (u32)(a0 - b0);
    s32 t = (s32)(imul(e1, dbx) - imul(e0, dbz)), u = (s32)(imul(e1, dax) - imul(e0, daz));
    if (!in_range(t, den) || !in_range(u, den)) return 0;
    t >>= 10; den >>= 10;
    if (!den) den = 1;
    s32 f = (s32)((u32)t << 10) / den;
    U16(out) = (u16)(((s32)imul(dax, (u32)f) >> 10) + a0);
    U16(out + 4) = (u16)(((s32)imul(daz, (u32)f) >> 10) + a1);
    return 1;
}

/* wall segment s, circle p (+6 radius): v = how far to push p out (x, 0, z),
 * out = the radius vector toward the wall's outside */
u32 DC2CALL Wall_Push(gptr s, gptr p, gptr v, gptr out)            /* 0x4541F0 */
{
    s32 s0 = S16(s), s1 = S16(s + 2), s2 = S16(s + 4), s3 = S16(s + 6);
    s32 r4 = S16(p + 6) + 4;
    u32 a = (u32)(s3 - s1 + 1), b = (u32)(s0 - s2 + 1);
    u32 c = imul((u32)S16(p + 4), b) - imul((u32)s0, (u32)s3) + imul((u32)S16(p), a) + imul((u32)s2, (u32)s1);
    s32 n = SquareRoot0((s32)(imul(b, b) + imul(a, a)));
    s32 ac = (s32)((s32)c < 0 ? 0u - c : c);
    s32 d = ac / n;
    s32 push = (s32)c >= 0 ? d + r4 : r4 - d;
    U16(v + 4) = 0; U16(v) = 0;
    U16(v + 2) = (u16)((Angle_Seg(s) - 0x400) & 0xFFF);
    gptr m = Tmp_Push(0x20);
    RotMatrix(v, m);
    U16(v + 2) = 0; U16(v + 4) = (u16)r4;
    ApplyMatrixSV(m, v, out);
    U16(v + 4) = (u16)push;
    ApplyMatrixSV(m, v, v);
    Tmp_Pop(0x20);
    return 1;
}

/* Room walls: cylinder o+0x128 against the wall polygons of G+0xA4C.
 * Collision block: +0 polygon count, +8 offsets of the polygons.
 * Polygon: +1 edge count n, +3 kind mask, +4 layer bits (tested against bit
 * o+0x5A), +8 n edge entries (+0 flags: bit 0 solid, bit 7 not for objects
 * with flag 0x40; +1 kind mask), then n+1 vertices (x, z).
 * Objects are pushed out of every edge they touch; two touched edges are
 * resolved as a corner. Edge contacts of the first pass are recorded at
 * 0x6F0000 (0x20 bytes each: +0 edge, +2 crossed, +4 edge, +0xC edge moved
 * out by the radius, +0x14 moved in, +0x1C push).
 * The original keeps its state in a stack frame; F() mirrors that frame. */
#define FR8(x)   U8(fr + (x))
#define FR16(x)  U16(fr + (x))
#define FRS16(x) S16(fr + (x))
#define FR32(x)  U32(fr + (x))
#define REC0 0x6F0000u
#define REC1 0x6F0020u

/* 2D cross product y of (p - c) and (w - c) as the corner tests compute it */
static s32 corner_y(gptr pos, gptr c, gptr w)
{
    u32 ux = (u32)(S16(pos) - S16(c)), uz = (u32)(S16(pos + 4) - S16(c + 2));
    u32 wx = (u32)(S16(w) - S16(c)), wz = (u32)(S16(w + 2) - S16(c + 2));
    return (s32)(imul(wx, uz) - imul(ux, wz));
}

u32 DC2CALL Obj_CollideWalls(gptr o)                               /* 0x453430 */
{
    gptr c = U32(o + 0x128);
    if (!c) return 0;
    gptr fr = Tmp_Push(0xC8), pos = o + 0x40;
    u32 ret = 0;
    u8 mask = o != U32(G() + 0x940) ? 0x40 : 0x80;
    if (U32(o + 0x50) & 0x1000) mask = 0x20;
    int moved = U8(c + 0xA) != 0xFF || (U32(o + 0x50) & 0x100);
    if (moved) {                                    /* test at the part's point */
        gptr part = Obj_Part(U32(o + 0x64), U8(c + 0xA) != 0xFF ? U8(c + 0xA) : 0);
        FR32(0x78) = U32(o + 0x40); FR32(0x7C) = U32(o + 0x44);
        FR32(0x80) = U32(o + 0xE0); FR32(0x84) = U32(o + 0xE4);
        U16(o + 0xE0) = U16(part + 0x6C); U16(o + 0xE2) = U16(part + 0x6E); U16(o + 0xE4) = U16(part + 0x70);
        RotMatrix(o + 0x48, fr + 0xA8);
        ApplyMatrixSV(part + 0x20, U32(o + 0x128), pos);
        U16(o + 0x40) += U16(part + 0x40); U16(o + 0x42) += U16(part + 0x42); U16(o + 0x44) += U16(part + 0x44);
    }
    c = U32(o + 0x128);
    U16(o + 0x46) = U16(c + 6);
    U32(o + 0xE8) = U32(pos); U16(o + 0xEC) = U16(o + 0x44);
    for (u32 k = 0; k < U8(U32(G() + 0xA4C)); k++) {
        gptr coll = U32(G() + 0xA4C);
        gptr poly = coll + U32(coll + 8 + 4 * k);
        if (!(((u32)U16(poly + 4) >> (U8(o + 0x5A) & 31)) & 1)) continue;
        if (mask & U8(poly + 3)) continue;
        FR8(0x1F) = 0; FR8(0x38) = 0; FR8(0x13) = 0; FR8(0x1D) = 0xFF;
        gptr rec = REC0;
    restart:;
        u32 n = U8(poly + 1);
        gptr vtx = poly + 8 + n * 4, ef = poly + 8;
        FR16(0x44) = U16(o + 0xE0); FR16(0x46) = U16(o + 0xE4);
        FR16(0x48) = U16(pos); FR16(0x4A) = U16(o + 0x44);
        for (u32 j = 0; j < U8(poly + 1); j++, ef += 4, vtx += 4) {
            FR32(0x28) = U32(vtx); FR32(0x2C) = U32(vtx + 4);
            u8 fl = U8(ef);
            if (!(fl & 1)) continue;
            if ((fl & 0x80) && (U8(o + 0x50) & 0x40)) continue;
            if (U8(ef + 1) & mask) continue;
            if ((s32)(s8)FR8(0x1D) == (s32)j) continue;
            if (Seg_Intersect(fr + 0x28, fr + 0x44) && Seg_Side(fr + 0x44, fr + 0x2C, fr + 0x28)) {
                if (U8(o + 0x184) & 2) { ret = 1; goto out_now; }
                Seg_Cross(fr + 0x28, fr + 0x44, fr + 0x14);
            } else {
                if (!Circle_Seg(fr + 0x28, pos)) continue;
                if (!FR8(0x13)) {                    /* first pass: record the contact */
                    U16(rec + 2) = Seg_Side(fr + 0x44, fr + 0x2C, fr + 0x28) != 0;
                    Wall_Push(fr + 0x28, pos, fr + 0x14, fr + 0x20);
                    U16(rec + 0x1C) = FR16(0x14); U16(rec + 0x1E) = FR16(0x18);
                    U16(rec) = (u16)j;
                    U32(rec + 4) = FR32(0x28); U32(rec + 8) = FR32(0x2C);
                    U16(rec + 0xC) = FR16(0x20) + FR16(0x28); U16(rec + 0xE) = FR16(0x24) + FR16(0x2A);
                    U16(rec + 0x10) = FR16(0x20) + FR16(0x2C); U16(rec + 0x12) = FR16(0x24) + FR16(0x2E);
                    U16(rec + 0x14) = FR16(0x28) - FR16(0x20); U16(rec + 0x16) = FR16(0x2A) - FR16(0x24);
                    U16(rec + 0x18) = FR16(0x2C) - FR16(0x20); U16(rec + 0x1A) = FR16(0x2E) - FR16(0x24);
                    rec += 0x20;
                    FR8(0x38)++;
                    continue;
                }
                if (!Seg_Side(fr + 0x44, fr + 0x2C, fr + 0x28)) continue;
                FR8(0x38)++;
            }
            FR8(0x13) = 1;
            if (FR8(0x1F)) {                         /* second wall: stop at the corner */
                Wall_Push(fr + 0x28, pos, fr + 0x14, fr + 0x20);
                FR16(0x14) = FR16(0x20) + FR16(0x28); FR16(0x16) = FR16(0x24) + FR16(0x2A);
                FR16(0x18) = FR16(0x20) + FR16(0x2C); FR16(0x1A) = FR16(0x24) + FR16(0x2E);
                if (Seg_Cross(fr + 0x14, rec + 0xC, fr + 0x14)) { U16(pos) = FR16(0x14); U16(o + 0x44) = FR16(0x18); }
                continue;
            }
            Wall_Push(fr + 0x28, pos, fr + 0x14, fr + 0x20);
            U16(o + 0x44) += FR16(0x18); U16(pos) += FR16(0x14);
            FR8(0x1F) = 1;
            U16(rec + 0xC) = FR16(0x20) + FR16(0x28); U16(rec + 0xE) = FR16(0x24) + FR16(0x2A);
            FR8(0x1D) = (u8)j;
            U16(rec + 0x10) = FR16(0x20) + FR16(0x2C); U16(rec + 0x12) = FR16(0x24) + FR16(0x2E);
            goto restart;
        }
        if (U8(o + 0x184) & 2) {
            if (FR8(0x38)) { ret = FR8(0x38); goto out_now; }
            continue;
        }
        if (!FR8(0x38) || FR8(0x13)) continue;
        if (FR8(0x38) < 2) {                         /* one wall touched */
            if (!U16(REC0 + 2)) continue;
            FR8(0x1D) = U8(REC0);
            U16(pos) += U16(REC0 + 0x1C);
            FR8(0x1F) = 1;
            U16(o + 0x44) += U16(REC0 + 0x1E);
            rec = REC0;
            FR8(0x13) = 1;
            goto restart;
        }
        /* two or more: a corner of the first two (in edge order) */
        gptr re, rb, seg;
        if (U16(REC1) == U8(poly + 1) - 1u && !U16(REC0)) { re = REC1; rb = REC0; }
        else { re = REC0; rb = REC1; }
        if (Seg_Cross(re + 0xC, rb + 0xC, fr + 0x14)) {
            FR16(0x16) = FR16(0x18);
            if (Seg_Side(fr + 0x44, fr + 0x14, re + 8)) {
                FR32(0x34) = Angle_Seg(re + 4);
                FR32(0x40) = rb + 4;
                FR32(0x3C) = Angle_Seg(rb + 4);
                u16 d = (u16)((FR32(0x3C) - ((FR32(0x34) - 0x800) & 0xFFF)) & 0xFFF);
                if ((s16)d <= 0x400) { seg = rb + 4; goto push_e; }
                FR16(0x22) = U16(o + 0xE4); FR16(0x20) = U16(o + 0xE0);
                FR16(0x24) = U16(pos); FR16(0x26) = U16(o + 0x44);
                d = (u16)((FR32(0x3C) - Angle_Seg(fr + 0x20)) & 0xFFF);
                if ((s16)d >= 0xC00) goto push_b;
                seg = FR32(0x40);
            push_e:
                U16(pos) += U16(re + 0x1C); U16(o + 0x44) += U16(re + 0x1E);
            check_seg:
                if (Circle_Seg(seg, pos)) { U16(pos) = FR16(0x14); U16(o + 0x44) = FR16(0x18); }
                continue;
            }
            U16(pos) += U16(rb + 0x1C); U16(o + 0x44) += U16(rb + 0x1E);
            seg = re + 4;
            goto check_seg;
        }
        if (U16(re + 2)) {
            if (U16(rb + 2)) {
                s32 y1 = corner_y(pos, re + 8, rb + 0xC), y2 = corner_y(pos, re + 8, re + 0x10);
                if (y1 >= 0 && y2 < 0) goto round;
                Seg_Cross(re + 0x14, rb + 0x14, fr + 0x14);
                FR16(0x16) = FR16(0x18);
                if (Seg_Side(fr + 0x44, fr + 0x14, re + 8)) goto push_b;
                goto push_e2;
            }
            if (corner_y(pos, re + 8, re + 0x10) >= 0) goto push_e2;
        }
        if (!U16(rb + 2)) goto round;
        if (corner_y(pos, re + 8, rb + 0xC) >= 0) goto round;
    push_b:
        U16(pos) += U16(rb + 0x1C); U16(o + 0x44) += U16(rb + 0x1E);
        continue;
    push_e2:
        U16(pos) += U16(re + 0x1C); U16(o + 0x44) += U16(re + 0x1E);
        continue;
    round: {                                        /* round the corner vertex */
            u16 vx = U16(re + 8), vz = U16(re + 0xA);
            u32 dx = (u32)(S16(pos) - (s16)vx), dz = (u32)(S16(o + 0x44) - (s16)vz);
            FR32(0x34) = (u32)SquareRoot0((s32)(imul(dz, dz) + imul(dx, dx)));
            FR16(0x16) = vz; FR16(0x14) = vx; FR16(0x18) = U16(pos); FR16(0x1A) = U16(o + 0x44);
            FR16(0x24) = 0; FR16(0x20) = 0;
            FR16(0x22) = (u16)Angle_Seg(fr + 0x14);
            RotMatrix(fr + 0x20, fr + 0xA8);
            FR32(0x20) = 0;
            FR16(0x24) = (u16)(U16(U32(o + 0x128) + 6) - FR16(0x34) + 4);
            ApplyMatrixSV(fr + 0xA8, fr + 0x20, fr + 0x20);
            U16(pos) += FR16(0x20);
            U16(o + 0x44) += FR16(0x24);
        }
    }
    {
        u16 dx = (u16)(U16(pos) - U16(o + 0xE8)), dy = (u16)(U16(o + 0x42) - U16(o + 0xEA));
        u16 dz = (u16)(U16(o + 0x44) - U16(o + 0xEC));
        U16(o + 0xE8) = dx; U16(o + 0xEA) = dy; U16(o + 0xEC) = dz;
        c = U32(o + 0x128);
        if (U8(c + 0xA) != 0xFF || (U32(o + 0x50) & 0x100)) {
            U16(o + 0x42) = FR16(0x7A) + dy; U16(o + 0x44) = FR16(0x7C) + dz; U16(pos) = FR16(0x78) + dx;
            U16(o + 0xE0) = FR16(0x80); U16(o + 0xE2) = FR16(0x82); U16(o + 0xE4) = FR16(0x84);
        }
        gptr p = U32(o + 0x64);
        do {
            U16(p + 0x40) += U16(o + 0xE8);
            U16(p + 0x44) += U16(o + 0xEC);
            p = U32(p + 0x54);
        } while (p);
    }
out_now:
    Tmp_Pop(0xC8);
    return ret;
}

/* ---- floors ---------------------------------------------------------------- */
/* Count crossings of the vertical ray r (x, z, x, 0x7CFF) with the edges of
 * polygon poly; a vertex exactly on the ray is passed over to the next
 * vertex that is not (odd: inside). */
u32 DC2CALL Poly_RayCount(gptr poly, gptr r)                       /* 0x454870 */
{
    u32 count = 0;
    s32 n = U8(poly + 1), left = n, tries = 2;
    gptr v = poly + n * 4 + 8, s = Tmp_Push(8);
    for (;;) {
        U16(s) = U16(v); v += 4; U16(s + 2) = U16(v - 2);
        U16(s + 4) = U16(v); U16(s + 6) = U16(v + 2);
        left--;
        if (Seg_Intersect(r, s)) {
            u16 x = U16(r);
            if (U16(s) == x) goto next;
            u16 x1 = U16(s + 4);
            if (x1 == x) {
                while (tries) {
                    if (!left) { tries--; left = n; v = poly + n * 4 + 8; }
                    x1 = U16(v + 4); v += 4; U16(s + 4) = x1;
                    left--; U16(s + 6) = U16(v + 2);
                    if (x1 != x) break;
                }
                if ((s16)U16(s) > (s16)x) { if ((s16)x1 >= (s16)x) goto checked; }
                else if ((s16)x1 <= (s16)x) goto checked;
            }
            count++;
        }
    checked:
        if (tries != 2) break;
    next:
        if (!left) break;
    }
    Tmp_Pop(8);
    return count & 0xFFFF;
}

/* Height of the floor (probe below, a.y >= b.y) or ceiling above point b on
 * the line a -> b: the nearest floor polygon of G+0xA4C whose layers the
 * line passes; kind of the polygon (+3) into *kind. 0 / -32000 if none.
 * Polygon +0: bit 7 slope (bits 4-5 its direction), bit 6 flat at its
 * lowest layer; +4 layer bits (layer k: y -1000k .. -1000(k+1)); +6 low
 * nibble: floor raised by 100 each. */
u32 DC2CALL Floor_Height(gptr a, gptr b, u32 mask, gptr kind)      /* 0x454420 */
{
    int down = S16(a + 2) >= S16(b + 2);
    if (kind) U32(kind) = 0;
    u16 best = down ? 0x7D00 : 0x8300;
    gptr t = Tmp_Push(0x10), L = t, M = t + 8;
    gptr coll = U32(G() + 0xA4C);
    for (s32 k = U8(coll) - 1; ; k--) {
        coll = U32(G() + 0xA4C);
        gptr poly = coll + U32(coll + 8 + 4 * k);
        u8 ty = U8(poly);
        if (ty && !(U8(poly + 3) & ((u8)mask | 1))) {
            u16 layers = U16(poly + 4);
            s32 hi = 15, lo = 0;
            while (!(layers & (1u << hi)) && --hi > 0) ;
            while (!(layers & (1u << lo)) && ++lo < 15) ;
            hi++;
            u16 top = (u16)(-1000 * hi), bot = (u16)(-1000 * lo), h;
            if (ty & 0x80) {                         /* slope: height along x or z */
                int along_x = (ty & 0x10) != 0;
                s16 mx = (s16)0x8300, mn = 0x7CFF;
                gptr c = poly + 8 + U8(poly + 1) * 4 + (along_x ? 0 : 2);
                for (u32 i = 0; i < U8(poly + 1); i++, c += 4) {
                    if (mx < S16(c)) mx = S16(c);
                    if (mn > S16(c)) mn = S16(c);
                }
                int rising = along_x ? (ty & 0x30) == 0x10 : (ty & 0x30) == 0;
                U16(L) = (u16)mx; U16(L + 2) = rising ? top : bot;
                U16(L + 4) = (u16)mn; U16(L + 6) = rising ? bot : top;
                u16 p = along_x ? U16(a) : U16(a + 4);
                U16(M) = p; U16(M + 2) = 0x8300; U16(M + 4) = p; U16(M + 6) = 0x7CFF;
                Seg_Cross(M, L, L);
                h = U16(L + 4);
                if (down) h += (u16)((U8(poly + 6) & 0xF) * 100);
            } else if (ty & 0x40) {
                h = bot;
                if (down) h += (u16)((U8(poly + 6) & 0xF) * 100);
            } else if (down) {
                h = top + (u16)((U8(poly + 6) & 0xF) * 100);
            } else h = bot;
            /* ray through the line a -> b at height h */
            if (U16(a + 2) == U16(b + 2)) {
                U16(L) = U16(a); U16(L + 2) = U16(a + 4); U16(L + 4) = U16(a);
            } else {
                s32 dh = (s16)h - S16(b + 2), dy = S16(a + 2) - S16(b + 2);
                u16 x = (u16)((s32)imul((u32)(S16(a) - S16(b)), (u32)dh) / dy + S16(b));
                U16(L) = x; U16(L + 4) = x;
                U16(L + 2) = (u16)((s32)imul((u32)(S16(a + 4) - S16(b + 4)), (u32)dh) / dy + S16(b + 4));
            }
            U16(L + 6) = 0x7CFF;
            if (Poly_RayCount(poly, L) & 1) {
                if (down ? ((s16)h >= S16(b + 2) && (s16)best > (s16)h)
                         : ((s16)h < S16(b + 2) && (s16)best < (s16)h)) {
                    best = h;
                    if (kind) U32(kind) = U8(poly + 3);
                }
            }
        }
        if (!k) break;
    }
    Tmp_Pop(0x10);
    return best == 0x7D00 ? 0 : best;
}

/* ---- effect slots G+0x944 (0xAC bytes; *0x87DC88 of them) ------------------ */
#define K2_SIZE 0xACu
static gptr k2_at(u32 i) { return U32(G() + 0x944) + i * K2_SIZE; }

/* first free slot from G+0xA70 on */
gptr DC2CALL K2_Alloc(void)                                        /* 0x48DA40 */
{
    u32 i = U32(G() + 0xA70), n = U32(0x87DC88) - i;
    for (gptr p = k2_at(i); n--; p += K2_SIZE)
        if (!(U8(p + 0x50) & 1)) {
            memset(MEM(p), 0, K2_SIZE);
            U32(p + 0x50) = 1;
            return p;
        }
    return 0;
}

/* no free slot: reuse the end of the chain from G+0x852 (links +0x86/+0x87) */
gptr DC2CALL K2_Reuse(void)                                        /* 0x48D480 */
{
    u8 c = U8(G() + 0x852);
    if (c == 0xFF) return 0;
    gptr p = k2_at(c);
    u32 n = U32(0x87DC88);
    for (u32 i = 0;; ) {
        if (i >= n) return 0;
        u8 next = U8(p + 0x86);
        if (next == 0xFF) break;
        p = k2_at(next);
        i++;
    }
    u8 link = U8(p + 0x87);
    gptr src = U32(p + 0x8C), q = k2_at(link);
    if (p == q) U8(G() + 0x852) = 0xFF;
    else { link = U8(q + 0x86); U8(q + 0x86) = 0xFF; }
    if (src && U8(src + 1) == 1) U8(src + 0x18)--;
    memset(MEM(p), 0, K2_SIZE);
    U8(p + 0x87) = link; U8(p + 0x86) = link;
    U8(p + 0x66) = 0x80; U8(p + 0x65) = 0x80; U8(p + 0x64) = 0x80;
    U32(p + 0x50) = 1;
    U8(p + 0x67) = 0x2C;
    return p;
}

/* free slot from the end down to G+0xA70, else K2_Reuse */
gptr DC2CALL K2_AllocBack(void)                                    /* 0x48D580 */
{
    u32 n = U32(0x87DC88), first = U32(G() + 0xA70);
    gptr p = k2_at(n) - K2_SIZE;
    for (u32 left = n - first; left; ) {
        left--;
        if (!(U8(p + 0x50) & 1)) {
            memset(MEM(p), 0, K2_SIZE);
            U32(p + 0x50) = 1;
            U8(p + 0x66) = 0x80; U8(p + 0x65) = 0x80; U8(p + 0x64) = 0x80;
            U8(p + 0x67) = 0x2C;
            u8 idx = (u8)(U8(G() + 0xA70) + (u8)left);
            U8(p + 0x87) = idx; U8(p + 0x86) = idx;
            return p;
        }
        p -= K2_SIZE;
    }
    return K2_Reuse();
}

/* effect kind 6 following object o with data d (init: *0x87F948 [6]) */
gptr DC2CALL Effect_Attach(gptr o, gptr d)                         /* 0x48DA00 */
{
    gptr k = K2_Alloc();
    if (k) {
        U32(k + 0xA0) = o;
        U32(k + 0x98) = d;
        U8(k + 0x58) = 6;
        Orig_Call1(U32(U32(0x87F948) + 0x18), k);
    }
    return k;
}

/* effect kind 13 on object o (blood drops: count a, 0 = 3) */
gptr DC2CALL Effect_Drops(gptr o, u32 a, u32 b)                    /* 0x48DC80 */
{
    gptr k = K2_AllocBack();
    if (k) {
        U8(k + 0x58) = 0xD;
        U16(k + 0x6E) = (u16)a ? (u16)a : 3;
        U8(k + 0x7A) = 5;
        U32(k + 0xA4) = o;
        U16(k + 0x9E) = (u16)b;
        Orig_Call1(U32(U32(0x87F948) + 0x34), k);
    }
    return k;
}
