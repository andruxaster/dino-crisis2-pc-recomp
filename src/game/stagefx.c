/* Area effects G+0x948 (0x34 each) by kind (*0x87DC8C: +1 kind, +3 step):
 * kind 0 a test camera on the objects, kind 1 slot lists (k2.c), kind 2
 * lights (aobj.c), 3.. and the areas' own kinds 16.. .
 * The camera block C = G+0x778: +0 matrix, +0x20 angles, +0x28 target,
 * +0x30 distance (+0x34 its low word), +0x38 eye (see camera.c). */
#include <string.h>
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/pcio.h"
#include "dc2/gte.h"

#define CAM (G() + 0x778)

/* ---- kind 0: a camera on the player and the objects near it (steps 0x71E158) ----------- */

void DC2CALL CamFx(gptr e)                                           /* 0x40E360 */
{
    Orig_Call1(U32(0x71E158 + S8(e + 3) * 4), e);
}

void DC2CALL CamFx_Next(gptr e)                                      /* 0x40E380 */
{
    U8(e + 3)++;
}

/* an object's height: its model's root +0x1E */
u32 DC2CALL CamFx_ObjY(gptr o)                                       /* 0x40E870 */
{
    return U16(U32(o + 0x60) + 0x1E);
}

static int cam_obj(gptr o) { return (U8(o + 0x50) & 1) && S16(o + 0x108) >= 0; }
static gptr objs_end(void) { return U32(G() + 0x940) + U32(0x87F940) * 0x260; }

/* the camera's distance from the screen distance: d * h / 120 within [lo, 15000] */
static s16 cam_dist(gptr a, gptr b, s32 lo)
{
    s32 h = ReadGeomScreen();
    s32 d = (s32)(Camera_Distance(a, b) * (u32)h) / 120;
    return (s16)(d < lo ? lo : d > 15000 ? 15000 : d);
}

/* eye = target + matrix * {dist, 0, 0} */
static void eye_from(gptr c, gptr m, s16 dist)
{
    gptr v = Tmp_Push(8);
    U16(v) = (u16)dist; U16(v + 2) = 0; U16(v + 4) = 0;
    ApplyMatrixSV(m, v, v);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(v));
    U16(c + 0x3A) = (u16)(U16(c + 0x2A) + U16(v + 2));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(v + 4));
    Tmp_Pop(8);
}

/* the matrix from the angles (negated x and y, z 0) */
static void cam_matrix(gptr c, gptr from)
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)-U16(from); U16(r + 2) = (u16)-U16(from + 2); U16(r + 4) = 0;
    if (from == c + 0x20) U16(c + 0x24) = 0;
    RotMatrixZXY(r, c);
    Tmp_Pop(8);
}

static s32 div16(s32 a) { return a / 16; }

/* step 3: the target eases (1/16) to the middle of the player and the objects
   within 6000 of it, the eye at a distance by the farthest of them */
void DC2CALL CamFx_Follow(gptr e)                                    /* 0x40E390 */
{
    gptr c = CAM, pl = U32(G() + 0x940), pp = U32(pl + 0x64);
    if (!U8(e + 3)) {
        U8(e + 3) = 1;
        U16(c + 0x20) = 0xFF00; U16(c + 0x22) = 0;
        U16(c + 0x32) = 0; U16(c + 0x30) = 0; U16(c + 0x34) = 0xFA0;
        cam_matrix(c, c + 0x20);
        U16(c + 0x28) = U16(pp + 0x40); U16(c + 0x2A) = (u16)(U16(pl + 0x42) - 1000); U16(c + 0x2C) = U16(pp + 0x44);
    }
    gptr far = Tmp_Push(8), w = Tmp_Push(8);
    U16(far) = U16(pl + 0x40); U16(far + 2) = U16(pl + 0x42); U16(far + 4) = U16(pl + 0x44);
    u32 best = Camera_Distance(far, c + 0x28);
    s32 sx = S16(pp + 0x40), sy = (s32)S16(pl + 0x42) - 1000, sz = S16(pp + 0x44), n = 1;
    for (gptr o = U32(G() + 0x940) + 0x260; o < objs_end(); o += 0x260) {
        if (!cam_obj(o)) continue;
        gptr op = U32(o + 0x64);
        U16(w) = U16(op + 0x40); U16(w + 2) = (u16)CamFx_ObjY(o); U16(w + 4) = U16(op + 0x44);
        u32 d = Camera_Distance(w, c + 0x28);
        if (d > 6000) continue;
        sx += S16(w); sy += S16(w + 2); sz += S16(w + 4); n++;
        if (best < d) { best = d; U32(far) = U32(w); U32(far + 4) = U32(w + 4); }
    }
    U16(e + 0xC) = (u16)(sx / n); U16(e + 0xE) = (u16)(sy / n); U16(e + 0x10) = (u16)(sz / n);
    eye_from(c, c, cam_dist(far, c + 0x28, 4000));
    cam_matrix(c, c + 0x20);
    u32 d = Camera_Distance(c + 0x38, c + 0x28);
    U16(e + 0x12) = (u16)d;
    u16 dist = (u16)(U16(c + 0x34) + div16((s32)(d & 0xFFFF) - (s32)U16(c + 0x34)));
    U16(c + 0x34) = dist;
    U16(c + 0x28) = (u16)(U16(c + 0x28) + div16(S16(e + 0xC) - S16(c + 0x28)));
    U16(c + 0x2A) = (u16)(U16(c + 0x2A) + div16(S16(e + 0xE) - S16(c + 0x2A)));
    U16(c + 0x2C) = (u16)(U16(c + 0x2C) + div16(S16(e + 0x10) - S16(c + 0x2C)));
    Tmp_Pop(16);
    gptr buf = Tmp_Push(0x10);
    Crt_sprintf1(buf, 0x71E178, dist);
    Text_Ascii(0, 0x64, 0xFF00, buf);
    Tmp_Pop(0x10);
}

/* step 5: behind the player, 3000 back, looking at it */
void DC2CALL CamFx_Behind(gptr e)                                    /* 0x40E710 */
{
    (void)e;
    gptr c = CAM, pl = U32(G() + 0x940), pp = U32(pl + 0x64);
    U16(c + 0x28) = U16(pp + 0x40); U16(c + 0x2A) = (u16)(U16(pl + 0x42) - 1000); U16(c + 0x2C) = U16(pp + 0x44);
    gptr r = Tmp_Push(8), m = Tmp_Push(0x20);
    U16(r) = 0xFF00; U16(r + 2) = U16(pl + 0x4A); U16(r + 4) = 0;
    RotMatrixYXZ(r, m);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0xF448;
    ApplyMatrixSV(m, r, r);
    U16(c + 0x38) = (u16)(U16(r) + U16(c + 0x28));
    U16(c + 0x3A) = (u16)(U16(r + 2) + U16(c + 0x2A));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(r + 4));
    Tmp_Pop(0x28);
    Angles_LookAt(c + 0x28, c + 0x38, c + 0x20);
    cam_matrix(c, c + 0x20);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
}

/* step 6: the target on the player, distance 10000 */
void DC2CALL CamFx_Reset(gptr e)                                     /* 0x40E820 */
{
    (void)e;
    gptr c = CAM, pl = U32(G() + 0x940), pp = U32(pl + 0x64);
    U16(c + 0x28) = U16(pp + 0x40); U16(c + 0x2A) = (u16)(U16(pl + 0x42) - 1000); U16(c + 0x2C) = U16(pp + 0x44);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0; U16(c + 0x34) = 0x2710;
}

/* the middle of the other objects (n of them); none: def */
static s32 objs_mid(gptr out, gptr def)
{
    s32 sx = 0, sy = 0, sz = 0, n = 0;
    for (gptr o = U32(G() + 0x940) + 0x260; o < objs_end(); o += 0x260) {
        if (!cam_obj(o)) continue;
        gptr op = U32(o + 0x64);
        sx += S16(op + 0x40); sy += (s16)CamFx_ObjY(o); sz += S16(op + 0x44); n++;
    }
    if (n) { U16(out) = (u16)(sx / n); U16(out + 2) = (u16)(sy / n); U16(out + 4) = (u16)(sz / n); }
    else { U16(out) = U16(def); U16(out + 2) = U16(def + 2); U16(out + 4) = U16(def + 4); }
    return n;
}

/* step 4: the target halfway between the player and the objects' middle, the
   eye beside them, the camera's matrix turning 1/16 toward the new one */
void DC2CALL CamFx_Between(gptr e)                                   /* 0x40E880 */
{
    (void)e;
    gptr c = CAM, pl = U32(G() + 0x940), pp = U32(pl + 0x64);
    gptr p = Tmp_Push(8), mid = Tmp_Push(8), r = Tmp_Push(8), m = Tmp_Push(0x20), a = Tmp_Push(8);
    U16(p) = U16(pp + 0x40); U16(p + 2) = (u16)CamFx_ObjY(pl); U16(p + 4) = U16(pp + 0x44);
    objs_mid(mid, p);
    U16(c + 0x28) = (u16)((S16(mid) + S16(p)) / 2);
    U16(c + 0x2A) = (u16)((S16(mid + 2) + S16(p + 2)) / 2);
    U16(c + 0x2C) = (u16)((S16(mid + 4) + S16(p + 4)) / 2);
    U16(r) = 0; U16(r + 2) = (u16)Angle_XZ(p, mid); U16(r + 4) = 0xFF00;
    RotMatrixYXZ(r, m);
    s16 dist = cam_dist(p, c + 0x28, 5000);
    U16(r) = (u16)dist; U16(r + 2) = 0; U16(r + 4) = 0;
    ApplyMatrixSV(m, r, r);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(r));
    U16(c + 0x3A) = (u16)(U16(r + 2) + U16(c + 0x2A));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(r + 4));
    Angles_LookAt(c + 0x28, c + 0x38, a);
    Angles_LookAt(c + 0x28, c + 0x38, a);
    U16(r) = (u16)-U16(a); U16(r + 2) = (u16)-U16(a + 2); U16(r + 4) = 0;
    RotMatrixZXY(r, m);
    for (u32 i = 0; i < 9; i++)
        U16(c + 2 * i) = (u16)((((s32)S16(c + 2 * i) * 15 + S16(m + 2 * i)) << 8) / 4096);
    VectorNormalS(c, c);
    Tmp_Pop(0x40);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
}

/* step 7: the target on the objects' middle, the eye 5000 beyond it from
   the player (the eye's height and the target's swapped) */
void DC2CALL CamFx_Over(gptr e)                                      /* 0x40EC60 */
{
    (void)e;
    gptr c = CAM, pl = U32(G() + 0x940), pp = U32(pl + 0x64);
    gptr p = Tmp_Push(8), mid = Tmp_Push(8), r = Tmp_Push(8), m = Tmp_Push(0x20);
    /* (with no other object the original reads its own stack before the
       player's place is put there: zeros here, as under the harness) */
    U16(p) = 0; U16(p + 2) = 0; U16(p + 4) = 0;
    objs_mid(mid, p);
    U16(p) = U16(pp + 0x40); U16(p + 2) = (u16)CamFx_ObjY(pl); U16(p + 4) = U16(pp + 0x44);
    U16(c + 0x28) = U16(mid); U16(c + 0x2A) = U16(mid + 2); U16(c + 0x2C) = U16(mid + 4);
    u16 dx = (u16)(U16(p) - U16(c + 0x28)), dy = (u16)(U16(p + 2) - U16(c + 0x2A)), dz = (u16)(U16(p + 4) - U16(c + 0x2C));
    U16(r) = 0; U16(r + 2) = (u16)Angle_XZ(mid, p); U16(r + 4) = 0;
    RotMatrixYXZ(r, m);
    U16(r) = 0; U16(r + 2) = 0; U16(r + 4) = 0x1388;
    ApplyMatrixSV(m, r, r);
    dx = (u16)(dx + U16(r)); dy = (u16)(dy + U16(r + 2)); dz = (u16)(dz + U16(r + 4));
    Tmp_Pop(0x38);
    u16 ty = U16(c + 0x2A);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + dx);
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + dz);
    U16(c + 0x2A) = (u16)(ty + dy);
    U16(c + 0x3A) = ty;
    Angles_LookAt(c + 0x28, c + 0x38, c + 0x20);
    cam_matrix(c, c + 0x20);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
}

/* ---- area 0 kind 16: the turret (steps 0x71E1A4). The player stays put at
   (0, -300, -17400) and turns with the gun; e+0x12/+0x14 the aim (pitch,
   yaw), +0x18/+0x1A their speed, +0x10 bit 0 the barrel that fires next,
   +0x11 frames to the next shot, +0x16 the gauge, +0x1C a scroll.
   0x746E38..0x746E78: +2 hit points (180), +4 the hit flash, +6 the jitter,
   +8 frames (to 30000), +0xC the score's factor, +0x10/+0x14 a combo,
   +0x1C/+0x20 the nearest target right / left, +0x2C the score given once,
   +0x2E shots, +0x30 the last frame drawn, +0x34.. per object: bit 0 left,
   bit 1 right ----------------------------------------------------------------------- */

#define TU_HP     0x746E3Au
#define TU_FLASH  0x746E3Cu
#define TU_JIT    0x746E3Fu
#define TU_TIME   0x746E40u

/* the screen's rows y..y+h shifted dx to the right (or left) through a VRAM copy */
void DC2CALL Screen_ShiftRows(s32 dx, u32 y, u32 h)                  /* 0x411080 */
{
    s16 d = (s16)dx;
    if (!d) return;
    gptr r = Tmp_Push(8);
    s16 ry = (s16)(U32(0x87DE0C) ? y + 0x100 : y);
    U16(r) = 0; U16(r + 2) = (u16)ry; U16(r + 6) = (u16)h;
    if (d > 0) { U16(r + 4) = (u16)(0x140 - d); Gpu_SetDrawVramOp(U32(0x87DC98), r, (u32)(s32)d, (u32)(s32)ry); }
    else       { U16(r + 4) = (u16)(d + 0x140); Gpu_SetDrawVramOp(U32(0x87DC98), r, 0, (u32)(s32)ry); }
    Tmp_Pop(8);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0x18;
}

/* the enemies on one side of the aim (bit: 2 right, 1 left), the nearest
   (+0x1C8 least) kept in *best */
static void tu_side(gptr best, u32 bit)
{
    gptr g = G();
    s32 yaw = S16(g + 0x79A);
    u32 h = U16(U32(g + 0x7B8) + U8(g + 0x7C4) * 16 + 6);
    s32 a = (rsin(yaw) * (s32)h) / 4096, b = (rcos(yaw) * (s32)h) / 4096;
    if (bit == 2) { a += rcos(yaw) * 120 / 4096; b += rsin(yaw) * -120 / 4096; }
    else          { a += rcos(yaw) * -120 / 4096; b += rsin(yaw) * 120 / 4096; }
    s32 fx = rsin(yaw) * 2000 / 4096, fz = rcos(yaw) * 2000 / 4096;
    gptr v1 = Tmp_Push(12), v2 = Tmp_Push(12), out = Tmp_Push(12);
    S32(v1) = (s16)a; S32(v1 + 4) = 0; S32(v1 + 8) = (s16)b;
    U32(best) = 0;
    for (u32 i = 1; i < U32(0x87F940); i = (u32)(s32)(s16)(i + 1)) {
        gptr objs = U32(G() + 0x940), o = objs + i * 0x260;
        if (!(U8(o + 0x50) & 1) || !(U8(o + 0x184) & 1)) continue;
        S32(v2) = ((s32)S16(o + 0x40) - S16(objs + 0x40) + (s16)fx) / 8;
        S32(v2 + 4) = 0;
        S32(v2 + 8) = ((s32)S16(o + 0x44) - S16(objs + 0x44) + (s16)fz) / 8;
        OuterProduct12(v1, v2, out);
        if (bit == 2 ? S32(out + 4) <= 0 : S32(out + 4) >= 0) continue;
        U8(0x746E6C + i) |= (u8)bit;
        if (!U32(best)) U32(best) = o;
        else if (S16(U32(best) + 0x1C8) > S16(o + 0x1C8)) U32(best) = o;
    }
    Tmp_Pop(36);
}

void DC2CALL Turret_TargetsRight(gptr e)                             /* 0x412060 */
{
    (void)e;
    tu_side(0x746E54, 2);
}

void DC2CALL Turret_TargetsLeft(gptr e)                              /* 0x4122A0 */
{
    (void)e;
    tu_side(0x746E58, 1);
}

void DC2CALL Turret_Targets(gptr e)                                  /* 0x4124E0 */
{
    U32(0x746E6C) = 0; U32(0x746E70) = 0; U32(0x746E74) = 0; U32(0x746E78) = 0;
    Turret_TargetsLeft(e);
    Turret_TargetsRight(e);
}

/* a shot from the barrel by turns (300 to the side of the player), kind 16 */
void DC2CALL Turret_Fire(gptr e)                                     /* 0x412510 */
{
    s32 yaw = S16(G() + 0x79A);
    s32 dx = rcos(yaw) * 300 / 4096, dz = (-(rsin(yaw) * 15) * 20) / 4096;
    gptr k = K2_AllocBack();
    if (!k) return;
    Se_Play(U32(G() + 0x940), 0x66);
    U8(e + 0x10) ^= 1;
    gptr m = Tmp_Push(0x20);
    u16 sc = (U8(e + 0x10) & 1) ? 0x1200 : 0x1000;
    for (u32 i = 0; i < 9; i++) U16(m + 2 * i) = (i % 4) ? 0 : sc;
    RotMatrixY(-(s32)S16(e + 0x14), m);
    RotMatrixX(-25 - (s32)S16(e + 0x12), m);
    U8(k + 0x7A) = 3; U8(k + 0x58) = 0x10;
    gptr pl = U32(G() + 0x940);
    U32(k + 0x40) = U32(pl + 0x40); U32(k + 0x44) = U32(pl + 0x44);
    U16(k + 0x42) = 0xF736;
    if (U8(e + 0x10) & 1) { U16(k + 0x40) = (u16)(U16(pl + 0x40) + dx); U16(k + 0x44) = (u16)(U16(pl + 0x44) + dz); }
    else                  { U16(k + 0x40) = (u16)(U16(pl + 0x40) - dx); U16(k + 0x44) = (u16)(U16(pl + 0x44) - dz); }
    U16(k + 0x7C) = (u16)(S16(m + 0xC) / 2); U16(k + 0x7E) = (u16)(S16(m + 0xE) / 2);
    U16(k + 0x9E) = 0x14;
    U16(k + 0x80) = (u16)(S16(m + 0x10) / 2);
    U16(k + 0x6E) = 0xA; U16(k + 0x6C) = 0xA;
    Tmp_Pop(0x20);
}

/* the sight's frame: two textured halves (tpages 0x436, 0x437, shade c) and
   the dimming around it (two flat quads by c), all on the front table */
static void tu_link(u32 len)
{
    gptr ot = 0x87BC04 + (U32(0x87DE0C) << 12), p = U32(0x87DC98);
    U32(p) = U32(ot) | len << 24; U32(ot) = p & 0xFFFFFF;
}

/* a textured quad as the stack templates of the original build it: (x0, y0)
   to (x1, y1), uv (u0, v0) to (u1, v1), z 0.01; the pads (+0x36, +0x46) are
   bytes of the original's stack (0 here) */
static void tu_ft4(gptr p, u8 rgb, u8 code, float x0, float y0, float x1, float y1,
                   u8 u0, u8 v0, u8 u1, u8 v1, u16 clut, u16 tp)
{
    U8(p + 4) = rgb; U8(p + 5) = rgb; U8(p + 6) = rgb; U8(p + 7) = code;
    for (u32 k = 0; k < 4; k++) {
        gptr v = p + 8 + 0x10 * k;
        F32(v) = (k & 1) ? x1 : x0; F32(v + 4) = (k & 2) ? y1 : y0; F32(v + 8) = 0.01f;
        U8(v + 0xC) = (k & 1) ? u1 : u0; U8(v + 0xD) = (k & 2) ? v1 : v0;
        U16(v + 0xE) = k == 0 ? clut : k == 1 ? tp : 0;
    }
}

/* a flat quad (code 0x2A) over the screen's height, x0 to x1 */
static void tu_quad_f(gptr p, u8 r, u8 g, u8 b, float x0, float x1)
{
    U8(p + 4) = r; U8(p + 5) = g; U8(p + 6) = b; U8(p + 7) = 0x2A;
    for (u32 k = 0; k < 4; k++) {
        gptr v = p + 8 + 0xC * k;
        F32(v) = (k & 1) ? x1 : x0; F32(v + 4) = (k & 2) ? 240.0f : 0.0f; F32(v + 8) = 0.01f;
    }
}

/* the two halves of the screen dimmed (r, g, b; semi-transparent mode 0x40) */
static void tu_dim(u8 r, u8 g, u8 b)
{
    tu_quad_f(U32(0x87DC98), r, g, b, 0.0f, 255.0f);
    tu_link(5); U32(0x87DC98) += 0x38;
    tu_quad_f(U32(0x87DC98), r, g, b, 255.0f, 320.0f);
    tu_link(5); U32(0x87DC98) += 0x38;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x40, 0);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* a textured quad linked on the front table */
static void tu_sprite(u8 rgb, u8 code, float x0, float y0, float x1, float y1,
                      u8 u0, u8 v0, u8 u1, u8 v1, u16 clut, u16 tp)
{
    tu_ft4(U32(0x87DC98), rgb, code, x0, y0, x1, y1, u0, v0, u1, v1, clut, tp);
    tu_link(9); U32(0x87DC98) += 0x48;
}

void DC2CALL Turret_DrawFrame(gptr e, u32 shade)                     /* 0x412D50 */
{
    (void)e;
    u8 c = (u8)shade;
    tu_sprite(c, 0x2E, 0.0f, 0.0f, 256.0f, 256.0f, 0, 0, 0xFF, 0xFF, 0x7E44, 0x436);
    tu_sprite(c, 0x2E, 256.0f, 0.0f, 320.0f, 256.0f, 0, 0, 0x40, 0xFF, 0x7E44, 0x437);
    c = c >= 0x80 ? 0xFC : c >= 0x40 ? (u8)(c << 2) : 0;
    tu_dim(c, c, c);
}

void DC2CALL Turret(gptr e)                                          /* 0x40F220 */
{
    Flag_Set(3, 0, 0);
    Orig_Call1(U32(0x71E1A4 + S8(e + 3) * 4), e);
}

/* step 0: the scene set up (camera, sight distance 0x122, hit points 180) */
void DC2CALL Turret_Init(gptr e)                                     /* 0x40F240 */
{
    gptr g = G(), c = g + 0x778, pl = U32(g + 0x940);
    Flag_Set(1, 0xC, 1);
    Flag_Set(3, 7, 0);
    memset(MEM(0x746E38), 0, 0x11 * 4);
    U32(pl + 0x50) &= ~2u;
    U16(U32(c + 0x40) + U8(c + 0x4C) * 16 + 6) = 0x122;
    Flag_Set(3, 0, 0);
    U8(G() + 0xB4B) = 9;
    SetDepthScale((float)(s32)(0x400u << (U8(G() + 0xB4B) & 31)));
    U8(e + 3)++;
    U16(e + 0x1C) = 0;
    U16(U32(G() + 0x940) + 0x40) = 0;
    U16(U32(G() + 0x940) + 0x44) = 0xBBA4;
    U16(c + 0x30) = 0; U16(c + 0x32) = 0; U16(c + 0x34) = 0xC8; U16(c + 0x20) = 0xFE0;
    U16(c + 0x22) = U16(U32(G() + 0x940) + 0x4A);
    gptr rec = U32(c + 0x40) + U8(c + 0x4C) * 16;
    U16(e + 0xE) = (u16)ratan2(160.0f, (float)(s32)U16(rec + 6));
    U8(e + 0x10) = 0; U8(e + 0x11) = 0;
    U16(e + 0x12) = 0; U16(e + 0x14) = 0; U16(e + 0x18) = 0; U16(e + 0x1A) = 0; U16(e + 0x16) = 0;
    U16(rec + 6) = 0x122;
    SetGeomScreen(0x122);
    U16(0x746E38) = 0xB4; U16(TU_HP) = 0xB4;
    U32(G() + 0xC18) = 0xF;
    Turret_Wait(e);
}

/* step 1: 90 frames before it starts */
void DC2CALL Turret_Wait(gptr e)                                     /* 0x40F3C0 */
{
    Flag_Set(1, 3, 1);
    s16 t = (s16)(U16(TU_TIME) + 1);
    U16(TU_TIME) = (u16)t;
    if (t > 90) {
        U16(TU_TIME) = 0;
        U8(e + 3)++;
        Turret_Run(e);
    }
    U32(e + 4) = 0x40F420;
}

/* the drawing while it waits: the screen's rows jumping, a picture put up at
   frames 1 and 0x55, the sight */
void DC2CALL Turret_DrawWait(gptr e)                                 /* 0x40F420 */
{
    if (U16(TU_TIME) != U16(0x746E68)) {
        if (Rand8() % 3) {
            u32 i = 0;
            if ((Rand8() & 7) + 1 > 0)
                do {
                    s32 y = Rand8() % 0xEF;
                    s32 dx = Rand8() % 0xF - 8;
                    Screen_ShiftRows(dx, (u32)y, (Rand8() & 1) + 1u);
                    i++;
                } while ((s32)i < (s32)(Rand8() & 7) + 1);
        }
        if (U16(TU_TIME) == 1 || U16(TU_TIME) == 0x55) {
            gptr r = Tmp_Push(8);
            U16(r) = 0x1D0; U16(r + 2) = U16(TU_TIME) == 1 ? 0x100 : 0x110; U16(r + 4) = 6; U16(r + 6) = 0x10;
            Gpu_VramCopy(r, 0x1B0, 0x180);
            Tmp_Pop(8);
        }
    }
    Turret_DrawFrame(e, 0x80);
    U16(0x746E68) = U16(TU_TIME);
}

/* the speed toward 0 (a quarter) unless the stick or the pad still pushes it */
static void tu_ease(gptr sp, u32 neg, u32 pos)
{
    s16 v = S16(sp);
    u32 pad = U32(0x87DC08);
    if ((v < 0 && !(pad & neg)) || (v > 0 && !(pad & pos))) U16(sp) = (u16)(v / 4);
}

static void tu_clamp(gptr a, s16 lo, s16 hi)
{
    if (S16(a) > hi) U16(a) = (u16)hi;
    if (S16(a) < lo) U16(a) = (u16)lo;
}

/* step 2: aiming (stick 0x87DC18 angle / 0x87DC1A tilt, or the pad), hits
   taken, firing (0x20, or shots queued), the score at the end */
void DC2CALL Turret_Run(gptr e)                                      /* 0x40F570 */
{
    gptr c = CAM;
    if (S16(TU_FLASH)) {
        s16 f = (s16)(U16(TU_FLASH) - 16);
        U16(TU_FLASH) = (u16)f;
        if (f > 0x30) {
            u8 a = (u8)(f / 2);
            if (a > 0x60) a = 0x60;
            u16 half = (u16)(a >> 1);
            U16(c + 0x20) += (u16)(U16(0x87DE10) % a - half);
            U16(e + 0x12) += (u16)(U16(0x87DE10) % a - half);
            U16(c + 0x22) += (u16)(U16(0x87DE10) % a - half);
            U16(e + 0x14) += (u16)(U16(0x87DE10) % a - half);
        }
    }
    s16 t = (s16)(U16(TU_TIME) + 1);
    U16(TU_TIME) = (u16)t;
    if (t > 30000) U16(TU_TIME) = 30000;
    U32(e + 4) = 0x40FCD0;
    U16(c + 0x2A) = 0xF5D8;
    U16(c + 0x28) = U16(U32(G() + 0x940) + 0x40);
    U16(c + 0x30) = 0;
    U16(c + 0x2C) = U16(U32(G() + 0x940) + 0x44);
    U16(c + 0x32) = 0; U16(c + 0x34) = 0xC8;
    if (U16(e + 0xC) && !U16(0x746E64)) {            /* the end: the score, once */
        U16(0x746E64) = 1;
        s16 hp = S16(TU_HP);
        if (hp == 0xB4) U32(G() + 0xC18) = 0xF;
        else U32(G() + 0xC18) = (u32)(hp * 5 / 180 + 5);
        u32 s = U32(G() + 0xC18) * U32(0x746E44);
        s32 pts = ((s32)(s / 10) / 10) * 10;
        if (pts >= 100000) pts = 99990;
        U32(G() + 0x11F4) += (u32)pts;
        U32(G() + 0x120C) += (u32)pts;
        U32(G() + 0xC14) = (u32)pts;
    }
    U16(0x746E4E)--;
    if (S16(0x746E4E) < 0) { U16(0x746E4C) = 0; U16(0x746E4E) = 0; U32(0x746E48) = 0; }
    gptr pl = U32(G() + 0x940);
    if (Flag_Check(0, 0xB)) U16(TU_HP) = 0xB4;
    if (U8(pl + 0x174) & 1) {                        /* hit */
        if (!U16(TU_FLASH)) {
            Se_Play(0, 0x69);
            U16(TU_FLASH) = 0xF0;
            Shake_Set(0xA, 0, 0x14);
            u8 d = U8(G() + 0x108E);
            U16(TU_HP) += (u16)(d == 0 || d == 1 ? -16 : -42);
            U16(0x746E4C) = 0; U32(0x746E48) = 0;
            if (S16(TU_HP) <= 0) {
                U16(TU_HP) = 0xFFFF;
                Fade_Set(1, 0x3C, 3);
                GameOver_Start(0);
            }
        }
        U16(pl + 0x174) = 0;
        U32(pl + 0x54) = 1;
    }
    if (S16(TU_HP) <= 0 && S16(TU_FLASH) < 0x40) U16(TU_FLASH) = 0x40;
    Turret_Targets(e);
    U16(U32(G() + 0x940) + 0x40) = 0;
    U16(U32(G() + 0x940) + 0x42) = 0xFED4;
    U16(U32(G() + 0x940) + 0x44) = 0xBC08;
    pl = U32(G() + 0x940);
    if (!(U8(pl + 0x54) == 1 && U8(pl + 0x55) == 5 && !U8(pl + 0x56))) {
        U16(pl + 0x4A) = U16(c + 0x22);
        U16(U32(0x87FD60) + 0x30) = (u16)((U16(c + 0x20) - 0x800) & 0xFFF);
    }
    if (S16(TU_FLASH) < 0x14) {
        if (S16(0x87DC1A) > 0x34) {
            s32 ang = S16(0x87DC18), tilt = S16(0x87DC1A);
            U16(e + 0x1A) = (u16)(((rsin(ang) * tilt) << 5) / 128 / 4096);
            U16(e + 0x18) = (u16)((rcos(ang) * tilt * 35) / 128 / 4096);
        } else {
            if (U32(0x87DC08) & 0x8000) U16(e + 0x1A) -= 5;
            if (U32(0x87DC08) & 0x2000) U16(e + 0x1A) += 5;
            if (U32(0x87DC08) & 0x4000) U16(e + 0x18) -= 5;
            if (U32(0x87DC08) & 0x1000) U16(e + 0x18) += 5;
        }
    } else {
        U16(e + 0x1A) = (u16)(S16(e + 0x1A) / 4);
        U16(e + 0x18) = (u16)(S16(e + 0x18) / 4);
    }
    tu_clamp(e + 0x1A, -0x23, 0x23);
    tu_clamp(e + 0x18, -0x1D, 0x1D);
    if (S16(0x87DC1A) <= 0x34) { tu_ease(e + 0x1A, 0x8000, 0x2000); tu_ease(e + 0x18, 0x4000, 0x1000); }
    U16(e + 0x12) += U16(e + 0x18);
    U16(e + 0x14) += U16(e + 0x1A);
    s16 yaw = S16(e + 0x14);
    if (S16(e + 0x12) < -150) U16(e + 0x12) = (u16)-150;
    if (S16(e + 0x12) > 0x154 && S16(e + 0x12) < 0x800) U16(e + 0x12) = 0x154;
    if (yaw > 0x3AC && yaw < 0x800) U16(e + 0x14) = 0x3AC;
    if (S16(e + 0x14) < -0x3AC) U16(e + 0x14) = (u16)-0x3AC;
    s16 cy = S16(c + 0x22);
    if (cy > 0x800) cy = (s16)(cy - 0x1000);
    if (S16(c + 0x20) > 0x50) U16(c + 0x20) = 0x50;
    if (S16(c + 0x20) < S16(e + 0x12) - 0x8C) U16(c + 0x20) = (u16)(U16(e + 0x12) - 0x8C);
    if (S16(c + 0x20) > S16(e + 0x12) + 0x5A) U16(c + 0x20) = (u16)(U16(e + 0x12) + 0x5A);
    if (cy < S16(e + 0x14) - 0x96) U16(c + 0x22) = (u16)(U16(e + 0x14) - 0x96);
    if (cy > S16(e + 0x14) + 0x96) U16(c + 0x22) = (u16)(U16(e + 0x14) + 0x96);
    U16(U32(G() + 0x940) + 0x4A) = U16(e + 0x14);
    for (s32 n = (s8)((Rand8() % 3) / 2 + 1); n > 0; n--) {   /* the jitter */
        U8(TU_JIT)++;
        U16(c + 0x20) += (u16)(s16)S8(0x71E18C + U8(TU_JIT) % 24);
        U16(e + 0x12) += (u16)(s16)S8(0x71E18C + U8(TU_JIT) % 24);
    }
    Camera_LookAt(c + 0x28, c + 0x30, c + 0x20, c + 0x38);
    U16(c + 0x20) &= 0xFFF; U16(c + 0x22) &= 0xFFF; U16(c + 0x24) &= 0xFFF;
    gptr r = Tmp_Push(8);
    U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(r + 4) = U16(c + 0x24);
    RotMatrix(r, c);
    Tmp_Pop(8);
    if (U8(e + 0x11)) U8(e + 0x11)--;
    else if (!U16(TU_FLASH)) {
        u16 q;
        if (U32(0x87DC08) & 0x20) q = 2;
        else q = U16(0x746E66);
        if (q) {
            U16(0x746E66) = (u16)(q - 1);
            Turret_Fire(e);
            U8(e + 0x11) = 4;
        }
    }
    U16(e + 0x1C) -= 4;
    if (S16(e + 0x1C) < 0) U16(e + 0x1C) += 0x40;
    s16 g = (s16)((s32)S16(TU_TIME) * 56 / 4200);
    U16(e + 0x16) = (u16)g;
    if (g < 0) U16(e + 0x16) = 0;
    if (S16(e + 0x16) > 0x38) U16(e + 0x16) = 0x38;
}

/* ---- the turret's drawing (*(e+4) from step 2) ------------------------------------------ */

/* a textured quad linked at the back of the table (z 0.99) */
static void tu_back(float x0, float y0, float x1, float y1, u8 u0, u8 u1, u16 tp)
{
    gptr p = U32(0x87DC98), ot = 0x87CBFC + (U32(0x87DE0C) << 12);
    tu_ft4(p, 0x80, 0x2C, x0, y0, x1, y1, u0, 0, u1, 0xFF, 0x7F00, tp);
    for (u32 k = 0; k < 4; k++) F32(p + 0x10 + 0x10 * k) = 0.99f;
    U32(p) = U32(ot) | 9u << 24; U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) += 0x48;
}

/* a vertex of PC_ClipProject's output into a packet: x, y, z / (0x400 << (G+0xB4B + 1)), u v */
static void cl_vert(gptr p, gptr q)
{
    s32 d = (s32)(0x400u << ((U8(G() + 0xB4B) + 1) & 31));
    U32(p) = U32(q); U32(p + 4) = U32(q + 4);
    F32(p + 8) = (float)((long double)F32(q + 8) / (long double)d);
    U16(p + 0xC) = U16(q + 0xC);
}

/* a packet linked at the front of the table: header len, code word c */
static void cl_link(gptr p, u32 c, u32 len)
{
    gptr ot = 0x87CBFC + (U32(0x87DE0C) << 12);
    U32(p + 4) = c; U16(p + 0x16) = 0x7E45; U16(p + 0x26) = 0x18;
    U32(p) = U32(ot) | len << 24; U32(ot) = p & 0xFFFFFF;
}

/* the clouds: a plane at height -1000 under the camera's matrix, 10 x 10
   textured quads 6000 apart (each 59536 wide), scrolled by e+0x1C in v,
   clipped at the near plane (3, 4 or 5 corners: a triangle, a quad, a quad
   and a triangle) */
void DC2CALL Turret_DrawClouds(gptr e)                               /* 0x410710 */
{
    gptr g = G(), m = g + 0x778;
    gptr sv = Tmp_Push(8), tm = Tmp_Push(0x20), uv = Tmp_Push(8), v = Tmp_Push(0x20), out = Tmp_Push(0x50);
    S16(sv) = (s16)-U16(g + 0x7A0); S16(sv + 2) = (s16)-U16(g + 0x7A2); S16(sv + 4) = (s16)-U16(m + 0x2C);
    ApplyMatrixSV(m, sv, sv);
    S32(tm + 0x14) = S16(m + 0x30) + S16(sv);
    S32(tm + 0x18) = S16(m + 0x32) + S16(sv + 2);
    S32(tm + 0x1C) = (s32)U16(m + 0x34) + S16(sv + 4);
    SetTransMatrix(tm);
    u8 a = U8(e + 0x1C);
    U8(uv) = 0x80; U8(uv + 1) = a; U8(uv + 2) = 0xBF; U8(uv + 3) = a;
    U8(uv + 4) = 0x80; U8(uv + 5) = (u8)(a + 0x40); U8(uv + 6) = 0xBF; U8(uv + 7) = (u8)(a + 0x40);
    for (u32 k = 0; k < 4; k++) U16(v + 8 * k + 2) = (u16)-1000;
    U16(v + 4) = 0x7360; U16(v + 0xC) = 0x7360; U16(v + 0x14) = (u16)-0x7530; U16(v + 0x1C) = (u16)-0x7530;
    SetRotMatrix(m);
    for (u32 i = 0; i < 10; i++) {
        for (u32 k = 0; k < 4; k++) U16(v + 8 * k + 4) += 0x1770;
        U16(v + 0x10) = 0x7360; U16(v) = 0x7360; U16(v + 0x18) = (u16)-0x7530; U16(v + 8) = (u16)-0x7530;
        for (u32 j = 0; j < 10; j++) {
            for (u32 k = 0; k < 4; k++) U16(v + 8 * k) += 0x1770;
            s32 n = PC_ClipProject(v, uv, out, 0, 4);
            gptr p = U32(0x87DC98);
            if (n == 5) {
                cl_vert(p + 8, out); cl_vert(p + 0x18, out + 0x10); cl_vert(p + 0x28, out + 0x40); cl_vert(p + 0x38, out + 0x20);
                cl_link(p, 0x2C808080, 9);
                p += 0x48; U32(0x87DC98) = p;
                cl_vert(p + 8, out + 0x20); cl_vert(p + 0x18, out + 0x30); cl_vert(p + 0x28, out + 0x40);
                cl_link(p, 0x24808080, 7);
                U32(0x87DC98) = p + 0x38;
            } else if (n == 4) {
                cl_vert(p + 8, out); cl_vert(p + 0x18, out + 0x10); cl_vert(p + 0x28, out + 0x30); cl_vert(p + 0x38, out + 0x20);
                cl_link(p, 0x2C808080, 9);
                U32(0x87DC98) = p + 0x48;
            } else if (n == 3) {
                cl_vert(p + 8, out); cl_vert(p + 0x18, out + 0x10); cl_vert(p + 0x28, out + 0x20);
                cl_link(p, 0x24808080, 7);
                U32(0x87DC98) = p + 0x38;
            }
        }
    }
    Tmp_Pop(8 + 0x20 + 8 + 0x20 + 0x50);
}

/* the sky behind: three 256-wide pictures scrolled by the camera's yaw and
   pitch (the middle one, one to the side of it, one mirrored) */
void DC2CALL Turret_DrawSky(gptr e)                                  /* 0x410D10 */
{
    gptr g = G(), rec = U32(g + 0x7B8) + U8(g + 0x7C4) * 16;
    s16 yaw = S16(g + 0x79A);
    if (yaw > 0x800) yaw = (s16)(yaw - 0x1000);
    s16 x = (s16)(0x20 - yaw * 120 / S16(e + 0xE));
    s32 r = ratan2(120.0f, (float)(s32)U16(rec + 6));
    s16 pitch = S16(g + 0x798);
    if (pitch > 0x800) pitch = (s16)(pitch - 0x1000);
    s16 y0 = (s16)(pitch * 125 / (s16)r - 0x71);
    float fy0 = (float)y0, fy1 = (float)(y0 + 0x100);
    tu_back((float)x, fy0, (float)(x + 0x100), fy1, 0, 0xFF, 0x85);
    s16 x2 = (s16)(yaw > 0xC8 ? x + 0x200 : x - 0x100);
    tu_back((float)x2, fy0, (float)(x2 + 0x100), fy1, 0, 0xFF, 0x8E);
    s16 x3 = (s16)(yaw < -0xC8 ? x - 0x200 : x + 0x100);
    tu_back((float)x3, fy0, (float)(x3 + 0x100), fy1, 0xFF, 0, 0x8E);
}

/* a digit 0..9 at x, y (8 x 16) */
void DC2CALL Turret_DrawDigit(u32 d, s32 x, s32 y)                  /* 0x412BA0 */
{
    u8 u = (u8)((u8)(d + 5) << 3);
    float fx = (float)(s16)x, fy = (float)(s16)y;
    tu_sprite(0x80, 0x2C, fx, fy, (float)((s16)x + 8), (float)((s16)y + 16), u, 0x68, (u8)(u + 8), 0x78, 0x7F40, 0x418);
}

/* a number, its last digit at x, the others 8 to the left each */
void DC2CALL Turret_DrawNumber(u32 v, s32 x, s32 y)                 /* 0x412B30 */
{
    u32 d = v % 10, i = 0;
    Turret_DrawDigit(d, x, y);
    v -= d;
    while (v) {
        v /= 10; i++;
        d = v % 10;
        Turret_DrawDigit(d, x - (s32)(i * 8), y);
        v -= d;
    }
}

/* the combo: its count and the points, a label */
void DC2CALL Turret_DrawCombo(gptr e)                                /* 0x4129F0 */
{
    (void)e;
    s16 n = S16(0x746E4C);
    if (!n) return;
    Turret_DrawNumber((u32)(s32)n, n < 10 ? 0x48 : 0x4C, 0xB6);
    Turret_DrawNumber(U32(0x746E48), 0x4C, 0xC8);
    tu_sprite(0x80, 0x2E, 20.0f, 180.0f, 116.0f, 220.0f, 0x20, 0x80, 0x80, 0xA8, 0x7F40, 0x418);
}

/* a bar of 180 points (hp of them full) in palette b (0x7F40 + b + 7),
   blinking by the frame */
void DC2CALL Turret_DrawBar(s32 hp, u32 b)                           /* 0x412790 */
{
    s32 lost = 180 - hp;
    s32 w1 = (lost << 6) / 180, w2 = ((180 - lost) << 6) / 180;
    u8 t = U8(TU_TIME);
    u8 col = (t & 0x10) ? (u8)(((t & 0xF) + 0x40) << 1) : (u8)(0x9E - ((t & 0xF) << 1));
    u16 clut = (u16)((((s32)(b + 7) << 4) >> 4) | 0x7F40);
    tu_sprite(col, 0x2C, (float)((s16)w1 + 0x60), 24.0f, 160.0f, 48.0f, (u8)w1, 0xE7, 0x40, 0xFF, clut, 0x418);
    tu_sprite(col, 0x2C, 160.0f, 24.0f, (float)((s16)w2 + 0xA0), 48.0f, 0x40, 0xE7, (u8)(w2 + 0x40), 0xFF, clut, 0x418);
}

/* the gauge (+0x16 of 0x38): its needle, the dial and the level's number
   (flag 6/0x81: the short dial) */
void DC2CALL Turret_DrawGauge(gptr e)                                /* 0x40FF70 */
{
    s16 w, y;
    s32 kind;
    if (Flag_Check(6, 0x81)) {
        w = (s16)(S16(e + 0x16) * 35 / 56);
        if (w > 0x21) w = 0x21;
        y = (s16)(w >= 0x14 ? (w - 0x14) / 2 : -1);
        kind = w < 9 ? 1 : w < 0x12 ? 2 : 3 + (w >= 0x1A);
    } else {
        w = (s16)(S16(e + 0x16) * 50 / 56);
        if (w > 0x31) { w = 0x31; y = 0xF; }
        else y = (s16)(w < 0xF ? w - 1 : 0xF);
        kind = w < 8 ? 1 : w < 0x10 ? 2 : w < 0x1B ? 3 : 4 + (w >= 0x26);
    }
    if (U8(TU_TIME) & 0x10)
        tu_sprite(0x80, 0x2E, (float)(w + 0xE8), (float)(y + 0xAA), (float)(w + 0x108), (float)(y + 0xCA), 0x70, 0xB8, 0x90, 0xD8, 0x7E82, 0x407);
    tu_sprite(0x80, 0x2E, 196.0f, 143.0f, 308.0f, 223.0f, 0, 0x80, 0x70, 0xD0, 0x7E80, 0x407);
    u8 rgb = (U8(TU_TIME) & 0x10) ? 0x8F : 0x60;
    s32 ex = 0x31, ey = 0xF;
    if (Flag_Check(6, 0x81)) { ex = 0x21; ey = 6; }
    tu_sprite(rgb, 0x2E, (float)(ex + 0xE8), (float)(ey + 0xAA), (float)(ex + 0x108), (float)(ey + 0xCA), 0x70, 0xB8, 0x90, 0xD8, 0x7E82, 0x407);
    u32 v0 = 0x18, h = 0x20;
    if (Flag_Check(6, 0x81)) { v0 = 0; h = 0x18; }
    tu_sprite(0x80, 0x2E, 228.0f, 167.0f, 340.0f, (float)((s32)(s16)h + 0xA7), 0x70, (u8)(v0 + 0x80), 0xE0, (u8)(h + v0 + 0x80),
              (u16)((((kind + 1) << 4) >> 4) | 0x7E80), 0x407);
}

/* a hit: the camera's angles held in, the screen tinted (0, f, f) */
void DC2CALL Turret_DrawHit(gptr e)                                  /* 0x410500 */
{
    (void)e;
    gptr c = CAM;
    u16 f = U16(TU_FLASH);
    if (!f) return;
    if ((s16)f > 0x26) {
        s16 a = S16(c + 0x20);
        if (a < 0x800) { if (a > 0xC8) U16(c + 0x20) = 0xC8; }
        else if (a < 0xF7E) U16(c + 0x20) = 0xF7E;
        a = S16(c + 0x22);
        if (a < 0x800) { if (a > 0x316) U16(c + 0x22) = 0x316; }
        else if (a < 0xCEA) U16(c + 0x22) = 0xCEA;
    }
    tu_dim(0, (u8)f, (u8)f);
}

/* step 2's drawing: the rows jumping while hit, the sight fading in for 90
   frames, a flare every 11 frames, the scene and the panels */
void DC2CALL Turret_Draw(gptr e)                                     /* 0x40FCD0 */
{
    u16 t = U16(TU_TIME);
    if (t != U16(0x746E68) && U16(TU_FLASH)) {
        for (u32 n = 0x30; n; n--) {
            s32 y = Rand8() % 0xEF;
            s32 dx = Rand8() % 0xF - 8;
            Screen_ShiftRows(dx, (u32)y, (Rand8() & 1) + 1u);
        }
        t = U16(TU_TIME);
    }
    if ((s16)t < 0x5A) {
        if (t != U16(0x746E68)) {
            u32 v = t > 0x28 ? 0x28 : t;
            for (s32 n = (s32)(0x5A - v) / 2; n > 0; n--) {
                s32 y = Rand8() % 0xEF;
                s32 dx = Rand8() % 0xF - 8;
                Screen_ShiftRows(dx, (u32)y, (Rand8() & 1) + 1u);
            }
            t = U16(TU_TIME);
        }
        if (t == 2) {
            gptr r = Tmp_Push(8);
            U16(r) = 0x1D0; U16(r + 2) = 0x110; U16(r + 4) = 6; U16(r + 6) = 0x10;
            Gpu_VramCopy(r, 0x1B0, 0x180);
            Tmp_Pop(8);
            t = U16(TU_TIME);
        }
        Turret_DrawFrame(e, (u32)(((0x5A - (s16)t) << 7) / 90));
        t = U16(TU_TIME);
    }
    if ((s16)t % 11 == 0 && t != U16(0x746E68)) {
        gptr k = K2_AllocBack();
        if (k) {
            U8(k + 0x7A) = 7; U8(k + 0x58) = 0x10;
            U16(k + 0x40) = 0xDCD8; U16(k + 0x42) = 0;
            U16(k + 0x7C) = 0; U16(k + 0x7E) = 0;
            U16(k + 0x44) = (u16)(U16(U32(G() + 0x940) + 0x44) + 0x1B58);
            U16(k + 0x80) = 0; U16(k + 0x6E) = 0x6E; U16(k + 0x6C) = 0x122; U16(k + 0x9E) = 0x14;
        }
    }
    Turret_DrawCombo(e);
    Turret_DrawGauge(e);
    Turret_DrawHit(e);
    Turret_DrawClouds(e);
    Turret_DrawSky(e);
    Turret_Hud(e);
    s16 hp = S16(TU_HP), shown = S16(0x746E38);
    if ((s16)U16(TU_TIME) % 3 == 0 && shown > hp) { shown--; U16(0x746E38) = (u16)shown; }
    Turret_DrawBar(hp, shown >= 0x5A ? 0 : (u32)((0x5A - shown) / 22));
    Turret_DrawBar(S16(0x746E38), 3);
    U16(0x746E68) = U16(TU_TIME);
}

/* ---- kind 3: the water surface (steps table 0x71E1C0) ----
 * e+0xC/+0xE x/z of the corner, e+0x12 height /100, e+0x14 yaw, e+0x10/+0x11 tiles
 * across/along, e+0x1A tile size /500, e+0x16 fixed OT depth (0: from z),
 * e+0x18 flags (1 alternate rows, 3 swell, 4 still, 8 animate the clut),
 * e+0x19 tpage, e+0x1B clut row, e+0x1C/+0x1D swell speed/height, e+0x1E scroll */

void DC2CALL Water(gptr e)                                           /* 0x4134C0 */
{
    Orig_Call1(U32(0x71E1C0 + S8(e + 3) * 4), e);
}

void DC2CALL Water_Init(gptr e)                                      /* 0x4134E0 */
{
    U8(e + 3)++;
    U16(e + 0x1E) = 0;
    if (!U8(e + 0x1C)) U8(e + 0x1C) = 5;
    if (!U8(e + 0x1A)) U8(e + 0x1A) = 2;
    U8(e + 0x1B) = 0;
    U32(e + 4) = 0x4135B0;
    Flag_Set(3, 8, 1);
    Water_Step(e);
}

void DC2CALL Water_Step(gptr e)                                      /* 0x413530 */
{
    u8 f = U8(e + 0x18);
    if (f & 3) {
        s8 d = (s8)(U8(e + 0x1D) + U8(e + 0x1C));
        U8(e + 0x1D) = (u8)d;
        if (d > 0x32 || d < -0x32) U8(e + 0x1C) = (u8)-U8(e + 0x1C);
    }
    if (!(f & 4)) {
        U16(e + 0x1E) -= U8(e + 0x13);
        if (S16(e + 0x1E) < 0) U16(e + 0x1E) += 0x2580;
    }
    if (f & 8) U8(e + 0x1B) = (u8)((U8(e + 0x1B) + 1) & 0x3F);
    gptr v = Tmp_Push(8);
    U32(v) = 0; U32(v + 4) = 0;
    Water_Level(v);                     /* (the result is dropped) */
    Tmp_Pop(8);
}

void DC2CALL Water_Draw(gptr e)                                      /* 0x4135B0 */
{
    gptr g = G(), c = g + 0x778;
    gptr t = Tmp_Push(0x70), r = t, m = t + 8, lv = t + 0x28, vt = t + 0x38, z = t + 0x58;
    U16(r) = 0; U16(r + 2) = U16(e + 0x14); U16(r + 4) = 0;
    RotMatrix(r, m);
    MulMatrix0(c, m, m);
    SetRotMatrix(c);
    U32(lv) = (u32)((s32)S16(e + 0xC) - S16(c + 0x28));
    U32(lv + 4) = (u32)((s32)S8(e + 0x12) * 100 - S16(c + 0x2A));
    U32(lv + 8) = (u32)((s32)S16(e + 0xE) - S16(c + 0x2C));
    ApplyRotMatrixLV(lv, m + 0x14);
    U32(m + 0x14) += (u32)(s32)S16(c + 0x30);
    U32(m + 0x18) += (u32)(s32)S16(c + 0x32);
    U32(m + 0x1C) += U16(c + 0x34);
    SetTransMatrix(m);
    s32 zfix = S16(e + 0x16);
    u32 otz = (u32)zfix;
    u16 step = (u16)(U8(e + 0x1A) * 500);
    s32 nr = S8(e + 0x11), nc = S8(e + 0x10);
    s16 y = S8(e + 0x1D);
    u32 v0 = (u32)(S16(e + 0x1E) / 50), v1 = v0 + 0x40;
    u32 sh = (U8(g + 0xB4B) - 2) & 31;
    U16(vt + 6) = 0; U16(vt + 0xE) = 0; U16(vt + 0x16) = 0;   /* (the pads: gte_ldv3c loads them) */
    for (s32 i = 0; i < nr; i++) {
        v0 += 0x40; v1 += 0x40;
        if (v0 > 0xBF) { v0 -= 0xC0; v1 -= 0xC0; }
        s16 y2 = y;
        if (U8(e + 0x18) & 1) { y2 = (s16)-y; }
        U16(vt + 2) = U16(vt + 0xA) = (u16)y;
        U16(vt + 0x12) = U16(vt + 0x1A) = (u16)y2;
        U16(vt + 4) = U16(vt + 0xC) = (u16)(i * step);
        U16(vt + 0x14) = U16(vt + 0x1C) = (u16)((i + 1) * step);
        if (U8(e + 0x18) & 1) y = y2;
        for (s32 j = 0; j < nc; j++) {
            U16(vt) = U16(vt + 0x10) = (u16)(j * step);
            U16(vt + 8) = U16(vt + 0x18) = (u16)((j + 1) * step);
            gptr p = U32(0x87DC98);
            SetRotMatrix(m);
            gte_ldv3c(vt); gte_rtpt(); gte_stsxy3_g3(p);
            gte_ldv0(vt + 0x18); gte_rtps(); gte_stsxy(p + 0x38);
            PC_stsz4_g4(p);
            if (!zfix) {
                gte_stsz4(z);
                u32 a = U32(z);
                for (u32 q = 1; q < 4; q++) if (a > U32(z + 4 * q)) a = U32(z + 4 * q);
                a >>= sh;
                otz = a >= 0x3FF ? 0x3FF : a;
            }
            U8(p + 0x34) = 0; U8(p + 0x14) = 0; U8(p + 0x44) = 0x40; U8(p + 0x24) = 0x40;
            U8(p + 0x25) = (u8)v0; U8(p + 0x15) = (u8)v0; U8(p + 0x45) = (u8)v1; U8(p + 0x35) = (u8)v1;
            U32(p + 4) = 0x2E808080;
            U16(p + 0x26) = (u16)(((u16)(s16)S8(e + 0x19) << 5) | 0x417);
            U16(p + 0x16) = (u16)((U8(e + 0x1B) >> 2) + 0x7E80);
            gptr ot = 0x87BC00 + ((U32(0x87DE0C) << 10) + otz) * 4;
            U32(p) = U32(ot) | 0x09000000;
            U32(ot) = p & 0xFFFFFF;
            U32(0x87DC98) = p + 0x48;
        }
    }
    Tmp_Pop(0x70);
}

/* ---- kind 4: a burst of effects from a script (0x71E1D0 + e+2 * 0x2A8, entries 0x28) ----
 * entry +0xF the frame (e+0x1C) it fires on, -1 at +0 ends it; e+0xC/+0xE/+0x10 the place,
 * e+0x14 the rotation applied to the speed for script 1 */
void DC2CALL FxBurst(gptr e)                                         /* 0x413B60 */
{
    gptr d = 0x71E1D0 + U8(e + 2) * 0x2A8;
    if ((s16)U8(d + 0xF) <= S16(e + 0x1C)) {
        do {
            if (U32(d) == 0xFFFFFFFF) { Eff_Off(e); break; }
            if (U8(d + 0xF) == U16(e + 0x1C)) {
                gptr s = K2_AllocBack();
                if (!s) break;
                U8(s + 0x58) = 4;
                U16(s + 0x6C) = U8(d + 1); U16(s + 0x6E) = U8(d + 2); U8(s + 0x84) = U8(d + 3);
                U16(s + 0x64) = U16(d + 4); U8(s + 0x66) = U8(d + 6);
                U32(s + 0x40) = U32(d + 8); U32(s + 0x44) = U32(d + 0xC);
                U16(s + 0x40) += U16(e + 0xC); U16(s + 0x42) += U16(e + 0xE); U16(s + 0x44) += U16(e + 0x10);
                U32(s + 0x7C) = U32(d + 0x10); U32(s + 0x80) = U32(d + 0x14);
                U16(s + 0x7A) = U16(d + 0x1C);
                U32(s + 0x98) = U32(d + 0x20); U32(s + 0x9C) = U32(d + 0x24);
                if (U8(e + 2) == 1) {
                    gptr m = Tmp_Push(0x20);
                    RotMatrix(e + 0x14, m);
                    ApplyMatrixSV(m, s + 0x7C, s + 0x7C);
                    Tmp_Pop(0x20);
                    U16(s + 0x9C) += U16(e + 0x16);
                }
            }
            d += 0x28;
        } while ((s16)U8(d + 0xF) <= S16(e + 0x1C));
    }
    U16(e + 0x1C)++;
}

/* ---- kind 5: the flamethrower (the "light" slot of Light_Reset; steps table 0x71EFE8) ----
 * 32 flame records at 0x748160 (0x48 bytes): +0 MATRIX (its t the view position),
 * +0x20 position, +0x28 speed, +0x2E frames left of the picture, +0x30/+0x32 size,
 * +0x34 flags (1 moving, 2 drawn), +0x38 reach left, +0x3A stop at the end,
 * +0x3B hits, +0x3C/+0x40 the last x/z, +0x3E the reach used per frame, +0x44 picture list.
 * The slot: +0xC picture list, +0x10 the nozzle part, +0x14 reach, +0x18 light reach,
 * +0x1C frame, +0x1D next record, +0x1E script entry (0x71EE90, 0x14 bytes), +0x20 smoke
 * toggle, +0x21 firing, +0x23 sound toggle; 0x71EFE4 the sound countdown.
 * The light G+0xC: +0/+1 colour, +3 on, +4 position, +0xA reach, +0xE strength. */
#define FL_BEG  0x748160u
#define FL_END  0x748A60u

void DC2CALL Flame(gptr e)                                           /* 0x41BE20 */
{
    gptr g = G(), pl = U32(g + 0x940);
    if ((U8(g + 0x10B4) & 0xF) != 5) U8(e + 3) = 2;
    u32 fire = 0;
    if (U8(e + 0x21)) {
        u8 a = U8(pl + 0x55), b = U8(pl + 0x56);
        if (a == 6 || (a == 5 && (b == 5 || b == 6))) fire = 1;
    }
    if (!fire) {
        U8(e + 0x1C) = 0; U8(e + 0x1E) = 0; U8(e + 0x21) = 0;
        U8(0x71EFE4) = 1;
    }
    Orig_Call1(U32(0x71EFE8 + S8(e + 3) * 4), e);
    if (fire) { Flame_Aim(e); Flame_Emit(e); }
}

void DC2CALL Flame_Init(gptr e)                                      /* 0x41BEA0 */
{
    gptr g = G();
    U32(e + 0xC) = 0x682398;
    U32(FL_BEG) = 0;
    U32(e + 0x10) = Obj_Part(U32(U32(g + 0x940) + 0x64), 3);
    U8(e + 3)++;
    U32(e + 4) = 0x41C350;
    for (gptr a = FL_BEG; a < FL_END; a += 0x48) {
        U16(a + 0x10) = 0x1000; U16(a + 8) = 0x1000; U16(a) = 0x1000;
        U16(a + 0xE) = 0; U16(a + 0xC) = 0; U16(a + 0xA) = 0; U16(a + 6) = 0; U16(a + 4) = 0; U16(a + 2) = 0;
    }
    g = G();
    U8(g + 0xD) = 0; U8(g + 0xE) = 0;
    U16(g + 0x1A) = 0x1000; U16(g + 0x16) = 0x7D0;
}

/* this frame's script entries: new flames from the nozzle */
void DC2CALL Flame_Emit(gptr e)                                      /* 0x41BF40 */
{
    gptr pl = U32(G() + 0x940);
    gptr s = 0x71EE90 + S8(e + 0x1E) * 0x14;
    gptr m = U32(e + 0x10) + 0x20;
    while (U8(s + 6) == (s32)S8(e + 0x1C)) {
        gptr r = FL_BEG + S8(e + 0x1D) * 0x48;
        if (!U8(e + 0x1E)) { U8(0x71EFE4)--; U8(r + 0x3B) = 1; }
        else U8(r + 0x3B) = U8(e + 0x1E) == 8;
        U8(e + 0x1D) = (u8)((U8(e + 0x1D) + 1) & 0x1F);
        memcpy(MEM(r + 0x20), MEM(s), 0x14);
        U16(r + 0x3E) = U16(r + 0x2A);
        ApplyMatrixSV(m, r + 0x20, r + 0x20);
        gptr p = U32(e + 0x10);
        U16(r + 0x20) += U16(p + 0x40); U16(r + 0x22) += U16(p + 0x42); U16(r + 0x24) += U16(p + 0x44);
        ApplyMatrixSV(m, r + 0x28, r + 0x28);
        s16 left = (s16)(U16(e + 0x14) - (u16)Sqrt_Refined(Dist2_XZ(pl + 0x40, r + 0x20)));
        U16(r + 0x38) = (u16)left;
        U8(r + 0x3A) = (u8)(U32(e + 0x14) >> 31);
        if (left > 0 || !U8(e + 0x1E)) {
            if (left <= 0) {
                U16(r + 0x3E) = 0; U16(r + 0x38) = 0; U32(r + 0x28) = 0; U16(r + 0x2C) = 0;
            }
            U32(r + 0x34) = 3;
            U32(r + 0x44) = U32(e + 0xC);
            U16(r + 0x2E) = U8(U32(e + 0xC) + 5);
        }
        s += 0x14;
        if (U8(s + 6) == 0xFF) {
            U8(e + 0x21) = 0; U8(e + 0x1E) = 0; U8(e + 0x1C) = 0xFE;
            break;
        }
        U8(e + 0x1E)++;
    }
    U8(e + 0x1C)++;
    if (!U8(0x71EFE4)) {
        U8(e + 0x23) ^= 1;
        Se_Play(U32(G() + 0x940), U32(0x71EFF4 + U8(e + 0x23) * 4));
        U8(0x71EFE4) = 2;
    }
}

/* the flames move, light the room, burn what they touch and end in smoke */
void DC2CALL Flame_Step(gptr e)                                      /* 0x41C110 */
{
    gptr g = G(), l = g + 0xC, pl = U32(g + 0x940), p = U32(e + 0x10);
    gptr t = Tmp_Push(0x28), v = t, m = t + 8;
    U32(v) = 0; U16(v + 4) = 0x320;
    RotMatrixYXZ(pl + 0x48, m);
    ApplyMatrixSV(m, v, v);
    U8(l + 3) = 0;
    U16(l + 4) = (u16)(U16(p + 0x40) + U16(v));
    U16(l + 6) = (u16)(U16(p + 0x42) + U16(v + 2));
    U16(l + 8) = (u16)(U16(p + 0x44) + U16(v + 4));
    Tmp_Pop(0x28);
    U8(l) = 0; U8(l + 1) = 0;
    u32 lit = 0;
    for (gptr r = FL_BEG; r < FL_END; r += 0x48) {
        if (!(U8(r + 0x34) & 1)) continue;
        s32 end = 0;
        u16 n = U16(r + 0x2E);
        if (n == 1) end = S8(U32(r + 0x44) + 4) >> 5;
        else if (n == 0) {
            gptr a = U32(r + 0x44);
            if (!(U8(a + 4) & 0xE0)) {
                a += (U8(a + 4) & 0x1F) * 8;
                U32(r + 0x44) = a;
                U16(r + 0x2E) = U8(a + 5);
                if (U8(a + 5) == 1) end = S8(a + 4) >> 5;
            }
        }
        U16(r + 0x2E)--;
        if (end) { U32(r + 0x34) = 0; continue; }
        U16(r + 0x3C) = U16(r + 0x20);
        U16(r + 0x40) = U16(r + 0x24);
        U16(r + 0x20) += U16(r + 0x28);
        U16(r + 0x22) += U16(r + 0x2A);
        U16(r + 0x24) += U16(r + 0x2C);
        U8(l) += 6; U8(l + 1) += 3; U8(l + 3) = 1;
        lit++;
    }
    for (gptr r = FL_BEG; r < FL_END; r += 0x48)
        if ((U8(r + 0x34) & 1) && (U8(r + 0x3B) & 1)) Weapon_Hit(r, r + 0x20, 0x1B);
    if (!lit) U16(e + 0x18) = 1;
    else U16(e + 0x18) += 0xDC;
    if (!U8(e + 0x21) && !lit) U8(0x71EFE4) = 1;
    for (gptr r = FL_BEG; r < FL_END; r += 0x48) {
        if (!U32(r + 0x34)) continue;
        U16(r + 0x38) -= U16(r + 0x3E);
        if (S16(r + 0x38) >= 0) continue;
        if (U8(r + 0x3A)) { U32(r + 0x34) = 0; continue; }
        U32(r + 0x28) = 0; U16(r + 0x2C) = 0; U16(r + 0x3E) = 0; U16(r + 0x38) = 0;
        U8(e + 0x20) = (u8)((U8(e + 0x20) - 1) & 1);
        if (U8(e + 0x20)) continue;
        gptr s = K2_AllocBack();
        if (!s) continue;
        U8(s + 0x58) = 1; U8(s + 0x7A) = 1;
        U16(s + 0x6E) = 0xF; U16(s + 0x6C) = 0xF;
        U16(s + 0x7E) = (u16)(-0x19 - (Rand8() & 0x1F));
        U32(s + 0x40) = U32(r + 0x20); U16(s + 0x44) = U16(r + 0x24);
    }
}

void DC2CALL Flame_Draw(gptr e)                                      /* 0x41C350 */
{
    (void)e;
    gptr g = G(), c = g + 0x778;
    u32 sh = (U8(g + 0xB4B) - 2) & 31;
    s16 cx = S16(c + 0x28), cy = S16(c + 0x2A), cz = S16(c + 0x2C);
    gptr t = Tmp_Push(0x60), lv = t, m = t + 0x10, v = t + 0x30, a = t + 0x38, b = t + 0x40, z = t + 0x48;
    SetRotMatrix(c);
    for (gptr r = FL_BEG; r < FL_END; r += 0x48) {
        if (!(U8(r + 0x34) & 1)) continue;
        U32(lv) = (u32)((s32)S16(r + 0x20) - cx);
        U32(lv + 4) = (u32)((s32)S16(r + 0x22) - cy);
        U32(lv + 8) = (u32)((s32)S16(r + 0x24) - cz);
        ApplyRotMatrixLV(lv, r + 0x14);
        U32(r + 0x14) += (u32)(s32)S16(c + 0x30);
        U32(r + 0x18) += (u32)(s32)S16(c + 0x32);
        U32(r + 0x1C) += U16(c + 0x34);
    }
    for (u32 i = 0; i < 0x10; i += 2) U16(m + i) = 0;
    U16(m) = 0x1000; U16(m + 8) = 0x1000; U16(m + 0x10) = 0x1000; U16(m + 0x12) = 0;
    SetRotMatrix(m);
    for (gptr r = FL_BEG; r < FL_END; r += 0x48) {
        if (!(U8(r + 0x34) & 2)) continue;
        SetTransMatrix(r);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0;
        gte_ldv0(v); gte_rtps(); gte_stsxy(a);
        gptr f = U32(r + 0x44);
        U16(v) = (u16)((u16)(s16)S8(f + 6) * U16(r + 0x30));
        U16(v + 2) = (u16)((u16)(s16)S8(f + 7) * U16(r + 0x32));
        U16(v + 4) = 0;
        gte_ldv0(v); gte_rtps(); gte_stsxy(b);
        float x0 = F32(a), y0 = F32(a + 4);
        u32 dx = (u32)(s32)(int64_t)((long double)x0 - F32(b));
        u32 dy = (u32)(s32)(int64_t)((long double)y0 - F32(b + 4));
        gptr p = U32(0x87DC98);
        F32(p + 0x28) = (float)((long double)x0 - (long double)dx);
        U32(p + 8) = U32(p + 0x28);
        F32(p + 0x38) = (float)((long double)dx + (long double)x0);
        U32(p + 0x18) = U32(p + 0x38);
        F32(p + 0x1C) = (float)((long double)y0 - (long double)dy);
        U32(p + 0xC) = U32(p + 0x1C);
        F32(p + 0x3C) = (float)((long double)dy + (long double)y0);
        U32(p + 0x2C) = U32(p + 0x3C);
        PC_stsz_g4_flat(p);
        u8 u = U8(f), w = U8(f + 1);
        U8(p + 0x34) = u; U8(p + 0x14) = u; U8(p + 0x25) = w; U8(p + 0x15) = w;
        U8(p + 0x44) = (u8)(U8(f + 2) + u - 1); U8(p + 0x24) = U8(p + 0x44);
        U8(p + 0x45) = (u8)(U8(f + 3) + w - 1); U8(p + 0x35) = U8(p + 0x45);
        gte_stszotz(z);
        U16(p + 0x16) = 0x7E0B; U16(p + 0x26) = 0x2A;
        U32(p + 4) = 0x2E808080;
        s32 o = S32(z) >> sh;
        if (o >= 0x3FF) o = 0x3FF;
        gptr ot = 0x87BC00 + ((U32(0x87DE0C) << 10) + (u32)o) * 4;
        U32(p) = U32(ot) | 0x09000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x48;
    }
    Tmp_Pop(0x60);
}

/* the nearest wall (of the player's level) the flame's 3000-long line crosses: its distance, 0 none */
u32 DC2CALL Flame_Ray(gptr o)                                        /* 0x41C770 */
{
    u32 best = 0xFFFFFFFF, found = 0;
    gptr t = Tmp_Push(0x38), sa = t, sb = t + 8, x = t + 0x10, u = t + 0x18, w = t + 0x28;
    U16(sa) = U16(o + 0x74); U16(sa + 2) = U16(o + 0x78); U16(sa + 4) = U16(o + 0x40); U16(sa + 6) = U16(o + 0x44);
    gptr tab = U32(G() + 0xA4C);
    for (u32 i = 0; i < U8(tab); i++, tab = U32(G() + 0xA4C)) {
        gptr gr = U32(tab + 8 + 4 * i) + tab;
        if (!(U16(gr + 4) & (1u << (U8(o + 0x5A) & 31)))) continue;
        u8 f = U8(gr + 3);
        if ((f & 0x10) && (U8(o + 0x85) & 0x10)) continue;
        if ((f & 8) && (U8(o + 0x85) & 8)) continue;
        gptr fl = gr + 8, pt = gr + 8 + U8(gr + 1) * 4;
        for (u32 j = 0; j < U8(gr + 1); j++, fl += 4) {
            U32(sb) = U32(pt); pt += 4; U32(sb + 4) = U32(pt);
            if (!(U8(fl) & 1)) continue;
            if ((U8(fl + 1) & 0x10) && (U8(o + 0x85) & 0x10)) continue;
            if (!Seg_Cross(sb, sa, x)) continue;
            s32 ax = S16(sb), az = S16(sb + 2);
            U32(w) = (u32)(S16(sa) - ax); U32(w + 4) = 0; U32(w + 8) = (u32)(S16(sa + 2) - az);
            U32(u) = (u32)(S16(sb + 4) - ax); U32(u + 4) = 0; U32(u + 8) = (u32)(S16(sb + 6) - az);
            OuterProduct12(w, u, u);
            if (S32(u + 4) < 0) continue;
            u32 d = Sqrt_Refined(Dist2_XZ(x, U32(G() + 0x940) + 0x40));
            if (best > d) best = d;
            found = 1;
        }
    }
    Tmp_Pop(0x38);
    return found ? best : 0;
}

/* the reach: where the line from the player's nozzle height meets a wall, else 5000 */
void DC2CALL Flame_Aim(gptr e)                                       /* 0x41C690 */
{
    gptr pl = U32(G() + 0x940);
    gptr t = Tmp_Push(0xB8), o = t, v = t + 0x90, m = t + 0x98;
    U16(o + 0x74) = U16(pl + 0x40); U16(o + 0x78) = U16(pl + 0x44);
    U32(e + 0x14) = 5000;
    U8(o + 0x85) = 0x18;
    s16 y = (s16)(U16(pl + 0x42) - 0x47E);
    U16(v) = 0; U16(v + 2) = (u16)y; U16(v + 4) = 0xBB8;
    U16(o + 0x5A) = (u16)((s32)y / -1000);
    RotMatrix(pl + 0x48, m);
    ApplyMatrixSV(m, v, o + 0x40);
    U16(o + 0x40) += U16(pl + 0x40);
    U16(o + 0x44) += U16(pl + 0x44);
    u32 d = Flame_Ray(o);
    Tmp_Pop(0xB8);
    if (d) U32(e + 0x14) = d;
    if ((u32)(s32)S16(e + 0x18) > U32(e + 0x14)) U16(e + 0x18) = (u16)U32(e + 0x14);
}

/* ---- kind 6 (areas 2, 8, 9): six object-effect pieces (kind 9) thrown from an object ----
 * e+2 0: from object effect e+0x10+12, else from object e+0x10; heights/angles 0x71E720;
 * e+0xC their model, e+0x12 0: a sound (0x6C) */
void DC2CALL Fx_Pieces(gptr e)                                       /* 0x413CA0 */
{
    u32 n = U16(e + 0x10) + 0xC;
    u32 k = 0;
    for (gptr d = 0x71E720; d < 0x71E750; d += 8, k++) {
        gptr s = Eff_AllocBack();
        if (!s) continue;
        U32(s + 0x50) = 7; U8(s + 0x4E) = 9; U16(s + 0x6C) = 3; U16(s + 0x6E) = 0;
        U8(s + 0x4F) = (u8)k;
        U32(s + 0x68) = U32(e + 0xC);
        gptr b = U8(e + 2) ? U32(G() + 0x940) + U16(e + 0x10) * 0x260 : U32(G() + 0x93C) + n * 0x70;
        U16(s + 0x40) = U16(b + 0x40);
        U16(s + 0x42) = U16(d);
        b = U8(e + 2) ? U32(G() + 0x940) + U16(e + 0x10) * 0x260 : U32(G() + 0x93C) + n * 0x70;
        U16(s + 0x44) = U16(b + 0x44);
        U16(s + 0x4C) = U16(d + 2);
    }
    if (!U16(e + 0x12))
        Se_Play(U8(e + 2) ? U32(G() + 0x940) + n * 0x260 : U32(G() + 0x93C) + n * 0x70, 0x6C);
    Eff_Off(e);
}

/* ---- kind 19 (areas 1, 7, 8): a screen tint whose colour drifts between bounds (steps 0x71E750) ----
 * e+0xC/+0xE/+0x10 r/g/b x10, e+0x15/+0x1A/+0x19 their steps, e+0x12..+0x14 the upper and
 * e+0x16..+0x18 the lower bounds, e+0x1B the blend mode */
void DC2CALL Tint(gptr e)                                            /* 0x413DF0 */
{
    Orig_Call1(U32(0x71E750 + S8(e + 3) * 4), e);
}

void DC2CALL Tint_Init(gptr e)                                       /* 0x413E10 */
{
    U32(e + 4) = 0x413F70;
    U8(e + 3)++;
    U16(e + 0xC) = (u16)(U16(e + 0xC) * 10);
    U16(e + 0xE) = (u16)(U16(e + 0xE) * 10);
    U16(e + 0x10) = (u16)(U16(e + 0x10) * 10);
    Flag_Set(3, 8, 1);
}

static void tint_bounce(gptr e, u32 c, u32 hi, u32 lo, u32 st, s8 d)
{
    s32 v = S16(e + c);
    if (v >= U8(e + hi) * 10) { U16(e + c) = (u16)(U8(e + hi) * 10); U8(e + st) = (u8)-d; }
    else if (v <= U8(e + lo) * 10) { U16(e + c) = (u16)(U8(e + lo) * 10); U8(e + st) = (u8)-d; }
}

void DC2CALL Tint_Step(gptr e)                                       /* 0x413E60 */
{
    s8 dr = S8(e + 0x15), dg = S8(e + 0x1A), db = S8(e + 0x19);
    U16(e + 0xC) += (u16)dr;
    U16(e + 0xE) += (u16)dg;
    U16(e + 0x10) += (u16)db;
    tint_bounce(e, 0xC, 0x12, 0x16, 0x15, dr);
    tint_bounce(e, 0xE, 0x13, 0x17, 0x1A, dg);
    tint_bounce(e, 0x10, 0x14, 0x18, 0x19, db);
}

void DC2CALL Tint_Draw(gptr e)                                       /* 0x413F70 */
{
    gptr p = U32(0x87DC98), ot = 0x87BC0C + (U32(0x87DE0C) << 12);
    U8(p + 4) = (u8)(S16(e + 0xC) / 10); U8(p + 5) = (u8)(S16(e + 0xE) / 10); U8(p + 6) = (u8)(S16(e + 0x10) / 10);
    U8(p + 7) = 0x2A;
    static const float xy[4][2] = {{0, 0}, {320, 0}, {0, 240}, {320, 240}};
    for (u32 k = 0; k < 4; k++) {
        F32(p + 8 + 0xC * k) = xy[k][0]; F32(p + 0xC + 0xC * k) = xy[k][1]; F32(p + 0x10 + 0xC * k) = 0.01f;
    }
    U32(p) = U32(ot) | 0x05000000;
    U32(ot) = p & 0xFFFFFF;
    p += 0x38;
    U32(0x87DC98) = p;
    u32 tp = Gpu_GetTPage(0, (u32)(s32)S8(e + 0x1B), 0x140, 0x100);
    Gpu_SetDrawMode(p, 1, 0, (tp & 0xFFFF) | 0x400, 0);
    Gpu_AddPrim(ot, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* ---- kind 18 (area 1): object e+0xC walks to two points, then is gone ----
 * its +0x57 the state, +0x1D0 the point */
void DC2CALL Fx_Walk2(gptr e)                                        /* 0x414210 */
{
    gptr o = U32(G() + 0x940) + S16(e + 0xC) * 0x260;
    gptr t = Tmp_Push(0x40), wp = t, v = t + 0x10, w = t + 0x18, m = t + 0x20;
    U16(wp) = 0x2C6C; U16(wp + 2) = 0xEC78; U16(wp + 4) = 0xF049; U16(wp + 6) = 0;
    U16(wp + 8) = 0x2CFC; U16(wp + 0xA) = 0xEC78; U16(wp + 0xC) = 0xF146; U16(wp + 0xE) = 0;
    switch (U8(o + 0x57)) {
    case 0:
        U16(o + 0x1D0) = 0;
        U8(o + 0x57)++;
        /* fall through */
    case 1:
        U16(o + 0x4A) &= 0xFFF;
        U16(o + 0x4A) = (u16)(Angle_XZ(o + 0x40, wp + S16(o + 0x1D0) * 8) & 0xFFF);
        U8(o + 0x57)++;
        /* fall through */
    case 2:
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0x80; U16(v + 6) = 0;
        RotMatrix(o + 0x48, m);
        ApplyMatrixSV(m, v, w);
        U16(o + 0x40) += U16(w); U16(o + 0x44) += U16(w + 4);
        if (Dist2_XZ(o + 0x40, wp + S16(o + 0x1D0) * 8) < 250000) {
            U16(o + 0x1D0)++;
            U8(o + 0x57) = (u8)((U16(o + 0x1D0) & 2) | 1);
        }
        break;
    case 3:
        Obj_Disable(o);
        Eff_Off(e);
        break;
    }
    Tmp_Pop(0x40);
}

/* ---- kind 17 (areas 0, 1, 6): a burst of effects on the player when 0x74B16C latched 1 or 2 ---- */
void DC2CALL Fx_PlBurst(gptr e)                                      /* 0x414370 */
{
    Orig_Call1(U32(0x71E87C + S8(e + 3) * 4), e);
}

void DC2CALL Fx_PlBurst_Wait(gptr e)                                 /* 0x414390 */
{
    u8 n = (u8)Latch_74B16C(0);
    U8(e + 0x14) = n;
    if (n && n < 3) K2_Script(e, 0x71E788);
}

/* effects from a script d (entries 0x28, -1 ends) on the player's part 0x71E877[e+0x14] */
void DC2CALL K2_Script(gptr e, gptr d)                               /* 0x4143C0 */
{
    u32 base = U32(e + 0xC);
    for (; U32(d) != 0xFFFFFFFF; d += 0x28) {
        gptr s = K2_AllocBack();
        if (!s) break;
        U8(s + 0x58) = 0xF;
        U16(s + 0x6C) = U8(d + 1); U16(s + 0x6E) = U8(d + 2);
        U16(s + 0x64) = U16(d + 4); U8(s + 0x66) = U8(d + 6);
        U32(s + 0x40) = U32(d + 8); U32(s + 0x44) = U32(d + 0xC);
        U8(s + 0x46) = 0;
        U8(s + 0x84) = U8(d + 3);
        U32(s + 0x68) = U32(e + 0x10);
        U32(s + 0x70) = U16(d + 0x16) + base;
        U32(s + 0x48) = U32(d + 0x20); U32(s + 0x4C) = U32(d + 0x24);
        U8(s + 0x67) |= U8(0x71E888 + U16(e + 0x16));
        U32(s + 0x60) = Obj_Part(U32(U32(G() + 0x940) + 0x64), U8(0x71E877 + U8(e + 0x14)));
        K2_SyncParent(s);
        K2_Detach(s);
    }
}

/* ---- kind 18 (areas 3, 4, 5): an idle slot (steps 0x71E88C: this, nothing, off) ---- */
void DC2CALL Fx_Idle(gptr e)                                         /* 0x4144A0 */
{
    Orig_Call1(U32(0x71E88C + S8(e + 3) * 4), e);
}

void DC2CALL Fx_Idle_Init(gptr e)                                    /* 0x4144C0 */
{
    U8(e + 3)++;
    U8(e + 0xC) = 0; U8(e + 0xD) = 0;
}

/* ---- kind 20 (areas 0, 2): the player (and the next object, and held pieces 3/3) bob on water ----
 * steps 0x71E8C8; heights 0x71E898 (16), frame counts 0x71E8A8 (e+2 1: 0x71E8B8) */
void DC2CALL Fx_Bob(gptr e)                                          /* 0x414610 */
{
    if (Flag_Check(2, 0xF) || !Flag_Check(1, 3)) Orig_Call1(U32(0x71E8C8 + S8(e + 3) * 4), e);
}

void DC2CALL Fx_Bob_Init(gptr e)                                     /* 0x4144E0 */
{
    U16(e + 0xE) = 0; U16(e + 0xC) = 0;
    U8(e + 0x11) = 0;
    U8(e + 0x10) = U8(e + 2) == 1 ? U8(0x71E8B8) : U8(0x71E8A8);
    U8(e + 3)++;
}

void DC2CALL Fx_Bob_Step(gptr e)                                     /* 0x414520 */
{
    gptr pl = U32(G() + 0x940);
    u16 h = (u16)(((s16)S8(0x71E898 + U8(e + 0x11)) - 0xF0) * 25);
    U16(pl + 0x42) = h;
    U16(pl + 0x2A2) = h;
    for (u32 i = 0; i < 0x230; i += 0x70) {
        gptr s = U32(G() + 0x93C) + i;
        if (U8(s + 0x4E) == 3 && U8(s + 0x4F) == 3) U16(s + 0x42) = h;
    }
    U16(e + 0xE) = (u16)(s16)S8(0x71E898 + U8(e + 0x11));
    if (!U8(e + 0x10)) {
        u8 k = (u8)((U8(e + 0x11) + 1) & 0xF);
        U8(e + 0x11) = k;
        U8(e + 0x10) = U8(e + 2) == 1 ? U8(0x71E8B8 + k) : U8(0x71E8A8 + k);
    } else U8(e + 0x10)--;
}

/* ---- kind 21 (area 2): a blended full-screen tile whose red pulses (steps 0x71E8D0) ----
 * e+0xC the half period, e+0x10..+0x12 the colour, e+0x13 the step */
void DC2CALL Fx_Pulse(gptr e)                                        /* 0x4147F0 */
{
    Orig_Call1(U32(0x71E8D0 + S8(e + 3) * 4), e);
}

void DC2CALL Fx_Pulse_Init(gptr e)                                   /* 0x414660 */
{
    U8(e + 0x10) = 0; U8(e + 0x11) = 0;
    U8(e + 3)++;
    U16(e + 0xE) = U16(e + 0xC);
    U8(e + 0x12) = 0;
    U32(e + 4) = 0x4146A0;
    U8(e + 0x13) = (u8)(0x40 / (s32)S16(e + 0xC));
}

void DC2CALL Fx_Pulse_Draw(gptr e)                                   /* 0x4146A0 */
{
    gptr ot = U32(0x87F94C) + 0x88, p = U32(0x87DC98);
    Gpu_SetTile(p);
    Gpu_SetSemiTrans(p, 1);
    U32(p + 8) = 0; U32(p + 0xC) = 0;
    U16(p + 0x14) = 0x140; U16(p + 0x16) = 0xF0;
    U8(p + 4) = U8(e + 0x10); U8(p + 5) = U8(e + 0x11); U8(p + 6) = U8(e + 0x12);
    Gpu_AddPrim(ot, p);
    p += 0x18;
    U32(0x87DC98) = p;
    u32 tp = Gpu_GetTPage(0, 1, 0x140, 0x100);
    Gpu_SetDrawMode(p, 1, 0, tp & 0xFFFF, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x88, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

void DC2CALL Fx_Pulse_Step(gptr e)                                   /* 0x4147A0 */
{
    U16(e + 0xE)--;
    if (S16(e + 0xE) < 0) {
        U16(e + 0xE) = U16(e + 0xC);
        s32 n = S16(e + 0xC);
        U8(e + 0x13) = (u8)(S8(e + 0x13) < 0 ? 0x40 / n : -0x40 / n);
    } else U8(e + 0x10) += U8(e + 0x13);
}

/* ---- kind 19 (area 4): a mounted gun aimed with the pad (steps 0x71E8DC) ----
 * e+0x18/+0x1A pitch/yaw, e+0x12 the reload count, e+0x14 the shell (an effect slot) */
void DC2CALL Gun(gptr e)                                             /* 0x414D00 */
{
    Orig_Call1(U32(0x71E8DC + S8(e + 3) * 4), e);
}

void DC2CALL Gun_Init(gptr e)                                        /* 0x414810 */
{
    U32(e + 4) = 0x414850;
    U8(e + 3)++;
    U16(e + 0x18) = 0xF6D; U16(e + 0x1A) = 0xAFE;
    gptr s = K2_AllocBack();
    if (s) {
        U32(s + 0x50) = 3; U8(s + 0x58) = 0x11; U8(s + 0x7A) = 1;
        U32(e + 0x14) = s;
    }
}

/* the shell: a picture (+0x94) facing its own rotation */
void DC2CALL Gun_Draw(gptr e)                                        /* 0x414850 */
{
    gptr c = G() + 0x778;
    if (Flag_Check(7, 0x17)) return;
    gptr s = U32(e + 0x14);
    gptr t = Tmp_Push(0x30), lv = t, v = t + 0x10;
    RotMatrix(s + 0x48, s);
    U32(lv) = (u32)((s32)S16(s + 0x40) - S16(c + 0x28));
    U32(lv + 4) = (u32)((s32)S16(s + 0x42) - S16(c + 0x2A));
    U32(lv + 8) = (u32)((s32)S16(s + 0x44) - S16(c + 0x2C));
    ApplyMatrixLV(c, lv, s + 0x14);
    U32(s + 0x14) += (u32)(s32)S16(c + 0x30);
    U32(s + 0x18) += (u32)(s32)S16(c + 0x32);
    U32(s + 0x1C) += U16(c + 0x34);
    SetRotMatrix(s);
    SetTransMatrix(s);
    gptr f = U32(s + 0x94);
    u16 sx = U16(s + 0x6C), sy = U16(s + 0x6E);
    u16 x0 = (u16)((u16)(s16)S8(f + 6) * sx), x1 = (u16)((u16)((s16)S8(f + 6) + U8(f + 2)) * sx);
    u16 y0 = (u16)((u16)(s16)S8(f + 7) * sy), y1 = (u16)((u16)(U8(f + 3) + (s16)S8(f + 7)) * sy);
    U16(v + 6) = 0; U16(v + 0xE) = 0; U16(v + 0x16) = 0;   /* (the pads: gte_ldv3c loads them) */
    U16(v) = x0; U16(v + 2) = y0; U16(v + 4) = 0;
    U16(v + 8) = x1; U16(v + 0xA) = y0; U16(v + 0xC) = 0;
    U16(v + 0x10) = x0; U16(v + 0x12) = y1; U16(v + 0x14) = 0;
    gte_ldv3c(v); gte_rtpt();
    gptr p = U32(0x87DC98);
    U8(p + 0x34) = U8(f); U8(p + 0x14) = U8(p + 0x34);
    U8(p + 0x25) = U8(f + 1); U8(p + 0x15) = U8(p + 0x25);
    gte_stsxy3_g3(p);
    U16(v) = x1; U16(v + 2) = y1; U16(v + 4) = 0;
    gte_ldv0(v); gte_rtps();
    U8(p + 0x44) = (u8)(U8(f) + U8(f + 2) - 1); U8(p + 0x24) = U8(p + 0x44);
    U8(p + 0x45) = (u8)(U8(f + 1) + U8(f + 3) - 1); U8(p + 0x35) = U8(p + 0x45);
    gte_stsxy(p + 0x38);
    PC_stsz4_g4(p);
    U16(p + 0x16) = U16(s + 0x68);
    U16(p + 0x26) = (u16)(U16(s + 0x6A) | 0x400);
    U32(p + 4) = U32(s + 0x64);
    U8(p + 7) |= 2;
    gptr ot = 0x87BD40 + (U32(0x87DE0C) << 12);
    U32(p) = U32(ot) | 0x09000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) = p + 0x48;
    Tmp_Pop(0x30);
}

void DC2CALL Gun_Step(gptr e)                                        /* 0x414AC0 */
{
    gptr c = G() + 0x778;
    u32 pad = U32(0x87DC08);
    if (pad & 0x1000) { U16(e + 0x18) += 0x14; if (S16(e + 0x18) >= 0x1035) U16(e + 0x18) = 0x1035; }
    else if (pad & 0x4000) { U16(e + 0x18) -= 0x14; if (S16(e + 0x18) <= 0xEB9) U16(e + 0x18) = 0xEB9; }
    pad = U32(0x87DC08);
    if (pad & 0x2000) { U16(e + 0x1A) += 0x14; if (S16(e + 0x1A) >= 0xC3E) U16(e + 0x1A) = 0xC3E; }
    else if (pad & 0x8000) { U16(e + 0x1A) -= 0x14; if (S16(e + 0x1A) <= 0x9BE) U16(e + 0x1A) = 0x9BE; }
    Camera_LookAt(c + 0x28, c + 0x30, c + 0x20, c + 0x38);
    U16(c + 0x20) &= 0xFFF; U16(c + 0x22) &= 0xFFF; U16(c + 0x24) &= 0xFFF;
    gptr r = Tmp_Push(8);
    U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(r + 4) = U16(c + 0x24);
    RotMatrix(r, c);
    Tmp_Pop(8);
    if (U16(e + 0x12)) U16(e + 0x12)--;
    else if (U8(0x87DC08) & 0x20) Gun_Fire(e);
}

void DC2CALL Gun_Fire(gptr e)                                        /* 0x414BD0 */
{
    gptr t = Tmp_Push(0x28), z = t, m = t + 8;
    U16(z) = 0; U16(z + 2) = 0; U16(z + 4) = 0; U16(z + 6) = 0;
    gptr s = K2_AllocBack();
    if (s) {
        for (u32 a = 0; a < 0x12; a += 2) U16(m + a) = 0;
        U16(m) = 0x1000; U16(m + 8) = 0x1000; U16(m + 0x10) = 0x1000;
        RotMatrixY(-(s32)S16(e + 0x1A), m);
        RotMatrixX(-(s32)S16(e + 0x18), m);
        U16(s + 0x7C) = U16(m + 0xC); U16(s + 0x7E) = U16(m + 0xE); U16(s + 0x80) = U16(m + 0x10);
        U16(s + 0x6C) = 0xA; U16(s + 0x6E) = 0xA;
        U16(s + 0x40) = 0x1B66; U16(s + 0x42) = 0xE929; U16(s + 0x44) = 0x2F29;
        U8(s + 0x58) = 0x11;
        RotMatrix(z, s + 0x20);
        U16(s + 0x4A) = (u16)(U16(e + 0x1A) - 0x400);
        U16(s + 0x48) = (u16)(U16(e + 0x18) - 0x400);
        U16(s + 0x4C) = 0x400;
        RotMatrixYXZ(s + 0x48, s + 0x20);
    }
    Tmp_Pop(0x28);
    Shake_Set(5, 0, 5);
    Se_PlayFlat(0x6F, 0x50, 0x40);
    Flash_Play(0, 0x5F0F3C);
    U16(e + 0x12) = 0xF;
}

/* step 2 of the gun (and step 0 of kind 18 area 6): back to step 1 */
void DC2CALL Fx_Step1(gptr e)                                        /* 0x415030 */
{
    U8(e + 3) = 1;
}

/* ---- kind 18 (area 6): steps 0x71E8E4 (to step 1, nothing, off) ---- */
void DC2CALL Fx_Hold(gptr e)                                         /* 0x415010 */
{
    Orig_Call1(U32(0x71E8E4 + S8(e + 3) * 4), e);
}

/* ---- kind 20 (area 4): the player is put facing e+0xC's quarter, then the slot ends ---- */
void DC2CALL Fx_Face(gptr e)                                         /* 0x414D70 */
{
    gptr pl = U32(G() + 0x940);
    s32 a = ((U16(e + 0xC) + 0x200) >> 10) & 3, b = ((U16(pl + 0x4A) + 0x200) >> 10) & 3;
    U16(pl + 0x178) = U8(0x7031AB + a - b);
    U16(pl + 0x176) = 0;
    U16(pl + 0x44) = 0x2630;
    U8(pl + 0x54) = 2; U8(pl + 0x55) = 0; U8(pl + 0x56) = 0;
    Eff_Off(e);
}

/* ---- kind 16 (area 5): a counter to 2 kept by its draw handler ---- */
void DC2CALL Fx_Count(gptr e)                                        /* 0x414DE0 */
{
    if (U8(e + 3)) return;
    U16(e + 0x18) = 0;
    U8(e + 3)++;
    U32(e + 4) = 0x414E10;
}

void DC2CALL Fx_Count_Draw(gptr e)                                   /* 0x414E10 */
{
    if (S16(e + 0x18) < 2) U16(e + 0x18)++;
}

/* ---- kind 17 (area 5): e+0xC -1 clears four VRAM rows (0x140, 0x1FB..), else sets object
 * e+0xC's +0x106 from e+0xE/+0x10; object effects 12..17 whose +0x60 changed are re-shaded ---- */
void DC2CALL Fx_Shades(gptr e)                                       /* 0x414E30 */
{
    u16 a = U16(e + 0xC);
    if (a == 0xFFFF) {
        memset(MEM(0x746E80), 0, 0x200);
        gptr r = Tmp_Push(8);
        U16(r) = 0x140; U16(r + 2) = 0x1FB; U16(r + 4) = 0x100; U16(r + 6) = 1;
        for (u32 i = 0; i < 4; i++) {
            DrawSync(0);
            Gpu_LoadImage(r, 0x746E80);
            DrawSync(0);
            U16(r + 2)++;
        }
        Tmp_Pop(8);
    } else if (a) {
        if (S16(e + 0xE) < 0) U16(e + 0xE) = 0;
        U16(U32(G() + 0x940) + (s16)a * 0x260 + 0x106) = (u16)((u16)(S16(e + 0xE) >> 4) | (u16)(U16(e + 0x10) << 6));
    }
    for (u32 i = 0x540; i < 0x7E0; i += 0x70) {
        gptr s = U32(G() + 0x93C) + i;
        if (!(U8(s + 0x50) & 1)) continue;
        u16 v = U16(s + 0x60);
        if (v == U16(s + 0x62)) continue;
        U16(s + 0x62) = v;
        Fx_Shade(s, v, (u32)(s32)S16(e + 0x12));
    }
}

/* the vertex colours of model s+0x68 (triangles 0x28, quads 0x34): (((i & 15) + 15) * v >> 5) * k >> 8 */
void DC2CALL Fx_Shade(gptr s, u32 v, u32 k)                          /* 0x414F30 */
{
    gptr m = U32(s + 0x68);
    v &= 0xFF;
    for (u32 i = 0; i < U16(m + 8); i++) {
        u8 c = (u8)(((u32)(s32)(s8)(u8)((((i & 0xF) + 0xF) * v) >> 5) * k) >> 8);
        gptr q = U32(m) + 0x28 * i;
        for (u32 j = 0; j < 3; j++) { U8(q + 0x1C + 4 * j) = c; U8(q + 0x1D + 4 * j) = c; U8(q + 0x1E + 4 * j) = c; }
    }
    for (u32 i = 0; i < U16(m + 0xA); i++) {
        u8 c = (u8)(((u32)(s32)(s8)(u8)((((i & 0xF) + 0xF) * v) >> 5) * k) >> 8);
        gptr q = U32(m + 4) + 0x34 * i;
        for (u32 j = 0; j < 4; j++) { U8(q + 0x24 + 4 * j) = c; U8(q + 0x25 + 4 * j) = c; U8(q + 0x26 + 4 * j) = c; }
    }
}

/* ---- kind 16 (area 7): the screen copied in strips whose heights wave ---- */
void DC2CALL Fx_Wave(gptr e)                                         /* 0x415040 */
{
    if (!U8(e + 3)) {
        U8(e + 3) = 1;
        U32(e + 4) = 0x415080;
        U32(0x87DE00) = 3;
    }
    U16(e + 0xC) = (u16)((U16(e + 0xC) + 0x14) & 0xFFF);
}

void DC2CALL Fx_Wave_Draw(gptr e)                                    /* 0x415080 */
{
    s16 w[5];
    for (u32 i = 0; i < 5; i++) w[i] = (s16)((rsin((s32)(i * 0x1000 / 7) + S16(e + 0xC)) >> 10) + (s32)i * 60);
    w[4] = 0xF0; w[0] = 0;
    for (u32 x = 0; x < 0x140; x += 0x40) {
        for (u32 j = 0; j < 4; j++) {
            gptr p = U32(0x87DC98);
            F32(p + 8) = (float)x;
            U16(p + 0x14) = 0x40;
            F32(p + 0xC) = (float)w[j];
            U16(p + 0x16) = (u16)(w[j + 1] - w[j]);
            U16(p + 0x10) = (u16)x;
            U16(p + 0x18) = 0x40;
            U8(p + 0x12) = (u8)(j * 0x3C);
            U16(p + 0x1A) = 0x3C;
            U8(p + 0x13) = 0;
            U32(p + 4) = 0x8E808080;
            gptr ot = U32(0x87F94C) + 0x88;
            U32(p) = U32(ot) | 0x02000000;
            U32(ot) = p & 0xFFFFFF;
            U32(0x87DC98) = p + 0x1C;
        }
    }
}

/* ---- a flat water surface (steps table 0x71E8F0) ----
 * like kind 3 without the swell: e+0xC/+0xE x/z of the corner, e+0x1C height /10,
 * e+0x14 yaw, e+0x10/+0x11 tiles across/along, e+0x1A tile size /250, e+0x16 fixed
 * OT depth (0: from z), e+0x18 flags (4 still), e+0x19 tpage, e+0x1E scroll */

void DC2CALL Water2(gptr e)                                          /* 0x4151D0 */
{
    Orig_Call1(U32(0x71E8F0 + S8(e + 3) * 4), e);
}

void DC2CALL Water2_Init(gptr e)                                     /* 0x4151F0 */
{
    U8(e + 3)++;
    U16(e + 0x1E) = 0;
    if (!U8(e + 0x1A)) U8(e + 0x1A) = 4;
    U32(e + 4) = 0x415280;
    Flag_Set(3, 8, 1);
    Water2_Step(e);
}

void DC2CALL Water2_Step(gptr e)                                     /* 0x415230 */
{
    if (!(U8(e + 0x18) & 4)) {
        U16(e + 0x1E) += U8(e + 0x13);
        if (U16(e + 0x1E) > 0x257F) U16(e + 0x1E) = 0;
    }
    gptr v = Tmp_Push(8);
    U32(v) = 0; U32(v + 4) = 0;
    Water_Level(v);                     /* (the result is dropped) */
    Tmp_Pop(8);
}

void DC2CALL Water2_Draw(gptr e)                                     /* 0x415280 */
{
    gptr g = G(), c = g + 0x778;
    gptr t = Tmp_Push(0x70), r = t, m = t + 8, lv = t + 0x28, vt = t + 0x38, z = t + 0x58;
    u32 sh = (U8(g + 0xB4B) - 2) & 31;
    U16(r) = 0; U16(r + 2) = U16(e + 0x14); U16(r + 4) = 0;
    RotMatrix(r, m);
    MulMatrix0(c, m, m);
    SetRotMatrix(c);
    U32(lv) = (u32)((s32)S16(e + 0xC) - S16(c + 0x28));
    U32(lv + 4) = (u32)((s32)(s16)(U16(e + 0x1C) * 10) - S16(c + 0x2A));
    U32(lv + 8) = (u32)((s32)S16(e + 0xE) - S16(c + 0x2C));
    ApplyRotMatrixLV(lv, m + 0x14);
    U32(m + 0x14) += (u32)(s32)S16(c + 0x30);
    U32(m + 0x18) += (u32)(s32)S16(c + 0x32);
    U32(m + 0x1C) += U16(c + 0x34);
    SetTransMatrix(m);
    s32 zfix = S16(e + 0x16);
    u32 otz = (u32)zfix;
    u16 step = (u16)(U8(e + 0x1A) * 250);
    s32 nr = S8(e + 0x11), nc = S8(e + 0x10);
    u32 v0 = (u32)(U16(e + 0x1E) / 50), v1 = v0 + 0x30;
    for (u32 q = 0; q < 0x20; q += 2) U16(vt + q) = 0;   /* (y and the pads: gte_ldv3c loads them) */
    for (s32 i = 0; i < nr; i++) {
        v0 += 0x30; v1 += 0x30;
        if (v0 > 0xBF) { v0 -= 0xC0; v1 -= 0xC0; }
        U16(vt + 4) = U16(vt + 0xC) = (u16)(i * step);
        U16(vt + 0x14) = U16(vt + 0x1C) = (u16)((i + 1) * step);
        for (s32 j = 0; j < nc; j++) {
            U16(vt) = U16(vt + 0x10) = (u16)(j * step);
            U16(vt + 8) = U16(vt + 0x18) = (u16)((j + 1) * step);
            gptr p = U32(0x87DC98);
            SetRotMatrix(m);
            gte_ldv3c(vt); gte_rtpt(); gte_stsxy3_g3(p);
            gte_ldv0(vt + 0x18); gte_rtps(); gte_stsxy(p + 0x38);
            PC_stsz4_g4(p);
            if (!zfix) {
                gte_stsz4(z);
                u32 a = U32(z);
                for (u32 q = 1; q < 4; q++) if (a > U32(z + 4 * q)) a = U32(z + 4 * q);
                a >>= sh;
                otz = a >= 0x3FF ? 0x3FF : a;
            }
            U8(p + 0x34) = 0; U8(p + 0x14) = 0; U8(p + 0x44) = 0x30; U8(p + 0x24) = 0x30;
            U8(p + 0x25) = (u8)v0; U8(p + 0x15) = (u8)v0; U8(p + 0x45) = (u8)v1; U8(p + 0x35) = (u8)v1;
            U32(p + 4) = 0x2E404080;
            U16(p + 0x26) = (u16)(((u16)(s16)S8(e + 0x19) << 5) | 0x418);
            U16(p + 0x16) = 0x7F00;
            gptr ot = 0x87BC00 + ((U32(0x87DE0C) << 10) + otz) * 4;
            U32(p) = U32(ot) | 0x09000000;
            U32(ot) = p & 0xFFFFFF;
            U32(0x87DC98) = p + 0x48;
        }
    }
    Tmp_Pop(0x70);
}

/* ---- blasts (area kind 8, by e+2: 0 the charge 0x71E940, 1 a blast 0x71E948) ----
 * e+0x10..+0x14 the place, e+0x16 a countdown; a blast inside one of the quads at
 * 0x71E900 with its flag 7/0x10+i still clear sets that flag when it goes off */

void DC2CALL Fx_Blasts(gptr e)                                       /* 0x415A20 */
{
    Orig_Call1(U32(0x71E950 + U8(e + 2) * 4), e);
}

void DC2CALL Fx_Charge(gptr e)                                       /* 0x415840 */
{
    Orig_Call1(U32(0x71E940 + S8(e + 3) * 4), e);
}

void DC2CALL Fx_Charge_Arm(gptr e)                                   /* 0x415600 */
{
    if (U8(U32(G() + 0x948) + 0x37) != 4) { Eff_Off(e); return; }
    U16(e + 0x16) = 0x28;
    U8(e + 3)++;
}

u8 DC2CALL Fx_Blast_Quad(gptr e)                                     /* 0x4157F0 */
{
    for (u32 i = 0; i < 4; i++)
        if (PointInQuad(e + 0x10, 0x71E900 + i * 0x10) && !Flag_Check(7, 0x10 + i)) return (u8)(0x10 + i);
    return 0;
}

void DC2CALL Fx_Charge_Fire(gptr e)                                  /* 0x415630 */
{
    gptr g = G(), pl = U32(g + 0x940);
    if (U16(e + 0x16)) { U16(e + 0x16)--; return; }
    gptr s1 = U32(g + 0x948) + 0x34;
    if (!U8(s1 + 8)) { U8(s1 + 3) = 1; U8(s1 + 8) = 1; }
    Se_PlayFlat(0x66, 0x50, 0x40);
    u32 found = Fx_Blast_Quad(e);
    gptr s = Light_Alloc();
    if (s) {
        U8(s + 2) = 1; U8(s + 1) = U8(e + 1);
        U16(s + 0x10) = U16(e + 0x10); U16(s + 0x12) = U16(e + 0x12); U16(s + 0x14) = U16(e + 0x14);
        U16(s + 0x16) = 0xF; U16(s + 0x1A) = 1;
        if (found) { U16(s + 0x18) = (u16)found; U8(s + 3) = 1; }
    }
    gptr t = Tmp_Push(0x28), r = t, m = t + 8;
    U16(r) = 0; U16(r + 2) = U16(pl + 0x4A); U16(r + 4) = 0; U16(r + 6) = 0;
    for (u32 i = 0; i < 3; i++) {
        s = Light_Alloc();
        if (!s) continue;
        U8(s + 1) = U8(e + 1); U8(s + 2) = 1;
        RotMatrix(r, m);
        U16(s + 0x10) = 0x9C4; U16(s + 0x14) = 0; U16(s + 0x12) = 0;
        ApplyMatrixSV(m, s + 0x10, s + 0x10);
        U16(s + 0x10) += U16(e + 0x10); U16(s + 0x12) += U16(e + 0x12); U16(s + 0x14) += U16(e + 0x14);
        U16(s + 0x12) = (u16)Floor_Height(s + 0x10, s + 0x10, 0x80, 0);
        U16(r + 2) = (u16)((U16(r + 2) + 0x555) & 0xFFF);
        U16(s + 0x16) = (u16)(Rand8() % 15 + 15);
        U16(s + 0x1A) = (u16)(i & 1);
        if (!found) {
            found = Fx_Blast_Quad(s);
            if (found) { U8(s + 3) = 1; U16(s + 0x18) = (u16)found; }
        }
    }
    Tmp_Pop(0x28);
    Eff_Off(e);
}

void DC2CALL Fx_Blast(gptr e)                                        /* 0x415A00 */
{
    Orig_Call1(U32(0x71E948 + S8(e + 3) * 4), e);
}

void DC2CALL Fx_Blast_Wait(gptr e)                                   /* 0x415860 */
{
    gptr c = G() + 0x778;
    gptr t = Tmp_Push(0x28), r = t, m = t + 8;
    U16(r) = 0x800; U16(r + 2) = 0; U16(r + 4) = 0; U16(r + 6) = 0;
    RotMatrix(r, m);
    U16(t) = (u16)(U16(e + 0x10) - U16(c + 0x28));
    U16(t + 2) = (u16)(U16(e + 0x12) - U16(c + 0x2A));
    U16(t + 4) = (u16)(U16(e + 0x14) - U16(c + 0x2C));
    ApplyMatrix(c, t, m + 0x14);
    U32(m + 0x14) += (u32)(s32)S16(c + 0x30);
    U32(m + 0x18) += (u32)(s32)S16(c + 0x32);
    U32(m + 0x1C) += U16(c + 0x34);
    if (U16(e + 0x16)) U16(e + 0x16)--;
    else if (Weapon_Hit(m, e + 0x10, 0x1C)) Eff_Off(e);
    else U8(e + 3)++;
    Tmp_Pop(0x28);
}

void DC2CALL Fx_Blast_Go(gptr e)                                     /* 0x415960 */
{
    if (U16(e + 0x16)) { U16(e + 0x16)--; return; }
    if (U16(e + 0x18)) Flag_Set(7, U16(e + 0x18), 1);
    gptr s = Light_Alloc();
    if (s) {
        U8(s + 1) = 1; U32(s + 0xC) = 0x5E2CE4; U32(s + 0x10) = 0x5E2F78; U8(s + 2) = 3;
        U16(s + 0x1A) = U16(e + 0x10); U16(s + 0x1C) = U16(e + 0x12); U16(s + 0x1E) = U16(e + 0x14);
    }
    if (U16(e + 0x1A)) Se_PlayFlat(0x67, 0x64, 0x40);
    Shake_Set(5, 0, 5);
    Eff_Off(e);
}

/* ---- the ride (area kind 2: steps 0x71E9E8): the player fixed on a moving
 * craft, turning and shooting while the scenery (G+0x93C effect objects) scrolls by
 * e+0xC phase (0 start, 2 riding, 3 end), e+0xD the light, e+0xE..+0x10 fog rgb,
 * e+0x12 the gun's reload, e+0x18 turn speed, e+0x1A/+0x1C pitch/yaw, e+0x1E a clock,
 * e+0x20 the player's life kept; 0x87FFC0 the hit's countdown, 0x87FFC8 the
 * scenery's slow-down, 0x87FFCC/CE the gauge's blink, 0x87FFD8 the sway,
 * 0x87FFDA the next shot's object */

void DC2CALL Ride(gptr e)                                            /* 0x415AC0 */
{
    Flag_Set(3, 0, 0);
    Flag_Set(1, 3, 1);
    Flag_Set(1, 0, 0);
    Orig_Call1(U32(0x71E9E8 + S8(e + 3) * 4), e);
}

void DC2CALL Ride_Init(gptr e)                                       /* 0x415B00 */
{
    gptr g = G(), c = g + 0x778;
    u32 col = (u32)U8(e + 0x10) << 3 | (u32)U8(e + 0xF) << 11 | (u32)U8(e + 0xE) << 19;
    U32(0x7479F0) = col;
    Fog_SetColor(col, 0x3F800000, 0x40000000);          /* (1.0, 2.0) */
    Fog_Enable(1);
    U32(0x747880) = 0;
    Light_SetFar((u32)U8(e + 0x10) << 19 | (u32)U8(e + 0xF) << 11 | (u32)U8(e + 0xE) << 3);
    U8(e + 3)++;
    U32(e + 4) = 0x416450;
    Flag_Set(1, 9, 1);
    Flag_Set(3, 10, 1);
    memset(MEM(0x747888), 0, 0x168);
    memset(MEM(0x87FFC0), 0, 0x1C);
    gptr o = U32(g + 0x93C);
    U16(o + 0x6EE) = 0; U16(o + 0x75E) = 1; U16(o + 0x7CE) = 2; U16(o + 0x83E) = 3;
    U16(o + 0xB4E) = 0; U16(o + 0xBBE) = 1; U16(o + 0xC2E) = 2; U16(o + 0xC9E) = 3;
    U16(U32(c + 0x40) + U8(c + 0x4C) * 16 + 6) = 0xD2;
    SetGeomScreen(0xD2);
    U16(e + 0x1E) = 0;
    U16(c + 0x20) = 0xFE0;
    U16(c + 0x22) = U16(U32(g + 0x940) + 0x4A);
    U32(U32(g + 0x940) + 0x50) &= ~2u;
    U16(0x87FFCE) = 0xFFF0;
    U8(e + 0x12) = 0x16;
    Ride_Step(e);
}

void DC2CALL Ride_Step(gptr e)                                       /* 0x415CD0 */
{
    gptr g = G(), pl = U32(g + 0x940), c = g + 0x778;
    s16 t = (s16)(U16(0x87FFC4) + 1);
    U16(0x87FFC4) = (u16)(t > 30000 ? 30000 : t);
    if (Flag_Check(0, 0xB)) U16(pl + 0x108) = 0x4B0;
    if (U8(pl + 0x17B)) U8(pl + 0x17B)--;
    if (U8(pl + 0x174) & 1) {
        if (!U16(0x87FFC0)) {                           /* hit */
            Se_Play(0, 0x5F);
            U16(0x87FFC0) = 0xF0;
            Shake_Set(10, 0, 0x14);
            u8 d = U8(g + 0x108E);
            u16 a = U16(pl + 0x176);
            if (d == 0 || d == 1) U16(pl + 0x108) -= a;
            else U16(pl + 0x108) += (u16)(-2 * a - (a >> 1));
            if (S16(pl + 0x108) < 0 && !U8(0x87FFCA)) { U8(0x87FFCA) = 1; GameOver_Start(0); }
        }
        if (U16(0x87FFC0)) {
            U16(0x87FFC0) -= 8;
            u16 a = (u16)(U16(0x87FFC0) - 0x80);
            if (a == 0x70) U16(c + 0x20) += 0x70;
            else if (a == 0x60) U16(c + 0x20) += 0xC0;
            else if ((s16)a > 0) U16(c + 0x20) += (u16)(a * 3);
            s16 f = S16(0x87FFC0);
            U16(0x87FFC8) = (u16)(0x338 - f);
            if (f > 0x30) {
                u8 w = (u8)(f / 2);
                if (w > 0x60) w = 0x60;
                u16 h = w >> 1;
                U16(c + 0x22) += (u16)(U16(0x87DE10) % w - h);
                U16(e + 0x1C) += (u16)(U16(0x87DE10) % w - h);
            }
        }
        if (!U16(0x87FFC0)) { U16(0x87FFC8) = 0; U16(pl + 0x174) = 0; U32(pl + 0x54) = 1; }
    }
    U16(pl + 0x42) = 0xFED4;
    if (U8(e + 0x12) < 0x1E && S16(pl + 0x108) >= 0) {   /* turning */
        u32 pad = U32(0x87DC08);
        if (pad & 0x8000) U16(e + 0x18) -= 5;
        if (pad & 0x2000) U16(e + 0x18) += 5;
        if (S16(e + 0x18) > 0x2D) U16(e + 0x18) = 0x2D;
        if (S16(e + 0x18) < -0x2D) U16(e + 0x18) = (u16)-0x2D;
        s16 v = S16(e + 0x18);
        if ((v < 0 && !(pad & 0x8000)) || (v > 0 && !(pad & 0x2000))) U16(e + 0x18) = (u16)(v / 2);
        U16(e + 0x1C) += U16(e + 0x18);
    }
    s16 y = S16(e + 0x1C);
    if (y > 0x3AC && y < 0x800) U16(e + 0x1C) = 0x3AC;
    if (S16(e + 0x1C) < -0x3AC) U16(e + 0x1C) = (u16)-0x3AC;
    U16(pl + 0x4A) = U16(e + 0x1C);
    if (U8(e + 0xC) == 2) {
        if (U8(e + 0x12)) U8(e + 0x12)--;
        if (S16(0x87FFC0) < 0x10 && !U8(e + 0x12) && (U8(0x87DC08) & 0x20) && S16(pl + 0x108) >= 0) {
            U8(e + 0x12) = 0x16;
            Ride_Shoot(e);
        }
    }
    U16(c + 0x3A) = 0xBB8;
    if (U8(e + 0xC) == 0) {
        U16(e + 0x20) = U16(pl + 0x108);
        U16(pl + 0x108) = 0x4B0;
        U8(e + 0xC) = 2;
    } else if (U8(e + 0xC) == 3) {
        U16(pl + 0x108) = U16(e + 0x20);
        Fog_Enable(0);
        Light_SetFar(0);
    }
    if (U8(e + 0xC) == 2) {                             /* the camera on the craft */
        U16(pl + 0x40) = 0; U16(pl + 0x42) = 0xFED4; U16(pl + 0x44) = 0xB62C;
        U16(c + 0x30) = 0; U16(c + 0x32) = 0; U16(c + 0x34) = 0x14;
        U16(c + 0x2A) = 0xF768; U16(c + 0x28) = U16(pl + 0x40); U16(c + 0x2C) = U16(pl + 0x44);
        U16(c + 0x22) = U16(pl + 0x4A);
        Camera_LookAt(c + 0x28, c + 0x30, c + 0x20, c + 0x38);
        U16(c + 0x20) &= 0xFFF; U16(c + 0x22) &= 0xFFF; U16(c + 0x24) &= 0xFFF;
        gptr r = Tmp_Push(8);
        U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(r + 4) = U16(c + 0x24); U16(r + 6) = 0;
        RotMatrix(r, c);
        Tmp_Pop(8);
        static const u16 sway[8] = {0xFE0, 0xFDA, 0xFD8, 0xFD6, 0xFD9, 0xFD3, 0xFDC, 0xFE3};
        u8 b = (u8)(U8(0x87FFD8) + 1);
        U8(0x87FFD8) = b;
        U16(c + 0x20) = sway[b & 7];
        if (b & 0x20) U8(0x87FFD8) = ++b;
        if (b & 0x40) U8(0x87FFD8) = b = (u8)(b + 2);
        if (b & 0x80) U8(0x87FFD8) = (u8)(b + 3);
        SetBackColor(0xFF, 0xFF, 0xFF);
    } else {
        U8(g + 0x7C4) = U8(e + 0xD);
        SetBackColor(0x5F, 0x5F, 0x5F);
    }
    for (u32 d = 0; d < 0x150; d += 0x70) {              /* the first three: life back */
        gptr q = U32(g + 0x93C) + d;
        if (U8(q + 0x590) & 2) continue;
        s16 v = S16(q + 0x584);
        U16(q + 0x584) = (u16)(v < 30000 ? v + 500 : 30000);
    }
    U16(e + 0x1E) -= 7;
    if (U16(e + 0x1E) > 0x7D00) U16(e + 0x1E) += 0x30;
    for (u32 o = 0x690, i = 15; o < 0xE70; o += 0x70, i++) {   /* scenery passing by: a new piece when it wraps */
        gptr q = U32(g + 0x93C) + o;
        if (!(U8(q + 0x50) & 1)) continue;
        s16 old = S16(q + 0x44);
        U16(q + 0x44) = (u16)(old + U16(0x87FFC8) + 0x465);
        if (old <= S16(q + 0x44)) continue;
        u32 r1 = Rand8(), r2 = Rand8();
        u16 v = (u16)(((r1 + r2) / 2) & 3);
        if (i - 15 < 4) {
            U16(q + 0x5E) = v;
            U32(q + 0x68) = U32(0x71E958 + 4 * v);
        } else {
            U32(q + 0x68) = U32(0x71E97C + 4 * v);
            if (U32(q - 0x3F8) == U32(0x71E958 + 4 * v)) {   /* (not the same as the one 10 before) */
                v = v == 3 ? 0 : v + 1;
                U32(q + 0x68) = U32(0x71E97C + 4 * v);
            }
            U16(q + 0x5E) = v;
        }
    }
    for (u32 o = 0xF50; o < 0x1880; o += 0x70)
        if (U8(U32(g + 0x93C) + o + 0x50) & 1) U16(U32(g + 0x93C) + o + 0x44) += (u16)(U16(0x87FFC8) + 0x465);
    for (u32 o = 0xF50; o < 0x1180; o += 0x70) {
        gptr q = U32(g + 0x93C) + o + 0x44;
        if (S16(q) >= 0x1F40) U16(q) += 0x63C0;
    }
    for (u32 o = 0x1180; o < 0x12D0; o += 0x70) {
        gptr q = U32(g + 0x93C) + o + 0x44;
        if (S16(q) >= 0x7D00 || S16(q) < -0x5DC0) U16(q) -= 0x5DC0;
    }
    for (u32 o = 0x12D0; o < 0x15E0; o += 0x70) {
        gptr q = U32(g + 0x93C) + o + 0x44;
        if (S16(q) >= 0x5DC0) U16(q) += 0x2540;
    }
}

void DC2CALL Ride_Draw(gptr e)                                       /* 0x416450 */
{
    gptr c = G() + 0x778;
    Ride_Flash();
    Ride_Gauge(U8(e + 0x12) * 255u / 22);
    gptr t = Tmp_Push(0x28), v = t, m = t + 8;
    U16(v) = (u16)-U16(c + 0x28); U16(v + 2) = (u16)-U16(c + 0x2A); U16(v + 4) = (u16)-U16(c + 0x2C);
    ApplyMatrixSV(c, v, v);
    U32(m + 0x14) = (u32)(S16(c + 0x30) + S16(v));
    U32(m + 0x18) = (u32)(S16(c + 0x32) + S16(v + 2));
    U32(m + 0x1C) = (u32)(U16(c + 0x34) + S16(v + 4));
    SetTransMatrix(m);
    Tmp_Pop(0x28);
    if (U8(G() + 0xB4B) == 5 && !U32(0x747880)) {
        Fog_SetColor(U32(0x7479F0), 0x40000000, 0x40400000);   /* (2.0, 3.0) */
        U32(0x747880) = 1;
    }
}

void DC2CALL Ride_Shoot(gptr e)                                      /* 0x416570 */
{
    gptr g = G(), c = g + 0x778;
    gptr s = K2_AllocBack();
    if (!s) return;
    Se_Play(U32(g + 0x940), 0x61);
    gptr m = Tmp_Push(0x14);
    for (u32 a = 0; a < 0x12; a += 2) U16(m + a) = 0;
    U16(m) = 0xE8C; U16(m + 8) = 0xE8C; U16(m + 0x10) = 0xE8C;
    RotMatrixY(-(s32)S16(e + 0x1C), m);
    RotMatrixX(-(s32)S16(e + 0x1A), m);
    U8(s + 0x7A) = 3; U8(s + 0x58) = 0x14;
    gptr pl = U32(g + 0x940);
    U32(s + 0x40) = U32(pl + 0x40); U32(s + 0x44) = U32(pl + 0x44);
    U16(s + 0x40) = U16(c + 0x38); U16(s + 0x42) = 0xF830; U16(s + 0x9E) = 0x1C; U16(s + 0x44) = U16(c + 0x3C);
    U16(s + 0x7C) = (u16)(S16(m + 0xC) / 2); U16(s + 0x7E) = (u16)(S16(m + 0xE) / 2); U16(s + 0x80) = (u16)(S16(m + 0x10) / 2);
    Tmp_Pop(0x14);
    U16(s + 0x6E) = 0x10; U16(s + 0x6C) = 0x10;
    U16(s + 0xA0) = (u16)(U8(0x87FFDA) + 0xC);
    u8 k = (u8)(U8(0x87FFDA) + 1);
    U8(0x87FFDA) = k > 2 ? 0 : k;
    U16(U32(g + 0x93C) + S16(s + 0xA0) * 0x70 + 0x4A) = U16(e + 0x1C);
    Orig_Call1(U32(U32(0x87F948) + U8(s + 0x58) * 4), s);
    Orig_Call1(U32(U32(0x87F948) + U8(s + 0x58) * 4), s);
}

/* a full-screen flash while hit (two semi-transparent F4s, PC float vertices) */
void DC2CALL Ride_Flash(void)                                        /* 0x416700 */
{
    u16 f = U16(0x87FFC0);
    if (!f) return;
    gptr ot = 0x87BC04 + (U32(0x87DE0C) << 12);
    for (u32 k = 0; k < 2; k++) {
        gptr p = U32(0x87DC98);
        static const u32 xy[2][4] = {{0, 0x437F0000, 0, 0x437F0000}, {0x437F0000, 0x43A00000, 0x437F0000, 0x43A00000}};
        U8(p + 4) = 0; U8(p + 5) = (u8)f; U8(p + 6) = (u8)f; U8(p + 7) = 0x2A;
        for (u32 q = 0; q < 4; q++) {
            U32(p + 8 + 0xC * q) = xy[k][q];
            U32(p + 0xC + 0xC * q) = q < 2 ? 0 : 0x43700000;   /* (0, 240) */
            U32(p + 0x10 + 0xC * q) = 0x3C23D70A;
        }
        U32(p) = U32(ot) | 0x05000000;
        U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x38;
    }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x40, 0);
    Gpu_AddPrim(ot, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* the gun's gauge: 4 sprites (the frame, two halves of the bar, the lamp) */
static void ride_sprt(gptr b)
{
    gptr p = U32(0x87DC98), ot = U32(0x87F94C) + 0x7C;
    memcpy(MEM(p), MEM(b), 0x1C);
    U32(p) = U32(ot) | 0x04000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) = p + 0x1C;
}

void DC2CALL Ride_Gauge(u32 v)                                       /* 0x4168B0 */
{
    if (Flag_Check(2, 0xF)) return;
    gptr b = Tmp_Push(0x1C);
    u8 bl = (u8)v;
    U32(b) = 0;
    U32(b + 4) = 0x64808080; U16(b + 0x16) = 0x7F04;
    F32(b + 8) = 208.0f; F32(b + 0xC) = 200.0f; U32(b + 0x10) = 0x3C23D70A;
    U8(b + 0x14) = 0x10; U8(b + 0x15) = 0x80; U16(b + 0x18) = 0x60; U16(b + 0x1A) = 0x18;
    ride_sprt(b);
    F32(b + 0xC) = 208.0f; U8(b + 0x15) = 0x80; U16(b + 0x1A) = 8;
    s32 a;
    if (bl >= 0x80) {
        a = (0xFF - bl) * 48 / 32;
        F32(b + 8) = 208.0f;
        U8(b + 0x14) = (u8)(a + 0x70);
        s16 w = (s16)(0x30 - a);
        U16(b + 0x18) = (u16)(w < 0 ? 0 : w);
    } else {
        U8(b + 0x14) = 0x70;
        a = bl * 96 / 128;
        F32(b + 8) = (float)(s32)((a & 0xFFFF) + 0xD0);
        if ((u16)a <= 0x30) U16(b + 0x18) = 0x30;
        else { s16 w = (s16)(0x60 - a); U16(b + 0x18) = (u16)(w < 0 ? 0 : w); }
    }
    U32(b + 0x10) = 0x3C23D70A;
    ride_sprt(b);
    if (bl >= 0xC0) a = 0;
    else if (bl >= 0x80) a = (0xBF - bl) * 48 / 32;
    else a = bl * 96 / 128;
    F32(b + 0xC) = 208.0f; U32(b + 0x10) = 0x3C23D70A; U8(b + 0x14) = 0x70;
    F32(b + 8) = (float)(s32)((a & 0xFFFF) + 0x100);
    U8(b + 0x15) = 0x88;
    { s16 w = (s16)(0x30 - a); U16(b + 0x18) = (u16)(w < 0 ? 0 : w); }
    U16(b + 0x1A) = 8;
    ride_sprt(b);
    F32(b + 8) = 208.0f; F32(b + 0xC) = 192.0f; U32(b + 0x10) = 0x3C23D70A; U16(b + 0x1A) = 8;
    if (!bl) {
        U8(b + 4) = 0x80; U8(b + 6) = 0; U8(b + 5) = 0;
        U8(b + 0x14) = 0xA0; U8(b + 0x15) = 0x80; U16(b + 0x18) = 0x20;
    } else {
        u16 d = U16(0x87FFCE);
        if (!Flag_Check(2, 0x10)) U16(0x87FFCC) += d;
        s16 cc = S16(0x87FFCC);
        if (cc < 0) { U16(0x87FFCC) = 0; U16(0x87FFCE) = (u16)-d; }
        else if (cc > 0x80) { U16(0x87FFCC) = 0x80; U16(0x87FFCE) = (u16)-d; }
        u8 l = U8(0x87FFCC);
        U8(b + 0x14) = 0xA0; U8(b + 6) = l; U8(b + 5) = l; U8(b + 4) = l;
        U8(b + 0x15) = 0x88; U16(b + 0x18) = 0x38;
    }
    ride_sprt(b);
    Tmp_Pop(0x1C);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x1A, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x7C, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* ---- area kind 3: a strip of VRAM (x 0x1C0..0x1DF, rows 0x100..0x1FF) scrolled
 * a row a frame through the copy at x 0x1E0 (steps 0x71E9F4); 0x7479F4 the row */

static void vram_copy(u32 x, u32 y, u32 w, u32 h, s32 dx, s32 dy)
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)x; U16(r + 2) = (u16)y; U16(r + 4) = (u16)w; U16(r + 6) = (u16)h;
    Gpu_VramCopy(r, dx, dy);
    Tmp_Pop(8);
}

void DC2CALL VScroll(gptr e)                                         /* 0x416C50 */
{
    Orig_Call1(U32(0x71E9F4 + S8(e + 3) * 4), e);
}

void DC2CALL VScroll_Init(gptr e)                                    /* 0x416C70 */
{
    vram_copy(0x1C0, 0x100, 0x20, 0x100, 0x1E0, 0x100);
    U8(e + 3)++;
    VScroll_Step(e);
}

void DC2CALL VScroll_Step(gptr e)                                    /* 0x416CC0 */
{
    (void)e;
    u16 s = U16(0x7479F4);
    vram_copy(0x1E0, 0x100, 0x20, (u16)(0x100 - s), 0x1C0, (s32)(s16)s + 0x100);
    vram_copy(0x1E0, (u16)(0x200 - s), 0x20, s, 0x1C0, 0x100);
    s16 n = (s16)(s + 1);
    U16(0x7479F4) = (u16)(n > 0xFF ? 0 : n);
}

/* ---- area kind 7: a meter on the screen (steps 0x71EA00): filling, flashing,
 * emptying; e+0xC clut, e+0xE/+0x10 u/v (in: VRAM x/y), e+0x12/+0x13 x/y,
 * e+0x1A fill, e+0x1C its shade, e+0x18/+0x1D the lamp's blink, e+0x1E tpage;
 * e+2 == 1: a row of marks, *0x87FD60 +0x7C of them */

void DC2CALL Meter(gptr e)                                           /* 0x416D60 */
{
    Orig_Call1(U32(0x71EA00 + S8(e + 3) * 4), e);
}

void DC2CALL Meter_Init(gptr e)                                      /* 0x416D80 */
{
    U8(e + 3) = 4;
    U32(e + 4) = 0x416F40;
    s16 x = S16(e + 0xE), y = S16(e + 0x10);
    U16(e + 0x1E) = (u16)((((s32)y / 256) << 4) + (s32)x / 64) | 0x20;
    U16(e + 0xE) = (u16)((s32)x % 64);
    U16(e + 0x10) = (u16)(y - 0x100);
    U16(e + 0x1A) = 0x60; U8(e + 0x1C) = 0x60;
    U16(e + 0x18) = 0x80; U8(e + 0x1D) = 0xF0;
}

void DC2CALL Meter_Fill(gptr e)                                      /* 0x416E00 */
{
    gptr g = G();
    if (U16(g + 0x67C) == 9) U16(e + 0x1A) += U8(g + 0x927) ? 0x14 : 0xB;
    else U16(e + 0x1A) += 0xA;
    if (S16(e + 0x1A) > 0xA0) { U16(e + 0x18) = 0x80; U8(e + 0x1D) = 0xF0; U8(e + 3) = 2; }
}

void DC2CALL Meter_Fade(gptr e)                                      /* 0x416E50 */
{
    gptr g = G();
    s8 c;
    if (U16(g + 0x67C) == 9) c = (s8)(U8(e + 0x1C) + (U8(g + 0x927) ? -10 : -6));
    else c = (s8)(U8(e + 0x1C) - 5);
    U8(e + 0x1C) = (u8)c;
    if (c < -0x20) { U16(e + 0x1A) = 0; U8(e + 0x1C) = 0; U8(e + 3)++; }
}

void DC2CALL Meter_Rise(gptr e)                                      /* 0x416EA0 */
{
    gptr g = G();
    U16(e + 0x1A) += U16(g + 0x67C) == 9 && U8(g + 0x927) ? 4 : 3;
    if (S16(e + 0x1A) >= 0x60) {
        U16(e + 0x1A) = 0x60; U8(e + 3) = 4; U8(e + 8) = 0;
        if (U16(G() + 0x67C) == 9) U8(U32(0x87FD60) + 0x2F) = 1;
    }
    U8(e + 0x1C) = U8(e + 0x1A);
}

void DC2CALL Meter_Hold(gptr e)                                      /* 0x416F10 */
{
    if (U16(G() + 0x67C) == 9 && !U8(U32(0x87FD60) + 0x2F)) U8(e + 3) = 1;
}

void DC2CALL Meter_Draw(gptr e)                                      /* 0x416F40 */
{
    gptr g = G();
    if (U16(g + 0x67C) == 8 && (Flag_Check(2, 0xF) || (U16(G() + 0x10B4) & 0xFF00) != 0x900)) return;
    gptr b = Tmp_Push(0x1C);
    u8 st = U8(e + 3);
    U32(b) = 0;
    F32(b + 8) = (float)(s32)U8(e + 0x12); F32(b + 0xC) = (float)(s32)U8(e + 0x13); U32(b + 0x10) = 0x3C23D70A;
    U32(b + 4) = 0x64808080;
    U8(b + 0x14) = (u8)(U8(e + 0xE) + 0x10); U8(b + 0x15) = U8(e + 0x10); U16(b + 0x16) = U16(e + 0xC);
    U16(b + 0x18) = 0x60; U16(b + 0x1A) = 0x18;
    ride_sprt(b);                                       /* the frame */
    U16(b + 0x1A) = 8;
    F32(b + 0xC) = (float)(s32)(U8(e + 0x13) + 8);
    U8(b + 0x15) = U8(e + 0x10);
    s16 w;
    if (st != 1 && st != 2) {                           /* the fill */
        s16 f = S16(e + 0x1A);
        F32(b + 8) = (float)(s32)(U8(e + 0x12) - f + 0x60);
        U8(b + 0x14) = (u8)(U8(e + 0xE) + 0x70);
        w = f > 0x30 ? 0x30 : f;
        U16(b + 0x18) = (u16)w;
        if (w) ride_sprt(b);
    } else {
        F32(b + 8) = (float)(s32)U8(e + 0x12);
        U8(b + 0x14) = (u8)(U8(e + 0xE) + U8(e + 0x1A) + 0x10);
        w = (s16)(0x90 - U16(e + 0x1A));
        U16(b + 0x18) = (u16)w;
        if (w > 0) ride_sprt(b);
    }
    s8 sh = S8(e + 0x1C);                               /* the shade */
    F32(b + 8) = (float)(s32)(U8(e + 0x12) - sh + 0x90);
    F32(b + 0xC) = (float)(s32)(U8(e + 0x13) + 8);
    U8(b + 0x14) = (u8)(U8(e + 0xE) + 0x70);
    U8(b + 0x15) = (u8)(U8(e + 0x10) + 8);
    U16(b + 0x18) = (u16)(sh > 0x30 ? sh - 0x30 : 0);
    U16(b + 0x1A) = 8;
    ride_sprt(b);
    F32(b + 8) = (float)(s32)U8(e + 0x12);              /* the lamp */
    F32(b + 0xC) = (float)(s32)(U8(e + 0x13) - 8);
    if (st == 4 || st == 1) {
        U8(b + 4) = 0x80; U8(b + 6) = 0; U8(b + 5) = 0;
        U8(b + 0x14) = (u8)(U8(e + 0xE) - 0x60); U8(b + 0x15) = U8(e + 0x10); U16(b + 0x18) = 0x20;
    } else {
        if (!Flag_Check(2, 0x22)) U16(e + 0x18) += (u16)(s16)S8(e + 0x1D);
        s16 v = S16(e + 0x18);
        if (v < 0) { U16(e + 0x18) = 0; U8(e + 0x1D) = (u8)-U8(e + 0x1D); }
        else if (v > 0x80) { U16(e + 0x18) = 0x80; U8(e + 0x1D) = (u8)-U8(e + 0x1D); }
        u8 l = U8(e + 0x18);
        U8(b + 6) = l; U8(b + 5) = l; U8(b + 4) = l;
        U8(b + 0x14) = (u8)(U8(e + 0xE) - 0x60); U8(b + 0x15) = (u8)(U8(e + 0x10) + 8); U16(b + 0x18) = 0x38;
    }
    ride_sprt(b);
    if (U8(e + 2) == 1) {                               /* the marks */
        U8(b + 0x14) = (u8)(U8(e + 0xE) - 0x10);
        U16(b + 0x1A) = 0x10; U16(b + 0x18) = 0x10; U16(b + 0x16) = (u16)(U16(e + 0xC) + 1);
        U8(b + 0x15) = (u8)(U8(e + 0x10) + 0x10);
        F32(b + 8) = (float)(s32)(U8(e + 0x12) + 0x60);
        F32(b + 0xC) = (float)(s32)(U8(e + 0x13) + 0x18);
        U32(b + 4) = 0x64008080;
        for (u32 k = 0; k < U8(U32(0x87FD60) + 0x7C); k++) {
            F32(b + 8) -= F32(0x7031B0);
            ride_sprt(b);
        }
    }
    Tmp_Pop(0x1C);
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, (u32)(s32)S16(e + 0x1E), 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x7C, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* ---- area kind 19 (areas with several dinosaurs at once): the camera, by mode
 * G+0x84C (steps 0x71EA30); e+0xC..+0x10 the group's middle, e+0x12 the distance,
 * e+0x14/+0x16 its limits, e+0x18 a blend's clock, e+0x1A the mode last frame;
 * 0x7479F8 the group's middle kept for mode 4 */

static int mc_alive(gptr o) { return (U8(o + 0x50) & 1) && S16(o + 0x108) >= 0; }
static gptr mc_end(void) { return U32(G() + 0x940) + U32(0x87F940) * 0x260; }

/* the eye at the distance the screen asks for, kept within e+0x14..+0x16 */
static s32 mc_dist(gptr a, gptr b, gptr e)
{
    s32 h = ReadGeomScreen();
    s32 d = (s32)(Camera_Distance(a, b) * (u32)h) / 120;
    if ((s32)U16(e + 0x14) > d) d = U16(e + 0x14);
    if ((s32)U16(e + 0x16) < d) d = U16(e + 0x16);
    return d;
}

/* the camera's matrix 15/16 of the way to m */
static void mc_blend(gptr c, gptr m)
{
    for (u32 k = 0; k < 0x12; k += 2) S16(c + k) = (s16)(((s32)S16(c + k) * 15 + S16(m + k)) * 256 / 4096);
}

/* the matrix from the angles c+0x20 (negated; no roll) */
static void mc_rot(gptr c)
{
    gptr r = Tmp_Push(8);
    U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(c + 0x24) = 0; U16(r + 4) = 0; U16(r + 6) = 0;
    RotMatrixZXY(r, c);
    Tmp_Pop(8);
}

void DC2CALL MultiCam(gptr e)                                        /* 0x4173D0 */
{
    if (Flag_Check(7, 0x1F)) return;
    Flag_Set(3, 0, 0);
    if (U8(e + 0x1A) != U8(G() + 0x84C)) U8(e + 3) = 0;
    U8(e + 0x1A) = U8(G() + 0x84C);
    Orig_Call1(U32(0x71EA30 + U8(G() + 0x84C) * 4), e);
}

u16 DC2CALL MultiCam_Y(gptr o)                                       /* 0x417BF0 */
{
    return U8(G() + 0x108F) == 3 ? 0xF448 : U16(U32(o + 0x60) + 0x1E);
}

/* mode 0: behind the player, looking over the ones within 2500 of him */
void DC2CALL MultiCam_Avg(gptr e)                                    /* 0x417430 */
{
    gptr g = G(), pl = U32(g + 0x940), pp = U32(pl + 0x64), c = g + 0x778;
    gptr t = Tmp_Push(0x20), A = t, B = t + 8, C = t + 0x10, sv = t + 0x18;
    if (!U8(e + 3)) {
        U8(e + 3) = 1;
        U16(c + 0x20) = 0xFF00; U16(c + 0x22) = 0; U16(c + 0x32) = 0; U16(c + 0x30) = 0; U16(c + 0x34) = 0xFA0;
        U16(sv) = 0x100; U16(sv + 2) = (u16)-U16(c + 0x22); U16(c + 0x24) = 0; U16(sv + 4) = 0; U16(sv + 6) = 0;
        RotMatrixZXY(sv, c);
        U16(c + 0x28) = U16(pp + 0x40); U16(c + 0x2A) = (u16)(U16(pl + 0x42) - 1000); U16(c + 0x2C) = U16(pp + 0x44);
        U16(e + 0x16) = 30000;
        U16(e + 0x14) = U16(0x71EA14 + U8(G() + 0x108F) * 2);
    }
    for (u32 a = 0; a < 0x18; a += 2) U16(t + a) = 0;
    U16(A) = U16(B) = U16(pp + 0x40);
    U16(A + 2) = U16(B + 2) = (u16)(U16(pl + 0x42) - 1000);
    U16(A + 4) = U16(B + 4) = U16(pp + 0x44);
    s32 sx = S16(A), sy = S16(A + 2), sz = S16(A + 4), n = 1;
    u32 dmax = Camera_Distance(A, c + 0x28);
    for (gptr o = U32(G() + 0x940) + 0x260; o < mc_end(); o += 0x260) {
        if (!mc_alive(o)) continue;
        gptr p = U32(o + 0x64);
        U16(C) = U16(p + 0x40);
        U16(C + 2) = MultiCam_Y(o);
        U16(C + 4) = U16(p + 0x44);
        u32 d = Camera_Distance(C, B);
        if (d > U32(0x71EA48)) continue;
        sx += S16(C); sy += S16(C + 2); sz += S16(C + 4); n++;
        if (dmax < d) { dmax = d; U32(A + 4) = U32(C + 4); U32(A) = U32(C); }
    }
    U16(e + 0xC) = (u16)(sx / n); U16(e + 0xE) = (u16)(sy / n); U16(e + 0x10) = (u16)(sz / n);
    U16(sv) = (u16)mc_dist(A, c + 0x28, e); U16(sv + 2) = 0; U16(sv + 4) = 0;
    ApplyMatrixSV(c, sv, sv);
    U16(c + 0x28) = (u16)(S16(c + 0x28) + (S16(e + 0xC) - S16(c + 0x28)) / 16);
    U16(c + 0x2A) = (u16)(S16(c + 0x2A) + (S16(e + 0xE) - S16(c + 0x2A)) / 16);
    U16(c + 0x2C) = (u16)(S16(c + 0x2C) + (S16(e + 0x10) - S16(c + 0x2C)) / 16);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(sv)); U16(c + 0x3A) = (u16)(U16(c + 0x2A) + U16(sv + 2));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(sv + 4));
    Tmp_Pop(0x20);
    mc_rot(c);
    u32 d = Camera_Distance(c + 0x38, c + 0x28);
    U16(e + 0x12) = (u16)d;
    U16(c + 0x34) = (u16)(U16(c + 0x34) + ((s32)(d & 0xFFFF) - U16(c + 0x34)) / 16);
}

/* modes 1, 2: on the player (blended in over 64 frames in 1), back as far as the
 * farthest within 4000 of him */
void DC2CALL MultiCam_Follow(gptr e)                                 /* 0x4177C0 */
{
    gptr g = G(), pl = U32(g + 0x940), c = g + 0x778;
    gptr t = Tmp_Push(0x20), out = t, V1 = t + 8, V2 = t + 0x10, V3 = t + 0x18;
    for (u32 a = 0; a < 0x20; a += 2) U16(t + a) = 0;
    U16(V1) = U16(V2) = U16(pl + 0x40);
    U16(V1 + 2) = U16(V2 + 2) = (u16)(U16(pl + 0x42) - 1000);
    U16(V1 + 4) = U16(V2 + 4) = U16(pl + 0x44);
    switch (S8(e + 3)) {
    case 0:
        U8(e + 3)++;
        U16(e + 0x18) = 0;
        /* fall through */
    case 1: {
        u16 k = U16(e + 0x18);
        s32 p0 = (s16)((0x40 - k) << 6);
        U16(e + 0x18) = (u16)(k + 1);
        if (k >= 0x40) U8(e + 3)++;
        LoadAverageShort0(c + 0x28, V1, p0, 0x1000 - p0, out);
        U16(c + 0x28) = U16(out); U16(c + 0x2A) = U16(out + 2); U16(c + 0x2C) = U16(out + 4);
        break;
    }
    case 2:
        U16(c + 0x28) = U16(V1); U16(c + 0x2A) = U16(V1 + 2); U16(c + 0x2C) = U16(V1 + 4);
        break;
    }
    u32 dmax = Camera_Distance(V2, c + 0x28);
    for (gptr o = U32(G() + 0x940) + 0x260; o < mc_end(); o += 0x260) {
        if (!mc_alive(o)) continue;
        gptr p = U32(o + 0x64);
        U16(V3) = U16(p + 0x40);
        U16(V3 + 2) = MultiCam_Y(o);
        U16(V3 + 4) = U16(p + 0x44);
        u32 d = Camera_Distance(V3, V1);
        if (d <= U32(0x71EA4C) && dmax < d) { dmax = d; U32(V2) = U32(V3); U32(V2 + 4) = U32(V3 + 4); }
    }
    U16(out) = (u16)mc_dist(V2, c + 0x28, e); U16(out + 2) = 0; U16(out + 4) = 0;
    ApplyMatrixSV(c, out, out);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(out)); U16(c + 0x3A) = (u16)(U16(c + 0x2A) + U16(out + 2));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(out + 4));
    Tmp_Pop(0x20);
    mc_rot(c);
    u32 d = Camera_Distance(c + 0x38, c + 0x28);
    U16(e + 0x12) = (u16)d;
    U16(c + 0x34) = (u16)(U16(c + 0x34) + ((s32)(d & 0xFFFF) - U16(c + 0x34)) / 16);
}

static u32 mc_count(void)
{
    u32 n = 0;
    for (gptr o = U32(G() + 0x940) + 0x260; o < mc_end(); o += 0x260) if (mc_alive(o)) n++;
    return n;
}

/* mode 3: the group from beside the player; when one is left, over to him (16 frames) */
void DC2CALL MultiCam_Group3(gptr e)                                 /* 0x417A70 */
{
    u32 n = mc_count();
    switch (S8(e + 3)) {
    case 0:
        if (!n) return;
        U8(e + 3)++;
        U16(e + 0x18) = 0;
        /* fall through */
    case 1:
        if (n == 1) { MultiCam_Group(e, 0); return; }
        U8(e + 3)++;
        /* fall through */
    case 2: {
        u16 k = U16(e + 0x18);
        s32 p = 0x1000 - (k << 8);
        U16(e + 0x18) = (u16)(k + 1);
        if (k >= 0x10) U8(e + 3)++;
        MultiCam_Group(e, (u32)p);
        return;
    }
    case 3:
        if (n <= 1) U8(e + 3)++;
        MultiCam_Group(e, 0);
        return;
    case 4:
        MultiCam_Focus(e);
        return;
    }
}

/* mode 4: between the player and the group */
void DC2CALL MultiCam_Focus4(gptr e)                                 /* 0x417B70 */
{
    u32 n = mc_count();
    switch (S8(e + 3)) {
    case 0:
        if (!n) return;
        U8(e + 3)++;
        U16(e + 0x18) = 0;
        /* fall through */
    case 1:
        MultiCam_Focus(e);
        return;
    }
}

void DC2CALL MultiCam_Focus(gptr e)                                  /* 0x417C20 */
{
    gptr g = G(), pl = U32(g + 0x940), pp = U32(pl + 0x64);
    gptr t = Tmp_Push(0x38), r = t, P = t + 8, lk = t + 0x10, m = t + 0x18;
    for (u32 a = 0; a < 0x18; a += 2) U16(t + a) = 0;
    U16(P) = U16(pp + 0x40);
    U16(P + 2) = MultiCam_Y(pl);
    U16(P + 4) = U16(pp + 0x44);
    s32 sx = 0, sy = 0, sz = 0, n = 0;
    for (gptr o = U32(G() + 0x940) + 0x260; o < mc_end(); o += 0x260) {
        if (!mc_alive(o)) continue;
        gptr p = U32(o + 0x64);
        sx += S16(p + 0x40);
        sy += (s16)MultiCam_Y(o);
        sz += S16(p + 0x44);
        n++;
    }
    if (n) { U16(0x7479F8) = (u16)(sx / n); U16(0x7479FA) = (u16)(sy / n); U16(0x7479FC) = (u16)(sz / n); }
    gptr c = G() + 0x778;
    U16(c + 0x28) = (u16)((S16(0x7479F8) + S16(P)) / 2);
    U16(c + 0x2A) = (u16)((S16(0x7479FA) + S16(P + 2)) / 2);
    U16(c + 0x2C) = (u16)((S16(0x7479FC) + S16(P + 4)) / 2);
    U16(r) = 0; U16(r + 2) = (u16)Angle_XZ(P, 0x7479F8); U16(r + 4) = 0xFF00;
    RotMatrixYXZ(r, m);
    U16(r) = (u16)mc_dist(P, c + 0x28, e); U16(r + 2) = 0; U16(r + 4) = 0;
    ApplyMatrixSV(m, r, r);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(r)); U16(c + 0x3A) = (u16)(U16(c + 0x2A) + U16(r + 2));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(r + 4));
    Angles_LookAt(c + 0x28, c + 0x38, lk);
    Angles_LookAt(c + 0x28, c + 0x38, lk);
    U16(r) = (u16)-U16(lk); U16(r + 2) = (u16)-U16(lk + 2); U16(r + 4) = 0;
    RotMatrixZXY(r, m);
    mc_blend(c, m);
    VectorNormalS(c, c);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
    Tmp_Pop(0x38);
}

/* the camera over the player from the group's side; p: how far it still is
 * from where it was (0x1000: there) */
void DC2CALL MultiCam_Group(gptr e, u32 p)                           /* 0x417FF0 */
{
    gptr g = G();
    gptr t = Tmp_Push(0x40), out = t, sv = t + 8, P = t + 0x10, Gm = t + 0x18, m = t + 0x20;
    for (u32 a = 0; a < 0x20; a += 2) U16(t + a) = 0;
    s32 sx = 0, sy = 0, sz = 0, n = 0;
    for (gptr o = U32(G() + 0x940) + 0x260; o < mc_end(); o += 0x260) {
        if (!mc_alive(o)) continue;
        gptr q = U32(o + 0x64);
        sx += S16(q + 0x40);
        sy += (s16)MultiCam_Y(o);
        sz += S16(q + 0x44);
        n++;
    }
    if (n) { U16(Gm) = (u16)(sx / n); U16(Gm + 2) = (u16)(sy / n); U16(Gm + 4) = (u16)(sz / n); }
    gptr pl = U32(g + 0x940), pp = U32(pl + 0x64), c = G() + 0x778;
    U16(P) = U16(pp + 0x40);
    U16(P + 2) = MultiCam_Y(pl);
    U16(P + 4) = U16(pp + 0x44);
    s32 q = 0x1000 - (s32)p;
    LoadAverageShort0(c + 0x28, P, (s32)p, q, out);
    U16(c + 0x28) = U16(out); U16(c + 0x2A) = U16(out + 2); U16(c + 0x2C) = U16(out + 4);
    U16(out) = (u16)(U16(P) - U16(c + 0x28)); U16(out + 2) = (u16)(U16(P + 2) - U16(c + 0x2A));
    U16(out + 4) = (u16)(U16(P + 4) - U16(c + 0x2C));
    U16(sv) = 0; U16(sv + 2) = (u16)Angle_XZ(Gm, P); U16(sv + 4) = 0;
    RotMatrixYXZ(sv, m);
    U16(sv) = 0; U16(sv + 2) = 0; U16(sv + 4) = U16(e + 0x14);
    ApplyMatrixSV(m, sv, sv);
    for (u32 k = 0; k < 6; k += 2) U16(out + k) = (u16)(U16(out + k) + U16(sv + k) + U16(c + 0x28 + k));
    LoadAverageShort0(c + 0x38, out, (s32)p, q, out);
    u16 y = U16(c + 0x2A);
    U16(c + 0x38) = U16(out); U16(c + 0x3A) = U16(out + 2); U16(c + 0x3C) = U16(out + 4);
    U16(c + 0x2A) = U16(c + 0x3A);                      /* (the heights swapped: looking down from the player's) */
    U16(c + 0x3A) = y;
    Tmp_Pop(0x40);
    Angles_LookAt(c + 0x28, c + 0x38, c + 0x20);
    mc_rot(c);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
}

/* mode 5: the two first ones from the side */
void DC2CALL MultiCam_Pair(gptr e)                                   /* 0x4182C0 */
{
    if (!U8(e + 3)) {
        U8(e + 3) = 1;
        u8 k = U8(G() + 0x850);
        static const u16 far1[11] = {0x1770, 0x1770, 0x1B58, 0x1770, 0x1F40, 0x1770, 0x2EE0, 0x1770, 0x2328, 0x1770, 0xFA0};
        s32 a = (u32)(k - 0x1D) <= 10 ? far1[k - 0x1D] : 0x1770;
        s32 b = (u32)(k - 0x1E) <= 10 ? far1[k - 0x1E] : 0x1770;
        U16(e + 0x14) = (u16)(a < b ? b : a);
        U16(e + 0x16) = 30000;
    }
    gptr pl = U32(G() + 0x940);
    gptr t = Tmp_Push(0x38), r = t, A = t + 8, B = t + 0x10, lk = t + 0x18, m = t + 0x20;
    for (u32 a = 0; a < 0x20; a += 2) U16(t + a) = 0;
    gptr o1 = pl + 0x260, o2 = pl + 0x4C0;
    U16(A) = U16(U32(o1 + 0x64) + 0x40); U16(A + 2) = MultiCam_Y(o1); U16(A + 4) = U16(U32(o1 + 0x64) + 0x44);
    U16(B) = U16(U32(o2 + 0x64) + 0x40); U16(B + 2) = MultiCam_Y(o2); U16(B + 4) = U16(U32(o2 + 0x64) + 0x44);
    gptr c = G() + 0x778;
    U16(c + 0x28) = (u16)((S16(B) + S16(A)) / 2);
    U16(c + 0x2A) = (u16)((S16(B + 2) + S16(A + 2)) / 2);
    U16(c + 0x2C) = (u16)((S16(B + 4) + S16(A + 4)) / 2);
    U16(r) = 0; U16(r + 2) = (u16)Angle_XZ(A, B); U16(r + 4) = 0xFF00;
    RotMatrixYXZ(r, m);
    U16(r) = (u16)mc_dist(A, c + 0x28, e); U16(r + 2) = 0; U16(r + 4) = 0;
    ApplyMatrixSV(m, r, r);
    U16(c + 0x38) = (u16)(U16(c + 0x28) + U16(r)); U16(c + 0x3A) = (u16)(U16(r + 2) + U16(c + 0x2A));
    U16(c + 0x3C) = (u16)(U16(c + 0x2C) + U16(r + 4));
    Angles_LookAt(c + 0x28, c + 0x38, lk);
    Angles_LookAt(c + 0x28, c + 0x38, lk);
    U16(r) = (u16)-U16(lk); U16(r + 2) = (u16)-U16(lk + 2); U16(r + 4) = 0;
    RotMatrixZXY(r, m);
    mc_blend(c, m);
    VectorNormalS(c, c);
    U16(c + 0x32) = 0; U16(c + 0x30) = 0;
    U16(c + 0x34) = (u16)Camera_Distance(c + 0x38, c + 0x28);
    Tmp_Pop(0x38);
}

/* ---- the duel arena (area kind 21: steps 0x71ECA8): the players' pads, the
 * round's end, the opponent's picture, and spinning shards drawn while
 * 0x747A10 counts down (14 triangles 0x71EA5C, 3 rings of 17 quads 0x747A28);
 * 0x747A02 the shards' size, 0x747A04 their spin, 0x747A08..0C the place,
 * 0x747A18 the frames since, 0x747A14 the OT shift */

static void poly_g3(gptr p)                         /* SetPolyG3 (0x4C0630, PC layer) */
{
    U8(p + 7) = 0x30; U32(p + 0x10) = 0x3C23D70A; U32(p + 0x20) = 0x3C23D70A; U32(p + 0x30) = 0x3C23D70A;
}

static void poly_g4(gptr p)                         /* SetPolyG4 (0x4C0650) */
{
    poly_g3(p);
    U8(p + 7) = 0x38; U32(p + 0x40) = 0x3C23D70A;
}

void DC2CALL Duel(gptr e)                                            /* 0x4186F0 */
{
    Orig_Call1(U32(0x71ECA8 + S8(e + 3) * 4), e);
}

void DC2CALL Duel_Init(gptr e)                                       /* 0x418710 */
{
    U8(e + 3)++;
    U16(0x747A20) = 0; U16(0x747A1E) = 0; U16(0x747A00) = 0; U16(0x747A04) = 0; U16(0x747A1C) = 0;
    U32(e + 4) = 0x418800;
}

void DC2CALL Duel_Step(gptr e)                                       /* 0x418750 */
{
    (void)e;
    Duel_Pad(0, U16(0x87DC94));
    Duel_End();
    Duel_Picture();
    if (U8(0x87E747) != 2) return;
    u32 pad = U32(0x87DC08);                            /* (a debug chord: flag 0/0xB) */
    if (!(pad & 4) || (pad & 9) || !(pad & 2) || !(pad & 0x8000) || !(U8(0x87DC94) & 0x10)) return;
    s16 c = S16(0x747A00);
    if (c < 3) U16(0x747A00) = (u16)++c;
    if (c != 3) return;
    U16(0x747A00) = 0;
    u32 on = !Flag_Check(0, 0xB);
    Flag_Set(0, 0xB, on);
    Flag_Set(0, 0x14, on);
    Se_Play(0, 2);
}

/* pad presses for player i: a new button while 0x87E984+i counts lets it act again */
void DC2CALL Duel_Pad(u32 i, u32 pad)                                /* 0x419250 */
{
    i &= 0xFF;
    if (U8(0x87E984 + i)) {
        U8(0x87E984 + i)--;
        gptr o = U32(0x87E760) + i * 0x260;
        if (U8(o + 0x54) != 1 || !(pad & 0x7F) || (U16(0x87E98C + 2 * i) & pad & 0xFFFF)) return;
        U32(o + 0x54) = 1;
        U16(U32(0x87E760) + i * 0x260 + 0x42) = 0;
        U8(0x87E984 + i) = 0;
        U16(0x87E98C + 2 * i) |= (u16)pad;
        return;
    }
    if (!U16(0x87E98C + 2 * i)) return;
    if (U8(0x87E990 + i)) U8(0x87E990 + i)--;
    if (!U8(0x87E990 + i)) U8(0x87E990 + i) = 0x1E;
    if (U8(0x87E990 + i) == 1) { U8(0x87E990 + i) = 0; U16(0x87E98C + 2 * i) = 0; }
}

/* the round over (0x747A20): fade out, back in */
void DC2CALL Duel_End(void)                                          /* 0x4190B0 */
{
    if (!U16(0x747A20)) return;
    switch (S16(0x747A1E)) {
    case 0:
        Fade_Set(1, 2, 2);
        Flag_Set(2, 0xF, 1);
        U16(0x747A1E)++;
        break;
    case 1:
        if (Flag_Check(2, 6)) return;
        Fade_Set(0, 0x1E, 2);
        U16(0x747A1E)++;
        break;
    case 2:
        if (Flag_Check(2, 6)) return;
        U16(0x747A20) = 0; U16(0x747A1E) = 0;
        Flag_Set(2, 0xF, 0);
        break;
    }
}

/* the opponent's picture's palette, once, by the stage (0x87EEAF) and its kind */
void DC2CALL Duel_Picture(void)                                      /* 0x419140 */
{
    if (U8(0x87E747) != 1 || U16(0x747A1C)) return;
    u32 n = U32(0x87F940);
    gptr pl = U32(G() + 0x940), o = pl + 0x260;
    for (u16 k = (u16)(n - 1); k && !(U8(o + 0x50) & 1); k--) o += 0x260;
    if (o == pl + n * 0x260) return;
    u8 st = U8(0x87EEAF), ty = U8(o + 0x58);
    u32 i;
    if (st == 9) { if (ty != 3) return; i = 0; }
    else if (st == 0xC) { if (ty != 0xE) return; i = 1; }
    else if (st == 8) { if (ty != 8) return; i = 2; }
    else if (st == 7) { if (ty != 7) return; i = 3; }
    else return;
    static const u32 pics[4] = {0x5F31F4, 0x5F33FC, 0x5F3604, 0x5F380C};
    gptr r = Tmp_Push(8);
    U16(r) = 0; U16(r + 2) = 0x1FF; U16(r + 4) = 0x100; U16(r + 6) = 1;
    Gpu_LoadImageIfChanged(r, pics[i] + 8);
    Tmp_Pop(8);
    DrawSync(0);
    U16(0x747A1C) = 1;
}

void DC2CALL Duel_Draw(gptr e)                                       /* 0x418810 */
{
    (void)e;
    if (!U32(0x747A10)) return;
    U16(0x747A04) += 0x20;
    U32(0x747A10)--;
    U32(0x747A18)++;
    U32(0x747A14) = U8(G() + 0xB4B) - 2;
    Duel_Shape();
    gptr c = G() + 0x778, v = Tmp_Push(8);
    U16(v) = (u16)-U16(c + 0x28); U16(v + 2) = (u16)-U16(c + 0x2A); U16(v + 4) = (u16)-U16(c + 0x2C);
    ApplyMatrixSV(c, v, v);
    U32(c + 0x14) = (u32)(S16(c + 0x30) + S16(v));
    U32(c + 0x18) = (u32)(S16(c + 0x32) + S16(v + 2));
    U32(c + 0x1C) = (u32)(U16(c + 0x34) + S16(v + 4));
    Tmp_Pop(8);
    for (u32 k = 0; k < 14; k++) Duel_Tri(0x71EA5C + k * 0x24);
    for (u32 k = 0; k < 51; k++) Duel_Quad(0x747A28 + k * 0x20);
}

/* the shards this frame: the triangles turned by their own angles and the
 * spin, the rings' quads grown along their slopes and spread around */
void DC2CALL Duel_Shape(void)                                        /* 0x418910 */
{
    gptr t = Tmp_Push(0x28), r = t, m = t + 8;
    s16 s = S16(0x747A02);
    U16(r + 6) = 0;
    for (u32 k = 0; k < 14; k++) {
        gptr R = 0x71EA50 + k * 0x24, v = R + 0xC;
        s16 a = S16(R), b = S16(R + 2);
        U16(v) = 0; U16(v + 2) = 0; U16(v + 4) = 0;
        U16(v + 8) = (u16)-(s16)(s * a); U16(v + 0xA) = (u16)-(s16)(b * s); U16(v + 0xC) = 0;
        U16(v + 0x10) = (u16)(s * a); U16(v + 0x12) = (u16)-(s16)(b * s); U16(v + 0x14) = 0;
        RotMatrix(R + 4, m);
        for (u32 q = 0; q < 3; q++) ApplyMatrixSV(m, v + 8 * q, v + 8 * q);
        U16(r) = 0; U16(r + 2) = U16(0x747A04); U16(r + 4) = 0;
        RotMatrix(r, m);
        for (u32 q = 0; q < 3; q++) ApplyMatrixSV(m, v + 8 * q, v + 8 * q);
    }
    gptr Q = 0x747A28;
    for (u32 ring = 0; ring < 3; ring++) {
        gptr A = 0x71EC48 + ring * 0x10, dA = 0x71EC78 + ring * 0x10;
        u16 ang = 0;
        for (u32 k = 0; k < 17; k++, Q += 0x20) {
            u16 tt = U16(0x747A18);
            s = S16(0x747A02);
            for (u32 q = 0; q < 3; q++) {
                u16 x = (u16)((u16)(S16(dA + 2 * q) * tt + S16(A + 2 * q)) * s);
                U16(Q + 8 + 2 * q) = x; U16(Q + 2 * q) = x;
                u16 y = (u16)((u16)(S16(dA + 8 + 2 * q) * tt + S16(A + 8 + 2 * q)) * s);
                U16(Q + 0x18 + 2 * q) = y; U16(Q + 0x10 + 2 * q) = y;
            }
            U16(r) = 0; U16(r + 2) = ang; U16(r + 4) = 0;
            RotMatrix(r, m);
            ApplyMatrixSV(m, Q, Q);
            ApplyMatrixSV(m, Q + 0x10, Q + 0x10);
            ang = (u16)(ang + 0x100);
            U16(r) = 0; U16(r + 2) = ang; U16(r + 4) = 0;
            RotMatrix(r, m);
            ApplyMatrixSV(m, Q + 8, Q + 8);
            ApplyMatrixSV(m, Q + 0x18, Q + 0x18);
        }
    }
    Tmp_Pop(0x28);
}

/* a shard: n vertices at p (stride 8) moved to 0x747A08.., shaded G3/G4 */
static void duel_shard(gptr p, u32 n)
{
    gptr t = Tmp_Push(0x28), v = t, pq = t + 0x20, fl = t + 0x24;
    for (u32 k = 0; k < n; k++) {
        U16(v + 8 * k) = (u16)(U16(p + 8 * k) + U16(0x747A08));
        U16(v + 8 * k + 2) = (u16)(U16(p + 8 * k + 2) + U16(0x747A0A));
        U16(v + 8 * k + 4) = (u16)(U16(0x747A0C) + U16(p + 8 * k + 4));
    }
    u8 l = (u8)((s32)(U32(0x747A10) * 240) / 25);
    gptr c = G() + 0x778;
    SetRotMatrix(c);
    SetTransMatrix(c);
    gptr pr = U32(0x87DC98);
    u32 z = n == 3 ? (u32)RotTransPers3(v, v + 8, v + 0x10, pr + 8, pr + 0x18, pr + 0x28, pq, fl)
                   : (u32)RotTransPers4(v, v + 8, v + 0x10, v + 0x18, pr + 8, pr + 0x18, pr + 0x28, pr + 0x38, pq, fl);
    Tmp_Pop(0x28);
    z >>= U32(0x747A14) & 31;
    u32 otz = z >= 0x3FF ? 0x3FF : z;
    if (n == 3) { poly_g3(pr); PC_stsz3_g3(pr); }
    else { poly_g4(pr); PC_stsz4_g4(pr); }
    Gpu_SetSemiTrans(pr, 1);
    for (u32 k = 0; k < n; k++) {
        u8 b = k < n - 2 ? l : 0;
        U8(pr + 0x10 * k + 4) = b; U8(pr + 0x10 * k + 5) = b; U8(pr + 0x10 * k + 6) = b;
    }
    gptr ot = 0x87BC00 + ((U32(0x87DE0C) << 10) + otz) * 4;
    Gpu_AddPrim(ot, pr);
    U32(0x87DC98) = pr + (n == 3 ? 0x34 : 0x44);
    Gpu_SetDrawMode(U32(0x87DC98), 0, 0, 0x20, 0);
    Gpu_AddPrim(ot, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

void DC2CALL Duel_Tri(gptr p) { duel_shard(p, 3); }                  /* 0x418BA0 */
void DC2CALL Duel_Quad(gptr p) { duel_shard(p, 4); }                 /* 0x418D60 */

/* ---- the waves (area kind 20: steps 0x71ECDC): kill G+0x84C's number
 * (0x71ECC8) in this room (G+0x12D0+room), then the next wave (0x71ECB4) is
 * loaded; after the last, the results; e+8/+9 a sub-step and its clock */

static u32 waves_room(void) { return Room_GlobalIndex(U8(G() + 0x1091), U8(G() + 0x1090)) & 0xFF; }

void DC2CALL Waves(gptr e)                                           /* 0x419310 */
{
    Orig_Call1(U32(0x71ECDC + S8(e + 3) * 4), e);
}

void DC2CALL Waves_Init(gptr e)                                      /* 0x419330 */
{
    U32(e + 4) = 0x419710;
    U8(e + 3)++;
}

void DC2CALL Waves_Count(gptr e)                                     /* 0x419350 */
{
    u32 r = waves_room();
    gptr g = G();
    if (U8(g + 0x12D0 + r) < U32(0x71ECC8 + U8(g + 0x84C) * 4)) return;
    U32(g + 0xA68) = 0;
    U8(e + 3)++;
}

void DC2CALL Waves_Clear(gptr e)                                     /* 0x4193B0 */
{
    u8 s = U8(e + 8);
    switch (s) {
    case 0:
        Snd_FadeOutAll();
        U8(e + 9) = 0x1E;
        U8(e + 8)++;
        break;
    case 1: {
        u8 t = U8(e + 9);
        U8(e + 9) = (u8)(t - 1);
        if (!t) U8(e + 8) = (u8)(s + 1);
        break;
    }
    case 2: {
        gptr pl = U32(G() + 0x940);
        for (gptr o = pl + 0x260; o < pl + U32(0x87F940) * 0x260; o += 0x260) if (U32(o + 0x50)) return;
        U8(e + 3)++;
        U8(G() + 0x84C)++;
        if (U8(G() + 0x84C) > 4) {
            Flag_Set(1, 0xE, 1);
            U8(e + 3) = 4;
            U8(G() + 0x84C) = 4;
        }
        U8(e + 9) = 0; U8(e + 8) = 0;
        break;
    }
    }
}

void DC2CALL Waves_Load(gptr e)                                      /* 0x4194A0 */
{
    gptr w = U32(0x71ECB4 + U8(G() + 0x84C) * 4);
    u32 lv = U8(w + 4) & 0xF, kind = U8(w + 1);
    switch (U8(e + 8)) {
    case 0:
        if (File_IsBusy()) return;
        U8(e + 8)++;
        File_RequestLoad(U16(0x703520 + kind * 8));
        Flag_Set(2, 0xF, 1);
        /* fall through */
    case 1: {
        if (File_IsBusy()) return;
        u8 s = (u8)(U8(e + 8) + 1);
        U8(e + 8) = s;
        if (kind != 2 && kind != 7) { U8(e + 8) = (u8)(s + 1); return; }
        u32 i = (u32)((s32)S16(G() + 0x1388) * 3 + (s32)lv);
        if (kind == 2) File_RequestLoad(U32(0x703E48 + (i > 9 ? 9 : i) * 4));
        else File_RequestLoad(U32(0x703E70 + (i > 8 ? 8 : i) * 4));
    }
        /* fall through */
    case 2:
        if (File_IsBusy()) return;
        U8(e + 8) = 5;
        break;
    default:
        break;
    }
    Flag_Set(2, 0xF, 0);
    U8(e + 3) = 0; U8(e + 8) = 0;
    U32(G() + 0xA68) = w;
    U8(G() + 0x12D0 + waves_room()) = 0;
    gptr g = G();
    U8(g + 0xA84) = 0;
    memset(MEM(g + 0xA98), 0, 0x20);
    memset(MEM(g + 0xA80), 0, 0x48);
    U8(g + 0xA89) = 0xFF;
    U8(g + 0xAD1) = 0xFF;
}

void DC2CALL Waves_End(gptr e)                                       /* 0x419640 */
{
    switch (U8(e + 8)) {
    case 0: {
        U8(e + 8)++;
        Fade_Set(1, 0x5A, 1);
        Strm_SetFade(0, 3, 0, 0x80);
        gptr g = G(), pl = U32(g + 0x940);
        s16 m = S16(pl + 0x10A);
        U16(g + 0x84E) = (u16)(m ? (s32)S16(pl + 0x108) * 100 / m : 0);
        u8 d = U8(G() + 0x108F);
        if (!d || d == 6) U32(G() + 0x684) /= 3;
        break;
    }
    case 1:
        if (Flag_Check(2, 6)) return;
        Room_Reset();
        Game_Start(0x18);
        break;
    }
}

/* the kills still to go, beside a picture */
void DC2CALL Waves_Draw(gptr e)                                      /* 0x419710 */
{
    if (U8(e + 3) != 1 && U8(e + 3) != 2) return;
    gptr p = U32(0x87DC98);
    Gpu_SetSprt(p);
    U32(p + 4) = 0x64808080; U32(p + 8) = 0x42FA0000; U32(p + 0xC) = 0x43340000; U32(p + 0x10) = 0x3C23D70A;
    U8(p + 0x14) = 0; U8(p + 0x15) = 0; U16(p + 0x18) = 0x30; U16(p + 0x1A) = 0x20; U16(p + 0x16) = 0x7E80;
    Gpu_AddPrim(U32(0x87F94C) + 0x88, p);
    U32(0x87DC98) = p + 0x1C;
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x1D, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x88, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
    u32 r = waves_room();
    gptr g = G();
    Menu_Number11(0xA6, 0xC2, 2, 0, 0x808080, U32(0x71ECC8 + U8(g + 0x84C) * 4) - U8(g + 0x12D0 + r), 1);
}

/* ---- the versus duel (area kind 22: steps 0x71ECF0) between objects 1 and 2
 * (*0x87E760; the scenery: *0x87E75C, 0x70 each): an intro flown over the
 * arena, the fight with a timer (e+0x10), then the winner, a time-out or a
 * draw; e+0xD a sub-step, e+0xE a random intro or pose, e+0x12 a clock,
 * e+0x14..+0x1C a camera (height, z, angles, distance), e+0x1E/+0x20/+0x22 the
 * players' combo counts, their shown counts and its clock, e+0x24 the winner */

void DC2CALL Versus(gptr e)                                          /* 0x419850 */
{
    Orig_Call1(U32(0x71ECF0 + S8(e + 3) * 4), e);
    if (Flag_Check(7, 0x1F)) Vs_Camera(e);
    Flag_Set(3, 0, 0);
    for (u32 k = 0; k < 2; k++) {
        u8 c = U8(e + 0x22 + k);
        if (!c) continue;
        U8(e + 0x22 + k) = --c;
        if (!c) U8(e + 0x20 + k) = 0;
    }
}

/* the camera from e+0x14.. */
void DC2CALL Vs_Camera(gptr e)                                       /* 0x41BA60 */
{
    gptr c = G() + 0x778;
    U16(c + 0x28) = 0; U16(c + 0x2A) = U16(e + 0x14); U16(c + 0x2C) = U16(e + 0x16);
    U16(c + 0x20) = U16(e + 0x18); U16(c + 0x22) = U16(e + 0x1A); U16(c + 0x24) = 0;
    U16(c + 0x34) = U16(e + 0x1C);
    Camera_LookAt(c + 0x28, c + 0x30, c + 0x20, c + 0x38);
    U16(c + 0x20) &= 0xFFF; U16(c + 0x22) &= 0xFFF; U16(c + 0x24) &= 0xFFF;
    gptr r = Tmp_Push(8);
    U16(r) = (u16)-U16(c + 0x20); U16(r + 2) = (u16)-U16(c + 0x22); U16(r + 4) = U16(c + 0x24); U16(r + 6) = 0;
    RotMatrix(r, c);
    Tmp_Pop(8);
}

/* 0, 1: who has more of their life left; 0xFF: even */
u8 DC2CALL Vs_Winner(void)                                           /* 0x41BB00 */
{
    gptr o = U32(0x87E760);
    s32 a = S16(o + 0x368) * 6000 / S16(o + 0x36A), b = S16(o + 0x5C8) * 6000 / S16(o + 0x5CA);
    return a == b ? 0xFF : (u8)(a <= b);
}

/* a piece of scenery hopping (for the winner): up, wait, fall with a shake, wobble */
void DC2CALL Vs_Hop(gptr e, gptr o)                                  /* 0x41B940 */
{
    static const s16 wob[8] = {-10, 8, -6, 4, -3, 2, -1, 0};
    s8 st = S8(o + 0x46);
    switch (st) {
    case 0:
        U8(o + 0x46) = (u8)(st + 1);
        U32(o + 0x50) = 3;
        U8(o + 0x47) = (u8)(Rand8() % 5 + 1);
        U16(o + 0x42) = (u16)(Rand8() - 10000);
        break;
    case 1: {
        s8 c = (s8)(U8(o + 0x47) - 1);
        U8(o + 0x47) = (u8)c;
        if (c <= 0) U8(o + 0x46) = (u8)(st + 1);
        break;
    }
    case 2:
        U16(o + 0x62) += 0xB4;
        U16(o + 0x42) += U16(o + 0x62);
        if (S16(o + 0x42) < -2000) break;
        U16(o + 0x42) = (u16)-2000;
        U8(o + 0x46) = (u8)(st + 1); U8(o + 0x47) = 0;
        Shake_Set(3, 0, 0x14);
        break;
    case 3: {
        s8 c = S8(o + 0x47);
        U16(o + 0x4C) = (u16)wob[c];
        U8(o + 0x47) = (u8)++c;
        if (c < 8) break;
        U8(o + 0x46) = (u8)(st + 1);
        U8(e + 0x12)++;
        break;
    }
    }
}

/* player i's moves against j: a combo knock-back on j (0x87E986.. a clock,
 * 0x87E988 the push), pad presses for the next move (as Duel_Pad) */
void DC2CALL Vs_Pad(gptr e, u32 i, u32 j, u32 pad)                   /* 0x41B630 */
{
    j &= 0xFF;
    gptr o = U32(0x87E760);
    if (U8(o + j * 0x260 + 0x2B4) == 1) U8(e + 0x1E + (i & 0xFF)) = 0;
    if (Flag_Check(7, 0x1B)) { U8(0x87E986 + (i & 0xFF)) = 0; U16(0x87E988 + 2 * (i & 0xFF)) = 0; }
    i &= 0xFF;
    u32 f = i == 0 ? 0x1C : i == 1 ? 0x1D : 0;           /* (others: the stack's zeros) */
    if (Flag_Check(7, f)) {                             /* a hit: j's combo grows */
        Flag_Set(7, f, 0);
        U8(0x87E986 + i) = 0xA;
        U16(0x87E988 + 2 * i) = U16(U32(0x87E760) + i * 0x260 + 0x3D6);
        U8(0x87E984 + j) = 0xA;
        s8 c = (s8)(U8(e + 0x1E + j) + 1);
        U8(e + 0x1E + j) = (u8)c;
        if (c > 0x63) U8(e + 0x1E + j) = 0x63;
        U8(e + 0x20 + j) = U8(e + 0x1E + j);
        U8(e + 0x22 + j) = 0x5A;
    }
    if (U8(0x87E986 + i) && !Flag_Check(7, 0x1B)) {
        gptr v = U32(0x87E760) + (i + 1) * 0x260, sv = Tmp_Push(8);
        s16 q = (s16)(S16(0x87E988 + 2 * i) * 2 / 3);
        U16(sv) = 0; U16(sv + 2) = 0; U16(sv + 4) = (u16)(q * (s16)S8(0x87E986 + i));
        Vec_RotY(sv, sv, U16(v + 0x178));
        U16(v + 0x40) += U16(sv); U16(v + 0x42) += U16(sv + 2); U16(v + 0x44) += U16(sv + 4);
        Tmp_Pop(8);
        Obj_UpdateMatrix(v);
        if (!(U8(v + 0x50) & 0x20)) Obj_CollideObjs(v);
        if (!(U8(v + 0x50) & 0x10)) Obj_CollideWalls(v);
        if (U16(v + 0xE8) || U16(v + 0xEA) || U16(v + 0xEC)) {   /* pushed into the other: it moves too */
            gptr a = U32(0x87E760) + (j + 1) * 0x260;
            U16(a + 0x40) += U16(v + 0xE8); U16(a + 0x42) += U16(v + 0xEA); U16(a + 0x44) += U16(v + 0xEC);
            if (!(U8(a + 0x50) & 0x20)) Obj_CollideObjs(a);
            if (!(U8(a + 0x50) & 0x10)) Obj_CollideWalls(a);
        }
        U8(0x87E986 + i)--;
    }
    if (U8(0x87E984 + i)) {
        U8(0x87E984 + i)--;
        gptr p = U32(0x87E760) + i * 0x260;
        if (U8(p + 0x2B4) != 1 || !(pad & 0x7F) || (U16(0x87E98C + 2 * i) & pad & 0xFFFF)) return;
        U32(p + 0x2B4) = 1;
        U16(U32(0x87E760) + i * 0x260 + 0x2A2) = 0;
        U8(0x87E984 + i) = 0;
        U8(0x87E986 + j) = 0;
        U16(0x87E98C + 2 * i) |= (u16)pad;
        return;
    }
    if (U8(U32(0x87E760) + j * 0x260 + 0x2B4) != 1 || !U16(0x87E98C + 2 * i)) return;
    if (U8(0x87E990 + i)) U8(0x87E990 + i)--;
    if (!U8(0x87E990 + i)) U8(0x87E990 + i) = 0x14;
    if (U8(0x87E990 + i) == 1) { U8(0x87E990 + i) = 0; U16(0x87E98C + 2 * i) = 0; }
}

void DC2CALL Vs_Init(gptr e)                                         /* 0x4198B0 */
{
    gptr o = U32(0x87E760);
    U32(e + 0xC) = 1; U8(e + 3) = 1;
    U32(o + 0x50) = 0; U16(o + 0x108) = 0x4B0;
    Flag_Set(1, 0, 1);
    Flag_Set(2, 0xF, 1);
    Flag_Set(7, 0x1F, 1);
    Flag_Set(3, 7, 0);
    U8(0x87EEAF) = 4;
    U16(e + 0x10) = 0xBB7;
    U16(0x748090) = 0x1000; U16(0x748098) = 0x1000; U16(0x7480A0) = 0x1000;
    U32(0x7480E0) = 3;
    U16(0x748092) = 0; U16(0x748094) = 0; U16(0x748096) = 0; U16(0x74809A) = 0; U16(0x74809C) = 0; U16(0x74809E) = 0;
    U32(0x7480A4) = 0; U32(0x7480A8) = 0; U32(0x7480AC) = 0;
    for (u32 k = 0; k < 28; k++) U32(U32(0x87E75C) + 0x600 + k * 0x70) = 1;
}

/* the scenery's pieces k (0x70 each from *0x87E75C) */
#define VS_OBJ(k) (U32(0x87E75C) + (k) * 0x70)

void DC2CALL Vs_Intro(gptr e)                                        /* 0x419990 */
{
    static const s16 x0[4] = {0, 0, 0, -0x578}, y0[4] = {0, 0, 0, 0x1F40}, a0[4] = {0xC00, 0xC00, 0x400, 0},
                     b0[4] = {0, 0x800, 0x400, 0}, d0[4] = {-0x15A0, -0x15A0, 0x7530, 0x8FC};
    static const s16 dx[4] = {0, 0, 0, 0x1F}, dy[4] = {0, 0, 0, -0xB1}, da[4] = {0x10, 0x10, -0x1D, -6},
                     db[4] = {0, 0x2D, -0x16, 0}, dd[4] = {-1000, -1000, -0x14D, 0x11A};
    static const s16 v1[5] = {0xC8, 0xFA, 0x12C, 0x163, 0x19A}, x1[5] = {-2000, -1000, 0, 0x44C, 0x898},
                     v2x[5] = {0x1B8, 0x1A4, 0, -0x1B8, -0x1B8}, v2y[5] = {0x140, -0x2D0, 0x280, -0x2D0, 0x140},
                     x2[5] = {-0x76C, -0x44C, -0x96, 0x320, 0x708};
    switch (U8(e + 0xD)) {
    case 0: {                                           /* one of four flights, a voice */
        u32 k = (Rand8() & 0xFF) % 4;
        U8(e + 0xE) = (u8)k;
        U16(e + 0x14) = (u16)x0[k]; U16(e + 0x16) = (u16)y0[k]; U16(e + 0x18) = (u16)a0[k];
        U16(e + 0x1C) = (u16)d0[k]; U16(e + 0x1A) = (u16)b0[k];
        U8(e + 0x12) = 0x2D;
        U8(e + 0xD)++;
        Voice_Play(1, 0x3EF);
        gptr g = G();
        U8(g + 0xB58) = 0x80; U8(g + 0xB59) = 0x80; U8(g + 0xB5A) = 0x80; U8(g + 0xB5B) = 0; U8(g + 0xB5C) = 0;
        return;
    }
    case 1: {
        u32 k = U8(e + 0xE);
        U16(e + 0x14) += (u16)dx[k]; U16(e + 0x16) += (u16)dy[k]; U16(e + 0x18) += (u16)da[k];
        U16(e + 0x1A) += (u16)db[k]; U16(e + 0x1C) += (u16)dd[k];
        U8(e + 0x12)--;
        if (S8(e + 0x12) <= 0) { Fade_Set(1, 2, 2); U8(e + 0xD)++; }
        DrawSync(0);
        return;
    }
    case 2:
        if (!Flag_Check(2, 6)) {
            Flag_Set(7, 0x1F, 0);
            Fade_Set(0, 0x1E, 2);
            U8(e + 0xD)++;
        }
        DrawSync(0);
        return;
    case 3:                                             /* the letters fly in */
        if (Flag_Check(2, 6)) return;
        U8(e + 0x12) = 0x14;
        U8(e + 0xD)++;
        for (u32 k = 0; k < 5; k++) {
            gptr q = VS_OBJ(36 + k);
            U32(q + 0x50) = 3; U32(q + 0x54) = 0x748090; U16(q + 0x40) = 0xE890;
        }
        return;
    case 4:
        for (u32 k = 0; k < 5; k++) U16(VS_OBJ(36 + k) + 0x40) += (u16)v1[k];
        U8(e + 0x12)--;
        if (S8(e + 0x12) > 0) return;
        U8(e + 0x12) = 0x14;
        U8(e + 0xD)++;
        for (u32 k = 0; k < 5; k++) U16(VS_OBJ(36 + k) + 0x40) = (u16)x1[k];
        return;
    case 5:
        U8(e + 0x12)--;
        if (S8(e + 0x12) > 0) return;
        U8(e + 0x12) = 5;
        U8(e + 0xD)++;
        for (u32 k = 0; k < 5; k++) U32(VS_OBJ(36 + k) + 0x50) = 0;
        for (u32 k = 0; k < 5; k++) {
            gptr q = VS_OBJ(31 + k);
            U32(q + 0x50) = 3; U32(q + 0x54) = 0x748090;
        }
        return;
    case 6:
        for (u32 k = 0; k < 5; k++) {
            gptr q = VS_OBJ(31 + k);
            U16(q + 0x40) += (u16)v2x[k]; U16(q + 0x42) += (u16)v2y[k];
        }
        U8(e + 0x12)--;
        if (S8(e + 0x12) <= 0) {
            U8(e + 0xD)++;
            Fade_Set(1, 2, 2);
            for (u32 k = 0; k < 5; k++) {
                gptr q = VS_OBJ(31 + k);
                U16(q + 0x40) = (u16)x2[k]; U16(q + 0x42) = 0x1F4;
            }
        }
        DrawSync(0);
        return;
    case 7:
        if (!Flag_Check(2, 6)) { Fade_Set(0, 0x1E, 2); U8(e + 0xD)++; }
        DrawSync(0);
        return;
    case 8:
        if (Flag_Check(2, 6) || Flag_Check(2, 0)) return;
        for (u32 k = 0; k < 5; k++) U32(VS_OBJ(31 + k) + 0x50) = 0;
        Flag_Set(1, 0, 0);
        U32(e + 0xC) = 2; U8(e + 3) = 2;
        Flag_Set(2, 0xF, 0);
        U32(e + 4) = 0x41A940;
        return;
    }
}

static void vs_over(gptr e, u32 st)
{
    if (st) { U32(e + 0xC) = st; U8(e + 3) = (u8)st; }
    Flag_Set(1, 0, 1);
    Flag_Set(2, 0xF, 1);
    Strm_SetFade(0, 3, 0, 0x80);
    U32(U32(0x87E760) + 0x2B0) &= ~8u;
    U32(U32(0x87E760) + 0x510) &= ~8u;
}

void DC2CALL Vs_Fight(gptr e)                                        /* 0x419F60 */
{
    if (U16(e + 0x10)) U16(e + 0x10)--;
    if (Flag_Check(0, 0x14)) U16(e + 0x10) = 0xBB7;
    if (S16(e + 0x10) < 0x1E) { vs_over(e, 4); return; }  /* time out */
    gptr o = U32(0x87E760);
    s16 a = S16(o + 0x368), b = S16(o + 0x5C8);
    if (a < 0 && b < 0) { vs_over(e, 5); return; }      /* both down */
    if (a < 0 || b < 0) { vs_over(e, 3); return; }
    Vs_Pad(e, 0, 1, U16(0x87DC94));
    Vs_Pad(e, 1, 0, U16(0x87DC92));
}

static void vs_lights(u8 b5c)
{
    gptr g = G();
    U8(g + 0xB58) = 0x80; U8(g + 0xB59) = 0x80; U8(g + 0xB5A) = 0x80; U8(g + 0xB5B) = 0; U8(g + 0xB5C) = b5c;
}

static void vs_unhold(void)
{
    U32(U32(0x87E760) + 0x2B0) &= ~8u;
    U32(U32(0x87E760) + 0x510) &= ~8u;
}

/* one down: the winner's pieces hop, a pose, a voice by its kind */
void DC2CALL Vs_Win(gptr e)                                          /* 0x41A060 */
{
    static const s16 ty[4] = {-0x78, -0x78, -0x78, -0x78}, ta[4] = {-0x11, -0x16, -2, 0x16},
                     tb[4] = {0, 0x16, -0x2D, -0x16}, td[4] = {0x4BB, 0x4BB, 0x1E8, 0x4BB};
    vs_unhold();
    switch (U8(e + 0xD)) {
    case 0:
        vs_lights(0);
        U32(0x87DE00) = 3;
        U8(e + 0x12) = 0x3C;
        U8(e + 0xD)++;
        return;
    case 1:
        DrawSync(0);
        U8(e + 0x12)--;
        if (S8(e + 0x12) > 0) return;
        U32(0x87DE00) = 2;
        U8(e + 0xD)++;
        return;
    case 2: {
        if (!Flag_Check(7, 0x1E) || Flag_Check(2, 0)) return;
        U8(e + 0x24) = Vs_Winner();
        U16(e + 0x14) = 0xF18C; U16(e + 0x16) = 0x1518; U16(e + 0x18) = 0; U16(e + 0x1A) = 0; U16(e + 0x1C) = 0x1F4;
        U8(e + 0xD)++;
        U8(e + 0x12) = 0;
        Flag_Set(7, 0x1F, 1);
        Voice_Play(1, U8(e + 0x24) ? 0x3EC : 0x3EB);
        Flag_Set(3, 1, 0);
        return;
    }
    case 3:
        Vs_Hop(e, U32(0x87E75C) + (U8(e + 0x24) ? 0x620 : 0x5B0));
        for (u32 k = 0; k < 4; k++) Vs_Hop(e, U32(0x87E75C) + 0x690 + k * 0x70);
        if (S8(e + 0x12) < 5) return;
        U8(e + 0x12) = 0xF;
        U8(e + 0xD)++;
        return;
    case 4: {
        if (U8(e + 0x12)) U8(e + 0x12)--;
        if (S8(e + 0x12) > 0 || Flag_Check(2, 0)) return;
        U8(e + 0xD)++;
        U8(e + 0xE) = (u8)((Rand8() & 0xFF) % 4);
        U8(e + 0x12) = 0x2D;
        static const u16 voices[7] = {0x3FA, 0x3FB, 0x3F7, 0x3F6, 0x3F8, 0x3FC, 0x3F9};
        u32 k = U8(U32(0x87E760) + U8(e + 0x24) * 0x260 + 0x2B8) - 0x1D;
        if (k <= 0xD) Voice_Play(1, voices[k / 2]);
        vs_lights(1);
        Flag_Set(3, 1, 1);
        return;
    }
    case 5: {
        u32 k = U8(e + 0xE);
        U16(e + 0x18) += (u16)ta[k]; U16(e + 0x16) += (u16)ty[k];
        U16(e + 0x1C) += (u16)td[k]; U16(e + 0x1A) += (u16)tb[k];
        s8 c = (s8)(U8(e + 0x12) - 1);
        U8(e + 0x12) = (u8)c;
        if (c <= 0) { U32(e + 0xC) = 6; U8(e + 3) = 6; }
        if (c == 0x14) Fade_Set(1, 0x14, 1);
        DrawSync(0);
        return;
    }
    }
}

/* the time over: the TIME letters run across, then a winner or a draw */
void DC2CALL Vs_Timeout(gptr e)                                      /* 0x41A470 */
{
    static const u8 ids[8] = {0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A};
    static const s16 xs[8] = {-0x1388, -0xE74, -0x834, -0x1F4, 0x44C, 0x9C4, 0xF3C, 0x13EC};
    vs_unhold();
    switch (U8(e + 0xD)) {
    case 0:
        if (Flag_Check(2, 0)) return;
        for (u32 k = 0; k < 8; k++) {
            gptr q = VS_OBJ(ids[k]);
            U16(q + 0x40) = (u16)(xs[k] + 0x2710); U16(q + 0x42) = 0x1F4; U16(q + 0x44) = 0x1388;
            U32(q + 0x50) = 3; U32(q + 0x54) = 0x748090;
        }
        U8(e + 0xD)++;
        Voice_Play(1, 0x3F1);
        return;
    case 1:
        if (S16(U32(0x87E75C) + 0xBA0) < (s16)0xE890) U8(e + 0xD)++;
        for (u32 k = 0; k < 8; k++) U16(VS_OBJ(ids[k]) + 0x40) -= 400;
        return;
    case 2:
        for (u32 k = 0; k < 8; k++) U32(VS_OBJ(ids[k]) + 0x50) = 0;
        Flag_Set(7, 0x1E, 1);
        if (Vs_Winner() == 0xFF) { U32(e + 0xC) = 0x205; U8(e + 3) = 5; }
        else { U32(e + 0xC) = 0x203; U8(e + 3) = 3; }
        return;
    }
}

/* a draw: the DRAW letters drop in and hop */
void DC2CALL Vs_Draw(gptr e)                                         /* 0x41A650 */
{
    static const u8 ids[4] = {0x1B, 0x1C, 0x1D, 0x1E};
    static const s16 xs[4] = {-0xC1C, -0x4B0, 0x2BC, 0xC1C};
    vs_unhold();
    u8 s = U8(e + 0xD);
    switch (s) {
    case 0:
        vs_lights(0);
        U8(e + 0xD) = (u8)(s + 1);
        U8(e + 0x12) = 0x3C;
        return;
    case 1:
        DrawSync(0);
        U8(e + 0x12)--;
        if (S8(e + 0x12) > 0) return;
        U32(0x87DE00) = 2;
        U8(e + 0xD)++;
        return;
    case 2:
        if (!Flag_Check(7, 0x1E) || Flag_Check(2, 0)) return;
        U16(e + 0x14) = 0xF18C; U16(e + 0x16) = 0x1518; U16(e + 0x18) = 0; U16(e + 0x1A) = 0; U16(e + 0x1C) = 0x1F4;
        U8(e + 0xD)++;
        U8(e + 0x12) = 0;
        Flag_Set(7, 0x1F, 1);
        for (u32 k = 0; k < 4; k++) { gptr q = VS_OBJ(ids[k]); U16(q + 0x40) = (u16)xs[k]; U16(q + 0x44) = 0x32C8; }
        Voice_Play(1, 0x3F0);
        Flag_Set(3, 1, 0);
        return;
    case 3:
        for (u32 k = 0; k < 4; k++) Vs_Hop(e, U32(0x87E75C) + 0xBD0 + k * 0x70);
        if (S8(e + 0x12) < 4) return;
        U8(e + 0x12) = 0xF;
        U8(e + 0xD)++;
        return;
    case 4:
        U8(e + 0x12)--;
        if (S8(e + 0x12) > 0) return;
        U8(e + 0xD) = (u8)(s + 1);
        U8(e + 0x12) = 0x2D;
        return;
    case 5: {
        s8 c = (s8)(U8(e + 0x12) - 1);
        U8(e + 0x12) = (u8)c;
        if (c <= 0) { U32(e + 0xC) = 6; U8(e + 3) = 6; }
        if (c == 0x28) Fade_Set(1, 0x28, 3);
        return;
    }
    }
}

void DC2CALL Vs_Exit(gptr e)                                         /* 0x41A8E0 */
{
    switch (U8(e + 0xD)) {
    case 0:
        U8(e + 0xD)++;
        return;
    case 1:
        if (Flag_Check(2, 6) || Flag_Check(2, 0)) return;
        Flag_Set(0, 0xB, 0);
        Flag_Set(0, 0x14, 0);
        Room_Reset();
        Game_Start(0x19);
        return;
    }
}

/* a HUD sprite from a record {x, y, u, v, w, h, rgb, clut}, moved and cut */
static void vs_sprt(gptr r, s32 dx, u8 du, u8 dv, u16 dw)
{
    gptr p = U32(0x87DC98);
    Gpu_SetSprt(p);
    F32(p + 8) = (float)(S16(r) + dx);
    F32(p + 0xC) = (float)(s32)S16(r + 2);
    U8(p + 0x14) = (u8)(U8(r + 4) + du); U8(p + 0x15) = (u8)(U8(r + 5) + dv);
    U16(p + 0x18) = (u16)(U16(r + 6) - dw); U16(p + 0x1A) = U16(r + 8);
    U8(p + 4) = U8(r + 0xA); U8(p + 5) = U8(r + 0xB); U8(p + 6) = U8(r + 0xC);
    U16(p + 0x16) = (u16)(((U16(r + 0xE) >> 4) & 0x3F) | 0x7EC0);
    Gpu_AddPrim(U32(0x87F94C) + 0x74, p);
    U32(0x87DC98) = p + 0x1C;
}

/* the two life bars (by thirds of colour), the faces, the combo counts and the timer */
void DC2CALL Vs_Hud(gptr e)                                          /* 0x41A940 */
{
    for (u32 k = 0; k < 2; k++) vs_sprt(0x71ED10 + k * 0x10, 0, 0, 0, 0);
    if (!Flag_Check(0, 0xB)) {
        gptr o = U32(0x87E760);
        s16 h1 = S16(o + 0x368), h2 = S16(o + 0x5C8), m1 = S16(o + 0x36A), m2 = S16(o + 0x5CA);
        s16 c1 = h1 < 0 ? 0 : h1, c2 = h2 < 0 ? 0 : h2;
        u32 l1 = 0, l2 = 0;
        if ((s16)(m1 >> 1) >= h1) l1 = 1;
        if ((s16)(m1 >> 2) >= h1) l1 = 2;
        if ((s16)(m2 >> 1) >= h2) l2 = 1;
        if ((s16)(m2 >> 2) >= h2) l2 = 2;
        s32 b1 = m1 ? 0x7E - c1 * 126 / m1 : 0, b2 = m2 ? 0x7E - c2 * 126 / m2 : 0;
        if (b1 < 0) b1 = 0;
        if (b2 < 0) b2 = 0;
        vs_sprt(0x71ED30 + l1 * 0x10, b1, (u8)b1, 0, (u16)b1);
        vs_sprt(0x71ED60 + l2 * 0x10, 0, 0, 0, (u16)b2);
    } else {
        vs_sprt(0x71EDB0, 0, 0, 0, 0);
        vs_sprt(0x71EDC0, 0, 0, 0, 0);
    }
    for (u32 k = 0; k < 2; k++) {                       /* the faces by kind */
        s32 f = ((s32)U8(U32(0x87E760) + k * 0x260 + 0x2B8) - 0x1D) / 2;
        vs_sprt(0x71EDE0 + k * 0x10, 0, 0, (u8)(f << 4), 0);
    }
    for (u32 k = 0; k < 2; k++) {                       /* "n hits" */
        s8 n = S8(e + 0x20 + k);
        if (n <= 1 || !U8(e + 0x22 + k)) continue;
        s32 t = n / 10, d = n % 10;
        if (t) vs_sprt(0x71EE00 + k * 0x10, 0, (u8)(t << 4), 0, 0);
        vs_sprt(0x71EE20 + k * 0x10, 0, (u8)(d << 4), 0, 0);
        vs_sprt(0x71EE40 + k * 0x10, 0, 0, 0, 0);
        vs_sprt(0x71EE60 + k * 0x10, 0, 0, 0, 0);
    }
    if (!Flag_Check(0, 0x14)) {                         /* the seconds left */
        s32 s = S16(e + 0x10) / 30;
        vs_sprt(0x71ED90, 0, (u8)((s / 10) << 4), 0, 0);
        vs_sprt(0x71EDA0, 0, (u8)((s % 10) << 4), 0, 0);
    } else vs_sprt(0x71EDD0, 0, 0, 0, 0);
    Gpu_SetDrawMode(U32(0x87DC98), 0, 0, 0x19, 0);
    Gpu_AddPrim(U32(0x87F94C) + 0x74, U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}

/* ---- the vehicle (area kind 18, area 2?: steps 0x71EE80): the player's
 * matrix from its angles, moved by +0xF0; its wheel (part 1) turns by e+0xC and
 * two strips of the road in VRAM scroll with the speed +0xF4 (0x74815E, 0x74815C) */

void DC2CALL Vehicle(gptr e)                                         /* 0x41BB60 */
{
    gptr pl = U32(G() + 0x940);
    Orig_Call1(U32(0x71EE80 + S8(e + 3) * 4), e);
    RotMatrixYXZ(pl + 0x48, pl + 0x20);
    Obj_MoveLocal(pl, pl + 0xF0);
    Obj_UpdateMatrix(pl);
}

void DC2CALL Vehicle_Init(gptr e)                                    /* 0x41BBB0 */
{
    gptr w = Obj_Part(U32(U32(G() + 0x940) + 0x64), 1);
    U16(w + 0x48) = 0x400; U16(w + 0x4C) = 0;
    U16(0x74815C) = 0; U16(0x74815E) = 0;
    U8(e + 3)++;
}

/* a strip of 0xB8 at x 0x280 (row y) scrolled by s through the copy at 0x380 */
static void road_strip(u16 s, u32 y, u32 dy)
{
    s16 v = (s16)s;
    gptr r = Tmp_Push(8);
    U16(r) = (u16)(0x280 + v / 2); U16(r + 2) = (u16)y; U16(r + 4) = (u16)((0xB8 - v) / 2); U16(r + 6) = 0x24;
    Gpu_VramCopy(r, 0x380, (s32)dy);
    U16(r) = 0x280; U16(r + 4) = (u16)(v / 2);
    Gpu_VramCopy(r, 0x380 - (v - 0xB8) / 2, (s32)dy);
    Tmp_Pop(8);
}

static u16 road_scroll(gptr a, gptr pl)
{
    s16 v = (s16)(U16(a) + (s16)(S16(pl + 0xF4) / -20));
    U16(a) = (u16)v;
    if (v > 0xB8) U16(a) = 0;
    else if (v < 0) U16(a) = 0xB8;
    return U16(a);
}

void DC2CALL Vehicle_Step(gptr e)                                    /* 0x41BBF0 */
{
    gptr pl = U32(G() + 0x940);
    gptr w = Obj_Part(U32(pl + 0x64), 1);
    U16(w + 0x48) = 0x400;
    U16(w + 0x4C) += U16(e + 0xC);
    road_strip(road_scroll(0x74815E, pl), 0x1B8, 0xB8);
    road_strip(road_scroll(0x74815C, pl), 0x1DC, 0xDC);
}

void DC2CALL Vehicle_Fx1(gptr e)                                     /* 0x41BDC0 */
{
    Effect_Attach(U32(G() + 0x940), 0x666D34);
    U8(e + 3) = 1;
}

void DC2CALL Vehicle_Fx2(gptr e)                                     /* 0x41BDF0 */
{
    Effect_Attach(U32(G() + 0x940), 0x6672BC);
    U8(e + 3) = 1;
}

/* ---- the turret's HUD (kind 16 of area 0, after Turret_Draw) ---- */

/* a sprite at xywh with its uv, clut, colour (semi-transparent: semi) */
void DC2CALL Sprite_Put(gptr xywh, u32 u, u32 v, u32 clut, u32 col, u32 semi)   /* 0x413190 */
{
    gptr p = U32(0x87DC98);
    F32(p + 8) = (float)(s32)S16(xywh); F32(p + 0xC) = (float)(s32)S16(xywh + 2); U32(p + 0x10) = 0x3C23D70A;
    U16(p + 0x18) = U16(xywh + 4); U16(p + 0x1A) = U16(xywh + 6);
    U8(p + 0x14) = (u8)u; U8(p + 0x15) = (u8)v; U16(p + 0x16) = (u16)clut;
    U32(p + 4) = col | ((u16)semi ? 0x66000000u : 0x64000000u);
    gptr ot = 0x87BC04 + (U32(0x87DE0C) << 12);
    U32(p) = U32(ot) | 0x04000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) = p + 0x1C;
}

/* sprites from a list {u, v, w, h, dx, dy, (count in the first)} at x, y; then the tpage */
void DC2CALL Sprites_Draw(gptr l, u32 x, u32 y, u32 tp, u32 clut, u32 col)   /* 0x4130B0 */
{
    u32 n = U8(l + 6);
    if (n) {
        u32 semi = col >> 31 & 1;
        gptr r = Tmp_Push(8);
        for (; n; n--, l += 8) {
            U16(r) = (u16)(S8(l + 4) + x); U16(r + 2) = (u16)(S8(l + 5) + y);
            U16(r + 4) = U8(l + 2); U16(r + 6) = U8(l + 3);
            Sprite_Put(r, U8(l), U8(l + 1), clut, col & 0xFFFFFF, semi);
        }
        Tmp_Pop(8);
    }
    gptr p = U32(0x87DC98), ot = 0x87BC04 + (U32(0x87DE0C) << 12);
    U32(p + 4) = tp | 0xE1000400;
    U32(p + 8) = 0;
    U32(p) = U32(ot) | 0x02000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) = p + 0xC;
}

/* an FT4 from a 0x48 buffer into the HUD's OT entry */
static void ft4_put(gptr b)
{
    gptr p = U32(0x87DC98), ot = 0x87BC04 + (U32(0x87DE0C) << 12);
    memcpy(MEM(p), MEM(b), 0x48);
    U32(p) = U32(ot) | 0x09000000;
    U32(ot) = p & 0xFFFFFF;
    U32(0x87DC98) = p + 0x48;
}

static void ft4_z(gptr b)
{
    U32(b + 0x10) = 0x3C23D70A; U32(b + 0x20) = 0x3C23D70A; U32(b + 0x30) = 0x3C23D70A; U32(b + 0x40) = 0x3C23D70A;
}

static void ft4_xy(gptr b, float x0, float y0, float x1, float y1)
{
    F32(b + 8) = x0; F32(b + 0xC) = y0; F32(b + 0x18) = x1; F32(b + 0x1C) = y0;
    F32(b + 0x28) = x0; F32(b + 0x2C) = y1; F32(b + 0x38) = x1; F32(b + 0x3C) = y1;
}

static void ft4_uv(gptr b, u8 u0, u8 v0, u8 u1, u8 v1)
{
    U8(b + 0x14) = u0; U8(b + 0x15) = v0; U8(b + 0x24) = u1; U8(b + 0x25) = v0;
    U8(b + 0x34) = u0; U8(b + 0x35) = v1; U8(b + 0x44) = u1; U8(b + 0x45) = v1;
}

/* an angle into -0x800..0x800 the way the HUD folds it */
static s16 hud_fold(u16 a)
{
    if ((s16)a > 0x800) a = (u16)(a - (((u32)(s32)(s16)a + 0x7FF) >> 12 << 12));
    return (s16)a;
}

/* a target's mark on the screen (blinking cluts) */
void DC2CALL Turret_Mark(gptr o, u32 t)                              /* 0x411E30 */
{
    gptr b = Tmp_Push(0x50), v = b + 0x48;
    memset(MEM(b), 0, 0x50);
    U8(b + 7) = 0x2E; U8(b + 6) = 0x80; U8(b + 5) = 0x80; U8(b + 4) = 0x80;
    U8(b + 0x34) = 0x20; U8(b + 0x14) = 0x20; U16(b + 0x26) = 0x438; U8(b + 0x25) = 0xA8; U8(b + 0x15) = 0xA8;
    U8(b + 0x44) = 0x58; U8(b + 0x24) = 0x58; U8(b + 0x45) = 0xE0; U8(b + 0x35) = 0xE0;
    (void)t;                                            /* (the clut blinks by the global clock) */
    U16(b + 0x16) = (u16)(0x7F4C + (((s32)S16(0x746E40) / 2) & 3));
    s32 dx = 0x18, dy = 0x16;
    gptr pp;
    if (U8(o + 0x58) == 0xC) { pp = Obj_Part(U32(o + 0x64), 0x10); dx = 0x2C; dy = 0x28; }
    else pp = Obj_Part(U32(o + 0x64), 2);
    U16(v) = U16(pp + 0x40); U16(v + 2) = U16(pp + 0x42); U16(v + 4) = U16(pp + 0x44); U16(v + 6) = 0;
    gte_ldv0(v); gte_rtps(); gte_stsxy(v);
    s32 sx = S16(v), sy = S16(v + 2);
    ft4_xy(b, (float)(sx - dx), (float)(sy - dy), (float)(sx + dx), (float)(sy + dy));
    ft4_z(b);
    ft4_put(b);
    Tmp_Pop(0x50);
}

void DC2CALL Turret_Hud(gptr e)                                      /* 0x411160 */
{
    gptr c = G() + 0x778;
    U8(0x746E7C)++;
    gptr cr = U32(c + 0x40) + U8(c + 0x4C) * 16;
    Sprites_Draw(0x5E02A8, 0x80, 0x80, 0x15, 0x7E42, 0x80008080);
    Sprites_Draw(0x5E02D8, 0x120, 0x80, 0x15, 0x7E42, 0x80008080);
    gptr b = Tmp_Push(0x48);
    memset(MEM(b), 0, 0x48);
    ft4_uv(b, 0x80, 0x80, 0xA8, 0xA8);
    U8(b + 6) = 0x80; U8(b + 5) = 0x80; U8(b + 4) = 0x80; U8(b + 7) = 0x2C;
    U16(b + 0x26) = 0x418; U16(b + 0x16) = 0x7E41;
    /* the sight where the aim points against the camera */
    s16 cy = S16(c + 0x22);
    s32 ca = cy > 0x800 ? cy - 0x1000 : cy;
    s16 ay = hud_fold((u16)((U16(e + 0x14) & 0x1FFF) - ca));
    s16 ex = (s16)(ay * 140 / S16(e + 0xE));
    s32 h = ratan2(120.0f, (float)(s32)U16(cr + 6));
    s16 cx = S16(c + 0x20);
    ca = cx > 0x800 ? cx - 0x1000 : cx;
    s16 ax = hud_fold((u16)((U16(e + 0x12) & 0x1FFF) - ca));
    U16(0x746E5C) = (u16)ex;
    s16 ey = (s16)(-104 * ax / (s16)h);
    U16(0x746E5E) = (u16)ey;
    ft4_z(b);
    ft4_xy(b, (float)(ex + 0x8C), (float)(ey + 0x64), (float)(ex + 0xB4), (float)(ey + 0x8C));
    if (!U16(e + 0xC)) ft4_put(b);
    /* the middle dot */
    ft4_uv(b, 0x28, 0x78, 0x2F, 0x7F);
    U8(b + 7) = 0x2E; U16(b + 0x26) = 0x418; U16(b + 0x16) = 0x7F40;
    ft4_xy(b, 154.0f, 186.0f, 162.0f, 194.0f);
    ft4_put(b);
    /* the compass strip, in two pieces around the yaw */
    s16 q = (s16)(hud_fold(U16(c + 0x22)) / -12);
    if (q < 0) { U8(b + 0x44) = 0x78; U8(b + 0x24) = 0x78; U8(b + 0x34) = (u8)(0x18 - q); U8(b + 0x14) = (u8)(0x18 - q);
                 F32(b + 0x28) = 96.0f; F32(b + 8) = 96.0f; }
    else { U8(b + 0x34) = 0x18; U8(b + 0x14) = 0x18; F32(b + 0x28) = (float)(q + 0x60); F32(b + 8) = (float)(q + 0x60);
           U8(b + 0x44) = 0x78; U8(b + 0x24) = 0x78; }
    U8(b + 0x45) = 0x58; U8(b + 0x35) = 0x58; U8(b + 0x25) = 0x48; U8(b + 0x15) = 0x48;
    U16(b + 0x26) = 0x418; U16(b + 0x16) = 0x7F40;
    float mid = (float)(q + 0xA0);
    F32(b + 0x1C) = 170.0f; F32(b + 0x38) = mid; F32(b + 0x18) = mid; F32(b + 0xC) = 170.0f;
    F32(b + 0x3C) = 186.0f; F32(b + 0x2C) = 186.0f;
    ft4_put(b);
    U8(b + 0x34) = 0x18; U8(b + 0x14) = 0x18;
    if (q < 0) { U8(b + 0x44) = 0x78; U8(b + 0x24) = 0x78; F32(b + 0x38) = (float)(q + 0xE0); F32(b + 0x18) = (float)(q + 0xE0); }
    else { F32(b + 0x38) = 224.0f; F32(b + 0x18) = 224.0f; U8(b + 0x44) = (u8)(0x78 - q); U8(b + 0x24) = (u8)(0x78 - q); }
    F32(b + 0x28) = mid; F32(b + 8) = mid;
    F32(b + 0x1C) = 170.0f; F32(b + 0xC) = 170.0f; F32(b + 0x3C) = 186.0f; F32(b + 0x2C) = 186.0f;
    ft4_put(b);
    /* the pitch scale's marker */
    U8(b + 0x25) = 0x78; U8(b + 0x15) = 0x78; U8(b + 0x44) = 0x37; U8(b + 0x24) = 0x37;
    U8(b + 0x34) = 0x30; U8(b + 0x14) = 0x30; U8(b + 0x45) = 0x7F; U8(b + 0x35) = 0x7F;
    U16(b + 0x26) = 0x418; U16(b + 0x16) = 0x7F40;
    ft4_xy(b, 15.0f, 120.0f, 23.0f, 128.0f);
    ft4_put(b);
    /* the pitch scale, in one or two pieces */
    s16 q2 = (s16)(hud_fold(U16(c + 0x20)) * 4 / 9);
    U8(b + 0x34) = 0; U8(b + 0x14) = 0;
    u8 v3;
    if (q2 < 0) { U8(b + 0x25) = (u8)(0x40 - q2); U8(b + 0x15) = (u8)(0x40 - q2); v3 = 0xB0; }
    else { U8(b + 0x25) = 0x40; U8(b + 0x15) = 0x40; v3 = (u8)(0xB0 - q2); }
    U8(b + 0x45) = v3; U8(b + 0x35) = v3; U8(b + 0x44) = 0x10; U8(b + 0x24) = 0x10;
    F32(b + 0x28) = 24.0f; F32(b + 8) = 24.0f; F32(b + 0x38) = 40.0f; F32(b + 0x18) = 40.0f;
    if (q2 < 0) { F32(b + 0x1C) = 68.0f; F32(b + 0xC) = 68.0f; F32(b + 0x3C) = (float)(q2 + 0xB4); F32(b + 0x2C) = (float)(q2 + 0xB4); }
    else { F32(b + 0x3C) = 180.0f; F32(b + 0x2C) = 180.0f; F32(b + 0x1C) = (float)(q2 + 0x44); F32(b + 0xC) = (float)(q2 + 0x44); }
    ft4_put(b);
    if (q2 > 0) {
        F32(b + 0x3C) = (float)(q2 + 0x44); F32(b + 0x2C) = (float)(q2 + 0x44);
        U8(b + 0x45) = 0xC0; U8(b + 0x35) = 0xC0; U8(b + 0x34) = 0; U8(b + 0x14) = 0;
        U8(b + 0x25) = (u8)(0xC0 - q2); U8(b + 0x15) = (u8)(0xC0 - q2); U8(b + 0x44) = 0x10; U8(b + 0x24) = 0x10;
        F32(b + 0x28) = 24.0f; F32(b + 8) = 24.0f; F32(b + 0x38) = 40.0f; F32(b + 0x18) = 40.0f;
        F32(b + 0x1C) = 68.0f; F32(b + 0xC) = 68.0f;
        ft4_put(b);
    }
    /* the nearest enemies left and right (0x746E58, 0x746E54), blinking */
    if ((s16)U16(0x746E40) % 10 < 5) {
        for (u32 k = 0; k < 2; k++) {
            gptr r = U32(k ? 0x746E54 : 0x746E58);
            if (!r || S16(r + 0x1C8) >= 0x37) continue;
            s16 a = S16(r + 0x1C8);
            ft4_uv(b, k ? 0x40 : 0, 0, k ? 0x80 : 0x40, 0x40);
            U16(b + 0x26) = 0x418;
            U16(b + 0x16) = a < 0x24 ? (u16)((a / 6 % 6 + 1) | 0x7F40) : 0x7F46;
            if (k) ft4_xy(b, 202.0f, 48.0f, 266.0f, 112.0f);
            else ft4_xy(b, 54.0f, 48.0f, 118.0f, 112.0f);
            ft4_put(b);
        }
    }
    Tmp_Pop(0x48);
    /* the targets' marks, odd or even ones by turns */
    for (u32 k = 1; k < 10; k++) {
        gptr o = U32(G() + 0x940) + k * 0x260;
        if (!(U8(o + 0x50) & 1) || !(U8(o + 0x184) & 1) || S16(o + 0x1C8) >= 0x3C || U8(0x746E6D + k - 1)) continue;
        if (U32(0x746E40) & 8 ? (k & 1) : !(k & 1)) Turret_Mark(o, U16(0x746E40));
    }
    /* a grid over the screen */
    U16(0x746E60) = 0; U16(0x746E62) = 0;
    for (u32 k = 0; k < 13; k++) {
        gptr p = U32(0x87DC98), ot = 0x87BC04 + (U32(0x87DE0C) << 12);
        U32(p + 8) = 0; F32(p + 0xC) = (float)(s32)(k * 16); U32(p + 0x14) = 0x439F8000; F32(p + 0x18) = (float)(s32)(k * 16);
        U32(p + 0x1C) = 0x3C23D70A; U32(p + 0x10) = 0x3C23D70A; U32(p + 4) = 0x42004800;
        U32(p) = U32(ot) | 0x03000000; U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x20;
    }
    u32 x0 = U32(0x746E60) & 0xF;
    for (u32 k = 0; k < 18; k++) {
        gptr p = U32(0x87DC98), ot = 0x87BC04 + (U32(0x87DE0C) << 12);
        float x = (float)(s32)(x0 + k * 16);
        F32(p + 8) = x; U32(p + 0xC) = 0; F32(p + 0x14) = x; U32(p + 0x18) = 0x436F0000;
        U32(p + 0x1C) = 0x3C23D70A; U32(p + 0x10) = 0x3C23D70A; U32(p + 4) = 0x42004800;
        U32(p) = U32(ot) | 0x03000000; U32(ot) = p & 0xFFFFFF;
        U32(0x87DC98) = p + 0x20;
    }
    Gpu_SetDrawMode(U32(0x87DC98), 1, 0, 0x60, 0);
    Gpu_AddPrim(0x87BC04 + (U32(0x87DE0C) << 12), U32(0x87DC98));
    U32(0x87DC98) += 0xC;
}
