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
    Fn_410710(e);
    Turret_DrawSky(e);
    Fn_411160(e);
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
